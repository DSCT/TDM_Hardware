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

class CCameraAdimecSapphire :
   public CCamera
{
public:
   CCameraAdimecSapphire(
                   struct Options_t & options
                 , HWND hwnd
      );

   virtual ~CCameraAdimecSapphire(void);

private:
   virtual void setAcquisitionStart();
   virtual void setAcquisitionStop();
   virtual void setPixelFormat(enum CXP_PIXEL_FORMAT);
   virtual void setMiscellaneous();

   // Trigger modes
   virtual void initTriggerMode1();
   virtual void initFreeRunMode();
};