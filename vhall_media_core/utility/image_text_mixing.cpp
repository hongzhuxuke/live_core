//
//  image_text_mixing.cpp
//  VhallLiveApi
//
//  Created by ilong on 2017/10/26.
//  Copyright © 2017年 vhall. All rights reserved.
//

#include "image_text_mixing.h"
#include "../common/vhall_log.h"

#ifdef __cplusplus
extern "C"
{
#endif
#include <libavutil/error.h>
#include <libavutil/log.h>
#include <libavutil/imgutils.h>
#ifdef __cplusplus
};
#endif

const static char *filter_format = "drawtext=fontfile=%s:x=(w-text_w)/2:y=%s:fontcolor=%s:fontsize=%d:borderw=%d:fix_bounds=%s:bordercolor=%s:text='%s'";

ImageTextMixing::ImageTextMixing():
mFrameIn(NULL),
mFrameOut(NULL),
mFilterGraph(NULL),
mFrameBufferIn(NULL),
mFrameBufferOut(NULL),
mBufferSrc(NULL),
mBufferSrcCtx(NULL),
mBufferSinkCtx(NULL),
mBufferSink(NULL){
   mWidth = 0;
   mHeight = 0;
   mFontSize = 30;
   mFontFilePath = "boleyou.ttf";
   mFontColor = "white";
   mTextPosition = "h-text_h*2"; //"h-text_h*2";
   mFontBorderColor = "black";
   mFixTextBounds = true;
   mFontBorderWidth = 1;
   isInitFinish = false;
   avfilter_register_all();
   //av_log_set_level(AV_LOG_TRACE);
}

ImageTextMixing::~ImageTextMixing(){
   Destory();
}

void ImageTextMixing::Destory(){
   if (mFrameIn) {
      av_frame_free(&mFrameIn);
      mFrameIn = NULL;
   }
   if (mFrameOut) {
      av_frame_free(&mFrameOut);
      mFrameOut = NULL;
   }
   if (mFilterGraph) {
      avfilter_graph_free(&mFilterGraph);
      mFilterGraph = NULL;
   }
   if (mFrameBufferIn) {
      av_free(mFrameBufferIn);
      mFrameBufferIn = NULL;
   }
   if (mFrameBufferOut) {
      av_free(mFrameBufferOut);
      mFrameBufferOut = NULL;
   }
}

int ImageTextMixing::Init(const int width,const int height,enum AVPixelFormat in_pixel_format){
   mWidth = width;
   mHeight = height;
   mInPixelFormat = in_pixel_format;
   
   mBufferSrc = avfilter_get_by_name("buffer");
   mBufferSink = avfilter_get_by_name("buffersink");
   
   if (mFrameIn==NULL) {
      mFrameIn=av_frame_alloc();
   }
   if (mFrameBufferIn) {
      av_free(mFrameBufferIn);
      mFrameBufferIn = NULL;
   }
   int dataSzie =av_image_get_buffer_size(in_pixel_format, width,height,1);
   mFrameBufferIn=(unsigned char *)av_malloc(dataSzie);
   int ret = av_image_fill_arrays(mFrameIn->data, mFrameIn->linesize,mFrameBufferIn,in_pixel_format,width, height,1);
   if (ret<0)
   {
      ShowAVFilterError(ret,"av_image_fill_arrays in");
      return ret;
   }
   
   if (mFrameOut==NULL) {
      mFrameOut = av_frame_alloc();
   }
   if (mFrameBufferOut) {
      av_free(mFrameBufferOut);
      mFrameBufferOut = NULL;
   }
   mFrameBufferOut=(unsigned char *)av_malloc(av_image_get_buffer_size(in_pixel_format, width,height,1));
   ret = av_image_fill_arrays(mFrameOut->data, mFrameOut->linesize,mFrameBufferOut,in_pixel_format,width, height,1);
   if (ret<0)
   {
      ShowAVFilterError(ret, "av_image_fill_arrays in");
      return ret;
   }
   
   mFrameIn->width=width;
   mFrameIn->height=height;
   mFrameIn->format=in_pixel_format;
   return 0;
}

void ImageTextMixing::SetFontSize(int font_size){
   mFontSize = font_size;
}

int ImageTextMixing::GetFontSize(){
   return mFontSize;
}

void ImageTextMixing::SetFontFile(std::string&font_file_path){
   mFontFilePath = font_file_path;
}

void ImageTextMixing::SetFontColor(std::string&font_color){
   mFontColor = font_color;
}

void ImageTextMixing::SetTextPositionY(const std::string &position){
   mTextPosition = position;
}

void ImageTextMixing::SetFixTextBounds(const bool enable){
   mFixTextBounds = enable;
}

void ImageTextMixing::SetFontBorderColor(const std::string &border_color){
   mFontBorderColor = border_color;
}

void ImageTextMixing::SetFontBorderW(const int w){
   mFontBorderWidth = w;
}

