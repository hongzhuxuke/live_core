#include "../dshowcapture.hpp"
#include "dshow-graph.hpp"
#include "device.hpp"
#include "log.hpp"
#include <windows.h>
#include <stdio.h>
#include <direct.h> 
#include <io.h>
#include "pathManage.h"
//static wstring	GetAppPath() {
//   WCHAR tmp[1024 * 10];
//   tmp[0] = 0;
//   if (GetModuleFileNameW(NULL, tmp, sizeof(tmp)) > 0) {
//      WCHAR* dst = NULL;
//      WCHAR* p = tmp;
//      while (*p) {
//         if (*p == L'\\')
//            dst = p;
//         p = CharNextW(p);
//      }
//
//      if (dst)
//         *(dst + 1) = 0;
//   }
//   return tmp;
//}

using namespace DShow;
static DShowGraphicManager *G_DShowGraphicManager=NULL;
FILE *G_DebugFile=NULL;
DShowGraphic::DShowGraphic():initialized(false),active(false){
}
DShowGraphic::~DShowGraphic()
{
   DisconnectFilters(NULL);
}
bool DShowGraphic::CreateGraph()
{

   DShowLog(DShowLogType_Level1_Interface,DShowLogLevel_Debug,L"DShowGraphic::CreateGraph\n");
   DisconnectFilters(NULL);
   if(builder)
   {
      DShowLog(DShowLogType_Level1_Interface,DShowLogLevel_Info,L"DShowGraphic::CreateGraph builder.Release()\n");
      builder.Release();
      builder=NULL;
   }
   if(graph)
   {
      DShowLog(DShowLogType_Level1_Interface,DShowLogLevel_Info,L"DShowGraphic::CreateGraph graph.Release()\n");
      graph.Release();
      graph=NULL;
   }
   if(control)
   {
      DShowLog(DShowLogType_Level1_Interface,DShowLogLevel_Info,L"DShowGraphic::CreateGraph control.Release()\n");
      control.Release();
      control=NULL;
   }
   
	if (!CreateFilterGraph(&graph, &builder, &control)) {
      
      DShowLog(DShowLogType_Level1_Interface,DShowLogLevel_Error,L"DShowGraphic::CreateGraph CreateFilterGraph Failed!\n");
		return false;
   }

   DShowLog(DShowLogType_Level1_Interface,DShowLogLevel_Debug,L"DShowGraphic::CreateGraph CreateFilterGraph Successed!\n");
	initialized = true;
	return true;
}
void DShowGraphic::DisconnectFilters(DShow::HDevice *const device)
{
   DShowLog(DShowLogType_Level1_Interface,DShowLogLevel_Debug,L"DShowGraphic::DisconnectFilters\n");
	ComPtr<IEnumFilters>  filterEnum;
	HRESULT               hr;

	if (!graph) {
      DShowLog(DShowLogType_Level1_Interface,DShowLogLevel_Warning,L"DShowGraphic::DisconnectFilters graph is NULL\n");
		return;
   }
   
   DShowLog(DShowLogType_Level1_Interface,DShowLogLevel_Debug,L"DShowGraphic::DisconnectFilters graph->EnumFilters\n");
	hr = graph->EnumFilters(&filterEnum);
	if (FAILED(hr)){      
      DShowLog(DShowLogType_Level1_Interface,DShowLogLevel_Error,L"DShowGraphic::DisconnectFilters graph->EnumFilters Failed! %X\n",hr);
      Error(L"DShowGraphic::DisconnectFilters graph->EnumFilters %X\n",hr);
		return;
   }

   DShowLog(DShowLogType_Level1_Interface,DShowLogLevel_Error,L"DShowGraphic::DisconnectFilters before enum\n");
	ComPtr<IBaseFilter> filter;
	while (filterEnum->Next(1, &filter, nullptr) == S_OK) {
      if(device==NULL)
      {
         graph->RemoveFilter(filter);     
         filterEnum->Reset();
      }
      else
      {
         if(device->HasFilter(filter))
         {
            graph->RemoveFilter(filter);            
            filterEnum->Reset();
         }
      }
	} 
   
   DShowLog(DShowLogType_Level1_Interface,DShowLogLevel_Debug,L"DShowGraphic::DisconnectFilters end\n");
}
bool DShowGraphic::Active()
{
   return m_devices.size()!=0;
}

