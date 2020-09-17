#include "m_io_single_conn.h"
#include "m_io_socket.h"
#include "m_io_defines.h"
#include "m_io_rate_control.h"
#include <assert.h>

#define DEFAULT_SNDBUF 65536
#define DEFAULT_REVBUF 65536

MIOSingleConn::MIOSingleConn(MIOPeer *peer, MPacketPool* pool, bool isResend)
	:mPeer(peer),
	mPool(pool),
	mIsResend(isResend)
{
	mSocket = INVALID_SOCKET;
	mSessionId = -1;
	mConnectionPublisher = NULL;
	mSessionPublisher = NULL;
	mErrorFlag = false;
	mLastSendTime = 0;

	mReavCacheBufLastPos = 0;
	mSockSendBufSize = -1;
	mSockRecvBufSize = -1;
	mSendCacheSize = 0;
}

MIOSingleConn::~MIOSingleConn()
{
	Close();
}

int MIOSingleConn::SetRateControl(session_publisher ps)
{
	if (mSocket == INVALID_SOCKET){
		return -1;
	}

	mSessionPublisher = ps;

	int id = mSocket;

	if (0 != publisher_multitcp_connection_control_init(&id, &mConnectionPublisher)){
		mSessionPublisher = NULL;
		mConnectionPublisher = NULL;
		return -1;
	}
	if (0 != add_connection(&mConnectionPublisher, &mSessionPublisher)){
		mSessionPublisher = NULL;
		mConnectionPublisher = NULL;
		return -1;
	}

	if (mIsResend){
		setStandbyCon(&mConnectionPublisher, 1);
	}

	mUseRateControl = true;
	return 0;
}

struct addrinfo * MIOSingleConn::DnsResolve(std::string host,std::string port)
{
   struct addrinfo *answer, hint;
   memset(&hint,0,sizeof(hint));
   hint.ai_family = AF_UNSPEC;
   hint.ai_socktype = SOCK_STREAM;
   int ret = getaddrinfo(host.c_str(), port.c_str(), &hint, &answer);
   if (ret != 0) {
      return NULL;
   }
   return answer;
}

int MIOSingleConn::Connect0(std::string ip, unsigned short port, int timeout_ms)
{
	int ret = 0;
	//获取网络协议
   char portChar[16];
   sprintf(portChar, "%d",port);
   std::string portStr(portChar);
   struct addrinfo *info = DnsResolve(ip, portStr);
   struct addrinfo *result = NULL;
	mSocket = m_socket_tcp(info,&result);

	if ((ret = m_socket_connect_timeo(mSocket, result, timeout_ms)) != 0){
      if (info) {
         freeaddrinfo(info);
      }
		return ret;
	}
   if (info) {
      freeaddrinfo(info);
   }
   
	if ((ret = SetSockSendBufSize(DEFAULT_SNDBUF)) != 0){
		return ret;
	}
	if ((ret = SetSockSendBufSize(DEFAULT_SNDBUF)) != 0){
		return ret;
	}

	if ((ret = m_socket_set_block(mSocket)) != 0){
		return ret;
	}

	if ((ret = m_socket_set_linger(mSocket)) != 0){
		return ret;
	}

	if (mIsResend){
		u_long on = 1;
		setsockopt(mSocket, SOL_SOCKET, TCP_NODELAY, (const char*)&on, sizeof(u_long));
		setsockopt(mSocket, SOL_SOCKET, SO_KEEPALIVE, (const char*)&on, sizeof(u_long));
	}

	return ret;
}

