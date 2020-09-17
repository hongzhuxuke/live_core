#include "m_io_sys.h"
#include <time.h>
#include <list>
#include <map>
#include <queue>
#include "m_io.h"
#include "m_io_peer.h"

typedef struct{
	MIOPeer *peer;
	uint64_t send_bytes;
	uint64_t recv_bytes;
}content_t;

m_io_content_t m_io_create(int conn_num , int send_buf_size, int recv_buf_size)
{
	content_t * ctx = new content_t();
	memset(ctx, 0, sizeof(content_t));
	ctx->peer = new MIOPeer(conn_num, send_buf_size, recv_buf_size);

	return (void*)ctx;
}

int m_io_connect(m_io_content_t m_io, const char* ip, int port)
{
	content_t * ctx = (content_t*)m_io;
	if (ctx == NULL || ctx->peer == NULL){
		return -1;
	}

	return ctx->peer->Connect(ip,port,5000);

}
int m_io_read(m_io_content_t m_io, void* buf, int size, int& nread)
{
	content_t * ctx = (content_t*)m_io;
	if (ctx == NULL || ctx->peer == NULL){
		return -1;
	}

	if (ctx->peer->Read((char*)buf, size, nread) != 0){
		return -1;
	}
	ctx->recv_bytes += nread;
	return 0;
}

int m_io_read_fully(m_io_content_t m_io, void* buf, int size, int& nread)
{

	content_t * ctx = (content_t*)m_io;
	if (ctx == NULL || ctx->peer == NULL){
		return -1;
	}

	if (ctx->peer->ReadN((char*)buf, size, nread) != 0){
		return -1;
	}
	ctx->recv_bytes += nread;
	return 0;
}
int m_io_write(m_io_content_t m_io, void* buf, int size, int &nwrite)
{
	content_t * ctx = (content_t*)m_io;
	if (ctx == NULL || ctx->peer == NULL){
		return -1;
	}

	if (ctx->peer->Write((char*)buf, size, nwrite) != 0){
		return -1;
	}
	ctx->send_bytes += nwrite;
	return 0;
}

void m_io_destroy(m_io_content_t m_io)
{
	content_t * ctx = (content_t*)m_io;
	if (ctx == NULL){
		return;
	}
		
	if(ctx->peer == NULL){
		delete ctx;
		return;
	}

	delete ctx->peer;
	delete ctx;
	return;
}

void m_io_set_recv_timeout(m_io_content_t m_io, int64_t timeout_us)
{
	content_t * ctx = (content_t*)m_io;
	if (ctx == NULL || ctx->peer == NULL){
		return;
	}
	ctx->peer->SetReadTimeout(timeout_us);
}

void m_io_set_send_timeout(m_io_content_t m_io, int64_t timeout_us)
{
	content_t * ctx = (content_t*)m_io;
	if (ctx == NULL || ctx->peer == NULL){
		return;
	}
	ctx->peer->SetWriteTimeout(timeout_us);
}

int64_t m_io_get_recv_timeout(m_io_content_t m_io)
{
	content_t * ctx = (content_t*)m_io;
	if (ctx == NULL || ctx->peer == NULL){
		return -1;
	}
	return ctx->peer->GetReadTimeout();
}

int64_t m_io_get_send_timeout(m_io_content_t m_io)
{
	content_t * ctx = (content_t*)m_io;
	if (ctx == NULL || ctx->peer == NULL){
		return -1;
	}
	return ctx->peer->GetWriteTimeout();
}

int64_t m_io_get_recv_bytes(m_io_content_t m_io)
{
	content_t * ctx = (content_t*)m_io;
	if (ctx == NULL || ctx->peer == NULL){
		return -1;
	}
	return ctx->recv_bytes;
}
int64_t m_io_get_send_bytes(m_io_content_t m_io)
{
	content_t * ctx = (content_t*)m_io;
	if (ctx == NULL || ctx->peer == NULL){
		return -1;
	}
	return ctx->send_bytes;
	
}
