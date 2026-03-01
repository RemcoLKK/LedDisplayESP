#include <Arduino.h>
#include <display.h>
#include <FastLED.h>
#include <time.h>

uint16_t time_counter = 0, cycles = 0, fps = 0;
unsigned long fps_timer;

CRGB currentColor;
CRGBPalette16 palettes[] = {HeatColors_p, LavaColors_p, RainbowColors_p, RainbowStripeColors_p, CloudColors_p};
CRGBPalette16 currentPalette = palettes[0];


CRGB ColorFromCurrentPalette(uint8_t index = 0, uint8_t brightness = 255, TBlendType blendType = LINEARBLEND) {
  return ColorFromPalette(currentPalette, index, brightness, blendType);
}

void renderScreen() {
  virtualDisp->clearScreen();
  virtualDisp->drawPixel(0, 0, virtualDisp->color565(0, 0, 255)); // blue background
  virtualDisp->drawPixel(127, 0, virtualDisp->color565(0, 0, 255)); // blue background
  virtualDisp->drawPixel(0, 127, virtualDisp->color565(0, 0, 255)); // blue background
  virtualDisp->drawPixel(127, 127, virtualDisp->color565(0, 0, 255)); // blue background
  virtualDisp->flipDMABuffer();
}

void initTime() {
  configTzTime(
      "CET-1CEST,M3.5.0/02,M10.5.0/03",
      "pool.ntp.org"
  );

  Serial.println("Waiting for NTP time sync...");

  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    delay(1000);
  }

  Serial.println("Time synced!");
}

void clockScreen() {
  static int lastMin = -1;
  static int lastMday = -1;

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    // alleen bij statusverandering zou je dit moeten tekenen, maar ok:
    virtualDisp->clearScreen();
    virtualDisp->setTextColor(virtualDisp->color565(255,255,255));
    virtualDisp->setTextSize(2);
    virtualDisp->setCursor(0, 0);
    virtualDisp->print("No time");
    virtualDisp->flipDMABuffer();
    return;
  }

  // Alleen redraw als er echt iets verandert
  if (timeinfo.tm_min == lastMin && timeinfo.tm_mday == lastMday) {
    return; // niks doen -> geen tearing
  }
  lastMin  = timeinfo.tm_min;
  lastMday = timeinfo.tm_mday;

  char timeStr[6];   // HH:MM
  char dateStr[11];  // DD-MM-YYYY
  strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo);
  strftime(dateStr, sizeof(dateStr), "%d-%m-%Y", &timeinfo);

  int w = virtualDisp->width();
  int h = virtualDisp->height();

  uint16_t white = virtualDisp->color565(255,255,255);
  uint16_t black = virtualDisp->color565(0,0,0);

  virtualDisp->clearScreen();

  // --- Datum bovenaan (size 1) ---
  virtualDisp->setTextSize(1);
  int dateWidth = (int)strlen(dateStr) * 6;
  int dateX = (w - dateWidth) / 2;
  int dateY = 2;

  // Wis alleen de datum-regio (hoogte 8px bij size 1)
  virtualDisp->fillRect(0, dateY, w, 8, black);
  virtualDisp->setTextColor(white);
  virtualDisp->setCursor(dateX, dateY);
  virtualDisp->print(dateStr);

  // --- Tijd gecentreerd (size 3) ---
  virtualDisp->setTextSize(3);
  int ts = 3;
  int timeWidth  = (int)strlen(timeStr) * 6 * ts;
  int timeHeight = 8 * ts;

  int timeX = (w - timeWidth) / 2;
  int timeY = (h - timeHeight) / 2;

  // Wis alleen tijd-regio
  virtualDisp->fillRect(0, timeY, w, timeHeight, black);
  virtualDisp->setCursor(timeX, timeY);
  virtualDisp->print(timeStr);

  virtualDisp->flipDMABuffer();
}

void statusScreen(int status) {
  int x = status;
  if (x == 0) {
    return;
  } else if (x == 1) {
    virtualDisp->clearScreen();
    virtualDisp->setTextColor(virtualDisp->color565(255,255,255));
    virtualDisp->setTextSize(1);
    virtualDisp->setCursor(1, 1);
    virtualDisp->print("WiFi Connecting");
    virtualDisp->flipDMABuffer();
  } else if (x == 2) {
    virtualDisp->clearScreen();
    virtualDisp->setTextColor(virtualDisp->color565(255,255,255));
    virtualDisp->setTextSize(1);
    virtualDisp->setCursor(1, 1);
    virtualDisp->print("MQTT Connecting");
    virtualDisp->flipDMABuffer();
  } else if (x == 3) {
    virtualDisp->clearScreen();
    virtualDisp->setTextColor(virtualDisp->color565(255,255,255));
    virtualDisp->setTextSize(1);
    virtualDisp->setCursor(1, 1);
    virtualDisp->print("Wifi Error");
    virtualDisp->flipDMABuffer();
  } else if (x == 4) {
    virtualDisp->clearScreen();
    virtualDisp->setTextColor(virtualDisp->color565(255,255,255));
    virtualDisp->setTextSize(1);
    virtualDisp->setCursor(1, 1);
    virtualDisp->print("MQTT Error");
    virtualDisp->flipDMABuffer();
  } else {
    virtualDisp->print("Unknown Error");
  }
}

void drawPenis() {
  virtualDisp->fillCircle(59, 26, 5, virtualDisp->color565(255, 0, 0)); //ball
  virtualDisp->fillCircle(68, 26, 5, virtualDisp->color565(255, 0, 0)); //ball
  virtualDisp->fillRect(60, 8, 8, 15, virtualDisp->color565(255, 0, 0)); // shaft
  virtualDisp->fillCircle(64, 7, 5, virtualDisp->color565(255, 0, 0)); //head
  virtualDisp->fillCircle(63, 7, 5, virtualDisp->color565(255, 0, 0)); //head 
  virtualDisp->fillRect(63, 0, 2, 3, virtualDisp->color565(0, 0, 0)); // cutout
}

static bool plasmaStarted = false;

void drawColors() {
  if (!plasmaStarted) {
    plasmaStarted = true;
    currentPalette = RainbowColors_p;
    Serial.println("Starting plasma effect...");
    fps_timer = millis();
  }

  const int W = 128;
  const int H = 128;

  // Compute once per frame (faster)
  uint8_t wibble = sin8(time_counter);

  for (int x = 0; x < W; x++) {
    for (int y = 0; y < H; y++) {
      int16_t v = 128;

      v += sin16(x * wibble * 3 + time_counter);
      v += cos16(y * (128 - wibble) + time_counter);
      v += sin16((y * x * (int16_t)cos8((uint8_t)(-time_counter))) / 8);

      currentColor = ColorFromPalette(currentPalette, (uint8_t)(v >> 8));
      virtualDisp->drawPixelRGB888(x, y, currentColor.r, currentColor.g, currentColor.b);
    }
  }

  // Show the completed frame (once!)
  dma_display->flipDMABuffer();

  ++time_counter;
  ++cycles;
  ++fps;

  if (cycles >= 1024) {
    time_counter = 0;
    cycles = 0;
    currentPalette = palettes[random(0, sizeof(palettes) / sizeof(palettes[0]))];
  }

  if (fps_timer + 5000 < millis()) {
    Serial.printf_P(PSTR("Effect fps: %d\n"), fps / 5);
    fps_timer = millis();
    fps = 0;
  }
}
