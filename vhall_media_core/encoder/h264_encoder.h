#ifndef __VINNY_H264_ENCODER_H__
#define __VINNY_H264_ENCODER_H__

#include <list>
#include "../encoder/avc_encode_interface.h"
#ifdef __cplusplus
extern "C" {
#endif
   
#include "libavutil/opt.h"
#include "libavcodec/avcodec.h"
#include "libavutil/common.h"
#include "libavutil/imgutils.h"
#include "libavutil/mathematics.h"
#include "libavutil/samplefmt.h"

#ifdef __cplusplus
}
#endif

struct LivePushParam;
struct LiveExtendParam;
namespace VHJson {
   class Value;
}
class H264Encoder : public AVCEncodeInterface{
public:
   H264Encoder();
   ~H264Encoder();
   virtual bool Init(const LivePushParam * param);
  // @return On error a negative value is returned, on success zero or the number * of bytes used from the output buffer.
   virtual int Encode(const char * indata,
               int insize,
               char * outdata,
               int * p_out_size,
               int * p_frame_type,
               uint32_t ts,
               uint32_t * pts,LiveExtendParam *extendParam=NULL);
   virtual bool GetSpsPps(char*data,int *size);
   virtual bool LiveGetRealTimeStatus(VHJson::Value &value);
   virtual bool SetVideoQuality(double grade);
private:
   void Destroy();
private:
   AVCodec                 *mCodec;
   AVCodecContext          *mAvctx;
   AVFrame                 *mFrame;
   LivePushParam           *mLiveParam;
   std::list<uint32_t>     mVideoTsList;
   char                    *mYuvBuffer;
   int mOrgWidth;
   int mOrgHeight;
   int mFrameRate;
   int mFrameCount;
   int mFrameFailedCount;
   float mKeyframeInterval;
   int mBitrate;
   int Ori;
};

#endif
