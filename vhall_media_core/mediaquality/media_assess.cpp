#include <direct.h>
#include "media_assess.h"
#include "live_define.h"
#include "../common/vhall_log.h"

#define  MAX_FRAME_NUM 20

#if defined _WIN32 || defined WIN32
static FILETIME mPreIdleTime;
static FILETIME mPreKernelTime;
static FILETIME mPreUserTime;

__int64 CompareFileTime(FILETIME time1, FILETIME time2){
   __int64 a = time1.dwHighDateTime << 32 | time1.dwLowDateTime;
   __int64 b = time2.dwHighDateTime << 32 | time2.dwLowDateTime;
   
   return (b - a);
};
#endif // _WIN32

//FILE *infp = nullptr;
//FILE *outfp = nullptr;

int CpuUsageRateDetection() {
	int cpu_occupancy_rate = 0;
#if defined _WIN32 || defined WIN32
	FILETIME idleTime;
	FILETIME kernelTime;
	FILETIME userTime;
	GetSystemTimes(&idleTime, &kernelTime, &userTime);
	__int64 idle = CompareFileTime(mPreIdleTime, idleTime);
	__int64 kernel = CompareFileTime(mPreKernelTime, kernelTime);
	__int64 user = CompareFileTime(mPreUserTime, userTime);
     
	cpu_occupancy_rate = (kernel + user - idle) * 100 / (kernel + user);
	mPreIdleTime = idleTime;
	mPreKernelTime = kernelTime;
	mPreUserTime = userTime;
   if (kernel + user <= 0 || idle < 0) {
       return 0;
   }
#endif
	return cpu_occupancy_rate;
}

static std::string GetExePath() {
   std::string path;
#if defined _WIN32 || defined WIN32
   char szFilePath[MAX_PATH + 1] = { 0 };
   GetModuleFileNameA(NULL, szFilePath, MAX_PATH);
   (strrchr(szFilePath, '\\'))[0] = 0; // remove file name from the path  
   path = szFilePath;
#endif
   return path;
}

