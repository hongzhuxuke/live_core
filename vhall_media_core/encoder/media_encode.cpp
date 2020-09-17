#include "media_encode.h"
#include "../utility/utility.h"
#include "../common/vhall_log.h"
#include "../common/live_message.h"
#include "aac_encoder.h"
#include "../utility/utility.h"
#include "media_data_send.h"
#include "../common/live_status_listener.h"
#include "../common/safe_buffer_data.h"
#include "../ratecontrol/rate_control.h"

#ifdef VIDEO_ENCODE_X264
#include "x264_encoder.h"
#else
#include "h264_encoder.h"
#endif

#include "../3rdparty/json/json.h"

#define  MAX_AV_DIFF_TS                        500 //音视频的最大时间差值，单位MS
#define  VIDEO_QUEUE_WARN_RATE                 0.7
#define  VIDEO_QUEUE_WARN_RECOVER_RATE         0.3


MediaEncode::MediaEncode():
mVideoEncodedData(NULL),
mAudioEncodedData(NULL),
mH264Encoder(NULL),
mAacEncoder(NULL),
mVideoWorkThread(NULL),
mParam(NULL),
mStatusListener(NULL),
mOutputListener(NULL),
mMediaQADataInOut(NULL),
mAudioWorkThread(NULL),
mAudioPool(NULL),
mVideoPool(NULL),
mVideoEncParam(NULL),
mIsFirstRCBitrateDown(false)
{
   mVideoWorkThread = new talk_base::Thread();
   mVideoWorkThread->SetName("mVideoWorkThread", this);
   if (mVideoWorkThread==NULL) {
      LOGE("m_work_thread is NULL!");
   }
   mAudioWorkThread = new talk_base::Thread();
   mAudioWorkThread->SetName("mAudioWorkThread", this);
   if (mAudioWorkThread == NULL) {
      LOGE("mAudioWorkThread is NULL!");
   }
   mFrameRate = 15;
   mVideoPool = new SafeDataPool(30);
   mAudioPool = new SafeDataPool(30);
   mIsVideoInited = false;
   mIsAudioInited = false;
   mIsEncodeBusy = false;
   mVideoLastPostTS = 0;
}

MediaEncode::~MediaEncode(){
   VHALL_THREAD_DEL(mAudioWorkThread);
   VHALL_THREAD_DEL(mVideoWorkThread);
   VHALL_DEL(mH264Encoder);
   VHALL_DEL(mAacEncoder);
   VHALL_DEL(mVideoEncodedData);
   VHALL_DEL(mAudioEncodedData);
   VHALL_DEL(mVideoPool);
   VHALL_DEL(mAudioPool);
}

int MediaEncode::LiveSetParam(LivePushParam *param){
   if (param!=NULL) {
      mParam = param;
      VHALL_DEL(mAudioEncodedData);
      VHALL_DEL(mVideoEncodedData);
      int video_frame_size = param->width*param->height*3/2;
      mVideoEncodedData = (char*)calloc(1, video_frame_size);
      if (mVideoEncodedData==NULL) {
         LOGE("m_encoded_data calloc error!");
      }
      int audio_frame_size =  PCM_FRAME_SIZE*param->ch_num*Utility::GetBitNumWithSampleFormat(param->src_sample_fmt)/8;
      mAudioEncodedData = (char*)calloc(1, audio_frame_size);
      if (mAudioEncodedData==NULL) {
         LOGE("m_encoded_data calloc error!");
      }
      mFrameRate = MAX(mParam->frame_rate, 10);
      mFrameRate = MIN(mFrameRate, 60);
      return 0;
   }
   return -1;
}

void MediaEncode::SetStatusListener(LiveStatusListener * listener){
   mStatusListener = listener;
}

void MediaEncode::SetOutputListener(MediaDataSend * output_listener){
   mOutputListener = output_listener;
}

void MediaEncode::SetMediaQAListener(MQADataInteract * listener) {
	mMediaQADataInOut = listener;
}

void MediaEncode::SetVideoQualityGrade(double grade) {
   if (mIsVideoInited) {
      mH264Encoder->SetVideoQuality(grade);
   }
}

