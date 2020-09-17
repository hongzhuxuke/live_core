#include "rtmp_reader.h"
#include "../common/vhall_log.h"
#include "../api/live_interface.h"
#include "../os/vhall_monitor_log.h"
#include "../rtmpplayer/vhall_live_player.h"
#include "talk/base/thread.h"
#include "../common/auto_lock.h"
#include "../utility/utility.h"
#include "../api/vhall_live.h"

#define MAX_ENCODED_PKT_NUM     100
#define DEFULT_PUBLISH_TIME_OUT 5000
#define ERROR_SUCCESS           0

RtmpReader::RtmpReader(VhallPlayerInterface*player):
mWorkerThread(NULL),
mComputeSpeedThread(NULL),
mPlayer(player),
//mAvcAacCodec(NULL),
mFlvTagDemuxer(NULL),
mParam(NULL){
   mRtmp = 0;
   mTotalSize = 0;
   mConnectTimeout = 0;
   mStart = false;
   mGotAudioPkt = false;
   mGotVideoKeyFrame = false;
   vhall_lock_init(&mDeatoryRtmpMutex);
   Init();
}

RtmpReader::~RtmpReader() {
   Stop();
   Destory();
   if (mFlvTagDemuxer){
      VHALL_DEL(mFlvTagDemuxer);
   }
   vhall_lock_destroy(&mDeatoryRtmpMutex);
}

void RtmpReader::SetParam(LivePlayerParam *param){
   mParam = param;
}

void RtmpReader::AddMediaInNotify(IMediaNotify* mediaNotify) {
   mMediaOutputNotify.push_back(mediaNotify);
}

void RtmpReader::ClearMediaInNotify(){
   mMediaOutputNotify.clear();
}

void RtmpReader::Init(){
   mComputeSpeedThread = new talk_base::Thread();
   if (mComputeSpeedThread == NULL) {
      LOGE("mComputeSpeedThread is NULL");
      return;
   }
   mComputeSpeedThread->Start();
   mWorkerThread = new talk_base::Thread();
   if (mWorkerThread == NULL) {
      LOGE("mWorkerThread is NULL");
      return;
   }
   mWorkerThread->Start();
   
}

void RtmpReader::Destory(){
   VHALL_THREAD_DEL(mWorkerThread);
   VHALL_THREAD_DEL(mComputeSpeedThread);
}

void RtmpReader::CloseRtmp(){
   VhallAutolock lock(&mDeatoryRtmpMutex);
   if(mRtmp){
      srs_rtmp_destroy(mRtmp);
      mRtmp = NULL;
   }
}

void RtmpReader::DestoryRtmp()
{
   VhallAutolock lock(&mDeatoryRtmpMutex);
   if(mRtmp){
      srs_rtmp_destroy(mRtmp);
      mRtmp = NULL;
   }
}

void RtmpReader::Stop() {
   if(mStart == false)
      return;
   mStart = false;
   {
      VhallAutolock lock(&mDeatoryRtmpMutex);
      if (mRtmp){
         srs_rtmp_async_close(mRtmp);
      }
   }
   mWorkerThread->Clear(this);
   mComputeSpeedThread->Clear(this);
   mWorkerThread->Post(this, MSG_RTMP_Close);
}

void RtmpReader::Start(const char *url) {
   mStart = true;
   mConnectTimeout = mParam->watch_timeout>0?mParam->watch_timeout:0;
   if (url)
      mUrl = url;
   
   mWorkerThread->Post(this, MSG_RTMP_Connect);
   mMaxConnectTryTime = mParam->watch_reconnect_times;
   mConnectTryTime = 0;
   mParam->live_model = LIVE_TYPE_NONE;
}

bool RtmpReader::OnConnect(int timeout) {
   DestoryRtmp();
   mRtmp = srs_rtmp_create(mUrl.c_str());
   if (mRtmp == NULL) {
      LOGE("srs_rtmp_create failed.");
      return false;
   }
   
   LOGD("rtmp connect start");
   int time_out = mConnectTimeout <= 0 ? DEFULT_PUBLISH_TIME_OUT : mConnectTimeout;
   if (srs_rtmp_handshake(mRtmp) != 0) {
      LOGE("simple handshake failed.");
      DestoryRtmp();
      return false;
   }
   LOGD("simple handshake success");
   
   if (srs_rtmp_connect_app(mRtmp) != 0) {
      LOGE("connect vhost/app failed.");
      DestoryRtmp();
      return false;
   }
   srs_rtmp_set_timeout(mRtmp, time_out, time_out);
   LOGD("connect vhost/app success");
   if (srs_rtmp_play_stream(mRtmp) != 0) {
      LOGE("play stream failed.");
      DestoryRtmp();
      return false;
   }
   EventParam param;
   param.mId = -1;
   param.mDesc = GetServerIp();
   mPlayer->NotifyEvent(SERVER_IP, param);
   LOGI("play stream success,tcurl:%s",mUrl.c_str());
   param.mDesc = "Player Rtmp Connect OK";
   mPlayer->NotifyEvent(OK_WATCH_CONNECT, param);
   return true;
}

