/****************************************************************************
 *
 * ACTIVE SILICON LIMITED
 *
 * Authors     : Jean-Philippe Arnaud
 *
 * Copyright (c) 2000-2013 Active Silicon Ltd.
 ****************************************************************************
 * Comments:
 * --------
 * A simple example demonstrating the FireBird frame grabber with a number
 * of cameras. Refer to readme.txt included in the package for detailed
 * description of functionnality.
 ****************************************************************************
 */

#include <stdio.h>
#include <tchar.h>
#include <windows.h>
#include <windowsx.h>
#include <process.h>
#include <string>
#include <vector>
#include <ctime>
#include <strstream>
#include <ShlObj.h>
#include <direct.h>

#include "PhotonFocus.h"
#include "PhaseExtraction.cuh"
#include "resource.h"
#include "commandline.h"
#include "cameravieworks.h"
#include "NI_device.h"
#include "rs232.h"
#include "Stage.h"
#include "HostParameter.h"
#include "..\common\frametimeoutcalculator.h"

#include <shlobj.h>
#define NO_WIN32_LEAN_AND_MEAN
#define BIF_NEWDIALOGSTYLE 0x0040

#define _METHOD_BRUTE_FORCE     1
#define _METHOD_MULTIMEDIA      2

//#define _METHOD _METHOD_BRUTE_FORCE
#define _METHOD _METHOD_MULTIMEDIA

#define ID_TIMER	101

//setting window Ctrl
HWND hFrames, hExpTime;
HWND hVolXmax, hVolYmax;
HWND hVolXcur, hVolYcur;
HWND hFanStatus, hShutter532Status, hShutterMZStatus, hShutterBBStatus, hGWStatus, hPUStatus, hDMStatus, hPumpStatus;
HWND hPosX, hPosY, hPosZ, hPosWD;
HWND hIntervalEdit, hScanNoEdit, hNxNumEdit, hCurTimeEdit, hNxScanEdit;
HWND hDate, hSeries, hSaveType, hFolder, hName, hPF_Type, hPF_ExpTime;

HWND hCtrl;
char ShutterMode[20];

int port_stage_xy, port_stage_z, port_stage_window;
bool  StatusScanX, StatusScanY;					//used for checking the status of Scanner
int FanFlag;									//a flag for recording the status
int PatternFlag;
int ScanningFlag;
//bool Shutter532, ShutterMZ, ShutterBB;
int ExpTime, imgFrame;
std::string FolderPath;

int FoV_size;	//6: IDNO, no(1024); 7: IDYES, yes(3072)

//define NI device global parameters
TaskHandle TaskX, TaskY, TaskScan, TaskCounter;
int SaveType;																								//the image type
int ScanMode;																								//used for identify the scanning mode
char *ChScannerX, *ChScannerY, *ChScanners;																	//channel for Scanner X, Y and BOTH
char *ChShutter532_A, *ChShutter532_B, *ChShutterMZ, *ChShutterWhiteLight_A, *ChShutterWhiteLight_B, *ChPumping;		//channel for Shutters
char *ChGratingWindow, *ChDM;																			//channel for Servor Motors
double Vol_X_max, Vol_Y_max;		//maximum voltages of X/Y scanner (for rotating)
double Vol_X_cur, Vol_Y_cur, PZT_Volt;		//current voltages of X/Y scanner

double *AO;
//double *AO_array, *PZT_Volt;					//voltage array for Analog output
HWND hWnd_Main, hWnd_Setting;										//the window of setting properties
char *PosSX= (char *)malloc(30*sizeof(char));
char *PosSY= (char *)malloc(30*sizeof(char));
char *PosSZ= (char *)malloc(30*sizeof(char));
char *PosWD = (char *)malloc(30 * sizeof(char));
bool SHOT_Flag, Shutter532_Flag, ShutterMZ_Flag, ShutterWhiteLight_Flag, GratingWindow_Flag, PhaseUnwrapping_Flag, DichronicMirros_Flag, PumpPower_Flag;		//check manual/motor control
RS232 PortStageXY, PortStageWindow, PortStageZ;
int intervalWD;

// Estimate wrapped phase
int Nx, Ny, Nx2, Ny2;
float *SP_float;
cufftComplex *cuSP2;
float *SPWrapPhase2, *UnWrapPhaseSP2, *cuPhaseMap2, *cuPhaseMap;
unsigned char *tmp_uint8;

// Allocate global variables for phase reconstruction
float *LaplaceArray, *outX, *outY, *inX, *inY;
float *dst_dct;
cufftComplex *dSrc_tmp;
cufftHandle cuFFT1D_plan1_FORWARD;
cufftHandle cuFFT1D_plan2_FORWARD;
cufftHandle cuFFT1D_plan1_INVERSE;
cufftHandle cuFFT1D_plan2_INVERSE;

// Scanning time
int TL_interval, TL_times, accumulate_time;
TCHAR StartTime[20], NextTime[20];
tm *now, *start, *nextime;
time_t t_now, t_start, t_next, t_scan;
bool TLScan_Flag;

using namespace std;
using namespace ActiveSilicon;

CCamera::Options_t options;
bool ParseCommandLine(int argc, _TCHAR* argv[], CCamera::Options_t & options);
void RegisterRedPanelClass(void);
LRESULT CALLBACK PanelProc(HWND, UINT, WPARAM, LPARAM);

#ifndef WIN64
#ifdef _DEBUG
#pragma comment (lib, "phxlw32.lib")
#pragma comment (lib, "TmgPhx32.lib")
#pragma comment (lib, "adl_x32.lib")
#else
#pragma comment (lib, "phxlw32.lib")
#pragma comment (lib, "TmgPhx32.lib")
#pragma comment (lib, "adl_x32.lib")
#endif
#else
#ifdef _DEBUG
#pragma comment (lib, "phxlx64.lib")
#pragma comment (lib, "TmgPhx64.lib")
#pragma comment (lib, "adl_x64.lib")
#else
#pragma comment (lib, "phxlx64.lib")
#pragma comment (lib, "TmgPhx64.lib")
#pragma comment (lib, "adl_x64.lib")
#endif
#endif

#define ACTIVE_SILICON_TEST_MODULE     1
#define ADIMEC_OPAL_2000C              2
#define ADIMEC_OPAL_2000M              3
#define ADIMEC_SAPPHIRE                4
#define ADIMEC_QS_4A70                 5
#define ADIMEC_QUARTZ_4A180            6
#define AVT_BONITO_CL_400B             7
#define CAMERA_LINK                    8
#define DREAMPACT                      9
#define E2V_ELIIXA_16K                 10
#define E2V_ELIIXA_8K_COLOR            11
#define IOINDUSTRIES_4M140             12
#define IMPERX                         13
#define ISVI_C25                       14
#define ISVI_M25                       15
#define JAI_SP_5000M                   16
#define MIKROTRON_MC4086               17
#define NED_XCM80160                   18
#define OPTRONIS_CL4000                19
#define OPTRONIS_CL25000               20
#define SENSOR2IMAGE                   21
#define SONY                           22
#define TELI                           23
#define VIEWORKS                       24


// Following is use when automatically building the demo for all supported cameras
#if defined _ADIMEC_OPAL_2000C
   #define CAMERA ADIMEC_OPAL_2000C
#elif defined _ADIMEC_OPAL_2000M
   #define CAMERA ADIMEC_OPAL_2000M
#elif defined _ADIMEC_SAPPHIRE
   #define CAMERA ADIMEC_SAPPHIRE
#elif defined _ADIMEC_QS_4A70
   #define CAMERA ADIMEC_QS_4A70
#elif defined _ADIMEC_QUARTZ_4A180
   #define CAMERA ADIMEC_QUARTZ_4A180
#elif defined _AVT_BONITO_CL_400B
   #define CAMERA AVT_BONITO_CL_400B
#elif defined _CAMERA_LINK
   #define CAMERA CAMERA_LINK
#elif defined _DREAMPACT
   #define CAMERA DREAMPACT
#elif defined _E2V_ELIIXA_16K
   #define CAMERA E2V_ELIIXA_16K
#elif defined _E2V_ELIIXA_8K_COLOR
   #define CAMERA E2V_ELIIXA_8K_COLOR
#elif defined _IOINDUSTRIES_4M140
   #define CAMERA IOINDUSTRIES_4M140
#elif defined _IMPERX_ICX_MONO
   #define CAMERA IMPERX
#elif defined _ISVI_IC_C25CXP
   #define CAMERA ISVI_C25
#elif defined _ISVI_IC_M25CXP
   #define CAMERA ISVI_M25
#elif defined _JAI_SP_5000M
   #define CAMERA JAI_SP_5000M
#elif defined _MIKROTRON_MC4086
   #define CAMERA MIKROTRON_MC4086
#elif defined _NED_XCM80160_CXP
   #define CAMERA NED_XCM80160
#elif defined _OPTRONIS_CL4000
   #define CAMERA OPTRONIS_CL4000
#elif defined _OPTRONIS_CL25000
   #define CAMERA OPTRONIS_CL25000
#elif defined _SENSOR2IMAGE
   #define CAMERA SENSOR2IMAGE
#elif defined _SONY_FCB
   #define CAMERA SONY
#elif defined _TELI_EXPRESS_DRAGON
   #define CAMERA TELI
#elif defined _VIEWORKS
   #define CAMERA VIEWORKS
#endif

// Manually define the camera to use
#if !defined CAMERA
   #define CAMERA VIEWORKS
#endif

#if CAMERA==ACTIVE_SILICON_TEST_MODULE
   #define PCF IDR_PCF_ASL_TEST_MODULE
   #define DISPLAY_NAME "Active Silicon Test Module"
   #define LANES 0
   #define GBPS 0
   #define BAYER_COLOR_CORRECTION 0
#elif CAMERA==ADIMEC_OPAL_2000C
   #define PCF IDR_PCF_OPAL_2000C
   #define DISPLAY_NAME "Adimec Opal 2000 Color"
   #define LANES 0
   #define GBPS 2
   #define BAYER_COLOR_CORRECTION 1
#elif CAMERA==ADIMEC_OPAL_2000M
   #define PCF IDR_PCF_OPAL_2000M
   #define DISPLAY_NAME "Adimec Opal 2000 Mono"
   #define LANES 0
   #define GBPS 2
   #define BAYER_COLOR_CORRECTION 0
#elif CAMERA==ADIMEC_SAPPHIRE
   #define PCF IDR_PCF_SAPPHIRE
   #define DISPLAY_NAME "Adimec Sapphire S-25A45-Em"
   #define LANES 0
   #define GBPS 6
   #define BAYER_COLOR_CORRECTION 0
#elif CAMERA==ADIMEC_QS_4A70
   #define PCF IDR_PCF_QS_4A70
   #define DISPLAY_NAME "Adimec Qs-4A70"
   #define LANES 0
   #define GBPS 0
   #define BAYER_COLOR_CORRECTION 0
#elif CAMERA==ADIMEC_QUARTZ_4A180
   #define PCF IDR_PCF_QUARTZ_4A180m
   #define DISPLAY_NAME "Adimec Quartz Q-4A180m"
   #define LANES 0
   #define GBPS 0
   #define BAYER_COLOR_CORRECTION 0
#elif CAMERA==AVT_BONITO_CL_400B
   #define PCF IDR_PCF_AVT_BONITO_CL_400B
   #define DISPLAY_NAME "AVT Bonito CL 400B"
   #define LANES 0
   #define GBPS 0
   #define BAYER_COLOR_CORRECTION 0
#elif CAMERA==CAMERA_LINK
   #define PCF IDR_PCF_CAMERA_LINK
   #define DISPLAY_NAME "Camera Link"
   #define LANES 0
   #define GBPS 0
   #define BAYER_COLOR_CORRECTION 0
#elif CAMERA==DREAMPACT
   #define PCF IDR_PCF_DREAMPACT
   #define DISPLAY_NAME "Dreampact Test Module"
   #define LANES 0
   #define GBPS 5
   #define BAYER_COLOR_CORRECTION 0
#elif CAMERA==E2V_ELIIXA_16K
   #define PCF IDR_PCF_E2V_ELIIXA_PLUS
   #define DISPLAY_NAME "e2v ELiiXA+ LineScan (16384 pixels / 8 bits)"
   #define LANES 0
   #define GBPS 0
   #define BAYER_COLOR_CORRECTION 0
#elif CAMERA==E2V_ELIIXA_8K_COLOR
   #define PCF IDR_PCF_E2V_ELIIXA_PLUS_COLOR
   #define DISPLAY_NAME "e2v ELiiXA+ LineScan Color (8192 pixels / RGB 24 bits)"
   #define LANES 0
   #define GBPS 0
   #define BAYER_COLOR_CORRECTION 0
#elif CAMERA==IOINDUSTRIES_4M140
   #define PCF IDR_PCF_IOINDUSTRIES_4M140
   #define DISPLAY_NAME "IO Industries Flare 4M140 CCX"
   #define LANES 0
   #define GBPS 0
   #define BAYER_COLOR_CORRECTION 0
#elif CAMERA==IMPERX
   #define PCF IDR_PCF_IMPERX
   #define DISPLAY_NAME "Imperx Bobcat CXP"
   #define LANES 0
   #define GBPS 0
   #define BAYER_COLOR_CORRECTION 0
#elif CAMERA==ISVI_C25
   #define PCF IDR_PCF_ISVI_C25_CXP
   #define DISPLAY_NAME "ISVI IC-C25CXP"
   #define LANES 0
   #define GBPS 5
   #define BAYER_COLOR_CORRECTION 0
#elif CAMERA==ISVI_M25
   #define PCF IDR_PCF_ISVI_M25_CXP
   #define DISPLAY_NAME "ISVI IC-M25CXP"
   #define LANES 0
   #define GBPS 5
   #define BAYER_COLOR_CORRECTION 0
#elif CAMERA==JAI_SP_5000M
   #define PCF IDR_PCF_JAI_SP_5000M_CXP
   #define DISPLAY_NAME "JAI SP 5000M CXP"
   #define LANES 0
   #define GBPS 0
   #define BAYER_COLOR_CORRECTION 0
#elif CAMERA==MIKROTRON_MC4086
   #define PCF IDR_PCF_MIKROTRON
   #define DISPLAY_NAME "Mikrotron MC4086 Mono"
   #define LANES 0
   #define GBPS 6
   #define BAYER_COLOR_CORRECTION 0
