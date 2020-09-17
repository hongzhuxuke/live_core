
#ifndef __DECK_LINK_DEVICE_INCLUDE_H__
#define __DECK_LINK_DEVICE_INCLUDE_H__
#include <vector>
#include <map>
#include <list>
#include <string>
#include "DeckLinkAPI_h.h"   
#include "IDeckLinkDevice.h"


class BufferQueue;
class DeckLinkMemoryAllocator;
class DeckLinkDevice : public IDeckLinkInputCallback, public IDeckLinkDevice {
private:
   bool Init();
   bool SupportsFormatDetection();
   long GetFrameSize(BMDPixelFormat pft, long width, long height);
public:
   IDeckLink*DeckLinkInstance();

   DeckLinkDevice(IDeckLink*);
   virtual ~DeckLinkDevice();
   void Destory();
   const std::wstring& GetDeviceName();
   //-------------------------------------------------
   //implement IDeckLinkDevice
   
   virtual void AudioReInit();
   virtual void EnableAudio(const bool& enableAudio);
   virtual void EnableVideo(const bool& enableVideo, IVideoChangedNotify* videoNotify);
   virtual bool GetNextAudioBuffer(void **buffer, unsigned int *numFrames, unsigned long long *timestamp);

   virtual bool GetNextVideoBuffer(void **buffer, unsigned long long *bufferSize, unsigned long long *timestamp);

   virtual bool StartCapture();
   virtual void StopCapture();
   virtual bool IsCapturing();
   virtual bool GetAudioParam(unsigned int& channels, unsigned int& samplesPerSec, unsigned int& bitsPerSample);
   virtual bool GetVideoParam(unsigned long& videoWidth, unsigned long& videoHeightt, long long & frameDuration, long long &timeScale); // default pixfmt
   virtual const wchar_t* GetDeckLinkDeviceName() const;
   virtual void AudioSync();
   bool GetDeviceInfo(UINT &w,UINT &h,int &frameInternal)
   {
      w=mDisplayWidth;
      h=mDisplayHeight;
      frameInternal=mDisplayFrameInternal;
      return true;
   }



   //-------------------------------------------------
   //implement IDeckLinkInputCallback
   // IUnknown interface
   virtual HRESULT	STDMETHODCALLTYPE	QueryInterface(REFIID iid, LPVOID *ppv);
   virtual ULONG	STDMETHODCALLTYPE	AddRef();
   virtual ULONG	STDMETHODCALLTYPE	Release();

   // IDeckLinkInputCallback interface
   virtual HRESULT STDMETHODCALLTYPE	VideoInputFormatChanged(/* in */ BMDVideoInputFormatChangedEvents notificationEvents,
                                                               /* in */ IDeckLinkDisplayMode *newDisplayMode,
                                                               /* in */ BMDDetectedVideoInputFormatFlags detectedSignalFlags);
   virtual HRESULT STDMETHODCALLTYPE	VideoInputFrameArrived(/* in */ IDeckLinkVideoInputFrame* videoFrame,
                                                              /* in */ IDeckLinkAudioInputPacket* audioPacket);
private:
   ULONG	                              mRefCount;
   std::wstring	                     mDeviceName;
   IDeckLink*                          mDeckLink = nullptr;
   IDeckLinkInput*						   mDeckLinkInput = nullptr;

   BOOL								mSupportsFormatDetection;
   bool								mCurrentlyCapturing;
   bool								mApplyDetectedInputMode;
   bool                       mEnableAudio;
   bool                       mEnableVideo;
   bool                       mIsInit;
   bool                       mHasAudio;
   bool                       mHasVideo;
   bool                       mResetAudioStatus;
   ULONG64                    mNullVideoFrameCnt;
   IVideoChangedNotify*       mVideoInputChangedNotify = nullptr;

   unsigned int               mChannels;
   BMDAudioSampleRate         mSampleRate;
   BMDAudioSampleType         mSampleType; //bits per sample    
   BMDPixelFormat             mOutputPft;

   int                        mDisplayWidth;
   int                        mDisplayHeight;
   long long                  mDisplayFrameInternal;
   BMDDisplayMode             mDisplayMode;
   BMDTimeScale               mTimeScale;




   //audio buffer manager   
   //List<BYTE> mSampleBuffer;
   BufferQueue* mAudioBufferQueue = nullptr;
   BufferQueue* mLastAudioBufferQueue = nullptr;
   DataUnit*    mAudioOutputDataUnit = nullptr;
   List<DataUnit*>    mLastAudioOutputDataUnitList;


