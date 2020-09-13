/****************************************************************************
 *
 * ACTIVE SILICON LIMITED
 *
 * Authors     : Jean-Philippe Arnaud
 *
 * Copyright (c) 2000-2013 Active Silicon Ltd.
 ****************************************************************************
 */

#include "CameraAdimecQs4A70.h"

CCameraAdimecQs4A70::CCameraAdimecQs4A70(
                   struct Options_t & options
                 , HWND hwnd
           ) : 
            CCamera (options, hwnd)

{
   if (!m_internalTrigger) m_acquisitionModes.push_back("TTL#0 Frame Trigger (Grabber Side)");
   else m_acquisitionModes.push_back("Internal Frame Trigger (Grabber Side)");
}

CCameraAdimecQs4A70::~CCameraAdimecQs4A70(void)
{
}

void CCameraAdimecQs4A70::setAcquisitionStart()
{
   deviceWrite(0x00008204, 1);
}

void CCameraAdimecQs4A70::setAcquisitionStop()
{
   deviceWrite(0x00008208, 1);
}

void CCameraAdimecQs4A70::setPixelFormat(enum CXP_PIXEL_FORMAT format)
{
   deviceWriteAndReadBack(0x00008144, format);
}

void CCameraAdimecQs4A70::setMiscellaneous()
{
   if (m_applyROI) { // we apply the frame grabber ROI (from the pcf file) to the camera and center the new camera ROI

      const uint32_t widthReg       = 0x8118;
      const uint32_t heightReg      = 0x811c;
      const uint32_t maxWidthReg    = 0x8100;
      const uint32_t maxHeightReg   = 0x8104;
      const uint32_t xOffsetReg     = 0x8120;
      const uint32_t yOffsetReg     = 0x8124;

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
   }

   const uint32_t exposureTimeRawReg            = 0x00008258;
   const uint32_t acquisitionFramePeriodRawReg  = 0x00008220;

   deviceWrite(exposureTimeRawReg, 2000);
   deviceWrite(acquisitionFramePeriodRawReg, 26000);
}

// For the Adimec Qs4A70, the SetBitRateAndDiscovery function is overloaded to do nothing because the hardware does not let us change the default values.
void CCameraAdimecQs4A70::SetBitRateAndDiscovery(int lanes, int gbps)
{
   return;
}

void CCameraAdimecQs4A70::initTriggerMode1()
{
   if (!m_internalTrigger) {
      hostSetParameter(PHX_FGTRIG_SRC,       PHX_FGTRIG_SRC_TTLIN_CHX_0);
   } else {
      hostSetParameter(PHX_FGTRIG_SRC,       PHX_FGTRIG_SRC_TIMERA1_CHX);
      hostSetParameter(PHX_TIMERA1_PERIOD,   5000);
   }

   hostSetParameter(PHX_FGTRIG_MODE, PHX_FGTRIG_FIRST_POS_EDGE);

   startRecord();
}
