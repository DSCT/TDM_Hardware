#include "rs232.h"
#include <cuda_runtime.h>
#include <cufft.h>

extern int imgFrame;								//default number of frames
extern int ExpTime;
extern std::string FolderPath;

extern int FoV_size;

extern double *AO;

extern int ScanMode;					//used for identify the scanning mode
extern char *ChScannerX;				//channel text of X scanner
extern char *ChScannerY;				//channel text of Y scanner
extern char *ChScanners;				//channel of all devices
extern double Vol_X_max, Vol_Y_max;		//maximum voltages of X/Y scanner (for rotating)
extern double Vol_X_cur, Vol_Y_cur;		//current voltages of X/Y scanner
extern double PZT_Volt;

extern bool SHOT_Flag, Shutter532_Flag, ShutterMZ_Flag, ShutterWhiteLight_Flag, GratingWindow_Flag, PhaseUnwrapping_Flag, DichronicMirros_Flag, PumpPower_Flag;
extern HWND hPF_Type, hPF_ExpTime;
extern HWND hWnd_Main, hWnd_Setting;

extern int port_stage_xy, port_stage_z, port_stage_window;
extern RS232 PortStageXY, PortStageWindow, PortStageZ;
extern char *PosSX, *PosSY, *PosSZ, *PosWD;
extern int intervalWD;

// Estimate wrapped phase
extern int Nx, Ny, Nx2, Ny2;
extern float *SP_float;
extern cufftComplex *cuSP2;
extern float *SPWrapPhase2, *UnWrapPhaseSP2, *cuPhaseMap2, *cuPhaseMap;
extern unsigned char *tmp_uint8;

// Allocate global variables for phase reconstruction
extern float *LaplaceArray, *outX, *outY, *inX, *inY;
extern float *dst_dct;
extern cufftComplex *dSrc_tmp;
extern cufftHandle cuFFT1D_plan1_FORWARD;
extern cufftHandle cuFFT1D_plan2_FORWARD;
extern cufftHandle cuFFT1D_plan1_INVERSE;
extern cufftHandle cuFFT1D_plan2_INVERSE;

// used for time-lapse scanning
extern int TL_interval, TL_times, accumulate_time;
extern TCHAR StartTime[20], NextTime[20];
extern tm *now, *nextime;
extern time_t t_now, t_next;
extern bool TLScan_Flag;