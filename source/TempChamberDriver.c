#include <Windows.h>
#include "TempChamberDriver.h"
#include "TempChamberDriver_Private.h"
#include "DebugPrint.h"

HANDLE hThread = NULL, hTimerQueue = NULL;
DWORD dwThreadID = 0;
BOOL bTempChamberCtrlThreadMsgQueueCreated = FALSE;
HANDLE hMsgQueueCreateEvent = NULL;
char szYiHengModelString[MAX_MODEL_STRING_LENGTH] = "BluePard";
DWORD dwRefCounter = 0;

BOOL APIENTRY DllMain(HMODULE hMoudle/* hModule */, DWORD ul_reason_for_call, LPVOID lParam/* lpReserved */)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		if(!hMsgQueueCreateEvent)
			hMsgQueueCreateEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

		if(!hThread)
			hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)TempChamberCtrl, &hMoudle, 0, &dwThreadID);

		if (!hTimerQueue)
			hTimerQueue = CreateTimerQueue();

		if (!hThread || !hMsgQueueCreateEvent || !hTimerQueue)
			return FALSE;

		dwRefCounter++;
		//WaitForSingleObject(hMsgQueueCreateEvent, INFINITE);
		break;
	case DLL_THREAD_ATTACH:
		//if (!hThread)
			//hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)TempChamberCtrl, &hMoudle, 0, &dwThreadID);

		//if (!hThread)
			//return FALSE;

		break;
	case DLL_THREAD_DETACH:
		//if (hThread)
		//{
		//	PostThreadMessage(dwThreadID, TC_COMMAND_EXIT, 0, 0);
		//	hThread = NULL;
		//}
		break;
	case DLL_PROCESS_DETACH:

		if (dwRefCounter)
			dwRefCounter--;
		
		if (dwRefCounter)
			break;

		if (hMsgQueueCreateEvent)
			CloseHandle(hMsgQueueCreateEvent);

		if (hThread)
		{
			PostThreadMessage(dwThreadID, TC_COMMAND_EXIT, 0, 0);
			//hThread = NULL;
		}

		if (hTimerQueue)
			DeleteTimerQueue(hTimerQueue);
		hTimerQueue = NULL;

		break;
	}
	return TRUE;
}

unsigned int crc_chk(unsigned char *data, unsigned char length)
{
	int j; unsigned int reg_crc = 0xffff;
	while (length--) {
		reg_crc ^= *data++;
		for (j = 0; j<8; j++) {
			if (reg_crc & 0x01) {
				reg_crc = (reg_crc >> 1) ^ 0xa001;
			}
			else {
				reg_crc = reg_crc >> 1;
			}
		}
	}
	return reg_crc;
}

HANDLE DllOpenTempChamber(LPSTR szCOMStr, LPSTR szTempChamberModel)
{
	HANDLE hCom;
	COMMTIMEOUTS TimeOuts;
	DCB DeviceControlBlock;
	DWORD dwError;

	DWORD dwResult = WaitForSingleObject(hMsgQueueCreateEvent, 1000);

	if (dwResult != WAIT_OBJECT_0)
		return NULL;

	if (NULL == szCOMStr)
		return NULL;

	HTCHAMBER hTempChamber = malloc(sizeof(TEMP_CHAMBER));
	if (!hTempChamber)
		return NULL;

	memset(hTempChamber, 0, sizeof(TEMP_CHAMBER));

	strcpy(hTempChamber->szTempChamberModel, szTempChamberModel);

	if (strstr(hTempChamber->szTempChamberModel, szYiHengModelString))
	{
		hCom = CreateFile(szCOMStr, \
			GENERIC_READ | GENERIC_WRITE, \
			0, \
			0, \
			OPEN_EXISTING, \
			FILE_ATTRIBUTE_NORMAL, \
			0);	//同步方式打开串口

		if (NULL == hCom)
			return NULL;

		GetCommTimeouts(hCom, &TimeOuts);
		TimeOuts.ReadTotalTimeoutConstant = 800; // 3000;
		TimeOuts.ReadIntervalTimeout = 800; // 1000;
		TimeOuts.ReadTotalTimeoutMultiplier = 800; //1000;
		SetCommTimeouts(hCom, &TimeOuts);
		GetCommState(hCom, &DeviceControlBlock);
		DeviceControlBlock.BaudRate = CBR_9600;
		DeviceControlBlock.ByteSize = 8;
		DeviceControlBlock.fParity = FALSE;
		DeviceControlBlock.Parity = NOPARITY;
		DeviceControlBlock.StopBits = ONESTOPBIT;
		SetCommState(hCom, &DeviceControlBlock);

		hTempChamber->hCom = hCom;
		hTempChamber->pfun = TempChamberCtrl;

		if (hThread)
			hTempChamber->phThread = &hThread;
		else
			return NULL;

		READ_DATA *DummyData = NULL;
		if (DllReadChamberData(hTempChamber, DEV_ADDR, REG_REAL_TEMP, 1, &DummyData))
		{
			PostThreadMessage(dwThreadID, TC_COMMAND_CREATE, (WPARAM)hTempChamber, 0);
			dwError = GetLastError();
		}
		if (DummyData)
			free(DummyData);
	}
	return (HANDLE)hTempChamber;
}

