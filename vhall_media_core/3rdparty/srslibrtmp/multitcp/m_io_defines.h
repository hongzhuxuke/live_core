#ifndef __M_DEFINES_H__
#define __M_DEFINES_H__
#include <stdint.h>
#include <list>

#define PACKET_HEADER_SIZE  11 //1+2+4+4
#define PACKET_PAYLOAD_SIZE 1000

typedef enum{
	M_CONN_TYPE_SLAVE = 0,
	M_CONN_TYPE_MASTER = 1,
	M_CONN_TYPE_RESEND = 2
}M_CONN_TYPE;

typedef enum 
{
	M_PACKET_TYPE_UP_DATA = 0,
	M_PACKET_TYPE_MASTR = 1,
	M_PACKET_TYPE_DISCONNECT = 2,
	M_PACKET_TYPE_STATISTICAL = 3,
	M_PACKET_TYPE_RESEND = 4,
	M_PACKET_TYPE_SLAVE = 5
}M_PACKET_TYPE;

class MPacketPool;

class MPacket {
	friend MPacketPool;
public:
	unsigned char type;
	uint16_t payload_size;
	uint32_t global_seq;
	uint32_t send_time;	
	uint32_t recv_time;   //not used on send
	unsigned char *   pay_load;   //= buffer + PACKET_HEADER_SIZE
	unsigned char *   buffer;
	uint32_t buffer_size; 
	bool     resend_flag;
public:	
	void Free();
	void Make();
private:
	MPacket();
	MPacket(int payload_size, MPacketPool *pool);
	~MPacket();
	MPacketPool *pool;
};

class MPacketPool{
private:
	std::list<MPacket*> mFreePacket;
	std::list<MPacket*> mAllPacket;
public:
	MPacketPool(){};
	~MPacketPool();
	MPacket * GetPacket(int size);
	void      FreePacket(MPacket*);

};

void MAKE_PACKET(MPacket *pkt);
	
#endif //__M_DEFINES_H__