MPacket* MIOSingleConn::recv_packet(){
	char header_buf[PACKET_HEADER_SIZE];
	MPacket* pkt = NULL;
	int ret = 0;
	if ((ret = m_socket_readfull(mSocket, header_buf, PACKET_HEADER_SIZE)) != PACKET_HEADER_SIZE){
		mErrorFlag = true;
		M_IO_Log(M_IO_LOGERROR, "read multitcp header error  ret=%d error=%d", ret, socket_errno);
		return NULL;
	}

	int payload_size = ntohs(*(unsigned short*)(&header_buf[1]));
	pkt = mPool->GetPacket(payload_size);

	if (payload_size > 0){
		if ((ret = m_socket_readfull(mSocket, (char*)pkt->pay_load, payload_size)) != payload_size){
				mErrorFlag = true;
				pkt->Free();
				M_IO_Log(M_IO_LOGERROR, "read multitcp body error payload_size=%d ret=%d error=%d", 
					payload_size, ret, socket_errno);
				return NULL;
		}
	}

	memcpy(pkt->buffer, header_buf, PACKET_HEADER_SIZE);
	pkt->type = header_buf[0];
	pkt->payload_size = payload_size;
	pkt->global_seq = htonl(*(unsigned int*)(&header_buf[3]));
	pkt->send_time = htonl(*(unsigned int*)(&header_buf[7]));
	pkt->recv_time = get_systime_ms() - mPeer->GetStartTime();
	return pkt;
}

int  MIOSingleConn::send_packet(MPacket* pkt)
{

	mLastSendTime = get_systime_ms();

	if (pkt->type == M_PACKET_TYPE_UP_DATA){
		mSendCachePackets.push_back(pkt);
		mSendCacheSize += (pkt->payload_size + PACKET_HEADER_SIZE);
		if (!pkt->resend_flag){
			pkt->send_time = mLastSendTime - mPeer->GetStartTime();
		}
	}

	pkt->Make();

	if (m_socket_sendfull(mSocket, (char*)pkt->buffer, pkt->payload_size + PACKET_HEADER_SIZE)
		!= pkt->payload_size + PACKET_HEADER_SIZE){

		M_IO_Log(M_IO_LOGERROR, "send multitcp pkt error pkt_size=%d error=%d", 
			pkt->payload_size + PACKET_HEADER_SIZE, socket_errno);
		mErrorFlag = true;	
		return -1;
	}

	return 0;
}

int MIOSingleConn::Connect1(M_CONN_TYPE type, uint32_t &session_id)
{
	int ret = 0;
	MPacket *packet = mPool->GetPacket(0);
	if (type != M_CONN_TYPE_MASTER)
	{
		packet->type = M_PACKET_TYPE_SLAVE;
		packet->send_time = session_id;
	}
	else
	{
		packet->type = M_PACKET_TYPE_MASTR;
		packet->send_time = 0;
	}
	packet->payload_size = 0;
	packet->global_seq = 0;

	if (Write(packet) != 0){
		packet->Free();
		return -1;
	}

	packet->Free();

	MPacket * res_packet;
	if ((res_packet = recv_packet()) == NULL){
		return -1;
	}

	if (res_packet->global_seq != 1){
		res_packet->Free();
		return -1;
	}

	if (type == M_PACKET_TYPE_MASTR){
		session_id = res_packet->send_time;
	}

	res_packet->Free();
	mSessionId = session_id;

	return 0;
}

void MIOSingleConn::RemoveResendPacket(){
	int buf_size = 0;
	if (GetSockSendBufSize(buf_size) != 0){
		mErrorFlag = true;
		return;
	}
	
	while (mSendCachePackets.size() > 1){
		MPacket *pkt = mSendCachePackets.front();
		if (mSendCacheSize - (pkt->payload_size + PACKET_HEADER_SIZE) > buf_size){
			mSendCachePackets.pop_front();
			mSendCacheSize -= (pkt->payload_size + PACKET_HEADER_SIZE);
			pkt->Free();
			continue;
		}
		break;
	}
}


void MIOSingleConn::AddResenPacket()
{
	while (mSendCachePackets.size() > 0){
		MPacket *pkt = mSendCachePackets.back();
		assert(pkt->type == M_PACKET_TYPE_UP_DATA);
		pkt->resend_flag = true;
		mSendCachePackets.pop_back();
		mPeer->AddResendPacket(pkt);
	}
	//make sure small seq is first send.
	mPeer->SortCachePackets();
}

