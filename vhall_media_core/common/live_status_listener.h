//
//  LiveStatusListener.h
//  VinnyLive
//
//  Created by ilong on 2016/11/30.
//  Copyright © 2016年 vhall. All rights reserved.
//

#ifndef LiveStatusListener_h
#define LiveStatusListener_h

#include <string>

class EventParam;

class LiveStatusListener {
    
public:
   LiveStatusListener(){};
   virtual ~LiveStatusListener(){};
   virtual void NotifyEvent(const int type, const EventParam &param) = 0;
};

#endif /* LiveStatusListener_h */
