#define _USE_MATH_DEFINES

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <fstream> 
#include <iostream> 
#include <math.h>
#include <NIDAQmx.h>



int32 CVICALLBACK DoneCallback(TaskHandle taskHandle, int32 status, void *callbackData);

void GenTriangleWave(int numElements, double amplitude, double frequency, double *phase, double triangleWave[]);
int SetSampleClockRate(TaskHandle taskHandle, float64 desiredFreq, int32 sampsPerBuff, float64 cyclesPerBuff, float64 *desiredSampClkRate, float64 *sampsPerCycle, float64 *resultingSampClkRate, float64 *resultingFreq);
void RotateScanner(TaskHandle &NI_Task,char channel[], double freqX, double ampX);
void StopScanner(TaskHandle &NI_Task);
void SetToVoltage(char channel[], double vol_x, double vol_y, double vol_z);

void ParameterSetting(int mode, int totalStep, int S,
			int &algorithmStep, int &totalFrame, int &FreqCtr, int &FreqAO);

void PrearationScan(TaskHandle &NI_Task, double *AO_array, double *PZT_Volt, double max_X_vol, double max_Y_vol,
	int mode, int totalStep, int S,	int algorithmStep, int totalFrame);
void PreparationScan2(double *AO_array, double max_X_vol, double max_Y_vol,	int mode, int totalFrame);

void GenerateAO_MZ(int mode, double max_X_vol, double max_Y_vol, int S, int algorithmStep, 
					int totalStep, int totalFrame, double *AO_array, double *PZT_Volt);

void GenerateAO_CP(int mode, double max_X_vol, double max_Y_vol, int S, int algorithmStep, 
					int totalStep, int totalFrame, double *AO_array, double *PZT_Volt);

void PureCP_Array(double max_X_vol, double max_Y_vol, int totalFrame, int mode, double *AO_array);

void RunScanner(TaskHandle &NI_Task, TaskHandle &Ctr_Task,
		char channel[], int mode, double max_X_vol, double max_Y_vol, int totalStep);

void ServorMotor(char *ch, double frequency, double duty_cycle, int delay);
void Switch1Relay(char *ch);
void SwitchRelay(char *ch, bool status);
void Switch2Relay(char *ch1, char *ch2, bool status);