#include "NI_device.h"
#include "HostParameter.h"
using namespace std;



int32 CVICALLBACK DoneCallback(TaskHandle taskHandle, int32 status, void *callbackData)
{
	int32   error=0;
	//char    errBuff[2048]={'\0'};

	// Check to see if an error stopped the task.
	//DAQmxErrChk (status);

	return 0;
}

int SetSampleClockRate(TaskHandle taskHandle, float64 desiredFreq, int32 sampsPerBuff, float64 cyclesPerBuff, float64 *desiredSampClkRate, float64 *sampsPerCycle, float64 *resultingSampClkRate, float64 *resultingFreq)
{

	int error=0;
	*sampsPerCycle = sampsPerBuff / cyclesPerBuff;
	*desiredSampClkRate = desiredFreq * sampsPerBuff / cyclesPerBuff;
	DAQmxSetTimingAttribute(taskHandle,DAQmx_SampClk_Rate,*desiredSampClkRate);
	DAQmxGetTimingAttribute(taskHandle,DAQmx_SampClk_Rate,resultingSampClkRate);
	*resultingFreq = *resultingSampClkRate / *sampsPerCycle;

   return error;
}

void GenTriangleWave(int numElements, double amplitude, double frequency, double *phase, double triangleWave[])
{
	int i=0;

	for(;i<numElements;++i) {
		double	phase_i=fmod(*phase+360.0*frequency*i,360.0);
		double	percentPeriod=phase_i/360.0;
		double	dat=amplitude*4.0*percentPeriod;

		if( percentPeriod<=0.25 )
			triangleWave[i] = dat;
		else if( percentPeriod<=0.75 )
			triangleWave[i] = 2.0*amplitude - dat;
		else
			triangleWave[i] = dat - 4.0*amplitude;
		//printf("\nNum:\t%lf",triangleWave[i]);
	}
	*phase = fmod(*phase+frequency*360.0*numElements,360.0);

}

void RotateScanner(TaskHandle &NI_Task, char channel[], double freq, double amp)
{
	//char        chan[256]="Dev1/ao1";  //控制scanner的channel
	int         error=0;
	int         waveformType;
	double      resultingFrequency,desiredSampClkRate,sampsPerCycle,resultingSampClkRate;
	uInt32      sampsPerBuffer=2000;
	uInt32      cyclesPerBuffer=10;
	float64     *data= (float64 *)malloc(sampsPerBuffer*sizeof(float64));
	char        errBuff[2048]={'\0'};
	double      phase=0.0;
	int         log;
	int32 		written;
	double frequency = freq;
	double amplitude = amp;
		
	/*********************************************/
	// DAQmx Configure Code
	/*********************************************/
	DAQmxCreateTask("",&NI_Task);
	DAQmxCreateAOVoltageChan(NI_Task,channel,"",-10,+10,DAQmx_Val_Volts,NULL);
	SetSampleClockRate(NI_Task,frequency,sampsPerBuffer,cyclesPerBuffer,&desiredSampClkRate,&sampsPerCycle,&resultingSampClkRate,&resultingFrequency);

	//printf("\nfrequency:\t\t%lf",frequency);
	//printf("\namplitude:\t\t%lf",amplitude);
	//printf("\nsampsPerBuffer:\t\t%d",sampsPerBuffer);
	//printf("\ncyclesPerBuffer:\t%d",cyclesPerBuffer);
	//printf("\ndesiredSampClkRate:\t%lf",desiredSampClkRate);
	//printf("\nsampsPerCycle:\t\t%lf",sampsPerCycle);
	//printf("\nresultingSampClkRate:\t%lf",resultingSampClkRate);
	//printf("\nresultingFrequency:\t%lf",resultingFrequency);

	DAQmxCfgSampClkTiming(NI_Task,"",resultingSampClkRate,DAQmx_Val_Rising,DAQmx_Val_ContSamps,1000);
	DAQmxRegisterDoneEvent(NI_Task,0,DoneCallback,NULL);

	GenTriangleWave(sampsPerBuffer,amplitude,1/sampsPerCycle,&phase,data);
	

	/*********************************************/
	// DAQmx Write Code
	/*********************************************/
	DAQmxWriteAnalogF64(NI_Task,sampsPerBuffer,0,10.0,DAQmx_Val_GroupByChannel,data,NULL,NULL);

	/*********************************************/
	// DAQmx Start Code
	/*********************************************/
	DAQmxStartTask(NI_Task);
	if(NI_Task != 0)
		printf("\nStarting rotate the '%s' from -%.2lf to %.2lf...",channel,amp,amp);
	//_sleep(5000);
	//DAQmxStopTask(NI_Task);
	//DAQmxClearTask(NI_Task);
	/*Error:
	if( DAQmxFailed(error) )
		DAQmxGetExtendedErrorInfo(errBuff,2048);
	if( NI_Task!=0 ) {
		
		DAQmxStopTask(NI_Task);
		DAQmxClearTask(NI_Task);
	}
	if( DAQmxFailed(error) )
		printf("DAQmx Error: %s\n",errBuff);
	printf("End of program, press Enter key to quit\n");*/

	//system("pause");
}