#elif CAMERA==NED_XCM80160
   #define PCF IDR_PCF_NED_XCM80160
   #define DISPLAY_NAME "NED XCM80160 CXP"
   #define LANES 0
   #define GBPS 0
   #define BAYER_COLOR_CORRECTION 0
#elif CAMERA==OPTRONIS_CL4000
   #define PCF IDR_PCF_OPTRONIS_CL4000
   #define DISPLAY_NAME "Optronis CL4000 CXP"
   #define LANES 0
   #define GBPS 0
   #define BAYER_COLOR_CORRECTION 0
#elif CAMERA==OPTRONIS_CL25000
   #define PCF IDR_PCF_OPTRONIS_CL25000
   #define DISPLAY_NAME "Optronis CL25000 CXP"
   #define LANES 0
   #define GBPS 6
   #define BAYER_COLOR_CORRECTION 0
#elif CAMERA==SENSOR2IMAGE
   #define PCF IDR_PCF_SENSOR2IMAGE
   #define DISPLAY_NAME "Sensor2Image (752 x 480)"
   #define LANES 0
   #define GBPS 0
   #define BAYER_COLOR_CORRECTION 0
#elif CAMERA==SONY
   #define PCF IDR_PCF_SONY
   #define DISPLAY_NAME "Sony FCB H11"
   #define LANES 0
   #define GBPS 0
   #define BAYER_COLOR_CORRECTION 0
#elif CAMERA==TELI
   #define PCF IDR_PCF_TELI
   #define DISPLAY_NAME "Toshiba Teli Express Dragon"
   #define LANES 0
   #define GBPS 0
   #define BAYER_COLOR_CORRECTION 0
#elif CAMERA==VIEWORKS
   #define PCF IDR_PCF_VIEWORKS
   #define DISPLAY_NAME "Vieworks 12M"
   #define LANES 0
   #define GBPS 6
   #define BAYER_COLOR_CORRECTION 0
#else
#error Unknown camera or not camera defined
#endif

#pragma message( "Compiling for camera: " DISPLAY_NAME) 

void CALLBACK TimerFunction(  UINT uTimerID
                            , UINT uMsg
                            , DWORD_PTR dwUser
                            , DWORD_PTR dw1
                            , DWORD_PTR dw2
                            )
{
   CCamera * camera = reinterpret_cast<CCamera *>(dwUser);
   //if (camera) RedrawWindow(camera->getWindowHandle() , NULL , NULL , RDW_INVALIDATE);
   if (camera) RedrawWindow(hWnd_Main, NULL, NULL, RDW_INVALIDATE);
}





HMENU hMenubar;
HMENU hMenu1, hMenu2, hMenu3;
HMENU hMenuCP, hMenuMZ;
void AddMenus(HWND hWnd) 
{
	hMenubar = CreateMenu();
	hMenu1   = CreateMenu();
	hMenu2   = CreateMenu();
	hMenu3   = CreateMenu();
	hMenuCP  = CreateMenu();
	hMenuMZ  = CreateMenu();
	
	//menu1
	AppendMenuW(hMenubar, MF_POPUP    , (UINT_PTR)hMenu1, L"&File"); 
	AppendMenuW(hMenu1  , MF_STRING   , IDM_FILE_NEW, L"&New");
	AppendMenuW(hMenu1  , MF_STRING   , IDM_FILE_OPEN, L"&Open");
	AppendMenuW(hMenu1  , MF_SEPARATOR, 0, NULL);
	AppendMenuW(hMenu1  , MF_STRING   , IDM_FILE_QUIT, L"&Quit");
	
	//menu2	
	AppendMenuW(hMenubar, MF_POPUP    , (UINT_PTR)hMenu2, L"&Preview");
	AppendMenuW(hMenu2  , MF_STRING   , IDM_PRE_RUNX    , L"&Rotate ScannerX");
	AppendMenuW(hMenu2  , MF_STRING   , IDM_PRE_RUNY    , L"&Rotate ScannerY");
	AppendMenuW(hMenu2  , MF_STRING   , IDM_PRE_RUNXY   , L"&Rotate Scanners");
	AppendMenuW(hMenu2  , MF_SEPARATOR, 0, NULL);
	AppendMenuW(hMenu2  , MF_STRING   , IDM_PRE_FIXX    , L"&Fix ScannerX");
	AppendMenuW(hMenu2  , MF_STRING   , IDM_PRE_FIXY    , L"&Fix ScannerY");
	AppendMenuW(hMenu2  , MF_SEPARATOR, 0, NULL);
	AppendMenuW(hMenu2  , MF_STRING   , IDM_PRE_ZERO    , L"&Set to Zero");
	AppendMenuW(hMenu2  , MF_SEPARATOR, 0, NULL);
	AppendMenuW(hMenu2  , MF_STRING   , IDM_PRE_PROP     , L"&Property");

	//menu of common-path
	AppendMenuW(hMenuCP  , MF_STRING   , IDM_MODE_CP_X	 , L"&Single X-Axis");
	AppendMenuW(hMenuCP  , MF_STRING   , IDM_MODE_CP_Y	 , L"&Single Y-Axis");
	AppendMenuW(hMenuCP  , MF_STRING   , IDM_MODE_CP_DUAL, L"&Dual Axis");

	//menu of Mack-Zehnder
	AppendMenuW(hMenuMZ  , MF_STRING   , IDM_MODE_MZ_X	 , L"&Single X-Axis");
	AppendMenuW(hMenuMZ  , MF_STRING   , IDM_MODE_MZ_Y	 , L"&Single Y-Axis");
	AppendMenuW(hMenuMZ  , MF_STRING   , IDM_MODE_MZ_DUAL, L"&Dual Axis");

	//menu3
	AppendMenuW(hMenubar, MF_POPUP    , (UINT_PTR)hMenu3, L"&Mode");
	InsertMenuW(hMenu3  , 1, MF_BYPOSITION|MF_POPUP|MF_STRING , (UINT_PTR)hMenuCP   , L"&Common-Path");
	InsertMenuW(hMenu3  , 1, MF_BYPOSITION|MF_POPUP|MF_STRING , (UINT_PTR)hMenuMZ   , L"&Mach-Zehnder");
	
	SetMenu(hWnd, hMenubar);
}

void uncheckPreScanMenu()
{
	CheckMenuItem(hMenu2, IDM_PRE_RUNX , MF_BYCOMMAND | MF_UNCHECKED);
	CheckMenuItem(hMenu2, IDM_PRE_RUNY , MF_BYCOMMAND | MF_UNCHECKED);
	CheckMenuItem(hMenu2, IDM_PRE_RUNXY, MF_BYCOMMAND | MF_UNCHECKED);
}

void uncheckScanMode()
{
	CheckMenuItem(hMenuCP, IDM_MODE_CP_X   , MF_BYCOMMAND | MF_UNCHECKED);
	CheckMenuItem(hMenuCP, IDM_MODE_CP_Y   , MF_BYCOMMAND | MF_UNCHECKED);
	CheckMenuItem(hMenuCP, IDM_MODE_CP_DUAL, MF_BYCOMMAND | MF_UNCHECKED);

	CheckMenuItem(hMenuMZ, IDM_MODE_MZ_X   , MF_BYCOMMAND | MF_UNCHECKED);
	CheckMenuItem(hMenuMZ, IDM_MODE_MZ_Y   , MF_BYCOMMAND | MF_UNCHECKED);
	CheckMenuItem(hMenuMZ, IDM_MODE_MZ_DUAL, MF_BYCOMMAND | MF_UNCHECKED);
}

void InitialNI_Parameters()
{
	StatusScanX = false;
	StatusScanY = false;
	FanFlag = 1;
	PatternFlag = 0;
	ScanningFlag = 0;
	SHOT_Flag = true;
	Shutter532_Flag = false;
	ShutterMZ_Flag = true;
	ShutterWhiteLight_Flag = false;
	GratingWindow_Flag = true;
	PhaseUnwrapping_Flag = false;
	DichronicMirros_Flag = false;
	TLScan_Flag = false;
	imgFrame = 500;
	ExpTime = 200;

	SaveType = 0;
	ScanMode = 1;							//used for identify the scanning mode

	port_stage_xy = 2;
	port_stage_window = 3;
	port_stage_z = 4;
	
	ChScannerX   = "Dev1/ao2";				//channel text of X scanner
	ChScannerY   = "Dev1/ao3";				//channel text of Y scanner
	ChScanners = "Dev1/ao2:3";				//channel of all devices
	ChShutter532_A = "Dev1/port0/line2";
	ChShutter532_B = "Dev1/port0/line1";
	ChShutterWhiteLight_A = "Dev1/port0/line3";
	ChShutterWhiteLight_B = "Dev1/port0/line4";
	ChShutterMZ = "Dev1/port0/line5";	
	ChPumping = "Dev1/port0/line0";
	ChGratingWindow = "Dev1/ctr1";	
	ChDM = "Dev1/ctr2";

	Vol_X_max = 3.1, Vol_Y_max = 1.5;		//maximum voltages of X/Y scanner (for rotating)
	Vol_X_cur = 0.0, Vol_Y_cur = 0.0;		//current voltages of X/Y scanner
	PZT_Volt  = 0.0;
	FolderPath = "D:\\";

	intervalWD = 0;
}

void createPathString()
{
	TCHAR Location[100];
	GetDlgItemText(hWnd_Setting, IDC_FOLDER_EDIT, Location, sizeof(Location) / sizeof(TCHAR));
	TCHAR Date[12];
	GetDlgItemText(hWnd_Setting, IDC_DATE_EDIT, Date, sizeof(Date) / sizeof(TCHAR));
	TCHAR Series[5];
	GetDlgItemText(hWnd_Setting, IDC_NUMBER_EDIT, Series, sizeof(Series) / sizeof(TCHAR));
	TCHAR Name[20];
	GetDlgItemText(hWnd_Setting, IDC_NAME_EDIT, Name, sizeof(Name) / sizeof(TCHAR));
	SaveType = SendMessage(hSaveType , CB_GETCURSEL, 0, 0 );
	//TCHAR TestT[20];
	//SendMessage(hSaveType, CB_GETLBTEXT, SaveType, (LPARAM)TestT);
	TCHAR *Type;
	if(SaveType == 0) 
		Type = _T("SP");
	else if (SaveType == 1)
		Type = _T("BG");
	else if (SaveType == 2)
		Type = _T("Ang");
	else if (SaveType == 3)
		Type = _T("AngX");
	else if (SaveType == 4)
		Type = _T("AngY");

	char path[256];

	sprintf(path,"%s\\%s",Location,Date);
	if(_access(path,0) == -1)	_mkdir(path);

	sprintf(path,"%s\\%s\\%s",Location,Date,Name);
	if(_access(path,0) == -1)	_mkdir(path);

	sprintf(path, "%s\\%s\\%s\\%s", Location, Date, Name, Series);
	if (_access(path, 0) == -1)	_mkdir(path);

	sprintf(path, "%s\\%s\\%s\\%s\\%s", Location, Date, Name, Series, Type);
	if (_access(path, 0) == -1)	_mkdir(path);

	FolderPath = path;

	if (TLScan_Flag) {
		sprintf(path, "%s\\time-lapse", path);
		if (_access(path, 0) == -1)	_mkdir(path);

		sprintf(path, "%s\\%d", path, accumulate_time);
		if (_access(path, 0) == -1)	_mkdir(path);

		FolderPath = path;
	}
	
	printf("\nCreate: %s\n",path);
}

void changeEditText(HWND hwnd, int resourceCode, char *str)
{
	HWND hChangeEdit = GetDlgItem(hwnd, resourceCode);
	SetWindowText(hChangeEdit, str);
}

void OnToggleMenuState(HWND hWnd)
{
	// Get the popup menu which contains the "Toggle State" menu item.
	HMENU hMenu = GetMenu(hWnd);
	//ModifyMenu(hMenu,1,MF_STRING,101,"被更動了！");
	//EnableMenuItem(hMenu,CM_RESTART,MF_GRAYED);
	SetMenu(hWnd, hMenu);
}

/*LRESULT CALLBACK PanelProc( HWND hwnd, UINT msg, 
    WPARAM wParam, LPARAM lParam )
{
  switch(msg)  
  {
    case WM_LBUTTONUP:
    
      Beep(50, 40);
      break;    
  }

  return DefWindowProcW(hwnd, msg, wParam, lParam);
}*/

static int CALLBACK BrowseCallbackProc(HWND hwnd,UINT uMsg, LPARAM lParam, LPARAM lpData)
{

    if(uMsg == BFFM_INITIALIZED)
    {
        std::string tmp = (const char *) lpData;
        //std::cout << "path: " << tmp << std::endl;
        SendMessage(hwnd, BFFM_SETSELECTION, TRUE, lpData);
    }

    return 0;
}

std::string BrowseFolder(std::string saved_path)
{
    TCHAR path[MAX_PATH];
	
    const char * path_param = saved_path.c_str();
	
    BROWSEINFO bi = { 0 };
    bi.lpszTitle  = ("Browse for folder...");
	bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_DONTGOBELOWDOMAIN | BIF_RETURNFSANCESTORS;
	//bi.ulFlags =   BIF_NEWDIALOGSTYLE;
	//bi.ulFlags = BIF_NEWDIALOGSTYLE | BIF_UAHINT | 0x1;
    bi.lpfn       = BrowseCallbackProc;
    bi.lParam     = (LPARAM) path_param;
	
	//OleInitialize(NULL);
	//CoInitialize(NULL);
    LPITEMIDLIST pidl = SHBrowseForFolder ( &bi );

    if ( pidl != 0 )
    {
        //get the name of the folder and put it in path
        SHGetPathFromIDList ( pidl, path );

        //free memory used
        IMalloc * imalloc = 0;
        if ( SUCCEEDED( SHGetMalloc ( &imalloc )) )
        {
            imalloc->Free ( pidl );
            imalloc->Release ( );
        }

        return path;
    }

    return "C:\\";
}

