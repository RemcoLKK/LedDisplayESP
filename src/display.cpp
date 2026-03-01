#include <display.h>

MatrixPanel_I2S_DMA *dma_display = nullptr;
VirtualMatrixPanel_T<PANEL_CHAIN_TYPE>* virtualDisp = nullptr;
  
void displayInit(){
  HUB75_I2S_CFG::i2s_pins _pins={RL1, GL1, BL1, RL2, GL2, BL2, CH_A, CH_B, CH_C, CH_D, CH_E, LAT, OE, CLK};

  HUB75_I2S_CFG mxconfig(
  PANEL_RES_X,   
  PANEL_RES_Y,  
  PANEL_CHAIN_LEN,
  _pins
  );

  mxconfig.setPixelColorDepthBits(6); 
  mxconfig.latch_blanking   = 1; 
  mxconfig.i2sspeed         = HUB75_I2S_CFG::HZ_20M; 
  mxconfig.clkphase         = true;     
  mxconfig.double_buff      = true;   
  mxconfig.min_refresh_rate = 240; 

  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  dma_display->begin();
  dma_display->setBrightness8(255); //0-255
  dma_display->clearScreen();

  virtualDisp = new VirtualMatrixPanel_T<PANEL_CHAIN_TYPE>(VDISP_NUM_ROWS, VDISP_NUM_COLS, PANEL_RES_X, PANEL_RES_Y);

  virtualDisp->setDisplay(*dma_display);
}

void testDisplay() {
  // for (int y = 0; y < virtualDisp->height(); y++) {
  //     for (int x = 0; x < virtualDisp->width(); x++) {

  //     uint16_t color = virtualDisp->color565(96, 0, 0); // red

  //     if (x == 0)   color = virtualDisp->color565(0, 255, 0); // g
  //     if (x == (virtualDisp->width()-1)) color = virtualDisp->color565(0, 0, 255); // b

  //     virtualDisp->drawPixel(x, y, color);
  //     virtualDisp->flipDMABuffer(); 
  //     delay(1);
  //     }
  // }

  virtualDisp->drawLine(virtualDisp->width() - 1, virtualDisp->height() - 1, 0, 0, virtualDisp->color565(255, 255, 255));

  virtualDisp->print("Virtual Matrix Panel");

  delay(3000);
  virtualDisp->clearScreen();
  virtualDisp->drawDisplayTest(); // re draw text numbering on each screen to check connectivity   
  virtualDisp->flipDMABuffer(); // force flip to show the drawn test pattern
}

