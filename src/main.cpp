#include <display.h>
#include <png.h>
#include <wifiAndMQTT.h>

static const char* TOPIC = "matrix/frame";

void setup(){
  Serial.begin(115200);
  // pinMode(48, OUTPUT);
  // digitalWrite(48, LOW); // set pin 48 low to disable the on-board LED (if present)
  displayInit();
  testDisplay();
  delay(500);
  wifiInit();
  mqttInit();
  // pngInit();
}

void loop(){
  wifiReconnectCheck();
  mqttReconnectCheck();
  mqttLoop();
  // renderScreen();
  // drawPNG("/monkey.png", 0, 0);
  // dma_display->flipDMABuffer();
  drawColors();
}