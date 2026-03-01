#pragma once
#include <Arduino.h>

bool drawRGB565FromBytesBE(const uint8_t* bytes, size_t len, int x = 0, int y = 0);
void handleIncomingImageOnce();   // haalt file uit decode.cpp en tekent indien type=image/rgb565