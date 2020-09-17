#include "Utility.h"
#include "CoreAudioSource.h"
#include <share.h>
#include "Logging.h"
#define Log gLogger->logInfo
#define AppWarning gLogger->logWarning
#define SafeRelease(var) if(var) {var->Release(); var = NULL;}
void OBSApiUIWarning(wchar_t *d);

UINT ConvertMSTo100NanoSec(UINT ms) {
   return ms * 1000 * 10; //1000 microseconds, then 10 "100nanosecond" segments
}

const static TCHAR *IAudioHRESULTToString(HRESULT hr) {
   __declspec(thread) static TCHAR hResultCode[16];

   switch (hr) {
   case AUDCLNT_E_SERVICE_NOT_RUNNING:
      return TEXT("AUDCLNT_E_SERVICE_NOT_RUNNING");
   case AUDCLNT_E_ALREADY_INITIALIZED:
      return TEXT("AUDCLNT_E_ALREADY_INITIALIZED");
   case AUDCLNT_E_WRONG_ENDPOINT_TYPE:
      return TEXT("AUDCLNT_E_WRONG_ENDPOINT_TYPE");
   case AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED:
      return TEXT("AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED");
   case AUDCLNT_E_BUFFER_SIZE_ERROR:
      return TEXT("AUDCLNT_E_BUFFER_SIZE_ERROR");
   case AUDCLNT_E_CPUUSAGE_EXCEEDED:
      return TEXT("AUDCLNT_E_CPUUSAGE_EXCEEDED");
   case AUDCLNT_E_DEVICE_INVALIDATED:
      return TEXT("AUDCLNT_E_DEVICE_INVALIDATED");
   case AUDCLNT_E_DEVICE_IN_USE:
      return TEXT("AUDCLNT_E_DEVICE_IN_USE");
   case AUDCLNT_E_ENDPOINT_CREATE_FAILED:
      return TEXT("AUDCLNT_E_ENDPOINT_CREATE_FAILED");
   case AUDCLNT_E_INVALID_DEVICE_PERIOD:
      return TEXT("AUDCLNT_E_INVALID_DEVICE_PERIOD");
   case AUDCLNT_E_UNSUPPORTED_FORMAT:
      return TEXT("AUDCLNT_E_UNSUPPORTED_FORMAT");
   case AUDCLNT_E_EXCLUSIVE_MODE_NOT_ALLOWED:
      return TEXT("AUDCLNT_E_EXCLUSIVE_MODE_NOT_ALLOWED");
   case AUDCLNT_E_BUFDURATION_PERIOD_NOT_EQUAL:
      return TEXT("AUDCLNT_E_BUFDURATION_PERIOD_NOT_EQUAL");
   case AUDCLNT_E_NOT_INITIALIZED:
      return TEXT("AUDCLNT_E_NOT_INITIALIZED");
   case AUDCLNT_E_NOT_STOPPED:
      return TEXT("AUDCLNT_E_NOT_STOPPED");
   case AUDCLNT_E_EVENTHANDLE_NOT_SET:
      return TEXT("AUDCLNT_E_EVENTHANDLE_NOT_SET");
   case AUDCLNT_E_BUFFER_OPERATION_PENDING:
      return TEXT("AUDCLNT_E_BUFFER_OPERATION_PENDING");

   case E_POINTER:
      return TEXT("E_POINTER");
   case E_INVALIDARG:
      return TEXT("E_INVALIDARG");
   case E_OUTOFMEMORY:
      return TEXT("E_OUTOFMEMORY");
   case E_NOINTERFACE:
      return TEXT("E_NOINTERFACE");

   default:
      tsprintf_s(hResultCode, _countof(hResultCode), TEXT("%08lX"), hr);
      return hResultCode;
   }
}
CBlankAudioPlayback::CBlankAudioPlayback(CTSTR lpDevice) {

   const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
   const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
   const IID IID_IAudioClient = __uuidof(IAudioClient);
   const IID IID_IAudioRenderClient = __uuidof(IAudioRenderClient);

   HRESULT err;
   err = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator, (void**)&mmEnumerator);
   if (FAILED(err))
      CrashError(TEXT("Could not create IMMDeviceEnumerator: 0x%08lx"), err);

   if (scmpi(lpDevice, TEXT("Default")) == 0)
      err = mmEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &mmDevice);
   else
      err = mmEnumerator->GetDevice(lpDevice, &mmDevice);
   if (FAILED(err))
      CrashError(TEXT("Could not create IMMDevice"));

   err = mmDevice->Activate(IID_IAudioClient, CLSCTX_ALL, NULL, (void**)&mmClient);
   if (FAILED(err))
      CrashError(TEXT("Could not create IAudioClient"));

   WAVEFORMATEX *pwfx;
   err = mmClient->GetMixFormat(&pwfx);
   if (FAILED(err))
      CrashError(TEXT("Could not get mix format from audio client"));

   UINT inputBlockSize = pwfx->nBlockAlign;

   err = mmClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, ConvertMSTo100NanoSec(1000), 0, pwfx, NULL);
   if (FAILED(err))
      CrashError(TEXT("Could not initialize audio client, error = %08lX"), err);

   err = mmClient->GetService(IID_IAudioRenderClient, (void**)&mmRender);
   if (FAILED(err))
      CrashError(TEXT("Could not get audio render client"));

   //----------------------------------------------------------------

   UINT bufferFrameCount;
   err = mmClient->GetBufferSize(&bufferFrameCount);
   if (FAILED(err))
      CrashError(TEXT("Could not get audio buffer size"));

   BYTE *lpData;
   err = mmRender->GetBuffer(bufferFrameCount, &lpData);
   if (FAILED(err))
      CrashError(TEXT("Could not get audio buffer"));

   zero(lpData, bufferFrameCount*inputBlockSize);

   mmRender->ReleaseBuffer(bufferFrameCount, 0);//AUDCLNT_BUFFERFLAGS_SILENT); //probably better if it doesn't know

   if (FAILED(mmClient->Start()))
      CrashError(TEXT("Could not start audio source"));
}
CBlankAudioPlayback::~CBlankAudioPlayback() {
   mmClient->Stop();

   SafeRelease(mmRender);
   SafeRelease(mmClient);
   SafeRelease(mmDevice);
   SafeRelease(mmEnumerator);
}

