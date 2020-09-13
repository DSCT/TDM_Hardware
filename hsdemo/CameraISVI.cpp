/****************************************************************************
 *
 * ACTIVE SILICON LIMITED
 *
 * Authors     : Jean-Philippe Arnaud
 *
 * Copyright (c) 2000-2013 Active Silicon Ltd.
 ****************************************************************************
 */

#include "CameraISVI.h"

CCameraIsvi::CCameraIsvi(
                   struct Options_t & options
                 , HWND hwnd
           ) : 
            CCamera (options, hwnd)
           , m_TriggerSentCount(0)
           , m_timeoutCount(0)
           , m_isTimedOut(false)

{
   m_acquisitionModes.push_back("Software Frame Trigger (Camera Side)");
   if (!m_internalTrigger) m_acquisitionModes.push_back("TTL#0 Frame Trigger (Camera Side)");
   else m_acquisitionModes.push_back("Internal Frame Trigger (Grabber Side)");
}

CCameraIsvi::~CCameraIsvi(void)
{
}

void CCameraIsvi::setAcquisitionStart()
{
   m_BufferReceivedCount = 0; // Reset our test counter

   deviceWrite(0x6050, 1);

   if (m_acquisitionModeSelectedAtomic == 1) {
      m_TriggerSentCount = 0;
      sendTrigger();
   }
}

void CCameraIsvi::setAcquisitionStop()
{
   deviceWrite(0x6050, 0);
}

void CCameraIsvi::setPixelFormat(enum CXP_PIXEL_FORMAT format)
{
   if (format == CXP_PIXEL_FORMAT_BAYER_GR8
    || format == CXP_PIXEL_FORMAT_BAYER_RG8
    || format == CXP_PIXEL_FORMAT_BAYER_GB8
    || format == CXP_PIXEL_FORMAT_BAYER_BG8) {
      format = CXP_PIXEL_FORMAT_MONO8; // when set to Mono8, the camera applies a correction to R and B pixels. In Bayer format, no correction is applied
   }
   deviceWriteAndReadBack(0x6010, format);
}

void CCameraIsvi::setMiscellaneous()
{
   if (m_applyROI) { // we apply the frame grabber ROI (from the pcf file) to the camera and center the new camera ROI
      
      const uint32_t widthReg    = 0x6008;
      const uint32_t heightReg   = 0x600c;
      const uint32_t xOffsetReg  = 0x6000;
      const uint32_t yOffsetReg  = 0x6004;

      // width:   min 64, increment 64
      // height:  min 4,   increment 2 (actual increment value is 1 but set as multiple of 2 for Bayer camera)
      int32_t x_max, y_max, x, y;
      int32_t x_min = 64; int32_t x_inc = 64;
      int32_t y_min = 4;   int32_t y_inc = 2;

      deviceRead(widthReg,   x);
      deviceRead(heightReg,  y);
      x_max = (x & 0xFFFF0000) >> 16; // the value contains both the current width and the max width
      y_max = (y & 0xFFFF0000) >> 16; // the value contains both the current height and the max height

      // calculate new ROI, the function returns the new width and height in x and y
      calculateCameraROI(x_min, y_min, x_max, y_max, x_inc, y_inc, x, y);

      // calculate offsets to center camera ROI, the function returns the offsets in x_offset and y_offset
      int32_t x_offset, y_offset;
      int32_t x_offset_inc = 128;
      int32_t y_offset_inc = 2;     // actual value is 1 (multiple of 2 for Bayer camera)
      calculateCameraOffset(x, y, x_max, y_max, x_offset_inc, y_offset_inc, x_offset, y_offset);

      // set the new camera values
      deviceWrite(widthReg,     x);         // set the width
      deviceWrite(heightReg,    y);         // set the height
      deviceWrite(xOffsetReg,   x_offset);  // set x_offset
      deviceWrite(yOffsetReg,   y_offset);  // set y_offset

      Sleep(50);
      deviceWrite(heightReg,    y); // height doesn't seem to get applied first time it is written (but reading back the register gives the correct value)
   }
}