int RtmpReader::OnRecv() {
   if (mStart) {
      int ret = ERROR_SUCCESS;
      // read from cache first.
      int size = 0;
      char type = 0;
      char* data = NULL;
      uint32_t timestamp = 0;
      if (mRtmp==NULL){
         return -1;
      }
      if ((ret = srs_rtmp_read_packet(mRtmp, &type, &timestamp, &data, &size)) != 0) {
         LOGE("recv_message failed, will exit rtmp recv loop :%d",ret);
         // Modify by liwenlong 2016.2.27
         mComputeSpeedThread->Clear(this);
         mConnectTryTime++;
         if (mConnectTryTime> mMaxConnectTryTime){
            EventParam param;
            param.mId = -1;
            param.mDesc = "Player stream failed";
            mPlayer->NotifyEvent(ERROR_WATCH_CONNECT, param);
            LOGE("connect failed.");
            return ret;
         }
         if (mStart){
            LOGW("start reconnect: %d", mConnectTryTime);
            mWorkerThread->PostDelayed(mConnectTimeout, this, MSG_RTMP_Connect);
         }
         return ret;
      }
      
      if (data != NULL && size > 0){
         mTotalSize += size;
         if (type == SRS_RTMP_TYPE_AUDIO){
            OnAudio(timestamp, data, size);
         }else if (type == SRS_RTMP_TYPE_VIDEO){
            OnVideo(timestamp, data, size);
            // Modify by liwenlong 2016.2.27
            mConnectTryTime = 0;
         }else if (type == SRS_RTMP_TYPE_SCRIPT){
            int ret = OnMetaData(timestamp, data, size);
            if (ret != ERROR_SUCCESS) {
               LOGD("OnMetaData unknown ret:%d",ret);
            }
         } else {
            LOGW("unknown AMF0/AMF3 data message.");
         }
         VHALL_DEL(data);
      }
      mWorkerThread->Post(this, MSG_RTMP_Recving);
   }
   return 0;
}

void RtmpReader::OnMessage(talk_base::Message* msg) {
   switch (msg->message_id) {
      case MSG_RTMP_Connect:
         if (true == mStart ) {
            if(true == OnConnect(0)){
               mGotVideoKeyFrame = false;
               mGotAudioPkt = false;
               LOGI("will rtmp recv loop, destory last media out.");
               for (size_t i = 0; i < mMediaOutputNotify.size(); i++) {
                  mMediaOutputNotify[i]->Destory();
               }//end for mMediaOutputNotify.size()
               VHALL_DEL(mFlvTagDemuxer);
               mFlvTagDemuxer = new FlvTagDemuxer();
               mWorkerThread->Post(this, MSG_RTMP_Recving);
               mComputeSpeedThread->Clear(this);
               mComputeSpeedThread->PostDelayed(5, this, MSG_RTMP_ComputeSpeed);
               LOGD("will recv loop.");
            } else {
               //reconnect
               LOGE("exit rtmp connect.");
               mConnectTryTime ++;
               mComputeSpeedThread->Clear(this);
               if(mConnectTryTime> mMaxConnectTryTime){
                  EventParam param;
                  param.mId = -1;
                  param.mDesc = "Player stream failed";
                  mPlayer->NotifyEvent(ERROR_WATCH_CONNECT, param);
                  LOGE("connect failed.");
               }else{
                  LOGW("start reconnect: %d", mConnectTryTime);
                  mWorkerThread->PostDelayed(mConnectTimeout, this, MSG_RTMP_Connect);
               }
            }
         }else{
            LOGW("mStart is false!");
         }
         break;
      case MSG_RTMP_Recving:
         OnRecv();
         break;
      case MSG_RTMP_ComputeSpeed:
         if (mStart == true){
            OnComputeSpeed();
         }
         break;
      case MSG_RTMP_Close:
         CloseRtmp();
         LOGI("close RTMP connect");
         break;
      default:
         break;
   }
   VHALL_DEL(msg->pdata);
}

