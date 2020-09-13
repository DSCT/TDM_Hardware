/****************************************************************************
 *
 * ACTIVE SILICON LIMITED
 *
 * Authors     : Jean-Philippe Arnaud
 *
 * Copyright (c) 2000-2013 Active Silicon Ltd.
 ****************************************************************************
 */

#include "CameraMikrotron.h"

CCameraMikrotron::CCameraMikrotron(
                   struct Options_t & options
                 , HWND hwnd
           ) : 
            CCamera (options, hwnd)

{
   if (!m_internalTrigger) {
      m_acquisitionModes.push_back("TTL#0 Frame Trigger (Grabber Side)");
      m_acquisitionModes.push_back("TTL#0 Frame Trigger (Camera Side)");
   } else {
      m_acquisitionModes.push_back("Internal Frame Trigger (Grabber Side)");
      m_acquisitionModes.push_back("Internal Frame Trigger (Camera Side)");
   }
   m_acquisitionModes.push_back(ActiveSilicon::CASLException::msgComposer("Acquisition Burst %d Frames (Camera Side)", m_recordBufferCount));
}

CCameraMikrotron::~CCameraMikrotron(void)
{
}

void CCameraMikrotron::setAcquisitionStart()
{
   deviceWrite(0x8204, 0);
}

void CCameraMikrotron::setAcquisitionStop()
{
   deviceWrite(0x8208, 0);
}

void CCameraMikrotron::setPixelFormat(enum CXP_PIXEL_FORMAT format)
{
   deviceWriteAndReadBack(0x00008144, format);
}

void CCameraMikrotron::setMiscellaneous()
{
   if (m_applyROI) { // we apply the frame grabber ROI (from the pcf file) to the camera and center the new camera ROI

      const uint32_t sensorWidthReg    = 0x8808;
      const uint32_t sensorHeightReg   = 0x880c;
      const uint32_t widthReg          = 0x8118;
      const uint32_t heightReg         = 0x811c;
      const uint32_t xOffsetReg        = 0x8800;
      const uint32_t yOffsetReg        = 0x8804;
      // Mikrotron max dimensions depend on the camera offsets therefore we start by setting the offsets to zero
      deviceWriteAndReadBack(xOffsetReg, 0); // x offset
      deviceWriteAndReadBack(yOffsetReg, 0); // y offset

      // width:   min 16,  increment 16
      // height:  min 2,   increment 2
      int32_t x_max, y_max, x, y;
      int32_t x_min = 16;  int32_t x_inc = 16;
      int32_t y_min = 2;   int32_t y_inc = 2;

      deviceRead(sensorWidthReg,   x_max); // sensor width
      deviceRead(sensorHeightReg,  y_max); // sensor height

      // calculate new ROI, the function returns the new width and height in x and y
      calculateCameraROI(x_min, y_min, x_max, y_max, x_inc, y_inc, x, y);

      // calculate offsets to center camera ROI, the function returns the offsets in x_offset and y_offset
      int32_t x_offset, y_offset;
      int32_t x_offset_inc = 16;
      int32_t y_offset_inc = 2;
      calculateCameraOffset(x, y, x_max, y_max, x_offset_inc, y_offset_inc, x_offset, y_offset);

      // set camera values
      deviceWriteAndReadBack(widthReg,   x);          // set the width
      deviceWriteAndReadBack(heightReg,  y);          // set the height
      deviceWriteAndReadBack(xOffsetReg, x_offset);   // set x_offset
      deviceWriteAndReadBack(yOffsetReg, y_offset);   // set y_offset
   }

   const uint32_t cameraFrameRateReg     = 0x8814;
   const uint32_t cameraFrameRateMaxReg  = 0x881C;
   const uint32_t cameraExposureModeReg  = 0x8944;

   int32_t MaxFrameRate;
   deviceRead(cameraFrameRateMaxReg, MaxFrameRate);

   deviceWriteAndReadBack(cameraExposureModeReg,  1);
   deviceWriteAndReadBack(cameraFrameRateReg,     MaxFrameRate-2); // the camera does not seem to work at max framerate (sometimes only)

   // Enable pixel reset and pulse drain for trigger modes
   const uint32_t prstEnableReg           = 0x9200;
   const uint32_t pulseDrainEnable        = 0x9204;
   deviceWriteAndReadBack(prstEnableReg,           1);
   deviceWriteAndReadBack(pulseDrainEnable,        1);
}

// Trigger mode 1 sets the Frame Grabber to be triggered by an external (TTL) or internal source
void CCameraMikrotron::initTriggerMode1()
{
   if (!m_internalTrigger) {
      hostSetParameter(PHX_FGTRIG_SRC,       PHX_FGTRIG_SRC_TTLIN_CHX_0);
   } else {
      hostSetParameter(PHX_FGTRIG_SRC,       PHX_FGTRIG_SRC_TIMERA1_CHX);
      hostSetParameter(PHX_TIMERA1_PERIOD,   5000);
   }

   hostSetParameter(PHX_FGTRIG_MODE,   PHX_FGTRIG_FIRST_POS_EDGE);

   startRecord();
}