bool DShowGraphic::EnsureInitialized(DShow::HDevice *const,const wchar_t *tip)
{
	if (!initialized) {
		Error(L"%s: context not initialized", tip);
		return false;
	}
	return true;
}
bool DShowGraphic::EnsureInactive(DShow::HDevice *const,const wchar_t *tip)
{
	if (active) {
		Error(L"%s: cannot be used while active", tip);
		return false;
	}

	return true;
}
bool DShowGraphic::HDeviceRelease(DShow::HDevice *const device)
{
   DisconnectFilters(device);
   
   for(auto itor=m_devices.begin();itor!=m_devices.end();)
   {
      if(*itor==device)
      {
         m_devices.erase(itor++);
      }
      else
      {
         //DisconnectFilters((DShow::HDevice *const)*itor);
         itor++;
      }
   }
   return true;
}
bool DShowGraphic::HDeviceStop()
{
   DShowLog(DShowLogType_Level1_Interface,DShowLogLevel_Debug,L"DShowGraphic::HDeviceStop\n");
	if (active) {
        if (control) {
            control->Stop();
        }
		active = false;      
        DShowLog(DShowLogType_Level1_Interface,DShowLogLevel_Info,L"DShowGraphic::HDeviceStop control->Stop\n");
	}
   else {
        DShowLog(DShowLogType_Level1_Interface,DShowLogLevel_Info,L"DShowGraphic::HDeviceStop nop\n");
   }
   
   DShowLog(DShowLogType_Level1_Interface,DShowLogLevel_Debug,L"DShowGraphic::HDeviceStop return true\n");
   return true;
}


bool DShowGraphic::HDeviceStart()
{
   DShowLog(DShowLogType_Level1_Interface,DShowLogLevel_Debug,L"DShowGraphic::HDeviceStart\n");
   if (active) {      
      DShowLog(DShowLogType_Level1_Interface,DShowLogLevel_Warning,L"DShowGraphic::HDeviceStart active is true return true\n");
      return true;
   }

   LogFilters(graph);

	HRESULT hr=S_OK;
	if (!EnsureInitialized(NULL,L"Start")) {
      DShowLog(DShowLogType_Level1_Interface,DShowLogLevel_Warning,L"DShowGraphic::HDeviceStart EnsureInitialized false,return false\n");
		return false;
   }

   if(!EnsureInactive(NULL,L"Start")) {
      DShowLog(DShowLogType_Level1_Interface,DShowLogLevel_Warning,L"DShowGraphic::HDeviceStart EnsureInactive false,return false\n");
      return false;
   }

   for (auto itor = m_devices.begin(); itor!= m_devices.end();itor++)
   {
      DShow::HDevice *d = (DShow::HDevice *)*itor;
      if (d){
         d->isResetVideo = true;
         d->isResetAudio = true;
      }
   }

   DShowLog(DShowLogType_Level1_Interface,DShowLogLevel_Debug,L"DShowGraphic::HDeviceStart control->Run() before\n");
   if (control != NULL) {
      hr = control->Run();
   } else {
      DShowLog(DShowLogType_Level1_Interface, DShowLogLevel_Error, L"DShowGraphic::HDeviceStart control->Run() Failed! control == NULL\n");
      return false;
   }

	
   DShowLog(DShowLogType_Level1_Interface,DShowLogLevel_Debug,L"DShowGraphic::HDeviceStart control->Run() End %X\n",hr);
	if (FAILED(hr)) {
		if (hr == (HRESULT)0x8007001F) {
         DShowLog(DShowLogType_Level1_Interface,DShowLogLevel_Error,L"DShowGraphic::HDeviceStart control->Run() Failed! device already in use\n");
			WarningHR(L"Run failed, device already in use", hr);
			return false;
		} 
      else
      {   
         DShowLog(DShowLogType_Level1_Interface,DShowLogLevel_Error,L"DShowGraphic::HDeviceStart control->Run() Failed!\n");
			WarningHR(L"Run failed", hr);
			return false;
		}
	}

	active = true;
   DShowLog(DShowLogType_Level1_Interface,DShowLogLevel_Debug,L"DShowGraphic::HDeviceStart return true\n");
   return true;
}
bool DShowGraphic::HDeviceReleaseFilter(DShow::HDevice *const device,IBaseFilter *filter)
{
   if(!device)
   {
      return false;
   }
   
	graph->RemoveFilter(filter);   
   return true;
}
bool DShowGraphic::HDeviceSetupVideoCapture(DShow::HDevice *const device)
{
   if(!device)
   {
      return false;
   }
   
   return device->SetupVideoCapture(device->deviceFilter,device->videoConfig,graph);
}
bool DShowGraphic::HDeviceSetupAudioCapture(DShow::HDevice *const device)
{
   if(!device)
   {
      return false;
   }
   
   return device->SetupAudioCapture(device->deviceFilter,device->audioConfig,graph);
}
bool DShowGraphic::HDeviceAddDeviceFilter(DShow::HDevice *const device,IBaseFilter *filter,const wchar_t *tip)
{
   DShowLog(DShowLogType_Level1_Interface,DShowLogLevel_Debug,L"DShowGraphic::HDeviceAddDeviceFilter\n");
   if(!device)
   {
      DShowLog(DShowLogType_Level1_Interface,DShowLogLevel_Info,L"DShowGraphic::HDeviceAddDeviceFilter inputDevice is NULL\n");
      return false;
   }


	 ComPtr<IEnumFilters> filterEnum;
	 ComPtr<IBaseFilter>  filterTmp;
	 HRESULT hr;
	
    DShowLog(DShowLogType_Level1_Interface,DShowLogLevel_Debug,L"DShowGraphic::HDeviceAddDeviceFilter graph->EnumFilters\n");
	 hr = graph->EnumFilters(&filterEnum);
    if (FAILED(hr)) {
       DShowLog(DShowLogType_Level1_Interface,DShowLogLevel_Error,L"DShowGraphic::HDeviceAddDeviceFilter graph->EnumFilters Failed! %X\n",hr);
       return false;
    }
	
	 while (filterEnum->Next(1, &filterTmp, NULL) == S_OK) {
      if(filterTmp==filter)
      {   
         DShowLog(DShowLogType_Level1_Interface,DShowLogLevel_Info,L"DShowGraphic::HDeviceAddDeviceFilter filterTmp==filter return true\n");
      	return true;
      }
	 }
    
   DShowLog(DShowLogType_Level1_Interface,DShowLogLevel_Debug,L"DShowGraphic::HDeviceAddDeviceFilter AddFilter %s\n",tip);
	graph->AddFilter(filter,tip);

	LogFilters(graph);
	
   return true;
}