   HANDLE       mFormatChangedMutex;

   

   
   //current output timestamp, audio time or video time 
   unsigned long long mCurrentOutputTime;
   bool           mSyncAudio;
   List<char>                 mAudioBuffer;
   //video buffer 
   BufferQueue* mVideoBufferQueue = nullptr;
   //插拔设备导致内存泄露
   DataUnit*    mVideoOutputDataUnit = nullptr;
   DataUnit*    mVideoOutputDataUnitLast = nullptr;

   //keep last a/v buffer for device mode change
   //BufferQueue* mLastAudioBufferQueue;
   BufferQueue* mLastVideoBufferQueue = nullptr;
   v_mutex_t  mDeviceMutex;
   QWORD       mStartTimeInMs;
   QWORD       mStartTimeAudioMs;
   QWORD       mLastGetVideoTimeMs;
   List<char>                 mLastBytes;
   long long                  mLastAudioPts;
   DeckLinkMemoryAllocator* mMemoryAllocator = nullptr;
};

class DeckLinkDeviceDiscovery;
class  DeckLinkDevicesManager {
public:
   DeckLinkDevicesManager();
   ~DeckLinkDevicesManager();
public:
   bool Init();
   void UnInit();
   void AddDevice(IDeckLink* deckLink);
   void RemoveDevice(IDeckLink* deckLink);
   int GetDeviceNum();
   const wchar_t* GetDeviceName(const unsigned int &);
   IDeckLinkDevice* GetDevice(const wchar_t*,void *);
   void ReleaseDevice(IDeckLinkDevice*,void *);
   void *RegistterDeckLinkDeviceEvent(DeckLinkDeviceEventCallBack,void *param);
   void UnRegistterDeckLinkDeviceEvent(void *);
   void DeckLinkEventNotify(DeckLinkDeviceEventEnum ,void *);
private:
   std::map<void *,IDeckLinkDevice *> mUseList;
   std::map< std::wstring, DeckLinkDevice*>  mDevicesList;
   std::list<DeckLinkDeviceCallBackStruct *> mCallBackList;
   DeckLinkDeviceDiscovery* mDeviceDiscovery;
   HANDLE mManagerMutex;
   
   DeckLinkDevice *mLastDecklinkDevice = nullptr;
   bool mIsInit;

};

class DeckLinkDeviceDiscovery : public IDeckLinkDeviceNotificationCallback {
private:
   IDeckLinkDiscovery*					m_deckLinkDiscovery = nullptr;
   DeckLinkDevicesManager*			   mDevicesManager = nullptr;
   ULONG								      m_refCount;
   bool                             mInitialized;
public:
   DeckLinkDeviceDiscovery(DeckLinkDevicesManager* devicesManager);
   virtual ~DeckLinkDeviceDiscovery();

   bool						        Enable();
   void						        Disable();

   // IDeckLinkDeviceNotificationCallback interface
   virtual HRESULT	STDMETHODCALLTYPE	DeckLinkDeviceArrived(/* in */ IDeckLink* deckLink);
   virtual HRESULT	STDMETHODCALLTYPE	DeckLinkDeviceRemoved(/* in */ IDeckLink* deckLink);

   // IUnknown needs only a dummy implementation
   virtual HRESULT STDMETHODCALLTYPE	QueryInterface(REFIID iid, LPVOID *ppv);
   virtual ULONG	STDMETHODCALLTYPE	AddRef();
   virtual ULONG	STDMETHODCALLTYPE	Release();
};

class DeckLinkMemoryAllocator :public IDeckLinkMemoryAllocator {
public:
   DeckLinkMemoryAllocator();
   ~DeckLinkMemoryAllocator();
   virtual HRESULT STDMETHODCALLTYPE AllocateBuffer(
      /* [in] */ unsigned int bufferSize,
      /* [out] */ void **allocatedBuffer);
   virtual HRESULT STDMETHODCALLTYPE ReleaseBuffer(
      /* [in] */ void *buffer);
   virtual HRESULT STDMETHODCALLTYPE Commit(void);
   virtual HRESULT STDMETHODCALLTYPE Decommit(void);
   virtual HRESULT STDMETHODCALLTYPE	QueryInterface(REFIID iid, LPVOID *ppv);
   virtual ULONG	STDMETHODCALLTYPE	AddRef();
   virtual ULONG	STDMETHODCALLTYPE	Release();
private:
   ULONG m_refCount;
   std::map<unsigned int, void*>mBufferList;
   std::map<void*, ULONG>mBufferRef;
};

//
/*
*1. init decklink device discovery
*2. add local device to global device list. !note: the destop video is install , the device name will in list
*3. if device selected, will add this device object to start capture
*4.
*/

#endif 