int MediaQualityAssess::Init(LivePushParam * param)
{
   mIsInit = false;
   mIsVideoWorking = false;
   if (param == nullptr) {
      LOGE("[Meida QA]LivePushParam is null at init!");
      return -1;
   }
   if (param->is_assess_video_quality == false) {
      LOGI("[Meida QA]Video quality assess is close!");
      return 0;
   }
   mVideoParam.pixFormat = "YUV420P";

   mOriginalWidth = param->width;
   mOriginalHeight = param->height;

   if (param->fast_assess_mode == 1) {
      if (mOriginalWidth * mOriginalHeight >= 1280 * 720) {
         mVideoParam.width = mOriginalWidth;
         mVideoParam.height = mOriginalHeight / 2;
         mIsScaleDown = 1;
      }
      else {
         mVideoParam.width = mOriginalWidth;
         mVideoParam.height = mOriginalHeight;
         mIsScaleDown = 0;
      }
   }
   else {
      mVideoParam.width = mOriginalWidth;
      mVideoParam.height = mOriginalHeight;
      mIsScaleDown = 0;
   }
   
   mVideoParam.nThreads = 0;
   mVideoParam.nSubsample = param->frame_rate;
   if (mVideoParam.width * mVideoParam.height >= 1280 * 720) {
      mVideoParam.nSubsample *= 3;
   }else if (mVideoParam.width * mVideoParam.height >= 960 * 540) {
      mVideoParam.nSubsample *= 2;
   }
   mVideoParam.enableConfInterval = 0;
   mVideoParam.phoneModel = 0;
   mVideoParam.error = 0;
   mVideoParam.vmafScore = -1;
   mVideoParam.execFPS = -1;
   mVideoParam.scoreInterval = param->score_frames_period;

   mModelPath = GetExePath();
   if (mModelPath.empty()) {
      mModelPath = "vmaf_v0.6.1.pkl";
   }
   else {
      mModelPath += "/vmaf_v0.6.1.pkl";
   }
   mVideoParam.modelPath = (char *)mModelPath.c_str();

   mOriginalSubsampleNum = mVideoParam.nSubsample;
   mIsNewVideoScore = false;
   mIsNextRead = true;
   mVideoScore = -1.0;
   mIsVideoBanned = false;
   mIsVideoWorking = false;
   mIsAudioWorking = false;
   mErrorCode = 0;

   if (mVideoParam.width * mVideoParam.height > 1366 * 768) {
      LOGE("[Meida QA]Video size is too large to assess quality in real time!");
      mIsVideoBanned = true;
   }
   
   if (mVideoAssessInst != nullptr) {
      DestroyVQInstance(mVideoAssessInst);
      mVideoAssessInst = nullptr;
   }
   mVideoAssessInst = CreateVQInstance();
   if (mVideoAssessInst == nullptr) {
      LOGE("[Meida QA]Create VQA instance failed!");
      return -1;
   }
   if (mVideoAssessInst->Init(&mVideoParam)) {
      LOGE("[Meida QA]VQA instance init failed!");
      return -1;
   }
   if (mRefVideoBuffer == nullptr) {
      mRefVideoBuffer = new FrameBuffer;
   }
   if (mDifVideoBuffer == nullptr) {
      mDifVideoBuffer = new FrameBuffer;
   }
   VHALL_DEL(mRefVideoBuffer);
   mRefVideoBuffer = new FrameBuffer;
   VHALL_DEL(mDifVideoBuffer);
   mDifVideoBuffer = new FrameBuffer;
   VHALL_DEL_ARRAY(mRefNextFrame);
   mRefNextFrame = new char[mVideoParam.width * mVideoParam.height * 1.5];
   VHALL_DEL_ARRAY(mDifNextFrame);
   mDifNextFrame = new char[mVideoParam.width * mVideoParam.height * 1.5];
   VHALL_DEL_ARRAY(mAssessArea);
   mAssessArea = new char[mVideoParam.width * mVideoParam.height * 1.5];
   if (mRefVideoBuffer == nullptr || mDifVideoBuffer == nullptr) {
      LOGE("[Meida QA]Video buffer create failed!");
      return -1;
   }
   if (!mRefVideoBuffer->Init(mVideoParam.width, mVideoParam.height) || !mDifVideoBuffer->Init(mVideoParam.width, mVideoParam.height)) {
      LOGE("[Meida QA]Video buffer init failed!");
      return -1;
   }

   mCpuDetectCnt = 0;
   mCpuBusyCnt = 0;
   mCpuUsage = 0;
   mNeedRecover = 0;
   
   mIsInit = true;
   mVideoAssessInst->SetListener((VideoQualityListener *)this);
   return 0;
}

bool MediaQualityAssess::StartVideoAssess()
{
   int ret = 0;
   if (mIsInit && !mIsVideoBanned && mVideoAssessInst) {
      ret = mVideoAssessInst->Start();
#if defined _WIN32 || defined WIN32
      GetSystemTimes(&mPreIdleTime, &mPreKernelTime, &mPreUserTime);
#endif //_WIN32
      mIsVideoWorking = true;
   }
   else {
      ret = -1;
   }
   if (ret == -1) {
      LOGE("[Meida QA]Vidiao assessment starts without initialization or is banned!");
      return false;
   }
   else if (ret == -2) {
      LOGE("[Meida QA]Vidiao assessment starts without internal frame create!");
      return false;
   }
   /*if (infp) {
      fclose(infp);
      infp = nullptr;
   }
   infp = fopen("In.yuv","wb");
   if (outfp) {
      fclose(outfp);
      outfp = nullptr;
   }
   outfp = fopen("Out.yuv", "wb");*/
   return true;
}

bool MediaQualityAssess::StopVideoAssess()
{
   if (mIsVideoWorking) {
      if (mVideoAssessInst) {
         mVideoAssessInst->Stop(1);
      }
      else {
         LOGE("[Meida QA]mVideoAssessInst is null when stop!");
      }
      mIsVideoWorking = false;
      mRefVideoBuffer->Clear();
      mDifVideoBuffer->Clear();
      mCpuDetectCnt = 0;
      mCpuBusyCnt = 0;
      mCpuUsage = 0;
      mNeedRecover = 0;
   }
   /*if (infp) {
      fclose(infp);
      infp = nullptr;
   }
   if (outfp) {
      fclose(outfp);
      outfp = nullptr;
   }*/
   return true;
}

