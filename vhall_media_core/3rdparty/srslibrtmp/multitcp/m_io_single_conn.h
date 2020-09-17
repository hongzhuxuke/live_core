#ifndef __M_SIGLE_CONN__
#define __M_SIGLE_CONN__

#include "m_io_sys.h"
#include <string>
#include <stdint.h>
#include "m_io_defines.h"
#include "m_io_rate_control.h"
#include "m_io_peer.h"
#include "m_io_socket.h"

#define RECV_CACHE_BUFFER_SIZE  (16*1024)  //better be more than recv_buf in sock
class MIOPeer;
class MIOSingleConn
{
private:
	m_socket mSocket;
	uint32_t mSessionId;
	connection_publisher mConnectionPublisher;
	session_publisher    mSessionPublisher;
	MIOPeer*              mPeer;

	std::list<MPacket*> mSendCachePackets;
	int                 mSendCacheSize;
	
	bool mErrorFlag;
	bool mUseRateControl;

	MPacketPool *mPool;
	uint64_t mLastSendTime;
	char mRecvCacheBuf[RECV_CACHE_BUFFER_SIZE];
	int  mReavCacheBufLastPos;
	std::list<MPacket*> mRecvCachePackets;

	int mSockSendBufSize;
	int mSockRecvBufSize;

public:
	MIOSingleConn(MIOPeer *peer, MPacketPool* pool, bool isResend = false);
	~MIOSingleConn();
	int SetRateControl(session_publisher ps);
   struct addrinfo * DnsResolve(std::string host,std::string port);
	int Connect0(std::string ip, unsigned short port, int timeout_ms = -1);
	int Connect1(M_CONN_TYPE type, uint32_t &session_id);
	m_socket GetSock() { return mSocket; }

	void SyncCloseFd() { m_socket_close(mSocket); };
	bool CanSend();
	bool CanRecv();
	bool IsNeedClose();
	int  Recv();
	int  Write(MPacket* pkt);
	uint64_t GetLastSendTime() { return mLastSendTime; }

	int SetSockSendBufSize(int size);
	int SetSockRecvBufSize(int size);
	int GetSockSendBufSize(int &size);
	int GetSockRecvBufSize(int &size);

	//resend add 
	bool mIsResend;
	MPacket* FindPktBySeq(uint32_t seq, bool is_remove_from_cache = true);
	int isResend(){ return mIsResend; };
	//int Reconnect();

private:
	void Close();

	void AddResenPacket();
	void RemoveResendPacket();
	virtual int RecvToCache();
	virtual MPacket* GetRecvPacket();

	MPacket* recv_packet();
	int  send_packet(MPacket* pkt);
};

#endif //__M_SIGLE_CONN__
