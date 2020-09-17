#ifndef _VHALL_PLAYER_H_
#define _VHALL_PLAYER_H_

#include <iostream>
#include "../common/live_open_define.h"
#include "../common/live_obs.h"

class RtmpReader;
class MediaDecode;
class MediaRender;
class DemuxerInterface;

class VhallPlayerInterface {
   
public:
   VhallPlayerInterface(){};
   virtual ~VhallPlayerInterface(){};
   virtual void NotifyEvent(const int type, const EventParam &param) = 0;
   virtual int NotifyVideoData(const char *data, int size, int w, int h) = 0;
   virtual int NotifyAudioData(const char *data, int size) = 0;
   virtual int NotifyJNIDetachVideoThread() = 0;
   virtual int NotifyJNIDetachAudioThread() = 0;
   virtual int OnHWDecodeVideo(const char *data, int size, int w, int h, int64_t ts) = 0;
   virtual LivePlayerParam * GetParam() const = 0;
   virtual void SetPlayerBufferTime(int time) = 0;
   virtual DecodedVideoInfo * GetHWDecodeVideo() = 0;
};

class VHallLivePlayer:public VhallPlayerInterface{
public:
   VHallLivePlayer();
   ~VHallLivePlayer();
   void AddObs(LiveObs * obs);
   int  LiveSetParam(LivePlayerParam *param);
   void SetDemuxer(VHMuxerType muxer_type);
   void SetBufferTime(const int& bufferTime);
   bool Start(const char *url);
   void Stop();
   int GetPlayerBufferTime();
private:
   virtual void NotifyEvent(const int type, const EventParam &param);
   virtual int NotifyVideoData(const char * data, int size, int w, int h);
   virtual int NotifyAudioData(const char * data, int size);
   virtual int NotifyJNIDetachVideoThread();
   virtual int NotifyJNIDetachAudioThread();
   virtual int OnHWDecodeVideo(const char *data, int size, int w, int h, int64_t ts);
   virtual LivePlayerParam * GetParam() const;
   virtual void SetPlayerBufferTime(int time);
   virtual DecodedVideoInfo * GetHWDecodeVideo();
   void Init();
   void Destory();
private:
   bool                 mStart;
   DemuxerInterface*    mRtmpReader;
   MediaDecode*         mMediaDecode;
   MediaRender*         mMediaRender;
   bool                 mPlayerInit;
   LivePlayerParam      *mParam;
   LiveObs              *mObs;
   VHMuxerType          mPreMuxerType;
   volatile int         mPlayerBufferTime;//观看端缓冲队列的实际时间长度

   bool                 mIsFirstVideoRender;
   uint64_t             mStartPlayTimestamp;
};

#endif
