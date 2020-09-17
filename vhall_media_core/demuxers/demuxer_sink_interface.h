#ifndef __DEMUXER_SINK_INTERFACE_H__
#define __DEMUXER_SINK_INTERFACE_H__
#include "safe_buffer_data.h"

class DemuxerSinkInterface {
public:
	//means connect ok or open files is ok,we are going to pop Packet.
	virtual bool Init() = 0;

	//means connect destory or read files end, we will not pop Packet. Any More.
	virtual void Destory() = 0;

	//recv AV Packet, adudio AAC, Video H.264
	virtual bool OnAVPacket(SafeData *pkt) = 0;
};

#endif