// Trigger mode 2 sets the camera to be triggered via CXP by an external (TTL) or internal source
void CCameraMikrotron::initTriggerMode2()
{
   const uint32_t triggerModeReg       = 0x8904; // On: 1 - Off: 0
   const uint32_t triggerSourceReg     = 0x8908; // Software Trigger: 0 - CXP: 4
   const uint32_t exposureModeReg      = 0x8944; // Width: 1 - Timed: 2
   const uint32_t exposureTimeReg      = 0x8840; // in us

   deviceWriteAndReadBack(triggerModeReg,       1);
   deviceWriteAndReadBack(triggerSourceReg,     4);
   deviceWriteAndReadBack(exposureModeReg,      2);   // Exposure must be timed
   deviceWriteAndReadBack(exposureTimeReg,      500); // Set a fixed exposure time

   if (!m_internalTrigger) {
      hostSetParameter(PHX_FGTRIG_SRC,          PHX_FGTRIG_SRC_TTLIN_CHX_0);        // Set the frame grabber to use external TTL triggers
      hostSetParameter(PHX_CAMTRIG_SRC,         PHX_CAMTRIG_SRC_FGTRIGA_CHX);       // Use the triggers as source to the camera triggering
   } else {
      hostSetParameter(PHX_CAMTRIG_SRC,         PHX_CAMTRIG_SRC_TIMERA1_CHX);       // Set the frame grabber to use internal triggering for the camera
      hostSetParameter(PHX_TIMERA1_PERIOD,      5000);                              // Set the period of the triggering
   }
   hostSetParameter(PHX_CAMTRIG_CXPTRIG_SRC,    PHX_CAMTRIG_CXPTRIG_TIMERM1_CHX);   // Set the CXP trigger source
   hostSetParameter(PHX_TIMERM1_WIDTH,          500);                               // Trigger width
   hostSetParameter(PHX_CAMTRIG_CXPTRIG_MODE,   PHX_CAMTRIG_CXPTRIG_RISEFALL);

   hostSetParameter(PHX_FGTRIG_MODE,            PHX_FGTRIG_FREERUN);                // Set the frame grabber in free run as we trigger the camera directly
}

// Trigger mode 3 sets the camera to be triggered via CXP by an external (TTL) or internal source.
// When the camera receives the trigger, it then sends n buffers.
void CCameraMikrotron::initTriggerMode3()
{
   const uint32_t triggerSelectorReg   = 0x8900; // FrameStart: 0 - FrameBurstStart: 1
   const uint32_t triggerModeReg       = 0x8904; // On: 1 - Off: 0
   const uint32_t triggerSourceReg     = 0x8908; // Software Trigger: 0 - CXP: 4
   const uint32_t exposureModeReg      = 0x8944; // Width: 1 - Timed: 2
   const uint32_t exposureTimeReg      = 0x8840; // in us
   const uint32_t framesPerTriggerReg  = 0x8914; // number of frames generates by camera during an acquisition run

   deviceWriteAndReadBack(triggerSelectorReg,  1);
   deviceWriteAndReadBack(triggerModeReg,      1);
   deviceWriteAndReadBack(triggerSourceReg,    4);
   deviceWriteAndReadBack(exposureModeReg,     2);   // exposure must be timed
   deviceWriteAndReadBack(exposureTimeReg,     500);
   deviceWriteAndReadBack(framesPerTriggerReg, m_recordBufferCount + 1);   // number of images the camera will generate for each trigger it receives.
                                                                           // Note: the camera ignores new triggers until all images have been sent.
                                                                           //       So there must be enough time between two triggers for the camera
                                                                           //       to send all those images.
                                                                           // +1 because of an issue in the firmware of the frame grabber (B.2.F and B.3.6)

   if (!m_internalTrigger) {
      hostSetParameter(PHX_FGTRIG_SRC,          PHX_FGTRIG_SRC_TTLIN_CHX_0);        // Set the frame grabber to use external TTL triggers
      hostSetParameter(PHX_CAMTRIG_SRC,         PHX_CAMTRIG_SRC_FGTRIGA_CHX);       // Use the triggers as source to the camera triggering
   } else {
      hostSetParameter(PHX_CAMTRIG_SRC,         PHX_CAMTRIG_SRC_TIMERA1_CHX);       // Set the frame grabber to use internal triggering for the camera
      hostSetParameter(PHX_TIMERA1_PERIOD,      1000000);                           // Set the period of the triggering (in us)
   }
   hostSetParameter(PHX_CAMTRIG_CXPTRIG_SRC,    PHX_CAMTRIG_CXPTRIG_TIMERM1_CHX);   // Set the CXP trigger source
   hostSetParameter(PHX_TIMERM1_WIDTH,          500);                               // Trigger width
   hostSetParameter(PHX_CAMTRIG_CXPTRIG_MODE,   PHX_CAMTRIG_CXPTRIG_RISEFALL);

   hostSetParameter(PHX_FGTRIG_MODE,            PHX_FGTRIG_FREERUN);                // Set the frame grabber in free run as we trigger the camera directly

   startRecord();
}

void CCameraMikrotron::initFreeRunMode()
{
   const uint32_t triggerSelectorReg   = 0x8900; // FrameStart: 0 - FrameBurstStart: 1
   const uint32_t triggerModeReg       = 0x8904; // On: 1 - Off: 0

   deviceWriteAndReadBack(triggerModeReg, 0);
   deviceWriteAndReadBack(triggerSelectorReg, 0);

   CCamera::initFreeRunMode();
}