std::string GetFolder()
{
    std::string strReturnPath; 
    LPMALLOC pMalloc; 
    BROWSEINFO bi; 
    char pszBuffer[MAX_PATH]; 
    LPITEMIDLIST pidl; 
    // Gets the Shell's default allocator 
    if (::SHGetMalloc(&pMalloc) == NOERROR) 
    {   //bi.hwndOwner = AfxGetMainWnd()->GetSafeHwnd(); 
        bi.pidlRoot = NULL; 
        bi.pszDisplayName = pszBuffer; 
        bi.lpszTitle = ("Browse for folder..."); 
        bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE; 
        bi.lpfn = NULL; 
        bi.lParam = 0; 
        // Pop up the directory picker dialog box. 
        if ((pidl = ::SHBrowseForFolder(&bi)) != NULL) 
        {   if (::SHGetPathFromIDList(pidl, pszBuffer)) 
            {   // Return the selected path. 
                strReturnPath = pszBuffer; 
            } 
            // Free the PIDL allocated by SHBrowseForFolder. 
            pMalloc->Free(pidl); 
        } 
        // Release the shell's allocator. 
        pMalloc->Release(); 
    } 
    return strReturnPath; 
}



LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
   LRESULT lReturn = false;
   try {

      CCamera * camera = reinterpret_cast<CCamera *>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

	   switch(msg) {
	   case WM_CREATE:
		    SetTimer(hwnd, ID_TIMER, 1000, NULL);

			InitialNI_Parameters();
			AddMenus(hwnd);
			CheckMenuItem(hMenuCP, IDM_MODE_CP_X, MF_BYCOMMAND | MF_CHECKED);

			ServorMotor(ChDM, 50, 0.118, 1000);
			Sleep(2000);
			ServorMotor(ChGratingWindow, 50, 0.0725, 150);			

			PortStageXY.OpenComport(port_stage_xy, 9600, "Stage XY");
			//PortStageZ.OpenComport(port_stage_z, 38400, "Stage Z");
			PortStageWindow.OpenComport(port_stage_window, 9600, "Stage Window");
			break;

        case WM_CLOSE:         
		   //PortStageXY.CloseComport(port_stage_xy);
		   //PortStageZ.CloseComport(port_stage_z);
		   //PortStageWindow.CloseComport(port_stage_window);
		   PostQuitMessage(0);
		   KillTimer(hWnd_Main, ID_TIMER);
           break;

		case WM_DESTROY:
		   //PortStageXY.CloseComport(port_stage_xy);
		   //PortStageZ.CloseComport(port_stage_z);
		   //PortStageWindow.CloseComport(port_stage_window);
		   PostQuitMessage(0);
		   KillTimer(hWnd_Main, ID_TIMER);
		   break;

         case WM_TIMER:
			 switch (wParam)
			 {
				case 1:
					if (camera) {
						camera->paint();
						RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE);

						
					}
					break;
				case ID_TIMER:
					if (TLScan_Flag) {
						t_now = std::time(0);
						now = std::localtime(&t_now);
						sprintf(NextTime, "%02d:%02d:%02d", now->tm_hour, now->tm_min, now->tm_sec);
						changeEditText(hWnd_Setting, IDC_CURTIME_EDIT, NextTime);

						if (t_now == t_next && accumulate_time < TL_times)
						{													
							t_next += TL_interval;
							nextime = std::localtime(&t_next);
							sprintf(NextTime, "#%d", accumulate_time+1);
							changeEditText(hWnd_Setting, IDC_NXNUM_EDIT, NextTime);
							sprintf(NextTime, "%02d:%02d:%02d", nextime->tm_hour, nextime->tm_min, nextime->tm_sec);								
							changeEditText(hWnd_Setting, IDC_NXSCAN_EDIT, NextTime);	

							accumulate_time += 1;
							cout << "#" << accumulate_time << ": " << NextTime << endl;

							PostMessage(hWnd_Main, WM_KEYDOWN, VK_F7, 0);						
							if (accumulate_time == TL_times) {
								changeEditText(hWnd_Setting, IDC_NXNUM_EDIT, "##");
								changeEditText(hWnd_Setting, IDC_NXSCAN_EDIT, "FINISH");
							}
						}
						else if (t_next - t_now == 55 && accumulate_time == TL_times) {
							changeEditText(hWnd_Setting, IDC_NXNUM_EDIT, "#-");
							changeEditText(hWnd_Setting, IDC_NXSCAN_EDIT, "--:--:--");
							hCtrl = GetDlgItem(hWnd_Setting, IDC_TLSCAN_BUTTON);
							SetWindowText(hCtrl, "Start");
							TLScan_Flag = false;
						}
						
					}

					if (ScanningFlag == 1) {
						
						if (std::time(0) - t_scan == 1 && camera) {
							createPathString();

							AO = (double *)realloc(AO, imgFrame * 3 * sizeof(double));
							PreparationScan2(AO, Vol_X_max, Vol_Y_max, ScanMode, imgFrame);
							SetToVoltage(ChScanners, AO[0], AO[1], AO[2]);
							// stop the camera fan
							if (FanFlag != 0 && camera)	camera->FanOpertaor((uint32_t)0);

							if (ScanMode == 3) {							
								TCHAR posWD_txt[10];
								GetDlgItemText(hWnd_Setting, IDC_STAGE_WD_EDIT, posWD_txt, sizeof(posWD_txt) / sizeof(TCHAR));
								float posWD = atof(posWD_txt);
								intervalWD = int(posWD / (imgFrame/2))*(-1);
							}
							else{
								intervalWD = 0;
							}

							// turn on the laser
							if (!Shutter532_Flag){
								Switch2Relay(ChShutter532_A, ChShutter532_B, 0);
								Shutter532_Flag = true;
								sprintf(ShutterMode, "ON (1)");
								hCtrl = GetDlgItem(hWnd_Setting, IDC_SHUTTER532_EDIT);
								SetWindowText(hCtrl, ShutterMode);
							}

							// turn off the pump for air and hot water
							if (PumpPower_Flag){
								SwitchRelay(ChPumping, true);
								PumpPower_Flag = false;
								hCtrl = GetDlgItem(hWnd_Setting, IDC_PUMP_EDIT);
								SetWindowText(hCtrl, "OFF (1)");
							}

							// make sure the status of the window, grating and shutters
							if (SaveType != 2)	//Angle
							{
								if (!ShutterMZ_Flag)
								{
									ShutterMZ_Flag = true;
									sprintf(ShutterMode, "ON (1)");
									Switch1Relay(ChShutterMZ);
									hCtrl = GetDlgItem(hWnd_Setting, IDC_SHUTTER_MZ_EDIT);
									SetWindowText(hCtrl, ShutterMode);
								}
								if (ShutterWhiteLight_Flag)
								{
									Switch2Relay(ChShutterWhiteLight_A, ChShutterWhiteLight_B, 0);
									ShutterWhiteLight_Flag = false;
									sprintf(ShutterMode, "OFF (0)");
									hCtrl = GetDlgItem(hWnd_Setting, IDC_SHUTTER_BB_EDIT);
									SetWindowText(hCtrl, ShutterMode);
								}
								if (!GratingWindow_Flag)
								{
									ServorMotor(ChGratingWindow, 50, 0.0725, 150);
									GratingWindow_Flag = true;
									hCtrl = GetDlgItem(hWnd_Setting, IDC_SWITCH_GW_EDIT);
									SetWindowText(hCtrl, "ON (1)");
								}
							}
							else{
								if (ShutterMZ_Flag)
								{
									ShutterMZ_Flag = false;
									sprintf(ShutterMode, "OFF (0)");
									Switch1Relay(ChShutterMZ);
									hCtrl = GetDlgItem(hWnd_Setting, IDC_SHUTTER_MZ_EDIT);
									SetWindowText(hCtrl, ShutterMode);
								}
								if (GratingWindow_Flag)
								{
									ServorMotor(ChGratingWindow, 50, 0.125, 150);
									GratingWindow_Flag = false;
									hCtrl = GetDlgItem(hWnd_Setting, IDC_SWITCH_GW_EDIT);
									SetWindowText(hCtrl, "OFF (0)");
								}
							}
						}

						// start acquire (10 sec)
						if (std::time(0) - t_scan == 10)
							camera->toggleTriggerModeOnOff(1);

						// deal other components (15 sec)
						if (std::time(0) - t_scan == 15) {
							ScanningFlag = 0;
							if (FanFlag != 0 && camera)	camera->FanOpertaor((uint32_t)FanFlag);

							// close the shutter of laser light
							if (Shutter532_Flag){
								Switch2Relay(ChShutter532_A, ChShutter532_B, 1);
								Shutter532_Flag = false;
								sprintf(ShutterMode, "OFF (0)");
								hCtrl = GetDlgItem(hWnd_Setting, IDC_SHUTTER532_EDIT);
								SetWindowText(hCtrl, ShutterMode);
							}

							// turn on the pumps
							if (!PumpPower_Flag){
								SwitchRelay(ChPumping, false);
								PumpPower_Flag = true;
								hCtrl = GetDlgItem(hWnd_Setting, IDC_PUMP_EDIT);
								SetWindowText(hCtrl, "ON (1)");
							}

							// move the stage of window to original position at dual mirror scan mode
							if (ScanMode == 3)
							{
								TCHAR posWD_txt[10];
								GetDlgItemText(hWnd_Setting, IDC_STAGE_WD_EDIT, posWD_txt, sizeof(posWD_txt) / sizeof(TCHAR));
								float posWD = atof(posWD_txt);
								MoveRead_SHOT(PortStageWindow, port_stage_window, "2", int(posWD), PosSX, PosWD);
								changeEditText(hWnd_Setting, IDC_STAGE_WD_EDIT, PosWD);
							}
						}
					}

					break;
				default:
					break;
			 } break;

		 case WM_PAINT:
			 {
				PAINTSTRUCT ps;
				HDC hdc = BeginPaint(hwnd, &ps);
				EndPaint(hwnd, &ps);
				if (camera) camera->paint(); // only call paint() here so that when the window is minimized, paint() will not be called

				

			 } break;
		 
		 case WM_COMMAND:
			 switch(LOWORD(wParam))
			 {
				case IDM_FILE_NEW:
					options.recordBufferCount = 50;
					MessageBox(0, "IDM_FILE_NEW", "show", MB_OK);
					int argc;
					_TCHAR* argv[2];
					ParseCommandLine(argc, argv,options);
					break;
				
				case IDM_FILE_OPEN:
					/*RegisterRedPanelClass();
					CreateWindowW(L"RedPanelClass", NULL, 
					   WS_CHILD | WS_VISIBLE,
					   20, 20, 80, 80,
					   hwnd, (HMENU) 1, NULL, NULL);
					Beep(50, 100);*/
					break;
				
				case IDM_FILE_QUIT:
					SendMessage(hwnd, WM_CLOSE, 0, 0);
					break;
				
				case IDM_PRE_RUNX:
						uncheckPreScanMenu();
						if(StatusScanY == true)
						{
							StopScanner(TaskY);
							StatusScanY = false;
						}
						if(StatusScanX == false)
						{						
							TCHAR txt_X_max[10];
							GetDlgItemText(hWnd_Setting, IDC_VXMAX_EDIT, txt_X_max, sizeof(txt_X_max) / sizeof(TCHAR));
							Vol_X_max = atof(txt_X_max);
							//printf("maxY:%.3f",Vol_X_max);
							CheckMenuItem(hMenu2, IDM_PRE_RUNX , MF_BYCOMMAND | MF_CHECKED);
							StatusScanX = true;
							RotateScanner(TaskX, ChScannerX, 0.2, Vol_X_max);
						}
						else if(StatusScanX == true)
						{
							StopScanner(TaskX);
							StatusScanX = false;
						}						
					break;

				case IDM_PRE_RUNY:
						uncheckPreScanMenu();
						if(StatusScanX == true)
						{
							StopScanner(TaskX);
							StatusScanX = false;
						}
						if(StatusScanY == false)
						{
							TCHAR txt_Y_max[10];
							GetDlgItemText(hWnd_Setting, IDC_VYMAX_EDIT, txt_Y_max, sizeof(txt_Y_max) / sizeof(TCHAR));
							Vol_Y_max = atof(txt_Y_max);
							//printf("maxY:%.3f",Vol_Y_max);
							CheckMenuItem(hMenu2, IDM_PRE_RUNY , MF_BYCOMMAND | MF_CHECKED);
							StatusScanY = true;
							RotateScanner(TaskY, ChScannerY, 0.5, Vol_Y_max);
						}
						else if(StatusScanY == true)
						{
							StopScanner(TaskY);
							StatusScanY = false;
						}
						
					break;

				case IDM_PRE_RUNXY:
						if(StatusScanY == false)
						{
							CheckMenuItem(hMenu2, IDM_PRE_RUNXY , MF_BYCOMMAND | MF_CHECKED);
						}
						else
						{
							uncheckPreScanMenu();
						}
						break;

				case IDM_PRE_FIXX:
					uncheckPreScanMenu();
					if(StatusScanX == true)
					{
						StopScanner(TaskX);
						SetToVoltage(ChScanners, Vol_X_cur, 0, 0);
						StatusScanX = false;
					}
					else if(StatusScanX == false)
					{
						SetToVoltage(ChScanners, Vol_X_cur, 0, 0);
					}
					break;

				case IDM_PRE_FIXY:
					uncheckPreScanMenu();
					if(StatusScanY == true)
					{
						StopScanner(TaskY);
						SetToVoltage(ChScanners, 0, Vol_Y_cur, 0);
						StatusScanY = false;
					}
					else if(StatusScanY == false)
					{
						SetToVoltage(ChScanners, 0, Vol_Y_cur, 0);
					}
					break;

				case IDM_PRE_ZERO:
					if(StatusScanX == true)	{StopScanner(TaskX);	StatusScanX = false;}
					if(StatusScanY == true)	{StopScanner(TaskY);	StatusScanY = false;}
					SetToVoltage(ChScanners, 0, 0, 0);
					uncheckPreScanMenu();
					break;

				case IDM_PRE_PROP:
					ShowWindow(hWnd_Setting, SW_SHOW);
					break;

				case IDM_MODE_CP_X:
					ScanMode = 1;
					imgFrame = 500;
					SetDlgItemText(hWnd_Setting, IDC_FRAM_EDIT, "500");
					uncheckScanMode();
					CheckMenuItem(hMenuCP, IDM_MODE_CP_X , MF_BYCOMMAND | MF_CHECKED);
					if(StatusScanX == true)	{StopScanner(TaskX);	StatusScanX = false;}
					if(StatusScanY == true)	{StopScanner(TaskY);	StatusScanY = false;}
					break;

				case IDM_MODE_CP_Y:
					ScanMode = 2;
					imgFrame = 500;
					SetDlgItemText(hWnd_Setting, IDC_FRAM_EDIT, "500");
					uncheckScanMode();
					CheckMenuItem(hMenuCP, IDM_MODE_CP_Y , MF_BYCOMMAND | MF_CHECKED);
					if(StatusScanX == true)	{StopScanner(TaskX);	StatusScanX = false;}
					if(StatusScanY == true)	{StopScanner(TaskY);	StatusScanY = false;}
					break;

				case IDM_MODE_CP_DUAL:
					ScanMode = 3;
					imgFrame = 529;
					SetDlgItemText(hWnd_Setting, IDC_FRAM_EDIT, "529");
					uncheckScanMode();
					CheckMenuItem(hMenuCP, IDM_MODE_CP_DUAL , MF_BYCOMMAND | MF_CHECKED);
					if(StatusScanX == true)	{StopScanner(TaskX);	StatusScanX = false;}
					if(StatusScanY == true)	{StopScanner(TaskY);	StatusScanY = false;}
					break;

				case IDM_MODE_MZ_X:
					ScanMode = 4;
					imgFrame = 500;
					SetDlgItemText(hWnd_Setting, IDC_FRAM_EDIT, "500");
					uncheckScanMode();
					CheckMenuItem(hMenuMZ, IDM_MODE_MZ_X , MF_BYCOMMAND | MF_CHECKED);
					if(StatusScanX == true)	{StopScanner(TaskX);	StatusScanX = false;}
					if(StatusScanY == true)	{StopScanner(TaskY);	StatusScanY = false;}
					break;

				case IDM_MODE_MZ_Y:
					ScanMode = 5;
					imgFrame = 500;
					SetDlgItemText(hWnd_Setting, IDC_FRAM_EDIT, "500");
					uncheckScanMode();
					CheckMenuItem(hMenuMZ, IDM_MODE_MZ_Y , MF_BYCOMMAND | MF_CHECKED);
					if(StatusScanX == true)	{StopScanner(TaskX);	StatusScanX = false;}
					if(StatusScanY == true)	{StopScanner(TaskY);	StatusScanY = false;}
					break;

				case IDM_MODE_MZ_DUAL:
					ScanMode = 6;
					imgFrame = 529;
					SetDlgItemText(hWnd_Setting, IDC_FRAM_EDIT, "529");
					uncheckScanMode();
					CheckMenuItem(hMenuMZ, IDM_MODE_MZ_DUAL , MF_BYCOMMAND | MF_CHECKED);
					if(StatusScanX == true)	{StopScanner(TaskX);	StatusScanX = false;}
					if(StatusScanY == true)	{StopScanner(TaskY);	StatusScanY = false;}
					break;
				
			 }
			 break;

		// The keys match the text displayed in the text() function in the camera base class
		case WM_KEYDOWN:
			{
				switch(wParam)
				{
					case VK_ESCAPE:
						PostQuitMessage(0);
						break;
					
					case VK_F1:
					case '1':
						if (camera) camera->toggleFitToDisplayOnOff();
						break;
					
					case VK_F3:
					case '3':
						if (camera) camera->toggleDisplayOnOff();
						break;
					
					case VK_F4:
					case '4':
						{
							createPathString();

							SYSTEMTIME time;
							char filename[4096];
							GetLocalTime(&time);
							sprintf_s(filename, sizeof(filename), "img_%4d_%02d_%02d_%02d_%02d_%02d", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);
							
							if (camera)
							{
								//refer to "CCamera::paint()"
								camera->saveImageToFile(std::string(filename));
							}
						} break;
					
					case VK_F5:
					case '5':
						if (camera) camera->nextRecordFormat();
						break;

					// Trigger modes are defined in each derived camera class
					// F7, F8 and F9 will only be used if the corresponding trigger modes are implemented
					// Free run is implemented in the camera base class and is mode 0.
					case VK_F6:
					case '6':
						if (camera) camera->toggleTriggerModeOnOff(0);
						break;
					
					case VK_F7:
					case '7':
						if (ScanningFlag == 0)	{
							ScanningFlag = 1;
							t_scan = std::time(0);
						}
						/*{
							createPathString();

							AO = (double *)realloc(AO,imgFrame*3*sizeof(double));
							PreparationScan2(AO, Vol_X_max, Vol_Y_max,	ScanMode, imgFrame);
							SetToVoltage(ChScanners, AO[0], AO[1], AO[2]);
							// stop the camera fan
							if (FanFlag != 0 && camera)	camera->FanOpertaor((uint32_t)FanFlag);

							if (!Shutter532_Flag){
								Switch2Relay(ChShutter532_A, ChShutter532_B, 0);
								Shutter532_Flag = true;
								sprintf(ShutterMode, "ON (1)");
								hCtrl = GetDlgItem(hWnd_Setting, IDC_SHUTTER532_EDIT);
								SetWindowText(hCtrl, ShutterMode);
							}

							// make sure the status of the window, grating and shutters
							if (SaveType != 2)
							{								
								if (!ShutterMZ_Flag)
								{ 
									ShutterMZ_Flag = true;
									sprintf(ShutterMode, "ON (1)");
									Switch1Relay(ChShutterMZ);
									hCtrl = GetDlgItem(hWnd_Setting, IDC_SHUTTER_MZ_EDIT);
									SetWindowText(hCtrl, ShutterMode);
								}
								if (ShutterWhiteLight_Flag)
								{
									Switch2Relay(ChShutterWhiteLight_A, ChShutterWhiteLight_B, 0);
									ShutterWhiteLight_Flag = false;
									sprintf(ShutterMode, "OFF (0)");	
									hCtrl = GetDlgItem(hWnd_Setting, IDC_SHUTTER_BB_EDIT);
									SetWindowText(hCtrl, ShutterMode);
								}
								if (!GratingWindow_Flag)
								{
									ServorMotor(ChGratingWindow, 50, 0.0725, 150);
									GratingWindow_Flag = true;
									SetWindowText(hCtrl, "ON (1)");
								}
							}
							else{
								if (ShutterMZ_Flag)
								{
									ShutterMZ_Flag = false;
									sprintf(ShutterMode, "OFF (0)");
									Switch1Relay(ChShutterMZ);
									hCtrl = GetDlgItem(hWnd_Setting, IDC_SHUTTER_MZ_EDIT);
									SetWindowText(hCtrl, ShutterMode);
								}
								if (GratingWindow_Flag)
								{
									ServorMotor(ChGratingWindow, 50, 0.125, 150);
									GratingWindow_Flag = false;
									SetWindowText(hCtrl, "OFF (0)");
								}
							}
							
							Sleep(10000);	//waiting 10 seconds for setup stable
							if (camera) camera->toggleTriggerModeOnOff(1);
							
						}*/
						break;
					
					case VK_F8:
					case '8':
						if (camera) camera->toggleTriggerModeOnOff(2);
						break;
					
					case VK_F9:
					case '9':
						if (camera) camera->toggleTriggerModeOnOff(3);
						break;

					case VK_F11:
						if (camera) camera->startRecord();
						break;
					
					case 'e':
					case 'E':
						if (camera) camera->toggleFrameStartEvent();
						break;
					
					case 'S':
						if (camera) camera->toggleDisplayMenuSimpleAdvanced();
						break;
					
					case VK_SPACE:
						if (camera) camera->toggleCameraAcquisitionOnOff();
						break;
//case VK_NUMPAD3:
					case VK_DOWN:
						if(SHOT_Flag)
						{	//HomeSHOT(Port, 3, "1");
							MoveRead_SHOT(PortStageXY, 2, "1", 50, PosSX, PosSY);
							changeEditText(hWnd_Setting, IDC_STAGE_X_EDIT, PosSX);
							changeEditText(hWnd_Setting, IDC_STAGE_Y_EDIT, PosSY);
						}
						break;

					case VK_UP:
						if(SHOT_Flag)
						{
							MoveRead_SHOT(PortStageXY, 2, "1", -50, PosSX, PosSY);
							changeEditText(hWnd_Setting, IDC_STAGE_X_EDIT, PosSX);
							changeEditText(hWnd_Setting, IDC_STAGE_Y_EDIT, PosSY);
						}
						break;

					case VK_LEFT:
						if(SHOT_Flag)
						{
							MoveRead_SHOT(PortStageXY, 2, "2", -50, PosSX, PosSY);
							changeEditText(hWnd_Setting, IDC_STAGE_X_EDIT, PosSX);
							changeEditText(hWnd_Setting, IDC_STAGE_Y_EDIT, PosSY);
						}
						break;

					case VK_RIGHT:
						if(SHOT_Flag)
						{
							MoveRead_SHOT(PortStageXY, 2, "2", 50, PosSX, PosSY);
							changeEditText(hWnd_Setting, IDC_STAGE_X_EDIT, PosSX);
							changeEditText(hWnd_Setting, IDC_STAGE_Y_EDIT, PosSY);
						}
						break;

					case VK_SUBTRACT:
						MoveRead_MicroE(PortStageZ, port_stage_z, "1", -0.01, PosSZ);
						changeEditText(hWnd_Setting, IDC_STAGE_Z_EDIT, PosSZ);
						break;

					case VK_ADD:
						MoveRead_MicroE(PortStageZ, port_stage_z, "1", 0.01, PosSZ);
						changeEditText(hWnd_Setting, IDC_STAGE_Z_EDIT, PosSZ);
						break;
//case VK_DOWN:
					case VK_NUMPAD3:
						if (camera)
						{
							FanFlag = (FanFlag+1)%3;
							char fanMode[20];
							char *mode;
							switch(FanFlag)
							{
								case 0:
									mode = "OFF";
									break;
								case 1:
									mode = "ON";
									break;
								case 2:
									mode = "Tempature";
									break;
							}
							camera->FanOpertaor((uint32_t)FanFlag);

							sprintf(fanMode,"%s (%d)",mode,FanFlag);
							HWND hFanCtrl = GetDlgItem(hWnd_Setting, IDC_FAN_EDIT);
							SetWindowText(hFanCtrl, fanMode);
						}
						break;

					case VK_NUMPAD1:					
						if(camera)
						{
							PatternFlag++;
							PatternFlag = PatternFlag%5;
							printf("%d",PatternFlag);
							camera->TestPattern(PatternFlag);
						}
						break;

					case VK_MULTIPLY:
						HomeMicroE(PortStageZ, 4, "1");
						break;

					case VK_DIVIDE:
						//MotorSHOT(Port, 3, "W",SHOT_Flag);
						break;

					case VK_DECIMAL:
						if (camera) camera->PrintExposureData();
						break;
					
					default:
						break;
				}
			}
            break;

         case WM_RBUTTONDOWN:
            if (camera) camera->startRecord();
            break;

         case WM_MBUTTONDOWN:
			   if (camera) camera->restartAcquisition();
            break;

		   default:
			   lReturn = DefWindowProc(hwnd, msg, wParam, lParam);
	   }
   } catch (std::exception &e) {
      printf("error: %s\n", e.what());
   } catch (...) {
      printf("unknown error\n");
   }
   return lReturn;
}