void SetToVoltage(char channel[], double vol_x, double vol_y, double vol_z)
{
	float64 vol[3] = {vol_x, vol_y, vol_z};

	if(vol_x>10 || vol_x<-10 || vol_y>10 || vol_y<-10 || vol_z>10 || vol_z<-10)
	{
		MessageBox(0, "Error: Voltage range.", "show", MB_ICONEXCLAMATION);
	}
	else
	{
		TaskHandle NI_Task;
		DAQmxCreateTask("",&NI_Task);
		DAQmxCreateAOVoltageChan(NI_Task,channel,"",-10,10,DAQmx_Val_Volts,"");
		
		DAQmxStartTask(NI_Task);
		DAQmxWriteAnalogF64(NI_Task,1,1,10.0,DAQmx_Val_GroupByChannel,vol,NULL,NULL);
				
		DAQmxStopTask(NI_Task);
		DAQmxClearTask(NI_Task);
		NI_Task = 0;
		//printf("\nChange the voltage of '%s' to: %.2lf",channel, vol);		
	}
}

void StopScanner(TaskHandle &NI_Task)
{
	DAQmxStopTask(NI_Task);
	DAQmxClearTask(NI_Task);

	NI_Task=0;
}


void ParameterSetting(int mode, int totalStep, int S,
			int &algorithmStep, int &totalFrame, int &FreqCtr, int &FreqAO)
{
	switch(mode) {
	//common-path configuration on X-axis
	case 1:
		algorithmStep = 1;
		FreqCtr       = 120.0;
		break;

	//common-path configuration on Y-axis
	case 2:		
		algorithmStep = 1;		
		FreqCtr       = 120;//35;
		break;
	//common-path configuration on Dual-axis
	case 3:
		algorithmStep = 1;
		FreqCtr       = 120.0;
		break;
	//Mach-Zehnder configuration on X-axis
	case 4:
		algorithmStep = 6;
		FreqCtr       = 120.0;
		break;

	//Mach-Zehnder configuration on Dual-axis
	case 5:
		algorithmStep = 6;
		FreqCtr       = 120.0;
		break;
	//same as Case 1
	default:
		algorithmStep = 1;
		FreqCtr       = 120.0;
		break;
	}
	totalFrame        = totalStep * algorithmStep ;		//total frames of scanning
	FreqAO            = FreqCtr * S;					//frequency of the Analog output
} 