bool MediaQualityAssess::InputVideoData(char * ref, char * dif)
{
   int ret = 0;
   if (mIsVideoWorking && !mIsVideoBanned) {
      if (mErrorCode == 1) {
         StopVideoAssess();
         mNeedRecover = 1;
         mErrorCode = 0;
         return false;
      }
      //check CPU status
      mCpuDetectCnt++;
      if (mCpuDetectCnt >= mVideoParam.nSubsample || mCpuDetectCnt < 0) {
         mCpuDetectCnt = 0;
         //TODO: Have a problem about CPU detect
#if defined _WIN32 || defined WIN32
         int cpuOccupancy = CpuUsageRateDetection();
         if (cpuOccupancy > 0 && cpuOccupancy <= 100) {
            mCpuUsage = cpuOccupancy;
            LOGI("CPU occupancy is %d", mCpuUsage);
         }else{
            mCpuUsage = 0;
            LOGW("CPU occupancy is unavailable");
         }
         //printf("CPU occupancy is %d\n", mCpuUsage);
         if (mCpuUsage >= 95 ) {
            LOGW("CPU occupancy is over 95%!!");
            mCpuBusyCnt += 5;
         }
         else if (mCpuUsage >= 80 && mCpuUsage < 95) {
			 LOGW("CPU occupancy is over 80%!!");
            mCpuBusyCnt ++;
         }
         else if (mCpuUsage <= 50 && mCpuUsage > 30) {
            mCpuBusyCnt --;
         }
         else if (mCpuUsage <= 30 && mCpuUsage > 0) {
            mCpuBusyCnt -= 5;
         }
         if (mCpuBusyCnt > 60) {
            StopVideoAssess();
            mErrorCode = 0;
            mNeedRecover = 1;
            LOGW("[MediaQuality]CPU occupancy is in a high level, video quality assessment shut down and check period is %d ...", mVideoParam.nSubsample);
            return false;
         }
         else if (mCpuBusyCnt < 0) {
            mCpuBusyCnt = 0;
         }

#else // _WIN32
		 //TODO: Need a CPU detect strategy in Linux
#endif 
      }
      //NOTICE:if ref or dif pointed to a null address, video quality assessment would be ended.
      if (mVideoAssessInst) {
         ret = mVideoAssessInst->InputFrame(ref, dif);
      }
      else {
         LOGW("[MediaQuality]mVideoAssessInst is null when InputVideoData!");
      }
   }
   if (ret == -1) {
      return false;
   }
   mIsNextRead = true;
   return true;
}

double MediaQualityAssess::GetVideoQuality()
{
   return mVideoScore;
}

bool MediaQualityAssess::StartAudioAssess()
{
   LOGE("[Meida QA]Audio quality assessment is not supported now");
   return false;
}

bool MediaQualityAssess::StopAudioAssess()
{
   LOGE("[Meida QA]Audio quality assessment is not supported now");
   return false;
}

bool MediaQualityAssess::InputAudioData(char * ref, char * dif)
{
   LOGE("[Meida QA]Audio quality assessment is not supported now");
   return false;
}

