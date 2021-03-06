// main.cpp - webcam
//{{{  includes
#include <deque>
#include "../common/system.h"
#include "../common/utils.h"
#include "cLcd.h"
#include "../common/cTouch.h"
#include "../common/cPs2.h"

#include "usbd_msc.h"
#include "../FatFs/ff.h"
#include "../FatFs/diskio.h"

#include "sdRam.h"
#include "cCamera.h"

#include "jpeglib.h"

#include "cmsis_os.h"
#include "cpuUsage.h"
#include "lwip/opt.h"
#include "lwip/netif.h"
#include "lwip/tcpip.h"
#include "lwip/dhcp.h"
#include "lwip/arch.h"
#include "lwip/api.h"
#include "sntp.h"

#include "ethernetif.h"
//}}}
std::string kVersion = "WebCam 30/4/18";
uint8_t*  kCamBuf    =  (uint8_t*)0xc0080000;
uint8_t*  kCamBufEnd =  (uint8_t*)0xc0700000;
uint16_t* kRgb565Buf = (uint16_t*)kCamBufEnd;

//{{{
const char k404Response[] =
  "HTTP/1.0 404 File not found\r\n"
  "Server: lwIP/1.3.1\r\n"
  "Content-type: text/html\r\n\r\n"

  "<html>"
    "<body>"
      "<h1>404 not found heading</h1>"
      "<p>404 not found paragraph</p>"
    "</body>"
  "</html>\r\n";
//}}}
//{{{
const char kJpegResponseHeader[] =
  "HTTP/1.0 200 OK\r\n"
  "Server: lwIP/1.3.1\r\n"
  "Content-type: image/jpeg\r\n\r\n"; // header + body follows
//}}}
//{{{
const char kBmpResponseHeader[] =
  "HTTP/1.0 200 OK\r\n"
  "Server: lwIP/1.3.1\r\n"
  "Content-type: image/bmp\r\n\r\n"; // header + body follows
//}}}
//{{{
const char kHtmlResponseHeader[] =
  "HTTP/1.0 200 OK\r\n"
  "Server: lwIP/1.3.1\r\n"
  "Content-type: text/html\r\n\r\n"; // header + body follows
//}}}
//{{{
const char kHtmlBody[] =
  "<!DOCTYPE html>"
  "<html lang=en-GB>"
    "<body>"
      "<h1>Colin's webcam</h1>"
      "<p style=color:green title=tooltip>800x600</p>"
      "<img src=cam.jpg alt=missing width=800 height=600>"
      "<button>Click me</button>"
      "<a href=img1.jpg>This is a link</a>"
      "<svg width=100 height=100>"
        "<circle cx=50 cy=50 r=40 stroke=green stroke-width=8 fill=yellow>"
      "</svg>"
    "</body>"
  "</html>\r\n";
//}}}

//{{{
class cApp : public cTouch, public cLcd {
public:
  const uint16_t kBoxWidth = 65;
  const uint16_t kBoxHeight = 36;
  //{{{
  class cBox {
  public:
    //{{{
    cBox (const std::string& name, uint16_t width, uint16_t height)
        : mName(name), mLayoutWidth(width), mLayoutHeight(height) {}
    //}}}
    virtual ~cBox() {}

    // gets
    std::string getName() const { return mName; }

    bool getEnabled() { return mEnabled; }
    bool getProxed() { return mProxed; }
    bool getPressed() { return mPressed; }
    bool getMoved() { return mMoved; }

    cPoint getSize() { return mRect.getSize(); }
    uint16_t getWidth() { return mRect.getWidth(); }
    uint16_t getHeight() { return mRect.getHeight(); }
    cPoint getTL() { return mRect.getTL(); }
    cPoint getTL (int16_t offset) { return mRect.getTL (offset); }
    cPoint getTR() { return mRect.getTR(); }
    cPoint getBL() { return mRect.getBL(); }
    cPoint getBR() { return mRect.getBR(); }
    cPoint getCentre() { return mRect.getCentre(); }
    uint16_t getCentreX() { return mRect.getCentreX(); }
    uint16_t getCentreY() { return mRect.getCentreY(); }

    //{{{
    cBox* setPos (cPoint pos) {
      mLayoutX = pos.x;
      mLayoutY = pos.y;
      layout();
      return this;
      }
    //}}}
    cBox* setEnable (bool enabled) { mEnabled = enabled; return this;  }
    cBox* setProxed (bool proxed) { mProxed = proxed; return this;  }
    cBox* setPressed (bool pressed) { mPressed = pressed; return this;  }
    cBox* setMoved (bool moved) { mMoved = moved; return this;  }

    // overrides
    //{{{
    virtual void layout() {

      mRect.left = (mLayoutX < 0) ? cLcd::getWidth() + mLayoutX : mLayoutX;
      if (mLayoutWidth > 0)
        mRect.right = mRect.left + mLayoutWidth;
      else if (mLayoutWidth == 0)
        mRect.right = getSize().x - mLayoutX;
      else // mLayoutWidth < 0
        mRect.right = cLcd::getHeight() + mLayoutWidth + mLayoutX;

      mRect.top = (mLayoutY < 0) ? cLcd::getHeight() + mLayoutY : mLayoutY;
      if (mLayoutHeight > 0)
        mRect.bottom = mRect.top + mLayoutHeight;
      else if (mLayoutHeight == 0)
        mRect.bottom = cLcd::getHeight() - mLayoutY;
      else // mLayoutHeight < 0
        mRect.bottom = cLcd::getHeight() + mLayoutHeight + mLayoutY;
      }
    //}}}
    virtual bool pick (cPoint pos) { return mRect.inside (pos); }
    virtual bool onProx (cPoint pos, uint8_t z) { return false; }
    virtual bool onPress (cPoint pos, uint8_t z)  { return false; }
    virtual bool onMove (cPoint pos, cPoint inc, uint8_t z)  { return false; }
    virtual bool onRelease (cPoint pos, uint8_t z) { return false; }
    virtual bool onProxExit() { return false; }
    //{{{
    virtual void onDraw (cLcd* lcd) {
      lcd->fillRect (mColor, mRect);
      lcd->displayString (mTextColor, mRect.getCentre(), mName.c_str(), cLcd::eTextCentreBox);
      }
    //}}}

  protected:
    std::string mName;

