#ifndef __MEDIA_READER_INCLUDE_H_
#define __MEDIA_READER_INCLUDE_H_
#include "IMediaReader.h"
#include "vlc.h"
#include "WaveOutPlay.h"
#include <mutex>
#include <string>
#include <vector>
#include <atomic>

enum  MediaEntryType {
   MediaEntry_File = 0,
   MediaEntry_Url
};

class MediaEntry {
public:
   std::string mMediaFile;  //must utf8
   MediaEntryType mMediaEntryType;
   void* mMediaItem; // libvlc_media_t
};

class BufferQueue;
class MediaReader :public IMediaOutput, public IMediaReader {
public:
   MediaReader(void);
   virtual ~MediaReader();
   virtual void SetVideoNotify(IVideoNotify* videoNotify);
   virtual bool GetNextAudioBuffer(void **buffer, unsigned int *numFrames, uint64_t *timestamp);
   virtual bool GetNextVideoBuffer(void *buffer, unsigned long long bufferSize, uint64_t *timestamp);
   virtual bool GetAudioParam(unsigned int& channels, unsigned int& samplesPerSec, unsigned int& bitsPerSample);
   virtual bool GetVideoParam(unsigned long& videoWidth, unsigned long& videoHeightt, long long & frameDuration, long long &timeScale); // default pixfmt
   virtual int  GetPlayerState();   
   virtual void SetCurrentBaseTime(unsigned long long baseTime);
   void UIEventCallBack(int);
   virtual void ResetPlayFileAudioData();
public:
   virtual void SetVolume(const unsigned int & volume);
   virtual unsigned int GetVolume();   
   virtual void SetEventCallBack(IMediaEventCallBack cb,void *param);
   //------------------------------新增加的接口------------------------------
   virtual bool VhallPlay(char *,bool audioFile);
   virtual void VhallPause();
   virtual void VhallResume();
   virtual void VhallStop(); 
   virtual int SetPlayOutAudio(bool enable);
   virtual void VhallSeek(const unsigned long long& seekTimeInMs);
   virtual const long long VhallGetMaxDulation();
   virtual const long long VhallGetCurrentDulation();
   virtual void VhallSetEventCallBack(IMediaEventCallBack cb,void *param);
   virtual void VhallSetVolumn(unsigned int);
   virtual int VhallGetVolumn();
   virtual void VhallVolumnMute();
   //------------------------------------------------------------------------
   virtual IMediaOutput* GetMediaOut();   
   virtual void Refit();
   virtual void SetRefit(IMediaOutputRefitCallBack refitCallBack,void *param);
private:
   bool init();
   const long long GetCurrentVLCPlayTime();
   void ResetVideoPool();
private:
   bool                       mVlcIsInit;
   bool                       mEnableAudio;
   bool                       mEnableVideo;
   bool                       mIsInit;
   bool                       mHasVideo;
   bool  mbAudioFile = false;
   bool  mIsRefit;
   v_mutex_t mIsRefitMutex;
   IMediaOutputRefitCallBack func_RefitCallBack;
   void *mRefitParam = nullptr;
   bool mIsActive;  //output yum pcm
   std::vector<MediaEntry*> mMediaConfigs;
   unsigned int mAudioVolume;
   bool mAudioVolumeSet;
   bool mIsOutputDevice;
   std::string mOutputDevice; //utf8
   std::mutex   mNotifyMutex;
   IVideoNotify* mVideoNotify = nullptr;
   BufferQueue* mAudioBufferQueue = nullptr;
   //BufferQueue* mVideoBufferQueue = nullptr;
   std::list<DataUnit*> mVideoBufferQueue;
   std::list<DataUnit*> mVideoDataPool;
   std::atomic_llong mVideoCaptureStartTime = 0;
   std::atomic_llong mLastMixedVLCAudioTime = 0;
   std::atomic_llong mLastAudioTime = 0;
   std::atomic_llong mLastMixedBaseTime = 0;
   std::atomic_bool mbIsStop = false;
   unsigned char *mVideoBuffer=NULL;   
   v_mutex_t   mMutex;
   v_mutex_t   mMutexAudio;
   std::atomic_bool mbIsResume = false;
   std::atomic_bool mbIsEnableGetVideo = false;
   int mRecvAudioCount = 0;
   std::vector<char> mAudioBuffer;
  
   unsigned long mVideoWidth;
   unsigned long mVideoHeight;
   long mVideoFrameSize;

   unsigned int            mChannels;
   unsigned int            mSampleRate;
   unsigned int            mBitPerSample; //bits per sample 
   unsigned int            mAudioBytesPreS;  //   

   uint64_t                mBaseStartTS;

   FILE* mVlcLog = nullptr;

   //libvlc
   libvlc_instance_t *mVlc = nullptr;
   libvlc_media_list_player_t *mMediaListPlayer = nullptr;
   libvlc_media_list_t *mMediaList = nullptr;
   libvlc_media_player_t *mMediaPlayer = nullptr;
   v_mutex_t mMediaPlayerMutex;
   libvlc_log_t* mMediaLog = nullptr;
   friend void vlcEvent(const libvlc_event_t *e, void *data);
   IMediaEventCallBack eventCallBack;
   void *eventCallBackParam = nullptr;
   v_mutex_t   eventCallBackMutex;
   HANDLE mThreadHandle;
   WaveOutPlay* mpWaveOutPlay = NULL;
   int64_t mPtsCount = 0;
private:
   //libvlc
   
   void MediaPlayerStop(libvlc_media_player_t *mPlayer);
   static void vlcLog(void *data, int level, const libvlc_log_t *ctx,
                      const char *fmt, va_list args);
   static void vlcEvent(const libvlc_event_t *e, void *data);
   static void *videoLock(void *data, void **pixelData);
   static void videoUnlock(void *data, void *id, void *const *pixelData);
   static void display(void *data, void *id);
   static unsigned VideoSetupFormatCallback(
      void **data,
      char *chroma,
      unsigned *width,
      unsigned *height,
      unsigned *pitches,
      unsigned *lines);
   static void VideoSetupFormatCleanup(void *data);
   static int audioSetupCallbackProxy(void **data, char *format, unsigned *rate, unsigned *channels);
   static void audioCleanupCallbackProxy(void *data);
   static void audioPlayCallbackProxy(void *data, const void *samples, unsigned count, int64_t pts);

   void *videoLock(void **pixelData);
   void videoUnlock(void *id, void *const *pixelData);
   unsigned int VideoFormatCallback(char *chroma, unsigned *width, unsigned *height, unsigned *pitches, unsigned *lines);
   void VideoFormatCleanup();

   int AudioSetupCallback(char *format, unsigned *rate, unsigned *channels);
   void AudioCleanupCallback();
   void AudioPlayCallback(const void *samples, unsigned count, int64_t pts);


   DataUnit* GetUnUsedDataUinit();
};


#endif