double MediaQualityAssess::GetAudioQuality()
{
   LOGE("[Meida QA]Audio quality assessment is not supported now");
   return -1;
}
//int cnt1 = 0,cnt2 = 0;
double MediaQualityAssess::OnVideoDataInteract(const char * orignal_data, int size0, const char * decode_data, int size1)
{
   if (mNeedRecover) {
      mNeedRecover = 0;
      return -3.0;
   }
   if (!mIsInit || !mIsVideoWorking || mIsVideoBanned) {
      return -1.0;
   }
   //buffer original YUV data
   if (size0 != 0 && size0 != mOriginalWidth * mOriginalHeight * 3 / 2) {
      LOGE("[Meida QA]Input ref YUV size is error!");
      return -2.0;
   }
   else if (size0 != 0) {
      char * dst = mAssessArea;
      char * src = (char *)orignal_data;
      //NOTICE: we just copy chroma data, it is Ok for vmaf
      for (int h = 0; h < mVideoParam.height; h++) {
         memcpy(dst, src, mVideoParam.width);
         dst += mVideoParam.width;
         src += mOriginalWidth;
         if (mIsScaleDown) {
            src += mOriginalWidth;
         }
      }
      mRefVideoBuffer->AddFrame(mAssessArea);
      //fwrite(mAssessArea, 1, mVideoParam.width * mVideoParam.height * 3 / 2, infp);
      //printf("ref cnt is %d, dif cnt is %d\n", cnt1, cnt2);
      //cnt1++;
   }
   //buffer compared YUV data
   if (size1 != 0 && size1 != mOriginalWidth * mOriginalHeight * 3 / 2) {
      LOGE("[Meida QA]Input dif YUV size is error!");
      return -2.0;
   }
   else if (size1 != 0) {
      char * dst = mAssessArea;
      char * src = (char *)decode_data;
      //NOTICE: we just copy chroma data, it is Ok for vmaf
      for (int h = 0; h < mVideoParam.height; h++) {
         memcpy(dst, src, mVideoParam.width);
         dst += mVideoParam.width;
         src += mOriginalWidth;
         if (mIsScaleDown) {
            src += mOriginalWidth;
         }
      }
      mDifVideoBuffer->AddFrame(mAssessArea);
      //fwrite(mAssessArea, 1, mVideoParam.width * mVideoParam.height * 3 / 2, outfp);
      //printf("ref cnt is %d, dif cnt is %d\n", cnt1, cnt2);
      //cnt2++;
   }
   //check if ready to quality assess
   while (mRefVideoBuffer->GetFrameNum() > 0 && mDifVideoBuffer->GetFrameNum() > 0) {
      if (!mRefVideoBuffer->OutFrame(mRefNextFrame) && !mDifVideoBuffer->OutFrame(mDifNextFrame)) {
         mIsNextRead = false;
         InputVideoData(mRefNextFrame, mDifNextFrame);
      }
   }
   if (mIsNewVideoScore) {
      mIsNewVideoScore = false;
      return mVideoScore;
   }
   return -1.0;
}

int MediaQualityAssess::OnConfigAdjust(int type, void * param)
{
   if (!mIsInit) {
      return 0;
   }
   switch (type)
   {
   case MQA_VIDEO_ENCODE_BUSY: {
      mVideoParam.nSubsample += mOriginalSubsampleNum;
      if (mVideoParam.nSubsample <= mVideoParam.scoreInterval && mVideoAssessInst) {
         mVideoAssessInst->Reconfig(&mVideoParam);
         LOGW("[MediaQA]Encoder is busy, enlarge the interval.");
         //printf("too busy!\n");
      }
      else if (mVideoAssessInst == nullptr) {
         LOGW("[MediaQA]VideoAssess is null, encode busy is caused by other factor.");
         return -1;
      }
      else {
         LOGW("[MediaQA]Encoder too busy to keep video quality assess running!VAQ will be stopped.");
         StopVideoAssess();
         mNeedRecover = 1;
      }
   }
      break;
   case MQA_VIDEO_ENCODE_OK: {
      ;//do nothing to avoid a tidal cycle
   }
      break;
   default:
      break;
   }
   return 0;
}

