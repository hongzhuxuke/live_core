#include "muxer_interface.h"

static int ID_COUNT = 0;

MuxerInterface::MuxerInterface(MuxerListener * listener, std::string tag)
	: mId(ID_COUNT++), mTag(tag), mListenner(listener)
{
   
}

int MuxerInterface::ReportMuxerEvent(int type, MuxerEventParam *param){
	param->mId = mId;
	param->mTag = mTag;
	return mListenner->OnMuxerEvent(type, param);
}

const std::string MuxerInterface::GetTag()
{
	return mTag;
}

const int MuxerInterface::GetMuxerId(){
	return mId;
}

const VHMuxerType MuxerInterface::GetMuxerType(){
   return MUXER_NONE;
}

MuxerInterface::~MuxerInterface(){
	//TODO report evnet of destroctor
}
