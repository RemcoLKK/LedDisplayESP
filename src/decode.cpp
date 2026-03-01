#include <Arduino.h>
#include <ArduinoJson.h>
#include <wifiAndMQTT.h>   // global `client`

// ---- Limits / safety ----
static const uint16_t MAX_CHUNKS     = 256;
static const size_t   MAX_FILE_BYTES = 512 * 1024;

// If true: accept new image even if previous isn't consumed (overwrites old).
// If false: drop new chunks until you consume previous file.
static const bool OVERWRITE_WHEN_READY = false;

// ---------------- Base64 decode ----------------
static int b64_value(char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '+') return 62;
  if (c == '/') return 63;
  if (c == '=') return -2; // padding
  return -1;
}

static size_t base64MaxDecodedLen(size_t b64Len) {
  return (b64Len * 3) / 4 + 4;
}

// Returns decoded bytes written, or -1 on error
static int base64Decode(const char* in, size_t inLen, uint8_t* out, size_t outCap) {
  if (!in || !out) return -1;

  int val = 0, valb = -8;
  size_t outLen = 0;

  for (size_t i = 0; i < inLen; i++) {
    char c = in[i];
    if (c == '\r' || c == '\n' || c == ' ' || c == '\t') continue;

    int v = b64_value(c);
    if (v == -1) return -1;
    if (v == -2) break;

    val = (val << 6) + v;
    valb += 6;

    if (valb >= 0) {
      uint8_t byte = (uint8_t)((val >> valb) & 0xFF);
      valb -= 8;
      if (outLen >= outCap) return -1;
      out[outLen++] = byte;
    }
  }
  return (int)outLen;
}

// -------------- Reassembly state --------------
struct ImageRx {
  bool active = false;
  String id;
  String mime;
  uint16_t total = 0;
  uint16_t received = 0;

  bool* seen = nullptr;
  uint8_t** chunkData = nullptr;
  uint32_t* chunkLen = nullptr;

  void reset() {
    active = false;

    if (chunkData) {
      for (uint16_t i = 0; i < total; i++) {
        if (chunkData[i]) free(chunkData[i]);
      }
      free(chunkData);
    }
    if (chunkLen) free(chunkLen);
    if (seen) free(seen);

    seen = nullptr;
    chunkData = nullptr;
    chunkLen = nullptr;

    id = "";
    mime = "";
    total = 0;
    received = 0;
  }

  bool begin(const String& newId, const String& newMime, uint16_t newTotal) {
    reset();
    if (newTotal == 0 || newTotal > MAX_CHUNKS) return false;

    id = newId;
    mime = newMime;
    total = newTotal;

    seen = (bool*)calloc(total, sizeof(bool));
    chunkData = (uint8_t**)calloc(total, sizeof(uint8_t*));
    chunkLen = (uint32_t*)calloc(total, sizeof(uint32_t));
    if (!seen || !chunkData || !chunkLen) {
      reset();
      return false;
    }

    active = true;
    return true;
  }

  bool accept(uint16_t idx, const uint8_t* data, uint32_t len) {
    if (!active || idx >= total) return false;
    if (seen[idx]) return true; // ignore duplicates

    if (!data || len == 0) return false;

    uint8_t* copy = (uint8_t*)malloc(len);
    if (!copy) return false;

    memcpy(copy, data, len);

    chunkData[idx] = copy;
    chunkLen[idx] = len;
    seen[idx] = true;
    received++;
    return true;
  }

  bool complete() const {
    return active && total > 0 && received == total;
  }

  bool stitch(uint8_t** outBuf, size_t* outLen) const {
    if (!outBuf || !outLen) return false;
    *outBuf = nullptr;
    *outLen = 0;

    if (!complete()) return false;

    size_t totalLen = 0;
    for (uint16_t i = 0; i < total; i++) {
      if (!seen[i]) return false;
      totalLen += chunkLen[i];
      if (totalLen > MAX_FILE_BYTES) return false;
    }

    uint8_t* buf = (uint8_t*)malloc(totalLen);
    if (!buf) return false;

    size_t off = 0;
    for (uint16_t i = 0; i < total; i++) {
      memcpy(buf + off, chunkData[i], chunkLen[i]);
      off += chunkLen[i];
    }

    *outBuf = buf;
    *outLen = totalLen;
    return true;
  }
};

static ImageRx g_rx;

// Once completed, we store the final file here until user code consumes it
static bool     g_fileReady = false;
static uint8_t* g_fileBytes = nullptr;
static size_t   g_fileLen   = 0;
static String   g_fileId;
static String   g_fileMime;

static void clearCompletedFile() {
  if (g_fileBytes) free(g_fileBytes);
  g_fileBytes = nullptr;
  g_fileLen = 0;
  g_fileId = "";
  g_fileMime = "";
  g_fileReady = false;
}

// -------- Construct file from the stream --------
static void finalizeIfComplete() {
  if (!g_rx.complete()) return;

  uint8_t* buf = nullptr;
  size_t len = 0;

  if (!g_rx.stitch(&buf, &len)) {
    Serial.println("finalize: stitch failed");
    g_rx.reset();
    return;
  }

  clearCompletedFile();
  g_fileBytes = buf;
  g_fileLen = len;
  g_fileId = g_rx.id;
  g_fileMime = g_rx.mime;
  g_fileReady = true;

  Serial.printf("Image complete id=%s bytes=%u mime=%s\n",
                g_fileId.c_str(), (unsigned)g_fileLen, g_fileMime.c_str());

  g_rx.reset();
}