void MediaQualityAssess::NotifyEvent(VQEventsType type, void * param)
{
   if (type == VQ_TYPE_RESULT) {
      mIsNewVideoScore = false;
      mVideoScore = *(double *)param;
      if (mVideoParam.width * mVideoParam.height < 1280 * 720 && mVideoParam.width * mVideoParam.height >= 640 * 720 && mVideoScore > 50) {
         mVideoScore = mVideoScore - 3;
      }
      else if(mVideoScore > 50 && mVideoParam.width * mVideoParam.height < 640 * 720) {
         mVideoScore = mVideoScore - 5;
      }
      if (mVideoScore >= 0 && mVideoScore <= 100) {
         mIsNewVideoScore = true;
         LOGW("[MediaQualtiy]VMAF have a score : %f ",mVideoScore);
      }
      else {
         mVideoScore = -1;
      }
   }
   else if (type == VQ_TYPE_ERROR) {
      char *err = (char *)param;
      LOGE("[MediaQualtiy]VMAF have error, further info:\n %s",param);
      printf("[MediaQualtiy]VMAF have error, further info:\n %s\n", param);
      mErrorCode = 1;
   }
}

bool MediaQualityAssess::isInit()
{
   return mIsInit;
}

bool MediaQualityAssess::Destroy()
{
   if (mVideoAssessInst != nullptr) {
      mVideoAssessInst->Uninit();
      DestroyVQInstance(mVideoAssessInst);
   }
   VHALL_DEL(mRefVideoBuffer);
   VHALL_DEL(mDifVideoBuffer);
   VHALL_DEL_ARRAY(mRefNextFrame);
   VHALL_DEL_ARRAY(mDifNextFrame);
   VHALL_DEL_ARRAY(mAssessArea);
   mIsInit = false;
   return true;
}

FrameBuffer::FrameBuffer() {
	mFrameBuffer.clear();
}

FrameBuffer::~FrameBuffer() {

}

bool FrameBuffer::Init(int w, int h) {
	std::lock_guard<std::mutex> lock(mMutex);
	mWidth = w;
	mHeight = h;
	mFrameBytes = w * h * 3 / 2;
	mInited = true;
	return mInited;
}

bool FrameBuffer::Clear() {
	std::lock_guard<std::mutex> lock(mMutex);
	mFrameBuffer.clear();
	return true;
}

int FrameBuffer::AddFrame(const char *newFrame) {
	std::lock_guard<std::mutex> lock(mMutex);
	if (newFrame == nullptr || mInited == false) {
		return -1;
	}
	if ((int)mFrameBuffer.size() / mFrameBytes >= MAX_FRAME_NUM) {
		char* position = (char*)&mFrameBuffer.at(mFrameBuffer.size() - mFrameBytes);
		memcpy(position, newFrame, mFrameBytes);
	}
	else {
		mFrameBuffer.insert(mFrameBuffer.end(), (char*)newFrame, (char*)newFrame + mFrameBytes);
	}
	return 0;
}

int FrameBuffer::OutFrame(char *dst) {
	std::lock_guard<std::mutex> lock(mMutex);
	if (dst == nullptr) {
		return -2;
	}
	if (mFrameBuffer.size() <= 0 || mFrameBuffer.size() < mFrameBytes) {
		return -1;
	}
	char * dataPtr = (char*)&mFrameBuffer.at(0);
	memcpy(dst, dataPtr, mFrameBytes);
	mFrameBuffer.erase(mFrameBuffer.begin(), mFrameBuffer.begin() + mFrameBytes);
	return 0;
}

bool FrameBuffer::Uninit() {
	std::lock_guard<std::mutex> lock(mMutex);
	mFrameBuffer.clear();
	mInited = false;
	return true;
}

bool FrameBuffer::IsInited() {
	std::lock_guard<std::mutex> lock(mMutex);
	return mInited;
}

int FrameBuffer::GetFrameNum() {
	std::lock_guard<std::mutex> lock(mMutex);
	return (int)mFrameBuffer.size() / mFrameBytes;
}

int FrameBuffer::RemoveFrame(int nFrames) {
	std::lock_guard<std::mutex> lock(mMutex);
	if (nFrames < 0) {
		return -1;
	}
	mFrameBuffer.erase(mFrameBuffer.begin() + mFrameBytes * nFrames, mFrameBuffer.begin() + mFrameBytes);
	return (int)mFrameBuffer.size() / mFrameBytes;
}

char* FrameBuffer::GetFramePtr() {
	std::lock_guard<std::mutex> lock(mMutex);
	char * dataPtr = (char*)&mFrameBuffer.at(0);
	return dataPtr;
}

