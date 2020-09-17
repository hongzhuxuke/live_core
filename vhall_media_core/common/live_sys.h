#ifndef __LIVE_SYS_H__
#define __LIVE_SYS_H__

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN

    //llc include windows in winsock2.h
    #include <WinSock2.h>
    #include <ws2tcpip.h>
	 #include <malloc.h>
	//#include <windows.h>
    #include <time.h>
    #ifdef _MSC_VER
#if defined(_MSC_VER) && _MSC_VER<1900
    #define snprintf _snprintf
#endif
    #define strcasecmp stricmp
    #define strncasecmp strnicmp
#if defined(_MSC_VER) && _MSC_VER<1900
    #define vsnprintf _vsnprintf
#endif
    #endif
    #define GetSockError()	WSAGetLastError()
    #define SetSockError(e)	WSASetLastError(e)
    #define setsockopt(a,b,c,d,e)	(setsockopt)(a,b,c,(const char *)d,(int)e)

    #define sleep(n)	   Sleep(n*1000)
    #define msleep(n)	Sleep(n)
	 #define usleep(n)

    typedef CRITICAL_SECTION vhall_lock_t;
	//typedef HANDLE           vhall_cond_t;
	typedef CONDITION_VARIABLE vhall_cond_t;
#else

#if defined(OSX)||defined(IOS)
    #include <mach/clock.h>
    #include <mach/mach.h>
#endif
    #include <sys/time.h>
    #include <unistd.h>
    #include <arpa/inet.h>
    #include <pthread.h>
    #include <netdb.h>
	 #include <semaphore.h>
    #include <atomic>
	 #define msleep(n) usleep(n*1000)
    #define closesocket(s) close(s)
	 typedef pthread_mutex_t vhall_lock_t;
	 typedef pthread_cond_t  vhall_cond_t;

#endif

#if defined(IOS)||defined(ANDROID)||defined(OSX)
#define int64_t __int64_t
#define PTHREAD_MUTEX_RECURSIVE_NP PTHREAD_MUTEX_RECURSIVE
#endif

	int vhall_lock_init(vhall_lock_t*lock);
	int vhall_lock_destroy(vhall_lock_t*lock);
	int vhall_lock(vhall_lock_t*lock);
	int vhall_unlock(vhall_lock_t*lock);

	int vhall_cond_init(vhall_cond_t*cond);
	int vhall_cond_destroy(vhall_cond_t*cond);
	int vhall_cond_wait(vhall_cond_t*cond, vhall_lock_t*lock);
	int vhall_cond_wait_time(vhall_cond_t*cond, vhall_lock_t*lock, int time_ms = -1);
	int vhall_cond_signal(vhall_cond_t*cond);
	
#endif