static WNDPROC OriginalEditCtrlProc = NULL;
LRESULT CALLBACK EditProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if(msg == WM_CHAR)
    {
        if(!    ((wParam >= '0' && wParam <= '9')
                || wParam == '.'
				|| wParam == '-'
                || wParam == VK_RETURN
                || wParam == VK_DELETE
                || wParam == VK_BACK))
        {
            return 0;
        }
    }

    return CallWindowProc(OriginalEditCtrlProc, hwnd, msg, wParam, lParam);
}

void CreateButton(HWND hwnd, HWND ButtonName, LPCSTR text, HMENU hMenu, HGDIOBJ hfDef, int posX, int posY, int width, int height)
{
	ButtonName = CreateWindowEx(NULL,
		"BUTTON",
		text,
		WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
		posX, posY, width, height,
		hwnd,
		hMenu, GetModuleHandle(NULL), NULL);
	SendMessage(ButtonName, WM_SETFONT, (WPARAM)hfDef, MAKELPARAM(FALSE, 0));
}

void CreateTextLabel(HWND hwnd, LPCSTR text, int posX, int posY, int width, int height)
{
	HWND hText = CreateWindow("STATIC", text
		, WS_VISIBLE | WS_CHILD | SS_LEFT, posX, posY, width, height, hwnd, NULL, GetModuleHandle(NULL), NULL);
}

void CreateEdit(HWND hwnd, HWND hEdit, int srcCode, HGDIOBJ hfDefault, LPCSTR text, int posX, int posY, int width, int height, bool mode)
{
	hEdit=CreateWindowEx(WS_EX_CLIENTEDGE,
		"EDIT",
		text,
		WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, // | ES_NUMBER,
		posX, posY, width, height,
		hwnd, (HMENU)srcCode, GetModuleHandle(NULL),	NULL
		);			
	
	SendMessage(hEdit, WM_SETFONT, (WPARAM)hfDefault, MAKELPARAM(FALSE,0));
	//SendMessage(hFrames, WM_SETTEXT, NULL, (LPARAM)text);

	WNDPROC oldProc;
	if(mode == true)
	{
		if(hEdit != NULL)
		{
			oldProc = reinterpret_cast<WNDPROC>(SetWindowLongPtr(
				hEdit,
				GWLP_WNDPROC,
				reinterpret_cast<LONG_PTR>(EditProc)));
			if(OriginalEditCtrlProc == NULL)
			{
				OriginalEditCtrlProc = oldProc;
			}
		}
	}
}


