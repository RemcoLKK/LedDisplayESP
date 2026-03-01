#ifndef PNG_H
#define PNG_H

void pngInit();
void listFiles();
bool drawPNG(const char* path, int x, int y);

bool drawPNGFromBuffer(const uint8_t* data, size_t len, int x, int y);

#endif // PNG_H