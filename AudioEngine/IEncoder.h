#ifndef _AUDIO_ENCODER_INCLUDE__
#define _AUDIO_ENCODER_INCLUDE__


struct DataPacket {
   LPBYTE lpPacket;
   UINT size;
};

class IAudioEncoder {  
   

public:
   virtual ~IAudioEncoder() {}
   virtual void    GetHeaders(DataPacket &packet) = 0;
   virtual bool    Encode(float *input, UINT numInputFrames, DataPacket &packet, QWORD &timestamp) = 0;
   virtual UINT    GetFrameSize() const = 0;

   virtual int     GetBitRate() const = 0;
   virtual CTSTR   GetCodec() const = 0;

   virtual String  GetInfoString() const = 0;
};


IAudioEncoder* CreateAACEncoder(UINT bitRate, UINT sample, UINT Channel);

#endif