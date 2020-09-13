/****************************************************************************
 *
 * ACTIVE SILICON LIMITED
 *
 * Authors     : Jean-Philippe Arnaud
 *
 * Copyright (c) 2000-2013 Active Silicon Ltd.
 ****************************************************************************
 */

#include "CameraDreampact.h"

CCameraDreampact::CCameraDreampact(
                   struct Options_t & options
                 , HWND hwnd
           ) : 
            CCamera (options, hwnd)

{
   if (!m_internalTrigger) m_acquisitionModes.push_back("TTL#0 Frame Trigger (Grabber Side)");
   else m_acquisitionModes.push_back("Internal Frame Trigger (Grabber Side)");
}

CCameraDreampact::~CCameraDreampact(void)
{
}

void CCameraDreampact::setAcquisitionStart()
{
   deviceWrite(0x8004, 1);
}

void CCameraDreampact::setAcquisitionStop()
{
   deviceWrite(0x8008, 1);
}

void CCameraDreampact::setPixelFormat(enum CXP_PIXEL_FORMAT format)
{
   deviceWriteAndReadBack(0x703C, format);
}

void CCameraDreampact::setMiscellaneous()
{
   if (m_applyROI) { // we apply the frame grabber ROI (from the pcf file) to the camera and center the new camera ROI

      const uint32_t widthReg       = 0x7014;
      const uint32_t heightReg      = 0x7018;
      const uint32_t maxWidthReg    = 0x700C;
      const uint32_t maxHeightReg   = 0x7010;
      const uint32_t xOffsetReg     = 0x701C;
      const uint32_t yOffsetReg     = 0x7020;

      deviceWriteAndReadBack(xOffsetReg, 0);  // reset x_offset
      deviceWriteAndReadBack(yOffsetReg, 0);  // reset y_offset

      // width:   min 128, increment 128
      // height:  min 1, increment 1
      int32_t x_max, y_max, x, y;
      int32_t x_min = 2;   uint32_t x_inc = 1;
      int32_t y_min = 2;   uint32_t y_inc = 1;

      deviceRead(maxWidthReg,   x_max);
      deviceRead(maxHeightReg,  y_max);

      // calculate new ROI, the function returns the new width and height in x and y
      calculateCameraROI(x_min, y_min, x_max, y_max, x_inc, y_inc, x, y);

      // calculate offsets to center camera ROI, the function returns the offsets in x_offset and y_offset
      int32_t x_offset, y_offset;
      int32_t x_offset_inc = 128;
      int32_t y_offset_inc = 1;
      calculateCameraOffset(x, y, x_max, y_max, x_offset_inc, y_offset_inc, x_offset, y_offset);

      // set camera values
      deviceWriteAndReadBack(widthReg,   x);         // set the width
      deviceWriteAndReadBack(heightReg,  y);         // set the height
      deviceWriteAndReadBack(xOffsetReg, x_offset);  // set x_offset
      deviceWriteAndReadBack(yOffsetReg, y_offset);  // set y_offset
   }
}

void CCameraDreampact::initTriggerMode1()
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
