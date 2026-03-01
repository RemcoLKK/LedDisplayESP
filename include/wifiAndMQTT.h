#ifndef WIFI_AND_MQTT_H
#define WIFI_AND_MQTT_H

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

static const char* WIFI_SSID = "Remco's netwerk 2,4G";
static const char* WIFI_PASS = "Bananenplant10";

// HiveMQ Cloud
static const char* MQTT_HOST = "cc0f333c4acf4560a6c5d7d55405eab4.s1.eu.hivemq.cloud"; 
static const uint16_t MQTT_PORT = 8883;
static const char* MQTT_USER = "Remco2";
static const char* MQTT_PASS = "Remco121";

// ---- Configure topics ----
static const char* TOPIC_IMG = "/img/chunk";
static const char* MQTT_STATUS_TOPIC = "/esp32/status";

extern WiFiClientSecure espClient;  
extern PubSubClient client;

void wifiInit();
void wifiReconnectCheck();

void mqttInit();             
void mqttReconnectCheck();    
void mqttLoop();

#endif // WIFI_AND_MQTT_H