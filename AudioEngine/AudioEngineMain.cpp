// dllmain.cpp : 定义 DLL 应用程序的入口点。

#include "Utility.h"
#include <objbase.h>
#include "IAudioCapture.h"
#include "IAudioSource.h"
#include "AudioCapture.h"
#include "Logging.h"
Logger *gLogger = NULL;

VHALL_API   IAudioCapture* CreateAudioCapture(IDataReceiver* dataReceiver,int iSampleRateHz) {
	return new AudioCapture(dataReceiver, iSampleRateHz);
}
VHALL_API void DestroyAudioCapture(IAudioCapture** audioCapture) {
   if(*audioCapture)
   {
      delete *audioCapture;
      *audioCapture = NULL;
   }
}

BOOL APIENTRY DllMain(HMODULE hModule,
                      DWORD  ul_reason_for_call,
                      LPVOID lpReserved
                      ) {
   switch (ul_reason_for_call) {
   case DLL_PROCESS_ATTACH: {
         // CoInitialize(nullptr);
         CoInitializeEx(NULL, COINIT_MULTITHREADED);
         InitXT(NULL, L"FastAlloc", L"AudioEngine");

         SYSTEMTIME loSystemTime;
         GetLocalTime(&loSystemTime);
         wchar_t lwzLogFileName[255] = { 0 };
         wsprintf(lwzLogFileName, L"%s%s_%4d_%02d_%02d_%02d_%02d%s", VH_LOG_DIR, L"AudioEngine", loSystemTime.wYear, loSystemTime.wMonth, loSystemTime.wDay, loSystemTime.wHour, loSystemTime.wMinute, L".log");
         gLogger = new Logger(lwzLogFileName, USER);
      }
      break;
   case DLL_THREAD_ATTACH:
   case DLL_THREAD_DETACH:
      break;
   case DLL_PROCESS_DETACH:
      if(gLogger){
         delete gLogger;
         gLogger=NULL;
      }
      TerminateXT();
      CoUninitialize();
      break;
   }
   return TRUE;
}