HRESULT DllInitTempChamber(HANDLE hTChamber)
{
	if (!hTChamber)
		return ERROR_INVALID_HANDLE;

	HTCHAMBER hTempChamber = (HTCHAMBER)hTChamber;

	if (!(hTempChamber->dwTempChamberStatus & TC_STATUS_OPENED) || hTempChamber->dwTempChamberStatus & TC_STATUS_INITALISE_FAIL)
		return ERROR_DEVICE_NOT_AVAILABLE;

	BOOL bResult = PostThreadMessage(dwThreadID, TC_COMMAND_INIT, (WPARAM)hTempChamber, 0);
	if (!bResult)
	{
		return ERROR_FUNCTION_FAILED;
	}

	return ERROR_SUCCESS;
}

BOOL DllWriteToChamber(HANDLE hTempChamber, BYTE bDevAddr, WORD wStartRegAddr, WORD wData)
{
	HANDLE hCom;
	WRITE_BUFFER WriteBlock;
	WORD wCRC;
	DWORD dwNumOfByte;
	BOOL bResult = FALSE;

	hCom = ((HTCHAMBER)hTempChamber)->hCom;

	WriteBlock.bDevAddr = bDevAddr;
	WriteBlock.bFunNum = FUN_WRITE;
	WriteBlock.bStartRegHi = HIBYTE(wStartRegAddr);
	WriteBlock.bStartRegLo = LOBYTE(wStartRegAddr);
	WriteBlock.bDataHi = HIBYTE(wData);
	WriteBlock.bDataLo = LOBYTE(wData);
	wCRC = crc_chk((char *)&WriteBlock, sizeof(WRITE_BUFFER) - 2);
	WriteBlock.CRCHi = HIBYTE(wCRC);
	WriteBlock.CRCLo = LOBYTE(wCRC);
	bResult = WriteFile(hCom, &WriteBlock, sizeof(WRITE_BUFFER), &dwNumOfByte, NULL);
	return bResult;
}

BOOL DllReadChamberData(HANDLE hTempChamber, BYTE bDevAddr, WORD wStartRegAddr, WORD NumOfReg, READ_DATA **ppReadData)
{
	READ_BUFFER *pReadBuffer;
	WRITE_BUFFER WriteBuffer;
	HANDLE hCom;
	DWORD dwNumOfByte;
	WORD wCRC;
	BOOL bResult = FALSE;

	*ppReadData = NULL;

	if (!hTempChamber || !(((HTCHAMBER)hTempChamber)->hCom))
		return FALSE;

	hCom = ((HTCHAMBER)hTempChamber)->hCom;

	WriteBuffer.bDevAddr = bDevAddr;
	WriteBuffer.bFunNum = FUN_READ;
	WriteBuffer.bStartRegHi = HIBYTE(wStartRegAddr);
	WriteBuffer.bStartRegLo = LOBYTE(wStartRegAddr);
	WriteBuffer.bDataHi = HIBYTE(NumOfReg);
	WriteBuffer.bDataLo = LOBYTE(NumOfReg);
	wCRC = crc_chk((char *)&WriteBuffer, sizeof(WRITE_BUFFER) - 2);
	WriteBuffer.CRCHi = HIBYTE(wCRC);
	WriteBuffer.CRCLo = LOBYTE(wCRC);
	bResult = WriteFile(hCom, &WriteBuffer, sizeof(WRITE_BUFFER), &dwNumOfByte, NULL);
	if (bResult)
	{
		pReadBuffer = malloc(NumOfReg * sizeof(WORD) + 5);

		if (NULL == pReadBuffer)
			return FALSE;

		bResult &= ReadFile(hCom, pReadBuffer, NumOfReg * 2 + 5, &dwNumOfByte, NULL);
		if (!bResult)
			return bResult;
		if (0 == dwNumOfByte)
			bResult = 0;
	}
	else
		return bResult;

	*ppReadData = malloc(sizeof(UINT) + NumOfReg * sizeof(WORD));
	if (!*ppReadData)
	{
		free(pReadBuffer);
		return FALSE;
	}

	(*ppReadData)->uiNumData = NumOfReg;
	for (int i = 0; i < NumOfReg; i++)
	{
		(*ppReadData)->wData[i] = (pReadBuffer->bData[2 * i] << 8) + pReadBuffer->bData[2 * i + 1];
	}

	free(pReadBuffer);

	return bResult;
}