void PreparationScan(TaskHandle &NI_Task, double *AO_array, double *PZT_Volt, double max_X_vol, double max_Y_vol,
	int mode, int totalStep, int S,	int algorithmStep, int totalFrame)
{
	//set the array data of 'Scanners' and 'PZT'
	switch(mode){
		//common-path with Scanner X
		case 1:
			max_Y_vol = 0.0;
			GenerateAO_CP(mode, max_X_vol, max_Y_vol, S, algorithmStep, totalStep, totalFrame, AO_array, PZT_Volt);
			break;
		//common-path with Scanner Y
		case 2:
			max_X_vol = 0.0;
			GenerateAO_CP(mode, max_X_vol, max_Y_vol, S, algorithmStep, totalStep, totalFrame, AO_array, PZT_Volt);
			break;
		//common-path with Dual Scanner
		case 3:
			GenerateAO_CP(mode, max_X_vol, max_Y_vol, S, algorithmStep, totalStep, totalFrame, AO_array, PZT_Volt);
			break;
		//Mach-Zehnder with Scanner X
		case 4:
			max_Y_vol = 0.0;
			GenerateAO_MZ(mode, max_X_vol, max_Y_vol, S, algorithmStep, totalStep, totalFrame, AO_array, PZT_Volt);
			break;
		//Mach-Zehnder with Dual Scanner
		case 5:
			GenerateAO_MZ(mode, max_X_vol, max_Y_vol, S, algorithmStep, totalStep, totalFrame, AO_array, PZT_Volt);
			break;
		//same as case 1
		default:
			max_Y_vol = 0.0;
			GenerateAO_CP(mode, max_X_vol, max_Y_vol, S, algorithmStep, totalStep, totalFrame, AO_array, PZT_Volt);
			break;
	}

	//make the initial voltage for each channel
	double AO1 = 0.0, AO2 = 0.0, AO3 = 0.0;
	AO1 = AO_array[0             ];
	AO2 = AO_array[totalFrame*S  ];
	AO3 = AO_array[totalFrame*S*2];
	SetToVoltage(ChScanners, AO1, AO2, AO3);
	
}

void PreparationScan2(double *AO_array, double max_X_vol, double max_Y_vol,	int mode, int totalFrame)
{
	//set the array data of 'Scanners' and 'PZT'
	switch(mode){
		//common-path with Scanner X
		case 1:
			max_Y_vol = 0.0;
			PureCP_Array(max_X_vol, max_Y_vol, totalFrame, mode, AO_array);
			break;
		//common-path with Scanner Y
		case 2:
			max_X_vol = 0.0;
			PureCP_Array(max_X_vol, max_Y_vol, totalFrame, mode, AO_array);
			break;
		//common-path with Dual Scanner
		case 3:
			printf("\n%.2f\n%.2f", max_X_vol, max_Y_vol);
			PureCP_Array(max_X_vol, max_Y_vol, totalFrame, mode, AO_array);
			break;
		//Mach-Zehnder with Scanner X
		case 4:
			max_Y_vol = 0.0;
			//GenerateAO_MZ(mode, max_X_vol, max_Y_vol, S, algorithmStep, totalStep, totalFrame, AO_array, PZT_Volt);
			break;
		//Mach-Zehnder with Dual Scanner
		case 5:
			max_X_vol = 0.0;
			//GenerateAO_MZ(mode, max_X_vol, max_Y_vol, S, algorithmStep, totalStep, totalFrame, AO_array, PZT_Volt);
			break;
		//same as case 1
		default:
			max_Y_vol = 0.0;
			PureCP_Array(max_X_vol, max_Y_vol, totalFrame, mode, AO_array);
			break;
	}

	//make the initial voltage for each channel
	double AO1 = AO_array[0];
	double AO2 = AO_array[1];
	double AO3 = AO_array[2];
	//SetToVoltage(ChannelAll, AO1, AO2, AO3);

	//output the AO array
	/*ofstream ofs;
	ofs.open ("aodata_test.txt");
	double v1,v2,v3;
	int Adv = 3;
	for(int i=0 ; i<(totalFrame) ; i++)
	{
		v1 = AO_array[i*3  ];
		v2 = AO_array[i*3+1];
		v3 = AO_array[i*3+2];

		ofs << v1<<"\t"<<v2<<"\t"<<v3<<endl;
	}	
	ofs.close();*/

}


