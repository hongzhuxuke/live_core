//
//  Encoder.hpp
//  VinnyLive
//
//  Created by ilong on 2016/11/7.
//  Copyright © 2016年 vhall. All rights reserved.
//

#ifndef Encoder_hpp
#define Encoder_hpp

#include <stdlib.h>
#include <live_get_status.h>
#include <stdint.h>
#include "../ratecontrol/rate_control_interface.h"

class LiveStatusListener;
class MediaDataSend;
class MQADataInteract;
struct LivePushParam;
struct LiveExtendParam;
class  RateControl;

class MQADataInteract
{
public:
   MQADataInteract() {};
   virtual ~MQADataInteract() {};

   virtual double OnVideoDataInteract(const char* orignal_data, int size0, const char* decode_data, int size1) = 0;
   virtual int OnConfigAdjust(int type, void *param = nullptr) = 0;

   enum {
      MQA_VIDEO_ENCODE_BUSY,
      MQA_VIDEO_ENCODE_OK
   };
private:
   
};

class EncodeInterface : public LiveGetStatus , public EnocoderRateController
{
    
public:
    EncodeInterface(){};
    virtual ~EncodeInterface(){};
    virtual int LiveSetParam(LivePushParam *param) = 0;
    virtual void SetStatusListener(LiveStatusListener * listener) = 0;
    virtual void SetOutputListener(MediaDataSend * outputListener) = 0;
	virtual void SetMediaQAListener(MQADataInteract * listener) = 0;
    virtual void EncodeVideo(const char * data, int size, uint64_t timestamp, const LiveExtendParam *extendParam=NULL) = 0;
    virtual void EncodeVideoHW(const char * data, int size ,int type, uint64_t timestamp) = 0;
    virtual void EncodeAudio(const char * data, int size, uint64_t timestamp) = 0;
    virtual void EncodeAudioHW(const char * data, int size, uint64_t timestamp) = 0;
    virtual bool RequestKeyframe() = 0;
    virtual void Start() = 0;
    virtual void Stop() = 0;
    virtual bool isInit() = 0;
	 virtual void SetVideoQualityGrade(double grade) = 0;
    virtual bool RateControlAdjust(NetworkState state) {return true;};
};

#endif /* Encoder_hpp */
