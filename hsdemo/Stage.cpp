#include "Stage.h"
#include "resource.h"

using namespace std;


void moveSHOT(RS232 Port, int Com, char axis[], int range)
{
	int sizeMove=0, sizeStart=0;
	const int ARRAY_LENGTH = 30;
	char cmdMove[ARRAY_LENGTH], cmdRun[ARRAY_LENGTH],
		 cmdPos [ARRAY_LENGTH],readPos[ARRAY_LENGTH];
	char Num[5];

	itoa(range,Num,10);

	if(range>=0)
		sprintf(cmdMove,"M:%s+P%d\r\n",axis,range);
	else
		sprintf(cmdMove,"M:%s-P%d\r\n",axis,abs(range));

	sizeMove = strlen((char*)cmdMove);
	
	//Port.OpenComport(Com, 9600, "MotorSHOT");
	if(Port.SendBuf(Com,cmdMove,sizeMove)==-1)	printf("Error: send Movement command\n");	//send command	

	sprintf(cmdRun,"G\r\n");
	sizeStart = strlen((char*)cmdRun);
	if(Port.SendBuf(Com,cmdRun ,sizeStart)==-1)	printf("Error: send Start command\n");		//send command

	//Port.CloseComport(Com);

	printf("length: %2d; Str: %s",sizeMove ,cmdMove);
	printf("length: %2d; Str: %s",sizeStart,cmdRun );
}

void linearTravelSHOT(RS232 Port, int Com, char axis[], int range)
{
	int sizeMove = 0, sizeStart = 0;
	const int ARRAY_LENGTH = 30;
	char cmdMove[ARRAY_LENGTH], cmdRun[ARRAY_LENGTH],
		cmdPos[ARRAY_LENGTH], readPos[ARRAY_LENGTH];
	char Num[5];

	itoa(range, Num, 10);

	if (range >= 0)
		sprintf(cmdMove, "K:%s+%d\r\n", axis, range);
	else
		sprintf(cmdMove, "K:%s-%d\r\n", axis, abs(range));

	sizeMove = strlen((char*)cmdMove);

	//Port.OpenComport(Com, 9600, "MotorSHOT");
	if (Port.SendBuf(Com, cmdMove, sizeMove) == -1)	printf("Error: send Movement command\n");	//send command	

	sprintf(cmdRun, "G\r\n");
	sizeStart = strlen((char*)cmdRun);
	if (Port.SendBuf(Com, cmdRun, sizeStart) == -1)	printf("Error: send Start command\n");		//send command

	//Port.CloseComport(Com);

	printf("length: %2d; Str: %s", sizeMove, cmdMove);
	printf("length: %2d; Str: %s", sizeStart, cmdRun);
}

void HomeSHOT(RS232 Port, int Com, char axis[], int dir)
{
	int sizeStr=0;
	char Str[20];
	
	if (dir == 1)
		sprintf(Str,"H:%s+\r\n",axis);
	else
		sprintf(Str, "H:%s-\r\n", axis);

	sizeStr = strlen((char*)Str);
	
	//Port.OpenComport(Com,9600);
	Port.SendBuf(Com,Str,sizeStr);
	//Port.CloseComport(Com);

	printf("length: %2d; Str: %s",sizeStr,Str);
}

void SetOriginSHOT(RS232 Port, int Com, char axis[])
{
	int sizeStr = 0;
	char Str[20];

	sprintf(Str, "R:%s\r\n", axis);

	sizeStr = strlen((char*)Str);

	//Port.OpenComport(Com,9600);
	Port.SendBuf(Com, Str, sizeStr);
	//Port.CloseComport(Com);

	printf("length: %2d; Str: %s", sizeStr, Str);
}

void SetSpeedSHOT(RS232 Port, int Com, char axis[], int S, int F, int R)
{
	int sizeStr = 0;
	char Str[20];

	sprintf(Str, "D:%sS%dF%dR%d\r\n", axis, S, F, R);

	sizeStr = strlen((char*)Str);

	//Port.OpenComport(Com,9600);
	Port.SendBuf(Com, Str, sizeStr);
	//Port.CloseComport(Com);

	printf("length: %2d; Str: %s", sizeStr, Str);
	printf("\nSet the speed of axis%s to S:%d, F:%d, R:%d (pps)", axis, S, F, R);
}