void GenerateAO_MZ(int mode, double max_X_vol, double max_Y_vol, int S, int algorithmStep, int totalStep, int totalFrame
					, double *AO_array, double *PZT_Volt)
{
	const int Advance = 47; //2//提前1個element
	
	int i,j,k,step,n,aStep;
	int counter;
	const int Ncircle = 10;
	
	double VXmax = max_X_vol;
	double VYmax = max_Y_vol;
	
	int count=0;
	
	int totalStepPerCircle = totalStep/Ncircle;		     //每個circle有幾圈
	double spacingDeg = 360.0/((double)totalStepPerCircle); //每步的間隔角度  

	double *ao1=(double*)malloc((totalFrame*S+Advance)*sizeof(double));
	double *ao2=(double*)malloc((totalFrame*S+Advance)*sizeof(double));
	double *ao3=(double*)malloc((totalFrame*S+Advance)*sizeof(double));
	//----------------------------------------------------------------------------	

	//ao1
	for(n=0;n<Ncircle;n++)
		for(step=0;step<totalStepPerCircle;step++)
			for(i=0 ; i<S*algorithmStep ; i++)
			{
				ao1[  n*(totalStepPerCircle*S*algorithmStep) +   step*(S*algorithmStep) + i ]
					= (double)(n+1) * ((double)VXmax/(double)Ncircle) * cos(spacingDeg*(double)step * M_PI/180);
			}
	

	for(i=0;i<Advance;i++)
	{
		ao1[Ncircle*totalStepPerCircle*S*algorithmStep + i] = 0; 	
	}
	
	//ao2
	for(n=0;n<Ncircle;n++)
		for(step=0;step<totalStepPerCircle;step++)
			for(i=0 ; i<S*algorithmStep ; i++)
			{
				ao2[  n*(totalStepPerCircle*S*algorithmStep) +   step*(S*algorithmStep) + i ]
					= (double)(n+1) * ((double)VYmax/(double)Ncircle) * sin(spacingDeg*(double)step * M_PI/180);
			}
	

	for(i=0;i<Advance;i++)
	{
		ao2[Ncircle*totalStepPerCircle*S*algorithmStep + i] = 0; 	
	}
	
	
	//ao3
	for(n=0;n<Ncircle;n++)
		for(step=0;step<totalStepPerCircle;step++)
			for(aStep=0;aStep<algorithmStep;aStep++)
			{
				for(i=0 ; i<S ; i++)
				{					
					if(step%2==0)
					{
						ao3[  n*(totalStepPerCircle*algorithmStep*S) + step*(algorithmStep*S) + aStep*(S) + i ] = PZT_Volt[aStep];
					}
					else if(step%2==1)
					{   
						ao3[  n*(totalStepPerCircle*algorithmStep*S) + step*(algorithmStep*S) + aStep*(S) + i ] = PZT_Volt[algorithmStep+aStep];
					}
				}
			}
	
	for(i=0;i<Advance;i++)
	{
		ao3[Ncircle*totalStepPerCircle*S*algorithmStep + i] = PZT_Volt[0];
	}

    
	// 資料轉移到aodata
	for(i=0 ; i<(totalFrame*S) ; i++)
	{	
		AO_array[i                 ] = ao1[ i + Advance];
		AO_array[i+(totalFrame*S)  ] = ao2[ i + Advance];
		AO_array[i+(totalFrame*S)*2] = ao3[ i + Advance];		
	}	
	
	free(ao1); 
	free(ao2); 
	free(ao3); 
}