    bool mEnabled = true;
    bool mProxed = false;
    bool mPressed = false;
    bool mMoved = false;

    uint16_t mColor = LCD_COLOR_LIGHT_GREY;
    uint16_t mTextColor = LCD_COLOR_BLACK;

    uint16_t mLayoutWidth;
    uint16_t mLayoutHeight;
    uint16_t mLayoutX = 0;
    uint16_t mLayoutY = 0;

    cRect mRect;
    };
  //}}}

  //{{{
  cApp (int x, int y) : cTouch(x,y), cLcd((uint16_t*)SDRAM_DEVICE_ADDR, 14) {

    mLcd = this;
    init();

    //mPs2 = new cPs2 (mLcd);
    //mPs2->initKeyboard();
    //mscInit (mLcd);
    //mscStart();

    mCinfo.err = jpeg_std_error (&jerr);
    jpeg_create_decompress (&mCinfo);
    }
  //}}}
  //{{{
  ~cApp() {
    jpeg_destroy_decompress (&mCinfo);
    delete mLcd;
    }
  //}}}

  void adcInit();

  void run();

  cPs2* getPs2() { return mPs2; }
  cCamera* getCam() { return mCam; }
  //{{{
  int getCountFiles (std::string& dirPath) {

    mFiles = 0;
    countFiles (dirPath);
    return mFiles;
    }
  //}}}

  //{{{
  cBox* add (cBox* box, uint16_t x, uint16_t y) {
    mBoxes.push_back (box);
    box->setPos (cPoint(x,y));
    return box;
    }
  //}}}
  //{{{
  cBox* add (cBox* box) {
    auto lastBox = mBoxes.empty() ? nullptr : mBoxes.back();
    mBoxes.push_back (box);
    box->setPos (lastBox ? lastBox->getTR() + cPoint(2,0) : cPoint(0,0));
    return box;
    }
  //}}}
  //{{{
  cBox* addBelow (cBox* box) {

    auto lastBox = mBoxes.back();
    mBoxes.push_back (box);
    box->setPos (lastBox->getBL());
    return box;
    }
  //}}}
  //{{{
  cBox* addFirst (cBox* box, uint16_t x, uint16_t y) {

    mBoxes.push_front (box);
    box->setPos (cPoint(x,y));
    return box;
    }
  //}}}
  //{{{
  void removeBox (cBox* box) {

    for (auto boxIt = mBoxes.begin(); boxIt != mBoxes.end(); ++boxIt)
      if (*boxIt == box) {
        mBoxes.erase (boxIt);
        return;
        }
    }
  //}}}

  void onPs2Irq() { mPs2->onIrq(); }
  void touchThread();
  void saveThread();
  void serverThread (void* arg);
  void dhcpThread (void* arg);

protected:
  virtual void onTouchProx (cPoint pos, uint8_t z);
  virtual void onTouchPress (cPoint pos, uint8_t z);
  virtual void onTouchMove (cPoint pos, cPoint inc, uint8_t z);
  virtual void onTouchRelease (cPoint pos, uint8_t z);
  virtual void onKey (uint8_t ch, bool release);

private:
  void readDirectory (const std::string& dirPath);
  void countFiles (const std::string& dirPath);
  void reportFree();
  void reportLabel();

  void loadFile (const std::string& fileName, uint8_t* buf, uint16_t* rgb565Buf);
  void saveNumFile (const std::string& fileName, uint8_t* buf, int bufLen);
  void saveNumFile (const std::string& fileName, uint8_t* header, int headerLen, uint8_t* frame, int frameLen);
  void createNumFile (const std::string& fileName, uint8_t* header, int headerLen, uint8_t* frame, int frameLen);
  void appendFile (int num, uint8_t* header, int headerLen, uint8_t* frame, int frameLen);
  void closeFile();

  //{{{  vars
  cLcd* mLcd = nullptr;
  cPs2* mPs2 = nullptr;
  cCamera* mCam = nullptr;

  std::deque <cBox*> mBoxes;
  cBox* mPressedBox = nullptr;

  bool mMounted = false;
  std::string mLabel;
  int mFiles = 0;

  std::string mIpAddress;

  struct jpeg_error_mgr jerr;
  struct jpeg_decompress_struct mCinfo;

  bool mJpegChanged = false;
  bool mJpeg = false;

  bool mDebugChanged = false;
  bool mDebugValue = true;

  bool mClearDebugChanged = false;
  bool mFormatChanged = false;

  bool mFocusChanged = false;
  int mFocus = 0;

  bool mTakeChanged = false;
  bool mTakeMovieChanged = false;
  int bmpFrameNum = 0;
  int jpgFrameNum = 0;
  int mjpgFrameNum = 0;
  int mjpgFileNum = 1;
  //}}}
  };
//}}}

//{{{
class cCameraBox : public cApp::cBox {
public:
  //{{{
  cCameraBox (float width, float height, cCamera* cam)
      : cBox("bgnd", width, height), mCam(cam) {

    mCinfo.err = jpeg_std_error (&jerr);
    jpeg_create_decompress (&mCinfo);

    mZoom = height / (float)mCam->getHeight();
    mTextColor = LCD_COLOR_YELLOW;
    }
  //}}}
  //{{{
  virtual ~cCameraBox() {
    jpeg_destroy_decompress (&mCinfo);
    }
  //}}}

