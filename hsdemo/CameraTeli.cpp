/****************************************************************************
 *
 * ACTIVE SILICON LIMITED
 *
 * Authors     : Jean-Philippe Arnaud
 *
 * Copyright (c) 2000-2013 Active Silicon Ltd.
 ****************************************************************************
 */

#include "CameraTeli.h"

CCameraTeli::CCameraTeli(
                   struct Options_t & options
                 , HWND hwnd
           ) : 
            CCamera (options, hwnd)

{
   if (!m_internalTrigger) m_acquisitionModes.push_back("TTL#0 Frame Trigger (Grabber Side)");
   else m_acquisitionModes.push_back("Internal Frame Trigger (Grabber Side)");
}

CCameraTeli::~CCameraTeli(void)
{
}

void CCameraTeli::setAcquisitionStart()
{
   deviceWrite(0xB008, 1);
}

void CCameraTeli::setAcquisitionStop()
{
   deviceWrite(0xB00C, 1);
}

void CCameraTeli::setPixelFormat(enum CXP_PIXEL_FORMAT format)
{
}

void CCameraTeli::setMiscellaneous()
{
   m_fColorMatrix[0] = (float) 1.4; m_fColorMatrix[1] = (float) 0;   m_fColorMatrix[2] = (float) 0;
   m_fColorMatrix[3] = (float) 0;   m_fColorMatrix[4] = (float) 1.2; m_fColorMatrix[5] = (float) 0;
   m_fColorMatrix[6] = (float) 0;   m_fColorMatrix[7] = (float) 0;   m_fColorMatrix[8] = (float) 1.4;
}

void CCameraTeli::forceCameraOn()
{
   hostSetParameter(static_cast<etParam>(PHX_CXP_POCXP_MODE | PHX_CACHE_FLUSH), PHX_CXP_POCXP_MODE_FORCEON);
}

void CCameraTeli::forceCameraOff()
{
   hostSetParameter(static_cast<etParam>(PHX_CXP_POCXP_MODE | PHX_CACHE_FLUSH), PHX_CXP_POCXP_MODE_AUTO);
}

void CCameraTeli::initTriggerMode1()
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
