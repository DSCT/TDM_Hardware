/****************************************************************************
 *
 * ACTIVE SILICON LIMITED
 *
 * Authors     : Jean-Philippe Arnaud
 *
 * Copyright (c) 2000-2013 Active Silicon Ltd.
 ****************************************************************************
 */

#include "CameraIOIndustries.h"

CCameraIOIndustries::CCameraIOIndustries(
                   struct Options_t & options
                 , HWND hwnd
           ) : 
            CCamera (options, hwnd)

{
   if (!m_internalTrigger) m_acquisitionModes.push_back("TTL#0 Frame Trigger (Grabber Side)");
   else m_acquisitionModes.push_back("Internal Frame Trigger (Grabber Side)");
}

CCameraIOIndustries::~CCameraIOIndustries(void)
{
}

void CCameraIOIndustries::setAcquisitionStart()
{
   deviceWrite(0x10000020, 1);
}

void CCameraIOIndustries::setAcquisitionStop()
{
   deviceWrite(0x10000024, 1);
}

void CCameraIOIndustries::setPixelFormat(enum CXP_PIXEL_FORMAT format)
{
   if (format == CXP_PIXEL_FORMAT_BAYER_GB8) {
      deviceWrite(0x40000000, 0);
   } else if (format == CXP_PIXEL_FORMAT_BAYER_GB10) {
      deviceWrite(0x40000000, 1);
   }
}

void CCameraIOIndustries::setMiscellaneous()
{
}

void CCameraIOIndustries::initTriggerMode1()
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
