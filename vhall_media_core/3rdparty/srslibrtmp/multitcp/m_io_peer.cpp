#include "m_io_peer.h"
#include <algorithm>
#include <assert.h>
#include <stdlib.h>
#define MIOPEER_DEFULT_TIMEOUT   -1   //means never timeout
#define ADD_NEW_CONN_TIME_OUT    2000

//每个TCP链接发送的连续次数
#define CONTINUITYCOUNT 2

MIOPeer::MIOPeer(int conn_num, int send_buf_size, int recv_buf_size)
	:mInitConnNum(conn_num),
	mInitSendBufSize(send_buf_size),
	mInitRecvBufSize(recv_buf_size),
	mResendConn(NULL)
{
	m_lock_init(&mReadWriteLock);
	m_cond_init(&mCondRead);
	m_cond_init(&mCondWrite);
	m_lock_init(&mConnListLock);
	isReadWait = false;
	isWriteWait = false;

	_stop = false;

	m_socket mMaxSocket = INVALID_SOCKET;
	mThreadHandle = NULL;
	mStartTime = get_systime_ms();

	mGlobalSendSeq = 0;
	mGlobalRecvSeq = 0;

	mSendTimeout = MIOPEER_DEFULT_TIMEOUT;
	mRecvTimeout = MIOPEER_DEFULT_TIMEOUT;

	mSessionId = 0;

	mServerIp = "";
	mServerPort = -1;

	mSessionPublisher = NULL;

	mPacketPool = new MPacketPool();
	M_IO_Log(M_IO_LOGINFO, "#############################begin###############################");
}
MIOPeer::~MIOPeer()
{
 	M_IO_Log(M_IO_LOGINFO, "~MIOPeer() in");
	_stop = true;
	SyncCloseAllFd();

	if (mThreadHandle){
		m_thread_jion(mThreadHandle);
	}

	mConnSendList.clear();
	mConnRecvList.clear();
	MIOSingleConn *conn = NULL;
	while (mConnList.size() > 0){
		conn = mConnList.front();
		mConnList.pop_front();
		delete conn;	
	}

	MPacket *pkt = NULL;
	while (mRecvPackets.size() > 0){
		pkt = mRecvPackets.front();
		mRecvPackets.pop_front();
		pkt->Free();
	}

	while (mCachePackets.size() > 0){
		pkt = mCachePackets.front();
		mCachePackets.pop_front();
		pkt->Free();
	}

	if (mSessionPublisher)
	{
		publisher_multitcp_session_control_destroy(&mSessionPublisher);
		mSessionPublisher = NULL;
	}

	if (mPacketPool){
		delete mPacketPool;
	}

	m_lock_destroy(&mReadWriteLock);
	m_cond_destroy(&mCondRead);
	m_cond_destroy(&mCondWrite);
	M_IO_Log(M_IO_LOGINFO, "~MIOPeer() out");
}

int MIOPeer::Connect(std::string ip, unsigned short port, int time_ms)
{
	assert(mInitConnNum > 0);
	int left_num = mInitConnNum;

	mServerIp = ip;
	mServerPort = port;

	if (0 != publisher_multitcp_session_control_init(&mSessionPublisher))
	{
		return -1;
	}

	MIOSingleConn *master_conn = new MIOSingleConn(this, mPacketPool);
	if (master_conn->Connect0(ip, port, time_ms) != 0){
		M_IO_Log(M_IO_LOGERROR, "add master conn faild");
		delete master_conn;
		return -1;
	}
	if (master_conn->Connect1(M_CONN_TYPE_MASTER, mSessionId) != 0){
		M_IO_Log(M_IO_LOGERROR, "add master conn faild");
		delete master_conn;
		return -1;
	}

	mConnList.push_back(master_conn);

	master_conn->SetRateControl(mSessionPublisher);

	mMaxSocket = master_conn->GetSock();

	left_num--;

	//add resend conn
	if (left_num > 0 && AddNewConnection(time_ms, true)!= 0){
		M_IO_Log(M_IO_LOGERROR, "add resend conn faild");
	}
	left_num--;

	while (left_num > 0){
		if (AddNewConnection(time_ms) != 0){
			M_IO_Log(M_IO_LOGERROR, "add slave conn faild");
			break;
		}
		left_num--;	
	}

	//TODO start thread
	return CreatSelectThread();
}