LRESULT DllCloseTempChamber(HANDLE hTempChamber)
{
	DWORD dwWaitCounter = MAX_CLOSE_COUNTER;

	if (!hTempChamber)
		return ERROR_INVALID_HANDLE;

	((HTCHAMBER)hTempChamber)->dwTempChamberStatus |= TC_STATUS_DEVICE_CLOSING;

	if (((HTCHAMBER)hTempChamber)->hTimer)
	{
		DeleteTimerQueueTimer(hTimerQueue, ((HTCHAMBER)hTempChamber)->hTimer, INVALID_HANDLE_VALUE);
		Sleep(100);
	}

	PostThreadMessage(dwThreadID, TC_COMMAND_CLOSE, (WPARAM)(&hTempChamber), 0);

	while (dwWaitCounter)
	{
		Sleep(200);

		if (!hTempChamber)
			return S_OK;

		dwWaitCounter--;
	}
	return S_FALSE;
}

HRESULT WINAPI TempChamberCtrl(LPARAM lParam)
{
	MSG msg;
	HTCHAMBER hTempChamber = NULL;
	BOOL bResult;

	PeekMessage(&msg, (HWND)-1, WM_USER, WM_USER, PM_NOREMOVE);		//enforce system to create a message queue for this thread
	SetEvent(hMsgQueueCreateEvent);

	while (bResult = GetMessage(&msg, (HWND)-1, 0, 0) != 0)
	{
		switch (msg.message)
		{
		case TC_COMMAND_CREATE:
			hTempChamber = (HTCHAMBER)(msg.wParam);
			if (!hTempChamber)
				break;

			hTempChamber->dwTempChamberStatus = 0;
			hTempChamber->dwTempChamberStatus |= TC_STATUS_OPENED;
			hTempChamber->dwTempChamberStatus |= TC_STATUS_INITIALISNG;
			hTempChamber->dwStageIndex = 0;
			hTempChamber->dwTimerPeriod = 1000;
			hTempChamber->dwPresentStage = 0;				//assume inital stage is 0, the stage index is base from 0
			/*
			for (int i = 0; i < 30; i++)
			{
			//Sleep(1000);
			if (S_OK != SetTempChamberStage(hTempChamber, INITIAL_TEMP, INITIAL_TIME, i))
			break;
			}

			*/

			hTempChamber->dwPresentStage = 0;

			for (int i = 0; i <= MAX_STAGE_NUMBER; i++)
			{
				hTempChamber->StageData[i].wTime = INITIAL_TIME;
				hTempChamber->StageData[i].wTemp = INITIAL_TEMP;
			}

			CreateTimerQueueTimer(&(hTempChamber->hTimer), hTimerQueue, TimerCallBack, hTempChamber, 1000, 1000, WT_EXECUTEDEFAULT);

			//ReadChamberRegister(hTempChamber, DEV_ADDR, REG_REM_TIME, &(hTempChamber->dwRemainingTime));
			break;

		case TC_COMMAND_INIT:
			hTempChamber = (HTCHAMBER)(msg.wParam);
			if (!hTempChamber)
				break;

			hTempChamber->dwTempChamberStatus = 0;
			hTempChamber->dwTempChamberStatus |= TC_STATUS_OPENED;
			hTempChamber->dwTempChamberStatus |= TC_STATUS_INITIALISNG;
			hTempChamber->dwStageIndex = 0;
			hTempChamber->dwTimerPeriod = 1000;
			hTempChamber->dwPrevStageForShtdn = hTempChamber->dwPresentStage;
			hTempChamber->dwPresentStage = 0;				//assume inital stage is 0, the stage index is base from 0
															/*
															for (int i = 0; i < 30; i++)
															{
															//Sleep(1000);
															if (S_OK != SetTempChamberStage(hTempChamber, INITIAL_TEMP, INITIAL_TIME, i))
															break;
															}

															*/

			//hTempChamber->dwPresentStage = 0;

			for (int i = 0; i <= MAX_STAGE_NUMBER; i++)
			{
				hTempChamber->StageData[i].wTime = INITIAL_TIME;
				hTempChamber->StageData[i].wTemp = INITIAL_TEMP;
			}

			//CreateTimerQueueTimer(&(hTempChamber->hTimer), hTimerQueue, TimerCallBack, hTempChamber, 1000, 1000, WT_EXECUTEDEFAULT);

			//ReadChamberRegister(hTempChamber, DEV_ADDR, REG_REM_TIME, &(hTempChamber->dwRemainingTime));
			break;

		case TC_COMMAND_EXIT:
			/*
			if (!hTempChamber)
			return S_OK;
			else
			{
			if(hTempChamber->hCom)
			CloseHandle(hTempChamber->hCom);

			free(hTempChamber);
			return S_OK;
			}

			*/
			return S_OK;
			break;
		case TC_COMMAND_TIMER:
			hTempChamber = (HTCHAMBER)(msg.wParam);
#ifdef RELEASE_DEBUG
			DbgPrint("BMW: PresentStage is %d\n		The status of Temp Chamber is %08X\n", hTempChamber->dwPresentStage, hTempChamber->dwTempChamberStatus);
#endif // RELEASE_DEBUG

			if (!hTempChamber)
				break;

			if (!hTempChamber->hCom)
				break;

			if (hTempChamber->dwTempChamberStatus & TC_STATUS_INITALISE_FAIL)
				break;
			//初始化阶段，因为时间较长，需要30秒左右，所以放到这里处理
			if ((hTempChamber->dwTempChamberStatus & TC_STATUS_INITIALISNG) && !(hTempChamber->dwTempChamberStatus & TC_STATUS_INITIALISE_SUCCESS))
			{
				int i = hTempChamber->dwStageIndex;
				if (S_OK == SetTempChamberStage(hTempChamber, hTempChamber->StageData[i].wTemp, hTempChamber->StageData[i].wTime, hTempChamber->dwStageIndex))
				{
					if (hTempChamber->dwStageIndex < MAX_STAGE_NUMBER)
					{
						hTempChamber->dwStageIndex++;
#ifdef RELEASE_DEBUG
						DbgPrint("BMW: Initialising TempChamber, StageIndex is %d\n", hTempChamber->dwStageIndex);
#endif // RELEASE_DEBUG

					}
					else
					{
						Sleep(100);
						HRESULT hr = ReadChamberRegister(hTempChamber, DEV_ADDR, REG_REM_TIME, &(hTempChamber->wRemainingTime));
#ifdef RELEASE_DEBUG
						DbgPrint("BMW: 1st time after ReadChamberRegister, hr = %d, RemainingTime = %d\n", hr, hTempChamber->wRemainingTime);
#endif // RELEASE_DEBUG
						hTempChamber->wOldRemainingTime = hTempChamber->wRemainingTime;
						hTempChamber->dwTempChamberStatus |= TC_STATUS_INITIALISE_SUCCESS;
						hTempChamber->dwTempChamberStatus &= ~TC_STATUS_INITALISE_FAIL;
						hTempChamber->dwTempChamberStatus &= ~TC_STATUS_INITIALISNG;
						hTempChamber->dwTempChamberStatus &= ~TC_STATUS_STARTED;
						hTempChamber->dwStageIndex = 0;
						hTempChamber->dwPresentStage = -1;
					}
				}
				else
				{
					hTempChamber->dwTempChamberStatus |= TC_STATUS_INITALISE_FAIL;
					hTempChamber->dwStageIndex = 0;
					hTempChamber->dwPresentStage = 0;
					PurgeComm(hTempChamber->hCom, PURGE_RXABORT | PURGE_RXCLEAR | PURGE_TXABORT | PURGE_TXCLEAR);
					ClearThreadMessageQueue();

#ifdef RELEASE_DEBUG
					DbgPrint("BMW: StageIndex is %d\n", hTempChamber->dwStageIndex);
					DbgPrint("BMW: Initialise Fail\n");
#endif // RELEASE_DEBUG
				}
#ifdef RELEASE_DEBUG
				DbgPrint("BMW:After SetTempChamberStage\n");
#endif // RELEASE_DEBUG
				break;
			}

			//判断温箱是否已经启动，如果剩余时间有变化，就说明温箱已启动。
			if ((hTempChamber->dwTempChamberStatus & TC_STATUS_INITIALISE_SUCCESS))
			{
#ifdef RELEASE_DEBUG
				DbgPrint("BMW: before ReadChamberRegister\n");
#endif // RELEASE_DEBUG
				PurgeComm(hTempChamber->hCom, PURGE_RXABORT | PURGE_RXCLEAR | PURGE_TXABORT | PURGE_TXCLEAR);
				HRESULT hr;
				hr = ReadChamberRegister(hTempChamber, DEV_ADDR, REG_REM_TIME, &(hTempChamber->wRemainingTime));
				if (!(hTempChamber->wRemainingTime))
					hTempChamber->dwTempChamberStatus &= ~TC_STATUS_AWAITING_TERMINATION;
#ifdef RELEASE_DEBUG
				DbgPrint("BMW: after ReadChamberRegister, hr = %d, RemainingTime = %d\n", hr, hTempChamber->wRemainingTime);
#endif // RELEASE_DEBUG

				if (S_OK != hr)
				{
					ClearThreadMessageQueue();			//clear the message in the message queue, the message can be blocked due to delay caused by synchronise operation of UART, discard all these messages so that the message won't get blocked
				}
				else
				{
					if (!(hTempChamber->dwTempChamberStatus & TC_STATUS_STARTED))
					{//温箱还没有置为启动状态
						if ((hTempChamber->wOldRemainingTime != hTempChamber->wRemainingTime)
							&& (hTempChamber->wRemainingTime != 0)
							&& (hTempChamber->dwPresentStage == -1)
							&& !(hTempChamber->dwTempChamberStatus & TC_STATUS_AWAITING_TERMINATION))
						{

							hTempChamber->dwTempChamberStatus |= TC_STATUS_STARTED;
							hTempChamber->dwPresentStage++;								//第一次检测到剩余时间变化，不管变大或变小，都把dwPresentStage变成0;
						}
					}
					else
					{//温箱已启动
						if (hTempChamber->wOldRemainingTime < hTempChamber->wRemainingTime\
							&& hTempChamber->dwPresentStage < MAX_STAGE_NUMBER)		//需要防止dwPresentStage超过29 19/7/8
						{
							hTempChamber->dwPresentStage++;								//每次剩余时间变多时，表示温箱进入下一阶段
						}
						else
						{
							if (hTempChamber->dwPresentStage >= MAX_STAGE_NUMBER)
								break;
						}

						if ((hTempChamber->wOldRemainingTime > hTempChamber->wRemainingTime) 
							&& (LAST_MINUTE == hTempChamber->wRemainingTime)
							|| (hTempChamber->dwTempChamberStatus & TC_STATUS_LAST_MINUTES_UPDATE_MISSED))				////这时要为进入下一阶段作准备或者如果错过了更新时机
						{
							//检查是否正在处理温度设置命令
							if (!(hTempChamber->dwTempChamberStatus & TC_STATUS_TEMPERATURE_SET))
							{
								//WORD wNextStageTime;
								//ReadChamberRegister(hTempChamber, DEV_ADDR, (WORD)(REG_STAGE_1_TIME + hTempChamber->dwPresentStage + 1) * sizeof(DWORD), &wNextStageTime);
								//if (wNextStageTime <= INITIAL_TIME)
								DWORD dwPresentStage = hTempChamber->dwPresentStage;
								Sleep(100);
								hr = SetTempChamberStageTime(hTempChamber, hTempChamber->StageData[dwPresentStage + 1].wTime, dwPresentStage + 1);
								if (S_OK == hr)
								{
									Sleep(100);
									hr = SetTempChamberStageTemp(hTempChamber, hTempChamber->StageData[dwPresentStage + 1].wTemp, dwPresentStage + 1);
								}
								else
								{
									hTempChamber->dwTempChamberStatus |= TC_STATUS_LAST_MINUTES_UPDATE_MISSED;
									break;
								}

								if (S_OK != hr)
									break;

								hTempChamber->dwTempChamberStatus &= ~TC_STATUS_LAST_MINUTES_UPDATE_MISSED;
							}
							else
							{
								hTempChamber->dwTempChamberStatus |= TC_STATUS_LAST_MINUTES_UPDATE_MISSED;		//置这一位表示错过了更新时机
																												/*
																												SetTempChamberStageTemp(hTempChamber, hTempChamber->wSetTemp, hTempChamber->dwPresentStage + 1);
																												//SetTempChamberStageTemp(hTempChamber, hTempChamber->wSetTemp, hTempChamber->dwPresentStage + 2);
																												SetTempChamberStageTime(hTempChamber, INITIAL_TIME, hTempChamber->dwPresentStage + 1);
																												//SetTempChamberStageTime(hTempChamber, hTempChamber->wMinStageTime, hTempChamber->dwPresentStage + 2);

																												hTempChamber->dwTempChamberStatus &= ~TC_STATUS_TEMPERATURE_SET;
																												hTempChamber->dwTempChamberStatus |= TC_STATUS_TEMPERATURE_SET_DONE;
																												*/
							}		//最后一分钟收到了改变温度的命令，就把下一段温度改为设置温度，同时把下下段时间改为设置的维时间
						}
					}
				}
				hTempChamber->wOldRemainingTime = hTempChamber->wRemainingTime;
			}
			break;
		case TC_COMMAND_CLOSE:
		{   HTCHAMBER *phTempChamber;
			phTempChamber = (HTCHAMBER *)(msg.wParam);

			if (!phTempChamber)
				break;

			if (!((*phTempChamber)->dwTempChamberStatus & TC_STATUS_INITALISE_FAIL))
			{
				Sleep(100);
				SetTempChamberStageTime(*phTempChamber, 0, (*phTempChamber)->dwPresentStage + 1);
			}

			if ((*phTempChamber)->hCom)
				CloseHandle((*phTempChamber)->hCom);

			free(*phTempChamber);
			*phTempChamber = NULL;

			break;

		}
		default:
			break;
		}
	}
	return S_OK;
}

