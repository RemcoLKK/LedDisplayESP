#include <wifiAndMQTT.h>
#include <display.h>
#include <decode.h>

WiFiClientSecure espClient;
PubSubClient client(espClient);

static unsigned long wifiPrevMillis  = 0;
static unsigned long mqttPrevMillis  = 0;
static const unsigned long WIFI_RETRY_INTERVAL_MS = 30000;
static const unsigned long MQTT_RETRY_INTERVAL_MS = 5000;

static void mqttTlsSetup() {
    
  // Recommended security: set CA cert:
  // espClient.setCACert(root_ca_pem);

  // Quick to get it working:
  espClient.setInsecure();
}

void wifiInit() {
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    statusScreen(1);          // show WiFi screen while waiting
    delay(200);
    Serial.print(".");
  }

  Serial.println("\nConnected to WiFi!");
  IPAddress dns1(1,1,1,1);   // Cloudflare
  IPAddress dns2(8,8,8,8);   // Google
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, dns1, dns2);
  statusScreen(0);            // clear / normal screen
}

void wifiReconnectCheck() {
  unsigned long currentMillis = millis();
  // if WiFi is down, try reconnecting every CHECK_WIFI_TIME seconds
  if ((WiFi.status() != WL_CONNECTED) && (currentMillis - wifiPrevMillis >=WIFI_RETRY_INTERVAL_MS)) {
    Serial.print(millis());
    Serial.println("Reconnecting to WiFi...");
    statusScreen(3);
    WiFi.disconnect();
    WiFi.reconnect();
    wifiPrevMillis = currentMillis;
  }
  statusScreen(0); // clear / normal screen if connected
}

void mqttInit() {
  espClient.stop();
  delay(100);
  // Must be called after WiFi is connected
  client.setServer(MQTT_HOST, MQTT_PORT);

  mqttTlsSetup();

  client.setKeepAlive(30);
  client.setSocketTimeout(5);
  client.setBufferSize(8192);

  Serial.print("Connecting to MQTT");

  while (!client.connected()) {
    statusScreen(2);

    String clientId = "esp32-";
    clientId += String((uint32_t)ESP.getEfuseMac(), HEX);

    bool ok = client.connect(clientId.c_str(), MQTT_USER, MQTT_PASS);

    if (ok) {
      Serial.println("\nConnected to MQTT!");
      statusScreen(0);

      decodeMqttRegister();  // subscribe + callback

      // ---- Publish "connected" status ----
      if (client.connected()) {
        String payload = "{\"status\":\"connected\",\"client\":\"";
        payload += clientId;
        payload += "\"}";

        bool pubOk = client.publish(MQTT_STATUS_TOPIC, payload.c_str(), true); // retained

        if (pubOk) {
          Serial.println("Status published (connected)");
        } else {
          Serial.println("Failed to publish status");
        }
      }

      break;
    }

    Serial.print(".");
    delay(500);
  }
}

void mqttReconnectCheck() {
  const unsigned long now = millis();

  if (WiFi.status() != WL_CONNECTED) return;
  if (client.connected()) return;
  if (now - mqttPrevMillis < MQTT_RETRY_INTERVAL_MS) return;

  mqttPrevMillis = now;

  Serial.println("Reconnecting to MQTT...");
  statusScreen(4);

  String clientId = "esp32-";
  clientId += String((uint32_t)ESP.getEfuseMac(), HEX);
  espClient.stop();
  bool ok = client.connect(clientId.c_str(), MQTT_USER, MQTT_PASS);

  if (ok) {
    Serial.println("MQTT reconnected!");
    statusScreen(0);

    decodeMqttRegister();  // re-subscribe

    // ---- Publish reconnect status ----
    if (client.connected()) {
      String payload = "{\"status\":\"reconnected\",\"client\":\"";
      payload += clientId;
      payload += "\"}";

      client.publish(MQTT_STATUS_TOPIC, payload.c_str(), true);
    }
  }
}

void mqttLoop() {
  // Call this in loop() when connected to keep PubSubClient alive
  if (client.connected()) {
    client.loop();
  }
}