void GenerateAO_CP(int mode, double max_X_vol, double max_Y_vol, int S, int algorithmStep, int totalStep, int totalFrame
					, double *AO_array, double *PZT_Volt)
{
	const int Advance   = 3; //提前3個element
	
	int i,j,k,step,n,aStep;
	int counter;
		
	double VXmax = max_X_vol, VYmax = max_Y_vol;
	double VXmin = -VXmax   , VYmin = -VYmax   ;
	double dVX   = (VXmax-VXmin)/(totalFrame-1);
	double dVY   = (VYmax-VYmin)/(totalFrame-1); 

	double *ao1=(double*)malloc((totalFrame*S+Advance)*sizeof(double));
	double *ao2=(double*)malloc((totalFrame*S+Advance)*sizeof(double));
	double *ao3=(double*)malloc((totalFrame*S+Advance)*sizeof(double));
	//----------------------------------------------------------------------------	 

	//ao1
	for(step=0;step<totalStep;step++)
		for(i=0 ; i<S*algorithmStep ; i++)
		{
			//every group (1~8; 1~S*algorithm) have the same voltage of scanner X
			ao1[   step*(S*algorithmStep) + i ] = VXmin + step*dVX;
		}

	for(i=0;i<Advance;i++)
	{
		ao1[totalStep*S*algorithmStep + i] = 0; 	
	}
	
	//ao2
	for(step=0;step<totalStep;step++)
		for(i=0 ; i<S*algorithmStep ; i++)
		{
			//every group (1~8; 1~S*algorithm) have the same voltage of scanner Y
			ao2[   step*(S*algorithmStep) + i ] = VYmin + step*dVY;
		}

	for(i=0;i<Advance;i++)
	{
		ao2[ totalStep*(S*algorithmStep) + i] = 0; 	
	}
	
	//ao3
	for(step=0;step<totalStep;step++)
		for(aStep=0;aStep<algorithmStep;aStep++)
		{				
			for(i=0 ; i<S ; i++)
			{	
				if(step%2==0)
				{
					ao3[  step*(algorithmStep*S) + aStep*(S) + i ] = 0;
				}
				else if(step%2==1)
				{
					ao3[  step*(algorithmStep*S) + aStep*(S) + i ] = 0;
				}
			}
		}
	
	for(i=0;i<Advance;i++)
	{
		ao3[ totalStep*S*algorithmStep + i] = 0;	  
	}

	// 資料轉移到aodata
	for(i=0 ; i<(totalFrame*S) ; i++)
	{
		AO_array[i                 ] = ao1[ i + Advance];	      
		AO_array[i+(totalFrame*S)  ] = ao2[ i + Advance];	
		AO_array[i+(totalFrame*S)*2] = ao3[ i + Advance];
	}
		
	free(ao1);
	free(ao2);
	free(ao3);
}

void PureCP_Array(double max_X_vol, double max_Y_vol, int totalFrame, int mode, double *AO_array)
{
	double VXmax = max_X_vol, VYmax = max_Y_vol;
	double VXmin = -VXmax   , VYmin = -VYmax   ;
	double dVX   = (VXmax-VXmin)/(totalFrame-1);
	double dVY   = (VYmax-VYmin)/(totalFrame-1); 

	if(mode != 3)
	{
		for(int i=0;i<totalFrame;i++)
		{
			AO_array[i*3  ] = VXmin + i*dVX;
			AO_array[i*3+1] = VYmin + i*dVY;
			AO_array[i*3+2] = 0;
		}
	}
	else
	{
		int lineTemp = 1;
		int CurrentLine = 0;
		int CumNum   = 1;
		float slideNum = sqrtf(totalFrame);
		VXmax = max_X_vol, VYmax = max_Y_vol;
		VXmin = -VXmax   , VYmin = -VYmax   ;
		dVX   = (VXmax-VXmin)/(slideNum-1);
		dVY   = (VYmax-VYmin)/(slideNum-1)/2;

		for(int i=0;i<totalFrame;i++)
		{
			float Center = float(lineTemp+1)/2;
			if(lineTemp%2 == 1)
			{
				AO_array[i*3  ] = -(CumNum - i -Center)*dVX;
			}else{
				AO_array[i*3  ] = (CumNum - i -Center)*dVX;
			}
			//cout<<"i:"<<i<<"; CumNum"<<CumNum<<"; Center"<<Center<<"; CurLine"<<CurrentLine<<endl;
			//system("pause");			
			AO_array[i*3+1] = VYmin + (CurrentLine)*dVY;

			if(CumNum < (totalFrame/2) && i-CumNum == -1)
			{
				CurrentLine++;
				lineTemp++;
				CumNum+= lineTemp;
			}
			else if(CumNum >= (totalFrame/2) && i-CumNum == -1)
			{
				CurrentLine++;
				lineTemp--;
				CumNum+= lineTemp;
			}

			AO_array[i*3+2] = 0;
		}
	}


	/*	for (n=1;n<=max;n+=2){
    for (i=1;i<=n;i++){
    for (ii=1;ii<=S;ii++){
    N=N+1;
    ao2[N]=StepLengthY*((max-n)/2);
    ao1[N]=StepLengthX*Minus*((-(n-1)/2)+(i-1));

	}
	}
	Minus=Minus*-1;
	}
	
	for (n=max-2;n>=1;n-=2){
    for (i=1;i<=n;i++){
    for (ii=1;ii<=S;ii++){
    N=N+1;
    ao2[N]=StepLengthY*((n-max)/2);
    ao1[N]=StepLengthX*Minus*((-(n-1)/2)+(i-1));

	}
	}
	Minus=Minus*-1; 
	}
	
	for(i=0;i<Advance;i++){
		ao1[totalFrame*S + i] = 0; 	
	}
	
	for(i=0;i<Advance;i++){
		ao2[totalFrame*S + i] = 0; 	
	}	
	*/

}

