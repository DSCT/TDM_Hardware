/****************************************************************************
 *
 * ACTIVE SILICON LIMITED
 *
 * Authors     : Jean-Philippe Arnaud
 *
 * Copyright (c) 2000-2013 Active Silicon Ltd.
 ****************************************************************************
 */

#include "CameraE2v.h"

CCameraE2v::CCameraE2v(
                   struct Options_t & options
                 , HWND hwnd
           ) : 
            CCamera (options, hwnd)

{
   if (!m_internalTrigger) m_acquisitionModes.push_back("TTL#0 Frame Trigger (Grabber Side)");
   else m_acquisitionModes.push_back("Internal Frame Trigger (Grabber Side)");
   m_acquisitionModes.push_back("RS422 Line Trigger");
}

CCameraE2v::~CCameraE2v(void)
{
}

void CCameraE2v::setAcquisitionStart()
{
   deviceWrite(0x700c, 0);
}

void CCameraE2v::setAcquisitionStop()
{
   deviceWrite(0x7010, 0);
}

void CCameraE2v::setPixelFormat(enum CXP_PIXEL_FORMAT format)
{
   deviceWriteAndReadBack(0x7014, format);
}

void CCameraE2v::setMiscellaneous()
{
}

void CCameraE2v::initTriggerMode1()
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

void CCameraE2v::initTriggerMode2()
{
   // This simple setup uses a shaft encoder to trigger the camera with the following characteristics:
   // - Shaft encoder phase A is on 422_IN_1
   // - Shaft encoder phase B is on 422_IN_2
   // - Shaft encoder marker (=image trigger) is on 422_IN_3

   hostSetParameter(PHX_CAMTRIG_SRC,            PHX_CAMTRIG_SRC_ENCODER_CHX);
   hostSetParameter(PHX_CAMTRIG_CXPTRIG_SRC ,   PHX_CAMTRIG_CXPTRIG_TIMERM1_CH1);
   hostSetParameter(PHX_CAMTRIG_ENCODER_SRC,    PHX_CAMTRIG_SRC_422IN_CH1_0);
   hostSetParameter(PHX_CAMTRIG_ENCODER_MODE,   PHX_CAMTRIG_ENCODER_MODE4);
   hostSetParameter(PHX_CAMTRIG_MULTIPLIER,     500);
   hostSetParameter(PHX_CAMTRIG_DIVIDER,        500);

   hostSetParameter(PHX_TIMERM1_WIDTH ,         10); //<exposure time in us>

   hostSetParameter(PHX_FGTRIG_SRC ,            PHX_FGTRIG_SRC_422IN_CH3_0);
   hostSetParameter(PHX_FGTRIG_MODE ,           PHX_FGTRIG_EACH_POS_EDGE); 

   // This section configures the e2v camera to use CXP triggers
   deviceWrite(0x840C, 1);   // triggerpreset: triggered with fixed exposure time
   deviceWrite(0x8408, 49);  //  exposure time
   deviceWrite(0x8440, 1<<31 /*on*/| 3<<26 /*CXP*/| 0<<23 /*risingedge*/| 0<<16 /*delay*/); // exposure start

   {
      uint32_t temp;
      hostGetParameter(PHX_TIMERM1_WIDTH, temp);
   }
   hostSetParameter((etParam)(PHX_DUMMY_PARAM | PHX_CACHE_FLUSH), NULL);
}

void CCameraE2v::initFreeRunMode()
{
   deviceWrite(0x840C, 5);   // triggerpreset: freerun

   CCamera::initFreeRunMode();
}

void CCameraE2v::dumpCameraInfo()
{
   // device temperature
   printCameraIntInfo(0x8E04);
}
