/****************************************************************************
 *
 * ACTIVE SILICON LIMITED
 *
 * Authors     : Jean-Philippe Arnaud
 *
 * Copyright (c) 2000-2013 Active Silicon Ltd.
 ****************************************************************************
 */

#include "CameraOptronis.h"

CCameraOptronis::CCameraOptronis(
                   struct Options_t & options
                 , HWND hwnd
           ) : 
            CCamera (options, hwnd)

{
   if (!m_internalTrigger) m_acquisitionModes.push_back("TTL#0 Frame Trigger (Grabber Side)");
   else m_acquisitionModes.push_back("Internal Frame Trigger (Grabber Side)");
}

CCameraOptronis::~CCameraOptronis(void)
{
}

void CCameraOptronis::setAcquisitionStart()
{
   deviceWrite(0x601c, 1);
}

void CCameraOptronis::setAcquisitionStop()
{
   deviceWrite(0x601c, 0);
}

void CCameraOptronis::setPixelFormat(enum CXP_PIXEL_FORMAT format)
{
   deviceWriteAndReadBack(0x60f0, format);
}

void CCameraOptronis::setMiscellaneous()
{
   if (m_applyROI) { // we apply the frame grabber ROI (from the pcf file) to the camera and center the new camera ROI
      
      const uint32_t widthReg    = 0x6000;
      const uint32_t heightReg   = 0x6004;
      const uint32_t xOffsetReg  = 0x60d0;
      const uint32_t yOffsetReg  = 0x60d4;

      // width:   min 256, increment 256
      // height:  min 4,   increment 4
      int32_t x_max, y_max, x, y;
      int32_t x_min = 256; int32_t x_inc = 256;
      int32_t y_min = 4;   int32_t y_inc = 4;

      deviceRead(widthReg,   x);
      deviceRead(heightReg,  y);
      x_max = (x & 0xFFFF0000) >> 16; // the value contains both the current width and the max width
      y_max = (y & 0xFFFF0000) >> 16; // the value contains both the current height and the max height

      // calculate new ROI, the function returns the new width and height in x and y
      calculateCameraROI(x_min, y_min, x_max, y_max, x_inc, y_inc, x, y);

      // calculate offsets to center camera ROI, the function returns the offsets in x_offset and y_offset
      int32_t x_offset, y_offset;
      int32_t x_offset_inc = 64;
      int32_t y_offset_inc = 4;
      calculateCameraOffset(x, y, x_max, y_max, x_offset_inc, y_offset_inc, x_offset, y_offset);

      // set the new camera values
      deviceWrite(widthReg,     x);         // set the width
      deviceWrite(heightReg,    y);         // set the height
      deviceWrite(xOffsetReg,   x_offset);  // set x_offset
      deviceWrite(yOffsetReg,   y_offset);  // set y_offset
   }

   // set the frame rate to its max
   const uint32_t frameRateMaxReg   = 0x600c;
   const uint32_t frameRateReg      = 0x6008;

   int32_t frameRate_max;
   deviceRead(frameRateMaxReg, frameRate_max);
   deviceWriteAndReadBack(frameRateReg, frameRate_max);
}

void CCameraOptronis::initTriggerMode1()
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
