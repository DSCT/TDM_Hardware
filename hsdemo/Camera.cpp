/****************************************************************************
 *
 * ACTIVE SILICON LIMITED
 *
 * Authors     : Jean-Philippe Arnaud
 *
 * Copyright (c) 2000-2013 Active Silicon Ltd.
 ****************************************************************************
 */

// Important Note: Acquisition Engine Extra Buffer
// read comment "Acquisition Engine Extra Buffer" for
// explanation about the extra acquisition buffer used by the acquisition
// engine.

#include "Camera.h"
#include "..\common\Display.h"
#include "..\common\adl_helper.h"
#include <sstream>
#include "PhaseExtraction.cuh"
#include "NI_device.h"
#include "rs232.h"
#include "Stage.h"
#include "HostParameter.h"

TaskHandle NI_Task;
LONG recordingBuffersSoFar;
//std::string FolderPath;


class CLock
{
public:
   explicit CLock (CRITICAL_SECTION &cs)  : m_cs(cs)  { EnterCriticalSection(&m_cs); }
   ~CLock         (void)                              { LeaveCriticalSection(&m_cs); }
private:
   CRITICAL_SECTION &m_cs;
};

#define htonl(A) ((((ui32)(A) & 0xff000000) >> 24) | \
                  (((ui32)(A) & 0x00ff0000) >>  8) | \
                  (((ui32)(A) & 0x0000ff00) <<  8) | \
                  (((ui32)(A) & 0x000000ff) << 24))

static const char * g_strRecordFormats[]     = {".avi", ".tiff", ".bmp", ".dat", ".avi+.tiff+.bmp+.dat"};

void CCamera::stoupper(std::string& s)
{
   std::string::iterator i = s.begin();
   std::string::iterator end = s.end();
   while (i != end) {
      *i = toupper((unsigned char)*i);
      ++i;
   }
}

CCamera::CCamera(
                   struct Options_t & options
                 , HWND hwnd
                 ) :
  m_hCamera(0)
, m_hCamera2(0)
, m_BufferReceivedCount(0)
, m_hwnd(hwnd)
, m_cameraPixelformat(CXP_PIXEL_FORMAT_UNKNOWN)
, m_grabberPixelformat(CXP_PIXEL_FORMAT_UNKNOWN)
, m_imageWidth(0)
, m_imageHeight(0)
, m_widthBytes(0)
, m_heightBytes(0)
, m_packedWidth(0)
, m_packedDepth(0)
, m_packedExtraBits(0)
, m_boardNumber(options.boardnumber)
, m_channelNumber(options.channelnumber)
, m_boardVariant(options.boardvariant)
, m_lanes(options.lanes)
, m_gbps(options.gbps)
, m_recordWidth(options.recordWidth)
, m_recordHeight(options.recordHeight)
, m_dvrFrameRate(options.dvrFrameRate)
, m_jpegQFactor(options.jpegQFactor)
, m_timeoutPHX(5000)
, m_recordBufferCount(options.recordBufferCount)
, m_saveImageToFileFlagAtomic(0)
, m_saveImageToFileName()
, m_buffersBeforeTrigger(options.bufferBeforeTriggerCount)
, m_initialBuffersBeforeTrigger(0)
, m_bufferReadySemaphore(0)
, m_bufferProcessingThreadID(0)
, m_ni_deviceThreadID(0)
, m_watchdogThreadID(0)
, m_movestageThreadID(0)
, m_acquisitionFramerateThreadID(0)
, m_bufferProcessingThread(0)
, m_bufferProcessingThreadStop(false)
, m_ni_deviceThread(0)
, m_ni_deviceThreadStop(false)
, m_watchdogThread(0)
, m_watchdogThreadStop(false)
, m_watchdogTimer(options.watchdogTimeMs)
, m_movestageThread(0)
, m_movestageThreadStop(false)
, m_acquisitionFramerateThread(0)
, m_acquisitionFramerateThreadStop(false)
, m_acquisitionStopwatchCount(0)
, m_acquisitionFPS(0.)
, m_acquisitionBlockedAtomic(0)
, m_acquisitionModeSelectedAtomic(0)
, m_recordAtomic(0)
, m_isCameraRunningAtomic(0)
, m_bayerColorCorrection(options.bayerColorCorrection)
, m_paintDoneAtomic(0)
, m_blocking(0)
, m_recordingBuffersSoFar(0)
, m_ImgFrames(500)
, m_ExposureTime(200)
, m_recordingThreadSignaledAtomic(0)
, m_applyROI(options.applyROI)
, m_multipleROI(options.multipleROI)
, m_internalTrigger(options.internalTrigger)
, m_callbackCount(0)
, m_cameraCallbackCount(0)
, m_camera2CallbackCount(0)
, m_bufferForDisplayAtomic(1)
, m_isFrameGrabberRunningAtomic(0)
, m_semaphoreFlagAtomic(0)
, m_isRecoveringAtomic(0)
, m_datastream(false)
, m_FrameStartEventEnabledAtomic(false)
, m_FrameStartEventCount(0)

, m_recordNoPrefixAtomic(options.noPrefix)
, m_recordSelectedFormatAtomic(2)
, m_recordThreadID(0)
, m_recordThread(0)
, m_recordThreadStop(false)
, m_eventFromRecording(0)
, m_strOutputDir(options.outputDirectory)
, m_recordCurrentFrameIndex(0)
, m_strFilePrefix(options.prefix)

, m_display(new CDisplay(hwnd))
, m_displayTMGImageFromCamera(0)
, m_displayPausedAtomic(0)
, m_displayImageADL(0)
, m_fitToDisplay(false)
, m_displayFontMainMenu(0)
, m_displayFontConfiguration(0)
, m_displayMenuAdvanced(0)
, m_displayImageWidth(0)
, m_displayImageHeight(0)
, m_displayTMGImageProcessingCurrent(0)
, m_displayTMGImageProcessingNext(0)
, m_displayCopyOfTMGImage(0)
, m_companyLogoFile(options.companyLogo)
, m_adlLogoTarget(0)
, m_tmgLogoImage(0)
, m_adlLogoImage(0)
, m_displayCameraName(options.cameraName)
, m_isCameraLink(false)
, m_useSynchronizedAcquisition(false)
{
   // Initialize m_acquisitionModes with Continuous mode
   m_acquisitionModes.push_back("Continuous");

   m_pcf = options.pcf;
   stoupper(m_boardVariant);

   // Default initialisation to identity matrix - if color correction is needed, the matrix will be set in corresponding camera class
   m_fColorMatrix[0] = (float) 1; m_fColorMatrix[1] = (float) 0; m_fColorMatrix[2] = (float) 0;
   m_fColorMatrix[3] = (float) 0; m_fColorMatrix[4] = (float) 1; m_fColorMatrix[5] = (float) 0;
   m_fColorMatrix[6] = (float) 0; m_fColorMatrix[7] = (float) 0; m_fColorMatrix[8] = (float) 1;

   if (m_buffersBeforeTrigger > m_recordBufferCount) { // the number of buffers before trigger must not be higher than the number of record buffers
      char message[256];
      sprintf_s(message, 256, "Number of buffers before trigger (%d) is higher than number of record buffers (%d).\nNumber of buffers before trigger has now been set to %d."
                , m_buffersBeforeTrigger, m_recordBufferCount, m_recordBufferCount);
      MessageBox(0, message, "Error in"__FUNCTION__, MB_OK);
      m_buffersBeforeTrigger = m_recordBufferCount; // set number of buffers before trigger to number of record buffers and continue
   }

   m_initialBuffersBeforeTrigger = m_buffersBeforeTrigger;

   InitializeCriticalSection(&m_callbackCS);
}

CCamera::~CCamera(void)
{
}

void CCamera::close()
{
   // stop the threads
   m_watchdogThreadStop             = true;
   m_bufferProcessingThreadStop     = true;
   m_ni_deviceThreadStop            = true;
   m_recordThreadStop               = true;
   m_acquisitionFramerateThreadStop = true;
   m_movestageThreadStop			= true;

   WaitForSingleObject(m_bufferProcessingThread, 2500);
   WaitForSingleObject(m_ni_deviceThread, 2500);
   WaitForSingleObject(m_recordThread, 2500);
   WaitForSingleObject(m_acquisitionFramerateThread, 2500);
   WaitForSingleObject(m_movestageThread, 2500);

   while (m_isRecoveringAtomic) Sleep(1); // Waiting 2500 ms might not be enough if recovery is currently under way (camera powercycle would take at least 5000 ms)
                                          // Therefore we make sure the recovery attempt has ended before closing the thread.
   WaitForSingleObject(m_watchdogThread, 2500);

   CloseHandle(m_watchdogThread);
   CloseHandle(m_bufferProcessingThread);
   CloseHandle(m_ni_deviceThread);
   CloseHandle(m_recordThread);
   CloseHandle(m_acquisitionFramerateThread);
   CloseHandle(m_movestageThread);

   ALERT_AND_SWALLOW(toggleTriggerModeOnOff(0)); // set hardware back to freerun

   // stop acquisition
   ALERT_AND_SWALLOW(stopCamera());

   ALERT_AND_SWALLOW(stopFrameGrabber());

   forceCameraOff(); // power off specific cameras

   // close handles other than thread handles
   CloseHandle(m_bufferReadySemaphore);
   CloseHandle(m_eventFromRecording);

   ALERT_AND_SWALLOW(_PHX_THROW_ON_ERROR(PHX_Close(&m_hCamera)));
   ALERT_AND_SWALLOW(_PHX_THROW_ON_ERROR(PHX_Destroy(&m_hCamera)));
   if (m_hCamera2) {
      ALERT_AND_SWALLOW(_PHX_THROW_ON_ERROR(PHX_Close(&m_hCamera2)));
      ALERT_AND_SWALLOW(_PHX_THROW_ON_ERROR(PHX_Destroy(&m_hCamera2)));
   }

   // destroy TMG and ADL images and ADL target
   if (m_displayTMGImageFromCamera)          TMG_image_destroy(m_displayTMGImageFromCamera);
   if (m_displayTMGImageProcessingCurrent)   TMG_image_destroy(m_displayTMGImageProcessingCurrent);
   if (m_displayTMGImageProcessingNext)      TMG_image_destroy(m_displayTMGImageProcessingNext);
   if (m_tmgLogoImage)                       TMG_image_destroy(m_tmgLogoImage);

   if (m_displayImageADL)  ADL_ImageDestroy(&m_displayImageADL);
   if (m_adlLogoImage)     ADL_ImageDestroy(&m_adlLogoImage);

   if (m_adlLogoTarget) m_display->adlDisplay()->TargetDestroy(&m_adlLogoTarget);

   DeleteCriticalSection(&m_callbackCS);

   free(SP_float);
   cudaFree(tmp_uint8);
   cudaFree(cuSP2);
   cudaFree(SPWrapPhase2);
   cudaFree(UnWrapPhaseSP2);
   cudaFree(cuPhaseMap2);
   cudaFree(cuPhaseMap);

   (cudaFree(LaplaceArray));
   (cudaFree(inX));
   (cudaFree(inY));
   (cudaFree(outX));
   (cudaFree(outY));
   (cudaFree(dst_dct));
   cudaFree(dSrc_tmp);

}