int RtmpReader::OnAudio(uint32_t timestamp, char*data, int size)
{
   int ret = ERROR_SUCCESS;
   //SrsAutoFree(SrsCommonMessage, audio);
   AacAvcCodecSample codecSample;
   if ((ret = mFlvTagDemuxer->audio_aac_demux(data, size, &codecSample)) != ERROR_SUCCESS) {
      LOGE("aac codec demux audio failed. ret=%d", ret);
      return ret;
   }
   if (codecSample.nb_sample_units == 0){
      LOGD("no audio sample unit");
      return ret;
   }
   if (mFlvTagDemuxer->audio_codec_id != FlvSoundFormatAAC) {
      LOGE("only suppot aac codec");
      return ret;
   }
   if (mGotAudioPkt == false) {
      mGotAudioPkt = true;
      AudioParam audioParam;
      GetAudioParam(audioParam, mFlvTagDemuxer, &codecSample);
      LOGI("Get first audio packet, will get audio codec, and notify mediaout(InitAudio)");
      for (size_t i = 0; i < mMediaOutputNotify.size(); i++) {
         mMediaOutputNotify[i]->InitAudio(&audioParam);
      }
   }
   for (int i = 0; i < codecSample.nb_sample_units; i++) {
      CodecSampleUnit* sample_unit = &codecSample.sample_units[i];
      ret = -1;
      
      LOGD("Audio(AAC) pkt timestamp=%lld ,size=%d,will notify %u mediaout",
           timestamp, sample_unit->size, (unsigned int)mMediaOutputNotify.size());
      
      for (size_t i = 0; i < mMediaOutputNotify.size(); i++) {
         DataUnit* newPkt = mMediaOutputNotify[i]->MallocDataUnit(STREAM_TYPE_AUDIO, sample_unit->size, 0);
         if (newPkt) {
            newPkt->dataSize = sample_unit->size;
            memcpy(newPkt->unitBuffer, sample_unit->bytes, (size_t)newPkt->dataSize);
            newPkt->timestap = timestamp;
            newPkt->isKey = false;
            newPkt->next = NULL;
            mMediaOutputNotify[i]->AppendStreamPacket(STREAM_TYPE_AUDIO, newPkt);
         }else{
            LOGW("Media output can't malloc free dataunit,will discard audio data, something wrong.");
         }
      }//end for mMediaOutputNotify.size()
   }//end nb_sample_units
   return ret;
}

