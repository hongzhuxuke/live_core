#ifndef __LIVE_OBS_H__
#define __LIVE_OBS_H__

#include <string>
#include "live_define.h"

class DecodedVideoInfo {

public:
   int mWidth;
   int mHeight;
   int mSize;
   int mMediaFormat;
   unsigned long long mTS;
   char * mData;
};

#define VHALL_COLOR_FormatYUV420SemiPlanar  21
#define VHALL_COLOR_FormatYUV420Planar 19
#define VHALL_COLOR_FormatYUV420PackedSemiPlanar32m 0x7FA30C04

class LiveObs {
public:
   LiveObs(){};
   virtual ~LiveObs(){};
   /**
    *  事件回调
    *
    *  @param type    事件类型
    *  @param content 事件内容
    *
    *  @return 是否成功
    */
   virtual int OnEvent(int type, const std::string content) = 0;
   /**
    将事件线程从JVM中分离,android使用可不实现

    @return 0是成功，-1失败
    */
   virtual int OnJNIDetachEventThread(){return 0;};
   /**
    *  抛出原始视频数据
    *
    *  @param data 视频数据
    *  @param size 数据大小
    *  @param w    视频宽
    *  @param h    视频高
    *
    *  @return 是否成功
    */
   virtual int OnRawVideo(const char *data, int size, int w, int h) = 0;
   
   /**
    将视频渲染线程从JVM中分离,android使用可不实现

    @return 0是成功，-1失败
    */
   virtual int OnJNIDetachVideoThread(){return 0;};
   /**
    *  抛出原始音频数据
    *
    *  @param data 音频PCM数据
    *  @param size 音频数据大小
    *
    *  @return 是否成功
    */
   virtual int OnRawAudio(const char *data, int size) = 0;
   /**
    将视频渲染线程从JVM中分离,android使用可不实现
    
    @return 0是成功，-1失败
    */
   virtual int OnJNIDetachAudioThread(){return 0;};
   
   virtual int OnHWDecodeVideo(const char *data, int size, int w, int h, int64_t ts) = 0;
   
   virtual DecodedVideoInfo * GetHWDecodeVideo() = 0;
      
};

#endif