void CCamera::open()
{
   // See comments of PHX_ParameterSet(m_hCamera, PHX_ACQ_NUM_IMAGES, &total_buffer_count) for the extra buffer count
   m_bufferReadySemaphore = CreateSemaphore(NULL, 0, m_recordBufferCount + 1, NULL);

   /*if (!m_companyLogoFile.empty()) { // add Active Silicon logo to display
      _TMG_THROW_ON_ERROR(TMG_Image_Create(&m_tmgLogoImage, 0, NULL));
      if (m_display->adlDisplay()) _ADL_THROW_ON_ERROR(m_display->adlDisplay()->TargetCreate(&m_adlLogoTarget));
      if (m_adlLogoTarget) _ADL_THROW_ON_ERROR(m_adlLogoTarget->Init());
      addImage(m_adlLogoTarget, m_tmgLogoImage, m_adlLogoImage, m_companyLogoFile.c_str(), 35, 50, false, ADL_ARGB(255, 255, 255, 255), 0, 0);
   }*/

   _TMG_THROW_ON_ERROR(TMG_Image_Create(&m_displayTMGImageFromCamera, 0, NULL));
   _TMG_THROW_ON_ERROR(TMG_Image_Create(&m_displayTMGImageProcessingCurrent, 0, NULL));
   _TMG_THROW_ON_ERROR(TMG_Image_Create(&m_displayTMGImageProcessingNext, 0, NULL));

   if (m_display->adlDisplay()) { // create fonts for text display
      _ADL_THROW_ON_ERROR(m_display->adlDisplay()->FontCreate(&m_displayFontMainMenu));
      _ADL_THROW_ON_ERROR(m_displayFontMainMenu->SetAttribute(ADL_FONT_ATTRIBUTE_FACENAME, reinterpret_cast<void *>("arial")));
      _ADL_THROW_ON_ERROR(m_displayFontMainMenu->Init());

      _ADL_THROW_ON_ERROR(m_display->adlDisplay()->FontCreate(&m_displayFontConfiguration));
	  // used for setting the size of title and FPS info.
      //_ADL_THROW_ON_ERROR(m_displayFontConfiguration->SetAttribute(ADL_FONT_ATTRIBUTE_HEIGHT,   reinterpret_cast<void *>(75)));
      //_ADL_THROW_ON_ERROR(m_displayFontConfiguration->SetAttribute(ADL_FONT_ATTRIBUTE_WIDTH,    reinterpret_cast<void *>(50)));
      _ADL_THROW_ON_ERROR(m_displayFontConfiguration->SetAttribute(ADL_FONT_ATTRIBUTE_FACENAME, reinterpret_cast<void *>("arial")));
      _ADL_THROW_ON_ERROR(m_displayFontConfiguration->Init());
   }

   // set up the board
   etParamValue eBoardNumber   = PHX_BOARD_NUMBER_1;
   etParamValue eChannelNumber = PHX_CHANNEL_NUMBER_1;
   etParamValue eConfigMode    = PHX_CONFIG_ACQ_ONLY;

   switch (m_boardNumber) {
      case 1: eBoardNumber = PHX_BOARD_NUMBER_1; break;
      case 2: eBoardNumber = PHX_BOARD_NUMBER_2; break;
      case 3: eBoardNumber = PHX_BOARD_NUMBER_3; break;
      case 4: eBoardNumber = PHX_BOARD_NUMBER_4; break;
      case 6: eBoardNumber = PHX_BOARD_NUMBER_6; break;
      case 7: eBoardNumber = PHX_BOARD_NUMBER_7; break;
      default: THROW_UNRECOVERABLE(ActiveSilicon::CASLException::msgComposer("Unknown board number %d", m_boardNumber));
   }

   switch (m_channelNumber) {
      case 1: eChannelNumber = PHX_CHANNEL_NUMBER_1; break;
      case 2: eChannelNumber = PHX_CHANNEL_NUMBER_2; break;
      case 3: eChannelNumber = PHX_CHANNEL_NUMBER_3; break;
      case 4: eChannelNumber = PHX_CHANNEL_NUMBER_4; break;
      case 5: eChannelNumber = PHX_CHANNEL_NUMBER_5; break;
      case 6: eChannelNumber = PHX_CHANNEL_NUMBER_6; break;
      case 7: eChannelNumber = PHX_CHANNEL_NUMBER_7; break;
      case 8: eChannelNumber = PHX_CHANNEL_NUMBER_8; break;
      default: THROW_UNRECOVERABLE(ActiveSilicon::CASLException::msgComposer("Unknown channel number %d", eChannelNumber));
   }

   etParamValue eBoardVariant;
        if (!m_boardVariant.compare("PHX_BOARD_DIGITAL"))         eBoardVariant = PHX_BOARD_DIGITAL;
   else if (!m_boardVariant.compare("PHX_BOARD_FBD_1XCXP6_2PE8")) eBoardVariant = PHX_BOARD_FBD_1XCXP6_2PE8;
   else if (!m_boardVariant.compare("PHX_BOARD_FBD_2XCXP6_2PE8")) eBoardVariant = PHX_BOARD_FBD_2XCXP6_2PE8;
   else if (!m_boardVariant.compare("PHX_BOARD_FBD_4XCXP6_2PE8")) eBoardVariant = PHX_BOARD_FBD_4XCXP6_2PE8;
   else if (!m_boardVariant.compare("PHX_BOARD_FBD_1XCLD_2PE8"))  eBoardVariant = PHX_BOARD_FBD_1XCLD_2PE8;
   else if (!m_boardVariant.compare("PHX_BOARD_FBD_2XCLD_2PE8"))  eBoardVariant = PHX_BOARD_FBD_2XCLD_2PE8;
   else if (!m_boardVariant.compare("PHX_BOARD_FBD_1XCLM_2PE8"))  eBoardVariant = PHX_BOARD_FBD_1XCLM_2PE8;
   else if (!m_boardVariant.compare("PHX_BOARD_FBD_2XCLM_2PE8"))  eBoardVariant = PHX_BOARD_FBD_2XCLM_2PE8;
   else THROW_UNRECOVERABLE(ActiveSilicon::CASLException::msgComposer("Unknown board type %s", m_boardVariant));

   _PHX_THROW_ON_ERROR(PHX_Create(&m_hCamera, PHX_ErrHandlerDefault));

   // set the configuration file
   char* pszConfigFileName = NULL;
   char szConfigFileName[256];
   pszConfigFileName = &szConfigFileName[0];
   sprintf_s(szConfigFileName, m_pcf.c_str());

   FILE *pf = fopen(szConfigFileName, "rb");
   if (pf) {
      fclose(pf);
      _PHX_THROW_ON_ERROR(PHX_ParameterSet(m_hCamera, PHX_CONFIG_FILE, &pszConfigFileName));
   } else THROW_UNRECOVERABLE("Failed to read configuration file");

   _PHX_THROW_ON_ERROR(PHX_ParameterSet(m_hCamera, PHX_BOARD_VARIANT,   &eBoardVariant));
   _PHX_THROW_ON_ERROR(PHX_ParameterSet(m_hCamera, PHX_BOARD_NUMBER,    &eBoardNumber));
   _PHX_THROW_ON_ERROR(PHX_ParameterSet(m_hCamera, PHX_CHANNEL_NUMBER,  &eChannelNumber));
   _PHX_THROW_ON_ERROR(PHX_ParameterSet(m_hCamera, PHX_CONFIG_MODE,     &eConfigMode));

   _PHX_THROW_ON_ERROR(PHX_Open(m_hCamera));
   forceCameraOn(); // Some cameras need to be powered on via the PHX_CXP_POCXP_MODE_FORCEON frame grabber setting

   //***************************************************************
   // Acquisition Engine Extra Buffer
   //***************************************************************
   // This application records acquired frames to disk.
   // Once enough frames have been received to start recording,
   // the application stops the acquisition engine and stores each
   // one of the acquisition buffer to disk.
   // Unfortunately, function PHX_StreamRead(PHX_ABORT), used to stop
   // acquisition is not blocking and returns regardless of the state
   // of the acquisition engine. As a result a final buffer may be
   // acquired after the function returns.
   // If we would start recording straight after the function returns,
   // we may be recording this buffer (which would be in the processed
   // of being filled).
   // One solution would be to wait for some time after the function
   // returns. Such solution is not elegant as it depends on the rate
   // at which images are generated.
   // Instead an extra buffer is supplied to the acquisition engine to
   // receive that final image.
   // The recording starts immediately after returning from function
   // PHX_StreamRead(PHX_ABORT), but ALWAYS skips the first buffer.
   // This way, it does not matter if an extra frame was received or not,
   // as the buffer is always skipped.
   //***************************************************************
   uint32_t total_buffer_count = m_recordBufferCount + 1;
   _PHX_THROW_ON_ERROR(PHX_ParameterSet(m_hCamera, PHX_ACQ_NUM_IMAGES, &total_buffer_count));

   // Setup context for our event handler
   _PHX_THROW_ON_ERROR(PHX_ParameterSet(m_hCamera, static_cast<etParam>(PHX_EVENT_CONTEXT | PHX_CACHE_FLUSH), this));

   // If use of synchronised acquisition, open 2nd PHX device
   if (m_useSynchronizedAcquisition) openCamera2(pszConfigFileName, eBoardVariant, eBoardNumber, eConfigMode, total_buffer_count);

   _PHX_THROW_ON_ERROR(PHX_ParameterGet(m_hCamera, PHX_ACQ_BLOCKING, &m_blocking));

   int captureFormat;
   _PHX_THROW_ON_ERROR(PHX_ParameterGet(m_hCamera, PHX_CAPTURE_FORMAT, &captureFormat));

   int cameraSrcDepth;
   _PHX_THROW_ON_ERROR(PHX_ParameterGet(m_hCamera, PHX_CAM_SRC_DEPTH, &cameraSrcDepth));

   int cameraSrcCol;
   _PHX_THROW_ON_ERROR(PHX_ParameterGet(m_hCamera, PHX_CAM_SRC_COL, &cameraSrcCol));

   m_datastream = isDatastream();

   // camera pixel format
   switch(cameraSrcCol) {
      case PHX_CAM_SRC_MONO:
         switch(cameraSrcDepth) {
            case 8:  m_cameraPixelformat = CXP_PIXEL_FORMAT_MONO8;   break;
            case 10: m_cameraPixelformat = CXP_PIXEL_FORMAT_MONO10;  break;
            case 12: m_cameraPixelformat = CXP_PIXEL_FORMAT_MONO12;  break;
            default: THROW_UNRECOVERABLE(ActiveSilicon::CASLException::msgComposer("Number of bits per pixel not supported for Mono."));
         }
         break;
         
      case PHX_CAM_SRC_BAY_RGGB:
         switch(cameraSrcDepth) {
            case 8:  m_cameraPixelformat = CXP_PIXEL_FORMAT_BAYER_RG8;  break;
            case 10: m_cameraPixelformat = CXP_PIXEL_FORMAT_BAYER_RG10; break;
            case 12: m_cameraPixelformat = CXP_PIXEL_FORMAT_BAYER_RG12; break;
            default: THROW_UNRECOVERABLE(ActiveSilicon::CASLException::msgComposer("Number of bits per pixel not supported for Bayer."));
         }
         break;

      case PHX_CAM_SRC_BAY_GRBG:
         switch(cameraSrcDepth) {
            case 8:  m_cameraPixelformat = CXP_PIXEL_FORMAT_BAYER_GR8;  break;
            case 10: m_cameraPixelformat = CXP_PIXEL_FORMAT_BAYER_GR10; break;
            case 12: m_cameraPixelformat = CXP_PIXEL_FORMAT_BAYER_GR12; break;
            default: THROW_UNRECOVERABLE(ActiveSilicon::CASLException::msgComposer("Number of bits per pixel not supported for Bayer."));
         }
         break;

      case PHX_CAM_SRC_BAY_GBRG:
         switch(cameraSrcDepth) {
            case 8:  m_cameraPixelformat = CXP_PIXEL_FORMAT_BAYER_GB8;  break;
            case 10: m_cameraPixelformat = CXP_PIXEL_FORMAT_BAYER_GB10; break;
            case 12: m_cameraPixelformat = CXP_PIXEL_FORMAT_BAYER_GB12; break;
            default: THROW_UNRECOVERABLE(ActiveSilicon::CASLException::msgComposer("Number of bits per pixel not supported for Bayer."));
         }
         break;

      case PHX_CAM_SRC_BAY_BGGR:
         switch(cameraSrcDepth) {
            case 8:  m_cameraPixelformat = CXP_PIXEL_FORMAT_BAYER_BG8;  break;
            case 10: m_cameraPixelformat = CXP_PIXEL_FORMAT_BAYER_BG10; break;
            case 12: m_cameraPixelformat = CXP_PIXEL_FORMAT_BAYER_BG12; break;
            default: THROW_UNRECOVERABLE(ActiveSilicon::CASLException::msgComposer("Number of bits per pixel not supported for Bayer."));
         }
         break;

      case PHX_CAM_SRC_YUV:
         m_cameraPixelformat = CXP_PIXEL_FORMAT_YUV422;
         break;
      case PHX_CAM_SRC_RGB:
         switch(cameraSrcDepth) {
            case 8: m_cameraPixelformat = CXP_PIXEL_FORMAT_RGB24; break;
            default: THROW_UNRECOVERABLE(ActiveSilicon::CASLException::msgComposer("Number of bits per pixel not supported for RGB."));
         }
         break;

      default: THROW_UNRECOVERABLE(ActiveSilicon::CASLException::msgComposer("Pixel format not supported."));
   }

   // frame grabber pixel format
   if (!m_datastream) {
      switch(captureFormat) {
         case PHX_DST_FORMAT_Y8:    m_grabberPixelformat = CXP_PIXEL_FORMAT_MONO8;  break;
         case PHX_DST_FORMAT_Y10:   m_grabberPixelformat = CXP_PIXEL_FORMAT_MONO10; break;
         case PHX_DST_FORMAT_Y12:   m_grabberPixelformat = CXP_PIXEL_FORMAT_MONO12; break;
         case PHX_DST_FORMAT_BAY8:  
         {
            switch (cameraSrcCol) {
               case PHX_CAM_SRC_BAY_RGGB: m_grabberPixelformat = CXP_PIXEL_FORMAT_BAYER_RG8; break;
               case PHX_CAM_SRC_BAY_GRBG: m_grabberPixelformat = CXP_PIXEL_FORMAT_BAYER_GR8; break;
               case PHX_CAM_SRC_BAY_GBRG: m_grabberPixelformat = CXP_PIXEL_FORMAT_BAYER_GB8; break;
               case PHX_CAM_SRC_BAY_BGGR: m_grabberPixelformat = CXP_PIXEL_FORMAT_BAYER_BG8; break;
               default: THROW_UNRECOVERABLE(ActiveSilicon::CASLException::msgComposer("Pixel order not supported for Bayer decoding."));
            }
         } break;
         case PHX_DST_FORMAT_BAY10:
         {
            switch (cameraSrcCol) {
               case PHX_CAM_SRC_BAY_RGGB: m_grabberPixelformat = CXP_PIXEL_FORMAT_BAYER_RG10; break;
               case PHX_CAM_SRC_BAY_GRBG: m_grabberPixelformat = CXP_PIXEL_FORMAT_BAYER_GR10; break;
               case PHX_CAM_SRC_BAY_GBRG: m_grabberPixelformat = CXP_PIXEL_FORMAT_BAYER_GB10; break;
               case PHX_CAM_SRC_BAY_BGGR: m_grabberPixelformat = CXP_PIXEL_FORMAT_BAYER_BG10; break;
               default: THROW_UNRECOVERABLE(ActiveSilicon::CASLException::msgComposer("Pixel order not supported for Bayer decoding."));
            }
         } break;
         case PHX_DST_FORMAT_BAY12:
         {
            switch (cameraSrcCol) {
               case PHX_CAM_SRC_BAY_RGGB: m_grabberPixelformat = CXP_PIXEL_FORMAT_BAYER_RG12; break;
               case PHX_CAM_SRC_BAY_GRBG: m_grabberPixelformat = CXP_PIXEL_FORMAT_BAYER_GR12; break;
               case PHX_CAM_SRC_BAY_GBRG: m_grabberPixelformat = CXP_PIXEL_FORMAT_BAYER_GB12; break;
               case PHX_CAM_SRC_BAY_BGGR: m_grabberPixelformat = CXP_PIXEL_FORMAT_BAYER_BG12; break;
               default: THROW_UNRECOVERABLE(ActiveSilicon::CASLException::msgComposer("Pixel order not supported for Bayer decoding."));
            }
         } break;
         case PHX_DST_FORMAT_YUV422:
            m_grabberPixelformat = CXP_PIXEL_FORMAT_YUV422;
            break;
         case PHX_DST_FORMAT_RGB24:
            m_grabberPixelformat = CXP_PIXEL_FORMAT_RGB24;
            break;
         default: THROW_UNRECOVERABLE("Pixel format specified is not supported.");
      }
   } else {
      // in datastream mode, the destination format is the same as the source format but the grabber depth has to be set to 8 bits
      m_grabberPixelformat = m_cameraPixelformat;
   }

   if (!m_isCameraLink) InitialiseCamera();

   _PHX_THROW_ON_ERROR(PHX_ParameterGet(m_hCamera, PHX_ROI_XLENGTH, &m_imageWidth));
   _PHX_THROW_ON_ERROR(PHX_ParameterGet(m_hCamera, PHX_ROI_YLENGTH, &m_imageHeight));

   //estimate wrapped phase
   Nx = m_imageWidth;
   Ny = m_imageHeight;
   Nx2 = Nx / 4, Ny2 = Ny / 4;
   SP_float = (float *)malloc(Nx * Ny * sizeof(float));
   cudaMalloc((void **)&cuSP2, sizeof(cufftComplex)*Nx2*Ny2);
   cudaMalloc((void **)&SPWrapPhase2, sizeof(float)*Nx2*Ny2);
   cudaMalloc((void **)&UnWrapPhaseSP2, sizeof(float)*Nx2*Ny2);
   cudaMalloc((void **)&cuPhaseMap2, sizeof(float)*Nx2*Ny2);
   cudaMalloc((void **)&cuPhaseMap, sizeof(float)*Nx*Ny);
   cudaMalloc((void **)&tmp_uint8, sizeof(uint8_t)*Nx*Ny);
   //phase unwrapping
   cudaMalloc((void **)&LaplaceArray, sizeof(float)*Nx2*Ny2);
   cudaMalloc((void **)&inX, sizeof(float)*Nx2*Ny2);
   cudaMalloc((void **)&inY, sizeof(float)*Nx2*Ny2);
   cudaMalloc((void **)&outX, sizeof(float)*Nx2*Ny2);
   cudaMalloc((void **)&outY, sizeof(float)*Nx2*Ny2);
   size_t DeviceStride;
   cudaMallocPitch((void **)&dst_dct, &DeviceStride, Nx2 * sizeof(float), Ny2);
   cudaMalloc((void **)&dSrc_tmp, sizeof(cufftComplex)*(Nx2 * 2)*Ny2);
   cufftPlan1d(&cuFFT1D_plan1_FORWARD, Nx2 * 2, CUFFT_C2C, Ny2);
   cufftPlan1d(&cuFFT1D_plan2_FORWARD, Ny2 * 2, CUFFT_C2C, Nx2);
   cufftPlan1d(&cuFFT1D_plan1_INVERSE, Nx2 * 2, CUFFT_C2C, Ny2);
   cufftPlan1d(&cuFFT1D_plan2_INVERSE, Ny2 * 2, CUFFT_C2C, Nx2);

   if (m_imageWidth  < m_recordWidth)  THROW_NOT_SUPPORTED(ActiveSilicon::CASLException::msgComposer("Image width (%d) must be greater than width used for recording (%d)",   m_imageWidth,  m_recordWidth));
   if (m_imageHeight < m_recordHeight) THROW_NOT_SUPPORTED(ActiveSilicon::CASLException::msgComposer("Image height (%d) must be greater than height used for recording (%d)", m_imageHeight, m_recordHeight));

   _PHX_THROW_ON_ERROR(PHX_ParameterGet(m_hCamera, PHX_BUF_DST_XLENGTH, &m_widthBytes));
   _PHX_THROW_ON_ERROR(PHX_ParameterGet(m_hCamera, PHX_BUF_DST_YLENGTH, &m_heightBytes));

   if (m_datastream) {
      // calculate packed width
      m_packedDepth = cameraSrcDepth;
      m_packedWidth = m_widthBytes * m_packedDepth / 8;
      if (m_packedWidth % 16) {
         m_packedExtraBits = 16 - (m_packedWidth % 16);     // packed width has to be a multiple of 16
         m_packedWidth += m_packedExtraBits;                // if it is not the case, extra bits are added at the end of each line
      }

      // the width in bytes of the unpacked data is doubled
      m_widthBytes *= 2;

      // set frame grabber ROI and Camera widths to the packed width
      _PHX_THROW_ON_ERROR(PHX_ParameterSet(m_hCamera, PHX_ROI_XLENGTH,        &m_packedWidth));
      _PHX_THROW_ON_ERROR(PHX_ParameterSet(m_hCamera, PHX_CAM_ACTIVE_XLENGTH, &m_packedWidth));
   }

   // Set up virtual buffers for synchronized acquisition
   if (m_useSynchronizedAcquisition) synchronizedAcquisitionSetup(total_buffer_count);

   // initialize buffers used for display
   m_currentDisplayBuffer.bytes           = m_widthBytes * m_heightBytes;
   m_currentDisplayBuffer.cxpPixelFormat  = m_grabberPixelformat;
   m_currentDisplayBuffer.height          = m_imageHeight;
   m_currentDisplayBuffer.width           = m_imageWidth;

   m_nextDisplayBuffer = m_currentDisplayBuffer; // two buffers are used so that one can be displayed while the other one gets updated with new data

   // allocate memory for display buffers
   std::tr1::shared_ptr<void> data1(new char [m_widthBytes * m_heightBytes]);
   std::tr1::shared_ptr<void> data2(new char [m_widthBytes * m_heightBytes]);
   m_currentDisplayBuffer.data   = data1;
   m_nextDisplayBuffer.data      = data2;

   // Starting threads and starting acquisition
   m_bufferProcessingThread      = CreateThread(NULL, 0, BufferProcessingThread,       this, 0, &m_bufferProcessingThreadID);
  // m_ni_deviceThread             = CreateThread(NULL, 0, NIdeviceThread,               this, 0, &m_ni_deviceThreadID);
   m_recordThread                = CreateThread(NULL, 0, RecordThread,                 this, 0, &m_recordThreadID);

   m_eventFromRecording          = CreateEvent(NULL, true, false, 0);

   AcquisitionStart();

   // Start watchdog last so that it does not kick-in while acquisition is being started
   m_watchdogThread              = CreateThread(NULL, 0, WatchdogThread,               this, 0, &m_watchdogThreadID);

   m_movestageThread = CreateThread(NULL, 0, MoveStageThread, this, 0, &m_movestageThreadID);

   // Start frame rate monitoring thread
   m_acquisitionFramerateThread = CreateThread(NULL, 0, AcquisitionFramerateThread, this, 0, &m_acquisitionFramerateThreadID);
}

