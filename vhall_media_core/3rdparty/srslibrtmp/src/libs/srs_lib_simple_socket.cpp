/*
 The MIT License (MIT)
 
 Copyright (c) 2013-2015 SRS(ossrs)
 
 Permission is hereby granted, free of charge, to any person obtaining a copy of
 this software and associated documentation files (the "Software"), to deal in
 the Software without restriction, including without limitation the rights to
 use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 the Software, and to permit persons to whom the Software is furnished to do so,
 subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <srs_lib_simple_socket.hpp>
#include <srs_kernel_error.hpp>
#include <atomic>
#include <../../multitcp/m_io_socket.h>
// for srs-librtmp, @see https://github.com/ossrs/srs/issues/213
#ifndef _WIN32
#define SOCKET_ETIME EWOULDBLOCK
#define SOCKET_ECONNRESET ECONNRESET

#define SOCKET_ERRNO() errno
#define SOCKET_RESET(fd) fd = -1; (void)0
#define SOCKET_CLOSE(fd) \
if (fd > 0) { \
::shutdown(fd,SHUT_RDWR); \
::close(fd); \
fd = -1; \
} \
(void)0
#define SOCKET_VALID(x) (x > 0)
#define SOCKET_SETUP() (void)0
#define SOCKET_CLEANUP() (void)0
#define SET_RCVTIMEO(tv,s,m)	struct timeval tv = {s,m}
#define SET_SNDTIMEO(tv,s,m)	struct timeval tv = {s,m}
//add by llc
#define ASYNC_SOCKET_CLOSE(fd) ::close(fd);
#else
#define SOCKET_ETIME WSAETIMEDOUT
#define SOCKET_ECONNRESET WSAECONNRESET
#define SOCKET_ERRNO() WSAGetLastError()
#define SOCKET_RESET(x) x=INVALID_SOCKET
#define SOCKET_CLOSE(x) \
if(x!=INVALID_SOCKET) \
{ \
::shutdown(x,SD_BOTH); \
::closesocket(x); \
x=INVALID_SOCKET; \
}
#define SOCKET_VALID(x) (x!=INVALID_SOCKET)
#define SOCKET_BUFF(x) ((char*)x)
#define SOCKET_SETUP() socket_setup()
#define SOCKET_CLEANUP() socket_cleanup()
//add by llc
#define ASYNC_SOCKET_CLOSE(x) ::closesocket(x);
#endif

// for srs-librtmp, @see https://github.com/ossrs/srs/issues/213
#ifndef _WIN32
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/uio.h>
#include <netdb.h>
#endif

#include <sys/types.h>
#include <errno.h>

#include <srs_kernel_utility.hpp>

#ifndef ST_UTIME_NO_TIMEOUT
#define ST_UTIME_NO_TIMEOUT -1
#endif

// when io not hijacked, use simple socket, the block sync stream.
#ifndef SRS_HIJACK_IO
struct SrsBlockSyncSocket
{
   SOCKET fd;
   int64_t recv_timeout;
   int64_t send_timeout;
   std::atomic_llong recv_bytes;
   std::atomic_llong send_bytes;
   struct addrinfo * addrinfo;
   SrsBlockSyncSocket() {
      send_timeout = recv_timeout = ST_UTIME_NO_TIMEOUT;
      recv_bytes = send_bytes = 0;
      addrinfo = NULL;
      SOCKET_RESET(fd);
      SOCKET_SETUP();
   }
   
   virtual ~SrsBlockSyncSocket() {
      if(addrinfo){
         srs_addrinfo_free(addrinfo);
         addrinfo = NULL;
      }
      SOCKET_CLOSE(fd);
      SOCKET_CLEANUP();
   }
};

srs_hijack_io_t srs_hijack_io_create(){
   SrsBlockSyncSocket* skt = new SrsBlockSyncSocket();
   return skt;
}

void srs_hijack_io_destroy(srs_hijack_io_t ctx){
   SrsBlockSyncSocket* skt = (SrsBlockSyncSocket*)ctx;
   srs_freep(skt);
}

int srs_hijack_io_create_socket(srs_hijack_io_t ctx, const std::string& host, std::string& port){
   SrsBlockSyncSocket* skt = (SrsBlockSyncSocket*)ctx;
   if (skt->addrinfo) {
      srs_addrinfo_free(skt->addrinfo);
      skt->addrinfo = NULL;
   }
   skt->addrinfo = srs_dns_resolve(host, port);
   if (skt->addrinfo == NULL) {
      return ERROR_SOCKET_CREATE;
   }
   struct addrinfo * addr = skt->addrinfo;
   while (addr!=NULL) {
      skt->fd = ::socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
      if (SOCKET_VALID(skt->fd)) {
         skt->addrinfo = addr;
         break;
      }
      addr=addr->ai_next;
   }
   if (!SOCKET_VALID(skt->fd)) {
      return ERROR_SOCKET_CREATE;
   }
   return ERROR_SUCCESS;
}

int socket_set_linger(int sock, int n){
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

int srs_hijack_io_set_recv_timeout(srs_hijack_io_t ctx, int64_t timeout_us){
    SrsBlockSyncSocket* skt = (SrsBlockSyncSocket*)ctx;
    skt->recv_timeout = timeout_us;
    if (!SOCKET_VALID(skt->fd)||timeout_us<0) {
        return -1;
    }
    int sec = (int)(timeout_us / 1000000LL);
    int microsec = (int)(timeout_us % 1000000LL);
    
    sec = srs_max(0, sec);
    microsec = srs_max(0, microsec);
    
#ifdef _WIN32
    sec = (int)((timeout_us / 1000LL));
    if (setsockopt(skt->fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&sec, sizeof(sec)) == -1) {
        return SOCKET_ERRNO();
    }
#else
    SET_RCVTIMEO(tv,sec,microsec);
    if (setsockopt(skt->fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv)) == -1) {
        return SOCKET_ERRNO();
    }
#endif
    return ERROR_SUCCESS;
}

int64_t srs_hijack_io_get_recv_timeout(srs_hijack_io_t ctx){
    SrsBlockSyncSocket* skt = (SrsBlockSyncSocket*)ctx;
    return skt->recv_timeout;
}

int64_t srs_hijack_io_get_recv_bytes(srs_hijack_io_t ctx){
    SrsBlockSyncSocket* skt = (SrsBlockSyncSocket*)ctx;
    return skt->recv_bytes;
}

int srs_hijack_io_set_send_timeout(srs_hijack_io_t ctx, int64_t timeout_us){
    SrsBlockSyncSocket* skt = (SrsBlockSyncSocket*)ctx;
    skt->send_timeout = timeout_us;
    if (!SOCKET_VALID(skt->fd)||timeout_us<0) {
        return -1;
    }
    int sec = (int)(timeout_us / 1000000LL);
    int microsec = (int)(timeout_us % 1000000LL);
    
    sec = srs_max(0, sec);
    microsec = srs_max(0, microsec);
    
#ifdef _WIN32
    sec = (int)((timeout_us / 1000LL));
    if (setsockopt(skt->fd, SOL_SOCKET, SO_SNDTIMEO, (const char*)&sec, sizeof(sec)) == -1) {
        return SOCKET_ERRNO();
    }
#else
    SET_SNDTIMEO(tv, sec, microsec);
    if (setsockopt(skt->fd, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv)) == -1) {
        return SOCKET_ERRNO();
    }
#endif
    
    return ERROR_SUCCESS;
}

int64_t srs_hijack_io_get_send_timeout(srs_hijack_io_t ctx){
    SrsBlockSyncSocket* skt = (SrsBlockSyncSocket*)ctx;
    return skt->send_timeout;
}

int64_t srs_hijack_io_get_send_bytes(srs_hijack_io_t ctx){
    SrsBlockSyncSocket* skt = (SrsBlockSyncSocket*)ctx;
    return skt->send_bytes;
}

int srs_hijack_io_connect(srs_hijack_io_t ctx){
   SrsBlockSyncSocket* skt = (SrsBlockSyncSocket*)ctx;
   struct addrinfo * addr = skt->addrinfo;
   int n = 0;
   int error;
   struct timeval timeout;
   fd_set rset, wset;
   int ret = -1;
   int retval = 0;
   if (m_socket_set_nonblock(skt->fd) != 0){
      return ERROR_SOCKET_CONNECT;
   }
   ret = retval = ::connect(skt->fd, addr->ai_addr, addr->ai_addrlen);
   int e = socket_errno;
   if (ret < 0 && e != M_EAGAIN && e != M_EINTR&& e != M_EINPROGRESS){
      return ERROR_SOCKET_CONNECT;
   }
   if (retval == 0){
      return ERROR_SUCCESS;
   }
   
   FD_ZERO(&rset);
   FD_SET(skt->fd, &rset);
   FD_ZERO(&wset);
   FD_SET(skt->fd, &wset);
   MAKE_TIMEVAL(timeout, 5*1000);//5秒超时
   n = select(skt->fd + 1, &rset, &wset, NULL, &timeout);
   if (n == 0){//timeout
      SOCKET_CLOSE(skt->fd);
      return ERROR_SOCKET_CONNECT;
   }
   
   error = 0;
   if (FD_ISSET(skt->fd, &rset) || FD_ISSET(skt->fd, &wset)){
      socklen_t len = sizeof(error);
      if (getsockopt(skt->fd, SOL_SOCKET, SO_ERROR, (char *)&error, &len) < 0){
         return ERROR_SOCKET_CONNECT;
      }
      if (error){
         return ERROR_SOCKET_CONNECT;
      }
      srs_hijack_io_set_recv_timeout(skt,skt->recv_timeout);
      srs_hijack_io_set_send_timeout(skt, skt->send_timeout);
      //设置socket close方式
      socket_set_linger(skt->fd,0);
      return ERROR_SUCCESS;
   }
   return ERROR_SOCKET_CONNECT;
}

int srs_hijack_io_read(srs_hijack_io_t ctx, void* buf, size_t size, ssize_t* nread){
   SrsBlockSyncSocket* skt = (SrsBlockSyncSocket*)ctx;
   
   int ret = ERROR_SUCCESS;
   
   ssize_t nb_read = ::recv(skt->fd, (char*)buf, size, 0);
   
   if (nread) {
      *nread = nb_read;
   }
   
   // On success a non-negative integer indicating the number of bytes actually read is returned
   // (a value of 0 means the network connection is closed or end of file is reached).
   if (nb_read <= 0) {
      if (nb_read < 0 && SOCKET_ERRNO() == SOCKET_ETIME) {
         return ERROR_SOCKET_TIMEOUT;
      }
      
      if (nb_read == 0) {
         errno = SOCKET_ECONNRESET;
      }
      
      return ERROR_SOCKET_READ;
   }
   
   skt->recv_bytes += nb_read;
   return ret;
}

int srs_hijack_io_writev(srs_hijack_io_t ctx, const iovec *iov, int iov_size, ssize_t* nwrite){
   SrsBlockSyncSocket* skt = (SrsBlockSyncSocket*)ctx;
   
   int ret = ERROR_SUCCESS;
   
   ssize_t nb_write = ::writev(skt->fd, iov, iov_size);
   
   if (nwrite) {
      *nwrite = nb_write;
   }
   
   // On  success,  the  readv()  function  returns the number of bytes read;
   // the writev() function returns the number of bytes written.  On error, -1 is
   // returned, and errno is set appropriately.
   if (nb_write <= 0) {
      // @see https://github.com/ossrs/srs/issues/200
      if (nb_write < 0 && SOCKET_ERRNO() == SOCKET_ETIME) {
         return ERROR_SOCKET_TIMEOUT;
      }
      
      return ERROR_SOCKET_WRITE;
   }
   
   skt->send_bytes += nb_write;
   
   return ret;
}

bool srs_hijack_io_is_never_timeout(srs_hijack_io_t ctx, int64_t timeout_us){
   return timeout_us == (int64_t)ST_UTIME_NO_TIMEOUT;
}

int srs_hijack_io_read_fully(srs_hijack_io_t ctx, void* buf, size_t size, ssize_t* nread){
   SrsBlockSyncSocket* skt = (SrsBlockSyncSocket*)ctx;
   
   int ret = ERROR_SUCCESS;
   
   size_t left = size;
   ssize_t nb_read = 0;
   
   while (left > 0) {
      char* this_buf = (char*)buf + nb_read;
      ssize_t this_nread;
      
      if ((ret = srs_hijack_io_read(ctx, this_buf, left, &this_nread)) != ERROR_SUCCESS) {
         return ret;
      }
      
      nb_read += this_nread;
      left -= (size_t)this_nread;
   }
   
   if (nread) {
      *nread = nb_read;
   }
   skt->recv_bytes += nb_read;
   
   return ret;
}

int srs_hijack_io_write(srs_hijack_io_t ctx, void* buf, size_t size, ssize_t* nwrite){
   SrsBlockSyncSocket* skt = (SrsBlockSyncSocket*)ctx;
   
   int ret = ERROR_SUCCESS;
   
   ssize_t nb_write = ::send(skt->fd, (char*)buf, size, 0);
   
   if (nwrite) {
      *nwrite = nb_write;
   }
   
   if (nb_write <= 0) {
      // @see https://github.com/ossrs/srs/issues/200
      if (nb_write < 0 && SOCKET_ERRNO() == SOCKET_ETIME) {
         return ERROR_SOCKET_TIMEOUT;
      }
      
      return ERROR_SOCKET_WRITE;
   }
   
   skt->send_bytes += nb_write;
   
   return ret;
}
#endif

SimpleSocketStream::SimpleSocketStream(){
   io = srs_hijack_io_create();
}

SimpleSocketStream::~SimpleSocketStream(){
   if (io) {
      srs_hijack_io_destroy(io);
      io = NULL;
   }
}

void* SimpleSocketStream::get_io(){
   return io;
}

int SimpleSocketStream::connect(const char* server_ip, int port){
    std::lock_guard<std::mutex> lock(mMutex);
   srs_assert(io);
   char portChar[16];
   sprintf(portChar, "%d",port);
   std::string portStr(portChar);
   if(srs_hijack_io_create_socket(io,server_ip,portStr)==ERROR_SOCKET_CREATE){
      return ERROR_SOCKET_CREATE;
   }
   int ret = srs_hijack_io_connect(io);
   if (ret  == ERROR_SUCCESS) {
      // Modify by liwenlong 2016.8.1 close()阻塞
      SrsBlockSyncSocket* skt = (SrsBlockSyncSocket*)io;
      if ((ret = m_socket_set_block(skt->fd)) != ERROR_SUCCESS){
         return ret;
      }
      // Modify by liwenlong 2016.8.1 close()阻塞
      ret = m_socket_set_linger(skt->fd);
   }
   return ret;
}

void SimpleSocketStream::async_close(){
    std::lock_guard<std::mutex> lock(mMutex);
   srs_assert(io);
   SrsBlockSyncSocket* skt = (SrsBlockSyncSocket*)io;
   SOCKET_CLOSE(skt->fd);
}

// ISrsBufferReader
int SimpleSocketStream::read(void* buf, size_t size, ssize_t* nread)
{
   srs_assert(io);
   return srs_hijack_io_read(io, buf, size, nread);
}

// ISrsProtocolReader
void SimpleSocketStream::set_recv_timeout(int64_t timeout_us)
{
   srs_assert(io);
   srs_hijack_io_set_recv_timeout(io, timeout_us);
}

int64_t SimpleSocketStream::get_recv_timeout()
{
   srs_assert(io);
   return srs_hijack_io_get_recv_timeout(io);
}

int64_t SimpleSocketStream::get_recv_bytes()
{
   srs_assert(io);
   return srs_hijack_io_get_recv_bytes(io);
}

// ISrsProtocolWriter
void SimpleSocketStream::set_send_timeout(int64_t timeout_us)
{
   srs_assert(io);
   srs_hijack_io_set_send_timeout(io, timeout_us);
}

int64_t SimpleSocketStream::get_send_timeout()
{
   srs_assert(io);
   return srs_hijack_io_get_send_timeout(io);
}
   
int64_t SimpleSocketStream::get_send_bytes()
{
   srs_assert(io);
   return srs_hijack_io_get_send_bytes(io);
}

int SimpleSocketStream::writev(const iovec *iov, int iov_size, ssize_t* nwrite)
{
   srs_assert(io);
   return srs_hijack_io_writev(io, iov, iov_size, nwrite);
}

// ISrsProtocolReaderWriter
bool SimpleSocketStream::is_never_timeout(int64_t timeout_us)
{
   srs_assert(io);
   return srs_hijack_io_is_never_timeout(io, timeout_us);
}

int SimpleSocketStream::read_fully(void* buf, size_t size, ssize_t* nread)
{
   srs_assert(io);
   return srs_hijack_io_read_fully(io, buf, size, nread);
}

int SimpleSocketStream::write(void* buf, size_t size, ssize_t* nwrite)
{
   srs_assert(io);
   return srs_hijack_io_write(io, buf, size, nwrite);
}


