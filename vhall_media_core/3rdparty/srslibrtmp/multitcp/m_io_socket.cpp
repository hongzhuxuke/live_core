#include "m_io_socket.h"

m_socket m_socket_tcp(struct addrinfo *info,struct addrinfo **result)
{
   int fd = -1;
   struct addrinfo * addr = info;
   while (addr!=NULL) {
      fd = ::socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
      if (fd>0) {
         *result = addr;
         break;
      }
      addr=addr->ai_next;
   }
   return fd;
}

m_socket m_socket_udp(int ip_type)
{
	if (ip_type == 0){
		return socket(PF_INET, SOCK_DGRAM, 0);
	}
	else {
		return socket(PF_INET6, SOCK_DGRAM, 0);
	}
}

int m_socket_close(m_socket s){
	return closesocket(s);
}

int m_socket_connect_timeo(m_socket sock, struct addrinfo *result, int timeout_ms)
{
   struct addrinfo * addr = result;
   return m_socket_connect_timeo(sock, addr->ai_addr,addr->ai_addrlen, timeout_ms);
}

int m_socket_connect_timeo(m_socket sock, const void *addr, int addr_len, int timeout_ms)
{
	int n = 0;
	int error;
	struct timeval timeout;
	fd_set rset, wset;
	int ret = -1;
	int retval = 0;
	if (timeout_ms < 0) // 阻塞
	{
		return connect(sock, (struct sockaddr *)addr, addr_len);
	}
   
	if (m_socket_set_nonblock(sock) != 0)
	{
		return -1;
	}

	ret = retval = connect(sock, (struct sockaddr *) addr, addr_len);
	int e = socket_errno;
	if (ret < 0 && e != M_EAGAIN && e != M_EINTR&& e != M_EINPROGRESS)
	{
		return -1;
	}

	if (retval == 0)
	{
		return 0;
	}

	if (timeout_ms == 0)   //timeout
	{
		return -1;
	}
	FD_ZERO(&rset);
	FD_SET(sock, &rset);
	FD_ZERO(&wset);
	FD_SET(sock, &wset);
	MAKE_TIMEVAL(timeout, timeout_ms);
	n = select(sock + 1, &rset, &wset, NULL, &timeout);
	if (n == 0) //timeout
	{
		return -1;
	}

	error = 0;
	if (FD_ISSET(sock, &rset) || FD_ISSET(sock, &wset))
	{
		socklen_t len = sizeof(error);
		if (getsockopt(sock, SOL_SOCKET, SO_ERROR, (char *)&error, &len) < 0)
		{
			return -1;
		}

		if (error)
		{
			return -1;
		}
		return 0;
	}
	//not ready...
	return -1;
}

int m_socket_bind(m_socket sock, const char *ip, const unsigned short port)
{
	struct sockaddr_in inaddr;
	memset(&inaddr, 0, sizeof(struct sockaddr_in));
	inaddr.sin_family = AF_INET;        //ipv4协议族    
	inaddr.sin_port = htons(port);
	if ((!ip) || (ip[0] == 0) || (strcmp(ip, "0.0.0.0") == 0))
	{
		inaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	}
	else
	{
		inaddr.sin_addr.s_addr = inet_addr(ip);
	}
	return bind(sock, (struct sockaddr*)&inaddr, sizeof(struct sockaddr));
}

int m_socket_sendto(m_socket s, const char *buf, unsigned int len, int flags,
	const sockaddr *to, socklen_t tolen)
{
	return (int)sendto(s, buf, len, flags, to, tolen);
}

int m_socket_recvfrom(m_socket s, char *buf, unsigned int len, 
	int flags, sockaddr *from, socklen_t *fromlen)
{
	return (int)recvfrom(s, buf, len, flags, from, fromlen);
}

int m_socket_read(m_socket s, char *buf, unsigned int buf_size)
{
	int n = 0;
	do{
#ifdef WIN32
		n = recv(s, buf, buf_size, 0);
#else
		n = (int)read(s, buf, buf_size);
#endif
		
	} while ((n < 0) && (socket_errno == M_EINTR || socket_errno == M_EAGAIN));
	return n;
}

int m_socket_readfull(m_socket s, char *buf, unsigned int buf_size)
{
	int result, old_len = buf_size;
	do {
		result = m_socket_read(s, buf, buf_size);
		if (result == 0){ 
			return 0; 
		}
		if (result == socket_error){
			int errorno = socket_errno;
			return errorno;
		}

		buf_size -= result;
		buf += result;
	} while (buf_size);

	return old_len;
}