void CCamera::openCamera2(char * configFileName, etParamValue boardVariant, etParamValue boardNumber, etParamValue configMode, uint32_t bufferCount)
{
   etParamValue channelNumber = PHX_CHANNEL_NUMBER_1;
   _PHX_THROW_ON_ERROR(PHX_ParameterSet(m_hCamera, PHX_CHANNEL_NUMBER,  &channelNumber));

   _PHX_THROW_ON_ERROR(PHX_Create(&m_hCamera2, PHX_ErrHandlerDefault));
   _PHX_THROW_ON_ERROR(PHX_ParameterSet(m_hCamera2, PHX_CONFIG_FILE, &configFileName));

   channelNumber = PHX_CHANNEL_NUMBER_2;
   _PHX_THROW_ON_ERROR(PHX_ParameterSet(m_hCamera2, PHX_BOARD_VARIANT,   &boardVariant));
   _PHX_THROW_ON_ERROR(PHX_ParameterSet(m_hCamera2, PHX_BOARD_NUMBER,    &boardNumber));
   _PHX_THROW_ON_ERROR(PHX_ParameterSet(m_hCamera2, PHX_CHANNEL_NUMBER,  &channelNumber));
   _PHX_THROW_ON_ERROR(PHX_ParameterSet(m_hCamera2, PHX_CONFIG_MODE,     &configMode));

   _PHX_THROW_ON_ERROR(PHX_Open(m_hCamera2));

   _PHX_THROW_ON_ERROR(PHX_ParameterSet(m_hCamera2, PHX_ACQ_NUM_IMAGES, &bufferCount));

   _PHX_THROW_ON_ERROR(PHX_ParameterSet(m_hCamera2, static_cast<etParam>(PHX_EVENT_CONTEXT | PHX_CACHE_FLUSH), this));
}

void CCamera::synchronizedAcquisitionSetup(uint32_t bufferCount)
{
   m_imageWidth *= 2;
   m_widthBytes *= 2;

   _PHX_THROW_ON_ERROR(PHX_ParameterSet(m_hCamera, PHX_BUF_DST_XLENGTH,  &m_widthBytes));
   _PHX_THROW_ON_ERROR(PHX_ParameterSet(m_hCamera2, PHX_BUF_DST_XLENGTH,  &m_widthBytes));

   uint32_t offset = m_widthBytes / 2;
   _PHX_THROW_ON_ERROR(PHX_ParameterSet(m_hCamera2, PHX_ROI_DST_XOFFSET, &offset));

   // Allocate memory manually for acquisition
   stImageBuff * pasImageBuffs = new stImageBuff[bufferCount + 1]; // alloc space to hold the array of image pointers
   if (!pasImageBuffs) THROW_RUNTIME("Failed allocating pasImageBuffs");
   
   for (uint32_t i = 0; i < bufferCount; i++) {
      std::tr1::shared_ptr<void> x(new char [m_widthBytes * m_heightBytes]);
      m_virtualBuffers.push_back(x);
      pasImageBuffs[i].pvAddress = x.get();
      pasImageBuffs[i].pvContext = reinterpret_cast<void *>(i);
   }

   // Terminate the list of stImage buffers
   pasImageBuffs[bufferCount].pvAddress = NULL;
   pasImageBuffs[bufferCount].pvContext = NULL;
   
   // Camera #1
   _PHX_THROW_ON_ERROR(PHX_ParameterSet(m_hCamera, PHX_DST_PTRS_VIRT,   pasImageBuffs)); // Instruct Phoenix to use the user supplied Virtual Buffers

   int eParamValue = PHX_DST_PTR_USER_VIRT;
   _PHX_THROW_ON_ERROR(PHX_ParameterSet(m_hCamera, PHX_DST_PTR_TYPE,    &eParamValue));

   // Camera #2
   _PHX_THROW_ON_ERROR(PHX_ParameterSet(m_hCamera2, PHX_DST_PTRS_VIRT,  pasImageBuffs)); // Instruct Phoenix to use the user supplied Virtual Buffers

   eParamValue = PHX_DST_PTR_USER_VIRT;
   _PHX_THROW_ON_ERROR(PHX_ParameterSet(m_hCamera2, PHX_DST_PTR_TYPE,   &eParamValue));
}

// Add image TMGImage to the target. The TMG image is stored in ADL image object pADL_Image.
// x, y: target's origin point within the display
// y < 0: image positined below y axis
// x < 0: image positioned left of x axis
// dwWidth, dwHeight: if non-null the ROI, otherwise ROI is the full image
void CCamera::addImage (
                          IADL_Target *& pT
                        , int          & TMGImage
                        , IADL_Image  *& pADL_Image
                        , const char   * szFilename
                        , int x, int y, bool bColorKey, int dwColorKey, int dwWidth, int dwHeight)
{
   if (!szFilename)  THROW_UNRECOVERABLE("Null file name specified");   
   if (!TMGImage)    THROW_UNRECOVERABLE("TMG Image should be null");
 
	_TMG_THROW_ON_ERROR(TMG_Image_FilenameSet(TMGImage, TMG_FILENAME_IN, const_cast<char *>(szFilename)));

   if (TMG_Image_FileRead(TMGImage) != TMG_OK) {
      THROW_UNRECOVERABLE(ActiveSilicon::CASLException::msgComposer("%s needs to be in current folder.", szFilename));
   }

   _ADL_THROW_ON_ERROR(ImageFromTMG(&pADL_Image, TMGImage));
   if (pT) _ADL_THROW_ON_ERROR(pT->SetAttribute(ADL_TARGET_ATTRIBUTE_IMAGE, reinterpret_cast<void *>(pADL_Image)));

   if (!dwWidth)  if (pADL_Image) _ADL_THROW_ON_ERROR(pADL_Image->GetAttribute(ADL_IMAGE_ATTRIBUTE_WIDTH, reinterpret_cast<void *>(&dwWidth)));
   if (!dwHeight) if (pADL_Image) _ADL_THROW_ON_ERROR(pADL_Image->GetAttribute(ADL_IMAGE_ATTRIBUTE_HEIGHT, reinterpret_cast<void *>(&dwHeight)));

   ADL_RECT Rect = {0, 0, dwWidth, dwHeight};
   ADL_POINT Origin;

   if (y > 0)  Origin.y = y;              // image above y
   else        Origin.y = dwHeight - y;   // image below y

   if (x > 0)  Origin.x = x;              // image right of x
   else        Origin.x = x - dwWidth;    // image left of x

   if (pT) _ADL_THROW_ON_ERROR(pT->SetAttribute(ADL_TARGET_ATTRIBUTE_GEOMETRY, reinterpret_cast<void *>(&Rect)));
   if (pT) _ADL_THROW_ON_ERROR(pT->SetAttribute(ADL_TARGET_ATTRIBUTE_LOCATION, reinterpret_cast<void *>(&Origin)));

   if (bColorKey) {
      if (pT) _ADL_THROW_ON_ERROR(pT->SetAttribute(ADL_TARGET_ATTRIBUTE_PROCESSING_COLORKEY, reinterpret_cast<void *>(dwColorKey)));
      if (pT) _ADL_THROW_ON_ERROR(pT->SetAttribute(ADL_TARGET_ATTRIBUTE_PROCESSING,          reinterpret_cast<void *>(1)));
   } else {
      if (pT) _ADL_THROW_ON_ERROR(pT->SetAttribute(ADL_TARGET_ATTRIBUTE_PROCESSING,          reinterpret_cast<void *>(0)));
   }
}

// set the frame grabber to free run mode
void CCamera::initFreeRunMode()
{
   hostSetParameter(PHX_FGTRIG_MODE, PHX_FGTRIG_FREERUN);
   hostSetParameter((etParam)(PHX_DUMMY_PARAM | PHX_CACHE_FLUSH), NULL);
}

void CCamera::toggleTriggerModeOnOff(uint8_t mode)
{
   if (mode >= m_acquisitionModes.size()) return; // selected mode is out of range

   // If recording has been signaled, the frame grabber acquisition has been stopped and the record thread is then using acquisition buffers.
   // Toggling trigger mode would restart the frame grabber and let another thread use the acquisition buffers, which is unsafe.
   if (m_recordingThreadSignaledAtomic) return;

   // If the current mode is different from free run, the acquisition mode will be set to free run
   // If not, the acquisition will be set to the selected mode
   m_acquisitionModeSelectedAtomic = /*m_acquisitionModeSelectedAtomic ? 0 :*/ mode;

   stopCamera();
   stopFrameGrabber();
   m_recordAtomic = 0; // stop recording

   m_buffersBeforeTrigger  = 0; // in trigger mode, the recording should start at once (reset to its initial value in free run)

   if (m_acquisitionModeSelectedAtomic == 1) {

      initTriggerMode1();

   } else if (m_acquisitionModeSelectedAtomic == 2) {

      initTriggerMode2();

   } else if (m_acquisitionModeSelectedAtomic == 3) {

      initTriggerMode3();

   } else {
      
      m_buffersBeforeTrigger  = m_initialBuffersBeforeTrigger;
      initFreeRunMode();

   }

   startFrameGrabber();
   startCamera();
}

uint8_t CCamera::toggleCameraAcquisitionOnOff()
{
   // start or stop camera acquisition, depending on current status
   if (m_isCameraRunningAtomic) {
      stopCamera();
   } else {
      startCamera();
   }
   return m_isCameraRunningAtomic;
}

void CCamera::toggleFrameStartEvent()
{
   if (m_FrameStartEventEnabledAtomic) {
      hostSetParameter(PHX_INTRPT_CLR, PHX_INTRPT_FRAME_START);
      m_FrameStartEventEnabledAtomic = false;
   } else {
      hostSetParameter(PHX_INTRPT_SET, PHX_INTRPT_FRAME_START);
      m_FrameStartEventEnabledAtomic = true;
      m_FrameStartEventCount = 0;
   }
   hostSetParameter((etParam)(PHX_DUMMY_PARAM | PHX_CACHE_FLUSH), NULL);
}

void CCamera::startFrameGrabber()
{
   stopFrameGrabber(); // make sure the frame grabber is stopped before starting it

   // now that the frame grabber is stopped, we do not want to wait for semaphores anymore
   m_semaphoreFlagAtomic = 1;

   Sleep(500); // allow time for WaitForSingleObject() (timeout is 250 ms) function to finish the wait for semaphores (behaviour is undefined if the handle closes while waiting is still pending)

   // when we start ( - restart) the frame grabber, we need to set ( - reset) the semaphore count
   if (m_bufferReadySemaphore) CloseHandle(m_bufferReadySemaphore); // there should be no waiting pending, as we have waited long enough for a semaphore after setting m_semaphoreFlagAtomic to 1
   m_bufferReadySemaphore = CreateSemaphore(NULL, 0, m_recordBufferCount + 1, NULL);

   // for thread safety, we use interlocked exchanges to set ( - reset) the counts (it should not be needed as the frame grabber is stopped and there will be no callbacks)
   uint32_t callbackCount = 0;
   InterlockedExchange(reinterpret_cast<volatile LONG *>(&m_callbackCount), *reinterpret_cast<LONG *>(&callbackCount));
   uint32_t recordingBuffersSoFar = 0;
   InterlockedExchange(reinterpret_cast<volatile LONG *>(&m_recordingBuffersSoFar), *reinterpret_cast<LONG *>(&recordingBuffersSoFar));

   // also reset counts used for synchronised acquisition
   m_cameraCallbackCount   = 0;
   m_camera2CallbackCount  = 0;

   // update the number of buffers after trigger in case the number of buffers before trigger has changed
   m_buffersAfterTrigger   = m_recordBufferCount - m_buffersBeforeTrigger;
   
   m_semaphoreFlagAtomic = 0; // we can now restart waiting for semaphores
   _PHX_THROW_MSG_ON_ERROR(PHX_StreamRead(m_hCamera, PHX_START, (void*)PHX_CallbackEvent), "Failed to start acquisition (PHX_StreamRead(PHX_START))");
   if (m_useSynchronizedAcquisition && m_hCamera2)
      _PHX_THROW_MSG_ON_ERROR(PHX_StreamRead(m_hCamera2, PHX_START, (void*)PHX_CallbackEvent), "Failed to start acquisition (PHX_StreamRead(PHX_START))");

   m_isFrameGrabberRunningAtomic = 1;
}

void CCamera::stopFrameGrabber()
{
   _PHX_THROW_MSG_ON_ERROR(PHX_StreamRead(m_hCamera, PHX_ABORT, NULL), "Failed to abort acquisition (PHX_StreamRead(PHX_ABORT))");
   if (m_useSynchronizedAcquisition && m_hCamera2)
      _PHX_THROW_MSG_ON_ERROR(PHX_StreamRead(m_hCamera2, PHX_ABORT, NULL), "Failed to abort acquisition (PHX_StreamRead(PHX_ABORT))");

   m_isFrameGrabberRunningAtomic = 0;
   Sleep(100); // wait for the final hypothetical callback
}

void CCamera::restartAcquisition()
{
   if (!m_recordAtomic) startFrameGrabber(); // the frame grabber is stopped in startFrameGrabber()
}

void CCamera::startCamera()
{
   setAcquisitionStart();
   m_isCameraRunningAtomic = 1;
}

void CCamera::stopCamera()
{
   setAcquisitionStop();
   m_isCameraRunningAtomic = 0;
}

DWORD CCamera::semaphoreWait()
{
   DWORD dwStatus = WAIT_TIMEOUT;
   if (!m_semaphoreFlagAtomic) { // if the flag is not signaled, the semaphore is not going to be closed (in startFrameGrabber()) so we can use it
      dwStatus = WaitForSingleObject(m_bufferReadySemaphore, 250); // should always return, even if it fails
   }
   return dwStatus;
}

void CCamera::saveToTIFF(std::string filename)
{
   //filename = m_strOutputDir + std::string("\\") + filename;
   filename = FolderPath + std::string("\\") + filename;
   DoSaveToTiff(m_displayCopyOfTMGImage, filename, true);
}

void CCamera::saveToRAW(std::string filename)
{
   //filename = m_strOutputDir + std::string("\\") + filename;
   filename = FolderPath + std::string("\\") + filename;
   DoSaveToRaw(m_displayCopyOfTMGImage, filename);
}

void CCamera::saveToBMP(std::string filename)
{
   //filename = m_strOutputDir + std::string("\\") + filename;
   filename = FolderPath + std::string("\\") + filename;
   DoSaveToBmp(m_displayCopyOfTMGImage, filename);
}

void CCamera::saveImageToFile(std::string filename)
{
   if (!m_recordAtomic) { // only if recording has not been triggered
      m_saveImageToFileFlagAtomic = 1;
      m_saveImageToFileName = filename;
   }
}

void CCamera::RecoverAcquisition()
{
   printf("\n\n");
   printf("***************************\n");
   printf("Watchdog thread triggered: entering recovery state.\n");
   printf("***************************\n");
   printf("\n\n");
   DumpStatusInfo();
   if (!m_isCameraLink) InitialiseCamera();
   AcquisitionStart();
   printf("\n\n");
   printf("***************************\n");
   printf("Watchdog thread triggered: end of recovery.\n");
   printf("***************************\n");
   printf("\n\n");
}

#define PRINT_INDENT(indent) \
   printf("%*s", indent, "");

