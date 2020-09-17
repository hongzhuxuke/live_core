#ifndef __VINNY_H264_DECODER_H__
#define __VINNY_H264_DECODER_H__

#include "Decoder.h"
#include <list>
#include <live_sys.h>

#ifdef __cplusplus
extern "C" {
#endif
   
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
   
#ifdef __cplusplus
}
#endif

class H264Decoder : public Decoder{
public:
   H264Decoder();
   H264Decoder(char* extra_data,
               int  extra_size);
   virtual ~H264Decoder();
private:
   void destroy();
public:
   virtual bool Init(int w, int h);
   virtual bool Decode(const char * data, int size, int & decode_size, uint64_t ts = 0);
   virtual bool GetDecodecData(unsigned char * decoded_data, int & decode_size, uint64_t& pts);
   //VideoParam GetVideoParam() const;
private:
   AVCodec           *codec_;
   AVCodecContext    *avctx_;
   AVFrame           *frame_;
   AVPacket           pkt_;
   int                mCodecExtraSize;
   char               *mCodecExtraData;
};

#endif
