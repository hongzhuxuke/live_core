#ifndef __VINNY_AAC_ENCODER_H__
#define __VINNY_AAC_ENCODER_H__

#include <list>

#ifdef __cplusplus
extern "C" {
#endif
   
#include "libavutil/imgutils.h"
#include "libavutil/opt.h"
#include "libavcodec/avcodec.h"
#include "libavutil/mathematics.h"
#include "libswresample/swresample.h"

#ifdef __cplusplus
}
#endif

#include "live_get_status.h"

struct LivePushParam;
namespace VHJson {
   class Value;
}
class AACEncoder:public LiveGetStatus
{
public:
   AACEncoder();
   ~AACEncoder();
   bool Init(LivePushParam * param);
   int Encode(const char * in_data,
               int in_size,
               uint64_t in_ts,
               char * out_data,
               int * out_size,
               uint64_t *out_pts);
   virtual bool LiveGetRealTimeStatus(VHJson::Value &value);
   bool GetAudioHeader(char*out_data,int *data_size);
private:
   
   void Destroy();
   
private:
   AVCodec              *mCodec;
   AVCodecContext       *mAvctx;
   AVFrame              *mFrame;
   AVPacket             *mPkt;
   LivePushParam        *mParam;
   struct SwrContext    *mSwrContext;
   enum AVSampleFormat  mDstSampleFmt;
   enum AVSampleFormat  mSrcSampleFmt;
   int                  mFrameCount;
   int                  mFrameFaildCount;
};

#endif