#define PARAM_GET(indent, param, value) \
   PRINT_INDENT(indent);\
   try {\
      _PHX_THROW_ON_ERROR(PHX_ParameterGet(m_hCamera, param, &value));\
   } catch (std::exception &e) {\
      printf("Failure: %s - ", e.what());\
   }\
   printf("%s = 0x%08X\n", #param, value);

#define PARAM_PRINT_FIELD(indent, field, value) \
   PRINT_INDENT(indent);\
   printf("%s = %1d\n", #field, value);

#define PARAM_PRINT_FIELD_BOOL(indent, field, value) \
   PRINT_INDENT(indent);\
   printf("%s = %1d\n", #field, (value & field) != 0);

#define PARAM_PRINT_VALUE_EQUAL(indent, ref, value) \
   PRINT_INDENT(indent);\
   printf("%s = %1d\n", #ref, value == ref);

#define CAMREG_PRINT_INT(indent, reg, success, specific) \
   {\
      PRINT_INDENT(indent);\
      int32_t value;\
      try {\
         deviceRead(reg, value);\
      } catch (std::exception &e) {\
         printf("Failure: %s - ", e.what());\
         success = false;\
      }\
      if (!specific) printf("%s = 0x%08X\n", #reg, value);\
      else printf("CXP_REG 0x%08X: 0x%08X\n", reg, value);\
   }

#define CAMREG_PRINT_STRING(indent, reg, size, success, specific) \
   {\
      PRINT_INDENT(indent);\
      std::string s;\
      try {\
         deviceRead(reg, size, s);\
      } catch (std::exception &e) {\
         printf("Failure: %s - ", e.what());\
         success = false;\
      }\
      if (!specific) printf("%s = %s\n", #reg, s.c_str());\
      else printf("CXP_REG 0x%08X: %s\n", reg, s.c_str());\
   }

#define GRABREG_PRINT_INT(indent, reg) \
   {\
      PRINT_INDENT(indent);\
      int32_t value;\
      try {\
         hostReadWord(reg, value);\
      } catch (std::exception &e) {\
         printf("Failure: %s - ", e.what());\
      }\
      printf("%s = 0x%08X\n", #reg, value);\
   }

void CCamera::printCameraIntInfo(uint32_t address)
{
   bool success = true;
   CAMREG_PRINT_INT(0, address, success, 1); // specific is set to 1 so the value of address gets printed instead of #address
}

void CCamera::printCameraStringInfo(uint32_t address, size_t size)
{
   bool success = true;
   CAMREG_PRINT_STRING(0, address, size, success, 1); // specific is set to 1 so the value of address gets printed instead of #address
}

void CCamera::DumpStatusInfo()
{
   bool IsCameraDiscovered = true;

   printf("\n\n");
   printf("***************************\n");
   printf("Start of system status dump \n");
   printf("***************************\n");
   printf("\n\n");

   printf("Frame grabber status\n");
   printf("---------------------------\n");
   printf("\n\n");

   etParamValue eParam;

   PARAM_GET(0, PHX_REV_HW_MAJOR, eParam);
   PARAM_GET(0, PHX_REV_HW_MINOR, eParam);
   PARAM_GET(0, PHX_REV_HW_SUBMINOR, eParam);
   PARAM_GET(0, PHX_REV_SW_MAJOR, eParam);
   PARAM_GET(0, PHX_REV_SW_MINOR, eParam);
   PARAM_GET(0, PHX_REV_SW_SUBMINOR, eParam);

   PARAM_GET(0, PHX_CXP_INFO, eParam);
   
   IsCameraDiscovered = (eParam & PHX_CXP_CAMERA_DISCOVERED) != 0;
   
   PARAM_PRINT_FIELD_BOOL(2, PHX_CXP_CAMERA_DISCOVERED, eParam);
   PARAM_PRINT_FIELD_BOOL(2, PHX_CXP_CAMERA_IS_POCXP, eParam);
   PARAM_PRINT_FIELD_BOOL(2, PHX_CXP_POCXP_UNAVAILABLE, eParam);
   PARAM_PRINT_FIELD_BOOL(2, PHX_CXP_POCXP_TRIPPED, eParam);
   PARAM_PRINT_FIELD_BOOL(2, PHX_CXP_LINK1_USED, eParam);
   PARAM_PRINT_FIELD_BOOL(2, PHX_CXP_LINK2_USED, eParam);
   PARAM_PRINT_FIELD_BOOL(2, PHX_CXP_LINK3_USED, eParam);
   PARAM_PRINT_FIELD_BOOL(2, PHX_CXP_LINK4_USED, eParam);
   PARAM_PRINT_FIELD_BOOL(2, PHX_CXP_LINK1_MASTER, eParam);
   PARAM_PRINT_FIELD_BOOL(2, PHX_CXP_LINK2_MASTER, eParam);
   PARAM_PRINT_FIELD_BOOL(2, PHX_CXP_LINK3_MASTER, eParam);
   PARAM_PRINT_FIELD_BOOL(2, PHX_CXP_LINK4_MASTER, eParam);

   PARAM_GET(0, PHX_CXP_BITRATE, eParam);
   PARAM_PRINT_VALUE_EQUAL(2, PHX_CXP_BITRATE_UNKNOWN, eParam);
   PARAM_PRINT_VALUE_EQUAL(2, PHX_CXP_BITRATE_CXP1, eParam);
   PARAM_PRINT_VALUE_EQUAL(2, PHX_CXP_BITRATE_CXP2, eParam);
   PARAM_PRINT_VALUE_EQUAL(2, PHX_CXP_BITRATE_CXP3, eParam);
   PARAM_PRINT_VALUE_EQUAL(2, PHX_CXP_BITRATE_CXP5, eParam);
   PARAM_PRINT_VALUE_EQUAL(2, PHX_CXP_BITRATE_CXP6, eParam);

   PARAM_GET(0, PHX_CXP_DISCOVERY, eParam);
   PARAM_PRINT_VALUE_EQUAL(2, PHX_CXP_DISCOVERY_UNKNOWN, eParam);
   PARAM_PRINT_VALUE_EQUAL(2, PHX_CXP_DISCOVERY_1X, eParam);
   PARAM_PRINT_VALUE_EQUAL(2, PHX_CXP_DISCOVERY_2X, eParam);
   PARAM_PRINT_VALUE_EQUAL(2, PHX_CXP_DISCOVERY_4X, eParam);

   GRABREG_PRINT_INT(0, 0x016000);
   GRABREG_PRINT_INT(0, 0x216000);
   GRABREG_PRINT_INT(0, 0x416000);
   GRABREG_PRINT_INT(0, 0x616000);

   printf("\n\n");
   printf("Camera status\n");
   printf("---------------------------\n");
   printf("\n\n");

   if (IsCameraDiscovered) {
      bool success = true;
      CAMREG_PRINT_INT(0, CXP_REG_STANDARD, success, 0);
      if (!success) {
         printf("Failed to read CXP_REG_STANDARD from camera not attempting to read further registers.\n");
      } else {
         CAMREG_PRINT_INT(0, CXP_REG_XMLMANIFESTSIZE, success, 0);
         CAMREG_PRINT_INT(0, CXP_REG_XMLMANIFESTSELECTOR, success, 0);
         CAMREG_PRINT_INT(0, CXP_REG_XMLVERSION, success, 0);
         CAMREG_PRINT_INT(0, CXP_REG_XMLSCHEMAVERSION, success, 0);
         CAMREG_PRINT_INT(0, CXP_REG_XMLURLADDRESS, success, 0);
         CAMREG_PRINT_INT(0, CXP_REG_LIDCPOINTER, success, 0);

         CAMREG_PRINT_STRING(0, CXP_REG_DEVICEVENDORNAME, 32, success, 0);
         CAMREG_PRINT_STRING(0, CXP_REG_DEVICEMODELNAME, 32, success, 0);
         CAMREG_PRINT_STRING(0, CXP_REG_DEVICEMANUFACTURERINFO, 48, success, 0);
         CAMREG_PRINT_STRING(0, CXP_REG_DEVICEVERSION, 32, success, 0);
         CAMREG_PRINT_STRING(0, CXP_REG_DEVICEFIRMWAREVERSION, 32, success, 0);
         CAMREG_PRINT_STRING(0, CXP_REG_DEVICEID, 16, success, 0);
         CAMREG_PRINT_STRING(0, CXP_REG_DEVICEUSERID, 16, success, 0);

         CAMREG_PRINT_INT(0, CXP_REG_LINKID, success, 0);
         CAMREG_PRINT_INT(0, CXP_REG_MASTERHOSTLINKID, success, 0);
         CAMREG_PRINT_INT(0, CXP_REG_CONTROLPACKETDATASIZE, success, 0);
         CAMREG_PRINT_INT(0, CXP_REG_STREAMPACKETDATASIZE, success, 0);
         CAMREG_PRINT_INT(0, CXP_REG_LINKCONFIG, success, 0);
         CAMREG_PRINT_INT(0, CXP_REG_LINKCONFIGDEFAULT, success, 0);

         dumpCameraInfo();
      }
   } else {
      printf("Camera is not discovered, not attempting to read any status info.\n");
   }

   printf("\n\n");
   printf("***************************\n");
   printf("End of system status dump.\n");
   printf("***************************\n");
   printf("\n\n");
}

void CCamera::AcquisitionStart()
{
   stopCamera();

   initFreeRunMode();

   startFrameGrabber();

   startCamera();
}

void CCamera::InitialiseCamera()
{
   // Attempt to initialise the camera.
   bool IsHardwareOperational;
   for (int loop = 0; loop < 3; loop++) {
      IsHardwareOperational = true;
      try {
         printf("Checking for 'tripped' state\n");
         // Current implementation of PHX_CXP_POCXP parameters only allow to check and resolve trip status for the 1st connection of the link.
         // Code below uses direct register access to the frame grabber to access the other Connections.
         // This is a work-around and should not be used in production code as the addesses used below will change without notification.

         // If Connection #0 has tripped the current PHX lets us untrip it. But for the others it's impossible to untrip without open a 
         // handle to each one of the channels individually.

         // Note that cameras that do not use the other Connections would be able to work even if the other Connections are tripped.
         // For simplicity, this application does not take this factor into consideration and will fail if any of Connections #1, #2 or #3
         // is tripped.
         int32_t reg_1, reg_2, reg_3, reg_4;
         hostReadWord(0x016000, reg_1);
         hostReadWord(0x216000, reg_2);
         hostReadWord(0x416000, reg_3);
         hostReadWord(0x616000, reg_4);

#define IS_TRIPPED(x) (((x >> 25) & 3) == 1)
         etParamValue eBoardVariant;
         _PHX_THROW_ON_ERROR(PHX_ParameterGet(m_hCamera, PHX_BOARD_VARIANT, &eBoardVariant));

         if (eBoardVariant == PHX_BOARD_FBD_4XCXP6_2PE8 && (IS_TRIPPED(reg_2) || IS_TRIPPED(reg_3) || IS_TRIPPED(reg_4))) {
            THROW_UNRECOVERABLE("Connection #2, #3 or #4  has tripped and cannot be untripped to due limitation in PHX API. Application will now terminate.");
         } else if (eBoardVariant == PHX_BOARD_FBD_2XCXP6_2PE8 && IS_TRIPPED(reg_3)) { // Physically link 3 and not 2
            THROW_UNRECOVERABLE("Connection #2 has tripped and cannot be untripped to due limitation in PHX API. Application will now terminate.");
         }
         
         // Now deal with Connection #0 which we can reset
         etParamValue eParamCxPInfo;
         _PHX_THROW_ON_ERROR(PHX_ParameterGet(m_hCamera, PHX_CXP_INFO, &eParamCxPInfo));
         if (eParamCxPInfo & PHX_CXP_POCXP_TRIPPED) {
            etParamValue ePowerMode = PHX_CXP_POCXP_MODE_TRIP_RESET;
            _PHX_THROW_ON_ERROR(PHX_ParameterSet(m_hCamera, static_cast<etParam>(PHX_CXP_POCXP_MODE | PHX_CACHE_FLUSH), &ePowerMode ));

            printf("Waiting for camera to power up\n");
            Sleep(getPowerUpTimeMs()); // since power had tripped camera has now to reboot

            _PHX_THROW_ON_ERROR(PHX_ParameterGet(m_hCamera, PHX_CXP_INFO, &eParamCxPInfo));
            if (eParamCxPInfo & PHX_CXP_POCXP_TRIPPED) THROW_UNRECOVERABLE("Connection #1 did not untrip (short-circuit?). Application will now terminate.");
            printf("Connection #1 untripped\n");
         }
         
         // Configure discovery settings
         printf("Setting bitrate and discovery mode\n");
         SetBitRateAndDiscovery(m_lanes, m_gbps);

         // Hardware should now be discovered, check if it is operationnal by reading bootstrap register
         printf("Try to access camera register\n");
         int32_t x = 0;
         deviceRead(CXP_REG_STANDARD, x);
      } catch (PHXException &) { // PHXException from which we may be able to recover as it could be the hardware
                                 // failing to execute the command.
         IsHardwareOperational = false;
      } catch (HWException &) {  // HWException means hardware failure from which we may be able to recover.
         IsHardwareOperational = false;
      } catch (...) {            // Other exceptions we can't recover from and abort app.
         throw;
      }

      if (!IsHardwareOperational && loop < 2) {
         printf("\nCamera is not responding, power cycling it. This will take %d s\n", getPowerUpTimeMs() / 1000);
         PowerCycleCamera();
         printf("\nDone.\n");
      } else {
         break;
      }
   }

   // If we are in the startup of the application and the hardware is not operational, the application will quit
   if (!m_isRecoveringAtomic) {
      if (!IsHardwareOperational) THROW_UNRECOVERABLE("\nCamera is still not responding, application will now close.\n");
   }

   // Camera must be stopped before we can write its registers
   stopCamera();
   
   setPixelFormat(m_cameraPixelformat);
   setMiscellaneous();
}

void CCamera::PowerCycleCamera()
{
   hostSetParameter(static_cast<etParam>(PHX_CXP_POCXP_MODE | PHX_CACHE_FLUSH), PHX_CXP_POCXP_MODE_OFF);
   Sleep(100);
   hostSetParameter(static_cast<etParam>(PHX_CXP_POCXP_MODE | PHX_CACHE_FLUSH), PHX_CXP_POCXP_MODE_AUTO);
   Sleep(getPowerUpTimeMs());
}

void CCamera::SetBitRateAndDiscovery(int lanes, int gbps)
// Manually configures the number of CXP lanes and the bitrate.
{
   if (m_isRecoveringAtomic) {
      // If the watchdog is trying a recovery, the frame grabber first has to be set back to auto discovery and auto bitrate
      // because the camera might start up at a different state from the current frame grabber settings and therefore not get discovered.
      etParamValue eParamValue = PHX_CXP_DISCOVERY_MODE_AUTO;
      _PHX_THROW_ON_ERROR(PHX_ParameterSet(m_hCamera, static_cast<etParam>(PHX_CXP_DISCOVERY_MODE | PHX_CACHE_FLUSH), &eParamValue));
      eParamValue = PHX_CXP_BITRATE_MODE_AUTO;
      _PHX_THROW_ON_ERROR(PHX_ParameterSet(m_hCamera, static_cast<etParam>(PHX_CXP_BITRATE_MODE | PHX_CACHE_FLUSH), &eParamValue));
   }

   // check that camera is discovered
   for (int i=0;;i++) {
      etParamValue eParamBitInfo;
      _PHX_THROW_ON_ERROR(PHX_ParameterGet(m_hCamera, static_cast<etParam>(PHX_CXP_INFO), &eParamBitInfo));
      if (eParamBitInfo & PHX_CXP_CAMERA_DISCOVERED) break;
      Sleep(500);
      if (i > 40) THROW_HW_EXCEPTION("Discovery state unknown");
   }

   etParamValue eParamValue;
   switch(lanes) {
      case 0: eParamValue = PHX_CXP_DISCOVERY_MODE_AUTO; break;
      case 1: eParamValue = PHX_CXP_DISCOVERY_MODE_1X;   break;
      case 2: eParamValue = PHX_CXP_DISCOVERY_MODE_2X;   break;
      case 4: eParamValue = PHX_CXP_DISCOVERY_MODE_4X;   break;
      THROW_UNRECOVERABLE(ActiveSilicon::CASLException::msgComposer("Invalid number of lanes requested (%d)", lanes));
   }
   // Set discovery mode before bitrate mode.
   _PHX_THROW_ON_ERROR(PHX_ParameterSet(m_hCamera, static_cast<etParam>(PHX_CXP_DISCOVERY_MODE | PHX_CACHE_FLUSH), &eParamValue));

   switch(gbps) {
      case 0: eParamValue = PHX_CXP_BITRATE_MODE_AUTO; break;
      case 1: eParamValue = PHX_CXP_BITRATE_MODE_CXP1; break;
      case 2: eParamValue = PHX_CXP_BITRATE_MODE_CXP2; break;
      case 3: eParamValue = PHX_CXP_BITRATE_MODE_CXP3; break;
      case 5: eParamValue = PHX_CXP_BITRATE_MODE_CXP5; break;
      case 6: eParamValue = PHX_CXP_BITRATE_MODE_CXP6; break;
      THROW_UNRECOVERABLE(ActiveSilicon::CASLException::msgComposer("Invalid bitrate requested (%d)", gbps));
   }

   _PHX_THROW_ON_ERROR(PHX_ParameterSet(m_hCamera, static_cast<etParam>(PHX_CXP_BITRATE_MODE | PHX_CACHE_FLUSH), &eParamValue));

   etParamValue eParamBitInfo;
   _PHX_THROW_ON_ERROR(PHX_ParameterGet(m_hCamera, static_cast<etParam>(PHX_CXP_INFO), &eParamBitInfo));
   if (!eParamBitInfo || !PHX_CXP_CAMERA_DISCOVERED) THROW_HW_EXCEPTION("Discovery state unknown");

   etParamValue eParamBitDiscoveryMode, eParamBitDiscovery;
   _PHX_THROW_ON_ERROR(PHX_ParameterGet(m_hCamera, static_cast<etParam>(PHX_CXP_DISCOVERY_MODE), &eParamBitDiscoveryMode));
   if (eParamBitDiscoveryMode != PHX_CXP_DISCOVERY_MODE_AUTO) { // in auto mode, we don't need to check anything
      _PHX_THROW_ON_ERROR(PHX_ParameterGet(m_hCamera, static_cast<etParam>(PHX_CXP_DISCOVERY), &eParamBitDiscovery));
      if (eParamBitDiscoveryMode != eParamBitDiscovery) THROW_HW_EXCEPTION("Couldn't configure PHX_CXP_DISCOVERY_MODE to the value requested");
   }

   etParamValue eParamBitRateMode, eParamBitRate;
   _PHX_THROW_ON_ERROR(PHX_ParameterGet(m_hCamera, static_cast<etParam>(PHX_CXP_BITRATE_MODE), &eParamBitRateMode));
   if (eParamBitRateMode != PHX_CXP_BITRATE_MODE_AUTO) { // in auto mode, we don't need to check anything
      _PHX_THROW_ON_ERROR(PHX_ParameterGet(m_hCamera, static_cast<etParam>(PHX_CXP_BITRATE), &eParamBitRate));
      if (eParamBitRateMode != eParamBitRate) THROW_HW_EXCEPTION("Couldn't configure PHX_CXP_BITRATE_MODE to the value requested");
   }
}

// the BufferProcessingThread waits for semaphores released in PHX_CallbackEvent
// when a semaphore is signaled, a buffer is retrieved from the frame grabber
// the buffer is either memcopied to the display buffer or directly released if the display is not keeping up with the acquisition rate
DWORD WINAPI CCamera::BufferProcessingThread (void *x)
{
   if (!x) {
      MessageBox(0, "Null PHX handle", "Error", MB_OK);
      return 1;
   }

   CCamera * This = reinterpret_cast<CCamera *>(x);
   

   while (!This->m_bufferProcessingThreadStop) {
      try {
         if (!This->m_recordingThreadSignaledAtomic) { // if the reocrding is signaled, the buffers will be retrieved in the DoRecord() function
            try {
               DWORD dwStatus = This->semaphoreWait();

               // Call our bufferCallback function which can be specialized in the derived camera classes
               This->bufferCallback(dwStatus == WAIT_OBJECT_0); // there might be a semaphore error in wich case we shouldn't send a trigger
                                                               // therefore use dwStatus == WAIT_OBJECT_0 instead of dwStatus != WAIT_TIMEOUT

               if (dwStatus == WAIT_TIMEOUT) {
                  continue; // if we get a timeout, we just continue
               } else if (dwStatus != WAIT_OBJECT_0) {
                  THROW_UNRECOVERABLE("Semaphore error"); // if we get an error, we signal it to the user and continue
               }

               This->m_acquisitionBlockedAtomic = 0; // if we get a semaphore, the acquisition is still running and the watchdog thread will not interfere

            } catch (std::exception &e) {
               MessageBox(0, e.what(), "Error in"__FUNCTION__, MB_OK);
               continue;
            }  catch (...) {
               MessageBox(0, "Unknown Exception", "Error in"__FUNCTION__, MB_OK);
               continue;
            }
			uint8_t *dstBuffer = (uint8_t*)malloc(This->m_imageWidth * This->m_imageHeight * sizeof(uint8_t));
            stImageBuff stBuffer;
            _PHX_THROW_ON_ERROR(PHX_StreamRead(This->m_hCamera, PHX_BUFFER_GET, &stBuffer));
            try {
               // see paint() function for comments
               if (This->m_bufferForDisplayAtomic) {
                  if (!This->m_datastream) {
					  if (GratingWindow_Flag && PhaseUnwrapping_Flag)
					  {						  
						  FastExtraction(dstBuffer, reinterpret_cast<uint8_t *>(stBuffer.pvAddress), This->m_imageWidth , This->m_imageHeight);
						  memcpy(This->m_nextDisplayBuffer.data.get(), dstBuffer, This->m_nextDisplayBuffer.bytes);						  
					  }
					  else
					  {
						  memcpy(This->m_nextDisplayBuffer.data.get(), stBuffer.pvAddress, This->m_nextDisplayBuffer.bytes);
					  }
					  //memcpy(This->m_nextDisplayBuffer.data.get(), stBuffer.pvAddress, This->m_nextDisplayBuffer.bytes);

					 //YH added
					 /*
					 char fname[20];
					 sprintf(fname, "D:\\buffer.raw");
					 FILE *fp = fopen(fname, "wb");
					 fwrite(dstBuffer, 1024*1024, sizeof(uint8_t), fp);
					 fclose(fp);
					 */
                  } else {
                     This->unpackData(reinterpret_cast<uint8_t *>(stBuffer.pvAddress), reinterpret_cast<uint16_t *>(This->m_nextDisplayBuffer.data.get()));
                  }
                  This->m_bufferForDisplayAtomic = 0;
               }
            } catch (std::exception &e) {
               MessageBox(0, e.what(), "Error in"__FUNCTION__, MB_OK);
            }  catch (...) {
               MessageBox(0, "Unknown Exception", "Error in"__FUNCTION__, MB_OK);
            }
            _PHX_THROW_ON_ERROR(PHX_StreamRead(This->m_hCamera, PHX_BUFFER_RELEASE, NULL));
			free(dstBuffer);
         }
      } catch (std::exception &e) {
         MessageBox(0, e.what(), "Error in"__FUNCTION__, MB_OK);
      }  catch (...) {
         MessageBox(0, "Unknown Exception", "Error in"__FUNCTION__, MB_OK);
      }
   }
   return 0;
}

/*DWORD WINAPI CCamera::NIdeviceThread (void *x)
{
   if (!x) {
      MessageBox(0, "Null PHX handle", "Error", MB_OK);
      return 1;
   }

   int kkk;

   CCamera * This = reinterpret_cast<CCamera *>(x);
   while (!This->m_bufferProcessingThreadStop) {
	   try {
		  while (This->m_recordAtomic) {
			  kkk = recordingBuffersSoFar*3;
			  SetToVoltage(ChScanners, AO[(int)kkk], AO[(int)kkk + 1], AO[(int)kkk + 2]);
		  }
	   } catch (std::exception &e) {
		  MessageBox(0, e.what(), "Error", MB_OK);
		  return 1;
	   } catch (...) {
		  MessageBox(0, "Unknown Exception in "__FUNCTION__, "Error", MB_OK);
		  return 1;   
   }
   }
   return 0;
}*/

DWORD WINAPI CCamera::MoveStageThread(void *x)
{
	if (!x) {
		MessageBox(0, "Null PHX handle", "Error", MB_OK);
		return 1;
	}
	
	CCamera * This = reinterpret_cast<CCamera *>(x);
	while (!This->m_movestageThreadStop) {
		try {
			while (This->m_recordAtomic) {
				if (ScanMode == 3)
				if (recordingBuffersSoFar < imgFrame)
				{
					printf("\nsofat:%d, interval:%d", recordingBuffersSoFar, intervalWD);
					moveSHOT(PortStageWindow, port_stage_window, "2", intervalWD);
				}				
			}
		}
		catch (std::exception &e) {
			MessageBox(0, e.what(), "Error", MB_OK);
			return 1;
		}
		catch (...) {
			MessageBox(0, "Unknown Exception in "__FUNCTION__, "Error", MB_OK);
			return 1;
		}
	}
	return 0;
}

// the AcquisitionFrameRateThread calculates the frame grabber acquisition rate by counting the number of callbacks in 2 sec time intervals
DWORD WINAPI CCamera::AcquisitionFramerateThread (void *x)
{
   if (!x) {
      MessageBox(0, "Null PHX handle", "Error", MB_OK);
      return 1;
   }

   CCamera *This = reinterpret_cast<CCamera *>(x);
   CStopwatch stopwatch;
   uint32_t lastCount = 0;

   stopwatch.Start();
   try {
      while (!This->m_acquisitionFramerateThreadStop) {
         Sleep(2000);
         LONG count = InterlockedCompareExchange(&This->m_acquisitionStopwatchCount, 0, 0);
         stopwatch.Stop();
         float ms = (float)stopwatch.ElapsedTimeMilliSec();
         stopwatch.Start();
         uint32_t delta = count - lastCount; // should auto-wrap
         float fps = ms ? (1000 * delta) / ms : 0;
         lastCount = count;
         InterlockedExchange(reinterpret_cast<volatile LONG *>(&This->m_acquisitionFPS), *reinterpret_cast<LONG *>(&fps));
      }
   } catch (std::exception &e) {
      MessageBox(0, e.what(), "Error", MB_OK);
      return 1;
   } catch (...) {
      MessageBox(0, "Unknown Exception in "__FUNCTION__, "Error", MB_OK);
      return 1;   
   }
   return 0;
}

// if enabled, the WatchdogThread will try to recover the acquisition if it gets blocked (if no semaphores are released during m_watchdogTimer ms)
DWORD WINAPI CCamera::WatchdogThread (void *x)
{
   if (!x) {
      MessageBox(0, "Null PHX handle", "Error", MB_OK);
      return 1;
   }

   CCamera *This = reinterpret_cast<CCamera *>(x);

   if (!This->m_watchdogTimer) return 0;

   try {
      int recovery = 0;
      while (!This->m_watchdogThreadStop) {
         try {
            if (!This->m_recordAtomic && This->m_isFrameGrabberRunningAtomic && This->m_isCameraRunningAtomic) {
               // disable watchdog while recording or while restarting acquisition or while camera is stopped
               This->m_acquisitionBlockedAtomic = 1;
               Sleep(This->m_watchdogTimer);
               if (This->m_acquisitionBlockedAtomic) {
                  if (recovery == 1) MessageBox(0, "Recovery Failed.\nWatchdog will keep attempting recovery until the user quits the application.", "Watchdog", MB_OK);
                  This->m_isRecoveringAtomic = 1;
                  This->RecoverAcquisition();
                  This->m_isRecoveringAtomic = 0;
                  recovery++;
               } else recovery = 0;
            }
         } catch (std::exception &e) {
            MessageBox(0, e.what(), "Error", MB_OK);
         } catch (...) {
            MessageBox(0, "Unknown Exception in "__FUNCTION__, "Error", MB_OK);
         }
      }
   } catch (std::exception &e) {
      MessageBox(0, e.what(), "Error", MB_OK);
      return 1;
   } catch (...) {
      MessageBox(0, "Unknown Exception in "__FUNCTION__, "Error", MB_OK);
      return 1;
   }
   return 0;
}
double   duration, t_start, t_finish;
clock_t start, finish;
void __cdecl CCamera::PHX_CallbackEvent (tHandle hCamera, uint32_t dwInterruptMask, void *pvParams)
{
   (void) hCamera;

   CCamera * This = reinterpret_cast<CCamera *>(pvParams);

	if( This->m_FrameStartEventCount == 30 )
	{
		UINT i = 10;
		i = i * 10;
	}
	

   if (!This) {
      MessageBox(0, "Null handle.", "Error in"__FUNCTION__, MB_OK);
      return;
   }
   if (!This->m_useSynchronizedAcquisition) {
      if (hCamera != This->m_hCamera) {
         MessageBox(0, "Unknown handle.", "Error in"__FUNCTION__, MB_OK);
         return;
      }
   } else {
      if (hCamera != This->m_hCamera && hCamera != This->m_hCamera2) {
         MessageBox(0, "Unknown handle.", "Error in"__FUNCTION__, MB_OK);
         return;
      }
   }
   if (!This->m_bufferReadySemaphore) {
      MessageBox(0, "Null semaphore handle, a callback event will be lost.", "Error in"__FUNCTION__, MB_OK);
      return;
   }

   if (PHX_INTRPT_BUFFER_READY & dwInterruptMask) {
      CLock lock(This->m_callbackCS); // necessary for a synchronized acquisition because there are two callback threads

      if (This->m_useSynchronizedAcquisition) {
         if (hCamera == This->m_hCamera)  This->m_cameraCallbackCount++;
         else                             This->m_camera2CallbackCount++;
      }

      if (!This->m_useSynchronizedAcquisition || (This->m_cameraCallbackCount && This->m_camera2CallbackCount)) {

         if (This->m_useSynchronizedAcquisition) {
            This->m_camera2CallbackCount--;
            This->m_cameraCallbackCount--;
         }

         // a callbackCount is used for the recording: when possible, m_buffersBeforeTrigger buffers are recorded (before the recording was triggered)
         LONG callbackCount = InterlockedCompareExchange(reinterpret_cast<volatile LONG *>(&This->m_callbackCount), 0, 0);
         if (callbackCount++ <= (LONG)This->m_buffersBeforeTrigger) { // we won't need to count to more than m_buffersBeforeTrigger
            InterlockedIncrement(reinterpret_cast<volatile LONG *>(&This->m_callbackCount));
         }

         // count used in bufferCallback() function
         InterlockedIncrement(reinterpret_cast<volatile LONG *>(&This->m_BufferReceivedCount));
		 
         // acquisition frame rate
         InterlockedIncrement(&This->m_acquisitionStopwatchCount);

         int res = ReleaseSemaphore(This->m_bufferReadySemaphore, 1, NULL);
         if (!res) {
            if (This->m_blocking == PHX_ENABLE) {
               DWORD error = GetLastError();
               LPVOID lpMsgBuf = 0;
               DWORD ret = FormatMessage(
                                          FORMAT_MESSAGE_ALLOCATE_BUFFER |
                                          FORMAT_MESSAGE_FROM_SYSTEM |
                                          FORMAT_MESSAGE_IGNORE_INSERTS,
                                          NULL,
                                          error,
                                          MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                          (LPTSTR) &lpMsgBuf,
                                          0, NULL);

               std::string s;
               if (lpMsgBuf) {
                  s = static_cast<char *>(lpMsgBuf);
                  LocalFree(lpMsgBuf);
               }
               MessageBox(0, ActiveSilicon::CASLException::msgComposer("Error while releasing semaphore: %s(%08X)", s.c_str(), error).c_str(), "Error in"__FUNCTION__, MB_OK);
            } else {
            }
            return;
         } else {
            // Deal with recording. Do the following in the callback rather than the buffer processing thread to avoid delay
            // due to signaling between the callback and the buffer processing thread.
            if (This->m_recordAtomic) 
			{ // recording has been triggered
               if (callbackCount <= (LONG)(This->m_buffersBeforeTrigger )) 
			   { // we can only start recording if we have reached the number of buffers before trigger
               } 
			   else 
			   { // we have m_buffersBeforeTrigger buffers in the queue
                  if (!This->m_recordingThreadSignaledAtomic) 
				  { // we are not already in the process of recording
                     // after the record has been triggered, we need to acquire m_buffersAfterTrigger more buffers to be sure to have the right sequence of buffers
                     recordingBuffersSoFar = InterlockedCompareExchange(reinterpret_cast<volatile LONG *>(&This->m_recordingBuffersSoFar), 0, 0);
					 
                     if (recordingBuffersSoFar + 1 < LONG(This->m_buffersAfterTrigger)) 
					 {
						 int kkk = recordingBuffersSoFar * 3;
						 //MoveRead_SHOT(PortStageWindow, port_stage_window, "2", -1, PosSX, PosWD);
						 SetToVoltage(ChScanners, AO[kkk], AO[kkk + 1], AO[kkk + 2]);
						 //if(recordingBuffersSoFar ==0)	start = clock();						
						 InterlockedIncrement(reinterpret_cast<volatile LONG *>(&This->m_recordingBuffersSoFar));					 
						 //printf("Num:%2d; %2.3f\n",(int)recordingBuffersSoFar,AO[(int)recordingBuffersSoFar*3]); 
						/*if(recordingBuffersSoFar+2 ==LONG(This->m_buffersAfterTrigger))
						{
							finish = clock();
							//t_start  = (double)(start ) / CLOCKS_PER_SEC;   
							//t_finish = (double)(finish) / CLOCKS_PER_SEC;  
							duration = (double)(finish - start) / CLOCKS_PER_SEC;  
							printf( "\nDuration: %3.3f seconds\n", duration );							
						}	*/					
                     }
					 else
					 { // we now have all required buffers and we can signal the recording thread						
                        if (This->m_eventFromRecording)
						{
                           // we signal recording: the recordThread will save the sequence to file(s) and the display will be paused
                           This->m_recordingThreadSignaledAtomic = 1;
                           SetEvent(This->m_eventFromRecording);
                        }
                        This->stopFrameGrabber(); // frame grabber will be restarted when sequence has been recorded
						SetToVoltage(ChScanners, 0.f, 0.f, 0.f);
                     }
                  }
               }
            }
         }
      }
   } else if (PHX_INTRPT_FRAME_START & dwInterruptMask) {
      This->frameStartCallback();
      This->m_FrameStartEventCount++;
   } else {
      MessageBox(0, ActiveSilicon::CASLException::msgComposer("Unexpected callback event 0x%08X", dwInterruptMask).c_str(), "Error in"__FUNCTION__, MB_OK);
   }
}

void CCamera::PhoenixWriteWord(uint32_t dwAddress, uint32_t dwData, etControlPort port)
{
   uint32_t dwBytes = 4;
   uint32_t dwValue = dwData;   
   if (port == PHX_REGISTER_DEVICE) dwValue = htonl(dwValue); // CXP makes the camera look big endian
   if (port == PHX_REGISTER_HOST)   dwValue = htonl(dwValue);
   Sleep(10);
   _PHX_THROW_MSG_ON_ERROR(PHX_ControlWrite(m_hCamera, port, (ui32 *)&dwAddress, (ui8 *)&dwValue, (ui32 *)&dwBytes, m_timeoutPHX), 
                           ActiveSilicon::CASLException::msgComposer("PHX_ControlWrite (%s, 0x%08X, 0x%08X, %d) failed.", port == PHX_REGISTER_DEVICE?"camera":"host", dwAddress, dwData, dwBytes).c_str());
}

void CCamera::hostWriteWord(uint32_t dwAddress, uint32_t dwData)
{
   PhoenixWriteWord(dwAddress, dwData, PHX_REGISTER_HOST);
}

void CCamera::deviceWrite(uint32_t dwAddress, int32_t dwData)
{
   PhoenixWriteWord(dwAddress, (uint32_t)dwData, PHX_REGISTER_DEVICE);
}

void CCamera::PhoenixReadWord(uint32_t dwAddress, int32_t &dwData, etControlPort port)
{
   int32_t dwTmp;
   uint32_t dwBytes = 4;
   _PHX_THROW_MSG_ON_ERROR(PHX_ControlRead(m_hCamera, port, (ui32 *)&dwAddress, reinterpret_cast<ui8 *>(&dwTmp), (ui32 *)&dwBytes, m_timeoutPHX),
                           ActiveSilicon::CASLException::msgComposer("PHX_ControlRead (%s, 0x%08X, %d) failed.", port == PHX_REGISTER_DEVICE?"camera":"host", dwAddress, dwBytes).c_str());

   dwData = dwTmp; // only update if success
   if (port == PHX_REGISTER_DEVICE) dwData = htonl(dwData); // CXP makes the camera look big endian
   if (port == PHX_REGISTER_HOST) dwData = htonl(dwData);
}

void CCamera::hostReadWord(uint32_t dwAddress, int32_t &dwData)
{
   PhoenixReadWord(dwAddress, dwData, PHX_REGISTER_HOST);
}

void CCamera::deviceRead(uint32_t dwAddress, int32_t &dwData)
{
   PhoenixReadWord(dwAddress, dwData, PHX_REGISTER_DEVICE);
}

float CCamera::deviceReadfloat(uint32_t dwAddress)
{
	etControlPort port = PHX_REGISTER_DEVICE;
	int32_t dwTmp, dwData;
	uint32_t dwBytes = 4;
	_PHX_THROW_MSG_ON_ERROR(PHX_ControlRead(m_hCamera, port, (ui32 *)&dwAddress, reinterpret_cast<ui8 *>(&dwTmp), (ui32 *)&dwBytes, m_timeoutPHX),
                           ActiveSilicon::CASLException::msgComposer("PHX_ControlRead (%s, 0x%08X, %d) failed.", port == PHX_REGISTER_DEVICE?"camera":"host", dwAddress, dwBytes).c_str());

	dwData = dwTmp;			// only update if success
	dwData = htonl(dwData);	// CXP makes the camera look big endian
	float trueValue = 0.0;
	*(int32_t*)&trueValue = dwData;

	return trueValue;
}

void CCamera::deviceRead(uint32_t dwAddress, size_t size, std::string &strData)
{
   std::vector<char> s;
   s.resize(size);
   uint32_t dwBytes = (uint32_t) s.size();
   _PHX_THROW_MSG_ON_ERROR(PHX_ControlRead(m_hCamera, PHX_REGISTER_DEVICE, (ui32 *)&dwAddress, reinterpret_cast<ui8 *>(&s[0]), (ui32 *)&dwBytes, m_timeoutPHX),
                           ActiveSilicon::CASLException::msgComposer("PHX_ControlRead (%s, 0x%08X, %d) failed.", "camera", dwAddress, dwBytes).c_str());
   s[s.size() - 1] = '\0';
   strData = std::string(&s[0]);
}

void CCamera::deviceWriteAndReadBack(uint32_t dwAddress, int32_t dwData)
{
   // Write value to device
   deviceWrite(dwAddress, dwData);

   // Read back and check that value is correct
   int32_t data;
   deviceRead(dwAddress, data);
   if (data != dwData) THROW_PHX_EXCEPTION(ActiveSilicon::CASLException::msgComposer("at address 0x%X\nData written: %d\nData read back: %d", dwAddress, dwData, data));
}

void CCamera::hostSetParameter(etParam parameter, uint32_t value)
{
   _PHX_THROW_ON_ERROR(PHX_ParameterSet(m_hCamera, parameter, &value));
}

void CCamera::hostGetParameter(etParam parameter, uint32_t & value)
{
   _PHX_THROW_ON_ERROR(PHX_ParameterGet(m_hCamera, parameter, &value));
}

// When possible, we set the camera to send buffers of the size specified by the frame grabber (width and height values from the pcf file)
// We have to make sure that the new ROI sizes are valid for the camera (min, max and increment values) and we also want to center the new ROI

// This function checks that the frame grabber ROI can be applied to the camera
void CCamera::calculateCameraROI(int32_t x_min, int32_t y_min, int32_t x_max, int32_t y_max, int32_t x_inc, int32_t y_inc, int32_t &x, int32_t &y)
{
   if (x_min < 0 || y_min < 0 || x_max <= x_min || y_max <= y_min) THROW_UNRECOVERABLE("Could not calculate camera ROI. Min and max values are invalid.");
   if (x_inc < 0 || y_inc < 0) THROW_UNRECOVERABLE("Could not calculate camera ROI. Increment values are invalid.");

   // get the frame grabber ROI
   uint32_t x_roi, y_roi;
   hostGetParameter(PHX_ROI_XLENGTH, x_roi);
   hostGetParameter(PHX_ROI_YLENGTH, y_roi);

   // make sure the width (x) is valid for the camera
   if ((int32_t)x_roi > x_max) THROW_UNRECOVERABLE(ActiveSilicon::CASLException::msgComposer("the width given in the pcf file (%d) is greater than the camera maximum width (%d).", x_roi, x_max));
   if ((int32_t)x_roi < x_min) THROW_UNRECOVERABLE(ActiveSilicon::CASLException::msgComposer("the width given in the pcf file (%d) is smaller than the camera minimum width (%d).", x_roi, x_min));

   int32_t r = (x_roi-x_min)%x_inc; // check that the new value is a multiple of x_inc
   if (r == 0) x = x_roi;
   else {
      // the new value is not valid, we throw an exception and calculate and provide the nearest valid value
      if (r >= x_inc/2) x = x_roi - r + x_inc;
      else  x = x_roi - r;
      THROW_UNRECOVERABLE(ActiveSilicon::CASLException::msgComposer("the width given in the pcf file (%d) has to be a multiple of the camera increment value (%d).\nThe nearest value is: %d.", x_roi, x_inc, x));
   }

   // make sure the height (y) is valid for the camera
   if ((int32_t)y_roi > y_max) THROW_UNRECOVERABLE(ActiveSilicon::CASLException::msgComposer("the height given in the pcf file (%d) is greater than the camera maximum height (%d).", y_roi, y_max));
   if ((int32_t)y_roi < y_min) THROW_UNRECOVERABLE(ActiveSilicon::CASLException::msgComposer("the height given in the pcf file (%d) is smaller than the camera minimum height (%d).", y_roi, y_min));

   r = (y_roi-y_min)%y_inc; // check that the new value is a multiple of y_inc
   if (r == 0) y = y_roi;
   else {
      // the new value is not valid, we throw an exception and calculate and provide the nearest valid value
      if (r >= y_inc/2) y = y_roi - r + y_inc;
      else  y = y_roi - r;
      THROW_UNRECOVERABLE(ActiveSilicon::CASLException::msgComposer("the height given in the pcf file (%d) has to be a multiple of the camera increment value (%d).\nThe nearest value is: %d.", y_roi, y_inc, y));
   }
}

// This function calculates an offset to center the camera ROI
void CCamera::calculateCameraOffset(int32_t x, int32_t y, int32_t x_max, int32_t y_max, int32_t x_offset_inc, int32_t y_offset_inc, int32_t &x_offset, int32_t &y_offset)
{
   if (x > x_max || y > y_max || x_offset_inc < 0 || y_offset_inc < 0) THROW_UNRECOVERABLE("Could not calculate camera offset. Invalid values.");

   // the max offset values are determined with the current and max width and height values
   int32_t x_offset_max = x_max - x;
   int32_t y_offset_max = y_max - y;

   // width offset
   // the new offset is the max offset divided by 2 (we center the ROI)
   int32_t r_x_offset = (x_offset_max/2)%x_offset_inc; // make sure that the new offset value is a multiple of the offset increment
   if (r_x_offset == 0) x_offset = (x_offset_max/2);
   else {
      // if the new offset value is not a multiple of the offset increment, we set it to the nearest valid value
      if (r_x_offset >= x_offset_inc/2) x_offset = (x_offset_max/2) - r_x_offset + x_offset_inc;
      else x_offset = (x_offset_max/2) - r_x_offset;
   }

   // height offset
   // the new offset is the max offset divided by 2 (we center the ROI)
   int32_t r_y_offset = (y_offset_max/2)%y_offset_inc; // make sure that the new offset value is a multiple of the offset increment
   if (r_y_offset == 0) y_offset = (y_offset_max/2);
   else {
      // if the new offset value is not a multiple of the offset increment, we set it to the nearest valid value
      if (r_y_offset >= y_offset_inc/2) y_offset = (y_offset_max/2) - r_y_offset + y_offset_inc;
      else y_offset = (y_offset_max/2) - r_y_offset;
   }
}


void CCamera::FanOpertaor(uint32_t dwData)
{
	const uint32_t FANOPERATIONREG			= 0x10090004;  // fan mode
	const uint32_t FanOpTempReg				= 0x10090010;  // FanOperationTemperature
	const uint32_t FanOpTempMinReg			= 0x10090014;  // FanOperationTemperature
	const uint32_t FanOpTempMaxReg			= 0x10090018;  // FanOperationTemperature
	const uint32_t FanOpTempIncReg			= 0x1009001C;  // FanOperationTemperature
	etControlPort port						= PHX_REGISTER_DEVICE;
 
	if(dwData == 2)
	{
		int32_t FanOpTemp = 0;
		deviceRead(FanOpTempReg   , FanOpTemp);
		if(FanOpTemp != (int32_t)50)	deviceWrite(FanOpTempReg  , 50);
	}

	uint32_t dwValue = dwData; 
	deviceWrite(FANOPERATIONREG,   dwData);
	
	float CameraTemp = MonitorTemp();
	printf("Current temperature: %2.3f\n", CameraTemp);
	
	/*deviceRead(FanOpTempMinReg   ,   fan_tempature);
	printf("\nTemptureMin: %d",fan_tempature);
	deviceRead(FanOpTempMaxReg   ,   fan_tempature);
	printf("\nTemptureMax: %d",fan_tempature);
	deviceRead(FanOpTempIncReg   ,   fan_tempature);
	printf("\nTemptureInc: %d",fan_tempature);*/	
}

float CCamera::MonitorTemp()
{
	//monitor the temperature of camera
	const uint32_t DeviceTemperatureReg = 0x10000844;
	float Temperature = deviceReadfloat(DeviceTemperatureReg);
	return Temperature;
}


void CCamera::TestPattern(int flag)
{	
	int32_t dwData = flag;
	const uint32_t PatternValueReg = 0x10010404;
	deviceWrite(PatternValueReg , dwData);
}

void CCamera::PrintExposureData()
{
	etControlPort port = PHX_REGISTER_DEVICE;
	// trigger
	const uint32_t TriggerModeReg	= 0x10020044;
	const uint32_t TriggerCtrlReg	= 0x100200C4;
	const uint32_t TriggerSrcReg	= 0x10020084;
	
	// frame rate
	const uint32_t FrameRateReg		= 0x100A0020;
	const uint32_t FrameRateMinReg	= 0x100A0024;
	const uint32_t FrameRateMaxReg	= 0x100A0028;

	// exposure data
	const uint32_t ExpCtrlRegExpSrc	= 0x10030004;
	const uint32_t ExpTimeRawReg	= 0x10030044;
	const uint32_t ExpTimeRawMinReg = 0x10030048;
	const uint32_t ExpTimeRawMaxReg = 0x1003004C;
	const uint32_t ExpTimeRawIncReg = 0x10030050;
	
	//trigger mode
	int32_t TriggerMode = 0;	deviceRead(TriggerModeReg   , TriggerMode);
	int32_t TriggerCtrl = 0;	deviceRead(TriggerCtrlReg   , TriggerCtrl);
	int32_t TriggerSrc  = 0;	deviceRead(TriggerSrcReg    , TriggerSrc );
	printf("Trigger Mode: %2d\n"
		   "Trigger Ctrl: %2d\n"
		   "Trigger Src : %2d\n"
		   , TriggerMode, TriggerCtrl, TriggerSrc);

	//read date
	float FrameRate	   = deviceReadfloat(FrameRateReg);
	float FrameRateMin = deviceReadfloat(FrameRateMinReg);
	float FrameRateMax = deviceReadfloat(FrameRateMaxReg);
	deviceWrite(ExpCtrlRegExpSrc   , 0);

	int32_t ExpMode = 0;	deviceRead(ExpCtrlRegExpSrc   , ExpMode);
	//deviceWrite(ExpTimeRawReg,500);
	float ExpTime	 = deviceReadfloat(ExpTimeRawReg);
	float ExpTimeMin = deviceReadfloat(ExpTimeRawMinReg);
	float ExpTimeMax = deviceReadfloat(ExpTimeRawMaxReg);
	float ExpTimeInc = deviceReadfloat(ExpTimeRawIncReg);

	printf("FrameRate      : %3.2f\n"		  
		   "FrameRateMin   : %3.2f\n"
		   "FrameRateMax   : %3.2f\n"
		   "\nExposureMode   : %d\n"
		   "ExposureTime   : %3.2f\n"
		   "ExposureTimeMin: %3.2f\n"
		   "ExposureTimeMax: %3.2f\n"
		   "ExposureTimeInc: %3.2f\n"
		   ,FrameRate,FrameRateMin,FrameRateMax
		   ,ExpMode,ExpTime,ExpTimeMin,ExpTimeMax,ExpTimeInc);

	/*uint32_t period, width;
	hostGetParameter(PHX_TIMERA1_PERIOD,      period);
	hostGetParameter(PHX_TIMERM1_WIDTH,      width);
	printf("%d\n",period);
	printf("%d\n",width);*/

}

// recording
void CCamera::DoRecord()
{
   int hTMGImageTemp = 0; // TMG image containing the image to be stored to AVI
   int hTMGImageProcessing = 0;
   int hTMGImageFromCamera = 0;
   int hTMGImageFromCameraCropped = 0;
   int hDVR;
   uint32_t recordWidth  = m_recordWidth  ? m_recordWidth  : m_imageWidth;
   uint32_t recordHeight = m_recordHeight ? m_recordHeight : m_imageHeight;

   size_t nbFrames = m_recordBufferCount;

   char filename[128];
   filename[0] = 0;

   // set the filename (with or without prefix)
   if (!m_recordNoPrefixAtomic || recordAvi()) {
      SYSTEMTIME time;
      GetLocalTime(&time);
      if (!m_strFilePrefix.empty() && !m_recordNoPrefixAtomic) {
         sprintf_s(filename, "%s%4d_%02d_%02d_%02d_%02d_%02d", m_strFilePrefix.c_str(), time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);
      } else {
         sprintf_s(filename, "%4d_%02d_%02d_%02d_%02d_%02d", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);
      }
   }

   _TMG_THROW_ON_ERROR(TMG_Image_Create(&hTMGImageProcessing, 0, NULL));
   _TMG_THROW_ON_ERROR(TMG_Image_Create(&hTMGImageTemp, 0, NULL));
   _TMG_THROW_ON_ERROR(TMG_Image_Create(&hTMGImageFromCamera, 0, NULL));
   _TMG_THROW_ON_ERROR(TMG_Image_Create(&hTMGImageFromCameraCropped, 0, NULL));

   _TMG_THROW_ON_ERROR(TMG_Stream_Library_Open());
   _TMG_THROW_ON_ERROR(TMG_Stream_Create(&hDVR));

   try {
      _TMG_THROW_ON_ERROR(TMG_Image_ParameterSet(hTMGImageFromCamera, TMG_IMAGE_WIDTH,  m_imageWidth));
      _TMG_THROW_ON_ERROR(TMG_Image_ParameterSet(hTMGImageFromCamera, TMG_IMAGE_HEIGHT, m_imageHeight));

      int TMGPixelFormat = TMG_Y8;
      switch(m_grabberPixelformat)
      {
         case CXP_PIXEL_FORMAT_MONO8:
         case CXP_PIXEL_FORMAT_BAYER_GR8:
         case CXP_PIXEL_FORMAT_BAYER_RG8:
         case CXP_PIXEL_FORMAT_BAYER_GB8:
         case CXP_PIXEL_FORMAT_BAYER_BG8:    TMGPixelFormat = TMG_Y8;      break;
         case CXP_PIXEL_FORMAT_MONO10:
         case CXP_PIXEL_FORMAT_MONO12:
         case CXP_PIXEL_FORMAT_BAYER_GR10:
         case CXP_PIXEL_FORMAT_BAYER_RG10:
         case CXP_PIXEL_FORMAT_BAYER_GB10:
         case CXP_PIXEL_FORMAT_BAYER_BG10:
         case CXP_PIXEL_FORMAT_BAYER_GR12:
         case CXP_PIXEL_FORMAT_BAYER_RG12:
         case CXP_PIXEL_FORMAT_BAYER_GB12:
         case CXP_PIXEL_FORMAT_BAYER_BG12:   TMGPixelFormat = TMG_Y16;     break;
         case CXP_PIXEL_FORMAT_YUV422:       TMGPixelFormat = TMG_YUV422;  break;
         case CXP_PIXEL_FORMAT_RGB24:        TMGPixelFormat = TMG_RGB24;   break;
         default: THROW_UNRECOVERABLE("Unsupported pixel format.");
      }

	   _TMG_THROW_ON_ERROR(TMG_Image_ParameterSet(hTMGImageFromCamera, TMG_PIXEL_FORMAT, TMGPixelFormat));
      if (recordAvi()) {

         std::string aviFile = m_strOutputDir + std::string("\\") + std::string(filename) + std::string(".avi");
         _TMG_THROW_ON_ERROR(TMG_Stream_FilenameSet(hDVR, TMG_FILENAME_OUT, const_cast<char *>(aviFile.c_str())));
#if defined _APP_CROPPED
         _TMG_THROW_ON_ERROR(TMG_Stream_ParameterSet(hDVR, TMG_IMAGE_WIDTH,  1920));
         _TMG_THROW_ON_ERROR(TMG_Stream_ParameterSet(hDVR, TMG_IMAGE_HEIGHT, 1024));
#else
         _TMG_THROW_ON_ERROR(TMG_Stream_ParameterSet(hDVR, TMG_IMAGE_WIDTH,  recordWidth));
         _TMG_THROW_ON_ERROR(TMG_Stream_ParameterSet(hDVR, TMG_IMAGE_HEIGHT, recordHeight));
#endif
         _TMG_THROW_ON_ERROR(TMG_Stream_ParameterSet(hDVR, TMG_DVR_FRAME_RATE,  m_dvrFrameRate));
         _TMG_THROW_ON_ERROR(TMG_Stream_ParameterSet(hDVR, TMG_DVR_FORMAT,      TMG_AVI_MJPEG));
         _TMG_THROW_ON_ERROR(TMG_Stream_ParameterSet(hDVR, TMG_DVR_DESTINATION, TMG_FILE));
         _TMG_THROW_ON_ERROR(TMG_Stream_ParameterSet(hDVR, TMG_JPEG_Q_FACTOR,   m_jpegQFactor));
         _TMG_THROW_ON_ERROR(TMG_Stream_OpenForOutput(hDVR));
      }

      stImageBuff stBuffer;
      _TMG_THROW_ON_ERROR(PHX_StreamRead(m_hCamera, PHX_BUFFER_GET,    &stBuffer));
      _TMG_THROW_ON_ERROR(PHX_StreamRead(m_hCamera, PHX_BUFFER_RELEASE, NULL));
      // Don't use first buffer in case its content is being overwritten while being saved

      // We need a buffer in which we can unpack the data if in datastream mode
      std::tr1::shared_ptr<void> unpackedBuffer(new char [m_widthBytes * m_heightBytes]);

	  //char fname[20];
      for (int nFrameNum = 1; nFrameNum <= nbFrames; nFrameNum++) {
         // Update image with pointer to image data
         _TMG_THROW_ON_ERROR(PHX_StreamRead(m_hCamera, PHX_BUFFER_GET, &stBuffer));
         void * buffer = stBuffer.pvAddress;

		/*sprintf(fname, "D:\\buffer_%3d.raw", nFrameNum);
		FILE *fp = fopen(fname, "wb");
		fwrite(reinterpret_cast<uint8_t *>(buffer), m_imageWidth*m_imageHeight, sizeof(uint8_t), fp);
		fclose(fp);*/

         if (m_datastream) {
            unpackData(reinterpret_cast<uint8_t *>(buffer), reinterpret_cast<uint16_t *>(unpackedBuffer.get()));
            buffer = unpackedBuffer.get();
         }
		 
         // Update image with pointer to image data
         _TMG_THROW_ON_ERROR(TMG_Image_ParameterPtrSet(hTMGImageFromCamera, TMG_IMAGE_BUFFER, buffer));

         // Lock the buffer memory to prevent any re-allocation (for speed).
         _TMG_THROW_ON_ERROR(TMG_image_set_flags(hTMGImageFromCamera, TMG_LOCKED, TRUE));

         // Check the TMG image is valid
         _TMG_THROW_ON_ERROR(TMG_image_check(hTMGImageFromCamera));

         // Debayer the image if needed
         int TMGImageToRecord = hTMGImageFromCamera;
         if (BayerFilter(hTMGImageFromCamera, hTMGImageProcessing, m_bayerColorCorrection)) {
            TMGImageToRecord = hTMGImageProcessing;
            _TMG_THROW_ON_ERROR(TMG_Image_ParameterSet(hTMGImageFromCamera, TMG_PIXEL_FORMAT, TMGPixelFormat)); // the BayerFilter function changes the pixel format to RGGB32 and we want to preserve the format of the image
         }

         // Crop the image if different record sizes have been specified
         int offsetX = (m_imageWidth  - recordWidth)  / 2;
         if (offsetX < 0) offsetX = 0;
         int offsetY = (m_imageHeight - recordHeight) / 2;
         if (offsetY < 0) offsetY = 0;

         ui32 naRoi [] = {offsetX, offsetY, recordWidth, recordHeight};
         _TMG_THROW_ON_ERROR(TMG_IP_Crop_Image(TMGImageToRecord, hTMGImageFromCameraCropped, naRoi));

         TMGImageToRecord = hTMGImageFromCameraCropped;

         if (recordAvi()) {
            char szString[128];
            sprintf_s(szString, sizeof(szString), " Frame: %04d  ", nFrameNum);   /* Needs to be even number of characters! */         
            _TMG_THROW_ON_ERROR(TMG_IP_Image_Convert(TMGImageToRecord, hTMGImageTemp, TMG_YUV422, 0));
            _TMG_THROW_ON_ERROR(TMG_Draw_Text(hTMGImageTemp, szString, TMG_FONT_STRUCT_15x19, 8, 40, TMG_DRAW_BG_TRANSPARENT_0, TMG_LIGHT_WHITE));
            _TMG_THROW_ON_ERROR(TMG_Stream_FileWrite(hDVR, hTMGImageTemp));
         }

         char file[4096];
         /*if (!m_recordNoPrefixAtomic) {
            sprintf_s(file, sizeof(file), "%s\\%s_%04d", m_strOutputDir.c_str(), filename, nFrameNum);
         } else {
            sprintf_s(file, sizeof(file), "%s\\%04d",    m_strOutputDir.c_str(), nFrameNum);
         }*/
		 if (!m_recordNoPrefixAtomic) {
            sprintf_s(file, sizeof(file), "%s\\Buffer%d", FolderPath.c_str(), nFrameNum);
         } else {
            sprintf_s(file, sizeof(file), "%s\\%04d",    FolderPath.c_str(), nFrameNum);
         }


         if (recordTiff()) {
            DoSaveToTiff(TMGImageToRecord, std::string(file), false);
         }

         if (recordBmp()) {
            DoSaveToBmp(TMGImageToRecord, std::string(file));
         }

         if (recordRaw()) {
            DoSaveToRaw(TMGImageToRecord, std::string(file));
         }

         m_recordCurrentFrameIndex = nFrameNum;

         _TMG_THROW_ON_ERROR(PHX_StreamRead(m_hCamera, PHX_BUFFER_RELEASE, NULL));
      }  
	  

      _TMG_THROW_ON_ERROR(TMG_Stream_Close(hDVR));
      _TMG_THROW_ON_ERROR(TMG_Image_ParameterPtrSet(hTMGImageFromCamera, TMG_IMAGE_BUFFER, 0)); // make sure TMG won't free our memory when destroying handle.

   } catch (...) {
      TMG_image_destroy(hTMGImageProcessing);
      TMG_image_destroy(hTMGImageTemp);
      TMG_image_destroy(hTMGImageFromCamera);
      TMG_image_destroy(hTMGImageFromCameraCropped);
      if (recordAvi()) {
         TMG_Stream_Close(hDVR);
         TMG_Stream_Destroy(hDVR);
         TMG_Stream_Library_Close();
      }
      throw;
   }
   _TMG_THROW_ON_ERROR(TMG_image_destroy(hTMGImageProcessing));
   _TMG_THROW_ON_ERROR(TMG_image_destroy(hTMGImageTemp));
   _TMG_THROW_ON_ERROR(TMG_image_destroy(hTMGImageFromCamera));
   _TMG_THROW_ON_ERROR(TMG_image_destroy(hTMGImageFromCameraCropped));
   _TMG_THROW_ON_ERROR(TMG_Stream_Destroy(hDVR));
   if (recordAvi()) {
      _TMG_THROW_ON_ERROR(TMG_Stream_Library_Close());
   }
}

bool CCamera::isDatastream()
{
   bool result = false;

   int cameraType, captureFormat, cameraSrcDepth;
   _PHX_THROW_ON_ERROR(PHX_ParameterGet(m_hCamera, PHX_CAM_TYPE,        &cameraType));
   _PHX_THROW_ON_ERROR(PHX_ParameterGet(m_hCamera, PHX_CAPTURE_FORMAT,  &captureFormat));
   _PHX_THROW_ON_ERROR(PHX_ParameterGet(m_hCamera, PHX_CAM_SRC_DEPTH,   &cameraSrcDepth));

   if (cameraType == PHX_CAM_AREASCAN_NO_ROI || cameraType == PHX_CAM_LINESCAN_NO_ROI) {
      if (cameraSrcDepth == 10 || cameraSrcDepth == 12) {
         if (captureFormat == PHX_DST_FORMAT_Y8 || captureFormat == PHX_DST_FORMAT_BAY8 || captureFormat == PHX_DST_FORMAT_RGB24 || captureFormat == PHX_DST_FORMAT_BGR24) {
            result = true;
         } else THROW_NOT_SUPPORTED(ActiveSilicon::CASLException::msgComposer("In datastream mode, destination depth must be 8 bits.\nChange the settings in the configuration file.\n"));
      } else THROW_NOT_SUPPORTED(ActiveSilicon::CASLException::msgComposer("In datastream mode, source depth must be 10 or 12 bits.\nChange the settings in the configuration file.\n"));
   }

   return result;
}

void CCamera::unpackData(uint8_t * imageIn, uint16_t * imageOut)
{
   ui16 tmpData;
   for (uint32_t i = 0; i < m_imageHeight; i++) {
      if (m_packedDepth == 10) {
         for (uint32_t j = 0; j < m_imageWidth/4; j++) {
            tmpData = 0x00;
            tmpData  = (ui16) ( *imageIn++         << 8 );
            tmpData |= (ui16) ((*imageIn   & 0xc0)      );
            *imageOut++ = tmpData;
            tmpData = 0x00;
            tmpData  = (ui16) ((*imageIn++ & 0x3f) << 10);
            tmpData |= (ui16) ((*imageIn   & 0xf0) << 2 );
            *imageOut++ = tmpData;
            tmpData = 0x00;
            tmpData  = (ui16) ((*imageIn++ & 0x0f) << 12);
            tmpData |= (ui16) ((*imageIn   & 0xfc) << 4 );
            *imageOut++ = tmpData;
            tmpData = 0x00;
            tmpData  = (ui16) ((*imageIn++ & 0x03) << 14);
            tmpData |= (ui16) ((*imageIn++       ) << 6 );
            *imageOut++ = tmpData;
         }
      } else {
         for (uint32_t j = 0; j < m_imageWidth/2; j++) {
            tmpData = 0x00;
            tmpData  = (ui16) ( *imageIn++         << 8 );
            tmpData |= (ui16) ((*imageIn   & 0xf0)      );
            *imageOut++ = tmpData;
            tmpData = 0x00;
            tmpData  = (ui16) ((*imageIn++ & 0x0f) << 12);
            tmpData |= (ui16) ((*imageIn++       ) << 4 );
            *imageOut++ = tmpData;
         }
      }
      imageIn+= m_packedExtraBits;
   }
}

bool CCamera::BayerFilter(int TMGImageIn, int TMGImageOut, bool colorCorrection)
{
   int TMGConvertedImage = 0;

   try {
	   int topLeftPixel = -1;

	   switch (m_grabberPixelformat) {
	      case CXP_PIXEL_FORMAT_BAYER_RG8:
	      case CXP_PIXEL_FORMAT_BAYER_RG10:
	      case CXP_PIXEL_FORMAT_BAYER_RG12: topLeftPixel = 1; break;
	      case CXP_PIXEL_FORMAT_BAYER_GR8:
	      case CXP_PIXEL_FORMAT_BAYER_GR10:
	      case CXP_PIXEL_FORMAT_BAYER_GR12: topLeftPixel = 2; break;
	      case CXP_PIXEL_FORMAT_BAYER_GB8:
	      case CXP_PIXEL_FORMAT_BAYER_GB10:
	      case CXP_PIXEL_FORMAT_BAYER_GB12: topLeftPixel = 3; break;
	      case CXP_PIXEL_FORMAT_BAYER_BG8:
	      case CXP_PIXEL_FORMAT_BAYER_BG10:
	      case CXP_PIXEL_FORMAT_BAYER_BG12: topLeftPixel = 4; break;
	   }
	   
	   if (topLeftPixel == -1) return false;

	   int format;
	   _TMG_THROW_ON_ERROR(TMG_Image_ParameterGet(TMGImageIn, TMG_PIXEL_FORMAT, &format));

	   if (format != TMG_Y8) {
	      _TMG_THROW_ON_ERROR(TMG_Image_Create(&TMGConvertedImage, 0, NULL));
	      _TMG_THROW_ON_ERROR(TMG_IP_Image_Convert(TMGImageIn, TMGConvertedImage, TMG_Y8, 0));
	      TMGImageIn = TMGConvertedImage;
      }

	   _TMG_THROW_ON_ERROR(TMG_Image_ParameterSet(TMGImageIn, TMG_PIXEL_FORMAT, TMG_RGGB32));

	   float * pfColorMatrix = colorCorrection ? m_fColorMatrix : 0;

	   _TMG_THROW_ON_ERROR(TMG_IP_Bayer_Convert_to_BGRX32( TMGImageIn, TMGImageOut, 2, pfColorMatrix, NULL, topLeftPixel));
      if (TMGConvertedImage) TMG_Image_Destroy(TMGConvertedImage);
   } catch(...) {
      if (TMGConvertedImage) TMG_Image_Destroy(TMGConvertedImage);
      THROW_UNRECOVERABLE("Bayer conversion failed.");
   }
   return true;
}

void CCamera::startRecord()
{
   m_recordAtomic = 1;
}

void CCamera::nextRecordFormat()
{
   // only if recording not already triggered
   if (!m_recordAtomic) {
      if (++m_recordSelectedFormatAtomic >= sizeof(g_strRecordFormats) / sizeof(const char *)) {
         m_recordSelectedFormatAtomic = 0;
      }
   }
}

DWORD WINAPI CCamera::RecordThread(void * x)
{
   CCamera * This = reinterpret_cast<CCamera *>(x);
   if (!This) {
      MessageBox(0, "Null 'this' pointer.", "Error in "__FUNCTION__, MB_OK);
      return 1;
   }

   try {

      while (!This->m_recordThreadStop) {

         DWORD dwStatus = WaitForSingleObject(This->m_eventFromRecording, 250); // use an event to avoid Sleep() and the lag associated

         if (dwStatus != WAIT_OBJECT_0) {
            continue;
         } else {
            This->DoRecord(); // save sequence to file(s)
            // reset the flags and events associated to the recording and restart the frame grabber
            // m_callbackCount and m_recordingBuffersSoFar are reset in the startFrameGrabber() function
            This->m_recordAtomic = 0;
            ResetEvent(This->m_eventFromRecording);
            This->m_recordingThreadSignaledAtomic = 0;
            This->m_recordCurrentFrameIndex = 0;
            This->startFrameGrabber();
         }
      }
   } catch (std::exception &e) {
      MessageBox(0, e.what(), "Error in "__FUNCTION__, MB_OK);
   } catch (...) {
      MessageBox(0, "Unknown Exception.", "Error in "__FUNCTION__, MB_OK);
   }
   This->m_recordAtomic = 0;
   if (This->m_eventFromRecording) ResetEvent(This->m_eventFromRecording);
   return 0;
}

bool CCamera::recordAll()    {return m_recordSelectedFormatAtomic == ((sizeof(g_strRecordFormats) / sizeof(const char *)) - 1);}
bool CCamera::recordAvi()    {return m_recordSelectedFormatAtomic == 0 || recordAll();}
bool CCamera::recordTiff()   {return m_recordSelectedFormatAtomic == 1 || recordAll();}
bool CCamera::recordBmp()    {return m_recordSelectedFormatAtomic == 2 || recordAll();}
bool CCamera::recordRaw()    {return m_recordSelectedFormatAtomic == 3 || recordAll();}

void CCamera::DoSaveToTiff(int hTMGImage, std::string &filename, bool bSaveCompatible)
{
   if (hTMGImage) {
      int hTMGImageCompatible = 0;
      try {
         filename += std::string(".tif");
         _TMG_THROW_ON_ERROR(TMG_Image_FilenameSet(hTMGImage, TMG_FILENAME_OUT, const_cast<char *>(filename.c_str())));
         _TMG_THROW_ON_ERROR(TMG_image_write(hTMGImage, TMG_NULL, TMG_TIFF, TMG_RUN));

         if (bSaveCompatible) {
            int format;
            _TMG_THROW_ON_ERROR(TMG_Image_ParameterGet(hTMGImage, TMG_PIXEL_FORMAT, &format));

            if ((format != TMG_Y8) && (format != TMG_RGB8) && (format != TMG_YUV422)) {
               size_t pos = filename.find_last_of(".");
               std::string compatible = filename.substr(0, pos) + std::string("_bis.tif");
               // Also save a Y8 image as some viewers may not be able to open exotic pixel formats.
               _TMG_THROW_ON_ERROR(TMG_Image_Create(&hTMGImageCompatible, 0, NULL));
               _TMG_THROW_ON_ERROR(TMG_IP_Image_Convert(hTMGImage, hTMGImageCompatible, TMG_Y8, 0));
               _TMG_THROW_ON_ERROR(TMG_Image_FilenameSet(hTMGImageCompatible, TMG_FILENAME_OUT, const_cast<char *>(compatible.c_str())));
               _TMG_THROW_ON_ERROR(TMG_image_write(hTMGImageCompatible, TMG_NULL, TMG_TIFF, TMG_RUN));
               _TMG_THROW_ON_ERROR(TMG_image_destroy(hTMGImageCompatible));
            }
         }
      } catch (...) {
         if (hTMGImageCompatible) _TMG_MESSAGE_BOX_ON_ERROR(TMG_image_destroy(hTMGImageCompatible));
         throw;
      }
   }
}

void CCamera::DoSaveToBmp(int hTMGImage, std::string &filename)
{
   if (hTMGImage) {
      int format;
      TMG_Image_ParameterGet(hTMGImage, TMG_PIXEL_FORMAT, &format);
      if ((format == TMG_RGB24)   ||
          (format == TMG_BGR24)   ||
          (format == TMG_BGRX32)  ||
          (format == TMG_YUV422)  ||
          (format == TMG_Y8)      ||
          (format == TMG_PALETTED)   ) // only these formats can be saved to BMP files
      {
         filename += std::string(".bmp");
         _TMG_THROW_ON_ERROR(TMG_Image_FilenameSet(hTMGImage, TMG_FILENAME_OUT, const_cast<char *>(filename.c_str())));
         _TMG_THROW_ON_ERROR(TMG_image_write(hTMGImage, TMG_NULL, TMG_BMP, TMG_RUN));
      }
   }
}

void CCamera::DoSaveToRaw(int hTMGImage, std::string &filename)
{
   if (hTMGImage) {
      filename += std::string(".dat");
      _TMG_THROW_ON_ERROR(TMG_Image_FilenameSet(hTMGImage, TMG_FILENAME_OUT, const_cast<char *>(filename.c_str())));
      _TMG_THROW_ON_ERROR(TMG_image_write(hTMGImage, TMG_NULL, TMG_RAW, TMG_RUN));
   }
}

uint8_t CCamera::toggleDisplayOnOff()
{
   m_displayPausedAtomic = m_displayPausedAtomic ? 0 : 1;
   return m_displayPausedAtomic;
}

uint8_t CCamera::toggleDisplayMenuSimpleAdvanced()
{
   m_displayMenuAdvanced = m_displayMenuAdvanced ? 0 : 1;
   return m_displayMenuAdvanced;
}

void CCamera::toggleFitToDisplayOnOff()
{
   m_fitToDisplay = m_fitToDisplay ? false:true;
   m_display->fitToDisplay(m_fitToDisplay);
}

void CCamera::paint()
{
   if (m_saveImageToFileFlagAtomic) { // save current image
      m_saveImageToFileFlagAtomic = 0;
      //saveToTIFF(std::string(m_saveImageToFileName));
      //saveToRAW(std::string(m_saveImageToFileName));
      saveToBMP(std::string(m_saveImageToFileName)); // only 8 bits
   }

   // When the paint() function gets called (at each WM_PAINT message), a buffer has to be displayed
   // if m_bufferForDisplayAtomic == 1:
   //    in paint(), the previous buffer (stored in m_displayImageADL) is displayed
   //    in the BufferProcessingThread, we memcpy the next buffer to have a new one to display
   // if m_bufferForDisplayAtomic == 0:
   //    we now know there is a new buffer available and we use it for display. After the call to setImage(), we  "ask" for a new buffer (set m_bufferForDisplayAtomic to 1).
   //    in the BufferProcessingThread, we don't do anything as the current buffer has not been used by paint() yet.
   //    (we still get and release the buffers acquired by the frame grabber so when we will need a new buffer to display, we will have the most recently acquired)

   // we use 2 buffers, so we can overwrite memory of one while displaying the other
   // because the memcpy happens at a rate of maximum the display rate, we are sure that with 2 buffers, we will not overwrite the buffer being displayed

   uint8_t bufferForDisplayAtomic = m_bufferForDisplayAtomic; // only read it once, as it may change while this function is being run (the value is used later in the function)
   if (!bufferForDisplayAtomic) { // a new buffer has been memcopied to m_nextDisplayBuffer in the bufferProcessingThread, which we want to use for display

      if (!m_displayPausedAtomic) {
         _TMG_THROW_ON_ERROR(TMG_Image_ParameterSet(m_displayTMGImageFromCamera, TMG_IMAGE_WIDTH,  m_nextDisplayBuffer.width));
         _TMG_THROW_ON_ERROR(TMG_Image_ParameterSet(m_displayTMGImageFromCamera, TMG_IMAGE_HEIGHT, m_nextDisplayBuffer.height));

         m_displayImageWidth  = m_nextDisplayBuffer.width;
         m_displayImageHeight = m_nextDisplayBuffer.height;

         int TMGPixelFormat = TMG_Y8;
         switch(m_nextDisplayBuffer.cxpPixelFormat)
         {
            case CXP_PIXEL_FORMAT_MONO8:
            case CXP_PIXEL_FORMAT_BAYER_GR8:
            case CXP_PIXEL_FORMAT_BAYER_RG8:
            case CXP_PIXEL_FORMAT_BAYER_GB8:
            case CXP_PIXEL_FORMAT_BAYER_BG8:    TMGPixelFormat = TMG_Y8;      break;
            case CXP_PIXEL_FORMAT_MONO10:
            case CXP_PIXEL_FORMAT_MONO12:
            case CXP_PIXEL_FORMAT_BAYER_GR10:
            case CXP_PIXEL_FORMAT_BAYER_RG10:
            case CXP_PIXEL_FORMAT_BAYER_GB10:
            case CXP_PIXEL_FORMAT_BAYER_BG10:
            case CXP_PIXEL_FORMAT_BAYER_GR12:
            case CXP_PIXEL_FORMAT_BAYER_RG12:
            case CXP_PIXEL_FORMAT_BAYER_GB12:
            case CXP_PIXEL_FORMAT_BAYER_BG12:   TMGPixelFormat = TMG_Y16;     break;
            case CXP_PIXEL_FORMAT_YUV422:       TMGPixelFormat = TMG_YUV422;  break;
            case CXP_PIXEL_FORMAT_RGB24:        TMGPixelFormat = TMG_RGB24;   break;
            default: THROW_UNRECOVERABLE("Unsupported pixel format.");
         }

         _TMG_THROW_ON_ERROR(TMG_Image_ParameterSet(m_displayTMGImageFromCamera, TMG_PIXEL_FORMAT, TMGPixelFormat));

         // Update TMG image with pointer to image data
         _TMG_THROW_ON_ERROR(TMG_Image_ParameterPtrSet(m_displayTMGImageFromCamera, TMG_IMAGE_BUFFER, m_nextDisplayBuffer.data.get()));

         // Image is user allocated
         _TMG_THROW_ON_ERROR(TMG_image_set_flags(m_displayTMGImageFromCamera, TMG_USER_ALLOCATED_MEM, TRUE));

         // Lock the buffer memory associated with hTMGImageFromCamera to prevent any re-allocation (for speed).
         _TMG_THROW_ON_ERROR(TMG_image_set_flags(m_displayTMGImageFromCamera, TMG_LOCKED, TRUE));

         // Check the TMG image is valid
         _TMG_THROW_ON_ERROR(TMG_image_check(m_displayTMGImageFromCamera));

         m_displayCopyOfTMGImage = m_displayTMGImageFromCamera;

         // Use 2 images instead of one to avoid losing the handle of the image: while the m_displayTMGImageProcessing is debayered, it might also be used by the library for the display
         int temp = 0;
         temp = m_displayTMGImageProcessingNext; // keep the handle of the image
         m_displayTMGImageProcessingNext = m_displayTMGImageProcessingCurrent; // swap handles
         if (BayerFilter(m_displayTMGImageFromCamera, m_displayTMGImageProcessingNext, m_bayerColorCorrection)) {
            m_displayCopyOfTMGImage = m_displayTMGImageProcessingNext;
         }

         _ADL_THROW_ON_ERROR(ImageFromTMG(&m_displayImageADL, m_displayCopyOfTMGImage)); // creates ADL image if NULL pointer, otherwise reuse existing one
         m_displayTMGImageProcessingCurrent = temp; // swap handles

      }
   }

   // Display the image and the various text blocks
   if (!m_displayPausedAtomic && !bufferForDisplayAtomic) m_display->setImage(m_displayImageADL); // we provide the image to the display library. It is either the same as from the previous call or a new one.


   if (!bufferForDisplayAtomic) {
      // swap buffers to avoid memory of the one being displayed to be overwritten by a new one
      BufferInfo tempBuffer      = m_nextDisplayBuffer;
      m_nextDisplayBuffer        = m_currentDisplayBuffer;
      m_currentDisplayBuffer     = tempBuffer;
      m_bufferForDisplayAtomic   = 1; // we have now used the new buffer for display, and will then need a new one as soon as possible
   }

   text();

   m_paintDoneAtomic = 1;
}

void CCamera::text()
{
   if (!m_display->adlDisplay()) return;

   LONG fps = InterlockedCompareExchange(reinterpret_cast<volatile LONG *>(&m_acquisitionFPS), 0, 0);
   float acquisitionFPS = *reinterpret_cast<float *>(&fps);

#define ASL_WHITE ADL_ARGB(255,255,255,255)
#define ASL_RED ADL_ARGB(255,255,21,40)
#define ASL_GREEN ADL_ARGB(255,0,255,0)
#define ASL_BLUE ADL_ARGB(55,0,0,255)

#define DEMO_TITLE "DHuT"

#define SHOW_ADVANCED_MENU "'S': Show advanced menu\n"

#define START_RECORDING "F11 or Right-click: Start recording sequence to %s (%d / %d)\n"""

   std::stringstream acquisitionModeStream;

   acquisitionModeStream << "F6: Continuous Mode\n";

   // here, the parameter of 'm_acquisitionModes.size()' is 3.
   for (size_t i = 0; i < m_acquisitionModes.size() - 1; i++) {
      acquisitionModeStream << "F" << 7 + i << ": " << m_acquisitionModes[i + 1] << "\n";
   }

   std::string cameraStartStopText = m_isCameraLink ? "" : "Space: Start / Stop camera\n";

   #define ADVANCED_CAMERA_MENU(x)\
   sprintf_s(x, ""\
   "%s\n\n"\
   "Current acquisition mode: %s\n\n"\
   "F1: Stretch to fit display (%s)\n"\
   "F3: Pause display only (the screen will not be refreshed)\n"\
   "F4: Save current image to file (.bmp)\n"\
   "F5: Record to %s\n"\
   "%s"\
   "E: %s Frame Start Event (%d)\n"\
   "Middle-Click: Restart\n"\
   "%s"\
   START_RECORDING\
   "Esc: Quit\n"\
   "'S': Show simple menu\n"\
   "\n"\
   "%3.1f Fps (display)\n"\
   "%3.1f Fps (hardware)\n"\
   "",\
   m_displayCameraName.c_str(),\
   m_acquisitionModes[m_acquisitionModeSelectedAtomic].c_str(),\
   m_display->isStretched()?"true":"false",\
   g_strRecordFormats[m_recordSelectedFormatAtomic],\
   acquisitionModeStream.str().c_str(),\
   m_FrameStartEventEnabledAtomic ? "Disable" : "Enable",\
   m_FrameStartEventCount,\
   cameraStartStopText.c_str(),\
   g_strRecordFormats[m_recordSelectedFormatAtomic],\
   m_recordCurrentFrameIndex,\
   m_recordBufferCount,\
   m_display->fps(),\
   acquisitionFPS\
   );

   HWND hWnd;
   _ADL_THROW_ON_ERROR(m_display->adlDisplay()->GetAttribute(ADL_DISPLAY_ATTRIBUTE_WINDOW, &hWnd));

   RECT wndRect;
   GetClientRect(hWnd, &wndRect);

   uint32_t Height = wndRect.bottom - wndRect.top;

   ADL_RECT r = {0, Height - 100, 200, 400};
   char c[1024];

   sprintf_s(c, sizeof(c), "\
   \n"
   START_RECORDING
   "Esc: Quit\n"
   SHOW_ADVANCED_MENU\
   "\n\
   ",\
   g_strRecordFormats[m_recordSelectedFormatAtomic],
   m_recordCurrentFrameIndex,
   m_recordBufferCount
   );

   if (m_displayMenuAdvanced) {
      ADVANCED_CAMERA_MENU(c);
   }

   if (m_displayFontMainMenu) _ADL_THROW_ON_ERROR(m_displayFontMainMenu->Text(c, &r, ASL_GREEN));

   r.y = Height-60;
   r.height = 60;
   if (m_displayFontConfiguration) _ADL_THROW_ON_ERROR(m_displayFontConfiguration->Text(DEMO_TITLE, &r, ASL_RED));
   
   //float DeviceTemperature = MonitorTemp();

   {
      sprintf_s(c, "%d x %d @ %d fps", m_displayImageWidth, m_displayImageHeight
		  , static_cast<int32_t>(acquisitionFPS+0.5));
      ADL_RECT r = {0, 35, 150, 300};
      if (m_displayFontConfiguration) _ADL_THROW_ON_ERROR(m_displayFontConfiguration->Text(c, &r, ASL_RED));
   }
}

#undef ALERT_AND_SWALLOW
#undef htonl
