#ifndef __M_IO_PEER_H__
#define __M_IO_PEER_H__

#include "m_io_sys.h"
#include "m_io_defines.h"
#include <list>
#include <vector>
#include "m_io_single_conn.h"
#include <atomic>
#include "m_io_rate_control.h"

#define MAX_GET_BYTES 655360     //5M bits

class MIOSingleConn;
class MIOPeer{

private:
	m_lock_t  mReadWriteLock;
	m_cond_t  mCondRead;
	m_cond_t  mCondWrite;
	m_lock_t  mConnListLock;
	std::atomic_bool isReadWait;
	std::atomic_bool isWriteWait;

	m_thread_handle mThreadHandle;

	std::list<MIOSingleConn*> mConnList;
	std::list<MIOSingleConn*> mConnSendList;
	std::list<MIOSingleConn*> mConnRecvList;

	int mInitConnNum;

	std::atomic_bool _stop;
	std::atomic_bool _isConnected;

	m_socket mMaxSocket;

	uint64_t mStartTime;

	std::list<MPacket*> mRecvPackets;

	std::list<MPacket*> mCachePackets;

	unsigned int mGlobalSendSeq;
	unsigned int mGlobalRecvSeq;

	std::vector<unsigned char> mRecvBuf;
	std::vector<unsigned char> mSendBuf;
	int mInitSendBufSize;
	int mInitRecvBufSize;

	int mSendTimeout;
	int mRecvTimeout;

	uint32_t mSessionId;
	std::string mServerIp;
	unsigned short mServerPort;

	session_publisher mSessionPublisher;
	MPacketPool *mPacketPool;

	MIOSingleConn *mResendConn;
public:
	MIOPeer(int conn_num, int send_buf_size, int recv_buf_size);
	~MIOPeer();
	int Connect(std::string ip, unsigned short port, int time_ms = -1);

	int Write(const char *buffer, int size, int &write_size);
	int Read(char *buffer, int buffer_size, int &recv_size);
	int ReadN(char *buffer, int buffer_size, int &recv_size);

	int SetReadTimeout(int time_ms);
	int SetWriteTimeout(int time_ms);
	int GetReadTimeout();
	int GetWriteTimeout();

	void AddResendPacket(MPacket *pkt);
	void SortCachePackets();
	void AddRecvPacket(MPacket *pkt);
	uint64_t GetStartTime(){ return mStartTime; };

	//add for resend
	int SendResndPktNow(uint32_t seq);
	std::string GetDestIp(){ return mServerIp; };
	unsigned short GetDestPort(){ return mServerPort; };
	void IsNeedToUpdateMaxSock(m_socket new_sock);

private:
	int CreatSelectThread();
	int Loop();
	int PopSendBuf(int pop_size, int pkt_size);
	static m_thread_ret __stdcall SelectThreadFunc(void *);

	//return Curent connection alive.
	int UpdataConnections();
	void UpdataSendRecvList();
	void SyncCloseAllFd();

	int AddNewConnection(int time_ms, bool isResend = false);
	int DeleteConnection(MIOSingleConn* conn);
};


#endif // __M_IO_PEER_H__
