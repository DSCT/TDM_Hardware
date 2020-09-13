/****************************************************************************
 *
 * ACTIVE SILICON LIMITED
 *
 * Authors     : Jean-Philippe Arnaud
 *
 * Copyright (c) 2000-2013 Active Silicon Ltd.
 ****************************************************************************
 */

#include "CameraAslTestModule.h"

CCameraAslTestModule::CCameraAslTestModule(
                   struct Options_t & options
                 , HWND hwnd
           ) : 
            CCamera (options, hwnd)

{
   if (!m_internalTrigger) m_acquisitionModes.push_back("TTL#0 Frame Trigger (Grabber Side)");
   else m_acquisitionModes.push_back("Internal Frame Trigger (Grabber Side)");
}

CCameraAslTestModule::~CCameraAslTestModule(void)
{
}

void CCameraAslTestModule::setAcquisitionStart()
{
   deviceWrite(0x00006000, 0);
}

void CCameraAslTestModule::setAcquisitionStop()
{
   deviceWrite(0x00006000, 1);
}

void CCameraAslTestModule::setPixelFormat(enum CXP_PIXEL_FORMAT format)
{
}

void CCameraAslTestModule::setMiscellaneous()
{
}

void CCameraAslTestModule::initTriggerMode1()
{
   if (!m_internalTrigger) {
      hostSetParameter(PHX_FGTRIG_SRC,       PHX_FGTRIG_SRC_TTLIN_CHX_0);
   } else {
      hostSetParameter(PHX_FGTRIG_SRC,       PHX_FGTRIG_SRC_TIMERA1_CHX);
      hostSetParameter(PHX_TIMERA1_PERIOD,   50000);
   }

   hostSetParameter(PHX_FGTRIG_MODE, PHX_FGTRIG_FIRST_POS_EDGE);

   startRecord();
}