int m_socket_readv(m_socket s, iov_type *vec, int count)
{
	int n = 0;
#ifdef WIN32
	DWORD bytesRead = 0;
	DWORD flags = 0;
	if (WSARecv(s, vec, count, &bytesRead, &flags, NULL, NULL)) {
		/* The read failed. It might be a close,
		* or it might be an error. */
		if (WSAGetLastError() == WSAECONNABORTED)
			n = 0;
		else
			n = -1;
	}
	else
		n = bytesRead;

#else
	do{
		n = (int)readv(s, vec, count);
	} while ((n < 0) && (socket_errno == M_EINTR || socket_errno == M_EAGAIN));

#endif
	return n;
}

int m_socket_send(m_socket s, const char *buf, unsigned int count)
{
	int n = 0;
	do{
#ifdef WIN32
		n = send(s, buf, count, 0);
#else
		n = (int)write(s, buf, count);
#endif
	} while ((n < 0) && (socket_errno == M_EINTR || socket_errno == M_EAGAIN));
	return n;
}

int m_socket_sendfull(m_socket s, const char *buf, unsigned int len)
{
	int result, old_len = len;
	do {
		result = m_socket_send(s, buf, len);
		if (result == socket_error)
			return socket_error;
		len -= result;
		buf += result;
	} while (len);
	return old_len;
}

int m_socket_sendv(m_socket s, const iov_type *iov, int count)
{
	int n = 0;
#ifdef WIN32
	DWORD bytesSent = 0;
	if (WSASend(s, (LPWSABUF)iov, count, &bytesSent, 0, NULL, NULL))
		n = -1;
	else
		n = bytesSent;
#else
	do{
		n = (int)writev(s, iov, count);
	} while ((n < 0) && (socket_errno == M_EINTR || socket_errno == M_EAGAIN));

#endif
	return n;
}

int m_socket_set_recv_timeo(m_socket sock, int ms)
{
	struct timeval tv;
	tv.tv_sec = ms / 1000;
	tv.tv_usec = (ms % 1000) * 1000;
	return setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));
}

int m_socket_set_recv_buf(m_socket sock, int buf_size)
{
	int revbuf = buf_size;
	return setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (const char*)&revbuf, sizeof(int));
}

int m_socket_set_send_buf(m_socket sock, int buf_size)
{
	int sendbuf = buf_size;
	return setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (const char*)&sendbuf, sizeof(int));
}

int m_socket_get_recv_buf(m_socket sock , int &buf_size)
{
	socklen_t optlen = sizeof(int);
	return getsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char*)&buf_size, &optlen);
}

int m_socket_get_send_buf(m_socket sock ,int &buf_size)
{
	socklen_t optlen = sizeof(int);
	return getsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char*)&buf_size, &optlen);
}

int m_socket_set_snd_timeo(m_socket sock, int ms)
{
	struct timeval tv;
	tv.tv_sec = ms / 1000;
	tv.tv_usec = (ms % 1000) * 1000;
	return setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char *)&tv, sizeof(tv));
}

int m_socket_set_nonblock(m_socket s)
{
#ifdef WIN32
	u_long mode = 1;
	return ioctlsocket(s, FIONBIO, &mode);
#else
	int val = fcntl(s, F_GETFL, 0);
	if (val == -1){ return -1; }
	if (val & O_NONBLOCK){ return 0; }
	return fcntl(s, F_SETFL, val | O_NONBLOCK | O_NDELAY);
#endif    
}

int m_socket_set_block(m_socket s)
{
#ifdef WIN32
	u_long mode = 0;
	return ioctlsocket(s, FIONBIO, &mode);
#else
	int val = fcntl(s, F_GETFL, 0);
	if (val == -1){ return -1; }
	if (val & O_NONBLOCK){
      val &= ~O_NONBLOCK;
		return fcntl(s, F_SETFL, val);
	}
	return 0;
#endif    
}

int m_socket_set_linger(m_socket sock, int n){
	struct linger lin;
	lin.l_onoff = 1;
	lin.l_linger = n;
	int optlen = sizeof(linger);
	if (setsockopt(sock, SOL_SOCKET, SO_LINGER, &lin, optlen) < 0)
	{
		return -1;
	}
	return 0;
}