//==============================================================================================================================
CoreAudioSource::CoreAudioSource(bool useInputDevice, FUN_GetVideoTime funGetVideoTime, UINT dstSampleRateHz, bool useQPC, bool useVideoTime, int globalAdjust) :
IAudioSource(eAudioSource_Mic),
mUseInputDevice(useInputDevice),
mFunGetVideoTime(funGetVideoTime),
mUseQPC(useQPC) {
   mDstSampleRateHz = dstSampleRateHz;
   mCurBlankPlaybackThingy = NULL;
#ifdef DEBUG_CORE_AUDIO_SOURCE
   String rawFile;
   rawFile << L"d:\\" << mDeviceId << (mUseInputDevice == true ? L"recoding" : L"playback") << L"_raw.pcm";
   mPcmFile = _wfsopen(rawFile, L"wb", _SH_DENYNO);
#endif
   SetVolume(1.0f);
}
CoreAudioSource::~CoreAudioSource() {
   StopBlankSoundPlayback();
   StopCapture();
   FreeData();
   SafeRelease(mmEnumerator);
}
bool CoreAudioSource::Reinitialize() {

   const IID IID_IAudioClient = __uuidof(IAudioClient);
   const IID IID_IAudioCaptureClient = __uuidof(IAudioCaptureClient);
   //HRESULT err;

   if (bIsMic) {
      BOOL bMicSyncFixHack = true;
      mAngerThreshold = bMicSyncFixHack ? 40 : 1000;
   }

   gLogger->logInfo(L"CoreAudioSource::Reinitialize will get audio client id=%s name:%s", mDeviceId.Array(), mDeviceName.Array());
	if (NULL == mmDevice)
	{
		if (scmpi(mDeviceId, TEXT("Default")) == 0)
         mInitErr = mmEnumerator->GetDefaultAudioEndpoint(mUseInputDevice ? eCapture : eRender, mUseInputDevice ? eCommunications : eConsole, &mmDevice);
		else
         mInitErr = mmEnumerator->GetDevice(mDeviceId, &mmDevice);
		if (FAILED(mInitErr)) {
			if (!deviceLost)
				AppWarning(TEXT("CoreAudioSource::Initialize(%d): Could not create IMMDevice = %s"), (BOOL)bIsMic, IAudioHRESULTToString(mInitErr));
			return false;
		}
	}


	if (NULL == mmClient)
	{
      mInitErr = mmDevice->Activate(IID_IAudioClient, CLSCTX_ALL, NULL, (void**)&mmClient);
		if (FAILED(mInitErr)) {
			if (!deviceLost)
				gLogger->logWarning(TEXT("CoreAudioSource::Initialize(%d): Could not create IAudioClient = %s"), (BOOL)bIsMic, IAudioHRESULTToString(mInitErr));
			return false;
		}
	}


   //-----------------------------------------------------------------
   // get name
   IPropertyStore *store = NULL;
   if (SUCCEEDED(mmDevice->OpenPropertyStore(STGM_READ, &store))) {
      PROPVARIANT varName;
      PropVariantInit(&varName);
      if (SUCCEEDED(store->GetValue(PKEY_Device_FriendlyName, &varName))) {
         CWSTR wstrName = varName.pwszVal;
         mDeviceName = wstrName;
      }      
   }
   if (store) {
      store->Release();
      store = NULL;
   }

   if (bIsMic) {
      if (!deviceLost) {
         Log(TEXT("------------------------------------------"));
         Log(L"Using auxilary audio input: %s", GetDeviceName());
      }

      if (mUseQPC)
         Log(TEXT("Using Mic QPC timestamps"));
   } else {
      if (!deviceLost) {
         Log(TEXT("------------------------------------------"));
         Log(TEXT("Using desktop audio input: %s"), GetDeviceName());
      }
      Log(L"Global Audio time adjust: %d", mGlobalAdjustTime);
      SetTimeOffset(mGlobalAdjustTime);
   }

   //-----------------------------------------------------------------
   // get format

   WAVEFORMATEX *pwfx;
   mInitErr = mmClient->GetMixFormat(&pwfx);
   if (FAILED(mInitErr)) {
      if (!deviceLost) AppWarning(TEXT("CoreAudioSource::Initialize(%d): Could not get mix format from audio client = %s"), (BOOL)bIsMic, IAudioHRESULTToString(mInitErr));
      return false;
   }

   bool  bFloat;
   UINT  inputChannels;
   UINT  inputSamplesPerSec;
   UINT  inputBitsPerSample;
   UINT  inputBlockSize;
   DWORD inputChannelMask = 0;
   WAVEFORMATEXTENSIBLE *wfext = NULL;

   //the internal audio engine should always use floats (or so I read), but I suppose just to be safe better check
   if (pwfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {//波形声音的格式
      wfext = (WAVEFORMATEXTENSIBLE*)pwfx;
      inputChannelMask = wfext->dwChannelMask;

      if (wfext->SubFormat != KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) {
         if (!deviceLost) AppWarning(TEXT("CoreAudioSource::Initialize(%d): Unsupported wave format"), (BOOL)bIsMic);
         CoTaskMemFree(pwfx);
         return false;
      }
   } else if (pwfx->wFormatTag != WAVE_FORMAT_IEEE_FLOAT) {
      if (!deviceLost) AppWarning(TEXT("CoreAudioSource::Initialize(%d): Unsupported wave format"), (BOOL)bIsMic);
      CoTaskMemFree(pwfx);
      return false;
   }

   bFloat = true;
   inputChannels = pwfx->nChannels;//通道数量 1:单声道 2：立体声
   inputBitsPerSample = 32;
   inputBlockSize = pwfx->nBlockAlign;  //块对齐方式 / 最小数据的原子大小
   inputSamplesPerSec = pwfx->nSamplesPerSec;//每个升到播放和记录时的样本频率
   mSampleWindowSize = (inputSamplesPerSec / 100);
   int sampele = inputSamplesPerSec % 100;
   if (sampele >= 50) {
      mSampleWindowSize += 1;
   }
   DWORD flags = mUseInputDevice ? 0 : AUDCLNT_STREAMFLAGS_LOOPBACK;

   Log(L"CoreAudioSource::Reinitialize() mmClient->Initialize\n");
   mInitErr = mmClient->Initialize(AUDCLNT_SHAREMODE_SHARED, flags, ConvertMSTo100NanoSec(5000), 0, pwfx, NULL);
   Log(L"CoreAudioSource::Reinitialize() mmClient->Initialize End\n");

   //err = AUDCLNT_E_UNSUPPORTED_FORMAT;

   if (FAILED(mInitErr)) {
      if (!deviceLost) {
         AppWarning(TEXT("CoreAudioSource::Initialize(%d): Could not initialize audio client, result = %s"), (BOOL)bIsMic, IAudioHRESULTToString(mInitErr));
      }
      CoTaskMemFree(pwfx);
      return false;
   }

   //-----------------------------------------------------------------
   // acquire services

	if (NULL==mmCapture)
	{
      mInitErr = mmClient->GetService(IID_IAudioCaptureClient, (void**)&mmCapture);
		if (FAILED(mInitErr)) {
			if (!deviceLost)
				AppWarning(TEXT("CoreAudioSource::Initialize(%d): Could not get audio capture client, result = %s"), (BOOL)bIsMic, IAudioHRESULTToString(mInitErr));
			CoTaskMemFree(pwfx);
			return false;
		}
	}

	if (NULL == mmClock)
	{
      mInitErr = mmClient->GetService(__uuidof(IAudioClock), (void**)&mmClock);
		if (FAILED(mInitErr)) {
			if (!deviceLost)
				AppWarning(TEXT("CoreAudioSource::Initialize(%d): Could not get audio capture clock, result = %s"), (BOOL)bIsMic, IAudioHRESULTToString(mInitErr));
			CoTaskMemFree(pwfx);
			return false;
		}
	}


   CoTaskMemFree(pwfx);//释放 内存块 pwfx

   if (!mUseInputDevice && !bIsMic) {
      StopBlankSoundPlayback();
      StartBlankSoundPlayback(mDeviceId);
   }

   //-----------------------------------------------------------------

   InitAudioData(bFloat, inputChannels, inputSamplesPerSec, inputBitsPerSample, inputBlockSize, inputChannelMask);

   deviceLost = false;

   return true;
}

void CoreAudioSource::FreeData() {
   SafeRelease(mmCapture);
   SafeRelease(mmClient);
   SafeRelease(mmDevice);
   SafeRelease(mmClock);
}

HRESULT CoreAudioSource::GetErrorType() {
   return mInitErr; 
}

bool CoreAudioSource::Initialize()
{
	return Initialize(bIsMic, mDeviceId);
}

bool CoreAudioSource::Initialize(bool bMic, CTSTR lpID) {
	StopBlankSoundPlayback();
	StopCapture();
	FreeData();


	SafeRelease(mmEnumerator);
	if (NULL==mmEnumerator)
	{
		const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
		const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
		AppWarning("CoreAudioSource::Initialize bMic(%d)", bMic);
		bIsMic = bMic;
		mDeviceId = lpID;

      mInitErr = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator, (void**)&mmEnumerator);
		if (FAILED(mInitErr)) {
			AppWarning(TEXT("CoreAudioSource::Initialize(%d): Could not create IMMDeviceEnumerator = %s"), (BOOL)bIsMic, IAudioHRESULTToString(mInitErr));
			return false;
		}
	}

   bool ret = Reinitialize();

   return ret;
}
void CoreAudioSource::Reset() {
   Log(L"User purposely reset the device '%s'.  Did it go out, or were there audio issues that made the user want to do this?", GetDeviceName());
   deviceLost = true;
   reinitTimer = GetQPCTimeMS();
   //fakeAudioTimer = GetQPCTimeMS();
   FreeData();
}
void CoreAudioSource::StartCapture() {
	if (mmClient && mmClock && !mbIsStart) {
      mmClient->Start();
      UINT64 freq;
      mmClock->GetFrequency(&freq);
      mbIsStart = true;
   }
}