bool DShowGraphic::HDeviceConnectPin(DShow::HDevice *const device,const GUID &category,const GUID &type,IBaseFilter *filter,IBaseFilter *capture)
{
   DShowLog(DShowLogType_Level1_Interface,DShowLogLevel_Debug,L"DShowGraphic::HDeviceConnectPin\n");
   if(!device)
   {
      DShowLog(DShowLogType_Level1_Interface,DShowLogLevel_Error,L"DShowGraphic::HDeviceConnectPin device is NULL\n");
      return false;
   }
   
   if(MEDIATYPE_Audio == type) {
      DShowLog(DShowLogType_Level1_Interface,DShowLogLevel_Debug,L"DShowGraphic::HDeviceConnectPin Audio\n");
   }

   bool ret = device->ConnectPins(category,type,filter,capture,builder,graph);   
   DShowLog(DShowLogType_Level1_Interface,DShowLogLevel_Debug,L"DShowGraphic::HDeviceConnectPin %s\n",ret?L"SUCCESS":L"FAILED");
   return ret;
}

bool DShowGraphic::HDeviceRenderFilters(DShow::HDevice *const device,const GUID &category, const GUID &type,IBaseFilter *filter, IBaseFilter *capture)
{
   if(!device)
   {
      return false;
   }

   return device->RenderFilters(category,type,filter,capture,builder);
}
bool DShowGraphic::HasDevice(DShow::HDevice *const device)
{
   for(auto itor=m_devices.begin();itor!=m_devices.end();itor++)
   {
      if(*itor==device)
      {
         return true;
      }
   }
   return false;
}
bool DShowGraphic::HDeviceRestart(DShow::HDevice *const )
{
   if(Active())
   {
      HDeviceStop();
      HDeviceStart();
   }
   return true;
}

