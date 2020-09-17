#ifndef __VINNY_DECODER_H__
#define __VINNY_DECODER_H__

#include "../common/live_define.h"

class Decoder {
public:
   virtual ~Decoder(){};
   virtual bool Init(int w, int h) = 0;
   virtual bool Decode(const char * data, int size, int & decode_size, uint64_t ts = 0) = 0;
   virtual bool GetDecodecData(unsigned char * decoded_data, int & decode_size, uint64_t& pts) = 0;
   virtual VideoParam GetVideoParam() const{
      return mVideoParam;
   };
protected:
   VideoParam        mVideoParam;
};

#endif