void CoreAudioSource::StopCapture() {
   mbIsStart = false;
   if (mmClient)
      mmClient->Stop();
#ifdef DEBUG_CORE_AUDIO_SOURCE
   fclose(mPcmFile);
#endif

}

QWORD CoreAudioSource::GetTimestampInMs(QWORD qpcTimestamp) {
   QWORD newTimestamp;

   if (bIsMic) {
      //newTimestamp = (bUseQPC) ? qpcTimestamp : App->GetAudioTime();
      newTimestamp = qpcTimestamp;
      newTimestamp += GetTimeOffset();

      //Log(TEXT("got some mic audio, timestamp: %llu"), newTimestamp);

      return newTimestamp;
   } 
   else {
      if (!bFirstFrameReceived) {
         QWORD curTime = GetQPCTimeMS();

         newTimestamp = qpcTimestamp;

         mCurVideoTime = mLastVideoTime = mFunGetVideoTime();

         bFirstFrameReceived = true;
      } else {
         QWORD newVideoTime = mFunGetVideoTime();

         if (newVideoTime != mLastVideoTime)
            mCurVideoTime = mLastVideoTime = newVideoTime;
         else
            mCurVideoTime += 10;

         newTimestamp = (mUseVideoTime) ? mCurVideoTime : qpcTimestamp;
         newTimestamp += GetTimeOffset();
      }

      return newTimestamp;
   }
}

