#include <Arduino.h>
#include "incomingImage.h"
#include "display.h"
#include "decode.h"

static const int W = 128;
static const int H = 128;
static const size_t EXPECTED_LEN = (size_t)W * (size_t)H * 2;

// bytes zijn BIG-endian per pixel: [hi][lo] (zoals je JS zendt)
bool drawRGB565FromBytesBE(const uint8_t* bytes, size_t len, int x, int y) {
  if (!bytes || len != EXPECTED_LEN) {
    Serial.printf("drawRGB565FromBytesBE: invalid len=%u (expected=%u)\n",
                  (unsigned)len, (unsigned)EXPECTED_LEN);
    return false;
  }
  if (!virtualDisp) {
    Serial.println("drawRGB565FromBytesBE: virtualDisp is null");
    return false;
  }

  size_t i = 0;
  for (int yy = 0; yy < H; yy++) {
    for (int xx = 0; xx < W; xx++) {
      uint16_t c = ((uint16_t)bytes[i] << 8) | (uint16_t)bytes[i + 1];
      i += 2;

      // c is al RGB565 -> direct tekenen
      virtualDisp->drawPixel(x + xx, y + yy, c);
    }
  }

  // Jij gebruikt beide varianten in je code; deze is consistent met statusScreen/renderScreen
  virtualDisp->flipDMABuffer();
  return true;
}

void handleIncomingImageOnce() {
  uint8_t* bytes = nullptr;
  size_t len = 0;
  String id, mime;

  if (!decodeTryGetCompletedFile(&bytes, &len, &id, &mime)) return;

  Serial.printf("Got file id=%s mime=%s len=%u\n", id.c_str(), mime.c_str(), (unsigned)len);

  if (mime == "image/rgb565") {
    if (!drawRGB565FromBytesBE(bytes, len, 0, 0)) {
      Serial.println("RGB565 draw failed");
    }
  } else {
    Serial.println("Not RGB565 (expected mime/type = image/rgb565)");
  }

  free(bytes);
}