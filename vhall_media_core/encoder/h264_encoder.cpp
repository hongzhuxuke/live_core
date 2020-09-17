#include "h264_encoder.h"
#include "vhall_log.h"
#include "live_define.h"
#include "../utility/utility.h"
#include "../3rdparty/json/json.h"

H264Encoder::H264Encoder(){
   mCodec = NULL;
   mAvctx = NULL;
   mFrame = NULL;
   mYuvBuffer = NULL;
   mFrameCount = 0;
   mFrameFailedCount = 0;
   Ori = 0;
   mKeyframeInterval = 2.0;
   mLiveParam = (LivePushParam*)calloc(1, sizeof(LivePushParam));
}

H264Encoder::~H264Encoder(){
   Destroy();
}

bool H264Encoder::Init(const LivePushParam * param) {
   *mLiveParam = *param;
   if (param->gop_interval > 0){
	   mKeyframeInterval = param->gop_interval;
   }

   mCodec = avcodec_find_encoder(AV_CODEC_ID_H264);
   if (!mCodec) {
      LOGE("avcodec_find_encoder(AV_CODEC_ID_H264) error!");
      return false;
   }
   
   mAvctx = avcodec_alloc_context3(mCodec);
   if (!mAvctx) {
      LOGE("avcodec_alloc_context3(m_codec) error!");
      return false;
   }
   
   av_opt_set(mAvctx->priv_data, "tune", "grain", 0);
   av_opt_set(mAvctx->priv_data, "preset", "superfast", 0);
   //av_opt_set_double(mAvctx->priv_data,"crf", param->crf, 0);
   
   mBitrate = param->bit_rate;
   mAvctx->max_b_frames = 0;
   //m_avctx->bit_rate = m_bitrate;
   
   /* resolution must be a multiple of two */
   mOrgWidth = param->width;
   mOrgHeight = param->height;
   
   mAvctx->width = mOrgWidth;
   mAvctx->height = mOrgHeight;

   
   /* frames per second */
   mAvctx->time_base.den = (int)((double)param->frame_rate*100.0 / 1 + 0.5);
   mAvctx->time_base.num = 100;
   {
      if((mAvctx->time_base.den / mAvctx->time_base.num) < 5 || (mAvctx->time_base.den / mAvctx->time_base.num) > 30)
      {
         mAvctx->time_base.den = 1500;
         mAvctx->time_base.num = 100;
      }
   }
   
   mAvctx->time_base.den |= 1;
   mAvctx->time_base.num += (mAvctx->time_base.num & 1);
   
   mAvctx->gop_size = param->frame_rate * mKeyframeInterval;
   mFrameRate = param->frame_rate;
   mAvctx->pix_fmt = AV_PIX_FMT_YUV420P;
   mAvctx->thread_count = 2;
   
   /* open it */
   if (avcodec_open2(mAvctx, mCodec, NULL) < 0) {
      LOGE("video avcodec_open2() error!");
      return false;
   }
   
   mFrame = av_frame_alloc();
   if (!mFrame) {
      LOGE("av_frame_alloc() error!");
      Destroy();
      return false;
   }
   
   mFrame->format = mAvctx->pix_fmt;
   mFrame->width  = mAvctx->width;
   mFrame->height = mAvctx->height;
   
   /* the image can be allocated by any means and av_image_alloc() is
    * just the most convenient way if av_malloc() is to be used */
   int ret = av_image_alloc(mFrame->data,
                            mFrame->linesize,
                            mAvctx->width,
                            mAvctx->height,
                            mAvctx->pix_fmt,
                            1);
   if (ret < 0) {
      Destroy();
      LOGE("av_image_alloc() error!");
      return false;
   }
   
   mYuvBuffer = (char*)calloc(1, mOrgWidth * mOrgHeight * 3 / 2);
   if (mYuvBuffer==NULL) {
      LOGE("m_yuv_buffer==NULL");
   }
   mFrameCount = 0;
   mFrameFailedCount = 0;
   mVideoTsList.clear();
   return true;
}

void H264Encoder::Destroy()
{
   if (mAvctx) {
      avcodec_close(mAvctx);
      avcodec_free_context(&mAvctx);
      mAvctx = NULL;
   }
   if (mFrame) {
      if (mFrame->data[0]) {
         av_freep(&mFrame->data[0]);
         mFrame->data[0] = NULL;
      }
      av_frame_free(&mFrame);
      mFrame = NULL;
   }
   
   if (mYuvBuffer) {
      free(mYuvBuffer);
      mYuvBuffer = NULL;
   }
   VHALL_DEL(mLiveParam);
}

bool H264Encoder::LiveGetRealTimeStatus(VHJson::Value &value){

	value["Name"] = VHJson::Value("H264Encoder");
	//TODO may need to make it thread safe.
	value["width"] = VHJson::Value(mOrgWidth);
	value["height"] = VHJson::Value(mOrgHeight);
	value["frame_rate"] = VHJson::Value(mFrameRate);
	value["bitrate"] = VHJson::Value(mBitrate);
	value["gop_size"] = VHJson::Value(mAvctx->gop_size);
	value["profile"] = VHJson::Value(mAvctx->profile);

	value["frame_success_count"] = VHJson::Value(mFrameCount);
	value["frame_faild_count"] = VHJson::Value(mFrameFailedCount);
	
	return true;
}

bool H264Encoder::SetVideoQuality(double grade)
{
   return false;
}

int H264Encoder::Encode(const char * indata,
                         int insize,
                         char *outdata,
                         int *p_out_size,
                         int *p_frame_type,
                         uint32_t ts,
                         uint32_t * pts,
                         LiveExtendParam *extendParam)
{
   // yuv format transform,  yuv420sp -> yuv420
   if (mLiveParam->encode_pix_fmt == ENCODE_PIX_FMT_YUV420SP_NV21) {
      Utility::Nv212Planar((unsigned char*)indata, (unsigned char*)mFrame->data[0], mOrgWidth, mOrgHeight);
   }else if(mLiveParam->encode_pix_fmt == ENCODE_PIX_FMT_YUV420SP_NV12){
      Utility::SemiPlanar2Planar((unsigned char*)indata, (unsigned char*)mFrame->data[0], mOrgWidth, mOrgHeight);
   }
   
   AVPacket pkt;
   av_init_packet(&pkt);
   pkt.data = NULL; // packet data will be allocated by the encoder
   pkt.size = 0;
   int got_output = 0;
   int ret = avcodec_encode_video2(mAvctx, &pkt, mFrame, &got_output);
   if (ret < 0) {
      av_packet_unref(&pkt);
      LOGE("Video Encode Error!");
      return ret;
   }
   
   mVideoTsList.push_back(ts);
   
   if (got_output<=0) {
       LOGW("Video Encode got_output：%d",got_output);
      av_packet_unref(&pkt);
	  mFrameFailedCount++;
      return got_output;
   }
   *p_out_size = pkt.size;
   memcpy(outdata, pkt.data, pkt.size);
   av_packet_unref(&pkt);
   
   *pts = mVideoTsList.front();
   mVideoTsList.pop_front();
   
   if (outdata[4] == 0x41) {
      *p_frame_type = VIDEO_P_FRAME;
   } else {
      *p_frame_type = VIDEO_I_FRAME;
   }
   mFrameCount += got_output;
   return got_output;
}

bool H264Encoder::GetSpsPps(char*data,int *size){
   return true;
}