int MIOPeer::Write(const char *buffer, int size, int &write_size)
{
	bool stop = _stop;
	if (stop){
		return -1;
	}

	MAutolock l_(&mReadWriteLock);

	while(mSendBuf.size() >= mInitSendBufSize){
		int last_size = mSendBuf.size();
		isWriteWait = true;
		m_cond_wait(&mCondWrite, &mReadWriteLock, mSendTimeout);

		if (last_size > mSendBuf.size() && mSendBuf.size() >= mInitSendBufSize){
			continue;
		}
		break;
	}
	if (mSendBuf.size() >= mInitSendBufSize){
		return -1; //time_out
	}
	mSendBuf.insert(mSendBuf.end(), (unsigned char*)buffer, (unsigned char*)buffer + size);
	write_size = size;
	return 0;
}
int MIOPeer::Read(char *buffer, int buffer_size, int &recv_size)
{
	int ret = 0;
	bool stop = _stop;
	if (stop){
		return -1;
	}

	MAutolock l_(&mReadWriteLock);

	if(mRecvBuf.size() == 0){
		isReadWait = true;
		m_cond_wait(&mCondRead, &mReadWriteLock, mRecvTimeout);
	}
	if (mRecvBuf.size() == 0){
		return -1; //time_out;
	}
	int ret_size = std::min<size_t>(buffer_size, mRecvBuf.size());
	memcpy(buffer, &mRecvBuf.at(0), ret_size);
	mRecvBuf.erase(mRecvBuf.begin(), mRecvBuf.begin() + ret_size);
	recv_size = ret_size;

	return 0;
}
int MIOPeer::ReadN(char *buffer, int buffer_size, int &recv_size)
{
	int ret = 0;
	bool stop = _stop;
	if (stop){
		return -1;
	}

	MAutolock l_(&mReadWriteLock);

	while (mRecvBuf.size() < buffer_size){
		int last_size = mRecvBuf.size();
		isReadWait = true;
		m_cond_wait(&mCondRead, &mReadWriteLock, mRecvTimeout);
		if (last_size < mRecvBuf.size() && mRecvBuf.size() < buffer_size){
			continue;
		}
		break;
	}

	if (mRecvBuf.size() < buffer_size){
		return -1; //time_out;
	}
	memcpy(buffer, &mRecvBuf.at(0), buffer_size);
	mRecvBuf.erase(mRecvBuf.begin(), mRecvBuf.begin() + buffer_size);
	recv_size = buffer_size;

	return 0;
}

int MIOPeer::SetReadTimeout(int time_ms)
{
	mRecvTimeout = time_ms;
	return 0;
}
int MIOPeer::SetWriteTimeout(int time_ms)
{
	mSendTimeout = time_ms;
	return 0;
}
int MIOPeer::GetReadTimeout()
{
	return mRecvTimeout;
}
int MIOPeer::GetWriteTimeout()
{
	return mSendTimeout;
}


void MIOPeer::AddResendPacket(MPacket *pkt)
{
	mCachePackets.push_front(pkt);
}

#define cycnum32_before(t1,t2) ((int32_t)(t2) - (int32_t)(t1) > 0)

bool PackeCompFun(MPacket * pkt1, MPacket *pkt2){

	return cycnum32_before(pkt1->global_seq, pkt2->global_seq);
	//return (pkt1->global_seq < pkt2->global_seq);
}

void MIOPeer::SortCachePackets(){
	mCachePackets.sort(PackeCompFun);
}

void MIOPeer::AddRecvPacket(MPacket *pkt)
{
	mRecvPackets.push_back(pkt);
	//TODO,add it in right way not use this sort
	mRecvPackets.sort(PackeCompFun);

	MAutolock L_(&mReadWriteLock);
	int count = 0;
	MPacket* need_pkt = NULL;
	while (mRecvPackets.size() > 0){
		if (mRecvPackets.front()->global_seq == mGlobalRecvSeq){
			need_pkt = mRecvPackets.front();
			assert(need_pkt->type == M_PACKET_TYPE_UP_DATA);
			mRecvPackets.pop_front();
			mRecvBuf.insert(mRecvBuf.end(), need_pkt->pay_load, need_pkt->pay_load + need_pkt->payload_size);
			need_pkt->Free();
			mGlobalRecvSeq++;
			count++;
			continue;
		}
		break;
	}
	if (count > 0){
		if (isReadWait){
			isReadWait = false;
			m_cond_signal(&mCondRead);
		}
	}
}

