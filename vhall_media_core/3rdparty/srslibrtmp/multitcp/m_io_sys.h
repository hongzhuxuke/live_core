#ifndef __M_SYS_H__
#define __M_SYS_H__

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32

#include <winsock2.h>
#include <ws2tcpip.h>

typedef SOCKET m_socket;
#define socket_errno	WSAGetLastError()
#define socket_error    SOCKET_ERROR

#define M_EINTR	WSAEINTR
#define M_EAGAIN	WSAEWOULDBLOCK
#define M_EINPROGRESS WSAEINPROGRESS

typedef WSABUF iov_type;
typedef unsigned int    m_thread_ret;

typedef CRITICAL_SECTION m_lock_t;
typedef HANDLE           m_cond_t;
typedef unsigned int(__stdcall *m_thread_func)(void*);
typedef HANDLE           m_thread_handle;
#define msleep(n)	Sleep(n)
#define setsockopt(a,b,c,d,e)	(setsockopt)(a,b,c,(const char *)d,(int)e)

#ifdef _MSC_VER	/* MSVC */
#define snprintf _snprintf
#define strcasecmp stricmp
#define strncasecmp strnicmp
#define vsnprintf _vsnprintf
#endif

#else /* !_WIN32 */

#if defined(OSX)||defined(IOS)
#include <mach/clock.h>
#include <mach/mach.h>
#endif
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/errno.h>
#include <pthread.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/uio.h>
#include <sys/un.h>

typedef int m_socket;
#define socket_errno errno
#define socket_error -1
typedef struct iovec iov_type;
typedef void*  m_thread_ret;
#define __stdcall
typedef pthread_mutex_t m_lock_t;
typedef pthread_cond_t  m_cond_t;
typedef void *(*m_thread_func)(void*);
typedef pthread_t    m_thread_handle;
#define msleep(n)	usleep(n*1000)
#define INVALID_SOCKET (-1)
#define M_EINTR	EINTR   
#define M_EAGAIN	EAGAIN
#define M_EINPROGRESS EINPROGRESS
#define closesocket close
#if defined(IOS)||defined(ANDROID)||defined(OSX)
#define PTHREAD_MUTEX_RECURSIVE_NP PTHREAD_MUTEX_RECURSIVE
#endif
#endif


int m_lock_init(m_lock_t*lock);
int m_lock_destroy(m_lock_t*lock);
int m_lock(m_lock_t*lock);
int m_unlock(m_lock_t*lock);

int m_cond_init(m_cond_t*cond);
int m_cond_destroy(m_cond_t*cond);
int m_cond_wait(m_cond_t*cond, m_lock_t*lock, int time_ms = -1);
int m_cond_signal(m_cond_t*cond);

int m_thread_create(m_thread_handle *thread, m_thread_func start_func, void *arg, int stack_size);
int m_thread_jion(m_thread_handle thread, int time_ms = -1);
int m_thread_exit(m_thread_ret exit_code);
int m_thread_terminate(m_thread_handle thread);

uint64_t get_systime_ms();

class MAutolock{
public:
	MAutolock(m_lock_t* mutex);
	~MAutolock();
private:
	m_lock_t* mLock;
};

//#define SOCKADDR_IN(addr, ip, port) do{ \
//                                memset(&(addr), 0, sizeof(addr)); \
//                                (addr).sin_family      = AF_INET;     \
//                                (addr).sin_addr.s_addr = inet_addr(ip);   \
//                                (addr).sin_port        = htons(port);     \
//                                if ((addr).sin_addr.s_addr == INADDR_NONE){    \
//                                    struct hostent* pHost = gethostbyname(ip);  \
//                                    if (pHost == NULL){return -1;}  \
//                                    memcpy(&(addr).sin_addr, pHost->h_addr_list[0], pHost->h_length);} \
//								    }while(0)

#define MAKE_TIMEVAL(tv, millisec)  do{ \
                                           if(millisec > 0){   \
                                                tv.tv_sec = millisec / 1000;    \
                                                tv.tv_usec = (millisec % 1000)*1000;    \
										   										   } else{ \
                                                tv.tv_sec = 0;  \
                                                tv.tv_usec = 0; \
										   		}}while(0)

#endif //__M_SYS_H__
