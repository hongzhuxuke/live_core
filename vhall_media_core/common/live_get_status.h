#ifndef __LIVE_GET_STATUES_H__
#define __LIVE_GET_STATUES_H__
/*
athor：liulichuan
email：lichuan.liu@vhall.com
interface to get real time status for debug monitor.
*/

namespace VHJson {
   class Value;
}

class LiveGetStatus{
public:
   LiveGetStatus(){};
   virtual ~LiveGetStatus(){};
	virtual bool LiveGetRealTimeStatus(VHJson::Value & value){
		return true;
	};
};

#endif
