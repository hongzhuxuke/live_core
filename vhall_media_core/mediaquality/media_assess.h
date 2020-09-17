#ifndef MEDIA_ASSESS_H
#define MEDIA_ASSESS_H

#include <stdlib.h>
#include <stdint.h>
#include <atomic>
#include <chrono>
#include <thread>
#include <mutex>
#include <vector>
#include "media_qa_interface.h"

#if defined _WIN32 || defined WIN32
//#include <minwindef.h>
//#include <Pdh.h>
//#include <PdhMsg.h>
//#pragma comment(lib,"pdh.lib")
#endif

class FrameBuffer;
class VideoQualityInterface;
struct LivePushParam;

class MediaQualityAssess : public MediaQualityInterface
{
public:
   MediaQualityAssess() 
   {
      mIsInit = false;
      mIsVideoBanned = true;
      mRefVideoBuffer = nullptr;
      mDifVideoBuffer = nullptr;
      mVideoAssessInst = nullptr;
      mRefNextFrame = nullptr;
      mDifNextFrame = nullptr;
      mAssessArea = nullptr;
      mIsVideoWorking = false;
      mIsAudioWorking = false;
   };
   ~MediaQualityAssess() 
   {
      this->Destroy();
   };
   bool Destroy();

   //MediaQualityInterface
   virtual int Init(LivePushParam *param);
   virtual bool StartVideoAssess();
   virtual bool StopVideoAssess();
   virtual bool InputVideoData(char *ref, char *dif);
   virtual double GetVideoQuality();
   virtual bool StartAudioAssess();
   virtual bool StopAudioAssess();
   virtual bool InputAudioData(char *ref, char *dif);
   virtual double GetAudioQuality();
   virtual bool isInit();

   //MQADataInteract
   virtual double OnVideoDataInteract(const char* orignal_data, int size0, const char* decode_data, int size1);
   virtual int OnConfigAdjust(int type, void *param = nullptr);

   //VideoQualityListener
   virtual void NotifyEvent(VQEventsType type, void *param);

private:
   bool mIsInit;
   bool mIsVideoBanned;
   int mOriginalSubsampleNum;
   int mOriginalWidth;
   int mOriginalHeight;
   int mIsScaleDown;
   double mVideoScore;
   std::atomic_bool mIsNewVideoScore;
   std::string mModelPath;
   FrameBuffer *mRefVideoBuffer;
   FrameBuffer *mDifVideoBuffer;
   char * mRefNextFrame;
   char * mDifNextFrame;
   char * mAssessArea;
   bool mIsNextRead;
   bool mIsVideoWorking;
   bool mIsAudioWorking;
   int  mErrorCode;
   int  mCpuDetectCnt;
   int  mCpuBusyCnt;
   int  mCpuUsage;
   int  mNeedRecover;
   VQParam mVideoParam;
   VideoQualityInterface *mVideoAssessInst;
};

class FrameBuffer {

public:
	FrameBuffer();
	~FrameBuffer();
	bool Init(int w, int h);
	bool Clear();
	int AddFrame(const char* newFrame);
	int OutFrame(char*dst);
	bool Uninit();
	bool IsInited();
	int GetFrameNum();
	int RemoveFrame(int nFrames);
	char *GetFramePtr();
private:

	int mInited;
	int mWidth;
	int mHeight;
	int mFrameBytes;
	std::mutex mMutex;
	std::vector<char> mFrameBuffer;
};


#endif /* MEDIA_QA_INTERFACE_H */