LRESULT CALLBACK SetWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
		case WM_CREATE:
		{
			//WNDPROC oldProc;
			HGDIOBJ hfDef=GetStockObject(DEFAULT_GUI_FONT);
								
			CreateTextLabel(hwnd, "Frame:          ", 10, 15, 150, 20);
			CreateTextLabel(hwnd, "Exposure:       ", 10, 40, 150, 20);
			CreateTextLabel(hwnd, "(us)            ", 155, 40, 150, 20);

			CreateTextLabel(hwnd, "MaxV (X):       ", 10, 90, 150, 20);
			CreateTextLabel(hwnd, "MaxV (Y):       ", 10, 115, 150, 20);
			CreateTextLabel(hwnd, "Voltage (X):    ", 10, 140, 150, 20);
			CreateTextLabel(hwnd, "Voltage (Y):    ", 10, 165, 150, 20);
				
			CreateTextLabel(hwnd, "Fan Status:     ", 10, 190, 150, 20);
			CreateTextLabel(hwnd, "532 nm Laser:   ", 10, 215, 150, 20);
			CreateTextLabel(hwnd, "MZ Shutter:     ", 10, 240, 150, 20);
			CreateTextLabel(hwnd, "Broadband Light:", 10, 265, 150, 20);
			CreateTextLabel(hwnd, "Grating/Window: ", 10, 290, 150, 20);
			CreateTextLabel(hwnd, "Phase Retrieve: ", 10, 315, 150, 20);
			CreateTextLabel(hwnd, "Dichronic mirror", 10, 340, 150, 20);
			CreateTextLabel(hwnd, "Power of pump   ", 10, 365, 150, 20);

			CreateTextLabel(hwnd, "Stage Y (pulse) ", 10, 390, 150, 20);
			CreateTextLabel(hwnd, "Stage X (pulse) ", 10, 430, 150, 20);
			CreateTextLabel(hwnd, "Stage Z (um)    ", 10, 470, 150, 20);
			CreateTextLabel(hwnd, "Stage WD (pulse)", 10, 510, 150, 20);

			CreateTextLabel(hwnd, "Intervial (min)", 10, 560, 150, 20);
			CreateTextLabel(hwnd, "Scan Number ", 10, 585, 150, 20);
			CreateTextLabel(hwnd, "Current time ", 10, 610, 150, 20);
			CreateTextLabel(hwnd, "Next Scan time", 10, 635, 150, 20);

			CreateTextLabel(hwnd, "Location        ", 10, 670, 150, 20);

			//Create Edits
			CreateEdit(hwnd, hFrames, IDC_FRAM_EDIT, hfDef, "500", 125, 15, 60, 20, true);	// total frames for scanning
			CreateEdit(hwnd, hExpTime, IDC_TIME_EDIT, hfDef, "200", 125, 40, 60, 20, true);	// exposure time (microsecond)

			CreateEdit(hwnd, hVolXmax, IDC_VXMAX_EDIT, hfDef, "3.1", 125, 90, 60, 20, true);	// max voltage of |X| scanner
			CreateEdit(hwnd, hVolYmax, IDC_VYMAX_EDIT, hfDef, "1.5", 125, 115, 60, 20, true);	// max voltage of |Y| scanner
			CreateEdit(hwnd, hVolXcur, IDC_VXCUR_EDIT, hfDef, "0", 125, 140, 60, 20, true);	// current voltage of |X| scanner
			CreateEdit(hwnd, hVolYcur, IDC_VYCUR_EDIT  , hfDef, "0"   , 125, 165, 60, 20, true);	// current voltage of |Y| scanner	

			CreateEdit(hwnd, hFanStatus, IDC_FAN_EDIT, hfDef, "ON (1)", 125, 190, 60, 20, true);
			CreateEdit(hwnd, hShutter532Status, IDC_SHUTTER532_EDIT, hfDef, "OFF (0)", 125, 215, 60, 20, true);
			CreateEdit(hwnd, hShutterMZStatus, IDC_SHUTTER_MZ_EDIT, hfDef, "ON (1)", 125, 240, 60, 20, true);
			CreateEdit(hwnd, hShutterBBStatus, IDC_SHUTTER_BB_EDIT, hfDef, "OFF (0)", 125, 265, 60, 20, true);
			CreateEdit(hwnd, hGWStatus, IDC_SWITCH_GW_EDIT, hfDef, "ON (1)", 125, 290, 60, 20, true);
			CreateEdit(hwnd, hPUStatus, IDC_RETRIEVE_EDIT, hfDef, "OFF (0)", 125, 315, 60, 20, true);
			CreateEdit(hwnd, hDMStatus, IDC_SWITCH_DM_EDIT, hfDef, "OFF (0)", 125, 340, 60, 20, true);
			CreateEdit(hwnd, hPumpStatus, IDC_PUMP_EDIT, hfDef, "OFF (0)", 125, 365, 60, 20, true);

			CreateEdit(hwnd, hPosX , IDC_STAGE_X_EDIT, hfDef, "0"   , 120, 410, 60, 20, true);	// current position of x-axis	
			CreateEdit(hwnd, hPosY , IDC_STAGE_Y_EDIT, hfDef, "0"   , 120, 450, 60, 20, true);	// current position of y-axis	
			CreateEdit(hwnd, hPosZ , IDC_STAGE_Z_EDIT, hfDef, "0"   , 120, 490, 60, 20, true);	// current position of z-axis	
			CreateEdit(hwnd, hPosWD, IDC_STAGE_WD_EDIT, hfDef, "0", 120, 530, 60, 20, true);		// current position of z-axis			

			CreateEdit(hwnd, hIntervalEdit, IDC_INTEVAL_EDIT, hfDef, "10", 120, 560, 60, 20, true);		// scanning interval in min
			CreateEdit(hwnd, hScanNoEdit, IDC_SCANNO_EDIT, hfDef, "64", 120, 585, 60, 20, true);		// how many times would be ran
			CreateEdit(hwnd, hCurTimeEdit, IDC_CURTIME_EDIT, hfDef, "--:--:--", 155, 610, 60, 20, true);		// display the time of next scanning
			CreateEdit(hwnd, hNxNumEdit, IDC_NXNUM_EDIT, hfDef, "#-", 120, 635, 30, 20, true);		// display the time of next scanning
			CreateEdit(hwnd, hNxScanEdit, IDC_NXSCAN_EDIT, hfDef, "--:--:--", 155, 635, 60, 20, true);		// display the time of next scanning

			CreateEdit(hwnd, hFolder , IDC_FOLDER_EDIT , hfDef, "D:"  , 70, 670, 100, 20, true);	// saving folder path	
				
			// load the date for saving path
			CreateTextLabel(hwnd, "Date"			 , 10,695,35,20);
			CreateEdit(hwnd, hDate   , IDC_DATE_EDIT   , hfDef, ""    , 70, 695, 70, 20, true);	// dates of experiment
			char date_str[10];
			time_t now = time(0);
			tm *ltm = localtime(&now);
			sprintf(date_str, "%4d-%02d-%02d", 1900 + ltm->tm_year, 1 + ltm->tm_mon, ltm->tm_mday);

			HWND hDateCtrl = GetDlgItem(hwnd, IDC_DATE_EDIT);
			SetWindowText(hDateCtrl, date_str);

			CreateTextLabel(hwnd, "Series"		 , 200,718,50,20);
			CreateEdit(hwnd, hSeries , IDC_NUMBER_EDIT , hfDef, "1"   ,255, 718, 25, 20, true);	// dates of experiment

			CreateTextLabel(hwnd, "Name"		 , 10,718,50,20);
			CreateEdit(hwnd, hName , IDC_NAME_EDIT , hfDef, "Bead"   ,70, 718, 120, 20, false);	// dates of experiment
				
			// set the ComboBox for Image Type
			CreateTextLabel(hwnd, "Img Type"        , 10,740,70,20);
			hSaveType = CreateWindow(TEXT("combobox"), NULL, WS_VISIBLE | WS_CHILD  | CBS_DROPDOWN|WS_TABSTOP,
			80, 740,100, 210, hwnd, (HMENU) 12, NULL, NULL);
			SendMessage(hSaveType, CB_ADDSTRING, 1, (LPARAM) _T("Sample"));
			SendMessage(hSaveType, CB_ADDSTRING, 2, (LPARAM) _T("Background"));
			SendMessage(hSaveType, CB_ADDSTRING, 3, (LPARAM) _T("Angle"));
			SendMessage(hSaveType, CB_ADDSTRING, 4, (LPARAM) _T("Angle of x-axis"));
			SendMessage(hSaveType, CB_ADDSTRING, 5, (LPARAM) _T("Angle of y-axis"));
			SendMessage(hSaveType, CB_SETCURSEL, 0, 0);				

			HWND hResetParm, hRotXButton, hCurXButton, hRotYButton, hCurYButton;				
			// reset button
			CreateButton(hwnd, hResetParm, "Reset", (HMENU)IDC_RESET_BUTTON, hfDef, 125, 65, 60, 20);

			// the buttons for Scanner X
			CreateButton(hwnd, hRotXButton, "OK", (HMENU)IDC_VXROT_BUTTON, hfDef, 190, 90, 50, 20);
			CreateButton(hwnd, hCurXButton, "OK", (HMENU)IDC_VXCUR_BUTTON, hfDef, 190, 140, 50, 20);

			// the buttons for Scanner Y
			CreateButton(hwnd, hRotYButton, "OK", (HMENU)IDC_VYROT_BUTTON, hfDef, 190, 115, 50, 20);
			CreateButton(hwnd, hCurYButton, "OK", (HMENU)IDC_VYCUR_BUTTON, hfDef, 190, 165, 50, 20);

			HWND hFanButton, h532Button, hMZYButton, hBBButton, hGWButton, hPUButton, hDMButton, hPumpButton;
			// Buttons for the Camera Fan and Shutters
			CreateButton(hwnd, hFanButton, "switch", (HMENU)IDC_CAMFAN_BUTTON, hfDef, 190, 190, 50, 20);
			CreateButton(hwnd, h532Button, "switch", (HMENU)IDC_SH532_BUTTON, hfDef, 190, 215, 50, 20);
			CreateButton(hwnd, hMZYButton, "switch", (HMENU)IDC_SHMZ_BUTTON, hfDef, 190, 240, 50, 20);
			CreateButton(hwnd, hBBButton, "switch", (HMENU)IDC_SHBB_BUTTON, hfDef, 190, 265, 50, 20);
			CreateButton(hwnd, hGWButton, "switch", (HMENU)IDC_SWGW_BUTTON, hfDef, 190, 290, 50, 20);
			CreateButton(hwnd, hPUButton, "switch", (HMENU)IDC_SWPU_BUTTON, hfDef, 190, 315, 50, 20);
			CreateButton(hwnd, hDMButton, "switch", (HMENU)IDC_SWDM_BUTTON, hfDef, 190, 340, 50, 20);
			CreateButton(hwnd, hPumpButton, "switch", (HMENU)IDC_PUMP_BUTTON, hfDef, 190, 365, 50, 20);
			//******************** Set the stage buttons ********************//
			HWND hSX_NS_Button, hSX_PS_Button, hSX_NL_Button, hSX_PL_Button, hSX_NH_Button, hSX_PH_Button;
			// subtract 10 pulse of Stage X
			CreateButton(hwnd, hSX_NS_Button, "-1", (HMENU)IDC_SX_NS_BUTTON, hfDef, 90, 410, 30, 20);
			// add 10 pulse of Stage X
			CreateButton(hwnd, hSX_PS_Button, "+1", (HMENU)IDC_SX_PS_BUTTON, hfDef, 180, 410, 30, 20);				
			// subtract 100 pulse of Stage X
			CreateButton(hwnd, hSX_NL_Button, "-50", (HMENU)IDC_SX_NL_BUTTON, hfDef, 60, 410, 30, 20);				
			// add 100 pulse of Stage X
			CreateButton(hwnd, hSX_PL_Button, "+50", (HMENU)IDC_SX_PL_BUTTON, hfDef, 210, 410, 30, 20);
			// move to the negative HOME
			//CreateButton(hwnd, hSX_NH_Button, "-H", (HMENU)IDC_SX_NH_BUTTON, hfDef, 30, 410, 30, 20);
			// move to the positive HOME
			//CreateButton(hwnd, hSX_PH_Button, "+H", (HMENU)IDC_SX_PH_BUTTON, hfDef, 240, 410, 30, 20);


			HWND hSY_NS_Button, hSY_PS_Button, hSY_NL_Button, hSY_PL_Button, hSY_NH_Button, hSY_PH_Button;
			// subtract 10 pulse of Stage Y
			CreateButton(hwnd, hSY_NS_Button, "-1", (HMENU)IDC_SY_NS_BUTTON, hfDef, 90, 450, 30, 20);
			// add 10 pulse of Stage Y
			CreateButton(hwnd, hSY_PS_Button, "+1", (HMENU)IDC_SY_PS_BUTTON, hfDef, 180, 450, 30, 20);
			// subtract 100 pulse of Stage Y
			CreateButton(hwnd, hSY_NL_Button, "-50", (HMENU)IDC_SY_NL_BUTTON, hfDef, 60, 450, 30, 20);
			// add 100 pulse of Stage Y
			CreateButton(hwnd, hSY_PL_Button, "+50", (HMENU)IDC_SY_PL_BUTTON, hfDef, 210, 450, 30, 20);
			// move to the negative HOME
			//CreateButton(hwnd, hSY_NH_Button, "-H", (HMENU)IDC_SY_NH_BUTTON, hfDef, 30, 450, 30, 20);
			// move to the positive HOME
			//CreateButton(hwnd, hSY_PH_Button, "+H", (HMENU)IDC_SY_PH_BUTTON, hfDef, 240, 450, 30, 20);
				

			HWND hSZ_NS_Button, hSZ_PS_Button, hSZ_NL_Button, hSZ_PL_Button, hSZ_NH_Button, hSZ_PH_Button;
			// subtract 1 micrometer of Stage Z
			CreateButton(hwnd, hSZ_NS_Button, "-0.1", (HMENU)IDC_SZ_NS_BUTTON, hfDef, 90, 490, 30, 20);
			// add 1 micrometer of Stage Z
			CreateButton(hwnd, hSZ_PS_Button, "+0.1", (HMENU)IDC_SZ_PS_BUTTON, hfDef, 180, 490, 30, 20);
			// subtract 1 micrometer of Stage Z
			CreateButton(hwnd, hSZ_NL_Button, "-1", (HMENU)IDC_SZ_NL_BUTTON, hfDef, 60, 490, 30, 20);
			// add 1 micrometer of Stage Z
			CreateButton(hwnd, hSZ_PL_Button, "+1", (HMENU)IDC_SZ_PL_BUTTON, hfDef, 210, 490, 30, 20);
			// move to the negative HOME
			//CreateButton(hwnd, hSY_NH_Button, "-H", (HMENU)IDC_SZ_NH_BUTTON, hfDef, 30, 490, 30, 20);
			// move to the positive HOME
			CreateButton(hwnd, hSY_PH_Button, "+H", (HMENU)IDC_SZ_PH_BUTTON, hfDef, 240, 490, 30, 20);


			HWND hWD_NS_Button, hWD_PS_Button, hWD_NL_Button, hWD_PL_Button, hWD_NH_Button, hWD_PH_Button, hWD_0_Button;
			// subtract 1 micrometer of Stage Z
			CreateButton(hwnd, hWD_NS_Button, "-10", (HMENU)IDC_WD_NS_BUTTON, hfDef, 90, 530, 30, 20);
			// add 1 micrometer of Stage Z
			CreateButton(hwnd, hWD_PS_Button, "+10", (HMENU)IDC_WD_PS_BUTTON, hfDef, 180, 530, 30, 20);
			// subtract 1 micrometer of Stage Z
			CreateButton(hwnd, hWD_NL_Button, "-500", (HMENU)IDC_WD_NL_BUTTON, hfDef, 60, 530, 30, 20);
			// add 1 micrometer of Stage Z
			CreateButton(hwnd, hWD_PL_Button, "+500", (HMENU)IDC_WD_PL_BUTTON, hfDef, 210, 530, 30, 20);
			// move to the negative HOME
			CreateButton(hwnd, hWD_NH_Button, "-H", (HMENU)IDC_WD_NH_BUTTON, hfDef, 30, 530, 30, 20);
			// move to the positive HOME
			CreateButton(hwnd, hWD_PH_Button, "+H", (HMENU)IDC_WD_PH_BUTTON, hfDef, 240, 530, 30, 20);
			// set to zero position for dual-axis scanning
			CreateButton(hwnd, hWD_0_Button, "0", (HMENU)IDC_WD_0_BUTTON, hfDef, 270, 530, 15, 20);
			//********************          END          ********************//
			
			// add the Browse button
			HWND hFolder_Button;				
			CreateButton(hwnd, hFolder_Button, "Browse", (HMENU)IDC_FOLDER_BUTTON, hfDef, 175, 670, 40, 20);

			// time-lapse scan
			HWND hTLScan_Button;
			CreateButton(hwnd, hTLScan_Button, "Start", (HMENU)IDC_TLSCAN_BUTTON, hfDef, 190, 560, 50, 20);

			/*HWND hPFcamera_Button, hSVPF_IMG_Button;
			CreateButton(hwnd, hPFcamera_Button, "Open Camera", (HMENU)IDC_PFCAM_BUTTON, hfDef, 60, 740, 100, 20);
			CreateButton(hwnd, hSVPF_IMG_Button, "Save Image", (HMENU)IDC_SVPF_BUTTON, hfDef, 60, 765, 100, 20);
			CreateTextLabel(hwnd, "Acq. Type", 20, 715, 70, 20);
			hPF_Type = CreateWindow(TEXT("combobox"), NULL, WS_VISIBLE | WS_CHILD | CBS_DROPDOWN | WS_TABSTOP,
				120, 715, 160, 210, hwnd, (HMENU)12, NULL, NULL);
			SendMessage(hPF_Type, CB_ADDSTRING, 1, (LPARAM)_T("Continous"));
			SendMessage(hPF_Type, CB_ADDSTRING, 2, (LPARAM)_T("SingleFrame"));
			SendMessage(hPF_Type, CB_ADDSTRING, 3, (LPARAM)_T("MultiFrame"));
			SendMessage(hPF_Type, CB_ADDSTRING, 4, (LPARAM)_T("ContinousRecording"));
			SendMessage(hPF_Type, CB_ADDSTRING, 5, (LPARAM)_T("ContinousReadout"));
			SendMessage(hPF_Type, CB_ADDSTRING, 6, (LPARAM)_T("SingleFrameRecording"));
			SendMessage(hPF_Type, CB_ADDSTRING, 7, (LPARAM)_T("SingleFrameReadout"));
			SendMessage(hPF_Type, CB_SETCURSEL, 0, 0);

			CreateEdit(hwnd, hPF_ExpTime, IDC_PFEXP_EDIT, hfDef, "200", 60, 790, 100, 20, true);	// saving folder path	*/

			UpdateWindow(hwnd);
		}
		break;		

		case WM_CTLCOLORSTATIC:
		{
			HDC hdcStatic = (HDC) wParam;
			SetTextColor(hdcStatic, RGB(0,0,0));
			SetBkColor(hdcStatic, RGB(255,255,255));
			return (INT_PTR)CreateSolidBrush(RGB(255,255,255));
		}
		break;

		case WM_COMMAND:
			switch(LOWORD(wParam))
			{
				case IDC_RESET_BUTTON:
					TCHAR txt_Frame_Num[10];
					GetDlgItemText(hwnd, IDC_FRAM_EDIT, txt_Frame_Num,sizeof(txt_Frame_Num)/sizeof(TCHAR));
					imgFrame = atof(txt_Frame_Num);
						
					TCHAR txt_ExpTime[10];
					GetDlgItemText(hwnd, IDC_TIME_EDIT, txt_ExpTime,sizeof(txt_ExpTime)/sizeof(TCHAR));
					ExpTime = atof(txt_ExpTime);
					printf("Frame: %4d\n",imgFrame);
					printf("Time : %3d\n",ExpTime);

					break;

				case IDC_VXMAX_EDIT:
					TCHAR txt_X_max[10];
					GetDlgItemText(hwnd, IDC_VXMAX_EDIT, txt_X_max, sizeof(txt_X_max) / sizeof(TCHAR));
					Vol_X_max = atof(txt_X_max);
					break;

				case IDC_VYMAX_EDIT:
					TCHAR txt_Y_max[10];
					GetDlgItemText(hwnd, IDC_VYMAX_EDIT, txt_Y_max, sizeof(txt_Y_max) / sizeof(TCHAR));
					Vol_Y_max = atof(txt_Y_max);
					break;

				case IDC_VXROT_BUTTON:
					PostMessage(hWnd_Main, WM_COMMAND, IDM_PRE_RUNX, 0);
					break;

				case IDC_VYROT_BUTTON:
					PostMessage(hWnd_Main, WM_COMMAND, IDM_PRE_RUNY, 0);
					break;

				case IDC_VXCUR_BUTTON:
					CheckMenuItem(hMenu2, IDM_PRE_RUNX , MF_BYCOMMAND | MF_UNCHECKED);
					TCHAR txt_X_cur[10];
					GetDlgItemText(hwnd, IDC_VXCUR_EDIT, txt_X_cur,sizeof(txt_X_cur)/sizeof(TCHAR));
					Vol_X_cur = atof(txt_X_cur);

					if(StatusScanX == true)
					{
						StopScanner(TaskX);
						SetToVoltage(ChScanners, Vol_X_cur, 0, 0);
						StatusScanX = false;
					}
					else if(StatusScanX == false)
					{
						SetToVoltage(ChScanners, Vol_X_cur, 0, 0);
					}
					break;

				case IDC_VYCUR_BUTTON:
					CheckMenuItem(hMenu2, IDM_PRE_RUNY , MF_BYCOMMAND | MF_UNCHECKED);
					TCHAR txt_Y_cur[10];
					GetDlgItemText(hwnd, IDC_VYCUR_EDIT, txt_Y_cur,sizeof(txt_Y_cur)/sizeof(TCHAR));
					Vol_Y_cur = atof(txt_Y_cur);

					if(StatusScanY == true)
					{
						StopScanner(TaskY);
						SetToVoltage(ChScanners, 0, Vol_Y_cur, 0);
						StatusScanY = false;
					}
					else if(StatusScanY == false)
					{
						SetToVoltage(ChScanners, 0, Vol_Y_cur, 0);
					}
					break;

				case IDC_CAMFAN_BUTTON:
					PostMessage(hWnd_Main, WM_KEYDOWN, VK_NUMPAD3, 0);
					break;

				case IDC_SH532_BUTTON:
					if (Shutter532_Flag)
					{
						Switch2Relay(ChShutter532_A, ChShutter532_B, 1);
						Shutter532_Flag = 0;
						sprintf(ShutterMode, "OFF (0)");
					}
					else{
						Switch2Relay(ChShutter532_A, ChShutter532_B, 0);
						Shutter532_Flag = 1;
						sprintf(ShutterMode, "ON (1)");
					}
					
					hCtrl = GetDlgItem(hWnd_Setting, IDC_SHUTTER532_EDIT);
					SetWindowText(hCtrl, ShutterMode);
					break;

				case IDC_SHBB_BUTTON:
					if (ShutterWhiteLight_Flag)
					{
						Switch2Relay(ChShutterWhiteLight_A, ChShutterWhiteLight_B, 0);
						ShutterWhiteLight_Flag = 0;
						sprintf(ShutterMode, "OFF (0)");
					}
					else{
						Switch2Relay(ChShutterWhiteLight_A, ChShutterWhiteLight_B, 1);
						ShutterWhiteLight_Flag = 1;
						sprintf(ShutterMode, "ON (1)");
					}

					hCtrl = GetDlgItem(hWnd_Setting, IDC_SHUTTER_BB_EDIT);
					SetWindowText(hCtrl, ShutterMode);
					break;

				case IDC_SHMZ_BUTTON:
					if (ShutterMZ_Flag)
					{
						ShutterMZ_Flag = false;
						sprintf(ShutterMode, "OFF (0)");
					}
					else{
						ShutterMZ_Flag = true;
						sprintf(ShutterMode, "ON (1)");
					}
					Switch1Relay(ChShutterMZ);

					hCtrl = GetDlgItem(hWnd_Setting, IDC_SHUTTER_MZ_EDIT);
					SetWindowText(hCtrl, ShutterMode);
					break;

				case IDC_SWGW_BUTTON:
					hCtrl = GetDlgItem(hWnd_Setting, IDC_SWITCH_GW_EDIT);
					if (GratingWindow_Flag)
					{
						ServorMotor(ChGratingWindow, 50, 0.125, 150);
						GratingWindow_Flag = false;
						SetWindowText(hCtrl, "OFF (0)");
					}
					else
					{
						ServorMotor(ChGratingWindow, 50, 0.0725, 150);
						GratingWindow_Flag = true;
						SetWindowText(hCtrl, "ON (1)");
					}
					break;

				case IDC_SWPU_BUTTON:
					hCtrl = GetDlgItem(hWnd_Setting, IDC_RETRIEVE_EDIT);
					if (PhaseUnwrapping_Flag)
					{
						PhaseUnwrapping_Flag = false;
						SetWindowText(hCtrl, "OFF (0)");
					}
					else
					{
						PhaseUnwrapping_Flag = true;
						SetWindowText(hCtrl, "ON (1)");
					}
					break;

				case IDC_PUMP_BUTTON:
					hCtrl = GetDlgItem(hWnd_Setting, IDC_PUMP_EDIT);
					if (PumpPower_Flag)
					{
						SwitchRelay(ChPumping, true);
						PumpPower_Flag = false;
						SetWindowText(hCtrl, "OFF (0)");
					}
					else
					{
						SwitchRelay(ChPumping, false);
						PumpPower_Flag = true;
						SetWindowText(hCtrl, "ON (1)");
					}
					break;
					

				// move stage X
				case IDC_SX_NS_BUTTON:
					if(SHOT_Flag)
					{
						MoveRead_SHOT(PortStageXY, port_stage_xy, "1", -1, PosSX, PosSY);
						changeEditText(hWnd_Setting, IDC_STAGE_X_EDIT, PosSX);
					}
					break;
				case IDC_SX_PS_BUTTON:
					if(SHOT_Flag)
					{
						MoveRead_SHOT(PortStageXY, port_stage_xy, "1", 1, PosSX, PosSY);
						changeEditText(hWnd_Setting, IDC_STAGE_X_EDIT, PosSX);
					}
					break;
				case IDC_SX_NL_BUTTON:
					if(SHOT_Flag)
					{
						MoveRead_SHOT(PortStageXY, port_stage_xy, "1", -50, PosSX, PosSY);
						changeEditText(hWnd_Setting, IDC_STAGE_X_EDIT, PosSX);
					}
					break;
				case IDC_SX_PL_BUTTON:
					if(SHOT_Flag)
					{
						MoveRead_SHOT(PortStageXY, port_stage_xy, "1", 50, PosSX, PosSY);
						changeEditText(hWnd_Setting, IDC_STAGE_X_EDIT, PosSX);
					}
					break;
				case IDC_SX_NH_BUTTON:
					if (SHOT_Flag)
					{
						//HomeSHOT(PortStageXY, port_stage_xy, "2", 0);
						changeEditText(hWnd_Setting, IDC_STAGE_X_EDIT, "0");
					}
					break;
				case IDC_SX_PH_BUTTON:
					if (SHOT_Flag)
					{
						//HomeSHOT(PortStageXY, port_stage_xy, "2", 1);
						changeEditText(hWnd_Setting, IDC_STAGE_X_EDIT, "0");
					}
					break;

				// move stage Y
				case IDC_SY_NS_BUTTON:
					if(SHOT_Flag)
					{
						MoveRead_SHOT(PortStageXY, port_stage_xy, "2", -1, PosSX, PosSY);
						changeEditText(hWnd_Setting, IDC_STAGE_Y_EDIT, PosSY);
					}
					break;
				case IDC_SY_PS_BUTTON:
					if(SHOT_Flag)
					{
						MoveRead_SHOT(PortStageXY, port_stage_xy, "2", 1, PosSX, PosSY);
						changeEditText(hWnd_Setting, IDC_STAGE_Y_EDIT, PosSY);
					}
					break;
				case IDC_SY_NL_BUTTON:
					if(SHOT_Flag)
					{
						MoveRead_SHOT(PortStageXY, port_stage_xy, "2", -50, PosSX, PosSY);
						changeEditText(hWnd_Setting, IDC_STAGE_Y_EDIT, PosSY);
					}
					break;
				case IDC_SY_PL_BUTTON:
					if(SHOT_Flag)
					{
						MoveRead_SHOT(PortStageXY, port_stage_xy, "2", 50, PosSX, PosSY);
						changeEditText(hWnd_Setting, IDC_STAGE_Y_EDIT, PosSY);
					}
					break;
				case IDC_SY_NH_BUTTON:
					if (SHOT_Flag)
					{
						//HomeSHOT(PortStageXY, port_stage_xy, "2", 0);
						changeEditText(hWnd_Setting, IDC_STAGE_Y_EDIT, 0);
					}
					break;
				case IDC_SY_PH_BUTTON:
					if (SHOT_Flag)
					{
						//HomeSHOT(PortStageXY, port_stage_xy, "2", 1);
						changeEditText(hWnd_Setting, IDC_STAGE_Y_EDIT, "0");
					}
					break;

				// move stage Z
				case IDC_SZ_NS_BUTTON:
					MoveRead_MicroE(PortStageZ, port_stage_z, "1", -0.1, PosSZ);
					changeEditText(hWnd_Setting, IDC_STAGE_Z_EDIT, PosSZ);
					break;
				case IDC_SZ_PS_BUTTON:
					MoveRead_MicroE(PortStageZ, port_stage_z, "1", 0.1, PosSZ);
					changeEditText(hWnd_Setting, IDC_STAGE_Z_EDIT, PosSZ);
					break;
				case IDC_SZ_NL_BUTTON:
					MoveRead_MicroE(PortStageZ, port_stage_z, "1", -1, PosSZ);
					changeEditText(hWnd_Setting, IDC_STAGE_Z_EDIT, PosSZ);
					break;
				case IDC_SZ_PL_BUTTON:
					MoveRead_MicroE(PortStageZ, port_stage_z, "1", 1, PosSZ);
					changeEditText(hWnd_Setting, IDC_STAGE_Z_EDIT, PosSZ);
					break;
				case IDC_SZ_NH_BUTTON:
					HomeMicroE(PortStageZ, port_stage_z, "1");
					changeEditText(hWnd_Setting, IDC_STAGE_Z_EDIT, PosSZ);
					break;
				case IDC_SZ_PH_BUTTON:
					HomeMicroE(PortStageZ, port_stage_z, "1");
					changeEditText(hWnd_Setting, IDC_STAGE_Z_EDIT, PosSZ);
					break;

				// move the stage of window
				case IDC_WD_NS_BUTTON:
					if (SHOT_Flag)
					{
						MoveRead_SHOT(PortStageWindow, port_stage_window, "2", -10, PosSX, PosWD);
						changeEditText(hWnd_Setting, IDC_STAGE_WD_EDIT, PosWD);
					}
					break;
				case IDC_WD_PS_BUTTON:
					if (SHOT_Flag)
					{
						MoveRead_SHOT(PortStageWindow, port_stage_window, "2", 10, PosSX, PosWD);
						changeEditText(hWnd_Setting, IDC_STAGE_WD_EDIT, PosWD);
					}
					break;
				case IDC_WD_NL_BUTTON:
					if (SHOT_Flag)
					{
						MoveRead_SHOT(PortStageWindow, port_stage_window, "2", -500, PosSX, PosWD);
						changeEditText(hWnd_Setting, IDC_STAGE_WD_EDIT, PosWD);
					}
					break;
				case IDC_WD_PL_BUTTON:
					if (SHOT_Flag)
					{
						MoveRead_SHOT(PortStageWindow, port_stage_window, "2", 500, PosSX, PosWD);
						changeEditText(hWnd_Setting, IDC_STAGE_WD_EDIT, PosWD);
					}
					break;
				case IDC_WD_NH_BUTTON:
					if (SHOT_Flag)
					{
						HomeSHOT(PortStageWindow, port_stage_window, "2", 0);
						changeEditText(hWnd_Setting, IDC_STAGE_WD_EDIT, "0");
					}
					break;
				case IDC_WD_PH_BUTTON:
					if (SHOT_Flag)
					{
						HomeSHOT(PortStageWindow, port_stage_window, "2", 1);
						changeEditText(hWnd_Setting, IDC_STAGE_WD_EDIT, "0");
					}
					break;
				case IDC_WD_0_BUTTON:
					if (SHOT_Flag)
					{
						//HomeSHOT(PortStageWindow, port_stage_window, "2", 1);
						SetOriginSHOT(PortStageWindow, port_stage_window, "2");	//use it!
						changeEditText(hWnd_Setting, IDC_STAGE_WD_EDIT, "0");
						//SetSpeedSHOT(PortStageWindow, port_stage_window, "2", 2000,2000,5);
						///linearTravelSHOT(PortStageWindow, port_stage_window, "2", -1000);
					}
					break;

				case IDC_SWDM_BUTTON:
					hCtrl = GetDlgItem(hWnd_Setting, IDC_SWITCH_DM_EDIT);					
					if (DichronicMirros_Flag)
					{
						ServorMotor(ChDM, 50, 0.118, 1000);	//0.03
						DichronicMirros_Flag = false;
						SetWindowText(hCtrl, "OFF (0)");
					}
					else
					{
						ServorMotor(ChDM, 50, 0.069, 1000);	//0.12
						DichronicMirros_Flag = true;
						SetWindowText(hCtrl, "ON (1)");
					}
					break;

				case IDC_FOLDER_BUTTON:
				{
					FolderPath = BrowseFolder("");
					HWND hFolder = GetDlgItem(hWnd_Setting, IDC_FOLDER_EDIT);
					SetWindowText(hFolder, FolderPath.c_str());
					//GetFolder();
				}
					break;

				case IDC_TLSCAN_BUTTON:
				{					
					t_start = std::time(0);
					start = std::localtime(&t_start);
					sprintf(StartTime, "%02d:%02d:%02d", start->tm_hour, start->tm_min, start->tm_sec);
					changeEditText(hWnd_Setting, IDC_CURTIME_EDIT, StartTime);
					
					// get the initial parameters of time-lapse
					TCHAR interval_text[10], times_text[10];
					GetDlgItemText(hwnd, IDC_INTEVAL_EDIT, interval_text, sizeof(interval_text) / sizeof(TCHAR));
					GetDlgItemText(hwnd, IDC_SCANNO_EDIT, times_text, sizeof(times_text) / sizeof(TCHAR));
					TL_interval = atoi(interval_text) * 60;	//convert min to sec
					TL_times = atoi(times_text);

					hCtrl = GetDlgItem(hWnd_Setting, IDC_TLSCAN_BUTTON);

					accumulate_time = 0;

					if (!TLScan_Flag) {
						if (TL_interval > 0 && TL_times > 0)
						{
							t_next = t_start + TL_interval;
							nextime = std::localtime(&t_next);
							sprintf(NextTime, "#%d", accumulate_time+1);
							changeEditText(hWnd_Setting, IDC_NXNUM_EDIT, NextTime);
							sprintf(NextTime, "%02d:%02d:%02d", nextime->tm_hour, nextime->tm_min, nextime->tm_sec);
							changeEditText(hWnd_Setting, IDC_NXSCAN_EDIT, NextTime);
							PostMessage(hWnd_Main, WM_KEYDOWN, VK_F7, 0);
							SetWindowText(hCtrl, "Stop");
							TLScan_Flag = true;
						}
						else if (TL_interval > 0 && TL_times < 0)
						{
							MessageBox(NULL, "The value of scanning time must greater than zero.", "Setting Error", MB_OK | MB_ICONERROR);
							SetWindowText(hCtrl, "Start");
							TLScan_Flag = false;
						}
						else if (TL_interval < 0 && TL_times > 0)
						{
							MessageBox(NULL, "The value of interval must greater than zero.", "Setting Error", MB_OK | MB_ICONERROR);
							SetWindowText(hCtrl, "Start");
							TLScan_Flag = false;
						}
					}
					else {
						changeEditText(hWnd_Setting, IDC_NXNUM_EDIT, "#-");
						changeEditText(hWnd_Setting, IDC_NXSCAN_EDIT, "--:--:--");
						SetWindowText(hCtrl, "Start");
						TLScan_Flag = false;
					}
				}
					break;

				case IDC_PFCAM_BUTTON:
					PF_AcquireImages();
					break;
			}
			break;

        case WM_CLOSE:
				ShowWindow(hwnd,SW_HIDE);
                break;

        case WM_KEYDOWN:
                break;
        default:
                return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}


