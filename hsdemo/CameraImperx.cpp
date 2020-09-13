/****************************************************************************
 *
 * ACTIVE SILICON LIMITED
 *
 * Authors     : Jean-Philippe Arnaud
 *
 * Copyright (c) 2000-2013 Active Silicon Ltd.
 ****************************************************************************
 */

#include "CameraImperx.h"

CCameraImperx::CCameraImperx(
                   struct Options_t & options
                 , HWND hwnd
           ) : 
            CCamera (options, hwnd)

{
   if (!m_internalTrigger) m_acquisitionModes.push_back("TTL#0 Frame Trigger (Grabber Side)");
   else m_acquisitionModes.push_back("Internal Frame Trigger (Grabber Side)");
}

CCameraImperx::~CCameraImperx(void)
{
}

void CCameraImperx::setAcquisitionStart()
{
   deviceWrite(0x10000020, 1);
}

void CCameraImperx::setAcquisitionStop()
{
   deviceWrite(0x10000024, 1);
}

void CCameraImperx::setPixelFormat(enum CXP_PIXEL_FORMAT format)
{
   deviceWriteAndReadBack(0x2000060C, format);
}

void CCameraImperx::setMiscellaneous()
{
}

void CCameraImperx::initTriggerMode1()
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
