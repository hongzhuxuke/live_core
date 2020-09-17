/*
 *  Copyright (C) 2014 Hugh Bailey <obs.jim@gmail.com>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 */

#include "device.hpp"
#include "dshow-device-defs.hpp"
#include "dshow-media-type.hpp"
#include "dshow-formats.hpp"
#include "dshow-enum.hpp"
#include "log.hpp"

#define ROCKET_WAIT_TIME_MS 5000


HANDLE OSCreateMutex()
{
    CRITICAL_SECTION *pSection = (CRITICAL_SECTION*)malloc(sizeof(CRITICAL_SECTION));
    InitializeCriticalSection(pSection);

    return (HANDLE)pSection;
}

void  OSEnterMutex(HANDLE hMutex)
{
    //assert(hMutex);
    EnterCriticalSection((CRITICAL_SECTION*)hMutex);
}

BOOL OSTryEnterMutex(HANDLE hMutex)
{
    //assert(hMutex);
    return TryEnterCriticalSection((CRITICAL_SECTION*)hMutex);
}

void OSLeaveMutex(HANDLE hMutex)
{
    //assert(hMutex);
    LeaveCriticalSection((CRITICAL_SECTION*)hMutex);
}

void OSCloseMutex(HANDLE hMutex)
{
    //assert(hMutex);
    DeleteCriticalSection((CRITICAL_SECTION*)hMutex);
    free(hMutex);
}