bool MediaEncode::RateControlAdjust(NetworkState state){
   if (mIsVideoInited == false) {
      return false;
   }

   if(mH264Encoder){
      mH264Encoder->RateControlAdjust(state);
   }
   if(mIsFirstRCBitrateDown == false
      && (state == NetworkState_Overuse || state == NetworkState_Block)){
      mIsFirstRCBitrateDown = true;
      if(mStatusListener){
         EventParam param;
         param.mDesc = "bitrate first down.";
         mStatusListener->NotifyEvent(RC_BITRATE_FIRST_DOWN, param);
      }
   }
   return true;
}

void MediaEncode::EncodeVideo(const char * data, int size, uint64_t timestamp, const LiveExtendParam *extendParam)
{
   if (!mVideoWorkThread->started()) {
      return;
   }
   int queueSize = (int)mVideoWorkThread->size();
   if (queueSize>mFrameRate*VIDEO_QUEUE_WARN_RATE) {
      if (!mIsEncodeBusy) {
         mIsEncodeBusy = true;
         EventParam param;
         param.mId = -1;
         param.mDesc = "video encode is busy";
         LOGW("%s",param.mDesc.c_str());
         if (mStatusListener) {
            mStatusListener->NotifyEvent(VIDEO_ENCODE_BUSY, param);
         }
         if (mMediaQADataInOut) {
            mMediaQADataInOut->OnConfigAdjust(mMediaQADataInOut->MQA_VIDEO_ENCODE_BUSY);
         }
         LOGW("video encode is busy");
      }
      LOGW("abandon video data queue size:%d.",queueSize);
   }else{
      if (queueSize<=mFrameRate*VIDEO_QUEUE_WARN_RECOVER_RATE) {
         if (mIsEncodeBusy) {
            EventParam param;
            param.mId = -1;
            param.mDesc = "video encode is busy revert";
            LOGW("%s",param.mDesc.c_str());
            if (mStatusListener) {
               mStatusListener->NotifyEvent(VIDEO_ENCODE_OK, param);
            }
            mIsEncodeBusy = false;
            LOGW("video encode is busy revert");
         }
      }
      SafeData *safeData = mVideoPool->GetSafeData((char*)data, size, 0, timestamp);
      if (safeData) {
         SafeDataMessageData *msg = new SafeDataMessageData(safeData);
         msg->SetExtendParam((LiveExtendParam *)extendParam);
         mVideoWorkThread->Post(this, MSG_RTMP_ENCODE_VIDEO, msg);
         mVideoLastPostTS = timestamp;
      }
   }
}

void MediaEncode::EncodeVideoHW(const char * data, int size ,int type, uint64_t timestamp)
{
   if (!mVideoWorkThread->started()) {
      return;
   }
   SafeData *safeData = mVideoPool->GetSafeData((char*)data, size, type, timestamp);
   if (safeData) {
      mVideoWorkThread->Post(this, MSG_RTMP_ENCODE_VIDEOHW, new SafeDataMessageData(safeData));
   }
}

void MediaEncode::EncodeAudio(const char * data, int size, uint64_t timestamp)
{
   if (!mAudioWorkThread->started()) {
      return;
   }
   int tsDif = (int)(timestamp-mVideoLastPostTS);
   if ((mParam&&mParam->live_model==LIVE_TYPE_AUDIO_ONLY)||tsDif<MAX_AV_DIFF_TS) {
      SafeData *safeData = mAudioPool->GetSafeData((char*)data, size, 0, timestamp);
      if (safeData) {
         mAudioWorkThread->Post(this, MSG_RTMP_ENCODE_AUDIO, new SafeDataMessageData(safeData));
      }
   }else{
      LOGW("abandon audio data dif ts:%d",tsDif);
   }
}

void MediaEncode::EncodeAudioHW(const char * data, int size, uint64_t timestamp)
{
   if (!mAudioWorkThread->started()) {
      return;
   }
   SafeData *safeData = mAudioPool->GetSafeData((char*)data, size, 0, timestamp);
   if (safeData) {
      mAudioWorkThread->Post(this, MSG_RTMP_ENCODE_AUDIOHW, new SafeDataMessageData(safeData));
   }
}

