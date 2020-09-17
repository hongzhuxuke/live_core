//
//  demuxer_interface.h
//  VinnyLive
//
//  Created by ilong on 2017/1/16.
//  Copyright © 2017年 vhall. All rights reserved.
//

#ifndef demuxer_interface_h
#define demuxer_interface_h

class IMediaNotify;
struct LivePlayerParam;

class DemuxerInterface {
   
public:
   DemuxerInterface(){};
   virtual ~DemuxerInterface(){};
   virtual void AddMediaInNotify(IMediaNotify* mediaNotify) = 0;
   virtual void ClearMediaInNotify() = 0;
   virtual void SetParam(LivePlayerParam*param) = 0;
   virtual void Stop() = 0;
   virtual void Start(const char *url) = 0;
   
};

#endif /* demuxer_interface_h */