//CORE FUNCTION
bool DShowGraphic::ReConfig(DShow::HDevice *const device)
{

   if(device)
   {
      DShowDeviceType type=device->GetFilterType(); 
      if(DShowDeviceType_Audio==type){
         m_devices.push_front(device);
      }
      else{
         m_devices.push_back(device);
      }
      Info(L"DShowGraphic::ReConfig device ConnectFiltersInternal\n");
      //DisconnectFilters(NULL);
      if (!device->ConnectFiltersInternal()) {
         
         Info(L"DShowGraphic::ReConfig device ConnectFiltersInternal failed\n");
         if(DShowDeviceType_Audio==type){
            m_devices.pop_front();
         }
         else{
            m_devices.pop_back();
         }
      }
      else {
         Info(L"DShowGraphic::ReConfig device ConnectFiltersInternal Success\n");
      }

   }

   for (auto itor = m_devices.begin(); itor != m_devices.end(); itor++)
   {
      DShow::HDevice *deviceTmp = (DShow::HDevice *)*itor;
      if (deviceTmp&&device!=deviceTmp) {
         
         Info(L"DShowGraphic::ReConfig sub device ConnectFiltersInternal\n");
         deviceTmp->ConnectFiltersInternal();
      }
   }
   Info(L"DShowGraphic::ReConfig return true\n");

   return true;
}




/////////////////////////////////////////////////////////////////////////////////////////////


DShowGraphicManager::DShowGraphicManager(){
   //m_Graphic=new DShowGraphic();
}
DShowGraphicManager::~DShowGraphicManager()
{
   std::map<wstring, DShowGraphic*>::iterator iter = mDevGraphicMap.begin();
   while (iter != mDevGraphicMap.end()) {
      delete iter->second;
      iter->second = NULL;
      iter++;
   }
}

bool DShowGraphicManager::ReConfig(DShow::HDevice *const device)
{
   if (device == NULL) {
      return false;
   }
   DShowGraphic *pCurGraphic = NULL;
   wstring devId = wstring(device->deviceInfo.m_sDeviceName) + wstring(device->deviceInfo.m_sDeviceID);
   std::map<wstring, DShowGraphic*>::iterator iter = mDevGraphicMap.find(devId);
   if (iter != mDevGraphicMap.end()) {
      pCurGraphic = iter->second;
   }
   else {
      pCurGraphic = new DShowGraphic();
      mDevGraphicMap.insert(std::map<wstring, DShowGraphic*>::value_type(devId, pCurGraphic));
   }
   if(!pCurGraphic){
      return false;
   }
   

   if(pCurGraphic->Active())
   {
      pCurGraphic->HDeviceStop();
   }   
   pCurGraphic->CreateGraph();
   pCurGraphic->ReConfig(device);
   if(pCurGraphic->Active())
   {
      pCurGraphic->HDeviceStart();
   }
   return true;
}

DShowGraphic *DShowGraphicManager::GetDShowGraph(DShow::HDevice *const device)
{
   DShowGraphic *pCurGraphic = NULL;
   if (device) {
      wstring devId = wstring(device->deviceInfo.m_sDeviceName) + wstring(device->deviceInfo.m_sDeviceID);
      std::map<wstring, DShowGraphic*>::iterator iter = mDevGraphicMap.find(devId);
      if (iter != mDevGraphicMap.end()) {
         pCurGraphic = iter->second;
      }
   }
   return pCurGraphic;
}

DShowGraphic *DShowGraphicManager::GetDShowGraphByDevId(wchar_t* devId) {
   DShowGraphic *pCurGraphic = NULL;
   std::map<wstring, DShowGraphic*>::iterator iter = mDevGraphicMap.find(devId);
   if (iter != mDevGraphicMap.end()) {
      pCurGraphic = iter->second;
   }
   return pCurGraphic;
}

bool DShowGraphicManager::HDeviceRelease(DShow::HDevice *const device)
{
   DShowGraphic *graph=GetDShowGraph(device);
   if(!graph)
   {
      return false;
   }
   
   if(graph->Active()){
      graph->HDeviceStop();
   }
   graph->HDeviceRelease(device);
   //if(graph->Active()){
   //   graph->HDeviceStart();
   //}
   wstring devId = wstring(device->deviceInfo.m_sDeviceName) + wstring(device->deviceInfo.m_sDeviceID);
   std::map<wstring, DShowGraphic*>::iterator iter = mDevGraphicMap.find(devId);
   if (iter != mDevGraphicMap.end()) {
      delete iter->second;
      mDevGraphicMap.erase(iter);
   }
   return true;
}


#define DShowGraphManagerExec(X,Y) \
if(!G_DShowGraphicManager)\
{\
   return false;\
}\
return G_DShowGraphicManager->X(Y);


#define DShowGraphicExec(X,Y) \
if(!G_DShowGraphicManager)\
{\
   return false;\
}\
DShowGraphic *graph=G_DShowGraphicManager->GetDShowGraph(device);\
if(!graph)\
{\
   return false;\
}\
return graph->X Y;

