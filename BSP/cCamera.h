// camera.h
#pragma once
#include "stm32746g_discovery.h"
#include "cLcd.h"

class cCamera {
public:
  void init();
  void start (bool captureMode, uint32_t buffer);

  uint32_t getWidth() { return mWidth; }
  uint32_t getHeight() { return mHeight; }
  bool getCaptureMode() { return mCaptureMode; }

  void setFocus (int value);

private:
  void gpioInit();
  void mt9d111Init();
  void dcmiInit (DCMI_HandleTypeDef* dcmi);
  void dcmiStart (DCMI_HandleTypeDef* dcmi, uint32_t DCMI_Mode, uint32_t data, uint32_t length);

  void preview();
  void capture();

  DMA_HandleTypeDef dmaHandler;
  uint32_t cameraCurrentResolution = 0;
  uint32_t mWidth = 0;
  uint32_t mHeight = 0;
  bool mCaptureMode = false;
  };
