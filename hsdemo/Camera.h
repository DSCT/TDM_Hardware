/****************************************************************************
 *
 * ACTIVE SILICON LIMITED
 *
 * Authors     : Jean-Philippe Arnaud
 *
 * Copyright (c) 2000-2013 Active Silicon Ltd.
 ****************************************************************************
 * Comments:
 * --------
 * A simple class to handle a camera connected to a FireBird frame grabber.
 * Handles the following tasks:
 * - open connection to hardware and check discovery status
 * - communicate with hardware
 * - acquire images from camera
 * - display images on screen
 * - record a sequence of images to disk
 ****************************************************************************
 */

#pragma once

#include <tmg2_api.h>
#include <phx_api.h>
#include "..\common\Display.h"

#include <memory>
#include <string>
#include <vector>

#include "..\common\stopwatch.h"
#include "..\common\aslexception.h"



DECLARE_AND_DEFINE_EXCEPTION(PHXException); // error from PHX library
DECLARE_AND_DEFINE_EXCEPTION(TMGException); // error from TMG library
DECLARE_AND_DEFINE_EXCEPTION(HWException); // hardware failure

//#define FANOPERATIONREG                      0x10140004

// PHXException means failure calling into PHX API.
// Could be a logical error, for example if a wrong parameter is passed-in as argument.
// Could be a hardware error, if the hardware failed to do what we asked.
#define THROW_PHX_EXCEPTION(x) throw PHXException((x), __FUNCTION__, __FILE__, __LINE__)
// TMGException means failure calling into TMG API.
#define THROW_TMG_EXCEPTION(x) throw TMGException((x), __FUNCTION__, __FILE__, __LINE__)
// HWException means the hardware somehow failed. For example could be a loss of Link.
#define THROW_HW_EXCEPTION(x) throw HWException((x), __FUNCTION__, __FILE__, __LINE__)

#define _PHX_THROW_MSG_ON_ERROR(Function, Msg) \
{\
   etStat _dwStatus;\
   _dwStatus = Function;\
   if (_dwStatus != PHX_OK) {\
      char pString[4096];\
      memset(pString, 0, sizeof(pString)); \
      PHX_ErrCodeDecode(pString, _dwStatus);\
      THROW_PHX_EXCEPTION(ActiveSilicon::CASLException::msgComposer("%s\nLow level error: %s", Msg, pString).c_str());\
   }\
}

#define _PHX_THROW_ON_ERROR(Function) _PHX_THROW_MSG_ON_ERROR(Function, "")

