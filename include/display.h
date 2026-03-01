#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <ESP32-HUB75-VirtualMatrixPanel_T.hpp>

#define PANEL_RES_X     64     // Number of pixels wide of each INDIVIDUAL panel module. 
#define PANEL_RES_Y     64     // Number of pixels tall of each INDIVIDUAL panel module.

#define VDISP_NUM_ROWS      2 // Number of rows of individual LED panels 
#define VDISP_NUM_COLS      2 // Number of individual LED panels per row

#define PANEL_CHAIN_LEN     (VDISP_NUM_ROWS*VDISP_NUM_COLS)  // Don't change
#define PANEL_CHAIN_TYPE    CHAIN_TOP_RIGHT_DOWN
#define OE      12
#define CLK     11
#define CH_C    10
#define CH_A    9
#define BL2     8
#define RL2     18
#define BL1     17
#define RL1     16

#define LAT     13
#define CH_D    14
#define CH_B    21
#define CH_E    47
#define GL2     38
#define GL1     39

extern MatrixPanel_I2S_DMA *dma_display;
extern VirtualMatrixPanel_T<PANEL_CHAIN_TYPE>* virtualDisp;

void displayInit();
void testDisplay();
void renderScreen();
void funSettings();

void statusScreen(int status = 0);

void initTime();
void clockScreen(); 

void drawPenis();
void drawColors();

#endif // DISPLAY_H