namespace DShow {

bool SetRocketEnabled(IBaseFilter *encoder, bool enable);

HDevice::HDevice()
{
   memset(&deviceInfo,0,sizeof(DeviceInfo));
   deviceInfo.m_sDeviceType=TYPE_DSHOW_VIDEO;
   isResetVideo=true;
   isResetAudio=true;
   mConfigMutex = OSCreateMutex();
}

HDevice::~HDevice()
{
   bool DShowGraphicHDeviceStop(wchar_t* devId);
   DShowGraphicHDeviceRelease(this);

   OSEnterMutex(mConfigMutex);
   videoConfig.callback = nullptr;
   audioConfig.callback = nullptr;
   OSLeaveMutex(mConfigMutex);

	/*
	 * the sleeps for the rocket are required.  It seems that you cannot
	 * simply start/stop the stream right away after/before you enable or
	 * disable the rocket.  If you start it too fast after enabling, it
	 * won't return any data.  If you try to turn off the rocket too
	 * quickly after stopping, then it'll be perpetually stuck on, and then
	 * you'll have to unplug/replug the device to get it working again.
	 */
	if (!!rocketEncoder) {
		Sleep(ROCKET_WAIT_TIME_MS);
		SetRocketEnabled(rocketEncoder, false);
	}
   if(videoCapture){
      DShowGraphicHDeviceReleaseFilter(this,videoCapture);
      videoCapture.Release();
      videoCapture=NULL;
   }
   if(deviceFilter)
   {
      deviceFilter.Release();
      deviceFilter=NULL;
   }

   if(audioCapture){
      audioCapture.Release();
      audioCapture=NULL;
   }
   if(rocketEncoder){
      rocketEncoder.Release();
      rocketEncoder=NULL;
   }  
   DShowGraphicReConfig(NULL);
   OSCloseMutex(mConfigMutex);
}

bool HDevice::EnsureInitialized(const wchar_t *func)
{
   return DShowGraphicEnsureInitialized(this,func);
}

bool HDevice::Valid()
{
   return EnsureInitialized(L"HDevice::Valid");
}


bool HDevice::EnsureInactive(const wchar_t *func)
{
   return DShowGraphicEnsureInactive(this,func);
}
bool HDevice::HasFilter(IBaseFilter *filter)
{
   if(!filter)
   {
      return false;
   }

   if(filter==deviceFilter)
   {
      return true;
   }
   
   if(filter==videoCapture)
   {
      return true;
   }

   if(filter==audioCapture)
   {
      return true;
   }

   if(filter==rocketEncoder)
   {
      return true;
   }
   return false;
}


#define AUDIO_DEBUG(X,Y) {static int logIndex = 0;if(logIndex %X == 0) {logIndex = 0;Y;}logIndex++;}


inline void HDevice::SendToCallback(bool video,
		unsigned char *data, size_t size,
		long long startTime, long long stopTime)
{
#define RECVLOGSendToCallback(X) \
      DSHOWLogAudioExec((video?MEDIATYPE_Video:MEDIATYPE_Audio),\
         X);

    AUDIO_DEBUG(400, RECVLOGSendToCallback(DShowLog(DShowLogType_Level1_Receive, DShowLogLevel_Debug, L"HDevice::SendToCallback\n")));


    if (!size) {
        RECVLOGSendToCallback(DShowLog(DShowLogType_Level1_Receive, DShowLogLevel_Error, L"HDevice::SendToCallback size 0\n"));
	    return;
    }
   
    if (video){
        OSEnterMutex(mConfigMutex);
        videoConfig.isReset=isResetVideo;
        if(isResetVideo)
        {
            isResetVideo=false;
        }
        if (videoConfig.callback) {
            videoConfig.callback(videoConfig, data, size, startTime, stopTime);
        }
        OSLeaveMutex(mConfigMutex);
    }
    else{
        OSEnterMutex(mConfigMutex);
        audioConfig.isReset=isResetAudio;
        if(isResetAudio)
        {
            isResetAudio=false;
            RECVLOGSendToCallback(DShowLog(DShowLogType_Level1_Receive, DShowLogLevel_Warning, L"HDevice::SendToCallback Reset\n"));
        }
        if (audioConfig.callback != nullptr) {
            audioConfig.callback(audioConfig, data, size, startTime, stopTime);
        }
        OSLeaveMutex(mConfigMutex);
    }
}

void HDevice::Receive(bool isVideo, IMediaSample *sample)
{
#define RECVLOGReceive(X) \
   DSHOWLogAudioExec((isVideo?MEDIATYPE_Video:MEDIATYPE_Audio),\
      X);

    AUDIO_DEBUG(400, RECVLOGReceive(DShowLog(DShowLogType_Level1_Receive, DShowLogLevel_Debug, L"HDevice::Receive\n")));

    HRESULT hr = 0;
    BYTE *ptr;
    MediaTypePtr mt = NULL;
    OSEnterMutex(mConfigMutex);
    bool encoded = isVideo ? ((int)videoConfig.format >= 400) : ((int)audioConfig.format >= 200);
    if (!sample) {
        RECVLOGReceive(DShowLog(DShowLogType_Level1_Receive, DShowLogLevel_Error, L"HDevice::Receive sample is NULL return \n"));
        if(!isVideo) {         
            //Warning(L"HDevice::Receive sample is NULL");
        }
        OSLeaveMutex(mConfigMutex);
	    return;
    }

    if (isVideo ? !videoConfig.callback : !audioConfig.callback) {
        if(!isVideo) {
            //Warning(L"HDevice::Receive audioConfig.callback NULL");
        }
        RECVLOGReceive(DShowLog(DShowLogType_Level1_Receive, DShowLogLevel_Error, L"HDevice::Receive Config.callback is NULL \n"));
        OSLeaveMutex(mConfigMutex);
	    return;
    }
    OSLeaveMutex(mConfigMutex);
    if (sample->GetMediaType(&mt) == S_OK) {
	    if (isVideo) {
		    videoMediaType = mt;
		    ConvertVideoSettings();
	    } else {
		    audioMediaType = mt;
		    ConvertAudioSettings();
	    }
      
        //AUDIO_DEBUG(3,RECVLOG(DShowLog(DShowLogType_Level1_Receive,DShowLogLevel_Info,L"HDevice::Receive GetMediaType Successed!\n")));   
    }
    else {
        //AUDIO_DEBUG(3,RECVLOG(DShowLog(DShowLogType_Level1_Receive,DShowLogLevel_Error,L"HDevice::Receive GetMediaType Failed! %X\n",hr)));   
    }

    int size = sample->GetActualDataLength();
    if (!size){
        if(!isVideo){
            Warning(L"HDevice::Receive [AUDIO] GetActualDataLength 0");         
            RECVLOGReceive(DShowLog(DShowLogType_Level1_Receive, DShowLogLevel_Error, L"HDevice::Receive sample->GetActualDataLength is 0 \n"));
        }
	    return;
    }
   
    hr = sample->GetPointer(&ptr);
    DWORD error = GetLastError();
    if (FAILED(hr)){
        if(!isVideo){
            WarningHR(L"HDevice::Receive [AUDIO] GetPointer FAILED ",hr);
            RECVLOGReceive(DShowLog(DShowLogType_Level1_Receive, DShowLogLevel_Error, L"HDevice::Receive sample->GetPointer Failed! %X\n", hr));
        }
	    return;
    }
   
    long long startTime, stopTime;
    hr = sample->GetTime(&startTime, &stopTime);
    bool hasTime = SUCCEEDED(hr);
    if(!hasTime) {
        if(!isVideo){
            WarningHR(L"HDevice::Receive [AUDIO] sample->GetTime FAILED ",hr);  
        }
      
        RECVLOGReceive(DShowLog(DShowLogType_Level1_Receive, DShowLogLevel_Error, L"HDevice::Receive sample->GetTime Failed! %X\n", hr));
    }
    else {
        AUDIO_DEBUG(400, RECVLOGReceive(DShowLog(DShowLogType_Level1_Receive, DShowLogLevel_Debug, L"HDevice::Receive sample->GetTime Successed! %lld %lld\n", startTime, stopTime)));
    }
   
    if (encoded) {
	    EncodedData &data = isVideo ? encodedVideo : encodedAudio;

	    /* packets that have time are the first packet in a group of
		    * segments */
	    if (hasTime) {
		    SendToCallback(isVideo,
				    data.bytes.data(), data.bytes.size(),
				    data.lastStartTime, data.lastStopTime);

		    data.bytes.resize(0);
		    data.lastStartTime = startTime;
		    data.lastStopTime  = stopTime;
	    }

	    data.bytes.insert(data.bytes.end(),
			    (unsigned char*)ptr,
			    (unsigned char*)ptr + size);

    }
    else {
       if (hasTime) {
          SendToCallback(isVideo, ptr, size, startTime, stopTime);
       }
       else if (size > 0 && ptr && isVideo) {
          SendToCallback(isVideo, ptr, size, 0, 0);
       }
    }
}

void HDevice::ConvertVideoSettings()
{
	VIDEOINFOHEADER  *vih  = (VIDEOINFOHEADER*)videoMediaType->pbFormat;
	BITMAPINFOHEADER *bmih = GetBitmapInfoHeader(videoMediaType);

    if (bmih) {
	    Debug(L"Video media type changed");
        OSEnterMutex(mConfigMutex);
	    videoConfig.cx            = bmih->biWidth;
	    videoConfig.cy            = bmih->biHeight;
	    videoConfig.frameInterval = vih->AvgTimePerFrame;

	    bool same = videoConfig.internalFormat == videoConfig.format;
	    GetMediaTypeVFormat(videoMediaType, videoConfig.internalFormat);
        if (same)
		    videoConfig.format = videoConfig.internalFormat;
        OSLeaveMutex(mConfigMutex);
    }
}

void HDevice::ConvertAudioSettings()
{
	WAVEFORMATEX *wfex =
		reinterpret_cast<WAVEFORMATEX*>(audioMediaType->pbFormat);

	Debug(L"Audio media type changed");
    OSEnterMutex(mConfigMutex);
	audioConfig.sampleRate = wfex->nSamplesPerSec;
	audioConfig.channels   = wfex->nChannels;

	if (wfex->wFormatTag == WAVE_FORMAT_RAW_AAC1)
		audioConfig.format = AudioFormat::AAC;
	else if (wfex->wFormatTag == WAVE_FORMAT_DVM)
		audioConfig.format = AudioFormat::AC3;
	else if (wfex->wFormatTag == WAVE_FORMAT_MPEG)
		audioConfig.format = AudioFormat::MPGA;
	else if (wfex->wBitsPerSample == 16)
		audioConfig.format = AudioFormat::Wave16bit;
	else if (wfex->wBitsPerSample == 32)
		audioConfig.format = AudioFormat::WaveFloat;
	else
		audioConfig.format = AudioFormat::Unknown;
    OSLeaveMutex(mConfigMutex);
}

#define HD_PVR1_NAME L"Hauppauge HD PVR Capture"

bool HDevice::SetupExceptionVideoCapture(IBaseFilter *filter,
		VideoConfig &config,
		IGraphBuilder *graph)
{
	ComPtr<IPin> pin;

	if (GetPinByName(filter, PINDIR_OUTPUT, L"656", &pin))
		return SetupEncodedVideoCapture(filter, config, HD_PVR2,graph);

	else if (GetPinByName(filter, PINDIR_OUTPUT, L"TS Out", &pin))
		return SetupEncodedVideoCapture(filter, config, Roxio,graph);

	return false;
}

static bool GetPinMediaType(IPin *pin, MediaType &mt)
{
	ComPtr<IEnumMediaTypes> mediaTypes;

	if (SUCCEEDED(pin->EnumMediaTypes(&mediaTypes))) {
		MediaTypePtr curMT;
		ULONG        count = 0;

		while (mediaTypes->Next(1, &curMT, &count) == S_OK) {
			if (curMT->formattype == FORMAT_VideoInfo) {
				mt = curMT;
				return true;
			}
		}
	}

	return false;
}
//设置视频捕获
bool HDevice::SetupVideoCapture(IBaseFilter *filter,
   VideoConfig &config,
   IGraphBuilder *graph)
{
	ComPtr<IPin>  pin;
	HRESULT       hr;
	bool          success;
   Info(L"SetupVideoCapture AddFilter");

	if(filterType==DShowDeviceType_Audio)
	{
		return false;
	}

   
   //对于同一个filter，使用两次将会重复添加
   //graph->AddFilter(deviceFilter, L"Device Filter");
   DShowGraphicHDeviceAddDeviceFilter(this,deviceFilter,L"Video Filter");

	if (config.name.find(L"C875") != std::string::npos)
		return SetupEncodedVideoCapture(filter, config, AV_LGP,graph);

	else if (config.name.find(L"IT9910") != std::string::npos)
		return SetupEncodedVideoCapture(filter, config, HD_PVR_Rocket,graph);

	else if (config.name.find(HD_PVR1_NAME) != std::string::npos)
		return SetupEncodedVideoCapture(filter, config, HD_PVR1,graph);

	success = GetFilterPin(filter, MEDIATYPE_Video, PIN_CATEGORY_CAPTURE,
			PINDIR_OUTPUT, &pin);
	if (!success) {
		if (SetupExceptionVideoCapture(filter, config,graph)) {
			return true;
		} else {
			Error(L"Could not get video pin");
			return false;
		}
	}

	ComQIPtr<IAMStreamConfig> pinConfig(pin);
	if (pinConfig == NULL) {
		Error(L"Could not get IAMStreamConfig for device");
		return false;
	}

	if (config.useDefaultConfig) {
		MediaTypePtr defaultMT;

		hr = pinConfig->GetFormat(&defaultMT);
      GetPinMediaType(pin, videoMediaType);
		if (hr == E_NOTIMPL) {
			if (!GetPinMediaType(pin, videoMediaType))
         {
				Error(L"Couldn't get pin media type");
				return false;
			}

		} else if (FAILED(hr)) {
			ErrorHR(L"Could not get default format for video", hr);
			return false;

		} else {
			videoMediaType = defaultMT;
		}

		ConvertVideoSettings();

		config.format = config.internalFormat = VideoFormat::Any;
	}

	if (!GetClosestVideoMediaType(filter, config, videoMediaType)) {
		Error(L"Could not get closest video media type");
		return false;
	}

	hr = pinConfig->SetFormat(videoMediaType);
	if (FAILED(hr) && hr != E_NOTIMPL) {
		ErrorHR(L"Could not set video format", hr);
		return false;
	}

	ConvertVideoSettings();

	PinCaptureInfo info;
	info.callback          = [this] (IMediaSample *s) {Receive(true, s);};
	info.expectedMajorType = videoMediaType->majortype;
    OSEnterMutex(mConfigMutex);
	/* attempt to force intermediary filters for these types */
	if (videoConfig.format == VideoFormat::XRGB)
		info.expectedSubType = MEDIASUBTYPE_RGB32;
	else if (videoConfig.format == VideoFormat::ARGB)
		info.expectedSubType = MEDIASUBTYPE_ARGB32;
	else if (videoConfig.format == VideoFormat::YVYU)
		info.expectedSubType = MEDIASUBTYPE_YVYU;
	else if (videoConfig.format == VideoFormat::YUY2)
		info.expectedSubType = MEDIASUBTYPE_YUY2;
	else if (videoConfig.format == VideoFormat::UYVY)
		info.expectedSubType = MEDIASUBTYPE_UYVY;
	else
		info.expectedSubType = videoMediaType->subtype;
    OSLeaveMutex(mConfigMutex);
	videoCapture = new CaptureFilter(info);
	DShowGraphicHDeviceAddDeviceFilter(this,videoCapture,L"Video Capture Filter");

	//graph->AddFilter(videoCapture, L"Video Capture Filter");

	return true;
}
//设置视频配置
bool HDevice::SetVideoConfig(VideoConfig *config)
{

    //DShowGraphicHDeviceStop(this);
    //DShowGraphicHDeviceReleaseFilter(this,videoCapture);
    //videoCapture.Release();
    Info(L"HDevice::SetVideoConfig\n");
    if (!config)
    {
	    Info(L"HDevice::SetVideoConfig config is NULL retrun \n");
	    return true;
    }
		
   
    Info(L"HDevice::SetVideoConfig InitDeviceFilter\n");
    if(!InitDeviceFilter(config->name,config->path,DShowDeviceType_Video))
    {
	    Info(L"HDevice::SetVideoConfig InitDeviceFilter ERROR\n");
        return false;
    }
    OSEnterMutex(mConfigMutex);
    videoConfig = *config;
    OSLeaveMutex(mConfigMutex);
    Info(L"HDevice::SetVideoConfig DShowGraphicReConfig\n");

    if(!DShowGraphicReConfig(this))
    {
	    Info(L"HDevice::SetVideoConfig DShowGraphicReConfig ERROR\n");

        return false;
    }
    Info(L"HDevice::SetVideoConfig DShowGraphicReConfig SUCC\n");

    OSEnterMutex(mConfigMutex);
    *config = videoConfig;
    OSLeaveMutex(mConfigMutex);
    Info(L"HDevice::SetVideoConfig return true\n");
    return true;
}

bool HDevice::SetupExceptionAudioCapture(IPin *pin)
{
	ComPtr<IEnumMediaTypes>  enumMediaTypes;
	ULONG                    count = 0;
	HRESULT                  hr;
	MediaTypePtr             mt;

	hr = pin->EnumMediaTypes(&enumMediaTypes);
	if (FAILED(hr)) {
		WarningHR(L"SetupExceptionAudioCapture: pin->EnumMediaTypes "
		          L"failed", hr);
		return false;
	}

	enumMediaTypes->Reset();

	if (enumMediaTypes->Next(1, &mt, &count) == S_OK &&
	    mt->formattype == FORMAT_WaveFormatEx) {
		audioMediaType = mt;
		return true;
	}

	return false;
}
//设置音频捕获
bool HDevice::SetupAudioCapture(IBaseFilter *filter, AudioConfig &config,IGraphBuilder *)
{
	if (!filter){
		return false;
	}
   Info(L"[TODBG]HDevice::SetupAudioCapture");
	ComPtr<IPin>  pin;
	MediaTypePtr  defaultMT;
	bool          success;
	HRESULT       hr;

	success = GetFilterPin(filter, MEDIATYPE_Audio, PIN_CATEGORY_CAPTURE,
			PINDIR_OUTPUT, &pin);
	if (!success) {
		Error(L"SetupAudioCapture Could not get audio pin");
		return false;
	}

	ComQIPtr<IAMStreamConfig> pinConfig(pin);

	if (config.useDefaultConfig) {
		MediaTypePtr defaultMT;

		if (pinConfig && SUCCEEDED(pinConfig->GetFormat(&defaultMT))) {
			audioMediaType = defaultMT;
		} else {
			if (!SetupExceptionAudioCapture(pin)) {
				Error(L"Could not get default format for "
				      L"audio pin");
				return false;
			}
		}
	} else {
		if (!GetClosestAudioMediaType(filter, config, audioMediaType)) {
			Error(L"Could not get closest audio media type");
			return false;
		}
	}

	if (!!pinConfig) {
	    WAVEFORMATEX *wfex = (WAVEFORMATEX*)audioMediaType->pbFormat;
		hr = pinConfig->SetFormat(audioMediaType);

		if (FAILED(hr) && hr != E_NOTIMPL) {
			Error(L"Could not set audio format");
			return false;
		}
		ComQIPtr <IAMBufferNegotiation> pinNegotiation(pin);
      if (pinNegotiation != NULL) {
         ALLOCATOR_PROPERTIES prop = { 0 };
         hr = pinNegotiation->GetAllocatorProperties(&prop);
         if (hr != S_OK) {
            Error(L"Could not GetAllocatorProperties %X", hr);
         }       
             
         prop.cbBuffer = wfex->nSamplesPerSec * wfex->nChannels *wfex->wBitsPerSample / 8 * AUDIOENGINE_PERSAMPLE_TIME_MS / 1000 / 2;
         prop.cBuffers = 6;
         prop.cbAlign = wfex->wBitsPerSample *wfex->nChannels;
         hr = pinNegotiation->SuggestAllocatorProperties(&prop);
         if (hr != S_OK) {
            Error(L"Could not SuggestAllocatorProperties %X", hr);
         }
 		}
	}

	ConvertAudioSettings();

	PinCaptureInfo info;
	info.callback          = [this] (IMediaSample *s) {Receive(false, s);};
	info.expectedMajorType = audioMediaType->majortype;
	info.expectedSubType   = audioMediaType->subtype;

	audioCapture = new CaptureFilter(info);
    OSEnterMutex(mConfigMutex);
	audioConfig  = config;
    OSLeaveMutex(mConfigMutex);
	DShowGraphicHDeviceAddDeviceFilter(this,audioCapture,L"Audio Capture Filter");
	DShowGraphicHDeviceAddDeviceFilter(this,deviceFilter,L"Audio Filter");
	
	//graph->AddFilter(audioCapture, L"Audio Capture Filter");
	//graph->AddFilter(deviceFilter, L"Device Filter");
   
	return true;
}
bool HDevice::InitDeviceFilter
   (std::wstring &name,std::wstring &path,DShowDeviceType filterType)
{
   if(filterType == DShowDeviceType_Audio) {
      Info(L"[TODBG]HDevice::InitDeviceFilter");
   }
   
   //deviceFilter 不为空
   if(deviceFilter != NULL)
   {
      //设备类型不同
      if(this->filterType!=filterType)
      {
         if(filterType == DShowDeviceType_Audio)
            {Info(L"[TODBG]HDevice::InitDeviceFilter this->filterType!=filterType");}

         return false;
      }
      //设备名不一样
      if(wcscmp(deviceInfo.m_sDeviceName,name.c_str())!=0)
      {    
         if(filterType == DShowDeviceType_Audio)
            {Info(L"[TODBG]HDevice::InitDeviceFilter deviceName not current");}

         return false;
      }
      //设备路径不一样
      if(wcslen(deviceInfo.m_sDeviceID)!=path.size())
      {      
         if(filterType == DShowDeviceType_Audio)
            {Info(L"[TODBG]HDevice::InitDeviceFilter deviceID not current 1");}
      
         return false;
      }
      if(wcslen(deviceInfo.m_sDeviceID)!=0)
      {
         if(wcscmp(deviceInfo.m_sDeviceID,path.c_str())!=0)
         {
         
            if(filterType == DShowDeviceType_Audio)
               {Info(L"[TODBG]HDevice::InitDeviceFilter deviceID not current 2");}
            return false;
         }
      }
   
      if(filterType == DShowDeviceType_Audio)
         {Info(L"[TODBG]HDevice::InitDeviceFilter deviceFilter != NULL return true");}
   
      return true;
   }
   //deviceFilter 为空
   IID type;
   if(filterType==DShowDeviceType_Video)
   {
      type=CLSID_VideoInputDeviceCategory;
   }
   else if(filterType==DShowDeviceType_Audio)
   {
      type=CLSID_AudioInputDeviceCategory;
   }
   else
   {
   
      if(filterType == DShowDeviceType_Audio)
         {Info(L"[TODBG]HDevice::InitDeviceFilter unknow type of filterType");}
      
      //未知的filter 类型
      return false;
   }
   
	ComPtr<IBaseFilter> filter;
   //获得设备的Filter 
   bool success = GetDeviceFilter(type,name.c_str(), path.c_str(),&filter);
   if (!success&&filter!=NULL) {
      Error(L"Audio device '%s': %s not found", name.c_str(),path.c_str());
      return false;
   }

   if (name.size()) {
      size_t size = sizeof(deviceInfo.m_sDeviceName)/sizeof(wchar_t);

      wcscpy_s(deviceInfo.m_sDeviceName, size, name.c_str());
   }
   if (path.size())  {
      size_t size = sizeof(deviceInfo.m_sDeviceID) / sizeof(wchar_t);
      wcscpy_s(deviceInfo.m_sDeviceID, size, path.c_str());
   }
   
   if (filterType == DShowDeviceType_Video) {
      deviceInfo.m_sDeviceType = TYPE_DSHOW_VIDEO;
   }
   else if (filterType == DShowDeviceType_Audio) {
      deviceInfo.m_sDeviceType = TYPE_DSHOW_AUDIO;
   }
   
   this->filterType=filterType;
   this->deviceFilter=filter;
   
   if(filterType == DShowDeviceType_Audio)
      {Info(L"[TODBG]HDevice::InitDeviceFilter return true");}
   
   return true;
}


//设置音频配置
bool HDevice::SetAudioConfig(AudioConfig *config)
{
    Info(L"HDevice::SetAudioConfig\n");
    //DShowGraphicHDeviceStop(this);
   
    //清空音频捕获和输出的Filter
    //DShowGraphicHDeviceReleaseFilter(this,audioCapture);
   
    //audioCapture.Release();

    audioMediaType = NULL;

   
    if(!config)
    {
        if(filterType == DShowDeviceType_Audio)
            {Info(L"[TODBG]HDevice::SetAudioConfig config is null");}
        return false;
    }
    //初始化设备Filter,如果已经被初始化，则会返回成功
    if(!InitDeviceFilter(config->name,config->path,config->deviceType))
    {
        if(filterType == DShowDeviceType_Audio)
            {Info(L"[TODBG]HDevice::SetAudioConfig InitDeviceFilter failed!");}
      
        return false;
    }
    OSEnterMutex(mConfigMutex);
    audioConfig = *config;
    OSLeaveMutex(mConfigMutex);
    if(!DShowGraphicReConfig(this))
    {
        if(filterType == DShowDeviceType_Audio)
            {Info(L"[TODBG]HDevice::SetAudioConfig DShowGraphicReConfig failed!");}
  
        return false;
    }
    OSEnterMutex(mConfigMutex);
    *config = audioConfig;
    OSLeaveMutex(mConfigMutex);
    return true;
}

bool HDevice::FindCrossbar(IBaseFilter *filter, IBaseFilter **crossbar,
ICaptureGraphBuilder2 *builder)
{
	ComPtr<IPin> pin;
	REGPINMEDIUM medium;
	HRESULT hr;

	hr = builder->FindInterface(NULL, NULL, filter, IID_IAMCrossbar,
			(void**)crossbar);
	if (SUCCEEDED(hr))
		return true;

	if (!GetPinByName(filter, PINDIR_INPUT, nullptr, &pin))
		return false;
	if (!GetPinMedium(pin, medium))
		return false;
	if (!GetFilterByMedium(AM_KSCATEGORY_CROSSBAR, medium, crossbar))
		return false;

   DShowGraphicHDeviceAddDeviceFilter(this,*crossbar, L"Crossbar Filter");
	return true;
}

bool HDevice::ConnectPins(const GUID &category, const GUID &type,
		IBaseFilter *filter,
		IBaseFilter *capture,
		ICaptureGraphBuilder2 *builder,
		IGraphBuilder*          graph)
{
	HRESULT hr;
	ComPtr<IBaseFilter> crossbar;
	ComPtr<IPin> filterPin;
	ComPtr<IPin> capturePin;
	bool connectCrossbar = !encodedDevice && type == MEDIATYPE_Video;

	if (!EnsureInitialized(L"HDevice::ConnectPins") ||
	    !EnsureInactive(L"HDevice::ConnectPins"))
		return false;

	if (connectCrossbar && FindCrossbar(filter, &crossbar,builder)) {
		if (!DirectConnectFilters(graph, crossbar, filter)) {
			Warning(L"HDevice::ConnectPins: Failed to connect "
			        L"crossbar");
			return false;
		}
	}

	if (!GetFilterPin(filter, type, category, PINDIR_OUTPUT, &filterPin)) {
		Error(L"HDevice::ConnectPins: Failed to find pin");
		return false;
	}

	if (!GetPinByName(capture, PINDIR_INPUT, nullptr, &capturePin)) {
		Error(L"HDevice::ConnectPins: Failed to find capture pin");
		return false;
	}

	hr = graph->ConnectDirect(filterPin, capturePin, nullptr);
   WarningHR(L"HDevice::ConnectPins: to connect pins ",hr);
   
	if (FAILED(hr)) {
      switch (hr) {
      case  0x80040204:
            break;
         default:
            WarningHR(L"HDevice::ConnectPins: failed to connect pins",hr);
            return false;
            break;
      }

	}
   
   if(MEDIATYPE_Video==type)
   {
      GetPinMediaType(capturePin, videoMediaType);
   }

	return true;
}


bool HDevice::RenderFilters(const GUID &category, const GUID &type,IBaseFilter *filter, IBaseFilter *capture,ICaptureGraphBuilder2 *builder)
{
	HRESULT hr;

	if (!EnsureInitialized(L"HDevice::RenderFilters") ||
	    !EnsureInactive(L"HDevice::RenderFilters"))
		return false;

	hr = builder->RenderStream(&category, &type, filter, NULL, capture);
	if (FAILED(hr)) {
		WarningHR(L"HDevice::ConnectFilters: RenderStream failed", hr);
		return false;
	}

	return true;
}

void HDevice::ReleaseFilter()
{
	audioCapture.Release();
   audioCapture=NULL;
	videoCapture.Release(); 
   videoCapture=NULL;
   deviceFilter.Release();
   deviceFilter=NULL;
}

bool HDevice::ConnectFiltersInternal()
{
   Info(L"HDevice::ConnectFiltersInternal DShowGraphicHDeviceSetupVideoCapture\n");
	DShowGraphicHDeviceSetupVideoCapture(this);
   
   Info(L"HDevice::ConnectFiltersInternal DShowGraphicHDeviceSetupAudioCapture\n");
   DShowGraphicHDeviceSetupAudioCapture(this);

	bool success = true;

	if (!EnsureInitialized(L"ConnectFilters") ||
	    !EnsureInactive(L"ConnectFilters")){
	    
      Info(L"HDevice::ConnectFiltersInternal Ensure failed!\n");
		return false;
   }
   
   Info(L"HDevice::ConnectFiltersInternal Ensure Successed!\n");
	if (videoCapture != NULL) {
      
      Info(L"HDevice::ConnectFiltersInternal videoCapture not null\n");
		success = DShowGraphicHDeviceConnectPin(this,PIN_CATEGORY_CAPTURE,
				MEDIATYPE_Video, deviceFilter,
				videoCapture);
		if (!success) {
			success = DShowGraphicHDeviceRenderFilters(this,PIN_CATEGORY_CAPTURE,
					MEDIATYPE_Video, deviceFilter,
					videoCapture);
		}
	}

	if (audioCapture && success) {
      
      Info(L"HDevice::ConnectFiltersInternal audioCapture not null or successed\n");
		IBaseFilter *filter = audioCapture.Get();

		success = DShowGraphicHDeviceConnectPin(this,PIN_CATEGORY_CAPTURE,
				MEDIATYPE_Audio, deviceFilter,
				filter);
		if (!success) {
			success = DShowGraphicHDeviceRenderFilters(this,PIN_CATEGORY_CAPTURE,
					MEDIATYPE_Audio, deviceFilter,
					filter);
		}
	}
   else if(!audioCapture){
      Info(L"HDevice::ConnectFiltersInternal audioCapture null\n");
   }
   else if(!success){
      Info(L"HDevice::ConnectFiltersInternal videoCature connect failed\n");
   }

   isResetVideo=true;
   isResetAudio=true;
   
   Info(L"HDevice::ConnectFiltersInternal return \n");

	return success;
}
Result HDevice::Start()
{
   wstring devId = wstring(deviceInfo.m_sDeviceName) + wstring(deviceInfo.m_sDeviceID);
   if(DShowGraphicHDeviceStart((wchar_t*)devId.c_str())){
      return Result::Success;
   }
   return Result::Error;
}

void HDevice::Stop()
{
    wstring devId = wstring(deviceInfo.m_sDeviceName) + wstring(deviceInfo.m_sDeviceID);
    DShowGraphicHDeviceStop((wchar_t*)devId.c_str());
}

void HDevice::ReStart()
{
   DShowGraphicHDeviceRestart(this);
}

bool HDevice::GetDeviceFrameInfoList(DeviceInfo info,FrameInfoList *frameList,FrameInfo *currFrame)
{
   Info(L"HDevice::GetDeviceFrameInfoList\n");
   //根据info获得filter
   
   ComPtr<IGraphBuilder>          graph;
	ComPtr<ICaptureGraphBuilder2> builder;
	ComPtr<IMediaControl> control;

	ComPtr<IBaseFilter> filter;
	ComPtr<IPin>  pin;
	bool          success;   
   vector<VideoInfo> caps;

   Info(L"HDevice::GetDeviceFrameInfoList GetDeviceFilter CLSID_VideoInputDeviceCategory\n");
   //获得设备的Filter 
   success = GetDeviceFilter(CLSID_VideoInputDeviceCategory,
      info.m_sDeviceName, info.m_sDeviceID,&filter);
   if (!success&&filter!=NULL) {
      
      Info(L"HDevice::GetDeviceFrameInfoList GetDeviceFilter failed!,or !success\n");
      return false;
   }
   
   Info(L"HDevice::GetDeviceFrameInfoList GetFilterPin\n");
   //根据Filter获得Pin
   success = GetFilterPin(filter, MEDIATYPE_Video, PIN_CATEGORY_CAPTURE,PINDIR_OUTPUT, &pin);

   if(!success)
   {
      Info(L"HDevice::GetDeviceFrameInfoList GetFilterPin Failed!\n");
      if(!CreateFilterGraph(&graph, &builder, &control))
      {
         Info(L"HDevice::GetDeviceFrameInfoList CreateFilterGraph Failed!\n");
         return false;
      }
      
      Info(L"HDevice::GetDeviceFrameInfoList CoCreateInstance graph->AddFilter\n");
      if(filter==NULL) {
         Info(L"HDevice::GetDeviceFrameInfoList CoCreateInstance graph->AddFilter NULL\n");
      }
      else{
         Info(L"HDevice::GetDeviceFrameInfoList CoCreateInstance graph->AddFilter not NULL\n");
      }
      Info(L"HDevice::GetDeviceFrameInfoList CoCreateInstance graph->AddFilter %s\n",info.m_sDeviceName);
      if(graph==NULL)
      {
         Info(L"graph is NULL\n");
      }
      else
      {

         Info(L"graph is not NULL\n");
      }

      Sleep(100);
      graph->AddFilter(filter,L"Video Capture Filter");
      
      Info(L"HDevice::GetDeviceFrameInfoList GetFilterPin2\n");
      success = GetFilterPin(filter, MEDIATYPE_Video, PIN_CATEGORY_CAPTURE,PINDIR_OUTPUT, &pin);  
      Info(L"HDevice::GetDeviceFrameInfoList GetFilterPin2 End\n");
   }
   else{
      Info(L"HDevice::GetDeviceFrameInfoList GetFilterPin Successed!\n");
   }

   if(success&&pin!=NULL)
   {
      Info(L"HDevice::GetDeviceFrameInfoList EnumVideoCaps\n");
      if (EnumVideoCaps(pin,caps))
      {
         Info(L"HDevice::GetDeviceFrameInfoList EnumVideoCaps Successed!\n");
         for(auto itor=caps.begin();itor!=caps.end();itor++)
         {
            FrameInfo info;            
            info.minFrameInterval=itor->minInterval; 
            info.maxFrameInterval=itor->maxInterval;
            info.minCX=itor->minCX;
            info.minCY=itor->minCY;
            info.maxCX=itor->maxCX;
            info.maxCY=itor->maxCY;
            info.xGranularity=itor->granularityCX;
            info.yGranularity=itor->granularityCY;
            info.bUsingFourCC=false;
            info.format=itor->format;
            if(frameList)
            {
               frameList->Append(info);
            }
         }
         
         Info(L"HDevice::GetDeviceFrameInfoList EnumVideoCaps Successed! GetFormat\n");
         ComQIPtr<IAMStreamConfig> pinConfig(pin);
         MediaTypePtr am_type;
         HRESULT hr=pinConfig->GetFormat(&am_type);
         if (SUCCEEDED(hr) && !(am_type==NULL)) {
            VideoFormat format;
            bool bGetTypeSuc = GetMediaTypeVFormat(*am_type, format);
            if (bGetTypeSuc) {
               VIDEOINFOHEADER  *vih = (VIDEOINFOHEADER*)am_type->pbFormat;
               if (vih) {
                  currFrame->minFrameInterval = vih->AvgTimePerFrame;
                  currFrame->maxFrameInterval = vih->AvgTimePerFrame;
                  currFrame->minCX = vih->bmiHeader.biWidth;
                  currFrame->minCY = vih->bmiHeader.biHeight;
                  currFrame->maxCX = vih->bmiHeader.biWidth;
                  currFrame->maxCY = vih->bmiHeader.biHeight;
               }
               currFrame->format = format;
            }
            else {
               Info(L"HDevice::GetDeviceFrameInfoList GetMediaTypeVFormat return false!\n");
               return false;
            }
         }
         Info(L"HDevice::GetDeviceFrameInfoList EnumVideoCaps return true!\n");
         return true;
      }
   }
   Info(L"HDevice::GetDeviceFrameInfoList return\n");
   return true;
}

bool HDevice::GetCurrDeviceFrameInfoList(FrameInfoList *frameList,
   FrameInfo *currFrame)
{
   vector<VideoInfo> caps;

	ComPtr<IPin>  pin;
	bool success = GetFilterPin(deviceFilter,
         MEDIATYPE_Video,
			PIN_CATEGORY_CAPTURE,
			PINDIR_OUTPUT, 
			&pin);
   if(success&&pin!=NULL)
   {
      if (EnumVideoCaps(pin,caps))
      {
         for(auto itor=caps.begin();itor!=caps.end();itor++)
         {
            FrameInfo info;            
            info.minFrameInterval=itor->minInterval; 
            info.maxFrameInterval=itor->maxInterval;
            info.minCX=itor->minCX;
            info.minCY=itor->minCY;
            info.maxCX=itor->maxCX;
            info.maxCY=itor->maxCY;
            info.xGranularity=itor->granularityCX;
            info.yGranularity=itor->granularityCY;
            info.bUsingFourCC=false;
            info.format=itor->format;
            if(frameList)
            {
               frameList->Append(info);
            }
         }
         
         ComQIPtr<IAMStreamConfig> pinConfig(pin);
         MediaTypePtr am_type;
         HRESULT hr=pinConfig->GetFormat(&am_type);
         if (SUCCEEDED(hr)&&currFrame!=NULL)  {
            VideoFormat format;
            bool bGetTypeSuc = GetMediaTypeVFormat(*am_type, format);
            if (bGetTypeSuc) {
               VIDEOINFOHEADER *vih = (VIDEOINFOHEADER*)am_type->pbFormat;
               if (vih) {
                  currFrame->minFrameInterval = vih->AvgTimePerFrame;
                  currFrame->maxFrameInterval = vih->AvgTimePerFrame;
                  currFrame->minCX = vih->bmiHeader.biWidth;
                  currFrame->minCY = vih->bmiHeader.biHeight;
                  currFrame->maxCX = vih->bmiHeader.biWidth;
                  currFrame->maxCY = vih->bmiHeader.biHeight;
               }
               currFrame->format = format;
            }
            else {
               return false;
            }
         }
         return true;
      }
   }
   return false;
}
bool HDevice::SetDeviceDefaultAttribute(DeviceInfo deviceInfo,UINT width,UINT height,int frameInternal,VideoFormat)
{
	ComPtr<IPin>  pin;
	HRESULT       hr;
	bool          success,ret=true;
	ComPtr<IAMStreamConfig>  config;

	ComPtr<IBaseFilter> filter;
   //获得设备的Filter 
   success = GetDeviceFilter(CLSID_VideoInputDeviceCategory,
      deviceInfo.m_sDeviceName,
      deviceInfo.m_sDeviceID
      ,&filter);
   
   if (!success&&filter!=NULL) {
      return false;
   }
   
	success = GetFilterPin(filter, MEDIATYPE_Video, PIN_CATEGORY_CAPTURE,
			PINDIR_OUTPUT, &pin);
   if(success&&pin!=NULL)
   {
      if (FAILED(hr = pin->QueryInterface(IID_IAMStreamConfig, (void**)&config))) 
      {
         ret=false;
      }
      else
      {
         AM_MEDIA_TYPE *pmt;
         if(SUCCEEDED(config->GetFormat(&pmt)))
         {
            VIDEOINFOHEADER *pVih = reinterpret_cast<VIDEOINFOHEADER*>(pmt->pbFormat);
            pVih->AvgTimePerFrame = frameInternal;
            pVih->bmiHeader.biWidth=width;
            pVih->bmiHeader.biHeight=height;
            pVih->bmiHeader.biSize=width*height*4;
            if (FAILED(hr=config->SetFormat(pmt))) {
            
               ret=false;
            }         
         }  
      }
   }
   
   return ret;
}
bool HDevice::SetDeviceDefaultAttribute(int width,int height,int frameInternal,
   VideoFormat )
{
	ComPtr<IPin>  pin;
	HRESULT       hr;
	bool          success;
   
	ComPtr<IAMStreamConfig>  config;
   bool ret=true;
   Stop();
   
	success = GetFilterPin(deviceFilter, MEDIATYPE_Video, PIN_CATEGORY_CAPTURE,
			PINDIR_OUTPUT, &pin);
   if(success&&pin!=NULL)
   {
      if (FAILED(hr = pin->QueryInterface(IID_IAMStreamConfig, (void**)&config))) 
      {
         ret=false;
      }
      else
      {
         AM_MEDIA_TYPE *pmt;
         if(SUCCEEDED(config->GetFormat(&pmt)))
         {
            VIDEOINFOHEADER *pVih = reinterpret_cast<VIDEOINFOHEADER*>(pmt->pbFormat);
            pVih->AvgTimePerFrame = frameInternal;
            pVih->bmiHeader.biWidth=width;
            pVih->bmiHeader.biHeight=height;
         
            if (FAILED(hr=config->SetFormat(pmt))) {
            
               ret=false;
            }
            else
            {
                OSEnterMutex(mConfigMutex);
                videoConfig.cx=width;
                videoConfig.cy=height;
                videoConfig.frameInterval=frameInternal;
                OSLeaveMutex(mConfigMutex);
            }
         }  
      }
   }
   
   Start();
   return ret;
}

}; /* namespace DShow */
