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

class CCameraAvt :
   public CCamera
{
public:
   CCameraAvt(
                   struct Options_t & options
                 , HWND hwnd
      );

   virtual ~CCameraAvt(void);

private:
   virtual void setAcquisitionStart();
   virtual void setAcquisitionStop();
   virtual void setPixelFormat(enum CXP_PIXEL_FORMAT);
   virtual void setMiscellaneous();

   virtual void SetBitRateAndDiscovery(int lanes, int gbps);

   // Trigger Mode
   virtual void initTriggerMode1();
};