bool MediaEncode::RequestKeyframe(){
   if (mH264Encoder) {
      return mH264Encoder->RequestKeyframe();
   }
   return false;
}

void MediaEncode::Start()
{
   mIsEncodeBusy = false;
   mIsFirstRCBitrateDown = false;
   mVideoLastPostTS = 0;
   if (mParam->live_model==LIVE_TYPE_VIDEO_AND_AUDIO||mParam->live_model==LIVE_TYPE_VIDEO_ONLY) {
      if (!mVideoWorkThread->started()) {
         mVideoWorkThread->Start();
      }
      mVideoWorkThread->Restart();
      mVideoWorkThread->Post(this, MSG_RTMP_VIDEO_START);
   }
   if (mParam->live_model==LIVE_TYPE_VIDEO_AND_AUDIO||mParam->live_model==LIVE_TYPE_AUDIO_ONLY) {
      if (!mAudioWorkThread->started()) {
         mAudioWorkThread->Start();
      }
      mAudioWorkThread->Restart();
      mAudioWorkThread->Post(this, MSG_RTMP_AUDIO_START);
   }
}

void MediaEncode::Stop()
{
   if (mIsVideoInited) {
      mIsVideoInited = false;
      mVideoWorkThread->Clear(this);
      mVideoWorkThread->Post(this, MSG_RTMP_VIDEO_STOP);
      mVideoWorkThread->Stop();
   }
   if (mIsAudioInited) {
      mIsAudioInited = false;
      mAudioWorkThread->Clear(this);
      mAudioWorkThread->Post(this, MSG_RTMP_AUDIO_STOP);
      mAudioWorkThread->Stop();
   }
}

bool MediaEncode::isInit(){
   if (mParam->live_model==LIVE_TYPE_AUDIO_ONLY) {
      return mIsAudioInited;
   }else if(mParam->live_model==LIVE_TYPE_VIDEO_ONLY){
      return mIsVideoInited;
   }else{
      return mIsAudioInited&&mIsVideoInited;
   }
}

