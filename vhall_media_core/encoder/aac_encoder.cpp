#include "aac_encoder.h"
#include "../common/vhall_log.h"
#include "../common/live_define.h"
#include "../3rdparty/json/json.h"

char *AVSampleFormatStr[] = {
	(char*)"AV_SAMPLE_FMT_NONE",        // = -1
	(char*)"AV_SAMPLE_FMT_U8",          ///< unsigned 8 bits
	(char*)"AV_SAMPLE_FMT_S16",         ///< signed 16 bits
	(char*)"AV_SAMPLE_FMT_S32",         ///< signed 32 bits
	(char*)"AV_SAMPLE_FMT_FLT",         ///< float
	(char*)"AV_SAMPLE_FMT_DBL",         ///< double
	(char*)"AV_SAMPLE_FMT_U8P",         ///< unsigned 8 bits, planar
	(char*)"AV_SAMPLE_FMT_S16P",        ///< signed 16 bits, planar
	(char*)"AV_SAMPLE_FMT_S32P",        ///< signed 32 bits, planar
	(char*)"AV_SAMPLE_FMT_FLTP",        ///< float, planar
	(char*)"AV_SAMPLE_FMT_DBLP",        ///< double, planar
	(char*)"AV_SAMPLE_FMT_S64",         ///< signed 64 bits
	(char*)"AV_SAMPLE_FMT_S64P",        ///< signed 64 bits, planar
	(char*)"AV_SAMPLE_FMT_NB"          ///< Number of sample formats. DO NOT USE if linking dynamically
};

AACEncoder::AACEncoder(){
   mCodec = NULL;
   mAvctx = NULL;
   mFrame = NULL;
   mPkt = NULL;
   mSwrContext = NULL;
   mFrameCount = 0;
   mFrameFaildCount = 0;
}

AACEncoder::~AACEncoder(){
   Destroy();
}

bool AACEncoder::Init(LivePushParam * param) {
   mCodec = avcodec_find_encoder(AV_CODEC_ID_AAC);
   if (!mCodec) {
      LOGE("avcodec_find_encoder(AV_CODEC_ID_AAC) error!");
      return false;
   }
   mParam = (LivePushParam *)param;
   mAvctx= avcodec_alloc_context3(mCodec);
   if (!mAvctx) {
      LOGE("avcodec_alloc_context3(m_codec) error!");
      return false;
   }
   mSrcSampleFmt = (AVSampleFormat)param->src_sample_fmt;
   mDstSampleFmt = (AVSampleFormat)param->encode_sample_fmt;
   
   mAvctx->codec_type            = AVMEDIA_TYPE_AUDIO;
   mAvctx->codec_id              = mCodec->id;
   mAvctx->profile               = FF_PROFILE_AAC_LOW;
   mAvctx->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
   mAvctx->bit_rate              = param->audio_bitrate;
   mAvctx->sample_rate           = param->dst_sample_rate;
   mAvctx->channel_layout        = av_get_default_channel_layout(param->ch_num);
   mAvctx->channels              = param->ch_num;
   mAvctx->sample_fmt            = mDstSampleFmt;
   
   int ret = 0;
   if ((ret = avcodec_open2(mAvctx, mCodec, NULL)) < 0) {
      LOGE("audio avcodec_open2 error ret = %d",ret);
      return false;
   }
   
   mPkt = av_packet_alloc();
   if (!mPkt) {
      Destroy();
      LOGE("error av packet alloc.");
      return false;
   }
   
   mFrame = av_frame_alloc();
   if (!mFrame) {
      Destroy();
      LOGE("error av frame alloc.");
      return false;
   }
   
   mFrame->nb_samples     = mAvctx->frame_size;
   mFrame->format         = mAvctx->sample_fmt;
   mFrame->channel_layout = mAvctx->channel_layout;
   
   mSwrContext = swr_alloc_set_opts(NULL,
                                     mAvctx->channel_layout,
                                     mDstSampleFmt,
                                     mAvctx->sample_rate,
                                     mAvctx->channel_layout,
                                     mSrcSampleFmt,
                                     mAvctx->sample_rate,
                                     0,
                                     NULL);
   if (mSwrContext == NULL) {
       LOGE("m_swrContext alloc error!");
      Destroy();
      return false;
   }
   
   ret = swr_init(mSwrContext);
   if (ret<0) {
      LOGE("m_swrContext init error!");
      Destroy();
      return false;
   }
   
   ret = av_frame_get_buffer(mFrame, 0);
   if (ret < 0) {
      Destroy();
      LOGE("error av frame get buffer. ret:%d",ret);
      return false;
   }
   
   mFrameCount = 0;
   mFrameFaildCount = 0;

   return true;
}

