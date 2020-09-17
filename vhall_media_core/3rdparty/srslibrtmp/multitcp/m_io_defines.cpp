#include "m_io_defines.h"
#include <assert.h>
#include "m_io_sys.h"
MPacket::MPacket()
	:type(0),
	payload_size(0),
	global_seq(0),
	send_time(0),
	recv_time(0),
	pool(NULL),
	buffer_size(0),
	buffer(NULL),
	pay_load(NULL),
	resend_flag(false)
{
}



MPacket::MPacket(int size, MPacketPool *p)
	:type(0),
	payload_size(0),
	global_seq(0),
	send_time(0),
	recv_time(0),
	pool(p)
{
	buffer_size = size + PACKET_HEADER_SIZE;
	buffer = new unsigned char[buffer_size];
	pay_load = buffer + PACKET_HEADER_SIZE;
	resend_flag = false;
}
MPacket::~MPacket()
{
	if (buffer){
		delete[] buffer;
	}
}

void MPacket::Free(){

	type = 0;
	payload_size = 0;
	global_seq = 0;
	send_time = 0;
	recv_time = 0;
	resend_flag = false;
	pool->FreePacket(this);
}

void MPacket::Make(){
	assert(payload_size + PACKET_HEADER_SIZE <= buffer_size);
	buffer[0] = type;
	*(unsigned short*)&buffer[1] = htons(payload_size);
	*(unsigned int*)&buffer[3] = htonl(global_seq);
	*(unsigned int*)&buffer[7] = htonl(send_time);
}

MPacket * MPacketPool::GetPacket(int size)
{
	std::list<MPacket*>::iterator it;
	MPacket* ret = NULL;
	for (it = mFreePacket.begin(); it != mFreePacket.end(); it++){
		if ((*it)->buffer_size >= (size + PACKET_HEADER_SIZE)){
			ret = (*it);
			mFreePacket.erase(it);
			return ret;
		}
	}

	ret = new MPacket(size, this);
	mAllPacket.push_back(ret);
	return ret;
}

void MPacketPool::FreePacket(MPacket* pkt)
{
	mFreePacket.push_front(pkt);
	//delete pkt;
}

MPacketPool::~MPacketPool()
{
	MPacket *pkt;
	while (mAllPacket.size() > 0){
		pkt = mAllPacket.front();
		mAllPacket.pop_front();
		delete pkt;
	}
}
