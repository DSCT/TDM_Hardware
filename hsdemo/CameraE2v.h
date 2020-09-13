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

class CCameraE2v :
   public CCamera
{
public:
   CCameraE2v(
                   struct Options_t & options
                 , HWND hwnd
      );

   virtual ~CCameraE2v(void);

private:
   virtual void setAcquisitionStart();
   virtual void setAcquisitionStop();
   virtual void setPixelFormat(enum CXP_PIXEL_FORMAT);
   virtual void setMiscellaneous();

   // Trigger modes
   virtual void initTriggerMode1();
   virtual void initTriggerMode2();
   virtual void initFreeRunMode();

   virtual uint32_t getPowerUpTimeMs() {return 70000;}
   virtual void dumpCameraInfo();
};