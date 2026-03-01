#pragma once
#include <Arduino.h>

// Call once after MQTT connect (and again after reconnect) to set callback + subscribe
void decodeMqttRegister();

// Returns true once a full file has been reconstructed.
// Ownership: returned buffer is malloc()'d; caller must free() it.
bool decodeTryGetCompletedFile(uint8_t** outBytes, size_t* outLen, String* outId, String* outMime);