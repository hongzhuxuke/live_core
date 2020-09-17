#ifndef _DSHOW_GRAPH_H_
#define _DSHOW_GRAPH_H_
#include "VH_ConstDeff.h"
#include "../dshowcapture.hpp"
#include "capture-filter.hpp"
//???Ψ???graph
typedef void * HDeviceHandle;
class DShowGraphic
{
public:
   DShowGraphic();
   ~DShowGraphic();
   bool Active();
   bool ReConfig(DShow::HDevice *const device);
   bool EnsureInitialized(DShow::HDevice *const device,const wchar_t *tip);
   bool EnsureInactive(DShow::HDevice *const device,const wchar_t *tip);
   bool HDeviceRelease(DShow::HDevice *const device);
   bool HDeviceStop();
   bool HDeviceStart();
   bool HDeviceReleaseFilter(DShow::HDevice *const device,IBaseFilter *);
   bool HDeviceSetupVideoCapture(DShow::HDevice *const device);
   bool HDeviceSetupAudioCapture(DShow::HDevice *const device);
   bool HDeviceAddDeviceFilter(DShow::HDevice *const device,IBaseFilter *deviceFilter,const wchar_t *tip);
   //bool HDeviceSetupAudioOutput(DShow::HDevice *const device);
   bool HDeviceConnectPin(DShow::HDevice *const device,const GUID &category,const GUID &type,IBaseFilter *filter,IBaseFilter *capture);
   //bool HDeviceSetupAudioCapture(DShow::HDevice *const device,const GUID &category, const GUID &type,IBaseFilter *filter, IBaseFilter *capture);
   bool HDeviceRenderFilters(DShow::HDevice *const device,const GUID &category, const GUID &type,IBaseFilter *filter, IBaseFilter *capture);
   bool HDeviceRestart(DShow::HDevice *const device);

   bool HasDevice(DShow::HDevice *const device);

//private:
   bool CreateGraph();
   void DisconnectFilters(DShow::HDevice *const device);
private:
	ComPtr<IGraphBuilder>            graph = nullptr;
	ComPtr<ICaptureGraphBuilder2>    builder = nullptr;
	ComPtr<IMediaControl>            control = nullptr;
   std::list<HDeviceHandle>         m_devices;   

 	bool                             initialized = false;
	bool                             active = false;
};

class DShowGraphicManager
{
public:
   DShowGraphicManager();
   ~DShowGraphicManager();
   bool ReConfig(DShow::HDevice *const);
   DShowGraphic *GetDShowGraph(DShow::HDevice *const);   
   DShowGraphic *GetDShowGraphByDevId(wchar_t* devId);
   bool HDeviceRelease(DShow::HDevice *const);
private:
   std::map<wstring, DShowGraphic*> mDevGraphicMap;
};

bool DShowGraphicReConfig(DShow::HDevice *const device);
bool DShowGraphicHDeviceAddDeviceFilter(DShow::HDevice *const device,IBaseFilter *deviceFilter,const wchar_t *deviceFilterTip);
bool DShowGraphicEnsureInitialized(DShow::HDevice *const device,const wchar_t *);
bool DShowGraphicEnsureInactive(DShow::HDevice *const device,const wchar_t *);
bool DShowGraphicHDeviceRelease(DShow::HDevice *const);
bool DShowGraphicHDeviceReleaseFilter(DShow::HDevice *const,IBaseFilter *);
bool DShowGraphicHDeviceSetupVideoCapture(DShow::HDevice *const);
bool DShowGraphicHDeviceSetupAudioCapture(DShow::HDevice *const);
bool DShowGraphicHDeviceConnectPin(DShow::HDevice *const,const GUID &category,const GUID &type,IBaseFilter *filter,IBaseFilter *capture);
bool DShowGraphicHDeviceRenderFilters(DShow::HDevice *const,const GUID &category, const GUID &type,IBaseFilter *filter, IBaseFilter *capture);
bool DShowGraphicHDeviceRestart(DShow::HDevice *const);
bool DShowGraphicHDeviceStart(wchar_t* devId);
bool DShowGraphicHDeviceStop(wchar_t* devId);

#endif
