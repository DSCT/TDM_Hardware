/****************************************************************************
 *
 * ACTIVE SILICON LIMITED
 *
 * Authors     : Jean-Philippe Arnaud
 *
 * Copyright (c) 2000-2013 Active Silicon Ltd.
 ****************************************************************************
 */

#include "CameraAVT.h"

CCameraAvt::CCameraAvt(
                   struct Options_t & options
                 , HWND hwnd
           ) : 
            CCamera (options, hwnd)

{
   m_isCameraLink       = true;
   m_useSynchronizedAcquisition  = true;

   if (!m_internalTrigger) m_acquisitionModes.push_back("TTL#0 Frame Trigger (Grabber Side)");
   else m_acquisitionModes.push_back("Internal Frame Trigger (Grabber Side)");
}

CCameraAvt::~CCameraAvt(void)
{
}

void CCameraAvt::setAcquisitionStart()
{
}

void CCameraAvt::setAcquisitionStop()
{
}

void CCameraAvt::setPixelFormat(enum CXP_PIXEL_FORMAT format)
{
}

void CCameraAvt::setMiscellaneous()
{
}

void CCameraAvt::SetBitRateAndDiscovery(int lanes, int gbps)
{
}

void CCameraAvt::initTriggerMode1()
{
   if (!m_internalTrigger) {
      hostSetParameter(PHX_FGTRIG_SRC,       PHX_FGTRIG_SRC_TTLIN_CHX_0);
   } else {
      hostSetParameter(PHX_FGTRIG_SRC,       PHX_FGTRIG_SRC_TIMERA1_CHX);
      hostSetParameter(PHX_TIMERA1_PERIOD,   10000);
   }

   hostSetParameter(PHX_FGTRIG_MODE, PHX_FGTRIG_FIRST_POS_EDGE);

   startRecord();
}
