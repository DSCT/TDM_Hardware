/****************************************************************************
 *
 * ACTIVE SILICON LIMITED
 *
 * Authors     : Jean-Philippe Arnaud
 *
 * Copyright (c) 2000-2013 Active Silicon Ltd.
 ****************************************************************************
 */

#include "CameraNED.h"

CCameraNED::CCameraNED(
                   struct Options_t & options
                 , HWND hwnd
           ) : 
            CCamera (options, hwnd)

{
   if (!m_internalTrigger) m_acquisitionModes.push_back("TTL#0 Frame Trigger (Grabber Side)");
   else m_acquisitionModes.push_back("Internal Frame Trigger (Grabber Side)");
}

CCameraNED::~CCameraNED(void)
{
}

void CCameraNED::setAcquisitionStart()
{
   deviceWrite(0xA00C, 1);
}

void CCameraNED::setAcquisitionStop()
{
   deviceWrite(0xA010, 1);
}

void CCameraNED::setPixelFormat(enum CXP_PIXEL_FORMAT format)
{
   deviceWrite(0xA014, format);
}

void CCameraNED::setMiscellaneous()
{
}

void CCameraNED::initTriggerMode1()
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
