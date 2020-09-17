#ifndef __LIVE_RTMP_PUBLISH_H__
#define __LIVE_RTMP_PUBLISH_H__

#include "talk/base/messagehandler.h"
#include "../common/safe_buffer_data.h"
#include "media_muxer_interface.h"
#include "../encoder/media_data_send.h"
#include "muxer_interface.h"
#include "../common/live_sys.h"
#include <map>

namespace talk_base {
   class Thread;
}
namespace VHJson {
   class Value;
}
class LiveInterface;
struct LivePushParam;
class RateControl;

NS_VH_BEGIN

class MediaMuxer:public talk_base::MessageHandler,public SafeDataQueueStateListener,public MediaMuxerInterface,public MediaDataSend,public MuxerListener
{
public:
   MediaMuxer();
   ~MediaMuxer();
   // set param
   virtual int LiveSetParam(LivePushParam *param);
   // set status Listener
   virtual void SetStatusListener(LiveStatusListener * listener);
   // add a muxer return muxerId
   virtual int  AddMuxer(VHMuxerType type,void *param);
   // remove a muxer with muxerId
   virtual void RemoveMuxer(int muxer_id);
   // remove all muxer
   virtual void RemoveAllMuxer();
   // start
   virtual void StartMuxer(int muxer_id);
   // stop
   virtual void StopMuxer(int muxer_id);
   virtual void StopAllMuxer();
   // get muxerStatus with muxerId
   virtual int GetMuxerStatus(int muxer_id);
   
   virtual const VHMuxerType GetMuxerType(int muxer_id);
   
   virtual int GetMuxerStartCount();
   virtual int GetPushMuxerStartCount();
   virtual int GetMuxerCount();
   virtual int GetDumpSpeed(int muxer_id);
    // MediaDataSend
   virtual void OnSendVideoData(const char * data, int size, int type, uint64_t timestamp);
   virtual void OnSendAudioData(const char * data, int size, int type, uint64_t timestamp);
   virtual void OnSendAmf0Msg(const char * data, int size, int type, uint64_t timestamp);
    // MuxerListener
   virtual int  OnMuxerEvent(int type, MuxerEventParam* param);
    // BufferCallback
   virtual void OnSafeDataQueueChange(SafeDataQueueState state, std::string tag);

   virtual bool LiveGetRealTimeStatus(VHJson::Value& value);

   virtual void SetRateControl(RateControl *rateControl);
private:
   enum {
      MSG_RTMP_SYNC_DATA,
      MSG_RTMP_CLEAR_SYNC_DATA,
      MSG_RTMP_REMOVE_MUXER
   };
   void OnInit();
   void OnDestory();
   void OnMessage(talk_base::Message* msg);
   void OnConnect(const std::string& url);
   void OnSyncData();
   void OnSendOnlyAudio();
   void OnSendOnlyVideo();
   void OnSendAll();
   void SetMediaHeader(SafeData**header,SafeData**item);
   void PushData2Muxer(SafeData*header,SafeData*item);
   void OnGetUploadSpeed();

private:
   
   talk_base::Thread   *mSyncThread;
   
   SafeDataQueue       *mAudioQueue;
   SafeDataQueue       *mVideoQueue;
   SafeDataQueue       *mAmf0MsgQueue;
   
   SafeDataPool        *mDataPool;
   
   SafeData            *mBufferItem;
   SafeData            *mAudioItem;
   SafeData            *mVideoItem;
   SafeData            *mVideoHeader;
   SafeData            *mAudioHeader;

   vhall_lock_t        mMutex;
   RateControl         *mRateControl;
   LivePushParam       *mParam;
   LiveStatusListener  *mStatueListener;
   std::map<int,MuxerInterface*> mMuxers;
   volatile bool       mIsStart;
   std::list<SafeData*> mAmf0MsgUnsentlist;
};

NS_VH_END

#endif