void AACEncoder::Destroy(){
   
   if (mAvctx) {
      avcodec_close(mAvctx);
      avcodec_free_context(&mAvctx);
      mAvctx = NULL;
   }
   
   if (mFrame) {
      av_frame_free(&mFrame);
      mFrame = NULL;
   }
   
   if (mPkt) {
      av_packet_free(&mPkt);
      mPkt = NULL;
   }
   
   if(mSwrContext){
      swr_close(mSwrContext);
      swr_free(&mSwrContext);
      mSwrContext = NULL;
   }
}

int AACEncoder::Encode(const char * in_data,
                        int in_size,
                        uint64_t in_ts,
                        char * out_data,
                        int * out_size,
                        uint64_t *out_pts) {
   int ret = av_frame_make_writable(mFrame);
   if (ret<0) {
      LOGE("av_frame_make_writable error ret:%d",ret);
      return -1;
   }
   uint8_t * samples = (uint8_t*)mFrame->data[0];
   if (mParam&&mParam->src_sample_fmt==mParam->encode_sample_fmt) {
      memcpy(samples, in_data, in_size);
   }else{
     uint8_t *in[] = {(uint8_t *)in_data};
     ret = swr_convert(mSwrContext,mFrame->data,mFrame->nb_samples, (const uint8_t **)in,mFrame->nb_samples);
      if (ret<0) {
         LOGE("error swr convert.");
         return -2;
      }
   }
   mFrame->pts = in_ts;
   ret = avcodec_send_frame(mAvctx, mFrame);
   if (ret < 0) {
      mFrameFaildCount++;
      LOGE("error sending the frame to the encoder. ret:%d",AVERROR(ret));
      return -3;
   }
   *out_size = 0;
   *out_size = 0;
   while (ret >= 0) {
      ret = avcodec_receive_packet(mAvctx, mPkt);
       if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
           break;
       }else if(ret<0){
         mFrameFaildCount++;
         LOGE("error encoding audio frame. ret:%d",ret);
         return -4;
      }else {
         *out_size = mPkt->size;
         memcpy(out_data, mPkt->data, mPkt->size);
         if (mPkt->pts>=0) {
            *out_pts = mPkt->pts;
         }else{
            *out_pts = 1;
         }
         av_packet_unref(mPkt);
         mFrameCount++;
      }
   }
   return *out_size;
}

bool AACEncoder::GetAudioHeader(char*out_data,int *data_size){
   if (out_data!=NULL&&mAvctx!=NULL) {
      memcpy(out_data, mAvctx->extradata, mAvctx->extradata_size);
      *data_size = mAvctx->extradata_size;
      return true;
   }
   return false;
}

bool AACEncoder::LiveGetRealTimeStatus(VHJson::Value &value){
   
   if (mAvctx==NULL) {
      return false;
   }
   value["Name"] = VHJson::Value("AACEncoder");
   
   //TODO may need to make it thread safe.
   value["codec_id"] = VHJson::Value(mAvctx->codec_id);
   value["profile"] = VHJson::Value((int)mAvctx->profile);
   value["strict_std_compliance"] = VHJson::Value(mAvctx->strict_std_compliance);
   value["bit_rate"] = VHJson::Value((int)mAvctx->bit_rate);
   value["channel_layout"] = VHJson::Value((uint32_t)mAvctx->channel_layout);
   value["sample_rate"] = VHJson::Value(mAvctx->sample_rate);
   value["channels"] = VHJson::Value(mAvctx->channels);
   value["dst_sample_fmt"] = VHJson::Value(AVSampleFormatStr[mDstSampleFmt + 1]);
   value["src_sample_fmt"] = VHJson::Value(AVSampleFormatStr[mSrcSampleFmt + 1]);
   value["frame_success_count"] = VHJson::Value(mFrameCount);
   value["frame_faild_count"] = VHJson::Value(mFrameFaildCount);
   
   return true;
}
