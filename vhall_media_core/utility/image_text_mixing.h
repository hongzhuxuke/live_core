//
//  image_text_mixing.hpp
//  VhallLiveApi
//
//  Created by ilong on 2017/10/26.
//  Copyright © 2017年 vhall. All rights reserved.
//

#ifndef image_text_mixing_h
#define image_text_mixing_h

#include <stdio.h>
#include <string>
#include <atomic>
#ifdef __cplusplus
extern "C"
{
#endif
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/avutil.h>
#ifdef __cplusplus
};
#endif

//note:this is a sync class

//Instructions for use
// auto imageTextMixing = new ImageTextMixing();
// imageTextMixing->Init(640,480,AV_PIX_FMT_YUV420P);
// imageTextMixing->AppleFilters("vhall"); or imageTextMixing->AppleFilters("drawtext=fontfile=arial.ttf:x=(w-text_w)/2:y=(h-text_h)/2:fontcolor=green:fontsize=50:text='vhall'");
// imageTextMixing->MixingProcess(video_data,data_size);

class ImageTextMixing {
   
public:
   ImageTextMixing();
   ~ImageTextMixing();
   
   /**
    初始化 ImageTextMixing 对象

    @param width 视频宽
    @param height 视频高
    @param in_pixel_format yuv的格式类型
    @return 是否成功 0是成功，非0失败；
    */
   int Init(const int width,const int height,enum AVPixelFormat in_pixel_format);
   
   /**
    设置要融合的文本

    @param text 要融合的文本或者drawtext string
    @return 是否成功 0是成功，非0失败；
    */
   int AppleFilters(const std::string &text);
   
   /**
    处理文本图像融合

    @param input_data 输入的数据
    @param size 输入的数据大小
    @return 是否成功 0是成功，非0失败；
    */
   int MixingProcess(int8_t *input_data,const int size);
   
   /**
    设置显示字体大小
    
    @param font_size 字体大小 defualt=30
    */
   void SetFontSize(int font_size);
   int GetFontSize();
   /**
    设置显示字体样式
    
    @param font_file_path 字体库的路径 defualt=boleyou.ttf
    */
   void SetFontFile(std::string &font_file_path);
   
   /**
    设置字体显示的颜色
    
    @param font_color 字体颜色  eg:green red white... defualt=white
    */
   void SetFontColor(std::string &font_color);
   
   /**
    设置文本显示的位置

    @param position
    */
   void SetTextPositionY(const std::string &position);
   
   /**
    是否开启边界处理

    @param enable true 开启，false 关闭，默认开启
    */
   void SetFixTextBounds(const bool enable);
   
   /**
    设置字体描边的颜色

    @param border_color 颜色值
    */
   void SetFontBorderColor(const std::string &border_color);
   
   /**
    设置字体描边的宽度

    @param w
    */
   void SetFontBorderW(const int w);
private:
   void ShowAVFilterError(int err_num,const std::string&tag);
   void Destory();
private:
   enum AVPixelFormat mInPixelFormat;
   AVFrame *mFrameIn;
   AVFrame *mFrameOut;
   const AVFilter *mBufferSrc;
   const AVFilter *mBufferSink;
   unsigned char *mFrameBufferIn;
   unsigned char *mFrameBufferOut;
   AVFilterContext *mBufferSinkCtx;
   AVFilterContext *mBufferSrcCtx;
   AVFilterGraph *mFilterGraph;
   int mHeight;
   int mWidth;
   std::atomic_bool isInitFinish;
   int  mFontSize; //defualt=30
   std::string mFontFilePath; //defualt=boleyou.ttf
   std::string mFontColor;  //defualt=white
   std::string mTextPosition; //defualt = bottom
   std::string mFontBorderColor; //defualt = black
   int         mFontBorderWidth; //defualt = 1
   bool        mFixTextBounds;   //defualt = true
};

#endif /* image_text_mixing_hpp */
