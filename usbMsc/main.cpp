// main.cpp - hid class usb
//{{{  includes
#include "../common/system.h"
#include "../common/cLcd.h"
#include "../common/cTouch.h"

#include "usbd_msc.h"

#include "../FatFs/ff.h"
#include "../FatFs/diskio.h"

#include "../common/stm32746g_discovery_sd.h"
#include "../common/cLcd.h"
//}}}
const char* kVersion = "USB Msc 9/3/18";

//{{{
class cApp : public cTouch {
public:
  cApp (int x, int y) : cTouch (x,y) {}
  cLcd* getLcd() { return mLcd; }

  void run (bool keyboard);

protected:
  virtual void onProx (int x, int y, int z);
  virtual void onPress (int x, int y);
  virtual void onMove (int x, int y, int z);
  virtual void onScroll (int x, int y, int z);
  virtual void onRelease (int x, int y);

private:
  void readDirectory (char* path);

  cLcd* mLcd = nullptr;
  bool mButton = false;
  };
//}}}
cApp* gApp;

//{{{
void cApp::readDirectory (char* path) {

  FILINFO fno;
  DIR dir;
  int i, j;
  auto fr = f_opendir (&dir, path);
  if (fr == FR_OK) {
    for (i = 0; path[i]; i++);
    path[i++] = '/';

    while (true) {
      auto fr = f_readdir (&dir, &fno);
      if (fr != FR_OK || !fno.fname[0])
        break;
      if (_FS_RPATH && fno.fname[0] == '.')
        continue;

      j = 0;
      do {
        path[i+j] = fno.fname[j];
        } while (fno.fname[j++]);

      if (fno.fattrib & AM_DIR) {
        mLcd->debug (LCD_COLOR_GREEN, "%s", path);
        readDirectory (path);
        }
      else
        mLcd->debug (LCD_COLOR_WHITE, "- %s", path);
      }

    path[--i] = '\0';
    f_closedir (&dir);
    }
  }
//}}}

// init usbDevice library
//{{{
void cApp::run (bool keyboard) {

  mButton = BSP_PB_GetState (BUTTON_KEY);

  // init lcd
  mLcd = new cLcd (16);
  mLcd->init();

  mscInit (mLcd);

  FATFS sdFatFs;
  char sdPath[40] = "0:/";
  if (f_mount (&sdFatFs, (TCHAR const*)sdPath, 0) == FR_OK) {
      mLcd->debug (LCD_COLOR_WHITE, "mounted");
      char buff[256] = "/";
      readDirectory (buff);
      }
    else
      mLcd->debug (LCD_COLOR_RED, "not mounted");

  DWORD numFreeClusters;
  FATFS* fatFs;
  if (f_getfree ("0:", &numFreeClusters, &fatFs) != FR_OK)
    mLcd->debug (LCD_COLOR_WHITE, "f_getfree failed");
  else {
    int freeSectors = numFreeClusters * fatFs->csize;
    int totalSectors = (fatFs->n_fatent - 2) * fatFs->csize;
    mLcd->debug (LCD_COLOR_WHITE, "%d free of %d total", freeSectors/2, totalSectors/2);
    }

  mscStart();

  while (true) {
    pollTouch();
    mLcd->show (kVersion);
    mLcd->flip();
    }
  }
//}}}
//{{{
void cApp::onProx (int x, int y, int z) {

  if (x || y) {
    //uint8_t HID_Buffer[HID_IN_ENDPOINT_SIZE] = { 0,(uint8_t)x,(uint8_t)y,0 };
    // hidSendReport (&gUsbDevice, HID_Buffer);
    mLcd->debug (LCD_COLOR_MAGENTA, "onProx %d %d %d", x, y, z);
    }
  }
//}}}
//{{{
void cApp::onPress (int x, int y) {

  //uint8_t HID_Buffer[HID_IN_ENDPOINT_SIZE] = { 1,0,0,0 };
  //hidSendReport (&gUsbDevice, HID_Buffer);
  mLcd->debug (LCD_COLOR_GREEN, "onPress %d %d", x, y);
  }
//}}}
//{{{
void cApp::onMove (int x, int y, int z) {

  if (x || y) {
    //uint8_t HID_Buffer[HID_IN_ENDPOINT_SIZE] = { 1,(uint8_t)x,(uint8_t)y,0 };
    //hidSendReport (&gUsbDevice, HID_Buffer);
    mLcd->debug (LCD_COLOR_GREEN, "onMove %d %d %d", x, y, z);
    }
  }
//}}}
//{{{
void cApp::onScroll (int x, int y, int z) {
  mLcd->incScrollValue (y);
  }
//}}}
//{{{
void cApp::onRelease (int x, int y) {

  //uint8_t HID_Buffer[HID_IN_ENDPOINT_SIZE] = { 0,0,0,0 };
  //hidSendReport (&gUsbDevice, HID_Buffer);
  mLcd->debug (LCD_COLOR_GREEN, "onRelease %d %d", x, y);
  }
//}}}

//{{{
int main() {

  SCB_EnableICache();
  SCB_EnableDCache();
  HAL_Init();
  //{{{  config system clock
  // System Clock source            = PLL (HSE)
  // SYSCLK(Hz)                     = 216000000
  // HCLK(Hz)                       = 216000000
  // AHB Prescaler                  = 1
  // APB1 Prescaler                 = 4
  // APB2 Prescaler                 = 2
  // HSE Frequency(Hz)              = 25000000
  // PLL_M                          = 25
  // PLL_N                          = 432
  // PLL_P                          = 2
  // PLLSAI_N                       = 384
  // PLLSAI_P                       = 8
  // VDD(V)                         = 3.3
  // Main regulator output voltage  = Scale1 mode
  // Flash Latency(WS)              = 7

  // Enable HSE Oscillator and activate PLL with HSE as source
  RCC_OscInitTypeDef RCC_OscInitStruct;
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSIState = RCC_HSI_OFF;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 25;
  RCC_OscInitStruct.PLL.PLLN = 432;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 9;
  if (HAL_RCC_OscConfig (&RCC_OscInitStruct) != HAL_OK)
    while (true) {}

  // Activate the OverDrive to reach the 216 Mhz Frequency
  if (HAL_PWREx_EnableOverDrive() != HAL_OK)
    while (true) {}

  // Select PLL as system clock source and configure the HCLK, PCLK1 and PCLK2 clocks dividers
  RCC_ClkInitTypeDef RCC_ClkInitStruct;
  RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                                 RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
  if (HAL_RCC_ClockConfig (&RCC_ClkInitStruct, FLASH_LATENCY_7) != HAL_OK)
    while (true) {}
  //}}}
  BSP_PB_Init (BUTTON_KEY, BUTTON_MODE_GPIO);

  gApp = new cApp (BSP_LCD_GetXSize(), BSP_LCD_GetYSize());
  gApp->run (true);
  }
//}}}
