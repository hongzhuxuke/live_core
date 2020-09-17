//
//  MediaNotify.h
//  VinnyLive
//
//  Created by ilong on 2016/11/30.
//  Copyright © 2016年 vhall. All rights reserved.
//

#ifndef MediaDataSend_h
#define MediaDataSend_h

#include <stdlib.h>

class MediaDataSend {
   
public:
   MediaDataSend(){};
   virtual ~MediaDataSend(){};
   virtual void OnSendVideoData(const char * data, int size, int type, uint64_t timestamp) = 0;
   virtual void OnSendAudioData(const char * data, int size, int type, uint64_t timestamp) = 0;
   virtual void OnSendAmf0Msg(const char * data, int size, int type, uint64_t timestamp) = 0;
};

#endif /* MediaNotify_h */