int  RtmpReader::OnVideo(uint32_t timestamp, char*data, int size){
   int ret = 0;
   static u_int8_t aud_nal[] = { 0x00, 0x00, 0x00, 0x01};
   AacAvcCodecSample codecSample;
   if ((ret = mFlvTagDemuxer->video_avc_demux(data, size, &codecSample)) != ERROR_SUCCESS) {
      LOGE("hls codec demux video failed. ret=%d", ret);
      return ret;
   }
   // ignore info frame,
   // @see https://github.com/simple-rtmp-server/srs/issues/288#issuecomment-69863909
   if (codecSample.frame_type == FlvVideoFrameTypeVideoInfoFrame
       || FlvVideoAVCPacketTypeNALU != codecSample.avc_packet_type) {
      LOGI("found info frame,ignore it. ");
      return ret;
   }
   
   if (mFlvTagDemuxer->video_codec_id != FlvVideoCodecIDAVC) {
      LOGE("Only support AVC.");
      return ret;
   }
   if (codecSample.nb_sample_units == 0){
      LOGD("no video sample unit");
      return ret;
   }
   
   // ignore sequence header
   //first frame must key frame
   if (mGotVideoKeyFrame == false) {
      if (codecSample.frame_type != FlvVideoFrameTypeKeyFrame){
         LOGI("First frame must key frame,ignore this pkt. ");
         return ret;
      }
      LOGI("Got First frame must key frame. ");
      mGotVideoKeyFrame = true;
      VideoParam videoParam;
      GetVideoParam(videoParam, mFlvTagDemuxer, &codecSample);
      for (size_t i = 0; i < mMediaOutputNotify.size(); i++) {
         mMediaOutputNotify[i]->InitVideo(&videoParam);
      }
   }
   for (size_t i = 0; i < mMediaOutputNotify.size(); i++) {
       int unitSize = 0;
       for (int i = 0; i < codecSample.nb_sample_units; i++){
           CodecSampleUnit* sample_unit = &codecSample.sample_units[i];
           unitSize += 4;
           unitSize += sample_unit->size;
       }
       if(codecSample.frame_type == FlvVideoFrameTypeKeyFrame){ //
           unitSize += mFlvTagDemuxer->sequenceParameterSetLength + mFlvTagDemuxer->pictureParameterSetLength + 8;
       }
       DataUnit* newPkt = mMediaOutputNotify[i]->MallocDataUnit(STREAM_TYPE_VIDEO, unitSize, 0);
       if(newPkt){
           unsigned char* begin = newPkt->unitBuffer;
           int dataSize = 0;
           if(codecSample.frame_type == FlvVideoFrameTypeKeyFrame){
               memcpy(begin, aud_nal, 4);
               begin += 4;
               dataSize += 4;
               memcpy(begin, mFlvTagDemuxer->sequenceParameterSetNALUnit, mFlvTagDemuxer->sequenceParameterSetLength);
               begin += mFlvTagDemuxer->sequenceParameterSetLength;
               dataSize += mFlvTagDemuxer->sequenceParameterSetLength;
               memcpy(begin, aud_nal, 4);
               begin += 4;
               dataSize += 4;
               memcpy(begin, mFlvTagDemuxer->pictureParameterSetNALUnit, mFlvTagDemuxer->pictureParameterSetLength);
               begin += mFlvTagDemuxer->pictureParameterSetLength;
               dataSize += mFlvTagDemuxer->pictureParameterSetLength;
               newPkt->isKey = true;
           }else{
               newPkt->isKey = false;
           }
           
           //if i frame, wait for buffer.
           //block = codecSample.frame_type == SrsCodecVideoAVCFrameKeyFrame;
           for (int i = 0; i < codecSample.nb_sample_units; i++) {
              CodecSampleUnit* sample_unit = &codecSample.sample_units[i];
              AvcNaluType nalType = (AvcNaluType)(sample_unit->bytes[0] & 0x1F);
              if (nalType > AvcNaluTypeIDR && sample_unit->size < 80) {
                 LOGD("Not Frame data. ingore it. ");
                 continue;
              }
              LOGD("Video(AVC) pkt timestamp=%llu frame_type=%d, avc_packet_type=%d, nal type= %ld ,size=%d, will notify %u mediaout i=%d",
                   timestamp,
                   codecSample.frame_type,
                   codecSample.avc_packet_type,
                   nalType,
                   sample_unit->size,
                   (unsigned int)mMediaOutputNotify.size(),
                   i);
               memcpy(begin, aud_nal, 4);
               begin += 4;
               dataSize += 4;
               memcpy(begin, sample_unit->bytes, sample_unit->size);
               begin += sample_unit->size;
               dataSize += sample_unit->size;
           }
           newPkt->dataSize = dataSize;
           newPkt->timestap = timestamp;
           newPkt->next = NULL;
           mMediaOutputNotify[i]->AppendStreamPacket(STREAM_TYPE_VIDEO, newPkt);
       }else{
           if (codecSample.frame_type == FlvVideoFrameTypeKeyFrame){
              LOGD("Key Frame is lost.");
           }
           LOGE("Media output can't malloc free dataunit,will discard video data, something wrong.");
       }
   }
   return ret;
}

int RtmpReader::OnMetaData(uint32_t timestamp, char*data, int size)
{
   int ret = 0;
   if ((ret = mFlvTagDemuxer->metadata_demux(data, size)) != 0){
      LOGE("Demux Metadata failed");
      return ret;
   }
   LOGI("process onMetaData message success.");
   if (mFlvTagDemuxer->metadata_type == MetadataTypeOnMetaData) {
      EventParam param;
      param.mId = -1;
      param.mDesc = "process onMetaData message success.";
      mPlayer->NotifyEvent(DEMUX_METADATA_SUCCESS, param);
      LiveModel streamType = LIVE_TYPE_NONE;
      if (mFlvTagDemuxer->video_codec_id == FlvVideoCodecIDAVC&&mFlvTagDemuxer->audio_codec_id == FlvSoundFormatAAC) {
         streamType = LIVE_TYPE_VIDEO_AND_AUDIO;
      }else if (mFlvTagDemuxer->video_codec_id == FlvVideoCodecIDAVC&&mFlvTagDemuxer->audio_codec_id != FlvSoundFormatAAC){
         streamType = LIVE_TYPE_VIDEO_ONLY;
      }else if (mFlvTagDemuxer->audio_codec_id == FlvSoundFormatAAC&&mFlvTagDemuxer->video_codec_id != FlvVideoCodecIDAVC){
         streamType = LIVE_TYPE_AUDIO_ONLY;
      }
      if (mParam->live_model != LIVE_TYPE_NONE && streamType != mParam->live_model) {
         
      }else{
         char streamTypeStr[2] = { 0 };
         snprintf(streamTypeStr, 2, "%d", streamType);
         EventParam param;
         param.mId = -1;
         param.mDesc = streamTypeStr;
         mPlayer->NotifyEvent(RECV_STREAM_TYPE, param);
      }
      mParam->live_model = streamType;
      for (size_t i = 0; i < mMediaOutputNotify.size(); i++) {
         mMediaOutputNotify[i]->InitLiveModel(streamType);
      }
      mGotVideoKeyFrame = false;
      mGotAudioPkt = false;
      LOGI("receive metadata, destory decoder. ret:%d",ret);
   }else if(mFlvTagDemuxer->metadata_type == MetadataTypeOnCuePoint){
      std::string amfMag = mFlvTagDemuxer->cuepoint_amf_msg.ToJsonStr();
      for (auto item:mMediaOutputNotify) {
         DataUnit* newPkt = item->MallocDataUnit(STREAM_TYPE_ONCUEPONIT_MSG, amfMag.length(), 0);
         if (newPkt) {
            newPkt->dataSize = amfMag.length();
            newPkt->isKey = false;
            newPkt->timestap = timestamp;
            newPkt->next = NULL;
            memcpy(newPkt->unitBuffer, amfMag.c_str(), (int)(newPkt->dataSize));
            item->AppendStreamPacket(STREAM_TYPE_ONCUEPONIT_MSG, newPkt);
         }else{
            LOGE("amf msg newPkt is null,lose amf msg.");
         }
      }
   }
   return ret;
}

