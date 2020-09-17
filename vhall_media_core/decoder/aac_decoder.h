#ifndef __VINNY_AAC_DECODER_H__
#define __VINNY_AAC_DECODER_H__

#include "../common/live_define.h"

#ifdef __cplusplus
extern "C" {
#endif
   
#include "libavutil/imgutils.h"
#include "libavutil/opt.h"
#include "libavcodec/avcodec.h"
#include "libavutil/mathematics.h"
#include "libswresample/swresample.h"
#include "libavutil/fifo.h"
   
#ifdef __cplusplus
}
#endif

class AACDecoder {
public:
   AACDecoder(AudioParam *audioParam);
   ~AACDecoder();
public:
   bool Init(void);
   int Decode( unsigned char *data, int size);
   bool GetDecodecData(unsigned char * decoded_data, int & decode_size);
   AudioParam GetAudioParam() const;
private:
   AVCodec        * mCodec;
   AVFrame        * mDecodedFrame;
   AVCodecContext * mCodecCtx;
   AVPacket         mEncodedPkt;
   struct SwrContext* mSwrCtx;
   unsigned char*     mAudioBuff;
   size_t            mAudioBuffSize;
   AVFifoBuffer*      mAudiofifo;
   
   DECLARE_ALIGNED(16,uint8_t,mAudiobuf1)[19200 * 4];
   DECLARE_ALIGNED(16,uint8_t,mAudiobuf2)[19200 * 4];
   
   int mChannels;
   int mSampleRate;
   int mSampleBits;
   
   int   mDecodecSize;
   AudioParam mAudioParam;
   enum AVSampleFormat mDstAudioFmt;
};

#endif