#define DShowManagerDefine(X,Y,Z) \
bool DShowGraphic##X Y\
{\
   DShowGraphManagerExec(X,Z)\
}

#define DShowDefine(X,Y,Z) \
bool DShowGraphic##X Y\
{\
   DShowGraphicExec(X,Z);\
}

#define DShowParam(X) (X)

DShowManagerDefine(ReConfig,DShowParam(DShow::HDevice *const device),DShowParam(device));
DShowManagerDefine(HDeviceRelease,(DShow::HDevice *const device),(device));
DShowDefine(EnsureInitialized,(DShow::HDevice *const device,const wchar_t *tip),(device,tip));
DShowDefine(EnsureInactive,(DShow::HDevice *const device,const wchar_t *tip),(device,tip));
DShowDefine(HDeviceReleaseFilter,(DShow::HDevice *const device,IBaseFilter *filter),(device,filter));
DShowDefine(HDeviceSetupVideoCapture,(DShow::HDevice *const device),(device));
DShowDefine(HDeviceSetupAudioCapture,(DShow::HDevice *const device),(device));
DShowDefine(HDeviceAddDeviceFilter,(DShow::HDevice *const device,IBaseFilter *deviceFilter,const wchar_t *tip),(device,deviceFilter,tip));
DShowDefine(HDeviceConnectPin,(DShow::HDevice *const device,const GUID &category,const GUID &type,IBaseFilter *filter,IBaseFilter *capture),(device,category,type,filter,capture));
//DShowDefine(HDeviceSetupAudioCapture,(DShow::HDevice *const device,const GUID &category, const GUID &type,IBaseFilter *filter, IBaseFilter *capture),(device,category,type,filter,capture));
DShowDefine(HDeviceRenderFilters,(DShow::HDevice *const device,const GUID &category, const GUID &type,IBaseFilter *filter, IBaseFilter *capture),(device,category,type,filter,capture));
DShowDefine(HDeviceRestart,(DShow::HDevice *const device),(device));

bool DShowGraphicHDeviceStart(wchar_t* devId)
{
   DShowGraphic *graph=G_DShowGraphicManager->GetDShowGraphByDevId(devId);
   if(!graph)
   {
      return false;
   }
   return graph->HDeviceStart();
}
bool DShowGraphicHDeviceStop(wchar_t* devId)
{
   
   DShowGraphic *graph=G_DShowGraphicManager->GetDShowGraphByDevId(devId);
   if(!graph)
   {
      return false;
   }
   return graph->HDeviceStop();

}

//初始化DSHOW
DSHOWCAPTURE_EXPORT void InitDShowCapture()
{
   G_DShowGraphicManager=new DShowGraphicManager();
}
//反初始化DSHOW
DSHOWCAPTURE_EXPORT void UnInitDShowCapture()
{
   SetLogCallback(NULL,NULL);
   if(G_DShowGraphicManager)
   {
      delete G_DShowGraphicManager;
      G_DShowGraphicManager=NULL;
   }
}

class DShowLogFile{
public:
   DShowLogFile(wchar_t *logName){
      SYSTEMTIME curTime;
      GetLocalTime(&curTime);
      wchar_t timeStamp[128]={0};

      swprintf_s(timeStamp,128, L"%4d_%02d_%02d_%02d_%02d",curTime.wYear, curTime.wMonth, curTime.wDay,curTime.wHour,curTime.wMinute);

      wstring path=GetAppDataPath();
      swprintf_s(logFileName,1024,L"%s\\dshowlog\\%s_%s.log",path.c_str(),logName,timeStamp);
      OutputDebugString(logFileName);
      OutputDebugString(L"\n");
      
      f = _wfsopen(logFileName, L"a+", _SH_DENYNO);
   }
   ~DShowLogFile(){
      if(f) {
         fclose(f);
         f=NULL;
      }
   }
   void Log(DShowLogLevel level,wchar_t *msg_before,wchar_t *msg){
      if(!f) {
         return ;
      }
      //OutputDebugString(msg_before);
      //OutputDebugString(msg);
      fwprintf(f,L"%s%s",msg_before,msg);
   }
private:
   FILE * f = NULL;
   wchar_t logFileName[1024];
};

class DShowLogFileList{
public:
   DShowLogFileList(){

   }
   ~DShowLogFileList(){
      for(auto itor = logMap.begin(); itor != logMap.end();itor++) {
         delete itor->second;
      }
      
      logMap.clear();

      for(auto itor = logMutex.begin(); itor != logMutex.end();itor++) {
         OSCloseMutex(itor->second);
      }
      
      logMutex.clear();
   }
   
