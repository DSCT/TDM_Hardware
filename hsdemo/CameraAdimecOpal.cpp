/****************************************************************************
 *
 * ACTIVE SILICON LIMITED
 *
 * Authors     : Jean-Philippe Arnaud
 *
 * Copyright (c) 2000-2013 Active Silicon Ltd.
 ****************************************************************************
 */

#include "CameraAdimecOpal.h"

CCameraAdimecOpal::CCameraAdimecOpal(
                   struct Options_t & options
                 , HWND hwnd
           ) : 
            CCamera (options, hwnd)

{
   if (!m_internalTrigger) m_acquisitionModes.push_back("TTL#0 Frame Trigger (Grabber Side)");
   else m_acquisitionModes.push_back("Internal Frame Trigger (Grabber Side)");
}

CCameraAdimecOpal::~CCameraAdimecOpal(void)
{
}

void CCameraAdimecOpal::setAcquisitionStart()
{
   deviceWrite(0x00008204, 1);
}

void CCameraAdimecOpal::setAcquisitionStop()
{
   deviceWrite(0x00008208, 1);
}

void CCameraAdimecOpal::setPixelFormat(enum CXP_PIXEL_FORMAT format)
{
   deviceWrite(0x00008144, format);
   // do not read back the value as it will be Mono12 for the Opal Color, even if we write BayerGR12 to the register
}

void CCameraAdimecOpal::setMiscellaneous()
{
   // Matrix from Kodak for their CMOS KAC-9628 sensor
   m_fColorMatrix[0] = (float) 1.36; m_fColorMatrix[1] = (float)-0.22; m_fColorMatrix[2] = (float)-0.14;
   m_fColorMatrix[3] = (float)-0.25; m_fColorMatrix[4] = (float) 1.40; m_fColorMatrix[5] = (float)-0.15;
   m_fColorMatrix[6] = (float)-0.10; m_fColorMatrix[7] = (float)-0.32; m_fColorMatrix[8] = (float) 1.42;

   const uint32_t exposureTimeRawReg            = 0x00008258;
   const uint32_t acquisitionFramePeriodRawReg  = 0x00008220;

   deviceWrite(exposureTimeRawReg, 2000);
   deviceWrite(acquisitionFramePeriodRawReg, 0);
}

void CCameraAdimecOpal::initTriggerMode1()
{
   if (!m_internalTrigger) {
      hostSetParameter(PHX_FGTRIG_SRC,       PHX_FGTRIG_SRC_TTLIN_CHX_0);
   } else {
      hostSetParameter(PHX_FGTRIG_SRC,       PHX_FGTRIG_SRC_TIMERA1_CHX);
      hostSetParameter(PHX_TIMERA1_PERIOD,   20000);
   }

   hostSetParameter(PHX_FGTRIG_MODE, PHX_FGTRIG_FIRST_POS_EDGE);

   startRecord();
}
