/****************************************************************************
 *
 * ACTIVE SILICON LIMITED
 *
 * Authors     : Jean-Philippe Arnaud
 *
 * Copyright (c) 2000-2013 Active Silicon Ltd.
 ****************************************************************************
 */

#include "CameraAdimecSapphire.h"

CCameraAdimecSapphire::CCameraAdimecSapphire(
                   struct Options_t & options
                 , HWND hwnd
           ) : 
            CCamera (options, hwnd)

{
   if (m_multipleROI) {
      m_acquisitionModes.push_back("Multiple ROI - Internal Frame Trigger");
   } else {
      if (!m_internalTrigger) m_acquisitionModes.push_back("TTL#0 Frame Trigger (Grabber Side)");
      else m_acquisitionModes.push_back("Internal Frame Trigger (Grabber Side)");
   }
}

CCameraAdimecSapphire::~CCameraAdimecSapphire(void)
{
}

void CCameraAdimecSapphire::setAcquisitionStart()
{
   deviceWrite(0x00008204, 1);
}

void CCameraAdimecSapphire::setAcquisitionStop()
{
   deviceWrite(0x00008208, 1);
}

void CCameraAdimecSapphire::setPixelFormat(enum CXP_PIXEL_FORMAT format)
{
   deviceWriteAndReadBack(0x00008144, format);
}

void CCameraAdimecSapphire::setMiscellaneous()
{
   const uint32_t widthReg    = 0x8118;
   const uint32_t heightReg   = 0x811c;
   const uint32_t xOffsetReg  = 0x8120;
   const uint32_t yOffsetReg  = 0x8124;

   if (m_applyROI && !m_multipleROI) { // we apply the frame grabber ROI (from the pcf file) to the camera and center the new camera ROI
      const uint32_t maxWidthReg    = 0x8100;
      const uint32_t maxHeightReg   = 0x8104;

      // width:   min 2, increment 1
      // height:  min 2, increment 1
      int32_t x_max, y_max, x, y;
      int32_t x_min = 2;   uint32_t x_inc = 1;
      int32_t y_min = 2;   uint32_t y_inc = 1;

      deviceRead(maxWidthReg,   x_max);
      deviceRead(maxHeightReg,  y_max);

      // calculate new ROI, the function returns the new width and height in x and y
      calculateCameraROI(x_min, y_min, x_max, y_max, x_inc, y_inc, x, y);

      // calculate offsets to center camera ROI, the function returns the offsets in x_offset and y_offset
      int32_t x_offset, y_offset;
      int32_t x_offset_inc = 1;
      int32_t y_offset_inc = 1;
      calculateCameraOffset(x, y, x_max, y_max, x_offset_inc, y_offset_inc, x_offset, y_offset);

      // set camera values
      deviceWriteAndReadBack(widthReg,   x);         // set the width
      deviceWriteAndReadBack(heightReg,  y);         // set the height
      deviceWriteAndReadBack(xOffsetReg, x_offset);  // set x_offset
      deviceWriteAndReadBack(yOffsetReg, y_offset);  // set y_offset
   } else if (m_multipleROI && !m_applyROI) {
      // Settings for multiple ROI
      // In the pcf file, the width is set to 2368 and the height to 8000.
      // The frame grabber will then acquire 100 images (2368x80 each) within 1 buffer

      // Camera buffer settings
      uint32_t width             = 2368;
      uint32_t height            = 80;
      uint32_t xOffset           = 1344;
      uint32_t yOffset           = 2520;

      deviceWriteAndReadBack(widthReg,   width);
      deviceWriteAndReadBack(heightReg,  height);
      deviceWriteAndReadBack(xOffsetReg, xOffset);
      deviceWriteAndReadBack(yOffsetReg, yOffset);

      // Set the camera stream packet data size
      const uint32_t streamPacketDataSizeReg = 0x4010;
      uint32_t streamPacketDataSize = 2048;
      deviceWriteAndReadBack(streamPacketDataSizeReg, streamPacketDataSize);
   }

   // Set the camera frame period to the lowest
   const uint32_t acquisitionFramePeriodRawReg = 0x8220;
   deviceWrite(acquisitionFramePeriodRawReg, 0);
}

void CCameraAdimecSapphire::initTriggerMode1()
{
   if (m_multipleROI) {
      const uint32_t triggerSourceReg  = 0x8238;         // Trigger: 16
      const uint32_t exposureModeReg   = 0x8250;         // Timed: 0 - Width: 1 - SyncControlMode: 256
      deviceWriteAndReadBack(triggerSourceReg, 16);  // Set the camera to Trigger mode
      deviceWriteAndReadBack(exposureModeReg,  256); // Set the camera exposure mode to SyncControlMode

      const uint32_t framePeriodRawReg = 0x8220;
      deviceWrite(framePeriodRawReg, 0);             // Set the camera frame period to the lowest

      // Set the frame grabber CXP internal trigger
      hostSetParameter(PHX_CAMTRIG_SRC,            PHX_CAMTRIG_SRC_TIMERA1_CHX);       // Configure the frame grabber to use an internal trigger
      hostSetParameter(PHX_TIMERA1_PERIOD,         200);                               // Set the internal trigger period
      hostSetParameter(PHX_CAMTRIG_CXPTRIG_SRC,    PHX_CAMTRIG_CXPTRIG_TIMERM1_CHX);   // Set the trigger width
      hostSetParameter(PHX_TIMERM1_WIDTH ,         150);
      hostSetParameter(PHX_CAMTRIG_CXPTRIG_MODE,   PHX_CAMTRIG_CXPTRIG_RISEFALL);      // Set the trigger to rise/fall mode

      hostSetParameter(PHX_FGTRIG_MODE,            PHX_FGTRIG_FREERUN);                // Set the frame grabber to free run mode
   } else {
      if (!m_internalTrigger) {
         hostSetParameter(PHX_FGTRIG_SRC,       PHX_FGTRIG_SRC_TTLIN_CHX_0);
      } else {
         hostSetParameter(PHX_FGTRIG_SRC,       PHX_FGTRIG_SRC_TIMERA1_CHX);
         hostSetParameter(PHX_TIMERA1_PERIOD,   5000);
      }

      hostSetParameter(PHX_FGTRIG_MODE, PHX_FGTRIG_FIRST_POS_EDGE);

      startRecord();
   }
}

void CCameraAdimecSapphire::initFreeRunMode()
{
   const uint32_t exposureModeReg   = 0x8250;
   const uint32_t framePeriodRawReg = 0x8220;

   deviceWriteAndReadBack  (exposureModeReg, 0);   // Set camera back to Timed exposure
   deviceWrite             (framePeriodRawReg, 0); // Set the camera frame period to the lowest

   CCamera::initFreeRunMode();                     // Set the frame grabber to free run mode
}