int ImageTextMixing::AppleFilters(const std::string&text){
   int ret = 0;
   char args[512] = { 0 };
   char filter_descr[4096] = { 0 };
   enum AVPixelFormat pix_fmts[] = { mInPixelFormat, AV_PIX_FMT_NONE };
   AVFilterInOut * inputs = nullptr;
   AVFilterInOut * outputs = nullptr;
   
   isInitFinish = false;
   if (mFilterGraph) {
      avfilter_graph_free(&mFilterGraph);
      mFilterGraph = NULL;
   }
   mFilterGraph = avfilter_graph_alloc();
   if (!mFilterGraph) {
      LOGE("avfilter_graph_alloc,AVERROR(ENOMEM):%d", AVERROR(ENOMEM));
      return -1;
   }
   /* buffer video source: the decoded frames from the decoder will be inserted here. */
   snprintf(args, sizeof(args),
            "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
            mWidth,mHeight,mInPixelFormat,
            1, 25,1,1);
   
   ret = avfilter_graph_create_filter(&mBufferSrcCtx, mBufferSrc, "in",args, NULL, mFilterGraph);
   if (ret < 0) {
      isInitFinish = false;
      LOGE("Cannot create buffer source!");
      return ret;
   }
   
   /* buffer video sink: to terminate the filter chain. */
   AVBufferSinkParams *buffersink_params = av_buffersink_params_alloc();
   buffersink_params->pixel_fmts = pix_fmts;
   ret = avfilter_graph_create_filter(&mBufferSinkCtx, mBufferSink, "out",NULL, buffersink_params, mFilterGraph);
   av_free(buffersink_params);
   
   if (ret < 0) {
      ShowAVFilterError(ret,"Cannot create buffer sink.");
      isInitFinish = false;
      return ret;
   }
   
   outputs = avfilter_inout_alloc();
   if (outputs==NULL) {
      LOGE("avfilter_inout_alloc() out error.");
      isInitFinish = false;
      ret = -1;
      goto end;
   }
   
   inputs = avfilter_inout_alloc();
   if (inputs==NULL) {
      LOGE("avfilter_inout_alloc() in error.");
      isInitFinish = false;
      ret = -2;
      goto end;
   }
   
   /* Endpoints for the filter graph. */
   outputs->name       = av_strdup("in");
   outputs->filter_ctx = mBufferSrcCtx;
   outputs->pad_idx    = 0;
   outputs->next       = NULL;
   
   inputs->name       = av_strdup("out");
   inputs->filter_ctx = mBufferSinkCtx;
   inputs->pad_idx    = 0;
   inputs->next       = NULL;
   
   if (text.find("drawtext=",0)==std::string::npos){
      std::string fix_bounds;
      if (mFixTextBounds) {
         fix_bounds = "true";
      }else{
         fix_bounds = "false";
      }
	  snprintf(filter_descr, sizeof(filter_descr), filter_format, mFontFilePath.c_str(), mTextPosition.c_str(), mFontColor.c_str(), mFontSize,mFontBorderWidth,fix_bounds.c_str(),mFontBorderColor.c_str(), text.c_str());
   }else{
	   memcpy(filter_descr, text.c_str(),text.length());
   }

   if ((ret = avfilter_graph_parse_ptr(mFilterGraph, filter_descr, &inputs, &outputs, NULL)) < 0){
      ShowAVFilterError(ret,"avfilter_graph_parse_ptr");
      isInitFinish = false;
      goto end;
   }
   
   if ((ret = avfilter_graph_config(mFilterGraph, NULL)) < 0){
      ShowAVFilterError(ret,"avfilter_graph_config");
      isInitFinish = false;
      goto end;
   }
   isInitFinish = true;
end:
   if (inputs) {
      avfilter_inout_free(&inputs);
      inputs = nullptr;
   }
   if (outputs) {
      avfilter_inout_free(&outputs);
      outputs = nullptr;
   }
   return ret;
}

int ImageTextMixing::MixingProcess(int8_t *input_data,const int size){
   if (isInitFinish==false) {
      return -1;
   }
   if (input_data == NULL || size <= 0 || mBufferSrcCtx == NULL || mBufferSinkCtx == NULL) {
      LOGE("input_data==NULL||output_data==NULL||size<=0, param error");
      return -2;
   }
   int8_t *inputData = input_data;
   int ret = 0;
   memcpy(mFrameBufferIn, inputData, size);
   if ((ret = av_buffersrc_add_frame_flags(mBufferSrcCtx, mFrameIn, AV_BUFFERSRC_FLAG_PUSH)) < 0) {
      ShowAVFilterError(ret,"Error while add frame.");
      /* re-init */
      isInitFinish = false;
      return ret;
   }
   /* pull filtered pictures from the filtergraph */
   while (1) {
      ret = av_buffersink_get_frame_flags(mBufferSinkCtx, mFrameOut, AV_BUFFERSINK_FLAG_NO_REQUEST);
      if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
         break;
      }
      if (ret < 0)
      {
         /* re-init */
         isInitFinish = false;
         ShowAVFilterError(ret, "Error while get frame.");
         break;
      }
      // output Y,U,V
      int decodedSize = av_image_get_buffer_size((AVPixelFormat)mFrameOut->format,
                                                 mFrameOut->width,
                                                 mFrameOut->height,
                                                 1);
      av_image_copy_to_buffer((unsigned char*)input_data,
                              decodedSize,
                              mFrameOut->data,
                              mFrameOut->linesize,
                              (AVPixelFormat)mFrameOut->format,
                              mFrameOut->width,
                              mFrameOut->height,
                              1);
      av_frame_unref(mFrameOut);
   }
   return 0;
}

void ImageTextMixing::ShowAVFilterError(int err_num,const std::string&tag){
   char errorstr[256] = {0};
   av_strerror(err_num,errorstr,sizeof(errorstr));
   LOGE("%s error:%s err_num:%d",tag.c_str(),errorstr,err_num);
}
