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

class CCameraAdimecOpal :
   public CCamera
{
public:
   CCameraAdimecOpal(
                   struct Options_t & options
                 , HWND hwnd
      );

   virtual ~CCameraAdimecOpal(void);

private:
   virtual void setAcquisitionStart();
   virtual void setAcquisitionStop();
   virtual void setPixelFormat(enum CXP_PIXEL_FORMAT);
   virtual void setMiscellaneous();

   // Trigger Mode
   virtual void initTriggerMode1();
};