int MIOPeer::SendResndPktNow(uint32_t seq){
	std::list<MIOSingleConn*>::iterator it_conn;
	MPacket *pkt = NULL;
	if (!mResendConn){
		return -1;
	}

	for (it_conn = mConnList.begin(); it_conn != mConnList.end(); it_conn++){
		if ((pkt = (*it_conn)->FindPktBySeq(seq,true)) != NULL){
			break;
		}
	}

	if (!pkt){ //check if in mCachePackets;
		std::list<MPacket*>::iterator it_pkt;
		for (it_pkt = mCachePackets.begin(); it_pkt != mCachePackets.end(); it_pkt++){
			if ((*it_pkt)->global_seq == seq){
				pkt = *it_pkt;
				mCachePackets.erase(it_pkt);
				break;
			}
		}		
	}

	if (pkt){
		M_IO_Log(M_IO_LOGINFO, "send resend pkt seq = %I32u payload_size=%d", pkt->global_seq, pkt->payload_size);
		return mResendConn->Write(pkt);
	}
	else {
		M_IO_Log(M_IO_LOGINFO, "find resend pkt faild seq = %I32u ", seq);
		return 0; //not find is not error.
	}
}

void MIOPeer::IsNeedToUpdateMaxSock(m_socket new_sock)
{ 
	if (new_sock > mMaxSocket)
		mMaxSocket = new_sock; 
}


m_thread_ret  __stdcall MIOPeer::SelectThreadFunc(void *peer)
{
	MIOPeer * p = (MIOPeer*)peer;
	return (m_thread_ret)p->Loop();
}

int MIOPeer::PopSendBuf(int size, int pkt_size)
{
	MAutolock l_(&mReadWriteLock);
	MPacket *pkt;

	int pop_size = 0;

	if (mSendBuf.size() > 0){
		int left_size = std::min<size_t>(mSendBuf.size(), size);
		do{
			if (mSendBuf.size() < pkt_size){
				pkt_size = mSendBuf.size();
			}
			pkt = mPacketPool->GetPacket(pkt_size);
			memcpy(pkt->pay_load, &mSendBuf.at(0), pkt_size);
			
			pkt->global_seq = mGlobalSendSeq++;
			pkt->payload_size = pkt_size;
			pkt->resend_flag = false;
			pkt->type = M_PACKET_TYPE_UP_DATA;

			mSendBuf.erase(mSendBuf.begin(), mSendBuf.begin() + pkt_size);
			mCachePackets.push_back(pkt);

			left_size -= pkt_size;
			pop_size += pkt_size;

		} while (left_size > 0);
	}
	if (pop_size > 0){
		if (isWriteWait){
			isWriteWait = false;
			m_cond_signal(&mCondWrite);
		}
	}
	return 0;
}

int MIOPeer::AddNewConnection(int time_ms, bool isResend)
{
	MIOSingleConn *slave_conn = new MIOSingleConn(this, mPacketPool, isResend);
	if (slave_conn->Connect0(mServerIp, mServerPort, time_ms) != 0){
		delete slave_conn;
		return -1;
	}
	if (slave_conn->Connect1(M_CONN_TYPE_SLAVE, mSessionId) != 0){
		delete slave_conn;
		return -1;
	}

	mConnList.push_back(slave_conn);

	slave_conn->SetRateControl(mSessionPublisher);

	if (mMaxSocket < slave_conn->GetSock()){
		mMaxSocket = slave_conn->GetSock();
	}
	if (isResend){
		mResendConn = slave_conn;
	}

	return 0;
}

int MIOPeer::DeleteConnection(MIOSingleConn* conn)
{
	//update mMaxSocket
	if (mMaxSocket == conn->GetSock()){
		mMaxSocket = -1;
		std::list<MIOSingleConn*>::iterator it;
		for (it = mConnList.begin(); it != mConnList.end(); it++){
			
			if (conn->GetSock() > mMaxSocket){
				mMaxSocket = conn->GetSock();
			}
		}
	}

	delete conn;
	return 0;
}