   HANDLE OSCreateMutex()
   {
       CRITICAL_SECTION *pSection = (CRITICAL_SECTION*)malloc(sizeof(CRITICAL_SECTION));
       InitializeCriticalSection(pSection);
   
       return (HANDLE)pSection;
   }
   void   OSCloseMutex(HANDLE hMutex)
   {
       DeleteCriticalSection((CRITICAL_SECTION*)hMutex);
       free(hMutex);
   }

   void OSEnterMutex(HANDLE hMutex)
   {
       EnterCriticalSection((CRITICAL_SECTION*)hMutex);
   }
   void OSLeaveMutex(HANDLE hMutex)
   {
       LeaveCriticalSection((CRITICAL_SECTION*)hMutex);
   }
   
   void AddLog(int type,wchar_t *filename){
      DShowLogFile *logFile = new DShowLogFile(filename);
      logMap[type]=logFile;
      logMutex[type]=OSCreateMutex();    
   }
   void Log(int type,DShowLogLevel level,wchar_t *msg){

         HANDLE mutex = logMutex[type];
         if(!mutex) {
            return ;
         }
         
         DShowLogFile *logFile = logMap[type];
         if(!logFile) {
            return ;
         }
         
         wchar_t msg_before[1024]={0};


         SYSTEMTIME curTime;
         GetLocalTime(&curTime);
         wchar_t timeStamp[128]={0};
         swprintf_s(timeStamp,128, L"[%4d-%02d-%02d %02d:%02d:%02d]",
                   curTime.wYear, curTime.wMonth, curTime.wDay, curTime.wHour,
                   curTime.wMinute, curTime.wSecond);

         wchar_t *levelStr[]={
            L"Debug",
            L"Info",
            L"Warning",
            L"Error"
         };

         unsigned int tid = GetCurrentThreadId();
         swprintf_s(msg_before,1024,L"[%s][TID:%u][%s]\t",timeStamp,tid,levelStr[level]);

         OSEnterMutex(mutex);
         logFile->Log(level,msg_before,msg);
         OSLeaveMutex(mutex);         
   }
private:
   std::map<int,DShowLogFile *> logMap;

   std::map<int,HANDLE> logMutex;
};
static DShowLogFileList *g_LogFileList = NULL;
DSHOWCAPTURE_EXPORT void DShowLogInit() {
   OutputDebugString(L"DShowLogInit\n");


   wstring path=GetAppDataPath();
   OutputDebugString(path.c_str());
   OutputDebugString(L"\n");
   wstring logDir=path+L"dshowlog";
   if(_waccess(logDir.c_str(),0)!=0){
      _wmkdir(logDir.c_str());      
      OutputDebugString(L"dshowlog not exist\n");
   }
   else
   {
      OutputDebugString(L"dshowlog is exist\n");
   }
   
   g_LogFileList = new DShowLogFileList();
   g_LogFileList->AddLog((int)DShowLogType_Level1_Interface,L"Interface");
   g_LogFileList->AddLog((int)DShowLogType_Level1_Receive,L"Receive");
   g_LogFileList->AddLog((int)DShowLogType_Level1_AudioPretreatment,L"AudioPretreatment");
   g_LogFileList->AddLog((int)DShowLogType_Level1_AudioMixAndEncode,L"AudioMixAndEncode");
   g_LogFileList->AddLog((int)DShowLogType_Level1_USBHot,L"USBHot");
   g_LogFileList->AddLog((int)DShowLogType_Level2_ALL,L"ALL");
   g_LogFileList->AddLog((int)DShowLogType_Level3_AudioSource,L"AudioSource");
   g_LogFileList->AddLog((int)DShowLogType_Level3_DShowDevice,L"DShowDevice");
   g_LogFileList->AddLog((int)DShowLogType_Level3_DShowGraphic,L"DShowGraphic"); 
}
DSHOWCAPTURE_EXPORT void DShowLogUnInit() {
   
   OutputDebugString(L"DShowLogUnInit\n");
   delete g_LogFileList;
   g_LogFileList = NULL;
}

DSHOWCAPTURE_EXPORT void DShowLog(DShowLogType type,DShowLogLevel level,wchar_t *fmt,...) {
   wchar_t msg[1024]={0};
   va_list args;
   va_start(args, fmt);
   vswprintf_s(msg,1024,fmt,args);
   va_end(args);

   if(g_LogFileList) {
      g_LogFileList->Log(type,level,msg);
   }
}