void ReadSHOT(RS232 Port, int Com)
{
	int sizeStr=0;
	char Str[30], Buf[30];

	sprintf(Str,"Q:\r\n");

	sizeStr = strlen((char*)Str);
	
	Port.OpenComport(Com, 9600, "MotorSHOT");
	if(Port.SendBuf(Com,Str,sizeStr)==-1)
		printf("Error: send Position command\n");	//send command
	Port.PollComport(Com,Buf,30);
	Port.CloseComport(Com);

	printf("length: %2d; Str: %s",sizeStr,Str);
}

void MoveRead_SHOT(RS232 Port, int Com, char axis[], int range, char *posX, char *posY)
{
	//set the command of moving
	int sizeMove=0, sizePos=0;
	const int ARRAY_LENGTH = 30;
	char cmdMove[ARRAY_LENGTH], cmdPos [ARRAY_LENGTH],readPos[ARRAY_LENGTH];
	char Num[5];

	memset(cmdMove, '\0', sizeof(char) * ARRAY_LENGTH);
	memset(cmdPos , '\0', sizeof(char) * ARRAY_LENGTH);
	memset(readPos, '\0', sizeof(char) * ARRAY_LENGTH);

	itoa(range,Num,10);

	if(range>=0)
		sprintf(cmdMove,"M:%s+P%d\r\nG\r\n",axis,range);
	else
		sprintf(cmdMove,"M:%s-P%d\r\nG\r\n",axis,range*(-1));
	sizeMove = strlen((char*)cmdMove);		

	//Port.OpenComport(Com,9600);																//open comport
	if(Port.SendBuf(Com,cmdMove,sizeMove)==-1)	printf("Error: send Movement command\n");	//send command

	//set the command to obtain the position
	sprintf(cmdPos,"Q:\r\n");
	sizePos = strlen((char*)cmdPos);
	Port.flush(Com);
	if(Port.SendBuf(Com,cmdPos,sizePos)==-1)	printf("Error: send Position command\n");	//send command

	Port.PollComport(Com,readPos,ARRAY_LENGTH);												//read return value	
	//Port.CloseComport(Com);																	//close comport

	//printf("length: %2d; Str: %s",sizeMove ,cmdMove);
	//printf("length: %2d; Str: %s",sizePos  ,cmdPos );
	
	//check the return buffer
	char *strimLine = strim(readPos);			//elimates the SPACE char
	char *delim = ",";
	char *pch;
	int i =0;
	pch = strtok (strimLine,delim);
	while (pch != NULL)
	{
	//	printf ("%d-->%s\n",i,pch);
		if(i==0) sprintf(posX,"%s",pch);
		if(i==1) sprintf(posY,"%s",pch);
		i++;
		pch = strtok (NULL,delim);
	}
}
void MotorSHOT(RS232 Port, int Com, char axis[], bool &Flag)
{
	//set the command of moving
	int sizeStr=0;
	const int ARRAY_LENGTH = 30;
	char cmdStr[ARRAY_LENGTH];

	memset(cmdStr, '\0', sizeof(char) * ARRAY_LENGTH);

	if(Flag==true)
	{
		sprintf(cmdStr,"C:%s0\r\n",axis);
		Flag = false;
	}
	else
	{
		sprintf(cmdStr,"C:%s1\r\n",axis);
		Flag = true;
	}
	sizeStr = strlen((char*)cmdStr);		

	Port.OpenComport(Com,9600, "MotorSHOT");						//open comport
	if(Port.SendBuf(Com,cmdStr,sizeStr)==-1)
		printf("Error: send Motor command\n");		//send command
	Port.CloseComport(Com);							//close comport

	printf("length: %2d; Str: %s",sizeStr ,cmdStr);
}