#pragma comment(lib, "version.lib")

std::string GetFileVersion   (HINSTANCE hInst)
{
   char szFileName[1024];
   GetModuleFileName(hInst, szFileName, 1024);

   DWORD iVersionSize = GetFileVersionInfoSize(szFileName, NULL);
   void *pVersionBuffer = malloc(iVersionSize);
   GetFileVersionInfo(szFileName, 0, iVersionSize, pVersionBuffer);

   void *pE;
   UINT iSizeSubBuff = 0;
   BOOL bResult = VerQueryValue(pVersionBuffer, "\\", &pE, &iSizeSubBuff);

   uint64_t uiVersion = 0;
   if (bResult) {
      uint64_t a = ((VS_FIXEDFILEINFO *)pE)->dwFileVersionMS;
      uint64_t b = ((VS_FIXEDFILEINFO *)pE)->dwFileVersionLS;
      uiVersion = a << 32 | b;
   }

   std::strstream strs;
   std::string strVersion;
   strs << ((uint16_t*)&uiVersion)[3] << ".";
   strs << ((uint16_t*)&uiVersion)[2] << ".";
   strs << ((uint16_t*)&uiVersion)[1] << ".";
   strs << ((uint16_t*)&uiVersion)[0];
   strs << std::ends;
   strVersion = strs.str();

   free(pVersionBuffer);
   return strVersion;
}