#define _TMG_THROW_ON_ERROR(Function) \
{\
   ui32 _dwStatus = Function;\
   if (_dwStatus) THROW_TMG_EXCEPTION(ActiveSilicon::CASLException::msgComposer("TMG error calling %s", #Function).c_str());\
}

#define _TMG_MESSAGE_BOX_ON_ERROR(Function) \
{\
   ui32 _dwStatus = Function;\
   if (_dwStatus) MessageBox(0, "TMG error", "Error", MB_OK);\
}
#define THROW_UNRECOVERABLE(msg) \
{\
   std::stringstream ss;\
   ss << "Error: " << msg << "\nfile: "__FILE__ "\nfunction: "__FUNCTION__ "\nline: " << std::dec << __LINE__;\
   throw std::runtime_error(ss.str());\
}
 
#define HTONL(x) ( (((x) << 24) & 0xff000000) | (((x) << 8) & 0x00ff0000) | (((x) >> 8) & 0x0000ff00) | (((x) >> 24) & 0x000000ff) )
#define HTONS(x) ( (((x) << 8 ) & 0xff00)     | (((x) >> 8) & 0x00ff) )

class CDisplay;
struct IADL_Image;
struct IADL_Font;
struct IADL_Target;

class CCamera
{
public:

   enum CXP_PIXEL_FORMAT {
      CXP_PIXEL_FORMAT_UNKNOWN      = 0,
      CXP_PIXEL_FORMAT_MONO8        = 0x101,
      CXP_PIXEL_FORMAT_MONO10       = 0x102,
      CXP_PIXEL_FORMAT_MONO12       = 0x103,
      CXP_PIXEL_FORMAT_BAYER_GR8    = 0x311,
      CXP_PIXEL_FORMAT_BAYER_RG8    = 0x321,
      CXP_PIXEL_FORMAT_BAYER_GB8    = 0x331,
      CXP_PIXEL_FORMAT_BAYER_BG8    = 0x341,
      CXP_PIXEL_FORMAT_BAYER_GR10   = 0x312,
      CXP_PIXEL_FORMAT_BAYER_RG10   = 0x322,
      CXP_PIXEL_FORMAT_BAYER_GB10   = 0x332,
      CXP_PIXEL_FORMAT_BAYER_BG10   = 0x342,
      CXP_PIXEL_FORMAT_BAYER_GR12   = 0x313,
      CXP_PIXEL_FORMAT_BAYER_RG12   = 0x323,
      CXP_PIXEL_FORMAT_BAYER_GB12   = 0x333,
      CXP_PIXEL_FORMAT_BAYER_BG12   = 0x343,
      CXP_PIXEL_FORMAT_YUV422       = 0x353,
      CXP_PIXEL_FORMAT_RGB24        = 0x401,
   };

   struct BufferInfo {
      size_t   bytes;
      uint32_t width;
      uint32_t height;
      enum CXP_PIXEL_FORMAT cxpPixelFormat;
      std::tr1::shared_ptr<void> data;
	  BufferInfo() : bytes(0), width(0), height(0), cxpPixelFormat(CXP_PIXEL_FORMAT_UNKNOWN) {}
   };

   struct Options_t {
      uint8_t     boardnumber;
      uint8_t     channelnumber;
      std::string boardvariant;
      std::string pcf;
      bool        defaultPcf;
      uint8_t     lanes;
      uint8_t     gbps;
      uint32_t    recordBufferCount;
      uint32_t    watchdogTimeMs;
      uint32_t    bufferBeforeTriggerCount;
      std::string outputDirectory;
      std::string prefix;
      uint8_t     noPrefix;
      bool        bayerColorCorrection;
      std::string companyLogo;
      std::string cameraName;
      uint32_t    recordWidth;
      uint32_t    recordHeight;
      bool        applyROI;
      uint32_t    dvrFrameRate;
      uint32_t    jpegQFactor;
      bool        multipleROI;
      bool        internalTrigger;
   };

   CCamera(
             struct Options_t &
           , HWND hwnd
      );

   virtual ~CCamera(void);

   HWND getWindowHandle                () {return m_hwnd;}
   void open                           ();
   void close                          ();

   bool isCameraLink                   () {return m_isCameraLink;}

   uint8_t toggleCameraAcquisitionOnOff();
   void toggleFrameStartEvent          ();

   void restartAcquisition             ();

   void startRecord                    ();
   void nextRecordFormat               ();

   void paint                          ();
   uint8_t toggleDisplayOnOff          ();
   uint8_t toggleDisplayMenuSimpleAdvanced();
   void toggleFitToDisplayOnOff        ();

   void toggleTriggerModeOnOff         (uint8_t selectedMode);

   void saveImageToFile                (std::string filename);

   void hostWriteWord                  (uint32_t dwAddress, uint32_t dwData);
   void deviceWrite                    (uint32_t dwAddress, int32_t dwData);
   void hostReadWord                   (uint32_t dwAddress, int32_t &dwData);
   void deviceRead                     (uint32_t dwAddress, int32_t &dwData);
   float deviceReadfloat			   (uint32_t dwAddress);
   void FanOpertaor					   (uint32_t dwData);
   float MonitorTemp				   ();
   void PrintExposureData			   ();
   void TestPattern					   (int);

private:
   void              startCamera          ();
   void              stopCamera           ();
   void              startFrameGrabber    ();
   void              stopFrameGrabber     ();

   virtual void      setAcquisitionStart  ()                      = 0;
   virtual void      setAcquisitionStop   ()                      = 0;
   virtual void      setPixelFormat       (enum CXP_PIXEL_FORMAT) = 0;
   virtual void      setMiscellaneous     ()                      = 0;  // configure other settings in the camera, called before starting acquisition
   virtual void      forceCameraOn        ()                      {}    // work-around cameras that need power over CXP but don't support auto PoCXP
   virtual void      forceCameraOff       ()                      {}    // work-around cameras that need power over CXP but don't support auto PoCXP
   virtual uint32_t  getPowerUpTimeMs     ()                      {return 5000;}

   virtual void      bufferCallback       (bool)                  {}    // member function called for each buffer acquired which can be specialised for each camera

   virtual void      frameStartCallback   ()                      {}    // member function called for each frame start event which can be specialised for each camera

   // trigger modes
   virtual void      initTriggerMode1     ()                      {}
   virtual void      initTriggerMode2     ()                      {}
   virtual void      initTriggerMode3     ()                      {}

   virtual void      dumpCameraInfo       ()                      {}

   void              deviceRead           (uint32_t dwAddress, size_t size, std::string &strData);

   void openCamera2                       (char * configFileName, etParamValue boardVariant, etParamValue boardNumber, etParamValue configMode, uint32_t bufferCount);
   void synchronizedAcquisitionSetup      (uint32_t bufferCount);

protected:
   virtual void initFreeRunMode     ();

   void hostSetParameter            (etParam parameter, uint32_t value);
   void hostGetParameter            (etParam parameter, uint32_t & value);

   void deviceWriteAndReadBack      (uint32_t dwAddress, int32_t dwData);

   void calculateCameraROI          (int32_t x_min, int32_t y_min, int32_t x_max, int32_t y_max, int32_t x_inc, int32_t y_inc, int32_t &x, int32_t &y);
   void calculateCameraOffset       (int32_t x, int32_t y, int32_t x_max, int32_t y_max, int32_t x_offset_inc, int32_t y_offset_inc, int32_t &x_offset, int32_t &y_offset);

   tHandle  handle() {return m_hCamera;}

   void printCameraIntInfo(uint32_t address);
   void printCameraStringInfo(uint32_t address, size_t size);

private:
   tHandle     m_hCamera;
   tHandle     m_hCamera2;
   uint32_t    m_lanes;
   uint32_t    m_gbps;
   std::string m_pcf;
   uint8_t     m_boardNumber;
   uint8_t     m_channelNumber;
   std::string m_boardVariant;
   enum CXP_PIXEL_FORMAT m_cameraPixelformat;
   enum CXP_PIXEL_FORMAT m_grabberPixelformat;
   uint32_t    m_imageWidth;
   uint32_t    m_imageHeight;
   uint32_t    m_widthBytes;
   uint32_t    m_heightBytes;
   uint32_t    m_packedWidth;
   uint32_t    m_packedDepth;
   uint32_t    m_packedExtraBits;
   uint32_t    m_blocking;
   uint32_t    m_recordWidth;
   uint32_t    m_recordHeight;
   uint32_t    m_dvrFrameRate;
   uint32_t    m_jpegQFactor;

   volatile uint8_t m_paintDoneAtomic; //atomic

   volatile uint32_t m_callbackCount;
   uint32_t m_cameraCallbackCount;
   uint32_t m_camera2CallbackCount;
   uint32_t m_buffersBeforeTrigger;
   uint32_t m_initialBuffersBeforeTrigger;
   uint32_t m_buffersAfterTrigger;
   uint32_t m_recordingBuffersSoFar;
   uint32_t m_ImgFrames;
   uint32_t m_ExposureTime;

   bool     m_datastream;

   HANDLE   m_bufferReadySemaphore;

   DWORD    m_bufferProcessingThreadID;
   HANDLE   m_bufferProcessingThread;
   bool     m_bufferProcessingThreadStop;

   DWORD    m_ni_deviceThreadID;
   HANDLE   m_ni_deviceThread;
   bool     m_ni_deviceThreadStop;

   DWORD    m_watchdogThreadID;
   HANDLE   m_watchdogThread;
   bool     m_watchdogThreadStop;
   uint32_t m_watchdogTimer;

   DWORD    m_movestageThreadID;
   HANDLE   m_movestageThread;
   bool     m_movestageThreadStop;

   DWORD    m_acquisitionFramerateThreadID;
   HANDLE   m_acquisitionFramerateThread;
   bool     m_acquisitionFramerateThreadStop;

   volatile LONG     m_acquisitionStopwatchCount;
   volatile double   m_acquisitionFPS;

   volatile uint8_t  m_recordAtomic;
   volatile uint8_t  m_acquisitionBlockedAtomic;
   volatile uint8_t  m_recordingThreadSignaledAtomic;
   volatile uint8_t  m_isFrameGrabberRunningAtomic;
   volatile uint8_t  m_semaphoreFlagAtomic;
   volatile uint8_t  m_isRecoveringAtomic;

   bool              m_bayerColorCorrection;
   uint32_t          m_timeoutPHX;
   CRITICAL_SECTION  m_callbackCS;

   std::vector<std::tr1::shared_ptr<void>> m_virtualBuffers;

   bool isDatastream ();
   void unpackData   (uint8_t * imageIn, uint16_t * imageOut);

   void saveToTIFF   (std::string filename);
   void saveToRAW    (std::string filename);
   void saveToBMP    (std::string filename);

   void RecoverAcquisition();
   void InitialiseCamera();
   void DumpStatusInfo();
   virtual void SetBitRateAndDiscovery(int lanes, int gbps);
   void PowerCycleCamera();
   void AcquisitionStart();
   bool BayerFilter(int TMGImageIn, int TMGImageOut, bool colorCorrection);
   DWORD semaphoreWait();

   void PhoenixWriteWord (uint32_t dwAddress, uint32_t dwData, etControlPort port);
   void PhoenixReadWord  (uint32_t dwAddress, int32_t &dwData, etControlPort port);
   
   static void  __cdecl PHX_CallbackEvent(tHandle hCamera, uint32_t dwInterruptMask, void *pvParams);
   static DWORD WINAPI WatchdogThread (void *x);
   static DWORD WINAPI BufferProcessingThread (void *x);
   static DWORD WINAPI NIdeviceThread (void *x);
   static DWORD WINAPI MoveStageThread(void *x);
   static DWORD WINAPI AcquisitionFramerateThread (void *x);

   // record stuff
   static DWORD WINAPI RecordThread(void * x);

   void DoRecord();

   volatile uint8_t  m_recordNoPrefixAtomic;
   std::string       m_strFilePrefix;
   std::string       m_strOutputDir;
   volatile uint8_t  m_recordSelectedFormatAtomic;
   volatile uint32_t m_recordCurrentFrameIndex;

   DWORD    m_recordThreadID;
   HANDLE   m_recordThread;
   bool     m_recordThreadStop;
   HANDLE   m_eventFromRecording;

   bool recordAll ();
   bool recordAvi ();
   bool recordTiff();
   bool recordBmp ();
   bool recordRaw ();

   void DoSaveToTiff(int hTMGImage, std::string &filename, bool bSaveCompatible);
   void DoSaveToBmp (int hTMGImage, std::string &filename);
   void DoSaveToRaw (int hTMGImage, std::string &filename);

   // display stuff
   HWND                    m_hwnd;
   std::auto_ptr<CDisplay> m_display;
   int                     m_displayTMGImageFromCamera;
   int                     m_displayTMGImageProcessingCurrent;
   int                     m_displayTMGImageProcessingNext;
   int                     m_displayCopyOfTMGImage; // copy of a TMG handle. Do not attempt to destroy!!
   volatile uint8_t        m_displayPausedAtomic;
   IADL_Image *            m_displayImageADL;
   bool                    m_fitToDisplay;

   IADL_Font *             m_displayFontMainMenu;
   IADL_Font *             m_displayFontConfiguration;
   uint8_t                 m_displayMenuAdvanced;
   uint32_t                m_displayImageWidth;
   uint32_t                m_displayImageHeight;

   std::string             m_companyLogoFile;
   IADL_Target *           m_adlLogoTarget;
   int                     m_tmgLogoImage;
   IADL_Image *            m_adlLogoImage;

   std::string             m_displayCameraName;
   uint8_t                 m_saveImageToFileFlagAtomic;
   std::string             m_saveImageToFileName;

   volatile uint8_t        m_bufferForDisplayAtomic;
   BufferInfo              m_currentDisplayBuffer;
   BufferInfo              m_nextDisplayBuffer;

   void errorDetect(uint8_t* buffer, int32_t w, int32_t h);
   void text();

void addImage (
                 IADL_Target *& pT
               , int          & TMGImage
               , IADL_Image  *& pADL_Image
               , const char   * szFilename
               , int x, int y, bool bColorKey, int dwColorKey, int dwWidth, int dwHeight);

static void stoupper(std::string& s);

protected:
   float             m_fColorMatrix[9];
   bool              m_applyROI;
   bool              m_multipleROI;
   bool              m_internalTrigger;
   uint8_t           m_isCameraRunningAtomic;
   uint32_t          m_BufferReceivedCount;
   volatile uint8_t  m_acquisitionModeSelectedAtomic;
   uint32_t          m_recordBufferCount;
   bool              m_isCameraLink;
   bool              m_useSynchronizedAcquisition;

   unsigned char     m_FrameStartEventEnabledAtomic;
   uint32_t          m_FrameStartEventCount;

   std::vector<std::string> m_acquisitionModes;
};

#define COAXPRESS_V1_0_BOOTSTRAP_REGS \
CXP_REG_STANDARD                  = 0x0000,\
CXP_REG_REVISION                  = 0x0004,\
CXP_REG_XMLMANIFESTSIZE           = 0x0008,\
CXP_REG_XMLMANIFESTSELECTOR       = 0x000C,\
CXP_REG_XMLVERSION                = 0x0010,\
CXP_REG_XMLSCHEMAVERSION          = 0x0014,\
CXP_REG_XMLURLADDRESS             = 0x0018,\
CXP_REG_LIDCPOINTER               = 0x001C,\
CXP_REG_DEVICEVENDORNAME          = 0x2000,\
CXP_REG_DEVICEMODELNAME           = 0x2020,\
CXP_REG_DEVICEMANUFACTURERINFO    = 0x2040,\
CXP_REG_DEVICEVERSION             = 0x2070,\
CXP_REG_DEVICEFIRMWAREVERSION     = 0x2090,\
CXP_REG_DEVICEID                  = 0x20B0,\
CXP_REG_DEVICEUSERID              = 0x20C0,\
CXP_REG_LINKRESET                 = 0x4000,\
CXP_REG_LINKID                    = 0x4004,\
CXP_REG_MASTERHOSTLINKID          = 0x4008,\
CXP_REG_CONTROLPACKETDATASIZE     = 0x400C,\
CXP_REG_STREAMPACKETDATASIZE      = 0x4010,\
CXP_REG_LINKCONFIG                = 0x4014,\
CXP_REG_LINKCONFIGDEFAULT         = 0x4018,\
CXP_REG_TESTMODE                  = 0x401C,\
CXP_REG_TESTERRORCOUNTSELECTOR    = 0x4020,\
CXP_REG_TESTERRORCOUNT            = 0x4024,

#define COAXPRESS_V1_1_BOOTSTRAP_REGS \
CXP_REG_WIDTH_ADDRESS             = 0x3000,\
CXP_REG_HEIGHT_ADDRESS            = 0x3004,\
CXP_REG_ACQ_MODE_ADDRESS          = 0x3008,\
CXP_REG_ACQ_START_ADDRESS         = 0x300c,\
CXP_REG_ACQ_STOP_ADDRESS          = 0x3010,\
CXP_REG_PIXELFORMAT_ADDRESS       = 0x3014,\
CXP_REG_TAP_GEOMETRY_ADDRESS      = 0x3018,\
CXP_REG_IMAGE1_STREAMID_ADDRESS   = 0x301C,\
CXP_REG_IMAGE2_STREAMID_ADDRESS   = 0x3020,\
CXP_REG_IMAGE3_STREAMID_ADDRESS   = 0x3024,\
CXP_REG_IMAGE4_STREAMID_ADDRESS   = 0x3028,

enum CoaXPressBootstrapRegisters {
  COAXPRESS_V1_0_BOOTSTRAP_REGS
  COAXPRESS_V1_1_BOOTSTRAP_REGS
};
