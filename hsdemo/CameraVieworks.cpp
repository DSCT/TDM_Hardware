/****************************************************************************
 *
 * ACTIVE SILICON LIMITED
 *
 * Authors     : Jean-Philippe Arnaud
 *
 * Copyright (c) 2000-2013 Active Silicon Ltd.
 ****************************************************************************
 */

// !!! The demo targeting the Vieworks camera has been tested with a Mikrotron camera,
// !!! due to lack of Vieworks camera at the time of release.
// !!! Registers addresses and values need to be reviewed to match those of the Vieworks camera.

// !!! This demo shows 2 alternative ways to send a burst of camera triggers and pulses to TTL OUT #1:
// !!!  - initTriggerMode1() demonstrates how to trigger the camera from the software as soon as a Start
// !!!    of Frame Event is received.
// !!!  - initTriggerMode2() demonstrates a new feature of FireBird that allows programming TimerA1 to
// !!!    generate a burst of <n> pulses instead of free-running. The pulses are used to generate triggers
// !!!    to the camera and pulses on TTL OUT #1.

// Parameters to control the TimerA Burst mode will be included in phx_api.h
// in the future; for now we will directly access registers in the FPGA.
// Note that the addresses and values will change when this feature is officially
// supported.
#define PHX_ENABLE_TIMERA_BURST_ADDRESS 0x18018
#define PHX_ENABLE_TIMERA_BURST_VALUE(PulseCount) ((PulseCount << 8) | 1)
#define PHX_ENABLE_TIMERA_BURST_DISABLE (0)
#define PHX_ENABLE_TIMERA_START_ADDRESS 0x1801C
#define PHX_ENABLE_TIMERA_START_VALIDATE 1

#include "CameraVieworks.h"
#include "HostParameter.h"


//#define USE_MIKROTRON

CCameraVieworks::CCameraVieworks(
                   struct Options_t & options
                 , HWND hwnd
           ) : 
            CCamera (options, hwnd)

{
   m_acquisitionModes.push_back(ActiveSilicon::CASLException::msgComposer("Software Trigger using Frame Start Event (Camera Side)"));
   m_acquisitionModes.push_back(ActiveSilicon::CASLException::msgComposer("Acquisition Burst %d Frames Using TimerA Burst Mode (Camera Side)",		   
   m_recordBufferCount));
}

CCameraVieworks::~CCameraVieworks(void)
{
}

void CCameraVieworks::setAcquisitionStart()
{
#if defined USE_MIKROTRON
   deviceWrite(0x8204, 0);
#else
   deviceWrite(0x8044, 1);
#endif

   Sleep(100); // leave time for the camera to be ready

   if (m_acquisitionModeSelectedAtomic == 1) { // In Trigger Mode 1, send a software trigger
      PHX_StreamRead(handle(), PHX_EXPOSE, 0);
   } else if (m_acquisitionModeSelectedAtomic == 2) { // In Trigger Mode 3, send a trigger burst
      hostWriteWord(PHX_ENABLE_TIMERA_START_ADDRESS, PHX_ENABLE_TIMERA_START_VALIDATE);
   }
}

void CCameraVieworks::setAcquisitionStop()
{
#if defined USE_MIKROTRON
   deviceWrite(0x8208, 0);
#else
   deviceWrite(0x8048, 1);
#endif
}

void CCameraVieworks::setPixelFormat(enum CXP_PIXEL_FORMAT format)
{
#if defined USE_MIKROTRON
   deviceWriteAndReadBack(0x00008144, format);
#else
   deviceWriteAndReadBack(0x00008038, format);
#endif
}

void CCameraVieworks::setMiscellaneous()
{
#if defined USE_MIKROTRON
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
#else
   //0x4014 : CXP configuration mode
   //0x00040048 : 4 channels with 6 speed.
   deviceWrite(0x4014, 0x00040048);
#endif
}