const char * createFileFromResource(LPCSTR lpName, LPCSTR lpType, LPCSTR filename)
{
   if (!lpName)   throw std::invalid_argument("Null pointer for resource name");
   if (!lpType)   throw std::invalid_argument("Null pointer for resource type");
   if (!filename) throw std::invalid_argument("Null pointer for file to create");

	HRSRC hRes = FindResource(0, lpName, lpType);
	if (!hRes) {
      std::stringstream ss;
      ss << "Unable to find Resource, Error " << std::dec << GetLastError();
      throw std::invalid_argument(ss.str());
	}

	HGLOBAL hGL = LoadResource(0, hRes);
	if (!hGL) {
      std::stringstream ss;
      ss << "Unable to load resource, Error " << std::dec << GetLastError();
      throw std::invalid_argument(ss.str());
	}

	void *pData = LockResource(hGL);
	if (!pData) {
		throw std::invalid_argument("Unable to lock resource");
	}

   DWORD dwResSize = SizeofResource(0, hRes);
	if (!pData) {
		throw std::invalid_argument("Unable to get resource size");
	}

   FILE *pf = 0;
   
   if (fopen_s(&pf, filename, "wb+")) {
      throw std::runtime_error(std::string("Error while creating file.") + std::string(filename));
   }

   if (!pf) throw std::runtime_error("Unable to create file.");

   if (size_t n = fwrite(pData, sizeof(char), dwResSize, pf) != dwResSize) {
      std::stringstream ss;
      ss << "Unable to write entire PCF file, " << std::dec << n << " bytes written instead of " << std::dec << hGL;
      throw std::invalid_argument(ss.str());
	}

   fclose(pf);

   return filename;
}

