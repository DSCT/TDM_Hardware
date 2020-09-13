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
//#include "HostParameter.h"
class CCameraVieworks :
   public CCamera
{
public:
   CCameraVieworks(
                    struct Options_t & options
                 , HWND hwnd
      );

   virtual ~CCameraVieworks(void);

private:
   virtual void setAcquisitionStart();
   virtual void setAcquisitionStop();
   virtual void setPixelFormat(enum CXP_PIXEL_FORMAT);
   virtual void setMiscellaneous();

   // Trigger Mode
   virtual void initTriggerMode1();
   virtual void initTriggerMode2();
   virtual void initTriggerMode3();
   virtual void initFreeRunMode();
   virtual void frameStartCallback();
};