// Trigger mode 1 sets the camera to be triggered via CXP by software triggers.
// A first software trigger is sent at acquisition start, and then fron the callback function,
// every time a PHX_INTRPT_FRAME_START interrupt is received.
void CCameraVieworks::initTriggerMode1()
{
	// Configure camera to accept triggers from CXP.
#if defined USE_MIKROTRON
	const uint32_t triggerSelectorReg = 0x8900; // FrameStart: 0 - FrameBurstStart: 1
	const uint32_t triggerModeReg = 0x8904; // On: 1 - Off: 0
	const uint32_t triggerSourceReg = 0x8908; // Software Trigger: 0 - CXP: 4
	const uint32_t exposureModeReg = 0x8944; // Width: 1 - Timed: 2
	const uint32_t exposureTimeReg = 0x8840; // in us

	deviceWriteAndReadBack(triggerSelectorReg, 0);
	deviceWriteAndReadBack(triggerModeReg, 1);
	deviceWriteAndReadBack(triggerSourceReg, 4);
	deviceWriteAndReadBack(exposureModeReg, 2);
	deviceWriteAndReadBack(exposureTimeReg, 200);
#else
	const uint32_t triggerModeReg = 0x10020044;  // On: 1 - Off: 0
	const uint32_t triggerSourceReg = 0x10020084;  // Software:1 - External: 5
	const uint32_t exposureModeReg = 0x10030004;  // Timed: 0 - Width: 1
	const uint32_t exposureTimeReg = 0x10030044;  // Raw exposure time

	deviceWriteAndReadBack(triggerModeReg, 1);
	deviceWriteAndReadBack(triggerSourceReg, 1);
	deviceWriteAndReadBack(exposureModeReg, 1);
#endif
	// Configure FireBird; you shouldn't need to change this code, except for PHX_TIMERA1_PERIOD and PHX_TIMERM1_WIDTH. 
	// Refer to comment "Acquisition Engine Extra Buffer" for explanation about +1
	hostSetParameter(PHX_IO_TTLOUT_CHX,  PHX_IO_METHOD_BIT_CLR | 1);// Pull TTL I/O down
	hostSetParameter(PHX_CAMTRIG_SRC,    PHX_CAMTRIG_SRC_NONE); // Stop generating triggers and pulses
	hostSetParameter(static_cast<etParam>(PHX_DUMMY_PARAM | PHX_CACHE_FLUSH),  0);   // Flush all parameters to FG

	hostWriteWord(PHX_ENABLE_TIMERA_BURST_ADDRESS, PHX_ENABLE_TIMERA_BURST_VALUE(0)); // Initialise burst count to zero to stop TimerA ticking
	hostWriteWord(PHX_ENABLE_TIMERA_START_ADDRESS, PHX_ENABLE_TIMERA_START_VALIDATE); // Validate burst count (should stop TimerA from ticking now)
	hostWriteWord(PHX_ENABLE_TIMERA_BURST_ADDRESS, PHX_ENABLE_TIMERA_BURST_VALUE(m_recordBufferCount)); // Initialise burst count to what we need

	hostSetParameter(PHX_CAMTRIG_SRC, PHX_CAMTRIG_SRC_TIMERA1_CHX);       // Set the frame grabber to use internal triggering for the camera
	hostSetParameter(PHX_TIMERA1_PERIOD, 300);                             // Set the period of the triggering (in us)

	//hostSetParameter(PHX_CAMTRIG_SRC,            PHX_CAMTRIG_SRC_SWTRIG_CHX);       // Set the frame grabber to use software triggering for the camera

	hostSetParameter(PHX_CAMTRIG_CXPTRIG_SRC ,   PHX_CAMTRIG_CXPTRIG_TIMERM1_CHX);   // Set the CXP trigger source
	hostSetParameter(PHX_TIMERM1_WIDTH, ExpTime);                             // Set the period of the triggering (in us)
	hostSetParameter(PHX_CAMTRIG_CXPTRIG_MODE,   PHX_CAMTRIG_CXPTRIG_RISEFALL);

	hostSetParameter(PHX_IO_TTLOUT_CHX,		      PHX_IO_METHOD_BIT_TIMERM1_POS_CHX | 1); // generate strobe signal NOT inverted from trigger
	//hostSetParameter(PHX_IO_TTLOUT_CHX,		      PHX_IO_METHOD_BIT_TIMERM1_NEG_CHX | 1); // generate strobe signal inverted from trigger

	hostSetParameter(PHX_FGTRIG_MODE,            PHX_FGTRIG_FREERUN);                // Set the frame grabber in free run as we trigger the camera directly
   
	hostSetParameter(static_cast<etParam>(PHX_DUMMY_PARAM | PHX_CACHE_FLUSH),  0);   // Flush all parameters to FG
	
	// Frame Start Event has to be enabled
	if (!m_FrameStartEventEnabledAtomic) {
		hostSetParameter(PHX_INTRPT_SET, PHX_INTRPT_FRAME_START);
		m_FrameStartEventEnabledAtomic = true;
	}
	m_FrameStartEventCount = 0; // reset count when enabling Trigger Mode 1
	
	startRecord();
}