void RtmpReader::OnComputeSpeed(){
   uint32_t m_speed = 0;
   m_speed = mTotalSize * 8/1024;
   mTotalSize = 0;
   EventParam param;
   param.mId = -1;
   param.mDesc = Utility::ToString(m_speed);
   mPlayer->NotifyEvent(INFO_SPEED_DOWNLOAD, param);
   mComputeSpeedThread->PostDelayed(1000, this, MSG_RTMP_ComputeSpeed);
}

//http://www.cnblogs.com/musicfans/archive/2012/11/07/2819291.html
void RtmpReader::GetAudioParam(AudioParam &audioParam, FlvTagDemuxer* flv_tag_demuxer, AacAvcCodecSample* codecSample) {
   audioParam.extra_size = flv_tag_demuxer->aac_extra_size;
   audioParam.extra_data = (char*)malloc(flv_tag_demuxer->aac_extra_size);
   if (audioParam.extra_data)
      memcpy(audioParam.extra_data, flv_tag_demuxer->aac_extra_data, flv_tag_demuxer->aac_extra_size);
   audioParam.numOfChannels = codecSample->sound_type == FlvSoundTypeMono?1:2;
   audioParam.samplesPerSecond = flv_tag_demuxer->audio_samplerate>0?flv_tag_demuxer->audio_samplerate:44100;
   audioParam.bitsPerSample = codecSample->sound_size == FlvSoundSampleSize8bit?8:16;
}

void RtmpReader::GetVideoParam(VideoParam &videoParam, FlvTagDemuxer* flv_tag_demuxer, AacAvcCodecSample* codecSample) {
   videoParam.avc_extra_size = flv_tag_demuxer->avc_extra_size;
   videoParam.avc_extra_data = (char*)malloc(flv_tag_demuxer->avc_extra_size);
   if (videoParam.avc_extra_data)
      memcpy(videoParam.avc_extra_data, flv_tag_demuxer->avc_extra_data, flv_tag_demuxer->avc_extra_size);
   videoParam.width = flv_tag_demuxer->width;
   videoParam.height = flv_tag_demuxer->height;
   if (flv_tag_demuxer->frame_rate <=5) {
      videoParam.framesPerSecond = DEFAULT_FPS_SUPPORT;
   }else if(flv_tag_demuxer->frame_rate >= MAX_FPS_SUPPORT){
      videoParam.framesPerSecond = MAX_FPS_SUPPORT;
   }else{
      videoParam.framesPerSecond = flv_tag_demuxer->frame_rate;
   }
   LOGD("frame_rate:%d width:%d height:%d",videoParam.framesPerSecond,flv_tag_demuxer->width,flv_tag_demuxer->height);
}

std::string RtmpReader::GetServerIp(){
   VhallAutolock lock(&mDeatoryRtmpMutex);
   char ip_buf[64];
   if (mRtmp) {
      if (srs_rtmp_get_remote_ip(mRtmp, ip_buf, 64) > 0){
         std::string ip_str = ip_buf;
         return ip_str;
      }
   }
   return "";
}