  //{{{
  bool onPress (cPoint pos, uint8_t z)  {
    //cLcd::mLcd->debug (LCD_COLOR_WHITE, "press %d %d %f", pos.x, pos.y, mZoom);
    mTarget = 1.f;
    mZoomCentre = getCentre() - pos;
    return true;
    }
  //}}}
  //{{{
  bool onMove (cPoint pos, cPoint inc, uint8_t z)  {
    mZoomCentre += inc;
    return true;
    }
  //}}}
  //{{{
  bool onRelease (cPoint pos, uint8_t z)  {
    mZoomCentre = {0,0};
    mTarget = getHeight() / (float)mCam->getHeight();
    return true;
    }
  //}}}
  //{{{
  void onDraw (cLcd* lcd) {

    mZoom += (mTarget - mZoom) * 0.5f;

    uint32_t frameId;
    auto frame = mCam->getLastFrame (mLastFrameLen, mLastJpeg, frameId);

    if (!frame)
      lcd->clear (LCD_COLOR_BLACK);
    else {
      if (!mLastJpeg) {
        // simple rgb565 from cam
        mLastFrameSize =  mCam->getSize();
        mSrcZoom = 1.f;
        }
      else if (frameId != mLastFrameId) {
        //{{{  new jpeg frame, jpegDecode it
        mSrcZoom = 2.f;

        uint32_t headerLen;
        auto header = mCam->getFullJpgHeader (6, headerLen);
        jpeg_mem_src (&mCinfo, header, headerLen);
        jpeg_read_header (&mCinfo, TRUE);

        // jpegBody
        mCinfo.scale_num = 1;
        mCinfo.scale_denom = 2;
        mCinfo.dct_method = JDCT_FLOAT;
        mCinfo.out_color_space = JCS_RGB;

        jpeg_mem_src (&mCinfo, frame, mLastFrameLen);
        jpeg_start_decompress (&mCinfo);
        uint8_t buf[mCinfo.output_width * 3];
        uint8_t* bufArray = buf;
        while (mCinfo.output_scanline < mCinfo.output_height) {
          jpeg_read_scanlines (&mCinfo, &bufArray, 1);
          lcd->rgb888to565cpu (bufArray, kRgb565Buf + mCinfo.output_scanline * mCinfo.output_width, mCinfo.output_width, 1);
          }
        jpeg_finish_decompress (&mCinfo);

        frame = (uint8_t*)kRgb565Buf;
        mLastFrameSize = cPoint (mCinfo.output_width, mCinfo.output_height);
        mLastFrameId = frameId;
        }
        //}}}
      lcd->zoom565 ((uint16_t*)frame, mZoomCentre, mLastFrameSize, cRect(lcd->getSize()), mZoom * mSrcZoom, mZoom * mSrcZoom);
      }

    char str[20];
    sprintf (str, "%d:%x:%02d %dfps", mCam->getFrameLen(), mCam->getStatus(), mCam->getDmaCount(), mCam->getFps());
    lcd->displayStringShadow (mTextColor, mRect.getBR(), str, cLcd::eTextBottomRight);
    }
  //}}}

private:
  cCamera* mCam;

  uint32_t mLastFrameLen = 0;
  bool mLastJpeg = false;
  uint32_t mLastFrameId = 0;
  cPoint mLastFrameSize = {0,0};

  float mTarget = 0.5f;
  float mZoom = 0.5f;
  float mSrcZoom = 1.0f;
  cPoint mZoomCentre = {0,0};

  struct jpeg_error_mgr jerr;
  struct jpeg_decompress_struct mCinfo;
  };
//}}}
//{{{
class cToggleBox : public cApp::cBox {
public:
  //{{{
  cToggleBox (float width, float height, const std::string& name, bool& value, bool& changed)
      : cBox(name, width, height), mValue(value), mChanged(changed) {
    mChanged = false;
    }
  //}}}
  virtual ~cToggleBox() {}

  //{{{
  bool onProx (cPoint pos) {
    return true;
    }
  //}}}
  //{{{
  bool onPress (cPoint pos, uint8_t z)  {
    mValue = !mValue;
    mThickness = z;
    mChanged = true;
    return true;
    }
  //}}}
  //{{{
  bool onMove (cPoint pos, cPoint inc, uint8_t z)  {
    mThickness = z;
    return true;
    }
  //}}}
  //{{{
  void onDraw (cLcd* lcd) {
    mColor = mValue ? LCD_COLOR_YELLOW : LCD_COLOR_LIGHT_GREY;
    mTextColor = mValue ? LCD_COLOR_BLACK : LCD_COLOR_WHITE;
    cBox::onDraw (lcd);
    if (mProxed)
      lcd->drawRect (LCD_COLOR_WHITE, mRect, mThickness < 10 ? 1 : mThickness / 10 );
    }
  //}}}

private:
  bool& mChanged;
  bool& mValue;
  uint16_t mThickness = 1;
  };
//}}}
//{{{
class cInstantBox : public cApp::cBox {
public:
  //{{{
  cInstantBox (float width, float height, const std::string& name, bool& changed)
      : cBox(name, width, height), mChanged(changed) {
    mChanged = false;
    }
  //}}}
  virtual ~cInstantBox() {}

  //{{{
  bool onProx (cPoint pos) {
    return true;
    }
  //}}}
  //{{{
  bool onPress (cPoint pos, uint8_t z)  {
    mThickness = z;
    mChanged = true;
    return true;
    }
  //}}}
  //{{{
  bool onMove (cPoint pos, cPoint inc, uint8_t z)  {
    mThickness = z;
    return true;
    }
  //}}}
  //{{{
  void onDraw (cLcd* lcd) {
    mColor = mPressed ? LCD_COLOR_YELLOW : LCD_COLOR_LIGHT_GREY;
    mTextColor = mPressed ? LCD_COLOR_BLACK : LCD_COLOR_WHITE;
    cBox::onDraw (lcd);
    if (mProxed)
      lcd->drawRect (LCD_COLOR_WHITE, mRect, mThickness < 10 ? 1 : mThickness / 10 );
    }
  //}}}

private:
  bool& mChanged;
  uint16_t mThickness = 1;
  };
//}}}
//{{{
class cValueBox : public cApp::cBox {
public:
  //{{{
  cValueBox (float width, float height, const std::string& name, int min, int max, int& value, bool& changed)
      : cBox(name, width, height), mMin(min), mMax(max), mValue(value), mChanged(changed) {
    mChanged = false;
    mColor = LCD_COLOR_GREEN;
    }
  //}}}
  virtual ~cValueBox() {}

  //{{{
  bool onProx (cPoint pos) {
    return true;
    }
  //}}}
  //{{{
  bool onPress (cPoint pos, uint8_t z)  {
    mThickness = z;
    mTextColor = LCD_COLOR_WHITE;
    mChanged = true;
    return true;
    }
  //}}}
  //{{{
  bool onMove (cPoint pos, cPoint inc, uint8_t z) {
    mThickness = z;
    setValue (mValue + inc.x - inc.y);
    return true;
    }
  //}}}
  //{{{
  bool onRelease (cPoint pos, uint8_t z) {
    mThickness = z;
    mTextColor = LCD_COLOR_BLACK;
    return true;
    }
  //}}}
  //{{{
  void onDraw (cLcd* lcd) {
    lcd->fillRect (mColor, mRect);

    char str[40];
    sprintf (str, "%s %d", mName, mValue);
    lcd->displayString (mTextColor, mRect.getCentre(), str, cLcd::eTextCentreBox);

    if (mProxed)
      lcd->drawRect (LCD_COLOR_WHITE, mRect, mThickness < 10 ? 1 : mThickness / 10 );
    }
  //}}}

private:
  //{{{
  void setValue (float value) {
    mValue = value;
    mValue = std::max (mValue, mMin);
    mValue = std::min (mValue, mMax);
    mChanged = true;
    }
  //}}}

