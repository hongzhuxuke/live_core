#ifndef __VINNY_HW_VIDEO_DECODER_H__
#define __VINNY_HW_VIDEO_DECODER_H__

#include "Decoder.h"

class VhallPlayerInterface;

class HWVideoDecoder:public Decoder {
public:
   HWVideoDecoder(VhallPlayerInterface*vinnyLive);
   ~HWVideoDecoder();
private:
   void destroy();
public:
   virtual bool Init(int w, int h);
   virtual bool Decode(const char * data, int size, int & decode_size, uint64_t ts = 0);
   virtual bool GetDecodecData(unsigned char * decoded_data, int & decode_size, uint64_t& pts);
   
private:
   VhallPlayerInterface *mVinnyLive;
};

#endif
