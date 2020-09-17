#ifndef __MTP_SOCKET_H__
#define __MTP_SOCKET_H__

#include "m_io_sys.h"

m_socket m_socket_tcp(struct addrinfo *info,struct addrinfo **result);
m_socket m_socket_udp(int ip_type = 0);

int m_socket_close(m_socket sock);

int m_socket_connect_timeo(m_socket sock,struct addrinfo *result, int timeout_ms = -1);

//this will set sock to noblock.
int m_socket_connect_timeo(m_socket sock, 
	const void *addr, int addr_len, int timeout_ms = -1);

int m_socket_bind(m_socket sock, const char *ip, const unsigned short port);

int m_socket_sendto(m_socket sock, const char *buf, unsigned int len, int flags,
	const sockaddr *to, socklen_t tolen);
int m_socket_recvfrom(m_socket sock, char *buf, unsigned int len,
	int flags, sockaddr *from, socklen_t *fromlen);

int m_socket_read(m_socket sock, char *buf, unsigned int buf_size);
int m_socket_readfull(m_socket sock, char *buf, unsigned int buf_size);
int m_socket_readv(m_socket sock, iov_type *vec, int count);

int m_socket_send(m_socket sock, const char *buf, unsigned int count);
int m_socket_sendfull(m_socket sock, const char *buf, unsigned int len);
int m_socket_sendv(m_socket sock, const iov_type *iov, int count);

int m_socket_set_recv_timeo(m_socket sock, int ms);
int m_socket_set_snd_timeo(m_socket sock, int ms);
int m_socket_set_nonblock(m_socket sock);
int m_socket_set_block(m_socket sock);

int m_socket_set_linger(m_socket sock, int linger = 0);

int m_socket_set_recv_buf(m_socket sock, int buf_size);
int m_socket_set_send_buf(m_socket sock, int buf_size);

int m_socket_get_recv_buf(m_socket sock, int &buf_size);
int m_socket_get_send_buf(m_socket sock, int &buf_size);

#endif //__MTP_SOCKET_H__