HRESULT SetTempChamberStage(HANDLE hTempChamber, WORD wTemp, WORD wTime, DWORD dwStage)
{
	HRESULT hr;
	BOOL bResult;

	if (!hTempChamber)
		return ERROR_INVALID_HANDLE;

	if (dwStage > MAX_STAGE_NUMBER)
		return ERROR_PARAMETER_QUOTA_EXCEEDED;

	((HTCHAMBER)hTempChamber)->StageData[dwStage].wTemp = wTemp;
	((HTCHAMBER)hTempChamber)->StageData[dwStage].wTime = wTime;

	DllWriteToChamber(hTempChamber, DEV_ADDR, REG_STAGE_1_TEMP + (WORD)dwStage * sizeof(DWORD), wTemp);
	DllWriteToChamber(hTempChamber, DEV_ADDR, REG_STAGE_1_TIME + (WORD)dwStage * sizeof(DWORD), wTime);

#ifdef RELEASE_DEBUG
	DbgPrint("BMW: in SetTempChmagerStage Function\n");
#endif // RELEASE_DEBUG

	READ_DATA *pReadData;
	bResult = DllReadChamberData(hTempChamber, DEV_ADDR, REG_STAGE_1_TEMP + (WORD)dwStage * sizeof(DWORD), 2, &pReadData);

	if (!pReadData)
	{
#ifdef RELEASE_DEBUG
		DbgPrint("BMW: Read Chamber Data Fail\n");
#endif // RELEASE_DEBUG
		return S_FALSE;
	}
	if (pReadData->wData[0] == wTemp && pReadData->wData[1] == wTime && bResult)
		hr = S_OK;
	else
		hr = S_FALSE;

	free(pReadData);
#ifdef RELEASE_DEBUG
	DbgPrint("BMW: leave SetTempChmagerStage Function\n");
#endif // RELEASE_DEBUG

	return hr;
}