// -------- MQTT payload helper: handle quoted JSON --------
// Sometimes MQTT Explorer sends the JSON as a *string* with quotes, e.g.
// "{\"id\":\"x\",...}"
// This function tries to detect and unescape that into a real JSON buffer.
static bool unwrapQuotedJsonIfNeeded(const byte* payload, unsigned int length, String& out) {
  if (!payload || length == 0) return false;

  // Quick check: starts with quote and ends with quote
  if (length < 2) return false;
  if ((char)payload[0] != '"' || (char)payload[length - 1] != '"') return false;

  // Copy into String
  String s;
  s.reserve(length);
  for (unsigned int i = 0; i < length; i++) s += (char)payload[i];

  // Remove surrounding quotes
  s.remove(0, 1);
  s.remove(s.length() - 1);

  // Unescape \" and \\ (basic)
  s.replace("\\\"", "\"");
  s.replace("\\\\", "\\");

  out = s;
  return true;
}
static void printRawMqttPayload(const char* topic, const byte* payload, unsigned int length) {
  Serial.println("----- MQTT RX -----");
  Serial.print("Topic: ");
  Serial.println(topic ? topic : "(null)");
  Serial.print("Length: ");
  Serial.println(length);

  Serial.print("Raw: ");
  for (unsigned int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Also show first bytes as hex (helps spot hidden chars / quotes)
  Serial.print("Hex: ");
  const unsigned int maxHex = (length > 64) ? 64 : length;
  for (unsigned int i = 0; i < maxHex; i++) {
    if (payload[i] < 16) Serial.print('0');
    Serial.print(payload[i], HEX);
    Serial.print(' ');
  }
  if (length > 64) Serial.print("...");
  Serial.println();

  Serial.println("-------------------");
}

// -------------- MQTT callback --------------
static void onMqttMessage(char* topic, byte* payload, unsigned int length) {
  if (!topic || !payload || length == 0) return;
  if (strcmp(topic, TOPIC_IMG) != 0) return;

  // DEBUG: print exactly what ESP received
  printRawMqttPayload(topic, payload, length);

  if (g_fileReady) {
    if (!OVERWRITE_WHEN_READY) {
      Serial.println("Dropping chunks: previous file not consumed yet");
      return;
    }
    clearCompletedFile();
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, payload, length);
  if (err) {
    String unwrapped;
    if (unwrapQuotedJsonIfNeeded(payload, length, unwrapped)) {
      Serial.println("Unwrapped quoted JSON:");
      Serial.println(unwrapped);

      err = deserializeJson(doc, unwrapped);
    }
  }

  if (err) {
    Serial.printf("JSON parse error: %s (len=%u)\n", err.c_str(), (unsigned)length);
    return;
  }

  // Accept either "mime" or "type"
  JsonObject obj = doc.as<JsonObject>();
  if (obj.isNull()) {
    Serial.println("Root is not a JSON object!");
    return;
  }

  const char* id = obj["id"].as<const char*>();
  const char* mime = obj["mime"].as<const char*>();
  if (!mime) mime = obj["type"].as<const char*>();
  if (!mime) mime = "";

  int idx = obj["idx"].as<int>();
  int total = obj["total"].as<int>();
  const char* b64 = obj["b64"].as<const char*>();

  Serial.printf("Parsed fields -> id:%s idx:%d total:%d mime:%s\n",
                id ? id : "(null)", idx, total, mime);

  if (!id || !b64 || idx < 0 || total <= 0) {
    Serial.println("Invalid JSON fields");
    if (!id) Serial.println("- missing id");
    if (!b64) Serial.println("- missing b64");
    if (idx < 0) Serial.println("- invalid idx");
    if (total <= 0) Serial.println("- invalid total");
    return;
  }

  // Start new RX if needed or ID changes
  if (!g_rx.active || g_rx.id != id) {
    if (!g_rx.begin(String(id), String(mime), (uint16_t)total)) {
      Serial.println("Failed to begin RX");
      return;
    }
    Serial.printf("RX begin id=%s total=%d mime=%s\n", id, total, mime);
  }

  // Decode base64
  size_t b64Len = strlen(b64);
  size_t outCap = base64MaxDecodedLen(b64Len);

  // Hard cap per-chunk allocation to reduce crash risk
  if (outCap > (64 * 1024)) {
    Serial.println("Chunk too large (decoded cap > 64KB). Reduce chunk size.");
    return;
  }

  uint8_t* decoded = (uint8_t*)malloc(outCap);
  if (!decoded) {
    Serial.println("OOM base64 buffer");
    return;
  }

  int decLen = base64Decode(b64, b64Len, decoded, outCap);
  if (decLen <= 0) {
    free(decoded);
    Serial.println("Base64 decode error / empty decode");
    return;
  }

  // Store chunk
  if (!g_rx.accept((uint16_t)idx, decoded, (uint32_t)decLen)) {
    free(decoded);
    Serial.println("accept chunk failed");
    return;
  }
  free(decoded);

  Serial.printf("RX chunk idx=%d total=%d received=%d\n", idx, total, g_rx.received);

  finalizeIfComplete();
}

// -------------- Public API --------------
void decodeMqttRegister() {
  client.setCallback(onMqttMessage);
  client.subscribe(TOPIC_IMG);
  Serial.printf("Subscribed to %s\n", TOPIC_IMG);
}

bool decodeTryGetCompletedFile(uint8_t** outBytes, size_t* outLen, String* outId, String* outMime) {
  if (!g_fileReady) return false;
  if (!outBytes || !outLen) return false;

  *outBytes = g_fileBytes;
  *outLen = g_fileLen;

  if (outId) *outId = g_fileId;
  if (outMime) *outMime = g_fileMime;

  // transfer ownership
  g_fileBytes = nullptr;
  g_fileLen = 0;
  g_fileId = "";
  g_fileMime = "";
  g_fileReady = false;

  return true;
}