void MIOSingleConn::Close()
{
	m_socket_close(mSocket);

	while (mRecvCachePackets.size() > 0){
		MPacket *pkt = mRecvCachePackets.back();
		mRecvCachePackets.pop_back();
		if (pkt->type == M_PACKET_TYPE_UP_DATA){
			mPeer->AddRecvPacket(pkt);
		}
	}

	AddResenPacket();

	if (mConnectionPublisher){
		publisher_multitcp_connection_control_destroy(&mConnectionPublisher);
		mConnectionPublisher = NULL;
	}
}

bool MIOSingleConn::CanSend()
{
	if (mErrorFlag){
		return false;
	}
	if (mUseRateControl && !isSend(&mConnectionPublisher, &mSessionPublisher)){
		return false;
	}
	return true;
}
bool MIOSingleConn::CanRecv()
{
	if (mErrorFlag){
		return false;
	}
	return true;
}

bool MIOSingleConn::IsNeedClose()
{
	if (mErrorFlag){
		return true;
	}
	if (mUseRateControl && isSend(&mConnectionPublisher, &mSessionPublisher) == 0 &&
		isNeedClose(&mConnectionPublisher)){
		return true;
	}
	return false;
}

int MIOSingleConn::RecvToCache()
{
	int ret = 0;
	if ((ret = m_socket_read(mSocket, mRecvCacheBuf + mReavCacheBufLastPos, 
		RECV_CACHE_BUFFER_SIZE - mReavCacheBufLastPos)) < 0){

		mErrorFlag = true;
		return -1;	
	}

	mReavCacheBufLastPos += ret;

	//anlisys packet;

	int read_pos = 0;
	while(mReavCacheBufLastPos - read_pos >= PACKET_HEADER_SIZE){
		int payload_size = ntohs(*(unsigned short*)(&mRecvCacheBuf[read_pos+1]));
		if (mReavCacheBufLastPos - read_pos >= (payload_size + PACKET_HEADER_SIZE)){
			MPacket *pkt = mPool->GetPacket(payload_size);
			pkt->type = mRecvCacheBuf[read_pos];
			pkt->payload_size = payload_size;
			pkt->global_seq = htonl(*(unsigned int*)(&mRecvCacheBuf[read_pos + 3]));
			pkt->send_time = htonl(*(unsigned int*)(&mRecvCacheBuf[read_pos + 7]));
			pkt->recv_time = get_systime_ms() - mPeer->GetStartTime();
			memcpy(pkt->buffer, &mRecvCacheBuf[read_pos], payload_size + PACKET_HEADER_SIZE);
			mRecvCachePackets.push_back(pkt);
			read_pos += (payload_size + PACKET_HEADER_SIZE);
			continue;
		}
		break;
	}

	if (read_pos < mReavCacheBufLastPos){
		memmove(mRecvCacheBuf, mRecvCacheBuf + read_pos, (mReavCacheBufLastPos - read_pos));
		mReavCacheBufLastPos = mReavCacheBufLastPos - read_pos;
	}
	else {
		mReavCacheBufLastPos = 0;
	}
	return 0;
}
MPacket* MIOSingleConn::GetRecvPacket()
{
	if (mRecvCachePackets.size() == 0){
		return NULL;
	}
	MPacket *ret = mRecvCachePackets.front();
	mRecvCachePackets.pop_front();
	return ret;
}