void CCameraIsvi::initTriggerMode1()
{
   const uint32_t acquisitionModeReg   = 0x6020; // Live: 0 - Free run: 1 - Trigger Master: 2 - Trigger Slave: 3
   const uint32_t triggerSourceReg     = 0x6024; // CXP: 0 - External: 1

   deviceWriteAndReadBack(acquisitionModeReg,  3);
   deviceWriteAndReadBack(triggerSourceReg,    0);

   // Frame Grabber trigger setup
   hostSetParameter(PHX_CAMTRIG_SRC,            PHX_CAMTRIG_SRC_SWTRIG_CHX);     // Configure FG to use software trigger for camera
   hostSetParameter(PHX_CAMTRIG_CXPTRIG_SRC,    PHX_CAMTRIG_CXPTRIG_TIMERM1_CHX);
   hostSetParameter(PHX_TIMERM1_WIDTH,          10000);
   hostSetParameter(PHX_CAMTRIG_CXPTRIG_MODE,   PHX_CAMTRIG_CXPTRIG_RISEFALL);

   hostSetParameter(PHX_FGTRIG_MODE,            PHX_FGTRIG_FREERUN);

   hostSetParameter(PHX_IO_TTLOUT_CHX,          PHX_IO_METHOD_BIT_TIMERM1_POS_CHX | 0x01);   // TTL output is set to Timer M1
}

void CCameraIsvi::initTriggerMode2()
{
   const uint32_t acquisitionModeReg   = 0x6020; // Live: 0 - Free run: 1 - Trigger Master: 2 - Trigger Slave: 3
   const uint32_t triggerSourceReg     = 0x6024; // CXP: 0 - External: 1

   deviceWriteAndReadBack(acquisitionModeReg,  3);
   deviceWriteAndReadBack(triggerSourceReg,    0);

   // Frame Grabber trigger setup
   if (!m_internalTrigger) {
      hostSetParameter(PHX_FGTRIG_SRC,       PHX_FGTRIG_SRC_TTLIN_CHX_0);  // Use TTL trigger
      hostSetParameter(PHX_CAMTRIG_SRC,      PHX_CAMTRIG_SRC_FGTRIGA_CHX);
   } else {
      hostSetParameter(PHX_CAMTRIG_SRC,      PHX_CAMTRIG_SRC_TIMERA1_CHX); // Use internal trigger
      hostSetParameter(PHX_TIMERA1_PERIOD,   10000);
   }

   hostSetParameter(PHX_CAMTRIG_CXPTRIG_SRC,    PHX_CAMTRIG_CXPTRIG_TIMERM1_CHX);   // Send triggers to camera via CXP
   hostSetParameter(PHX_TIMERM1_WIDTH,          7500);
   hostSetParameter(PHX_CAMTRIG_CXPTRIG_MODE,   PHX_CAMTRIG_CXPTRIG_RISEFALL);

   hostSetParameter(PHX_FGTRIG_MODE,            PHX_FGTRIG_FREERUN);

   hostSetParameter(PHX_IO_TTLOUT_CHX,          PHX_IO_METHOD_BIT_TIMERM1_POS_CHX | 0x01);   // TTL output is set to Timer M1
}

void CCameraIsvi::initFreeRunMode()
{
   const uint32_t acquisitionModeReg = 0x6020; // Live: 0 - Free run: 1 - Trigger Master: 2 - Trigger Slave: 3
   deviceWriteAndReadBack(acquisitionModeReg, 0);

   CCamera::initFreeRunMode();

   hostSetParameter(PHX_IO_TTLOUT_CHX, PHX_IO_METHOD_BIT_CLR | 0x01);
}


// bufferCallback() is used to implement the software trigger.
// BufferSuccessfullyAcquired [in]  true: a buffer was acquired
//                                  false: acquisition engine timedout before a buffer could be received (or there was an error)

void CCameraIsvi::bufferCallback(bool BufferSuccessfullyAcquired)
{
   if (!BufferSuccessfullyAcquired) {
      if (m_timeoutCount++ > 10 && !m_isTimedOut) {
         if (m_isCameraRunningAtomic && m_acquisitionModeSelectedAtomic == 1) { // only if camera is running and if software triggered mode
            LONG bufferReceivedCount = InterlockedCompareExchange(reinterpret_cast<volatile LONG *>(&m_BufferReceivedCount), 0, 0);
            SYSTEMTIME time;
            GetLocalTime(&time);
            printf("%4d-%02d-%02d %02d:%02d\n", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute);
            printf("Error: timed out while waiting for buffer. Received %d buffers / Sent %d triggers\n\n", m_BufferReceivedCount, m_TriggerSentCount);
            m_isTimedOut = true;
            sendTrigger(); // if a trigger is lost, resend one to continue acquisition
         } else m_timeoutCount = 0;
      }
   } else {
      if (m_acquisitionModeSelectedAtomic == 1) {
         sendTrigger();
      }
      m_timeoutCount = 0;
      m_isTimedOut = false;
   }
}


void CCameraIsvi::sendTrigger()
{
   PHX_StreamRead(handle(), PHX_EXPOSE, 0);
   m_TriggerSentCount++;
}