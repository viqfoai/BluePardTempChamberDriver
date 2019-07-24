#pragma once

#define TC_COMMAND_CREATE	WM_USER + 100
#define TC_COMMAND_EXIT		WM_USER + 101
#define TC_COMMAND_TIMER	WM_USER + 102
#define TC_COMMAND_CLOSE	WM_USER + 103
#define TC_COMMAND_INIT		WM_USER + 104

#define MAX_TEMP	60
#define MIN_TEMP	-40
#define MAX_TIME	9999

//define temp chamber status
#define TC_STATUS_INITIALISNG			0x00000001
#define TC_STATUS_STARTED				0x00000002
#define TC_STATUS_INITIALISE_SUCCESS	0x00000004
#define TC_STATUS_INITALISE_FAIL		0x00000008
#define TC_STATUS_OPENED				0x00000010
#define TC_STATUS_TEMPERATURE_SET		0x00000020			//SetTemperatureCommand has been issued, but not excuted, this bit should be cleared once temperatrue has been set successfully
#define TC_STATUS_LAST_MINUTES_UPDATE_MISSED	0x00000040			//SetTemperatureCommand has been executed.
#define TC_STATUS_DEVICE_CLOSING		0x00000080
#define TC_STATUS_AWAITING_TERMINATION	0x00000100

#define MAX_MODEL_STRING_LENGTH	1024

typedef struct _READ_DATA
{
	UINT uiNumData;
	WORD wData[0];
}READ_DATA;


__declspec(dllexport) unsigned int crc_chk(unsigned char *data, unsigned char length);
__declspec(dllexport) HANDLE DllOpenTempChamber(LPSTR szCOMStr, LPSTR szTempChamberModel);
__declspec(dllexport) BOOL DllWriteToChamber(HANDLE hTempChamber, BYTE bDevAddr, WORD wStartRegAddr, WORD wData);
__declspec(dllexport) BOOL DllReadChamberData(HANDLE hTempChamber, BYTE bDevAddr, WORD wStartRegAddr, WORD NumOfReg, READ_DATA **ppReadData);
__declspec(dllexport) LRESULT DllCloseTempChamber(HANDLE hTempChamber);
__declspec(dllexport) LRESULT DllSetTemperature(HANDLE hTempChamber, INT iTemperature, INT iMiniTime);
__declspec(dllexport) DWORD DllGetChamberStatus(HANDLE hTempChamber);
__declspec(dllexport) HRESULT DllShutdownTempChmaber(HANDLE hTempChamber);
__declspec(dllexport) HRESULT DllInitTempChamber(HANDLE hTChmaber);