void moveMicroE(RS232 Port, int Com, char axis[], int range)
{
	int sizeStr=0;
	char Str[20];
	char Num[5];

	itoa(range,Num,10);

	sprintf(Str,"%sMVR%.3f\n\r",axis,range*0.001);
	//sprintf(Str,"1HOM\r\n",range*0.001);

	printf("%s",Str);

	sizeStr = strlen((char*)Str);
	
	Port.OpenComport(Com,38400, "MicroE");
	Port.SendBuf(Com,Str,sizeStr);
	Port.CloseComport(Com);
}

void readMicroE(RS232 Port, int Com, char axis[])
{
	//open comport
	Port.OpenComport(Com,38400, "MicroE");

	//set the command to move the stage
	int sizeStr=0;
	char Str[30], Buf[30];
	char Num[5],Axis[2];
		
	sprintf(Str,"%sPOS?\n\r",axis);
	sizeStr = strlen((char*)Str);
	Port.SendBuf(Com,Str,sizeStr);
	Port.PollComport(Com,Buf,30);
	printf("%s",Buf);

	Port.CloseComport(Com);
}

void MoveRead_MicroE(RS232 Port, int Com, char axis[], float range, char *posZ)
{
	//set the command to move the stage
	int sizeMove=0, sizePos=0;
	const int ARRAY_LENGTH = 30;
	char cmdMove[ARRAY_LENGTH], cmdPos[ARRAY_LENGTH],readPos[ARRAY_LENGTH];
	char Num[5];

	memset(cmdMove, '\0', sizeof(char) * ARRAY_LENGTH);
	memset(cmdPos , '\0', sizeof(char) * ARRAY_LENGTH);
	memset(readPos, '\0', sizeof(char) * ARRAY_LENGTH);

	itoa(range,Num,10);

	sprintf(cmdMove,"%sMVR%.4f\n\r",axis,range*0.001);//if "range*0.001 = 1" means that the stage is going to move 1 mm
	sizeMove = strlen((char*)cmdMove);	

	//Port.OpenComport(Com,38400);															//open comport
	if(Port.SendBuf(Com,cmdMove,sizeMove)==-1)	printf("Error: send Movement command\n");	//send command	

	sprintf(cmdPos,"%sPOS?\n\r",axis);
	sizePos = strlen((char*)cmdPos);
	Port.flush(Com);
	if(Port.SendBuf(Com,cmdPos,sizePos)==-1)	printf("Error: send Position command\n");	//send command
	Port.PollComport(Com,readPos,ARRAY_LENGTH);												//read return value
	//Port.CloseComport(Com);																	//close comport

	printf("length: %2d; Str: %s",sizeMove,cmdMove);
	printf("length: %2d; Str: %s",sizePos ,cmdPos );

	//check the return buffer
	char *strimLine = strim(readPos);			//elimates the SPACE char
	char *delim = ",";
	char *pch;
	int i =0;
	pch = strtok (strimLine,delim);
	while (pch != NULL)
	{
		//printf ("%d-->%s\n",i,pch);
		if(i==0) sprintf(posZ,"%s",pch);
		i++;
		pch = strtok (NULL,delim);
	}

	//#-0.002050,-0.002050
	//min 0.1; max: 4
}

void HomeMicroE(RS232 Port, int Com, char axis[])
{
	int sizeStr=0;
	char Str[20];

	sprintf(Str, "%sHOM\n\r", axis);

	printf("%s",Str);

	sizeStr = strlen((char*)Str);
	
	//Port.OpenComport(Com,38400);
	if(Port.SendBuf(Com,Str,sizeStr)==-1)
		printf("Error: send Home command\n");	//send command
	//Port.CloseComport(Com);
}

char *strim(char * str)
{
	char * tail = str;
	char * next = str;

	while(*next)
	{
		if(*next != ' ' && *next != '#')
		{
			if(tail < next)
				*tail = *next;
			tail++;
		}
		next++;
	}
	*tail = '\0';

	return str;
}