void RunScanner(TaskHandle &NI_Task, TaskHandle &Ctr_Task,
		char channel[], int mode, double max_X_vol, double max_Y_vol, int totalStep)
{
	int S = 8;
	int algorithmStep;
	int totalFrame;
	int FreqCtr = 0, FreqAO = 0;

	ParameterSetting(mode, totalStep, S, algorithmStep, totalFrame, FreqCtr, FreqAO);

	double *AO_array = (double *)malloc(totalFrame*S *3*sizeof(double));
	double *PZT_Volt = (double *)malloc(algorithmStep*2*sizeof(double));
	PreparationScan(NI_Task, AO_array, PZT_Volt, max_X_vol, max_Y_vol, mode, totalStep, S, algorithmStep, totalFrame);

	//output the AO array
	/*ofstream ofs;
	ofs.open ("aodata_test.txt");
	double v1,v2,v3;
	int Adv = 3;
	for(int i=0 ; i<(totalFrame*S) ; i++)
	{
		v1 = AO_array[i                 ];
		v2 = AO_array[i+(totalFrame*S)  ];
		v3 = AO_array[i+(totalFrame*S)*2];

		ofs << v1<<"\t"<<v2<<"\t"<<v3<<endl;
	}	
	ofs.close();	*/

	//AO channel設定
	int32  written;
	float64 aomin0 = -10.0, aomax0 = 10.0;
	double AO_SampClkRate     = FreqAO;							 //rate = 140*10 = 1400
	double AO_rSampClkRate    = 0; 
	uInt32 AO_sampsPerBuffer       = ( S * totalFrame ) * 3   ;  // 每個channel的訊號數目: (10*6*張數)*3  
	uInt32 AO_sampsPerChan         = ( S * totalFrame )       ;  // 每個channel的訊號數目: (10*6*張數)

	//TaskHandle AO_Task;
	if(NI_Task!=0)	StopScanner(NI_Task);
	DAQmxCreateTask("",&NI_Task);
	DAQmxCreateAOVoltageChan(NI_Task,channel,"",aomin0,aomax0,DAQmx_Val_Volts,NULL);

	DAQmxSetTimingAttribute(NI_Task,DAQmx_SampClk_Rate, AO_SampClkRate  );    //設定AO的取樣頻率
	DAQmxGetTimingAttribute(NI_Task,DAQmx_SampClk_Rate,&AO_rSampClkRate );    //讀取AO的取樣頻率

	cout<<"\n-------------------------------------"<<endl;
	cout<<"-------- Scanning Parameter ---------"  <<endl;
	cout<<"-------------------------------------"  <<endl;
	cout<<"mode             : "<<mode              <<endl;
	cout<<"maxX             : "<<max_X_vol         <<endl;
	cout<<"maxY             : "<<max_Y_vol         <<endl;
	cout<<"totalStep        : "<<totalStep         <<endl;
	cout<<"S                : "<<S                 <<endl;
	cout<<"algorithmStep    : "<<algorithmStep     <<endl;
	cout<<"totalFrame       : "<<totalFrame        <<endl;
	cout<<"FreqCtr          : "<<FreqCtr           <<endl;
	cout<<"AO_SampClkRate   : "<<AO_SampClkRate    <<endl;
	cout<<"AO_rSampClkRate  : "<<AO_rSampClkRate   <<endl;
	cout<<"AO_sampsPerBuffer: "<<AO_sampsPerBuffer <<endl;
	cout<<"AO_sampsPerChan  : "<<AO_sampsPerChan   <<endl;
	cout<<"-------------------------------------"  <<endl;

	if(AO_SampClkRate!=AO_rSampClkRate)
	{
		MessageBox(0, "Error: Sampling rate!!", "show", MB_ICONEXCLAMATION);
	}
	
		
	DAQmxCfgSampClkTiming(NI_Task,NULL,AO_rSampClkRate,DAQmx_Val_Rising,DAQmx_Val_FiniteSamps,AO_sampsPerChan);
	DAQmxWriteAnalogF64(NI_Task,AO_sampsPerChan,0,10.0,DAQmx_Val_GroupByChannel,AO_array,&written,NULL);

	DAQmxCreateTask("",&Ctr_Task);
	DAQmxCreateCOPulseChanFreq(Ctr_Task,"Dev1/ctr0","",DAQmx_Val_Hz,DAQmx_Val_Low,0.0,FreqCtr,0.6);
	DAQmxCfgImplicitTiming(Ctr_Task,DAQmx_Val_FiniteSamps,totalFrame);
	DAQmxConnectTerms ("/Dev1/PFI12", "/Dev1/RTSI0", DAQmx_Val_DoNotInvertPolarity );

	//Start the Tasks
	DAQmxStartTask(NI_Task );
	DAQmxStartTask(Ctr_Task);	
	DAQmxWaitUntilTaskDone(NI_Task,10.0);

	//Stop the Tasks
	DAQmxStopTask (NI_Task);
	DAQmxClearTask(NI_Task);
	NI_Task = 0;

	DAQmxStopTask (Ctr_Task);
	DAQmxClearTask(Ctr_Task);	
	Ctr_Task = 0;
	
}