int  MIOSingleConn::Recv()
{
	//if (RecvToCache() < 0){
	//	AddResenPacket();
	//	return -1;
	//}
	//MPacket *pkt;
	//while ((pkt = GetRecvPacket()) != NULL){
	//	if (pkt->type == M_PACKET_TYPE_UP_DATA){
	//		mPeer->AddRecvPacket(pkt); 
	//		continue; //do not need to free.
	//	}
	//	else if (pkt->type == M_PACKET_TYPE_STATISTICAL){
	//		publisher_multitcp_connection_control_on_feedback((char*)pkt->pay_load, pkt->payload_size, &mConnectionPublisher);
	//		pkt->Free();
	//	}
	//	else if (pkt->type == M_PACKET_TYPE_DISCONNECT){
	//		mErrorFlag = true;
	//		pkt->Free();
	//		AddResenPacket();
	//		return -1;
	//	}		
	//	
	//}


	MPacket *pkt;
	if ((pkt = recv_packet()) != NULL){
		if (pkt->type == M_PACKET_TYPE_UP_DATA){
			mPeer->AddRecvPacket(pkt); //do not need to free.
		}
		else if (pkt->type == M_PACKET_TYPE_STATISTICAL){
			publisher_multitcp_connection_control_on_feedback((char*)pkt->pay_load, pkt->payload_size, &mConnectionPublisher);
			M_IO_Log(M_IO_LOGINFO, "************got feedback********");
			pkt->Free();
		}
		else if (pkt->type == M_PACKET_TYPE_DISCONNECT){
			mErrorFlag = true;
			M_IO_Log(M_IO_LOGINFO, "************got disconnect ********");
			pkt->Free();
			AddResenPacket();
		}
		else{
			if (pkt->type == M_PACKET_TYPE_RESEND){
				int i = 0;
				uint32_t seq = 0;
				while (i < pkt->payload_size){
					seq = ntohl(*((unsigned int*)&(pkt->pay_load[i])));
					M_IO_Log(M_IO_LOGINFO, "recv resend request %I32u", seq);

					if (mPeer->SendResndPktNow(seq) != 0){
						break;
					}

					i += 4;
				}
				M_IO_Log(M_IO_LOGINFO, "************got resend ********");
			}
			else {
				M_IO_Log(M_IO_LOGINFO, "************got others type=%d********",pkt->type);
			}
			pkt->Free();
			
		}
		return 0;
	}
	else {
		AddResenPacket();
		return -1;
	}
}

int  MIOSingleConn::Write(MPacket* pkt)
{	

	if (send_packet(pkt) != 0){
	
		AddResenPacket();
		return -1;
	}
	
	RemoveResendPacket();
	return 0;
}


int MIOSingleConn::SetSockSendBufSize(int size)
{
	if (m_socket_set_send_buf(mSocket, size) == 0){
		mSockSendBufSize = size;
		return 0;
	}
	else {
		return -1;
	}
}
int MIOSingleConn::SetSockRecvBufSize(int size)
{
	if (m_socket_set_recv_buf(mSocket, size) == 0){
		mSockRecvBufSize = size;
		return 0;
	}
	else {
		return -1;
	}
}

int MIOSingleConn::GetSockSendBufSize(int &size)
{
	if (mSockSendBufSize > 0 || m_socket_get_send_buf(mSocket, mSockSendBufSize) ==0){
		size = mSockSendBufSize;
		return 0;
	}
	else {
		return -1;
	}
}

int MIOSingleConn::GetSockRecvBufSize(int &size)
{
	if (mSockRecvBufSize > 0 || m_socket_get_send_buf(mSocket, mSockRecvBufSize) == 0){
		size = mSockRecvBufSize;
		return 0;
	}
	else {
		return -1;
	}
}


MPacket* MIOSingleConn::FindPktBySeq(uint32_t seq, bool is_remove_from_cache)
{
	MPacket *ret = NULL;
	std::list<MPacket*>::iterator it;
	for (it = mSendCachePackets.begin(); it != mSendCachePackets.end(); it++){
		if ((*it)->global_seq == seq){
			ret = *it;
			ret->resend_flag = true;
			if (is_remove_from_cache){
				mSendCachePackets.erase(it);
				mSendCacheSize -= (ret->payload_size + PACKET_HEADER_SIZE);
			}
			break;
		}
	}
	return ret;
}

//int MIOSingleConn::Reconnect(){
//	m_socket_close(mSocket);
//	if (0 != Connect0(mPeer->GetDestIp(), mPeer->GetDestPort(), 5000)){
//		return -1;
//	}
//	if (0 != Connect1(M_CONN_TYPE_SLAVE, mSessionId)){
//		return -1;
//	}
//	mPeer->IsNeedToUpdateMaxSock(mSocket);
//	return 0;
//}