// Trigger mode 1 sets the camera to be triggered via CXP by software triggers.
// A first software trigger is sent at acquisition start, and then fron the callback function,
// every time a PHX_INTRPT_FRAME_START interrupt is received.
void CCameraVieworks::initTriggerMode3()
{
   // Configure camera to accept triggers from CXP.
#if defined USE_MIKROTRON
   const uint32_t triggerSelectorReg   = 0x8900; // FrameStart: 0 - FrameBurstStart: 1
   const uint32_t triggerModeReg       = 0x8904; // On: 1 - Off: 0
   const uint32_t triggerSourceReg     = 0x8908; // Software Trigger: 0 - CXP: 4
   const uint32_t exposureModeReg      = 0x8944; // Width: 1 - Timed: 2
   const uint32_t exposureTimeReg      = 0x8840; // in us

   deviceWriteAndReadBack(triggerSelectorReg,  0);
   deviceWriteAndReadBack(triggerModeReg,      1);
   deviceWriteAndReadBack(triggerSourceReg,    4);
   deviceWriteAndReadBack(exposureModeReg,     2);
   deviceWriteAndReadBack(exposureTimeReg,     200);
#else
   const uint32_t triggerModeReg    = 0x10020044;  // On: 1 - Off: 0
   const uint32_t triggerSourceReg  = 0x10020084;  // Software:1 - External: 5
   const uint32_t exposureModeReg   = 0x10030004;  // Timed: 0 - Width: 1
   const uint32_t exposureTimeReg   = 0x10030044;  // Raw exposure time

   deviceWriteAndReadBack(triggerModeReg,    1);
   deviceWriteAndReadBack(triggerSourceReg,  1);
   deviceWriteAndReadBack(exposureModeReg,   1);
   //deviceWriteAndReadBack(exposureTimeReg,   500);
#endif
   // Configure FireBird; you shouldn't need to change this code, except for PHX_TIMERA1_PERIOD and PHX_TIMERM1_WIDTH. 
   // Refer to comment "Acquisition Engine Extra Buffer" for explanation about +1
   hostSetParameter(PHX_IO_TTLOUT_CHX,  PHX_IO_METHOD_BIT_CLR | 1);// Pull TTL I/O down
   hostSetParameter(PHX_CAMTRIG_SRC,    PHX_CAMTRIG_SRC_NONE); // Stop generating triggers and pulses
   hostSetParameter(static_cast<etParam>(PHX_DUMMY_PARAM | PHX_CACHE_FLUSH),  0);   // Flush all parameters to FG

   hostSetParameter(PHX_CAMTRIG_SRC,            PHX_CAMTRIG_SRC_SWTRIG_CHX);       // Set the frame grabber to use software triggering for the camera

   hostSetParameter(PHX_CAMTRIG_CXPTRIG_SRC,    PHX_CAMTRIG_CXPTRIG_TIMERM1_CHX);   // Set the CXP trigger source
   hostSetParameter(PHX_TIMERM1_WIDTH,          10000);                              // Trigger width
   hostSetParameter(PHX_CAMTRIG_CXPTRIG_MODE,   PHX_CAMTRIG_CXPTRIG_RISEFALL);

   hostSetParameter(PHX_IO_TTLOUT_CHX,		      PHX_IO_METHOD_BIT_TIMERM1_POS_CHX | 1); // generate strobe signal NOT inverted from trigger
   //hostSetParameter(PHX_IO_TTLOUT_CHX,		      PHX_IO_METHOD_BIT_TIMERM1_NEG_CHX | 1); // generate strobe signal inverted from trigger

   hostSetParameter(PHX_FGTRIG_MODE,            PHX_FGTRIG_FREERUN);                // Set the frame grabber in free run as we trigger the camera directly
   
   hostSetParameter(static_cast<etParam>(PHX_DUMMY_PARAM | PHX_CACHE_FLUSH),  0);   // Flush all parameters to FG

   // Frame Start Event has to be enabled
   if (!m_FrameStartEventEnabledAtomic) {
      hostSetParameter(PHX_INTRPT_SET, PHX_INTRPT_FRAME_START);
      m_FrameStartEventEnabledAtomic = true;
   }
   m_FrameStartEventCount = 0; // reset count when enabling Trigger Mode 1

   startRecord();
}

