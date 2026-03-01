#include <display.h>
#include <png.h>
#include <wifiAndMQTT.h>
#include "decode.h"
#include "incomingImage.h"

void setup(){
  Serial.begin(115200);
  // pinMode(48, INPUT_PULLDOWN); // set onboard led to low
  displayInit();
  testDisplay();
  delay(500);
  wifiInit();
  mqttInit();
  initTime();
  pngInit();
}

void loop(){
  wifiReconnectCheck();
  mqttReconnectCheck();
  mqttLoop();
  uint8_t* bytes = nullptr;
  size_t len = 0;
  String id, mime;
  // clockScreen(); 
  handleIncomingImageOnce();
  // renderScreen();
  // drawPNG("/monkey.png", 0, 0);
  // dma_display->flipDMABuffer();
  // drawColors();
}