HRESULT SetTempChamberStageTime(HTCHAMBER hTempChamber, WORD wTime, DWORD dwStage)
{
	HRESULT hr;
	BOOL br;

	if (!hTempChamber)
		return ERROR_INVALID_HANDLE;

	if (dwStage > MAX_STAGE_NUMBER)
		return ERROR_PARAMETER_QUOTA_EXCEEDED;

	hTempChamber->StageData[dwStage].wTime = wTime;

	br = DllWriteToChamber(hTempChamber, DEV_ADDR, REG_STAGE_1_TIME + (WORD)dwStage * sizeof(DWORD), wTime);

	if (!br)
		return ERROR_FUNCTION_FAILED;

	READ_DATA *pReadData;
	br = DllReadChamberData(hTempChamber, DEV_ADDR, REG_STAGE_1_TIME + (WORD)dwStage * sizeof(DWORD), 1, &pReadData);
	if (!br)
		return ERROR_FUNCTION_FAILED;

	if (pReadData->wData[0] == wTime)
		hr = S_OK;
	else
		hr = S_FALSE;

	free(pReadData);

	return hr;
}

HRESULT SetTempChamberStageTemp(HTCHAMBER hTempChamber, WORD wTemp, DWORD dwStage)
{
	HRESULT hr;

	if (!hTempChamber)
		return ERROR_INVALID_HANDLE;

	if (dwStage > MAX_STAGE_NUMBER)
		return ERROR_PARAMETER_QUOTA_EXCEEDED;

	hTempChamber->StageData[dwStage].wTemp = wTemp;

	DllWriteToChamber(hTempChamber, DEV_ADDR, REG_STAGE_1_TEMP + (WORD)dwStage * sizeof(DWORD), wTemp);

	READ_DATA *pReadData;
	DllReadChamberData(hTempChamber, DEV_ADDR, REG_STAGE_1_TEMP + (WORD)dwStage * sizeof(DWORD), 1, &pReadData);

	if (pReadData->wData[0] == wTemp)
		hr = S_OK;
	else
		hr = S_FALSE;

	free(pReadData);

	return hr;
}
/*
HRESULT WINAPI SimpleTimer(LPARAM lParam)
{
while (TRUE)
{
Sleep(*(WORD *)lParam);
PostThreadMessage(dwThreadID, TC_COMMAND_TIMER, 0, 0);
#ifdef RELEASE_DEBUG
DbgPrint("BMW:TC_COMMAND_TIMER Message Posted");
#endif
}
}

*/

