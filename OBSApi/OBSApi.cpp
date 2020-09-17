/********************************************************************************
 Copyright (C) 2012 Hugh Bailey <obs.jim@gmail.com>

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
********************************************************************************/


#include "OBSAPI.h"
#include "Logging.h"
static Logger *gLogger=NULL;
APIInterface *API = NULL;


void ApplyRTL(HWND hwnd, bool bRTL)
{
    if (!bRTL)
        return;

    TCHAR controlClassName[128];
    GetClassName(hwnd, controlClassName, 128);
    if (scmpi(controlClassName, L"Static") == 0)
    {
        LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
        style ^= SS_RIGHT;
        SetWindowLongPtr(hwnd, GWL_STYLE, style);
    }

    LONG_PTR styles = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    styles ^= WS_EX_RIGHT;
    styles |= WS_EX_RTLREADING;
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, styles);
}  
int OBSMessageBox(HWND hwnd, LPCTSTR lpText, LPCTSTR lpCaption, UINT flags)
{
    if (LocaleIsRTL())
        flags |= MB_RTLREADING | MB_RIGHT;

    return MessageBox(hwnd, lpText, lpCaption, flags);
}
String GetCBText(HWND hwndCombo, UINT id)
{
    UINT curSel = (id != CB_ERR) ? id : (UINT)SendMessage(hwndCombo, CB_GETCURSEL, 0, 0);
    if(curSel == CB_ERR)
        return String();

    String strText;
    strText.SetLength((UINT)SendMessage(hwndCombo, CB_GETLBTEXTLEN, curSel, 0));
    if(strText.Length())
        SendMessage(hwndCombo, CB_GETLBTEXT, curSel, (LPARAM)strText.Array());

    return strText;
}

String GetEditText(HWND hwndEdit)
{
    String strText;
    strText.SetLength((UINT)SendMessage(hwndEdit, WM_GETTEXTLENGTH, 0, 0));
    if(strText.Length())
        SendMessage(hwndEdit, WM_GETTEXT, strText.Length()+1, (LPARAM)strText.Array());

    return strText;
}

static LPBYTE GetBitmapData(HBITMAP hBmp, BITMAP &bmp)
{
    if (!hBmp)
        return NULL;

    if (GetObject(hBmp, sizeof(bmp), &bmp) != 0) {
        UINT bitmapDataSize = bmp.bmHeight*bmp.bmWidth*bmp.bmBitsPixel;
        bitmapDataSize >>= 3;

        LPBYTE lpBitmapData = (LPBYTE)Allocate(bitmapDataSize);
        GetBitmapBits(hBmp, bitmapDataSize, lpBitmapData);

        return lpBitmapData;
    }

    return NULL;
}

static inline BYTE BitToAlpha(LPBYTE lp1BitTex, int pixel, bool bInvert)
{
    BYTE pixelByte = lp1BitTex[pixel/8];
    BYTE pixelVal = pixelByte >> (7-(pixel%8)) & 1;

    if (bInvert)
        return pixelVal ? 0xFF : 0;
    else
        return pixelVal ? 0 : 0xFF;
}

extern LARGE_INTEGER clockFreq;
__declspec(thread) LONGLONG lastQPCTime = 0;

QWORD GetQPCTimeNS()
{
    LARGE_INTEGER currentTime;
    QueryPerformanceCounter(&currentTime);

    if (currentTime.QuadPart < lastQPCTime)
        Log (TEXT("GetQPCTimeNS: WTF, clock went backwards! %I64d < %I64d"), currentTime.QuadPart, lastQPCTime);

    lastQPCTime = currentTime.QuadPart;

    double timeVal = double(currentTime.QuadPart);
    timeVal *= 1000000000.0;
    timeVal /= double(clockFreq.QuadPart);

    return QWORD(timeVal);
}

QWORD GetQPCTime100NS()
{
    LARGE_INTEGER currentTime;
    QueryPerformanceCounter(&currentTime);

    if (currentTime.QuadPart < lastQPCTime)
        Log (TEXT("GetQPCTime100NS: WTF, clock went backwards! %I64d < %I64d"), currentTime.QuadPart, lastQPCTime);

    lastQPCTime = currentTime.QuadPart;

    double timeVal = double(currentTime.QuadPart);
    timeVal *= 10000000.0;
    timeVal /= double(clockFreq.QuadPart);

    return QWORD(timeVal);
}

QWORD GetQPCTimeMS()
{
    LARGE_INTEGER currentTime;
    QueryPerformanceCounter(&currentTime);

    if (currentTime.QuadPart < lastQPCTime)
        Log (TEXT("GetQPCTimeMS: WTF, clock went backwards! %I64d < %I64d"), currentTime.QuadPart, lastQPCTime);

    lastQPCTime = currentTime.QuadPart;

    QWORD timeVal = currentTime.QuadPart;
    timeVal *= 1000;
    timeVal /= clockFreq.QuadPart;

    return timeVal;
}