void MediaEncode::OnMessage(talk_base::Message* msg)
{
   switch(msg->message_id)
   {
      case MSG_RTMP_VIDEO_START:{
         mVideoFirstTS = 0;
         mVideoCount = 0;
         if (mParam==NULL) {
            LOGW("m_param is NULL!");
            return;
         }
         VHALL_DEL(mH264Encoder);
         //初始化音视频编码器
#ifdef VIDEO_ENCODE_X264
         auto x264Encoder = new X264Encoder();
         mH264Encoder = x264Encoder;
#else
         mH264Encoder = new H264Encoder();
#endif
         if (mH264Encoder==NULL) {
            LOGE("m_h264_encoder new error");
         }
         
         //copy parameters
         VHALL_DEL(mVideoEncParam);
         mVideoEncParam = new VideoEncParam;
         if (mVideoEncParam == NULL) {
            printf("mVideoEncParam is nullptr\n");
         }
         mVideoEncParam->width = mParam->width;
         mVideoEncParam->height = mParam->height;
         mVideoEncParam->frame_rate = mParam->frame_rate;
         mVideoEncParam->bit_rate = mParam->bit_rate;
         mVideoEncParam->gop_interval = mParam->gop_interval;
         mVideoEncParam->video_process_filters = mParam->video_process_filters;
         mVideoEncParam->is_quality_limited = mParam->is_quality_limited;
         mVideoEncParam->is_quality_protect = mParam->is_quality_protect;
         mVideoEncParam->is_adjust_bitrate = mParam->is_adjust_bitrate;
         mVideoEncParam->high_codec_open = mParam->high_codec_open;
         mVideoEncParam->is_encoder_debug = mParam->is_encoder_debug;
         mVideoEncParam->is_saving_data_debug = mParam->is_saving_data_debug;

         switch (mParam->encode_pix_fmt)
         {
         case ENCODE_PIX_FMT_YUV420SP_NV21:
            mVideoEncParam->pixel_format = PIX_FMT_YUV420SP_NV21;
            break;
         case ENCODE_PIX_FMT_YUV420SP_NV12:
            mVideoEncParam->pixel_format = PIX_FMT_YUV420SP_NV12;
            break;
         case ENCODE_PIX_FMT_YUV420P_I420:
            mVideoEncParam->pixel_format = PIX_FMT_YUV420P_I420;
            break;
         case ENCODE_PIX_FMT_YUV420P_YV12:
            mVideoEncParam->pixel_format = PIX_FMT_YUV420P_YV12;
            break;
         default:
            mVideoEncParam->pixel_format = PIX_FMT_YUV420SP_NV21;
            break;
         }
      
         // init encoder
         if (!mH264Encoder->Init(mVideoEncParam)) {
            VHALL_DEL(mH264Encoder);
            if (mStatusListener) {
               EventParam param;
               param.mId = -1;
               param.mDesc = "H264 encoder init ERROR";
               mStatusListener->NotifyEvent(ERROR_PARAM, param);
            }
         }else{
            if (mParam->encode_type == ENCODE_TYPE_SOFTWARE) {
               int headerSize = 0;
               bool ret = mH264Encoder->GetSpsPps(mVideoEncodedData, &headerSize);
               if (ret&&mOutputListener) {
                  mOutputListener->OnSendVideoData(mVideoEncodedData, headerSize, VIDEO_HEADER, 0);
               }
            }
            mIsVideoInited = true;
         }
      }
         break;
      case MSG_RTMP_AUDIO_START:{
         VHALL_DEL(mAacEncoder);
         mAacEncoder = new AACEncoder();
         if (mAacEncoder==NULL) {
            LOGE("m_aac_encoder new error");
         }
         if (!mAacEncoder->Init(mParam)) {
            VHALL_DEL(mAacEncoder);
            if (mStatusListener) {
               EventParam param;
               param.mId = -1;
               param.mDesc = "AAC encoder init ERROR";
               mStatusListener->NotifyEvent(ERROR_PARAM, param);
            }
         }else{
            int headerSize = 0;
            bool ret = mAacEncoder->GetAudioHeader(mAudioEncodedData, &headerSize);
            if (ret&&mOutputListener) {
               mOutputListener->OnSendAudioData(mAudioEncodedData, headerSize,AUDIO_HEADER,0);
            }
            mIsAudioInited = true;
         }
      }
         break;
      case MSG_RTMP_VIDEO_STOP:{
         VHALL_DEL(mH264Encoder);
      }
         break;
      case MSG_RTMP_AUDIO_STOP:{
         VHALL_DEL(mAacEncoder);
      }
         break;
      case MSG_RTMP_ENCODE_VIDEO:
      {
         SafeDataMessageData * obs = static_cast<SafeDataMessageData*>(msg->pdata);
         SafeData * data = obs->mSafeData;
         OnEncodeVideo(data->mData, data->mSize, data->mType, data->mTs, obs->mExtendParam);
         VHALL_DEL(obs->mExtendParam);
         obs->mSafeData->SelfRelease();
         obs->mSafeData = nullptr;
      }
         break;
      case MSG_RTMP_ENCODE_VIDEOHW:
      {
         SafeDataMessageData * obs = static_cast<SafeDataMessageData*>(msg->pdata);
         SafeData * data = obs->mSafeData;
         OnEncodeVideoHW(data->mData, data->mSize, data->mType, data->mTs);
         obs->mSafeData->SelfRelease();
         obs->mSafeData = nullptr;
      }
         break;
      case MSG_RTMP_ENCODE_AUDIO:
      {
         SafeDataMessageData * obs = static_cast<SafeDataMessageData*>(msg->pdata);
         SafeData * data = obs->mSafeData;
         OnEncodeAudio(data->mData, data->mSize, data->mTs);
         obs->mSafeData->SelfRelease();
         obs->mSafeData = nullptr;
      }
         break;
      case MSG_RTMP_ENCODE_AUDIOHW:
      {
         SafeDataMessageData * obs = static_cast<SafeDataMessageData*>(msg->pdata);
         SafeData * data = obs->mSafeData;
         OnEncodeAudio(data->mData, data->mSize, data->mTs);
         obs->mSafeData->SelfRelease();
         obs->mSafeData = nullptr;
      }
         break;
      default:
         break;
   }
   VHALL_DEL(msg->pdata);
}

