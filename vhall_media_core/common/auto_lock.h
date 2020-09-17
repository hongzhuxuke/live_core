#ifndef __AUTO_LOCK_H__
#define __AUTO_LOCK_H__

#include "live_sys.h"

class VhallAutolock {
public:
	//构造的时候调用lock。  
	//inline VhallAutolock(vhall_lock_t& mutex) : mLock(&mutex)  { vhall_lock(mLock); }
	inline VhallAutolock(vhall_lock_t* mutex) : mLock(mutex) { vhall_lock(mLock); }
	//析构的时候调用unlock。  
	inline ~VhallAutolock() { vhall_unlock(mLock); }
private:
   VhallAutolock(const VhallAutolock& )=delete;//禁用copy方法
   const VhallAutolock& operator=( const VhallAutolock& )=delete;//禁用赋值方法
	vhall_lock_t* mLock;
};

#endif
