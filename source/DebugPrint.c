#include <stdio.h>
#include <stdlib.h>
//#include <iostream>
#include <Windows.h>
#include <tchar.h>
#include "DebugPrint.h"

void DbgPrintT(const TCHAR *format, ...)
{
    TCHAR buf[4096];
    va_list args;
    va_start(args, format);
    int len = _vstprintf(buf, format, args);
    va_end(args);
    OutputDebugString(buf);
}

void DbgPrint(const char *format, ...)
{
    char buf[4096], *p = buf;
    va_list args;
    va_start(args, format);
    p += _vsnprintf(p, sizeof buf - 1, format, args);
    va_end(args);
    OutputDebugStringA(buf);
}
