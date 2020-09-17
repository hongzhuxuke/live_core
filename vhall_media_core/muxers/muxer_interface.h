//
//  MuxerInterface.h
//  VinnyLive
//
//  Created by ilong on 2016/12/1.
//  Copyright © 2016年 vhall. All rights reserved.
//

#ifndef MuxerInterface_h
#define MuxerInterface_h

#include <string>
#include <stdlib.h>
#include <live_get_status.h>
#include <stdint.h>
#include <safe_buffer_data.h>

enum MuxerMSG{
	MUXER_MSG_START_SUCCESS = 0,
	MUXER_MSG_START_FAILD,
	MUXER_MSG_DUMP_FAILD,
	MUXER_MSG_NEW_KEY_FRAME,
	MUXER_MSG_RECONNECTING,
	MUXER_MSG_BUFFER_EMPTY,
	MUXER_MSG_BUFFER_NORMAL,
	MUXER_MSG_BUFFER_FULL,
   MUXER_MSG_BUFFER_MIN_WARN,
   MUXER_MSG_BUFFER_MAX_WARN,
   
   MUXER_MSG_SIMPLE_HANDSHAKE_FAILED = 4001,
   MUXER_MSG_CONNECT_VHOST_AND_APP_FAILED = 4002,
	//TODO
	MUXER_MSG_PUSH_ERROR_NET_DISCONNECT = 4003,
	MUXER_MSG_PUSH_ERROR_REJECTED_INVALID_TOKEN = 4004,
	MUXER_MSG_PUSH_ERROR_REJECTED_NOT_IN_WHITELIST = 4005,
	MUXER_MSG_PUSH_ERROR_REJECTED_IN_BLACKLIST = 4006,
	MUXER_MSG_PUSH_ERROR_REJECTED_ALREADY_EXISTS = 4007,
	MUXER_MSG_PUSH_ERROR_REJECTED_PROHIBIT = 4008,
	MUXER_MSG_PUSH_ERROR_UNSUPPORTED_RESOLUTION = 4009,
	MUXER_MSG_PUSH_ERROR_UNSUPPORTED_SAMPLERATE = 4010,
   MUXER_MSG_PUSH_ERROR_ARREARAGE = 4011,
};

class MuxerListener{
public:
	MuxerListener(){};
	virtual ~MuxerListener(){};
	virtual int OnMuxerEvent(int type, MuxerEventParam* param){ return 0; };
};

enum MuxerStates{
	
	MUXER_STATE_STARTED = 0,
	MUXER_STATE_STOPED,
	MUXER_STATE_RECONNECTING,
	MUXER_STATE_UNDEFINED
};

enum AVHeaderGetStatues{
	AV_HEADER_NONE = 0,
	AV_HEADER_AUDIO,
	AV_HEADER_VIDEO,
	AV_HEADER_ALL
};

class MuxerInterface:public LiveGetStatus
{
public:
   MuxerInterface(MuxerListener* listener, std::string tag);
   virtual ~MuxerInterface();
   virtual const std::string GetTag();
   virtual const int GetMuxerId();
   virtual const VHMuxerType GetMuxerType();
protected:
	virtual int ReportMuxerEvent(int type, MuxerEventParam *param);
   int     mHeaderWaitCount;
public:
   //sub class should impl the follow func and make sure they are thread safe.
   virtual bool Start(){ return false; };
   virtual bool PushData(SafeData *data){ return false; };
   virtual std::list<SafeData*> Stop() = 0;
   virtual int  GetDumpSpeed(){ return 0; };
   virtual std::string GetDest(){ return ""; };
   virtual int GetState(){ return MUXER_STATE_UNDEFINED; };
   virtual AVHeaderGetStatues GetAVHeaderStatus(){ return AV_HEADER_NONE; };
private:
	const int mId;
	MuxerListener *mListenner;
	const std::string mTag;
};

#endif /* MuxerInterface_h */
