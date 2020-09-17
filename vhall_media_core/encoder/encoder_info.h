#ifndef __ENCODERINFO_H__
#define __ENCODERINFO_H__

class EncoderInfo
{
public:

   EncoderInfo(){};
   
   virtual ~EncoderInfo(){};
	
	virtual int GetResolution() = 0;

	virtual int GetBitrate() = 0;

	virtual bool SetBitrate(int bitrate) = 0;
};

#endif