// Trigger mode 2 sets the camera to be triggered via CXP by internal source.
// When the camera receives the trigger, it then sends n buffers.
void CCameraVieworks::initTriggerMode2()
{
   // Configure camera to accept triggers from CXP.
#if defined USE_MIKROTRON
   const uint32_t triggerSelectorReg   = 0x8900; // FrameStart: 0 - FrameBurstStart: 1
   const uint32_t triggerModeReg       = 0x8904; // On: 1 - Off: 0
   const uint32_t triggerSourceReg     = 0x8908; // Software Trigger: 0 - CXP: 4
   const uint32_t exposureModeReg      = 0x8944; // Width: 1 - Timed: 2
   const uint32_t exposureTimeReg      = 0x8840; // in us

   deviceWriteAndReadBack(triggerSelectorReg,  0);
   deviceWriteAndReadBack(triggerModeReg,      1);
   deviceWriteAndReadBack(triggerSourceReg,    4);
   deviceWriteAndReadBack(exposureModeReg,     2);
   deviceWriteAndReadBack(exposureTimeReg,     500);
#else
   const uint32_t triggerModeReg    = 0x10020044;  // On: 1 - Off: 0
   const uint32_t triggerSourceReg  = 0x10020084;  // Software:1 - External: 5
   const uint32_t exposureModeReg   = 0x10030004;  // Timed: 0 - Width: 1
   const uint32_t exposureTimeReg   = 0x10030044;  // Raw exposure time

   deviceWriteAndReadBack(triggerModeReg,    1);
   deviceWriteAndReadBack(triggerSourceReg,  1);
   deviceWriteAndReadBack(exposureModeReg,   1);
   //deviceWriteAndReadBack(exposureTimeReg,   500);
#endif
   // Configure FireBird; you shouldn't need to change this code, except for PHX_TIMERA1_PERIOD and PHX_TIMERM1_WIDTH. 
   // Enable TimerA Burst Mode; the timer will generate a burst of m_recordBufferCount pulses
   // Refer to comment "Acquisition Engine Extra Buffer" for explanation about +1
   hostSetParameter(PHX_IO_TTLOUT_CHX,  PHX_IO_METHOD_BIT_CLR | 1);// Pull TTL I/O down
   hostSetParameter(PHX_CAMTRIG_SRC,    PHX_CAMTRIG_SRC_NONE); // Stop generating triggers and pulses
   hostSetParameter(static_cast<etParam>(PHX_DUMMY_PARAM | PHX_CACHE_FLUSH),  0);   // Flush all parameters to FG

   hostWriteWord(PHX_ENABLE_TIMERA_BURST_ADDRESS, PHX_ENABLE_TIMERA_BURST_VALUE(0)); // Initialise burst count to zero to stop TimerA ticking
   hostWriteWord(PHX_ENABLE_TIMERA_START_ADDRESS, PHX_ENABLE_TIMERA_START_VALIDATE); // Validate burst count (should stop TimerA from ticking now)
   hostWriteWord(PHX_ENABLE_TIMERA_BURST_ADDRESS, PHX_ENABLE_TIMERA_BURST_VALUE(m_recordBufferCount)); // Initialise burst count to what we need

   hostSetParameter(PHX_CAMTRIG_SRC,         PHX_CAMTRIG_SRC_TIMERA1_CHX);       // Set the frame grabber to use internal triggering for the camera
   hostSetParameter(PHX_TIMERA1_PERIOD,      5600);                             // Set the period of the triggering (in us)

   hostSetParameter(PHX_CAMTRIG_CXPTRIG_SRC,    PHX_CAMTRIG_CXPTRIG_TIMERM1_CHX);   // Set the CXP trigger source
   hostSetParameter(PHX_TIMERM1_WIDTH,          100);                              // Trigger width
   hostSetParameter(PHX_CAMTRIG_CXPTRIG_MODE,   PHX_CAMTRIG_CXPTRIG_RISEFALL);

   hostSetParameter(PHX_IO_TTLOUT_CHX,		      PHX_IO_METHOD_BIT_TIMERM1_POS_CHX | 1); // generate strobe signal NOT inverted from trigger
   //hostSetParameter(PHX_IO_TTLOUT_CHX,		      PHX_IO_METHOD_BIT_TIMERM1_NEG_CHX | 1); // generate strobe signal inverted from trigger

   hostSetParameter(PHX_FGTRIG_MODE,            PHX_FGTRIG_FREERUN);                // Set the frame grabber in free run as we trigger the camera directly
   
   hostSetParameter(static_cast<etParam>(PHX_DUMMY_PARAM | PHX_CACHE_FLUSH),  0);   // Flush all parameters to FG

   startRecord();
}