bool ParseCommandLine(int argc, _TCHAR* argv[], CCamera::Options_t & options)
{
   bool justPrintingUsage = false;
   bool fatal = false;
   bool bufferBeforeTriggerCountIsDefault = true;
   std::string s(argv[0]);
   size_t pos = s.find_last_of("\\");
   if (pos != std::string::npos) s = s.substr(pos+1, s.size());

   // Choose default number of buffers to record to fit within the limits of the OS
   if (sizeof(void *) != 8) options.recordBufferCount = 500; // 32bit OS (else if 64bit OS, we keep 100 as default)
   options.recordBufferCount = 500;
   options.bufferBeforeTriggerCount = 0;//options.recordBufferCount / 2;

   if (argc == 2 && (std::string(argv[1]) == std::string("--help")  || std::string(argv[1]) == std::string("-h"))) {
      justPrintingUsage = true;
   } else {
      for (int i = 1; i < argc; i++) {
         if (i + 1 != argc) { // Check that we haven't finished parsing already
            if (std::string(argv[i]) == std::string("-r") || std::string(argv[i]) == std::string("--record-buffer")) {
               options.recordBufferCount = atoi(argv[i+1]);
            } else if (std::string(argv[i]) == std::string("-o") || std::string(argv[i]) == std::string("--out-directory")) {
               options.outputDirectory = argv[i+1];
            } else if (std::string(argv[i]) == std::string("--cxp")) {
               options.gbps = atoi(argv[i+1]);
            } else if (std::string(argv[i]) == std::string("--lanes")) {
               options.lanes = atoi(argv[i+1]);
            } else if (std::string(argv[i]) == std::string("--watchdog")) {
               options.watchdogTimeMs = atoi(argv[i+1]);
            } else if (std::string(argv[i]) == std::string("--buffers-before-trigger")) {
               options.bufferBeforeTriggerCount = atoi(argv[i+1]);
               bufferBeforeTriggerCountIsDefault = false;
            } else if (std::string(argv[i]) == std::string("--prefix")) {
               options.prefix = argv[i+1];
            } else if (std::string(argv[i]) == std::string("--no-prefix")) {
               if (atoi(argv[i+1]) == 1) options.noPrefix = true;
            } else if (std::string(argv[i]) == std::string("--pcf")) {
               options.pcf = argv[i+1];
            } else if (std::string(argv[i]) == std::string("--boardnumber") || std::string(argv[i]) == std::string("-b")) {
               options.boardnumber = atoi(argv[i+1]);
            } else if (std::string(argv[i]) == std::string("--channelnumber") || std::string(argv[i]) == std::string("-c")) {
               options.channelnumber = atoi(argv[i+1]);
            } else if (std::string(argv[i]) == std::string("--boardvariant") || std::string(argv[i]) == std::string("-v")) {
               options.boardvariant = argv[i+1];
            } else if (std::string(argv[i]) == std::string("--record-width")) {
               options.recordWidth = atoi(argv[i+1]);
            } else if (std::string(argv[i]) == std::string("--record-height")) {
               options.recordHeight = atoi(argv[i+1]);
            } else if (std::string(argv[i]) == std::string("--roi-camera")) {
               if (atoi(argv[i+1]) == 0) options.applyROI = 0;
            } else if (std::string(argv[i]) == std::string("--dvr-frame-rate")) {
               options.dvrFrameRate = atoi(argv[i+1]);
            } else if (std::string(argv[i]) == std::string("--avi-q-factor")) {
               options.jpegQFactor = atoi(argv[i+1]);
            } else if (std::string(argv[i]) == std::string("--multiple-roi")) {
               if (atoi(argv[i+1])) options.multipleROI = true;
            } else if (std::string(argv[i]) == std::string("--use-internal-trigger")) {
               if (atoi(argv[i+1])) options.internalTrigger = true;
            }
         }
      }
   }

   // if the user has not changed options.bufferBeforeTriggerCount, we have to recalculate it in case options.recordBufferCount has been changed
   if (bufferBeforeTriggerCountIsDefault) {
      options.bufferBeforeTriggerCount = options.recordBufferCount / 2;
   }

   std::string ProductVersion = GetFileVersion(0);

   printf("Active Silicon High-Speed Demo v%s.\n", ProductVersion.c_str());
   printf("Copyright 2013 Active Silicon Ltd.\n");
   printf(DISPLAY_NAME"\n");

   printf("Usage: %s [options]\n\n", s.c_str());

   char tmpCXP[32]   = "auto";
   char tmpLanes[32] = "auto";

   if (options.gbps)  sprintf_s(tmpCXP,   sizeof(tmpCXP),   "%4d", options.gbps);
   if (options.lanes) sprintf_s(tmpLanes, sizeof(tmpLanes), "%4d", options.lanes);

   printf("   -b, --boardnumber         <%4d>  select the board to use\n", options.boardnumber);
   printf("   -c, --channelnumber       <%4d>  select the channel to use\n", options.channelnumber);
   printf("   -v, --boardvariant        <%s>\n", options.boardvariant.c_str());
   printf("                                     select board-type\n");
   printf("                                     PHX_BOARD_FBD_1XCXP6_2PE8\n");
   printf("                                     PHX_BOARD_FBD_2XCXP6_2PE8\n");
   printf("                                     PHX_BOARD_FBD_4XCXP6_2PE8\n");
   printf("                                     PHX_BOARD_FBD_1XCLD_2PE8\n");
   printf("                                     PHX_BOARD_FBD_2XCLD_2PE8\n");
   printf("                                     PHX_BOARD_FBD_1XCLM_2PE8\n");
   printf("                                     PHX_BOARD_FBD_2XCLM_2PE8\n");
   printf("   --pcf                             set the  configuration file to use\n");
   printf("   --cxp                     <%s>  set the speed on the CXP cable (0 = auto)\n", tmpCXP);
   printf("                                     (only available for CXP cameras)\n");
   printf("   --lanes                   <%s>  set the number of CXP lanes (0 = auto)\n", tmpLanes);
   printf("                                     (only available for CXP cameras)\n");
   printf("   -r, --record-buffer       <%4d>  set the number of buffers to record\n", options.recordBufferCount);
   printf("                                     (equal to the number of buffers used by\n");
   printf("                                     the acquisition engine)\n");
   printf("   --record-width            <%4d>  set the width to crop images during\n", options.recordWidth);
   printf("                                     recording\n");
   printf("   --record-height           <%4d>  set the height to crop images during\n", options.recordHeight);
   printf("                                     recording\n");
   printf("   --dvr-frame-rate          <%4d>  set the frame rate for AVI recording\n", options.dvrFrameRate);
   printf("   --avi-q-factor            <%4d>  set the quality factor for AVI recording\n", options.jpegQFactor);
   printf("   --watchdog                <%4d>  set time in milliseconds after which\n", options.watchdogTimeMs);
   printf("                                     the acquisition restarts if no image\n");
   printf("                                     has been received (zero means infinite)\n");
   printf("   --buffers-before-trigger  <%4d>  set the number of buffers preceding the\n", options.bufferBeforeTriggerCount);
   printf("                                     trigger to record\n");
   printf("   --roi-camera              <%4d>  1: apply frame grabber's ROI to camera and\n", options.applyROI);
   printf("                                     center it\n");
   printf("                                     (only available for some CXP cameras)\n");
   printf("                                     0: frame grabber's ROI will not be applied\n");
   printf("                                     to camera\n");
   printf("   --use-internal-trigger    <%4d>  0: use external source to trigger the\n", options.internalTrigger);
   printf("                                     frame grabber\n");
   printf("                                     1: use frame grabber's internal trigger\n");

   printf("   -o, --out-directory               set directory where output is produced\n");
   printf("   --prefix                          set prefix to use when saving images\n");
   printf("   --no-prefix               <%4d>  1: no prefix will be appended to saved\n", options.noPrefix);
   printf("                                     images (applies when AVI is not selected)\n");

   printf("   -h, --help                        show this help menu without starting");
   printf("                                            the demo\n");
   printf("\n\n");

   if (options.pcf.empty()) {
      printf("Using default PCF\n");
      options.defaultPcf = 1;
   } else {
      printf("Using PCF: %s\n", options.pcf.c_str());
      options.defaultPcf = 0;
   }

   if (options.lanes > 4)          printf("Error, invalid number of lanes: %d\n", options.lanes);
   if (options.gbps  > 6)          printf("Error, invalid CXP speed rate: %d\n", options.gbps);
   if (!options.recordBufferCount) printf("Error, invalid number of acquisition buffer (%d)\n", options.recordBufferCount);
   if (options.recordBufferCount && options.bufferBeforeTriggerCount >= options.recordBufferCount) {
      printf("Error, number of buffers to acquire before trigger (--buffers-before-trigger = %d)"\
             "cannot be greater or equal than the total number of buffers used for the recording (-r, --record-buffer = %d)\n", options.bufferBeforeTriggerCount, options.recordBufferCount);
      fatal = true;
   }

   return justPrintingUsage || fatal;
}

#include <Mmsystem.h>
#pragma comment (lib, "Winmm.lib")

int _tmain(int argc, _TCHAR* argv[])
{
	CCamera * camera = 0;
	char pcfname[1024];
	char logoname[1024];
	//CCamera::Options_t options;
	hWnd_Main = 0;				// Main window
	hWnd_Setting = 0;				// Setting window

	try {
			options.boardnumber              = 1;
			options.channelnumber            = 1;
			options.boardvariant             = "PHX_BOARD_DIGITAL";
			options.lanes                    = LANES;
			options.gbps                     = GBPS;
			options.recordBufferCount        = 100; // default may be changed in ParseCommandLine, if the system is 32-bit
			options.watchdogTimeMs           = 0;
			options.bufferBeforeTriggerCount = options.recordBufferCount / 2;
			options.outputDirectory          = ".";
			options.prefix                   = "";
			options.noPrefix                 = 0;
			options.bayerColorCorrection     = BAYER_COLOR_CORRECTION;
			options.recordWidth              = 0;
			options.recordHeight             = 0;
			options.applyROI                 = 1;
			options.dvrFrameRate             = 20;
			options.jpegQFactor              = 32;
			options.cameraName               = DISPLAY_NAME;
			options.multipleROI              = 0;
			options.internalTrigger          = 0;
			if (ParseCommandLine(argc, argv, options)) return 0;

			// main window
			WNDCLASS wc       = {0};
			wc.style          = CS_HREDRAW | CS_VREDRAW;
			wc.hInstance      = GetModuleHandle(NULL);
			wc.lpfnWndProc    = MainWndProc;
			wc.lpszClassName  = _T("foo");
			wc.lpszMenuName   = MAKEINTRESOURCE(NULL);
			wc.hIcon	      = LoadIcon(GetModuleHandle(NULL), (LPCTSTR)IDI_ICON1);
			wc.hCursor        = LoadCursor(NULL, IDC_ARROW);
			ATOM a            = RegisterClass(&wc);
			DWORD style       = WS_VISIBLE | WS_MINIMIZEBOX | WS_SYSMENU;
			wc.hbrBackground  = (HBRUSH)GetStockObject(WHITE_BRUSH);
			uint32_t monitorWidth   = GetSystemMetrics(SM_CXFULLSCREEN);
			uint32_t monitorHeight  = GetSystemMetrics(SM_CYFULLSCREEN);
			//uint32_t initialWidth   = monitorWidth  * 3 / 4;
			//uint32_t initialHeight  = monitorHeight * 3 / 4;
			uint32_t offsetWidth    = monitorWidth  / 8;
			uint32_t offsetHeight   = monitorHeight / 8;
			
			//RECT r = {0, 0, initialWidth, initialHeight};
			RECT r = {0, 0, 1024, 1024};
			AdjustWindowRect(&r, style, FALSE);
			
			//MessageBox(0, "Check point 1", "Check", MB_OK);
			hWnd_Main = CreateWindow(
						MAKEINTATOM(a), _T(""), style, 
						0,0, r.right - r.left, r.bottom - r.top,
						NULL, NULL, wc.hInstance, NULL);


			// setting window (child window of hWnd)
			WNDCLASS wc2       = {0};
			wc2.style          = CS_HREDRAW | CS_VREDRAW;
			wc2.hInstance      = GetModuleHandle(NULL);
			wc2.lpfnWndProc    = SetWndProc;
			wc2.lpszClassName  = _T("Setting");
			wc2.hCursor        = LoadCursor(NULL, IDC_ARROW);
			wc2.hbrBackground  = (HBRUSH)GetStockObject (WHITE_BRUSH);
			ATOM a2            = RegisterClass(&wc2);
			DWORD style2       =  WS_CAPTION | WS_VISIBLE | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX;

			RECT r2 = {0, 0, 300, 780};
			AdjustWindowRect(&r2, style2, FALSE);
			
			hWnd_Setting = CreateWindow(
						MAKEINTATOM(a2), _T("Setting"), style2, 
						1010,0, r2.right - r2.left, r2.bottom - r2.top,
						NULL, NULL, wc2.hInstance, NULL);

			FoV_size = MessageBox(NULL, "Record the image with 3072 x 3072?", "FoV size", MB_YESNO | MB_ICONQUESTION);

			if (!hWnd_Main) {
				return 0;
			}

			if (!hWnd_Setting) {
				return 0;
			}
			SetFocus(hWnd_Main);
			// temporary pcf and logo files
			if (FoV_size == IDYES)
			{
				if (options.defaultPcf) {
					sprintf_s(pcfname, 1024, "%d.pcf", hWnd_Main);
					options.pcf = createFileFromResource(MAKEINTRESOURCE(PCF), "PCF", pcfname);
					options.pcf = pcfname;
					options.recordBufferCount = 1;
					options.bufferBeforeTriggerCount = 0;
				}
			}
			else if (FoV_size == IDNO)
			{
				if (options.defaultPcf) {
					sprintf_s(pcfname, 1024, "%d.pcf", hWnd_Main);
					options.pcf = createFileFromResource(MAKEINTRESOURCE(IDR_PCF_VIEWORKS1024), "PCF", pcfname);
					options.pcf = pcfname;
					options.recordBufferCount = 500;
					options.bufferBeforeTriggerCount = 250;
				}
			}
			//sprintf_s(logoname, 1024, "%d.tif", hWnd_Main);
			//options.companyLogo = createFileFromResource(MAKEINTRESOURCE(IDR_IMAGES_ASL_LOGO), "IMAGES", logoname);
			//options.companyLogo = logoname;
			
			std::string pcfInfo;
			if (options.defaultPcf) pcfInfo = " - Using default PCF";
			else pcfInfo = " - PCF: " + options.pcf;
			char boardInfo[128];
			sprintf_s(boardInfo, 128, " - Board #%d - Channel #%d", options.boardnumber, options.channelnumber);
			std::string windowTitle = "DHuT v" + GetFileVersion(0) + " - " + DISPLAY_NAME + pcfInfo + boardInfo;
			SetWindowText(hWnd_Main, windowTitle.c_str());
			
			TMG_LibraryInit();

			camera = new
			#if   CAMERA==VIEWORKS
							CCameraVieworks
			#else
			#error Unknown camera
			#endif
				(options, hWnd_Main);
				(options, hWnd_Setting);

			if (!camera) throw std::runtime_error("Null pointer for camera.");
			camera->open();/**/

			SetLastError(0);
			SetWindowLongPtr(hWnd_Main, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(camera));

		#if _METHOD==_METHOD_BRUTE_FORCE
			// Display images at highest possible rate

			bool run = true;
			while (run) {
				MSG msg;
				while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
					if (msg.message == WM_QUIT) {
						run = false;
					}
					TranslateMessage(&msg);
					DispatchMessage(&msg);
				}

				RedrawWindow(hWnd_Main, NULL, NULL, RDW_INVALIDATE);
				//RedrawWindow(hWnd_Setting , NULL , NULL , RDW_INVALIDATE);
			}
		#elif _METHOD==_METHOD_MULTIMEDIA
			// Display images at given rate (first argument in timeSetEvent function is the period in ms)
			// Display rate may be limited depending on the system and the image and window sizes

			// Set resolution to the minimum supported by the system
			TIMECAPS tc;
			if (timeGetDevCaps(&tc, sizeof(TIMECAPS)) != MMSYSERR_NOERROR) THROW_UNRECOVERABLE("timeGetDevCaps() returned an error.");
			UINT resolution = min(max(tc.wPeriodMin, 0), tc.wPeriodMax);
			if (timeBeginPeriod(resolution) != MMSYSERR_NOERROR) THROW_UNRECOVERABLE("timeBeginPeriod() returned an error.");

			// create the timer
			MMRESULT idEvent = timeSetEvent(16, 1, TimerFunction, (DWORD)camera, TIME_PERIODIC);
			if (!idEvent) THROW_UNRECOVERABLE("timeSetEvent() returned an error.");

			bool run = true;
			while (run) {
				MSG msg;
				if (MsgWaitForMultipleObjects(0, NULL, FALSE, 50, QS_ALLINPUT) == WAIT_OBJECT_0) {
					while(GetMessage(&msg, NULL, 0, 0)) {
						TranslateMessage(&msg);
						DispatchMessage(&msg);
					}
					if (msg.message == WM_QUIT) {						
						run = false;
					}
				}
			}

			if (timeEndPeriod(resolution) != MMSYSERR_NOERROR) THROW_UNRECOVERABLE("timeEndPeriod() returned an error.");
			timeKillEvent(idEvent);
		#endif

		} catch (std::exception &e) {
			MessageBox(0, e.what(), "Error", MB_OK);
			printf("Error: %s\n", e.what());
			if (options.defaultPcf) DeleteFile(pcfname);
			DeleteFile(logoname);
		} catch (...) {
			MessageBox(0, "Unknown exception", "Error", MB_OK);
			printf("Error: unknown exception\n");
			if (options.defaultPcf) DeleteFile(pcfname);
			DeleteFile(logoname);
		}

		try {
			if(camera) {
				camera->close();
			}
		} catch(...) {
			MessageBox(0, "Failed to close camera", "Error", MB_OK);
		}

		delete camera;

		// Close and destroy main window, reposition console at top of Z order
		if (hWnd_Main) DestroyWindow(hWnd_Main);
		if (hWnd_Setting) DestroyWindow(hWnd_Setting);
		ShowWindow(GetConsoleWindow(), SW_RESTORE);
		SetWindowPos(GetConsoleWindow(), HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);

		//printf("\n\n\nPress any key to exit\n");
		//while(!_kbhit()) {}
   
		return 0;
}