bool CoreAudioSource::GetNextBuffer(void **buffer, UINT *numFrames, QWORD *timestamp) {
   UINT captureSize = 0;
   bool bFirstRun = true;
   HRESULT hRes;
   UINT64 devPosition, qpcTimestamp;
   LPBYTE captureBuffer;
   UINT32 numFramesRead;
   DWORD dwFlags = 0;

   if (deviceLost) {
      QWORD timeVal = GetQPCTimeMS();
      QWORD timer = (timeVal - reinitTimer);
      if (timer > 2000) {
		   AppWarning("CoreAudioSource::GetNextBuffer timer > 2000");         
         OBSApiUIWarning(L"声音采集无数据，正在重新加载设备");
         if (Reinitialize()) {
            Log(L"Device '%s' reacquired.", mDeviceName.Array());
            StartCapture();
         }
         reinitTimer = timeVal;
      }

      return false;
   }
   
   if (!mmCapture){
	   Log(L"CoreAudioSource::GetNextBuffer mmCapture is Null\n"); 
	   return false;
   }
   while (true) {
	   if ((mInputBufferSize >= mSampleWindowSize*GetChannelCount() )&& GetChannelCount()>0) {
         if (bFirstRun)
            mLastQPCTimestamp += 10;
         mFirstTimestamp = GetTimestampInMs(mLastQPCTimestamp);
         break;
      }		

		if (NULL == mmCapture){
			Log(L"CoreAudioSource::GetNextBuffer mmCapture is Null\n");
			return false;
		}

		hRes = mmCapture->GetNextPacketSize(&captureSize);

      if (FAILED(hRes)) {
         if (hRes == AUDCLNT_E_DEVICE_INVALIDATED) {
            FreeData();
            deviceLost = true;
            Log(L"Audio device '%s' has been lost, attempting to reinitialize", mDeviceName.Array());
            reinitTimer = GetQPCTimeMS();
            return false;
         }
         else if (hRes == AUDCLNT_E_RESOURCES_INVALIDATED) {
            FreeData();
            deviceLost = true;
            Log(L"Audio device '%s' has been lost, attempting to reinitialize", mDeviceName.Array());
            reinitTimer = GetQPCTimeMS();
         }
         RUNONCE AppWarning(TEXT("CoreAudioSource::GetBuffer: GetNextPacketSize failed, result = %s"), IAudioHRESULTToString(hRes));
         return false;
      }

      if (!captureSize)
         return false;

      //---------------------------------------------------------
		if (NULL == mmCapture){
			Log(L"CoreAudioSource::GetNextBuffer mmCapture is Null\n");
			return false;
		}
		hRes = mmCapture->GetBuffer(&captureBuffer, &numFramesRead, &dwFlags, &devPosition, &qpcTimestamp);

      if (FAILED(hRes)) {
         RUNONCE AppWarning(TEXT("CoreAudioSource::GetBuffer: GetBuffer failed, result = %s"), IAudioHRESULTToString(hRes));
         return false;
      }

      UINT totalFloatsRead = numFramesRead*GetChannelCount();

      if (mInputBufferSize) {
         double timeAdjust = double(mInputBufferSize / GetChannelCount());
         timeAdjust /= (double(GetSamplesPerSec())*0.0000001);

         qpcTimestamp -= UINT64(timeAdjust);
      }

      qpcTimestamp /= 10000;
      mLastQPCTimestamp = qpcTimestamp;

      //---------------------------------------------------------

      UINT newInputBufferSize = mInputBufferSize + totalFloatsRead;
      if (newInputBufferSize > mInputBuffer.Num())
         mInputBuffer.SetSize(newInputBufferSize);

      mcpy(mInputBuffer.Array() + mInputBufferSize, captureBuffer, totalFloatsRead*sizeof(float));
      mInputBufferSize = newInputBufferSize;
#ifdef DEBUG_CORE_AUDIO_SOURCE
      fwrite(captureBuffer, 1, totalFloatsRead*sizeof(float), mPcmFile);
      fflush(mPcmFile);
#endif
		if (NULL == mmCapture){
			Log(L"CoreAudioSource::GetNextBuffer mmCapture is Null\n");
			return false;
		}

		mmCapture->ReleaseBuffer(numFramesRead);

      bFirstRun = false;
   }

   *numFrames = mSampleWindowSize;
   *buffer = (void*)mInputBuffer.Array();
   *timestamp = mFirstTimestamp;

   return true;
}

void CoreAudioSource::ReleaseBuffer() {
   UINT sampleSizeFloats = mSampleWindowSize*GetChannelCount();
   if (mInputBufferSize > sampleSizeFloats)
      mcpy(mInputBuffer.Array(), mInputBuffer.Array() + sampleSizeFloats, (mInputBufferSize - sampleSizeFloats)*sizeof(float));
   mInputBufferSize -= sampleSizeFloats;
}


void CoreAudioSource::StartBlankSoundPlayback(CTSTR lpDevice) {
   if (!mCurBlankPlaybackThingy)
      mCurBlankPlaybackThingy = new CBlankAudioPlayback(lpDevice);
}

void CoreAudioSource::StopBlankSoundPlayback() {
   if (mCurBlankPlaybackThingy) {
      delete mCurBlankPlaybackThingy;
      mCurBlankPlaybackThingy = NULL;
   }
}
CTSTR CoreAudioSource::GetDeviceName() const {
   return mDeviceName.Array();
}