  uint16_t mThickness = 1;
  int mMin = 0;
  int mMax = 100;

  int& mValue;
  bool& mChanged;
  };
//}}}

//{{{  adc defines
#define TEMP_REFRESH_PERIOD   1000    /* Internal temperature refresh period */
#define MAX_CONVERTED_VALUE   4095    /* Max converted value */
#define AMBIENT_TEMP            25    /* Ambient Temperature */
#define VSENS_AT_AMBIENT_TEMP  760    /* VSENSE value (mv) at ambient temperature */
#define AVG_SLOPE               25    /* Avg_Solpe multiply by 10 */
#define VREF                  3300
//}}}

FATFS gFatFs = { 0 };  // encourges allocation in lower DTCM SRAM
FIL   gFile = { 0 };
cApp* gApp = { 0 };

ADC_HandleTypeDef AdcHandle;
__IO int32_t ConvertedValue = 0;

extern "C" {
  void EXTI9_5_IRQHandler() { gApp->onPs2Irq(); }
  void DMA2_Stream0_IRQHandler() { HAL_DMA_IRQHandler (AdcHandle.DMA_Handle); }
  }

//{{{
void cApp::adcInit() {


  //AdcHandle.Instance                   = ADC1;
  AdcHandle.Instance                   = ADC3;
  AdcHandle.Init.ClockPrescaler        = ADC_CLOCKPRESCALER_PCLK_DIV4;
  AdcHandle.Init.Resolution            = ADC_RESOLUTION_12B;
  AdcHandle.Init.ScanConvMode          = DISABLE;  // Sequencer disabled - ADC conversion on 1 channel on rank 1
  AdcHandle.Init.ContinuousConvMode    = ENABLE;   // Continuous mode enabled to have continuous conversion
  AdcHandle.Init.DiscontinuousConvMode = DISABLE;  // Parameter discarded because sequencer is disabled
  AdcHandle.Init.NbrOfDiscConversion   = 0;
  AdcHandle.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_NONE;  // Conversion start trigged at each external event
  AdcHandle.Init.ExternalTrigConv      = ADC_EXTERNALTRIGCONV_T1_CC1;
  AdcHandle.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
  AdcHandle.Init.NbrOfConversion       = 1;
  AdcHandle.Init.DMAContinuousRequests = ENABLE;
  AdcHandle.Init.EOCSelection          = DISABLE;
  if (HAL_ADC_Init (&AdcHandle) != HAL_OK)
    debug (LCD_COLOR_GREEN, "HAL_ADC_Init failed");

  ADC_ChannelConfTypeDef sConfig;
  sConfig.Channel = ADC_CHANNEL_TEMPSENSOR;
  sConfig.Channel = ADC_CHANNEL_VBAT;
  //sConfig.Channel = ADC_CHANNEL_VREFINT;
  sConfig.Channel      = ADC_CHANNEL_8;
  sConfig.Rank         = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_144CYCLES; // ADC_SAMPLETIME_3CYCLES;
  sConfig.Offset       = 0;

  //Configure ADC Temperature Sensor Channel
  //sConfig.Rank = 1;
  //sConfig.SamplingTime = ADC_SAMPLETIME_56CYCLES;
  //sConfig.Offset = 0;

  if (HAL_ADC_ConfigChannel (&AdcHandle, &sConfig) != HAL_OK)
    debug (LCD_COLOR_GREEN, "HAL_ADC_ConfigChannel failed");
  }
