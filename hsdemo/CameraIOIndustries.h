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

class CCameraIOIndustries :
   public CCamera
{
public:
   CCameraIOIndustries(
                   struct Options_t & options
                 , HWND hwnd
      );

   virtual ~CCameraIOIndustries(void);

private:
   virtual void setAcquisitionStart();
   virtual void setAcquisitionStop();
   virtual void setPixelFormat(enum CXP_PIXEL_FORMAT);
   virtual void setMiscellaneous();

   // Trigger Mode
   virtual void initTriggerMode1();
};