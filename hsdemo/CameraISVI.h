/****************************************************************************
 *
 * ACTIVE SILICON LIMITED
 *
 * Authors     : Jean-Philippe Arnaud
 *
 * Copyright (c) 2000-2013 Active Silicon Ltd.
 ****************************************************************************
 */

#pragma once
#include "camera.h"

class CCameraIsvi :
public CCamera
{
public:
   CCameraIsvi(
                   struct Options_t & options
                 , HWND hwnd
      );

   virtual ~CCameraIsvi(void);

private:
   virtual void setAcquisitionStart();
   virtual void setAcquisitionStop();
   virtual void setPixelFormat(enum CXP_PIXEL_FORMAT);
   virtual void setMiscellaneous();

   // Trigger modes
   virtual void initTriggerMode1();
   virtual void initTriggerMode2();
   virtual void initFreeRunMode();
   virtual void bufferCallback(bool);

   void sendTrigger();

   uint32_t m_TriggerSentCount;
   uint32_t m_timeoutCount;
   bool     m_isTimedOut;
};