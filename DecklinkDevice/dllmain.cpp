#include "Utility.h"
#include <comutil.h>
#include "BufferQueue.h"
#include "DeckLinkDevice.h"
#include "Logging.h"


Logger *gLogger = NULL;
DeckLinkDevicesManager* gDeckLinkManager = NULL;;
BOOL APIENTRY DllMain(HMODULE hModule,
                      DWORD  ul_reason_for_call,
                      LPVOID lpReserved
                      ) {
  
   return TRUE;
}


DECKDEVICE_API void *RegistterDeckLinkDeviceEvent(DeckLinkDeviceEventCallBack cb,void *p)
{
   if(gDeckLinkManager)
   {
      return gDeckLinkManager->RegistterDeckLinkDeviceEvent(cb,p);
   }
   return NULL;
}
DECKDEVICE_API void UnRegistterDeckLinkDeviceEvent(void *handle)
{
   if(gDeckLinkManager)
   {
      return gDeckLinkManager->UnRegistterDeckLinkDeviceEvent(handle);
   }
}
DECKDEVICE_API void DeckLinkEventNotify(DeckLinkDeviceEventEnum e,void *p)
{
   if(gDeckLinkManager)
   {
      return gDeckLinkManager->DeckLinkEventNotify(e,p);
   }
}                                      

DECKDEVICE_API void InitDeckLinkDeviceManager(const wchar_t* logPath) {
   //CoInitialize(0);
   OutputDebugString(L"~~~~~~DecklinkDevice DLL_PROCESS_ATTACH\n");
   InitXT(NULL, L"FastAlloc",L"DecklinkDevice");
   if (gLogger == NULL) {
      wchar_t lwzLogFileName[255] = { 0 };
      if (logPath == NULL) {
         SYSTEMTIME loSystemTime;
         GetLocalTime(&loSystemTime);
         wchar_t lwzLogFileName[255] = { 0 };
         wsprintf(lwzLogFileName, L"%s%s_%4d_%02d_%02d_%02d_%02d%s", VH_LOG_DIR, L"DecklinkDevice", loSystemTime.wYear, loSystemTime.wMonth, loSystemTime.wDay, loSystemTime.wHour, loSystemTime.wMinute, L".log");
         gLogger = new Logger(lwzLogFileName, USER);
      }
      else {
         if (!CreateDirectoryW(logPath, NULL)&& GetLastError() != ERROR_ALREADY_EXISTS) {
            OutputDebugStringW(L"Logger::Logger: CreateDirectoryW failed.");
         }
         SYSTEMTIME loSystemTime;
         GetLocalTime(&loSystemTime);
         wsprintf(lwzLogFileName, L"%s%s_%4d_%02d_%02d_%02d_%02d%s", logPath, L"DecklinkDevice", loSystemTime.wYear, loSystemTime.wMonth, loSystemTime.wDay, loSystemTime.wHour, loSystemTime.wMinute, L".log");
         gLogger = new Logger(lwzLogFileName, None);
      }
   }

   if (gDeckLinkManager == NULL){
      gDeckLinkManager = new DeckLinkDevicesManager();
      gDeckLinkManager->Init();
   }
}
DECKDEVICE_API void UnInitDeckLinkDeviceManager() {
   if (gDeckLinkManager)
   {  
      gDeckLinkManager->UnInit();
      delete gDeckLinkManager;
      gDeckLinkManager = NULL;
   }
   
   if(gLogger)
   {
      delete gLogger;
      gLogger=NULL;
   }
   
   TerminateXT();
   
   OutputDebugString(L"~~~~~~DecklinkDevice DLL_PROCESS_DETACH\n");
   //若执行以下函数，Decklink Mini Recoder ,驱动9.6.3 析构崩溃
//   CoUninitialize();
}


DECKDEVICE_API unsigned int  GetDeckLinkDeviceNum() {
   if(!gDeckLinkManager)
   {
      return 0;
   }
   return   gDeckLinkManager->GetDeviceNum();
}
DECKDEVICE_API const wchar_t*  GetDeckLinkDeviceName(const unsigned int &index) {
   if(!gDeckLinkManager)
   {
      return NULL;
   }
   return gDeckLinkManager->GetDeviceName(index);
}

DECKDEVICE_API IDeckLinkDevice*GetDeckLinkDevice(const wchar_t* deviceName,void *key) {
   if(!gDeckLinkManager)
   {
      return NULL;
   }
   return gDeckLinkManager->GetDevice(deviceName,key);
}
DECKDEVICE_API void  ReleaseDeckLinkDevice(IDeckLinkDevice *device,void *key)
{
   if(!gDeckLinkManager)
   {
      return ;
   }
   gDeckLinkManager->ReleaseDevice(device,key);
}
DECKDEVICE_API bool GetDeckLinkDeviceInfo(DeviceInfo devceInfo,UINT &w,UINT &h,int &frameInternal)
{
   if(devceInfo.m_sDeviceType!=TYPE_DECKLINK)
   {
      return false;
   }
   
   IDeckLinkDevice *iDevice=GetDeckLinkDevice(devceInfo.m_sDeviceName,NULL);
   if(!iDevice)
   {
      return false;
   }
   
   DeckLinkDevice *cDevice=dynamic_cast<DeckLinkDevice *>(iDevice);
   if(!cDevice)
   {
      return false;
   }
   return cDevice->GetDeviceInfo(w,h,frameInternal);
}
