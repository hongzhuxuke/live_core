#include "h264_decoder.h"
#include "../common/vhall_log.h"

H264Decoder::H264Decoder()
:codec_(NULL)
,avctx_(NULL)
,frame_(NULL) {
}

H264Decoder::H264Decoder(char* extra_data,int  extra_size)
:codec_(NULL)
,avctx_(NULL)
,frame_(NULL) {
   mCodecExtraData = 0;
   mCodecExtraSize = 0;
   if(extra_size > 0){
      mCodecExtraSize = extra_size;
      mCodecExtraData = (char*)malloc(extra_size);
      if (mCodecExtraData)
         memcpy(mCodecExtraData, extra_data, extra_size);
   }
}

H264Decoder::~H264Decoder() {
   destroy();
}

void H264Decoder::destroy() {
   if (avctx_) {
      if (avctx_->extradata) {
         av_free(avctx_->extradata);
         avctx_->extradata = NULL;
      }
      avcodec_close(avctx_);
      av_free(avctx_);
      avctx_ = NULL;
   }
   if (frame_) {
      av_freep(&frame_);
      frame_ = NULL;
   }
   VHALL_DEL(mCodecExtraData);
}

bool H264Decoder::Init(int w, int h) {
   codec_ = avcodec_find_decoder(AV_CODEC_ID_H264);
   if (!codec_) {
      LOGE("error avcodec find decoder.");
      return false;
   }
   avctx_ = avcodec_alloc_context3(codec_);
   if (!avctx_) {
      LOGE("error avcodec alloc context3.");
      return false;
   }
   
   if (codec_->capabilities&AV_CODEC_CAP_TRUNCATED) {
      avctx_->flags |= AV_CODEC_FLAG_TRUNCATED; /* we do not send complete frames */
   }
   if(mCodecExtraData){
      avctx_->extradata = (uint8_t*)av_mallocz(mCodecExtraSize);
      if(avctx_->extradata)
         memcpy(avctx_->extradata, mCodecExtraData, mCodecExtraSize);
      avctx_->extradata_size = mCodecExtraSize;
   }
   
   if (avcodec_open2(avctx_, codec_, NULL) < 0) {
      LOGE("error avcodec open2.");
      return false;
   }
   
   frame_ = av_frame_alloc();
   if (!frame_) {
      LOGE("error av frame alloc.");
      return false;
   }
   memset((void*)&mVideoParam, 0, sizeof(VideoParam));
   return true;
}

bool H264Decoder::Decode(const char * data, int size, int & decode_size, uint64_t ts) {
   av_init_packet(&pkt_);
   pkt_.data = (uint8_t*)data;
   pkt_.size = size;
   pkt_.pts = (int64_t)ts;
   
   int ret = avcodec_send_packet(avctx_, &pkt_);
   if (ret < 0) {
      LOGE("error avcodec send packet.");
      return false;
   }
   decode_size = size;
   av_packet_unref(&pkt_);
   return true;
}

bool H264Decoder::GetDecodecData(unsigned char * decoded_data, int & decode_size, uint64_t& pts){
    av_frame_unref(frame_);
   int ret = avcodec_receive_frame(avctx_, frame_);
   if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
      return false;
   }else if (ret < 0) {
      LOGE("error avcodec receice frame.ret:%d",ret);
      return false;
   }
   mVideoParam.width = avctx_->width;
   mVideoParam.height = avctx_->height;
   mVideoParam.framesPerSecond = avctx_->framerate.num/avctx_->framerate.den;
   mVideoParam.framesPerSecond  = MAX(5, mVideoParam.framesPerSecond);
   if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF){
      return false;
   }else{
      av_image_copy_to_buffer((unsigned char*)decoded_data,
                              decode_size,
                              frame_->data,
                              frame_->linesize,
                              (AVPixelFormat)frame_->format,
                              frame_->width,
                              frame_->height,
                              1);
      pts = (int64_t)frame_->pts;
      return true;
   }
}
