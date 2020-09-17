//
//  LiveInterface.hpp
//  VinnyLive
//
//  Created by ilong on 2016/11/4.
//  Copyright © 2016年 vhall. All rights reserved.
//

#ifndef LiveInterface_h
#define LiveInterface_h

#include <stdio.h>
#include <string>
#include <live_define.h>

class MediaMuxerInterface;
class MoreCDNSwitch;
class VHallMonitorLog;
class DecodedVideoInfo;

class LiveInterface {
    
public:
    LiveInterface(){};
    virtual ~LiveInterface(){};
    virtual int StartPublish(const char * url) = 0;
    virtual void StopPublish(void) = 0;
};

#endif /* LiveInterface_hpp */