void CCameraVieworks::initFreeRunMode()
{
#if defined USE_MIKROTRON
   const uint32_t triggerSelectorReg   = 0x8900; // FrameStart: 0 - FrameBurstStart: 1
   const uint32_t triggerModeReg       = 0x8904; // On: 1 - Off: 0

   deviceWriteAndReadBack(triggerModeReg, 0);
   deviceWriteAndReadBack(triggerSelectorReg, 0);
#else
   const uint32_t triggerModeReg    = 0x10020044;  // Trigger Mode On: 1 - Trigger Mode Off: 0
   deviceWriteAndReadBack(triggerModeReg,    0);
#endif
   CCamera::initFreeRunMode();

   hostSetParameter(static_cast<etParam>(PHX_IO_TTLOUT_CHX | PHX_CACHE_FLUSH),  PHX_IO_METHOD_BIT_CLR | 1);// Pull TTL I/O down
   hostSetParameter(static_cast<etParam>(PHX_CAMTRIG_SRC | PHX_CACHE_FLUSH),    PHX_CAMTRIG_SRC_NONE); // Stop generating triggers and pulses
   hostWriteWord(PHX_ENABLE_TIMERA_BURST_ADDRESS, PHX_ENABLE_TIMERA_BURST_DISABLE);                    // Disable TimerA Burst mode
}

// In trigger mode 1, frameStartCallback() gets called at each Frame Start event
void CCameraVieworks::frameStartCallback()
{
   if (m_acquisitionModeSelectedAtomic == 1) {
      PHX_StreamRead(handle(), PHX_EXPOSE, 0);
   }
}