//}}}
//{{{
void HAL_ADC_MspInit (ADC_HandleTypeDef *hadc) {

  static DMA_HandleTypeDef  hdma_adc;

  __HAL_RCC_ADC1_CLK_ENABLE();
  __HAL_RCC_ADC3_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_DMA2_CLK_ENABLE();

  // ADC Channel GPIO pin configuration
  GPIO_InitTypeDef GPIO_InitStruct;
  GPIO_InitStruct.Pin = GPIO_PIN_10;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

  // Set the parameters to be configured
  hdma_adc.Instance = DMA2_Stream0;
  hdma_adc.Init.Channel  = DMA_CHANNEL_2;
  hdma_adc.Init.Direction = DMA_PERIPH_TO_MEMORY;
  hdma_adc.Init.PeriphInc = DMA_PINC_DISABLE;
  hdma_adc.Init.MemInc = DMA_MINC_ENABLE;
  hdma_adc.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
  hdma_adc.Init.MemDataAlignment = DMA_MDATAALIGN_WORD;
  hdma_adc.Init.Mode = DMA_CIRCULAR;
  hdma_adc.Init.Priority = DMA_PRIORITY_HIGH;
  hdma_adc.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
  hdma_adc.Init.FIFOThreshold = DMA_FIFO_THRESHOLD_HALFFULL;
  hdma_adc.Init.MemBurst = DMA_MBURST_SINGLE;
  hdma_adc.Init.PeriphBurst = DMA_PBURST_SINGLE;
  HAL_DMA_Init (&hdma_adc);
  __HAL_LINKDMA (hadc, DMA_Handle, hdma_adc);

  // NVIC configuration for DMA transfer complete interrupt
  HAL_NVIC_SetPriority (DMA2_Stream0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ (DMA2_Stream0_IRQn);
  }
//}}}

// public
//{{{
void cApp::run() {

  adcInit();

  //diskDebugEnable();
  mMounted = !f_mount (&gFatFs, "", 1);
  if (mMounted) {
    //{{{  get label
    char label[20] = {0};
    DWORD vsn = 0;
    f_getlabel ("", label, &vsn);
    mLabel = label;
    }
    //}}}
  else
    debug (LCD_COLOR_GREEN, "sdCard not mounted");

  mCam = new cCamera (kCamBuf, kCamBufEnd);
  mCam->init();

  // define menu
  add (new cCameraBox (getWidth(), getHeight(), mCam));
  add (new cToggleBox (kBoxWidth,kBoxHeight, "jpeg", mJpeg, mJpegChanged), 0,getHeight()-kBoxHeight);
  add (new cToggleBox (kBoxWidth,kBoxHeight, "info", mDebugValue, mDebugChanged));
  add (new cInstantBox (kBoxWidth,kBoxHeight, "clear", mClearDebugChanged));
  add (new cValueBox (kBoxWidth,kBoxHeight, "f", 0,254, mFocus, mFocusChanged));
  if (mMounted) {
    //{{{  add boxes
    add (new cInstantBox (kBoxWidth,kBoxHeight, "snap", mTakeChanged));
    add (new cInstantBox (kBoxWidth,kBoxHeight, "movie", mTakeMovieChanged));
    add (new cInstantBox (kBoxWidth,kBoxHeight, "format", mFormatChanged));
    }
    //}}}
  if (mMounted) {
    //{{{  load splash piccy
    std::string path1 = "";
    readDirectory (path1);

    loadFile ("splash.jpg", kCamBuf, kRgb565Buf);
    char path[40] = "/";
    }
    //}}}
  //diskDebugDisable();

  HAL_ADC_Start (&AdcHandle);
  //HAL_ADC_StartDMA (&AdcHandle, (uint32_t*)&ConvertedValue, 1);

  int32_t avVal = 0;
  while (true) {
    //{{{  removed
    //while (mPs2->hasChar()) {
    //  auto ch = mPs2->getChar();
    //  onKey (ch & 0xFF, ch & 0x100);
    //  }
    //}}}
    // draw
    //if (BSP_PB_GetState (BUTTON_KEY))
    //  mBoxes.front()->onDraw (mLcd);
    //else {
    //if ((osKernelSysTick() % 50) == 0) {
    //  int32_t JTemp = ((((ConvertedValue * VREF)/MAX_CONVERTED_VALUE) - VSENS_AT_AMBIENT_TEMP) * 10 / AVG_SLOPE) + AMBIENT_TEMP;
    //  debug (LCD_COLOR_YELLOW, "temp %d %d", ConvertedValue, JTemp);
    //  }

    HAL_ADC_PollForConversion (&AdcHandle, 100);
    ConvertedValue = HAL_ADC_GetValue (&AdcHandle);
    if (avVal == 0)
      avVal = ConvertedValue;
    else
      avVal = (avVal*50 + ConvertedValue) / 51;
    float kScale = ((3.3f * (39.f + 27.f) / 39.f) / 4096.f) * 1000;
    //float kScale = (3.3f / 4096.f) * 1000;
    //float kScale = 3.f * (3.3f / 4096.f) * 1000;

    //BSP_LED_Toggle (LED1);
    for (auto box : mBoxes)
      box->onDraw (mLcd);
    drawInfo (LCD_COLOR_WHITE, cLcd::eTextLeft,
      (kVersion + " " + (mMounted ? mLabel : "") + " " + mIpAddress + " " + dec(ConvertedValue) + " " +
       dec(int(avVal*kScale) / 1000) + "." + dec(int(avVal*kScale) % 1000, 3)).c_str());
    drawInfo (LCD_COLOR_YELLOW, cLcd::eTextRight, "%d %d%%", xPortGetFreeHeapSize(), osGetCPUUsage());
    if (mDebugValue)
      drawDebug();
    //  }
    present();
    }
  }
//}}}
//{{{
void cApp::touchThread() {

  while (true) {
    pollTouch();

    if (mJpegChanged) {
      //{{{  jpeg
      mJpegChanged = false;
      mJpeg ? mCam->capture() : mCam->preview();
      mjpgFrameNum = 0;
      }
      //}}}
    else if (mFocusChanged) {
      //{{{  changeFocus
      mFocusChanged = false;
      mCam->setFocus (mFocus);
      }
      //}}}
    else if (mClearDebugChanged) {
      //{{{  clearDebug
      mClearDebugChanged = false;
      clearDebug();
      }
      //}}}
    else
      osDelay (40);
    }
  }
//}}}
//{{{
void cApp::saveThread() {

  while (true) {
    if (mMounted && (mTakeChanged || mTakeMovieChanged || BSP_PB_GetState (BUTTON_KEY))) {
      uint32_t frameLen;
      bool jpeg;
      auto frame = mCam->getNextFrame (frameLen, jpeg);
      if (frame) {
        if (BSP_PB_GetState (BUTTON_KEY) || mTakeChanged) {
          if (jpeg) {
            //{{{  save JFIF .jpg
            mTakeChanged = false;

            uint32_t headerLen;
            auto header = mCam->getFullJpgHeader (6, headerLen);
            saveNumFile ("save" + dec(jpgFrameNum++, 3) + ".jpg", header, headerLen, frame, frameLen);
            }
            //}}}
          else {
            //{{{  save rgb565 .bmp
            mTakeChanged = false;

            uint32_t headerLen;
            auto header = mCam->getBmpHeader (headerLen);
            saveNumFile ("save" + dec(bmpFrameNum++, 3) + ".bmp", header, headerLen, frame, frameLen);
            }
            //}}}
          }
        else if (!mjpgFrameNum) {
          //{{{  create .mjpeg
          mjpgFrameNum++;
          uint32_t headerLen;
          auto header = mCam->getFullJpgHeader (6, headerLen);
          createNumFile ("save" + dec(mjpgFileNum++,3) + ".mjpeg", header, headerLen, frame, frameLen);
          }
          //}}}
        else if (mjpgFrameNum < 500) {
          //{{{  append .mjpeg frame
          uint32_t headerLen;
          auto header = mCam->getSmallJpgHeader (6, headerLen);
          appendFile (mjpgFrameNum++, header, headerLen, frame, frameLen);
          }
          //}}}
        else  {
          //{{{  close .mjpeg
          mTakeMovieChanged = false;

          mjpgFrameNum++;
          closeFile();
          }
          //}}}
        }
      }
    else if (mFormatChanged) {
      //{{{  format sdCard
      mFormatChanged = false;
      debug (LCD_COLOR_YELLOW, "formatting sdCard");

      void* work = malloc (0x10000);
      f_mkfs ("", FM_EXFAT, 0, work, 0x10000);
      free (work);

      f_setlabel ("exfat");
      f_mount (&gFatFs, "", 1);

      debug (LCD_COLOR_YELLOW, "sdCard formatted");
      }
      //}}}
    else
      osDelay (20);
    }
  }
//}}}
//{{{
void cApp::serverThread (void* arg) {
// minimal http server

  // create a new TCP connection handle
  struct netconn* connection = netconn_new (NETCONN_TCP);
  if (connection != NULL) {
    // bind to port 80 (HTTP) with default IP address
    if (netconn_bind (connection, NULL, 80) == ERR_OK) {
      netconn_listen (connection);
      while (true) {
        struct netconn* request;
        if (netconn_accept (connection, &request) == ERR_OK) {
          struct netbuf* requestNetBuf;
          if (netconn_recv (request, &requestNetBuf) == ERR_OK) {
            if (netconn_err (request) == ERR_OK) {
              char* buf;
              u16_t bufLen;
              netbuf_data (requestNetBuf, (void**)&buf, &bufLen);

              //{{{  debug request buf
              char str[40];
              int src = 0;
              int dst = 0;

              // copy till return
              while ((src < bufLen) && (buf[src] != 0x0d)) {
                if (buf[src] == 0x0a) // skip lineFeed
                  src++;
                else if (dst < 39)
                  str[dst++] = buf[src++];
                }

              // terminate str
              str[dst] = 0;

              debug (LCD_COLOR_YELLOW, str);
              //}}}

              // simple HTTP GET command parser
              bool ok = false;
              if ((bufLen >= 5) && !strncmp (buf, "GET /", 5)) {
                if (!strncmp (buf, "GET / ", 6)) {
                  //{{{  html
                  netconn_write (request, kHtmlResponseHeader, sizeof(kHtmlResponseHeader)-1, NETCONN_NOCOPY);
                  netconn_write (request, kHtmlBody, sizeof(kHtmlBody)-1, NETCONN_NOCOPY);
                  ok = true;
                  }
                  //}}}
                else if (!strncmp (buf, "GET /cam.jpg", 12)) {
                  //{{{  cam.jpg
                  uint32_t frameLen;
                  bool jpeg;
                  uint32_t frameId;
                  auto frame = mCam->getLastFrame (frameLen, jpeg, frameId);
                  if (frame) {
                    // send http response header
                    netconn_write (request,
                                   jpeg ? kJpegResponseHeader : kBmpResponseHeader,
                                   jpeg ? sizeof(kJpegResponseHeader)-1 : sizeof(kBmpResponseHeader)-1,
                                   NETCONN_NOCOPY);

                    // send imageFile format header
                    uint32_t headerLen;
                    auto header = jpeg ? mCam->getFullJpgHeader (6, headerLen) : mCam->getBmpHeader (headerLen);
                    netconn_write (request, header, headerLen, NETCONN_NOCOPY);

                    // send imageFile body
                    netconn_write (request, frame, frameLen, NETCONN_NOCOPY);
                    ok = true;
                    }
                  }
                  //}}}
                if (!ok)
                  netconn_write (request, k404Response, sizeof(k404Response)-1, NETCONN_NOCOPY);
                }
              }
            }

          netconn_close (request);
          netbuf_delete (requestNetBuf);
          netconn_delete (request);
          }
        }
      }
    }
  }
//}}}
//{{{
void cApp::dhcpThread (void* arg) {

  enum eDhcpState { DHCP_OFF, DHCP_START, DHCP_WAIT_ADDRESS, DHCP_ADDRESS_ASSIGNED, DHCP_TIMEOUT, DHCP_LINK_DOWN };

  auto netif = (struct netif*)arg;

  uint8_t dhcpState = netif_is_up (netif) ? DHCP_START : DHCP_LINK_DOWN;
  while (true) {
    switch (dhcpState) {
      case DHCP_START:
        ip_addr_set_zero_ip4 (&netif->ip_addr);
        ip_addr_set_zero_ip4 (&netif->netmask);
        ip_addr_set_zero_ip4 (&netif->gw);
        dhcp_start (netif);
        dhcpState = DHCP_WAIT_ADDRESS;
        break;

      case DHCP_WAIT_ADDRESS:
        if (dhcp_supplied_address (netif)) {
          dhcpState = DHCP_ADDRESS_ASSIGNED;
          mIpAddress = ip4addr_ntoa ((const ip4_addr_t*)&netif->ip_addr);
          cLcd::mLcd->debug (LCD_COLOR_GREEN, "dhcp " + mIpAddress);
          sntpSetServerName (0, "pool.ntp.org");
          sntpInit();
          }
        else {
          auto dhcp = (struct dhcp*)netif_get_client_data (netif, LWIP_NETIF_CLIENT_DATA_INDEX_DHCP);
          if (dhcp->tries > 4) {
            dhcpState = DHCP_TIMEOUT;
            dhcp_stop (netif);
            cLcd::mLcd->debug (LCD_COLOR_RED, "dhcp timeout");
            }
          }
        break;

      case DHCP_LINK_DOWN:
        cLcd::mLcd->debug (LCD_COLOR_RED, "dhcp link down");
        dhcp_stop (netif);
        dhcpState = DHCP_OFF;
        break;

      default:
        break;
      }

    osDelay (250);
    }
  }
//}}}

// protected
//{{{
void cApp::onTouchProx (cPoint pos, uint8_t z) {

  // search for prox in reverse draw order
  cBox* proxBox = nullptr;
  for (auto boxIt = mBoxes.rbegin(); boxIt != mBoxes.rend(); ++boxIt) {
    bool wasProxed = (*boxIt)->getProxed();
    (*boxIt)->setProxed ((*boxIt)->getEnabled() && (*boxIt)->pick (pos));
    if (!proxBox && !wasProxed && (*boxIt)->getProxed()) {
      // transition box to prox
      proxBox = *boxIt;
      proxBox->onProx (pos - proxBox->getTL(), z);
      }
    else if (wasProxed && !(*boxIt)->getProxed())
      // transition box to notProxed
      (*boxIt)->onProxExit();
    }
  }
//}}}
//{{{
void cApp::onTouchPress (cPoint pos, uint8_t z) {

  // search for pressed in reverse draw order
  mPressedBox = nullptr;
  for (auto boxIt = mBoxes.rbegin(); boxIt != mBoxes.rend(); ++boxIt) {
    (*boxIt)->setProxed ((*boxIt)->getEnabled() && (*boxIt)->pick (pos));
    if (!mPressedBox && (*boxIt)->getProxed()) {
      // transition to press
      mPressedBox = *boxIt;
      mPressedBox->setPressed (true);
      mPressedBox->setMoved (false);
      mPressedBox->onPress (pos - mPressedBox->getTL(), z);
      }
    }
  }
//}}}
//{{{
void cApp::onTouchMove (cPoint pos, cPoint inc, uint8_t z) {

  if (mPressedBox && mPressedBox->getPressed()) {
    mPressedBox->setMoved (true);
    mPressedBox->onMove (pos - mPressedBox->getTL(), inc, z);
    }
  }
//}}}
//{{{
void cApp::onTouchRelease (cPoint pos, uint8_t z) {

  if (mPressedBox) {
    mPressedBox->setProxed (false);
    mPressedBox->setPressed (false);
    mPressedBox->onRelease (pos - mPressedBox->getTL(), z);
    }
  mPressedBox = nullptr;
  }
//}}}
//{{{
void cApp::onKey (uint8_t ch, bool release) {

  //debug (LCD_COLOR_GREEN, "onKey %x %s", ch, release ? "release" : "press");
  if (ch == 0x51) // down arrow
    incScrollIndex (-1);
  else if (ch == 0x52) // up arrow
    incScrollIndex (1);
  else if (ch == 0x4e) // pagedown
    incScrollIndex (-16);
  else if (ch == 0x4b) // pageup
    incScrollIndex (16);
  }
//}}}

// private
//{{{
void cApp::readDirectory (const std::string& dirPath) {

  DIR dir;
  if (f_opendir (&dir, dirPath.c_str()) == FR_OK) {
    while (true) {
      FILINFO filinfo;
      if (f_readdir (&dir, &filinfo) != FR_OK || !filinfo.fname[0])
        break;
      if (filinfo.fname[0] == '.')
        continue;

      std::string filePath = dirPath + "/" + filinfo.fname;
      if (filinfo.fattrib & AM_DIR) {
        debug (LCD_COLOR_GREEN, filePath);
        readDirectory (filePath);
        }
      else
        debug (LCD_COLOR_WHITE, filePath);
      }

    f_closedir (&dir);
    }
  }
//}}}
//{{{
void cApp::countFiles (const std::string& dirPath) {

  DIR dir;
  if (f_opendir (&dir, dirPath.c_str()) == FR_OK) {
    while (true) {
      FILINFO filinfo;
      if (f_readdir (&dir, &filinfo) != FR_OK || !filinfo.fname[0])
        break;
      if (filinfo.fname[0] == '.')
        continue;

      if (filinfo.fattrib & AM_DIR)
        countFiles (dirPath + "/" + filinfo.fname);
      else
        mFiles++;
      }

    f_closedir (&dir);
    }
  }
//}}}
//{{{
void cApp::reportFree() {

  DWORD freeClusters;
  FATFS* fatFs;
  if (f_getfree ("0:", &freeClusters, &fatFs) != FR_OK)
    debug (LCD_COLOR_WHITE, "f_getfree failed");
  else {
    int freeSectors = freeClusters * fatFs->csize;
    int totalSectors = (fatFs->n_fatent - 2) * fatFs->csize;
    debug (LCD_COLOR_WHITE, "%d free of %d total", freeSectors/2, totalSectors/2);
    }
  }
//}}}

//{{{
void cApp::loadFile (const std::string& fileName, uint8_t* buf, uint16_t* rgb565Buf) {

  FILINFO filInfo;
  if (f_stat (fileName.c_str(), &filInfo)) {
    debug (LCD_COLOR_RED, "%s not found", fileName);
    return;
    }

  debug (LCD_COLOR_WHITE, "%s %d bytes", fileName.c_str(), (int)(filInfo.fsize));
  debug (LCD_COLOR_WHITE, "- %u/%02u/%02u %02u:%02u %c%c%c%c%c",
    (filInfo.fdate >> 9) + 1980, (filInfo.fdate >> 5) & 15,
    (filInfo.fdate) & 31, (filInfo.ftime >> 11), (filInfo.ftime >> 5) & 63,
    (filInfo.fattrib & AM_DIR) ? 'D' : '-', (filInfo.fattrib & AM_RDO) ? 'R' : '-',
    (filInfo.fattrib & AM_HID) ? 'H' : '-', (filInfo.fattrib & AM_SYS) ? 'S' : '-',
    (filInfo.fattrib & AM_ARC) ? 'A' : '-');
  return;

  if (f_open (&gFile, fileName.c_str(), FA_READ)) {
    debug (LCD_COLOR_RED, "%s not read" + fileName);
    return;
    }

  UINT bytesRead;
  f_read (&gFile, (void*)buf, (UINT)filInfo.fsize, &bytesRead);
  debug (LCD_COLOR_WHITE, "- read  %d bytes", bytesRead);
  f_close (&gFile);

  if (bytesRead > 0) {
    jpeg_mem_src (&mCinfo, buf, bytesRead);
    jpeg_read_header (&mCinfo, TRUE);
    debug (LCD_COLOR_WHITE, "- image %dx%d", mCinfo.image_width, mCinfo.image_height);

    mCinfo.dct_method = JDCT_FLOAT;
    mCinfo.out_color_space = JCS_RGB;
    mCinfo.scale_num = 1;
    mCinfo.scale_denom = 4;
    uint8_t* bufArray = (uint8_t*)malloc (mCinfo.output_width * 3);
    jpeg_start_decompress (&mCinfo);
    while (mCinfo.output_scanline < mCinfo.output_height) {
      jpeg_read_scanlines (&mCinfo, &bufArray, 1);
      rgb888to565 (bufArray, rgb565Buf + (mCinfo.output_scanline * mCinfo.output_width), mCinfo.output_width,1);
      }
    free (bufArray);
    jpeg_finish_decompress (&mCinfo);

    debug (LCD_COLOR_WHITE, "- load  %dx%d scale %d", mCinfo.output_width, mCinfo.output_height, 4);
    }
  }
//}}}
//{{{
void cApp::saveNumFile (const std::string& fileName,  uint8_t* buf, int bufLen) {

  if (f_open (&gFile, fileName.c_str(), FA_WRITE | FA_CREATE_ALWAYS))
    debug (LCD_COLOR_RED, "save " + fileName);

  else {
    UINT bytesWritten;
    f_write (&gFile, buf, (bufLen + 3) & 0xFFFFFFFC, &bytesWritten);
    f_close (&gFile);
    debug (LCD_COLOR_YELLOW, "save " + fileName + dec(bufLen,0));
    }
   }
//}}}
//{{{
void cApp::saveNumFile (const std::string& fileName, uint8_t* header, int headerLen, uint8_t* frame, int frameLen) {

  if (f_open (&gFile, fileName.c_str(), FA_WRITE | FA_CREATE_ALWAYS))
    debug (LCD_COLOR_RED, "saveNumFile " + fileName);

  else {
    if (headerLen & 0x03)
      debug (LCD_COLOR_RED, "save align " + fileName + " " + dec(headerLen));

    UINT bytesWritten;
    f_write (&gFile, header, headerLen, &bytesWritten);
    f_write (&gFile, frame, (frameLen + 3) & 0xFFFFFFFC, &bytesWritten);
    f_close (&gFile);

    debug (LCD_COLOR_YELLOW, "save " + fileName + " " + dec(headerLen) + ":" + dec(frameLen,0));
    }
  }
//}}}
//{{{
void cApp::createNumFile (const std::string& fileName, uint8_t* header, int headerLen, uint8_t* frame, int frameLen) {

  if (f_open (&gFile, fileName.c_str(), FA_WRITE | FA_CREATE_ALWAYS))
    debug (LCD_COLOR_RED, "createFile %s fail", fileName.c_str());

  else {
    if (headerLen & 0x03)
      debug (LCD_COLOR_RED, "createFile align %s %d", fileName, headerLen);

    UINT bytesWritten;
    f_write (&gFile, header, headerLen, &bytesWritten);
    f_write (&gFile, frame, (frameLen + 3) & 0xFFFFFFFC, &bytesWritten);

    debug (LCD_COLOR_YELLOW, "%s %d:%d:%d ok", fileName.c_str(), headerLen,frameLen, bytesWritten);
    }
   }
//}}}
//{{{
void cApp::appendFile (int num, uint8_t* header, int headerLen, uint8_t* frame, int frameLen) {

  UINT bytesWritten;
  f_write (&gFile, header, headerLen, &bytesWritten);
  f_write (&gFile, frame, (frameLen + 3) & 0xFFFFFFFC, &bytesWritten);

  debug (LCD_COLOR_YELLOW, "append %d %d:%d:%d ok", num, headerLen,frameLen, bytesWritten);
  }
//}}}
//{{{
void cApp::closeFile() {

  f_close (&gFile);
  }
//}}}

//{{{
void touchThread (void* arg) {

  gApp->touchThread();
  }
//}}}
//{{{
void saveThread (void* arg) {

  gApp->saveThread();
  }
//}}}
//{{{
void appThread (void* arg) {

  gApp->run();
  }
//}}}
//{{{
void serverThread (void* arg) {
  gApp->serverThread (arg);
  }
//}}}
//{{{
void dhcpThread (void* arg) {
  gApp->dhcpThread (arg);
  }
//}}}
//{{{
void netThread (void* arg) {

  //{{{  Static IP ADDRESS
  #define IP_ADDR0   192
  #define IP_ADDR1   168
  #define IP_ADDR2   1
  #define IP_ADDR3   100
  //}}}
  //{{{  NETMASK
  #define NETMASK_ADDR0   255
  #define NETMASK_ADDR1   255
  #define NETMASK_ADDR2   255
  #define NETMASK_ADDR3   0
  //}}}
  //{{{  Gateway Address
  #define GW_ADDR0   192
  #define GW_ADDR1   168
  #define GW_ADDR2   0
  #define GW_ADDR3   1
  //}}}

  struct netif netIf;
  ip_addr_t ipaddr;
  ip_addr_t netmask;
  ip_addr_t gw;

  tcpip_init (NULL, NULL);

  //IP_ADDR4(&ipaddr, IP_ADDR0 ,IP_ADDR1 , IP_ADDR2 , IP_ADDR3 );
  //IP_ADDR4(&netmask, NETMASK_ADDR0, NETMASK_ADDR1, NETMASK_ADDR2, NETMASK_ADDR3);
  //IP_ADDR4(&gw, GW_ADDR0, GW_ADDR1, GW_ADDR2, GW_ADDR3);
  ip_addr_set_zero_ip4 (&ipaddr);
  ip_addr_set_zero_ip4 (&netmask);
  ip_addr_set_zero_ip4 (&gw);
  netif_add (&netIf, &ipaddr, &netmask, &gw, NULL, &ethernetIfInit, &tcpip_input);

  netif_set_default (&netIf);
  if (netif_is_link_up (&netIf)) {
    netif_set_up (&netIf);
    cLcd::mLcd->debug (LCD_COLOR_GREEN, "ethernet ok");
    sys_thread_new ("dhcp", dhcpThread, &netIf, 2048, osPriorityBelowNormal);
    sys_thread_new ("server", serverThread, NULL, 2048, osPriorityAboveNormal);
    }
  else {
    cLcd::mLcd->debug (LCD_COLOR_MAGENTA, "ethernet not connected");
    netif_set_down (&netIf);
    }

  osThreadTerminate (NULL);
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
  BSP_SDRAM_Init();
  memset ((void*)SDRAM_DEVICE_ADDR, 0, SDRAM_DEVICE_SIZE);

  BSP_PB_Init (BUTTON_KEY, BUTTON_MODE_GPIO);
  //BSP_LED_Init (LED1);

  gApp = new cApp (cLcd::getWidth(), cLcd::getHeight());

  sys_thread_new ("app", appThread, NULL, 2048, osPriorityNormal);
  sys_thread_new ("net", netThread, NULL, 1024, osPriorityNormal);
  sys_thread_new ("touch", touchThread, NULL, 2048, osPriorityNormal);
  sys_thread_new ("save", saveThread, NULL, 10000, osPriorityAboveNormal);

  osKernelStart();
  }
//}}}