void OBSAPI_EnterSceneMutex()
{
   API->EnterSceneMutex();
}
void OBSAPI_LeaveSceneMutex()
{
   API->LeaveSceneMutex();
}
XElement* OBSAPI_GetSceneElement()
{
   return API->GetSceneElement();
}
ImageSource* OBSAPI_CreateImageSource(CTSTR lpClassName, XElement *data)
{ 
   return API->CreateImageSource(lpClassName,data);
}
Vect2 OBSAPI_MapFrameToWindowPos(Vect2 framePos)
{
   return API->MapFrameToWindowPos(framePos);
}
Vect2 OBSAPI_GetFrameToWindowScale()
{ 
   return API->GetFrameToWindowScale();
}
Vect2 OBSAPI_MapFrameToWindowSize(Vect2 frameSize)
{ 
   return API->MapFrameToWindowSize(frameSize);
}

CTSTR OBSAPI_GetAppPath()
{
   return API->GetAppPath();
}
String OBSAPI_GetPluginDataPath() 
{
   return API->GetPluginDataPath();
}
UINT OBSAPI_GetAPIVersion()
{
   return 0x0103;
}
Vect2 OBSAPI_GetBaseSize()
{
   UINT width;
   UINT height;
   Vect2 size;
   API->GetBaseSize(width,height);
   size.x=(float)width;
   size.y = (float)height;
   return size;
}
void OBSAPI_GetBaseSize(UINT &width,UINT &height)
{
   API->GetBaseSize(width,height);

}
UINT OBSAPI_GetMaxFPS()
{
   return API->GetMaxFPS();
}
CTSTR OBSAPI_GetLanguage()
{
   return API->GetLanguage();
}
bool OBSAPI_UseMultithreadedOptimizations()
{
   return API->UseMultithreadedOptimizations();
}

void OBSAPI_InterfaceRegister(APIInterface *apiInterface,const wchar_t* logPath)
{
   //InitXT(NULL, L"FastAlloc", L"OBSAPI");
   if (logPath == NULL) {
      SYSTEMTIME loSystemTime;
      GetLocalTime(&loSystemTime);
      wchar_t lwzLogFileName[255] = { 0 };
      wsprintf(lwzLogFileName, L"%s%s_%4d_%02d_%02d_%02d_%02d%s", VH_LOG_DIR, L"OBSAPI", loSystemTime.wYear, loSystemTime.wMonth, loSystemTime.wDay, loSystemTime.wHour, loSystemTime.wMinute, L".log");
      gLogger = new Logger(lwzLogFileName, USER);
   }
   else {
      if (!CreateDirectoryW(logPath, NULL)
         && GetLastError() != ERROR_ALREADY_EXISTS) {
         OutputDebugStringW(L"Logger::Logger: CreateDirectoryW failed.");
      }
      wchar_t lwzLogFileName[255] = { 0 };
      SYSTEMTIME loSystemTime;
      GetLocalTime(&loSystemTime);
      wsprintf(lwzLogFileName, L"%s%s_%4d_%02d_%02d_%02d_%02d%s", logPath, L"OBSAPI", loSystemTime.wYear, loSystemTime.wMonth, loSystemTime.wDay, loSystemTime.wHour, loSystemTime.wMinute, L".log");
      gLogger = new Logger(lwzLogFileName, None);
   }


   if(gLogger)
   {
      gLogger->logInfo("OBSAPI_InterfaceRegister %d %d ",1,2);
   }
   
   API=apiInterface;
}
void OBSAPI_InterfaceUnRegister()
{
   if(gLogger)
   {
      gLogger->logInfo("OBSAPI_InterfaceUnRegister");
      delete gLogger;
      gLogger=NULL;
   }
   
   delete API;
   API=NULL;
   //TerminateXT();
}
void OBSApiLog(const char *format, ...)
{
	//return;
	va_list args;
	va_start(args, format);
   char *buffer;
   int len = _vscprintf(format, args) + 1;  
   buffer = (char *)malloc(len * sizeof(char));
   vsprintf_s(buffer, len, format, args);
   if(gLogger)
   {
      gLogger->logInfo(buffer);  
   }
   free(buffer);
	va_end(args);
}

void OBSAPI_GetThreadHandles (HANDLE *videoThread)
{
   API->GetThreadHandles(videoThread);
}

UINT OBSAPI_GetMonitorsNum()
{
   return API->GetMonitorsNum();
}
UINT OBSAPI_GetFrameTime()
{
   return API->GetFrameTime();
}

XConfig * OBSAPI_GetScenesConfig()
{
   return API->GetScenesConfig();
}

bool OBSAPI_GetIsBRunning()
{
   return API->GetIsBRunning();
}
ImageSource* OBSAPI_GetGlobalSource(CTSTR lpName)
{
   return API->GetGlobalSource(lpName);
}


void OBSApiUIWarning(wchar_t *d){
  if (API) {
    API->UIWarning(d);
  }
}

BOOL CALLBACK DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    if (fdwReason == DLL_PROCESS_ATTACH)
    {
#if defined _M_X64 && _MSC_VER == 1800
        //workaround AVX2 bug in VS2013, http://connect.microsoft.com/VisualStudio/feedback/details/811093
        _set_FMA3_enable(0);
#endif
    }

    return TRUE;
}
