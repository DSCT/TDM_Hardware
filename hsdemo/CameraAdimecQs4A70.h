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

class CCameraAdimecQs4A70 :
   public CCamera
{
public:
   CCameraAdimecQs4A70(
                   struct Options_t & options
                 , HWND hwnd
      );

   virtual ~CCameraAdimecQs4A70(void);

private:
   virtual void setAcquisitionStart();
   virtual void setAcquisitionStop();
   virtual void setPixelFormat(enum CXP_PIXEL_FORMAT);
   virtual void setMiscellaneous();

   virtual void SetBitRateAndDiscovery(int lanes, int gbps);

   // Trigger Mode
   virtual void initTriggerMode1();
};