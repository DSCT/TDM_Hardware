#include "rs232.h"
#include <fstream> 
#include <iostream> 

void moveSHOT(RS232 Port, int Com, char axis[], int range);
void linearTravelSHOT(RS232 Port, int Com, char axis[], int range);
void ReadSHOT(RS232 Port, int Com);
void MoveRead_SHOT(RS232 Port, int Com, char axis[], int range, char *, char *);
void HomeSHOT(RS232 Port, int Com, char axis[], int dir);
void SetOriginSHOT(RS232 Port, int Com, char axis[]);
void SetSpeedSHOT(RS232 Port, int Com, char axis[], int S, int F, int R);
void MotorSHOT(RS232 Port, int Com, char axis[], bool &Flag);

void moveMicroE(RS232 Port, int Com, char axis[], int range);
void readMicroE(RS232 Port, int Com, char axis[]);
void MoveRead_MicroE(RS232 Port, int Com, char axis[], float range, char *);
void HomeMicroE(RS232 Port, int Com, char axis[]);

char *strim(char * str);

extern char *PosSX, *PosSY, *PosSZ;
extern bool SHOT_Flag;