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

class CCameraTeli :
   public CCamera
{
public:
   CCameraTeli(
                   struct Options_t & options
                 , HWND hwnd
      );

   virtual ~CCameraTeli(void);

private:
   virtual void setAcquisitionStart();
   virtual void setAcquisitionStop();
   virtual void setPixelFormat(enum CXP_PIXEL_FORMAT);
   virtual void setMiscellaneous();
   virtual void forceCameraOn();
   virtual void forceCameraOff();

   // Trigger Mode
   virtual void initTriggerMode1();
};