void MediaEncode::OnStart(){
   
}

void MediaEncode::OnEncodeVideo(const char * data,
                                int size, int rotate,
                                uint64_t timestamp,
                                LiveExtendParam *extendParam)
{
   if (mH264Encoder==NULL||mIsVideoInited==false) {
      return;
   }
   uint64_t pts = 0;
   int encoded_size = 0;
   int encoded_type = 0;
   VideoType type = VIDEO_TYPE_NORMAL;
   if (extendParam != NULL && extendParam->scene_type == SceneType_Artificial) {
      type = VIDEO_TYPE_DESKTOP;
   }
   int ret = mH264Encoder->Encode(data, size,
                                  mVideoEncodedData, &encoded_size, &encoded_type,timestamp,&pts, type);
   if (mVideoFirstTS == 0) {
      mVideoFirstTS = Utility::GetTimestampMs();
   } else {
      uint64_t interval_time_stamp = Utility::GetTimestampMs() - mVideoFirstTS;
      LOGD("x264encode frame rate: %.3f type:%d",
           mVideoCount*1000.0f/interval_time_stamp,encoded_type);
   }
   if (ret>0) {
	  if (mMediaQADataInOut){
		 char* decodeData = NULL;
		 int dataSize = 0;
       double score = -1.0;
		 bool ret = mH264Encoder->GetRecentlyDecodeYUVData(&decodeData,&dataSize);
		 if (ret){
          if (mMediaQADataInOut) {
             score = mMediaQADataInOut->OnVideoDataInteract(data, size, decodeData, dataSize);
          }
		 }
       if (score != -1) {
          SetVideoQualityGrade(score);
       }
	  }
      if (mOutputListener) {
         mOutputListener->OnSendVideoData(mVideoEncodedData, encoded_size,encoded_type, pts);
         mVideoCount++;
      }
   }
   else {
      if (mMediaQADataInOut) {
         mMediaQADataInOut->OnVideoDataInteract(data, size, nullptr, 0);
      }
   }
}

void MediaEncode::OnEncodeVideoHW(const char *data, int size, int type, uint64_t timestamp)
{
   if (mOutputListener) {
      mOutputListener->OnSendVideoData(data, size, type, timestamp);
   }
}

void MediaEncode::OnEncodeAudio(const char * data, int size, uint64_t timestamp)
{
   if (mAacEncoder==NULL||mIsAudioInited==false) {
      return;
   }
   int encoded_size = 0;
   uint64_t pts = 0;
   if (mAacEncoder->Encode(data, size, timestamp,
                           mAudioEncodedData, &encoded_size,&pts)) {
      if (mOutputListener) {
         mOutputListener->OnSendAudioData(mAudioEncodedData, encoded_size, AUDIO_FRAME, pts);
      }
   }
}

bool MediaEncode::LiveGetRealTimeStatus(VHJson::Value &value)
{
   value["Name"] = VHJson::Value("MediaEncoder");
   //TODO may need to make it thread safe.
   value["video_frame_count"] = VHJson::Value(mVideoCount);
   VHJson::Value video_encoder, audio_encoder;
   bool ret = false;
   if (mH264Encoder){
      ret = mH264Encoder->LiveGetRealTimeStatus(video_encoder);
      if (!ret){
         LOGE("Get encoder realtime status failed!");
      }
      else {
         value["VideoEncoder"] = video_encoder;
      }
   }
   if (mAacEncoder){
      ret = mAacEncoder->LiveGetRealTimeStatus(audio_encoder);
      if (!ret){
         LOGE("Get muxer realtime status failed!");
      }
      else{
         value["AudioEncoder"] = audio_encoder;
      }
   }
   return true;
}

//void RtmpEncode::OnEncodeAudio2(const char * data, int size, uint32_t timestamp)
//{
//   if (!m_aac_encoder) {
//      return;
//   }
//   int encoded_size = 0;
//   uint32_t pts = 0;
//   if (m_aac_encoder->Encode(data, size,
//                             m_encoded_data, &encoded_size,timestamp,&pts)) {
//      p_vinny_live->GetRtmpPublish()->PublishAudio(m_encoded_data, encoded_size,
//                                                   pts);
//   }
//}
