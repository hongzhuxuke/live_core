#ifndef MEDIA_QA_INTERFACE_H
#define MEDIA_QA_INTERFACE_H

#include <stdlib.h>
#include <stdint.h>
#include <encode_interface.h>
#include <live_open_define.h>
#include "libvquality.h"


struct LivePushParam;

class MediaQualityInterface: public MQADataInteract, public VideoQualityListener
{
public:
   MediaQualityInterface() {};
   virtual ~MediaQualityInterface() {};
   virtual int Init(LivePushParam *param) = 0;
   virtual bool StartVideoAssess() = 0;
   virtual bool StopVideoAssess() = 0;
   virtual bool InputVideoData(char *ref, char *dif) = 0;
   virtual double GetVideoQuality() = 0;
   virtual bool StartAudioAssess() = 0;
   virtual bool StopAudioAssess() = 0;
   virtual bool InputAudioData(char *ref, char *dif) = 0;
   virtual double GetAudioQuality() = 0;
   virtual bool isInit() = 0;
   //MQADataInteract
   virtual double OnVideoDataInteract(const char* orignal_data, int size0, const char* decode_data, int size1) = 0;
   virtual int OnConfigAdjust(int type, void *param) = 0;
   //VideoQualityListener
   virtual void NotifyEvent(VQEventsType type, void *param = nullptr) = 0;
};

#endif /* MEDIA_QA_INTERFACE_H */