int MIOPeer::UpdataConnections()
{

	MAutolock _Lock(&mConnListLock);

	std::list<MIOSingleConn*>::iterator it;
	MIOSingleConn *conn;
	for (it = mConnList.begin(); it != mConnList.end();){

		if ((*it)->IsNeedClose()){	
			conn = *it;	
			if (conn->isResend()){
				mResendConn = NULL;
			}
			it = mConnList.erase(it);
			DeleteConnection(conn);
			continue;
			
		}
		it++;
	}

	if (isNeedAddConn(&mSessionPublisher)){
		if (mResendConn == NULL){
			AddNewConnection(ADD_NEW_CONN_TIME_OUT, true);
		}
		AddNewConnection(ADD_NEW_CONN_TIME_OUT);
	}
	if (mResendConn){
		return mConnList.size() - 1;
	}
	else {
		return mConnList.size();
	}
}

void MIOPeer::SyncCloseAllFd(){

	MAutolock _Lock(&mConnListLock);
	std::list<MIOSingleConn*>::iterator it;

	for (it = mConnList.begin(); it != mConnList.end(); it++){
		(*it)->SyncCloseFd();
	}
}

bool ConnCompareFunc(MIOSingleConn *conn1, MIOSingleConn *conn2){
	return (conn1->GetLastSendTime() < conn2->GetLastSendTime());
}

void MIOPeer::UpdataSendRecvList()
{
	MIOSingleConn *conn;
	mConnSendList.clear();
	mConnRecvList.clear();
	std::list<MIOSingleConn*>::iterator it;

	MAutolock _Lock(&mConnListLock);

	for (it = mConnList.begin(); it != mConnList.end(); it++){
		if ((*it)->CanRecv()){
			mConnRecvList.push_back(*it);
		}
		if ((*it)->CanSend() && !(*it)->isResend()){ //resend only used to send resend pkt.
			mConnSendList.push_back(*it);
		}
	}
	//TODO　need to sort SendList?
	mConnSendList.sort(ConnCompareFunc);
}

int MIOPeer::CreatSelectThread()
{
	if (m_thread_create(&mThreadHandle, MIOPeer::SelectThreadFunc, this, 0) != 0){
		return -1;
	}
	return 0;
}

int MIOPeer::Loop()
{

	fd_set readFd;
	fd_set writeFd;

	while (!_stop){
		//check if need pop buf
		if (mCachePackets.size() == 0){
			PopSendBuf(MAX_GET_BYTES, PACKET_PAYLOAD_SIZE);
		}

		if (UpdataConnections() <= 0){ //check if have connection
			break;
		}

		UpdataSendRecvList();

		FD_ZERO(&readFd);
		FD_ZERO(&writeFd);

		std::list<MIOSingleConn*>::iterator it_conn ;
		if (mCachePackets.size() > 0){
			for (it_conn = mConnSendList.begin(); it_conn != mConnSendList.end(); it_conn++){
				FD_SET((*it_conn)->GetSock(), &writeFd);
			}
		}

		for (it_conn = mConnRecvList.begin(); it_conn != mConnRecvList.end(); it_conn++){
			FD_SET((*it_conn)->GetSock(), &readFd);
		}

		struct timeval tv = { 0, 500000 };
		int ret = select(mMaxSocket, &readFd, &writeFd, NULL, &tv);

		if (ret == 0){
			continue;
		}

		for (it_conn = mConnRecvList.begin(); it_conn != mConnRecvList.end(); it_conn++){
			
			if (FD_ISSET((*it_conn)->GetSock(), &readFd)){
				(*it_conn)->Recv();
			}				
		}


		if (mCachePackets.size() > 0){
			for (it_conn = mConnSendList.begin(); it_conn != mConnSendList.end(); it_conn++)
			{
				if (FD_ISSET((*it_conn)->GetSock(), &writeFd)){
					
					int send_packet = 0;
					while (mCachePackets.size() > 0 && CONTINUITYCOUNT > send_packet++){
						MPacket * pkt = mCachePackets.front();
						mCachePackets.pop_front();
						if ((*it_conn)->Write(pkt) != 0){
							break;
						}
					}		
				}
			}
		} 
	}
	M_IO_Log(M_IO_LOGINFO, "out le!!!!!!!!!!", send);
	return 0;
}