// Servor Motor: freq = 50 Hz
// Duty cycle range (0-180 degrees):
// SG90  = 0.025-0.125
// MG995 = 0.03-0.1
void ServorMotor(char *ch, double frequency, double duty_cycle, int delay)
{
	TaskHandle  taskHandle = 0;
	float64 freq = frequency;
	float64 duty = duty_cycle;
	
	/*********************************************/
	/*/ DAQmx Configure Code
	/*********************************************/
	DAQmxCreateTask("", &taskHandle);
	DAQmxCreateCOPulseChanFreq(taskHandle, ch, "", DAQmx_Val_Hz, DAQmx_Val_High, 0.0, freq, duty);
	DAQmxCfgImplicitTiming(taskHandle, DAQmx_Val_ContSamps, 1000);

	/*********************************************/
	/*/ DAQmx Start Code
	/*********************************************/
	DAQmxStartTask(taskHandle);
	DAQmxWriteCtrFreqScalar(taskHandle, 0, 10.0, freq, duty, 0);
	Sleep(delay);

	/*********************************************/
	/*/ DAQmx Stop Code
	/*********************************************/
	DAQmxStopTask(taskHandle);
	DAQmxClearTask(taskHandle);
}


void Switch1Relay(char *ch)
{
	uInt8 dataOPEN[1];
	uInt8 dataCLOSED[1];
	dataOPEN[0] = 1;
	dataCLOSED[0] = 0;

	TaskHandle line1Handle;

	DAQmxCreateTask("", &line1Handle);
	DAQmxCreateDOChan(line1Handle, ch, "", DAQmx_Val_ChanForAllLines);

	DAQmxStartTask(line1Handle);

	
	DAQmxWriteDigitalLines(line1Handle, 1, 1, 10.0, DAQmx_Val_GroupByChannel, dataOPEN, NULL, NULL);
	Sleep(150);
	DAQmxWriteDigitalLines(line1Handle, 1, 1, 10.0, DAQmx_Val_GroupByChannel, dataCLOSED, NULL, NULL);


	DAQmxStopTask(line1Handle);
	DAQmxClearTask(line1Handle);
}
void SwitchRelay(char *ch, bool status)
{
	uInt8 dataOPEN[1];
	uInt8 dataCLOSED[1];
	dataOPEN[0] = 1;
	dataCLOSED[0] = 0;

	TaskHandle line1Handle;

	DAQmxCreateTask("", &line1Handle);
	DAQmxCreateDOChan(line1Handle, ch, "", DAQmx_Val_ChanForAllLines);

	DAQmxStartTask(line1Handle);

	if (status)
	{
		DAQmxWriteDigitalLines(line1Handle, 1, 1, 10.0, DAQmx_Val_GroupByChannel, dataOPEN, NULL, NULL);
	}
	else
	{
		DAQmxWriteDigitalLines(line1Handle, 1, 1, 10.0, DAQmx_Val_GroupByChannel, dataCLOSED, NULL, NULL);
	}

	DAQmxStopTask(line1Handle);
	DAQmxClearTask(line1Handle);
}
void Switch2Relay(char *ch1, char *ch2, bool status)
{
	uInt8 dataOPEN[1];
	uInt8 dataCLOSED[1];
	dataOPEN[0] = 1;
	dataCLOSED[0] = 0;

	TaskHandle line1Handle;
	TaskHandle line2Handle;

	DAQmxCreateTask("", &line1Handle);
	DAQmxCreateDOChan(line1Handle, ch1, "", DAQmx_Val_ChanForAllLines);

	DAQmxCreateTask("", &line2Handle);
	DAQmxCreateDOChan(line2Handle, ch2, "", DAQmx_Val_ChanForAllLines);

	DAQmxStartTask(line1Handle);
	DAQmxStartTask(line2Handle);

	if (status)
	{
		//DAQmxWriteDigitalLines(line1Handle, 1, 1, 10.0, DAQmx_Val_GroupByChannel, dataCLOSED, NULL, NULL);
		//Sleep(100);
		DAQmxWriteDigitalLines(line1Handle, 1, 1, 10.0, DAQmx_Val_GroupByChannel, dataOPEN, NULL, NULL);
		DAQmxWriteDigitalLines(line2Handle, 1, 1, 10.0, DAQmx_Val_GroupByChannel, dataCLOSED, NULL, NULL);
		Sleep(200);
		DAQmxWriteDigitalLines(line1Handle, 1, 1, 10.0, DAQmx_Val_GroupByChannel, dataCLOSED, NULL, NULL);
	}
	else
	{
		//DAQmxWriteDigitalLines(line2Handle, 1, 1, 10.0, DAQmx_Val_GroupByChannel, dataCLOSED, NULL, NULL);
		//Sleep(100);
		DAQmxWriteDigitalLines(line1Handle, 1, 1, 10.0, DAQmx_Val_GroupByChannel, dataCLOSED, NULL, NULL);
		DAQmxWriteDigitalLines(line2Handle, 1, 1, 10.0, DAQmx_Val_GroupByChannel, dataOPEN, NULL, NULL);
		Sleep(200);
		DAQmxWriteDigitalLines(line2Handle, 1, 1, 10.0, DAQmx_Val_GroupByChannel, dataCLOSED, NULL, NULL);
	}

	DAQmxStopTask(line1Handle);
	DAQmxClearTask(line1Handle);

	DAQmxStopTask(line2Handle);
	DAQmxClearTask(line2Handle);
}