HRESULT ReadChamberRegister(HTCHAMBER hTempChamber, BYTE ucDevAddr, WORD wRegAddress, WORD *pwData)
{
	BOOL bResult;

	if (NULL == hTempChamber)
		return ERROR_INVALID_HANDLE;

	READ_DATA *pReadData;

	bResult = DllReadChamberData(hTempChamber, ucDevAddr, wRegAddress, 1, &pReadData);

	if (!bResult)
	{
		if (pReadData)
			free(pReadData);

		return ERROR_FUNCTION_FAILED;
	}

	if (pReadData)
	{
		*pwData = pReadData->wData[0];
		free(pReadData);
		return S_OK;
	}
	else
		return ERROR_FUNCTION_FAILED;
}

LRESULT DllSetTemperature(HANDLE hTempChamber, INT iTemperature, INT iMinTime)
{
	int i;
	if (!hTempChamber)
		return ERROR_INVALID_HANDLE;

	if (((HTCHAMBER)hTempChamber)->dwTempChamberStatus & TC_STATUS_TEMPERATURE_SET)
		return ERROR_DEVICE_IN_USE;

	if (!(((HTCHAMBER)hTempChamber)->dwTempChamberStatus & TC_STATUS_OPENED))
		return ERROR_DEVICE_NOT_AVAILABLE;

	if (iTemperature > MAX_TEMP \
		|| iTemperature < MIN_TEMP \
		|| iMinTime > MAX_TIME \
		|| ((HTCHAMBER)hTempChamber)->dwPresentStage >= MAX_STAGE_NUMBER - 2)
		return ERROR_INVALID_PARAMETER;					//modified in 19/07/08

	((HTCHAMBER)hTempChamber)->dwTempChamberStatus |= TC_STATUS_TEMPERATURE_SET;

	((HTCHAMBER)hTempChamber)->wSetTemp = (WORD)(iTemperature * 10);
	((HTCHAMBER)hTempChamber)->wMinStageTime = (WORD)iMinTime;

	i = ((HTCHAMBER)hTempChamber)->dwPresentStage + 1;

	((HTCHAMBER)hTempChamber)->StageData[i].wTemp = ((HTCHAMBER)hTempChamber)->wSetTemp;
	((HTCHAMBER)hTempChamber)->StageData[i].wTime = INITIAL_TIME;

	for(int i = ((HTCHAMBER)hTempChamber)->dwPresentStage + 2; i < MAX_STAGE_NUMBER; i++)
	{
		((HTCHAMBER)hTempChamber)->StageData[i].wTemp = ((HTCHAMBER)hTempChamber)->wSetTemp;
		((HTCHAMBER)hTempChamber)->StageData[i].wTime = ((HTCHAMBER)hTempChamber)->wMinStageTime;
	}

	((HTCHAMBER)hTempChamber)->StageData[MAX_STAGE_NUMBER].wTemp = ((HTCHAMBER)hTempChamber)->wSetTemp;	//modified in 19/07/08
	((HTCHAMBER)hTempChamber)->StageData[MAX_STAGE_NUMBER].wTime = MAX_TIME;							//modified in 19/07/08

	((HTCHAMBER)hTempChamber)->dwTempChamberStatus &= ~TC_STATUS_TEMPERATURE_SET;

	return S_OK;
}

