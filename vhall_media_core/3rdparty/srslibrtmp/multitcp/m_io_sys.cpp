#include "m_io_sys.h"

#ifdef _WIN32
#include <process.h>
#else
#include <pthread.h>
#endif

int m_lock_init(m_lock_t*lock){
	int ret = 0;
#if defined _WIN32 || defined WIN32
	InitializeCriticalSection(lock);
#else
	pthread_mutexattr_t mattr;
	pthread_mutexattr_init(&mattr);
	pthread_mutexattr_settype(&mattr, PTHREAD_MUTEX_RECURSIVE_NP);
	ret = pthread_mutex_init(lock, &mattr);
	pthread_mutexattr_destroy(&mattr);
#endif
	return ret;
}

int m_lock_destroy(m_lock_t*lock){
	int ret = 0;
#if defined _WIN32 || defined WIN32
	DeleteCriticalSection(lock);
#else
	ret = pthread_mutex_destroy(lock);
#endif
	return ret;
}

int m_lock(m_lock_t*lock){
	int ret = 0;
#if defined _WIN32 || defined WIN32
	EnterCriticalSection(lock);
#else
	ret = pthread_mutex_lock(lock);
#endif
	return ret;
}

int m_unlock(m_lock_t*lock){
	int ret = 0;
#if defined _WIN32 || defined WIN32
	LeaveCriticalSection(lock);
#else
	ret = pthread_mutex_unlock(lock);
#endif
	return ret;
}

int m_cond_init(m_cond_t*cond)
{
	int ret = 0;
#if defined _WIN32 || defined WIN32
	*cond = CreateEvent(NULL, FALSE, FALSE, NULL);

#else
	ret = pthread_cond_init(cond, NULL);
#endif
	return ret;
}

int m_cond_destroy(m_cond_t*cond)
{
	int ret = 0;
#if defined _WIN32 || defined WIN32
	CloseHandle(*cond);
	*cond = NULL;
#else
	ret = pthread_cond_destroy(cond);
#endif
	return ret;
}

int m_cond_wait(m_cond_t*cond, m_lock_t*lock, int time_ms){

	int ret = 0;
#if defined _WIN32 || defined WIN32
	if (lock) {
		LeaveCriticalSection(lock);
	}
	if (time_ms < 0){
		ret = WaitForSingleObject(*cond, INFINITE);
	}
	else{
		ret = WaitForSingleObject(*cond, time_ms);
	}

	if (lock) {
		EnterCriticalSection(lock);
	}

#else
	if (time_ms < 0){
		ret = pthread_cond_wait(cond, lock);
	}	
	else {
		struct timespec ts;
#if defined(OSX)||defined(IOS) // OS X does not have clock_gettime, use clock_get_time
      clock_serv_t cclock;
      mach_timespec_t mts;
      host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &cclock);
      clock_get_time(cclock, &mts);
      mach_port_deallocate(mach_task_self(), cclock);
      ts.tv_sec = mts.tv_sec;
      ts.tv_nsec = mts.tv_nsec;
#else
		if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
			return -1;
		}
#endif
		ts.tv_sec += time_ms / 1000;
		ts.tv_nsec += (time_ms % 1000) * 1000 * 1000;
		ret = pthread_cond_timedwait(cond, lock, &ts);
	}
#endif

	return ret;
}

int m_cond_signal(m_cond_t*cond)
{
	int ret = 0;
#if defined _WIN32 || defined WIN32
	SetEvent(*cond);

#else
	ret = pthread_cond_signal(cond);
#endif
	return ret;
}
int m_thread_create(m_thread_handle *thread, m_thread_func start_func, void *arg, int stack_size)
{
	int ret = 0;
#ifdef WIN32
	*thread = (m_thread_handle)_beginthreadex(0, stack_size, start_func, arg, 0, 0);
	if (*thread == NULL)
	{
		ret = -1;
	}
#else
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, stack_size);
	//pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	ret = pthread_create(thread, &attr, start_func, arg);
#endif
	return ret;
}
int m_thread_jion(m_thread_handle thread, int time_ms)
{

#ifdef WIN32
	DWORD waitRet = 0;
	if (time_ms < 0){
		waitRet = WaitForSingleObject(thread, INFINITE);
	}
	else {
		waitRet = WaitForSingleObject(thread, time_ms);
	}

	if (waitRet != WAIT_OBJECT_0){
		return -1;
	}
	return waitRet;
#else
	int ret = 0;
	if (time_ms < 0){
		ret = pthread_join(thread, NULL);
	}
	else {
		struct timespec ts;
#if defined(OSX)||defined(IOS) // OS X does not have clock_gettime, use clock_get_time
      clock_serv_t cclock;
      mach_timespec_t mts;
      host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &cclock);
      clock_get_time(cclock, &mts);
      mach_port_deallocate(mach_task_self(), cclock);
      ts.tv_sec = mts.tv_sec;
      ts.tv_nsec = mts.tv_nsec;
#else
		if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
			return -1;
		}
#endif
		ts.tv_sec += time_ms/1000;
		ts.tv_nsec += (time_ms %1000)*1000* 1000;
#if defined(IOS)||defined(ANDROID)||defined(OSX)
      ret = pthread_join(thread,NULL);
#else
      ret = pthread_join(thread,NULL);
      //ret = pthread_timejoin_np(thread,NULL,&ts);
#endif
		
	}
	if(ret != 0){
		return -1;
	}
	return ret;
#endif
}

int m_thread_exit(m_thread_ret exit_code)
{
#ifdef WIN32
	ExitThread(exit_code);
#else
	pthread_exit(exit_code);
#endif
}

int m_thread_terminate(m_thread_handle thread){
	int r = 0;
#ifdef WIN32
	TerminateThread(thread, 0);
#else
#if defined(ANDROID)
   //TODO
   r = NULL;
#else
   r = pthread_cancel(thread);
#endif
#endif
	return r;
}


MAutolock::MAutolock(m_lock_t* mutex) : mLock(mutex)
{ 
	m_lock(mLock);
}

MAutolock::~MAutolock() { 
	m_unlock(mLock); 
}


#if defined(WIN32)
static bool have_clockfreq = false;
static LARGE_INTEGER clock_freq;
static uint64_t beginTime = 0;

static inline uint64_t get_clockfreq(void)
{
	if (!have_clockfreq)
		QueryPerformanceFrequency(&clock_freq);
	return clock_freq.QuadPart;
}

static uint64_t os_gettime_ns(void)
{

	LARGE_INTEGER current_time;
	double time_val;
	QueryPerformanceCounter(&current_time);
	time_val = (double)current_time.QuadPart;
	time_val *= 1000000000.0;
	time_val /= (double)get_clockfreq();
	return (uint64_t)time_val;
}
#endif

uint64_t get_systime_ms()
{
#if defined(WIN32)
	return static_cast<uint64_t> (os_gettime_ns() / 1000000);
#else
	struct timeval tv;
	gettimeofday(&tv, NULL);
	uint64_t curr = (uint64_t)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
	return curr;
#endif
}
