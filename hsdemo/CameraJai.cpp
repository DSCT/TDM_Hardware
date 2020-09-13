/****************************************************************************
 *
 * ACTIVE SILICON LIMITED
 *
 * Authors     : Jean-Philippe Arnaud
 *
 * Copyright (c) 2000-2013 Active Silicon Ltd.
 ****************************************************************************
 */

#include "CameraJai.h"

CCameraJai::CCameraJai(
                   struct Options_t & options
                 , HWND hwnd
           ) : 
            CCamera (options, hwnd)

{
   if (!m_internalTrigger) m_acquisitionModes.push_back("TTL#0 Frame Trigger (Grabber Side)");
   else m_acquisitionModes.push_back("Internal Frame Trigger (Grabber Side)");
}

CCameraJai::~CCameraJai(void)
{
}

void CCameraJai::setAcquisitionStart()
{
   deviceWrite(0x200B0, 1);
}

void CCameraJai::setAcquisitionStop()
{
   deviceWrite(0x200B4, 1);
}

void CCameraJai::setPixelFormat(enum CXP_PIXEL_FORMAT format)
{
   deviceWrite(0x20090, format);
   Sleep(50);                       // Changing the pixel format seems to take some time in the camera. A sleep will avoid a timeout on the next camera register access.
}

void CCameraJai::setMiscellaneous()
{
   if (m_applyROI) { // we apply the frame grabber ROI (from the pcf file) to the camera and center the new camera ROI
      const uint32_t widthReg             = 0x20064;
      const uint32_t heightReg            = 0x20068;
      const uint32_t horizontalBinningReg = 0x20078;
      const uint32_t verticalBinningReg   = 0x2007c;
      const uint32_t xOffsetReg           = 0x2006c;
      const uint32_t yOffsetReg           = 0x20070;

      int32_t x, y, x_max, y_max, x_min, x_inc, y_min, y_inc;
      int32_t horizontalBinning, verticalBinning;

      deviceRead(horizontalBinningReg, horizontalBinning);
      deviceRead(verticalBinningReg,   verticalBinning);
      x_min = horizontalBinning == 1 ? 16 : horizontalBinning * 8;
      x_inc = x_min;
      x_max = 2560 / horizontalBinning;
      y_min = 8;
      y_inc = verticalBinning;
      y_max = 2048 / verticalBinning;

      deviceRead(widthReg,   x);
      deviceRead(heightReg,  y);

      // calculate new ROI, the function returns the new width and height in x and y
      calculateCameraROI(x_min, y_min, x_max, y_max, x_inc, y_inc, x, y);

      // calculate offsets to center camera ROI, the function returns the offsets in x_offset and y_offset
      int32_t x_offset, y_offset;
      int32_t x_offset_inc = x_inc;
      int32_t y_offset_inc = y_inc;
      calculateCameraOffset(x, y, x_max, y_max, x_offset_inc, y_offset_inc, x_offset, y_offset);

      // set the new camera values
      deviceWrite(widthReg,     x);         // set the width
      deviceWrite(heightReg,    y);         // set the height
      deviceWrite(xOffsetReg,   x_offset);  // set x_offset
      deviceWrite(yOffsetReg,   y_offset);  // set y_offset
   }
}

void CCameraJai::SetBitRateAndDiscovery(int lanes, int gbps)
{
}

void CCameraJai::initTriggerMode1()
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