void CALLBACK TimerCallBack(PVOID lpParameter, BOOLEAN bTimerOrWaitFired)
{
	HTCHAMBER hTempChamber = (HTCHAMBER)lpParameter;
	if(dwThreadID && !(hTempChamber->dwTempChamberStatus & TC_STATUS_DEVICE_CLOSING))
		PostThreadMessage(dwThreadID, TC_COMMAND_TIMER, (WPARAM)hTempChamber, 0);

#ifdef RELEASE_DEBUG
	DbgPrint("BMW:TC_COMMAND_TIMER Message Posted\n");
#endif
}

void ClearThreadMessageQueue()
{
	MSG Msg;
	while (PeekMessage(&Msg, (HWND)-1, 0, 0, PM_REMOVE))
	{
		if (Msg.message == TC_COMMAND_CLOSE)
		{
			PostThreadMessage(dwThreadID, Msg.message, Msg.wParam, Msg.lParam);
			break;
		}

#ifdef RELEASE_DEBUG
		DbgPrint("BMW:Clearing Message...\n");
#endif // RELEASE_DEBUG

	}
}

DWORD DllGetChamberStatus(HANDLE hTempChamber)
{
	if (hTempChamber)
		return(((HTCHAMBER)hTempChamber)->dwTempChamberStatus);
	else
		return 0;
}

HRESULT DllShutdownTempChmaber(HANDLE hTempChamber)
{
	HRESULT hr;

	if (!hTempChamber)
		return ERROR_INVALID_HANDLE;

	if (!(((HTCHAMBER)hTempChamber)->dwTempChamberStatus & TC_STATUS_OPENED) || ((HTCHAMBER)hTempChamber)->dwTempChamberStatus & TC_STATUS_INITALISE_FAIL)
		return ERROR_DEVICE_NOT_AVAILABLE;

	Sleep(100);
	hr = SetTempChamberStageTime(hTempChamber, 0, ((HTCHAMBER)hTempChamber)->dwPrevStageForShtdn + 1);

	((HTCHAMBER)hTempChamber)->dwTempChamberStatus &= ~TC_STATUS_STARTED;
	((HTCHAMBER)hTempChamber)->dwTempChamberStatus |= TC_STATUS_AWAITING_TERMINATION;
	return hr;
}