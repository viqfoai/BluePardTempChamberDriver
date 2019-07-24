#pragma once

#define DEV_ADDR	1
#define REG_ON_OFF_CTRL 0x0000
#define REG_REAL_TEMP	0x1000
#define REG_REM_TIME	0x1002
#define REG_SET_TEMP	0x1004
#define REG_STAGE_1_TEMP	0x2000
#define REG_STAGE_1_TIME	0x2002
#define REG_STAGE_2_TEMP	0x2004
#define REG_STAGE_2_TIME	0x2006
#define REG_STAGE_3_TEMP	0x2008
#define REG_STAGE_3_TIME	0x200A
#define REG_STAGE_4_TEMP	0x200C
#define REG_STAGE_4_TIME	0x200E
#define REG_STAGE_5_TEMP	0x2010
#define REG_STAGE_5_TIME	0x2012
#define REG_STAGE_6_TEMP	0x2014
#define REG_STAGE_6_TIME	0x2016
#define REG_STAGE_7_TEMP	0x2018
#define REG_STAGE_7_TIME	0x201A
#define REG_STAGE_8_TEMP	0x201C
#define REG_STAGE_8_TIME	0x201E
#define REG_STAGE_9_TEMP	0x2020
#define REG_STAGE_9_TIME	0x2022
#define REG_STAGE_10_TEMP	0x2024
#define REG_STAGE_10_TIME	0x2026
#define REG_STAGE_11_TEMP	0x2028
#define REG_STAGE_11_TIME	0x202A
#define REG_STAGE_12_TEMP	0x202C
#define REG_STAGE_12_TIME	0x202E
#define REG_STAGE_13_TEMP	0x2030
#define REG_STAGE_13_TIME	0x2032
#define REG_STAGE_14_TEMP	0x2034
#define REG_STAGE_14_TIME	0x2036
#define REG_STAGE_15_TEMP	0x2038
#define REG_STAGE_15_TIME	0x203A
#define REG_STAGE_16_TEMP	0x203C
#define REG_STAGE_16_TIME	0x203E
#define REG_STAGE_17_TEMP	0x2040
#define REG_STAGE_17_TIME	0x2042
#define REG_STAGE_18_TEMP	0x2044
#define REG_STAGE_18_TIME	0x2046
#define REG_STAGE_19_TEMP	0x2048
#define REG_STAGE_19_TIME	0x204A
#define REG_STAGE_20_TEMP	0x204C
#define REG_STAGE_20_TIME	0x204E
#define REG_STAGE_21_TEMP	0x2050
#define REG_STAGE_21_TIME	0x2052
#define REG_STAGE_22_TEMP	0x2054
#define REG_STAGE_22_TIME	0x2056
#define REG_STAGE_23_TEMP	0x2058
#define REG_STAGE_23_TIME	0x205A
#define REG_STAGE_24_TEMP	0x205C
#define REG_STAGE_24_TIME	0x205E
#define REG_STAGE_25_TEMP	0x2060
#define REG_STAGE_25_TIME	0x2062
#define REG_STAGE_26_TEMP	0x2064
#define REG_STAGE_26_TIME	0x2066
#define REG_STAGE_27_TEMP	0x2068
#define REG_STAGE_27_TIME	0x206A
#define REG_STAGE_28_TEMP	0x206C
#define REG_STAGE_28_TIME	0x206E
#define REG_STAGE_29_TEMP	0x2070
#define REG_STAGE_29_TIME	0x2072
#define REG_STAGE_30_TEMP	0x2074
#define REG_STAGE_30_TIME	0x0276


#define FUN_WRITE	6
#define FUN_READ	3

#define MAX_STAGE_NUMBER		29
#define MAX_CLOSE_COUNTER		1024

#define INITIAL_TIME 3
#define INITIAL_TEMP 250

#define LAST_MINUTE	1

#define DEFAULT_STAGE_TIME	120

typedef struct _WRITE_BUFFER
{
	BYTE bDevAddr;
	BYTE bFunNum;
	BYTE bStartRegHi;
	BYTE bStartRegLo;
	BYTE bDataHi;
	BYTE bDataLo;
	BYTE CRCHi;
	BYTE CRCLo;
}WRITE_BUFFER;

typedef struct _READ_BUFFER
{
	BYTE bDevAddr;
	BYTE bFunNum;
	BYTE bDataNum;
	BYTE bData[0];
}READ_BUFFER;

typedef struct _TEMP_CHAMBER *PTEMP_CHAMBER;
//typedef struct _TEMP_CHAMBER *HTCHAMBER;
typedef struct _TEMP_CHAMBER
{
	HANDLE hCom;
	char   szTempChamberModel[MAX_MODEL_STRING_LENGTH];
	HANDLE *phThread;
	HANDLE hTimer;
	DWORD(WINAPI* pfun)(LPARAM lParam);
	DWORD dwTempChamberStatus;
	WORD wSetTemp;
	WORD wRealTemp;
	DWORD dwPresentStage;
	DWORD dwPrevStageForShtdn;
	WORD wOldRemainingTime;
	WORD wRemainingTime;
	WORD wMinStageTime;
	DWORD dwTimerPeriod;
	DWORD dwStageIndex;
	DWORD dwData1;
	DWORD dwData2;
	struct
	{
		WORD wTemp;
		WORD wTime;
	}StageData[30];
}TEMP_CHAMBER, *HTCHAMBER;


//__declspec(dllexport) LRESULT DllSetTemperature(HTCHAMBER hTempChamber, DWORD dwTemp);
//__declspec(dllexport) LRESULT DllReadTemperautre(HTCHAMBER hTempChamber, DWORD *dwTemp);

HRESULT WINAPI TempChamberCtrl(LPARAM lParam);
//HRESULT WINAPI SimpleTimer(LPARAM lParam);
HRESULT ReadChamberRegister(HTCHAMBER hTempChamber, BYTE ucDevAddr, WORD wRegAddress, WORD *pwData);
#define WriteChamberRegister(hTempChamber, ucDevAddr, wRegAddress, wData) DllWriteToChamber(hTempChamber, ucDevAddr, wRegAddress, wData)
HRESULT SetTempChamberStage(HANDLE hTempChamber, WORD wTemp, WORD wTime, DWORD dwStage);
HRESULT SetTempChamberStageTime(HTCHAMBER hTempChamber, WORD wTime, DWORD dwStage);
HRESULT SetTempChamberStageTemp(HTCHAMBER hTempChamber, WORD wTemp, DWORD dwStage);
void CALLBACK TimerCallBack(PVOID lpParameter, BOOLEAN bTimerOrWaitFired);
void ClearThreadMessageQueue();
