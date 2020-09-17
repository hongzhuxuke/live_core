//
//  vioce_dictate.hpp
//  VhallLiveApi
//
//  Created by ilong on 2017/10/10.
//  Copyright © 2017年 vhall. All rights reserved.
//

#ifndef vioce_dictate_packing_h
#define vioce_dictate_packing_h

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "live_open_define.h"

NS_VH_BEGIN
//note: this class only support win linux platform
class VoiceDictatePacking {

public:
	class VoiceDictateDelegate{
	public:
		VoiceDictateDelegate(){};
		virtual ~VoiceDictateDelegate(){};
		virtual void OnResult(const std::string &result, bool is_last) = 0;
		virtual void OnSpeedBegin() = 0;
		virtual void OnSpeedEnd(int reason) = 0;/* 0 if VAD.  others, error : see E_SR_xxx and msp_errors.h  */
	};

   VoiceDictatePacking();
   ~VoiceDictatePacking();
   int Init();
   void SetDelegate(VoiceDictateDelegate *_delegate);
   
   /**
    设置口音

    @param std::string&accent 可取值：mandarin：普通话 cantonese：粤语lmz：四川话 默认值：mandarin
    */
   void SetAccent(std::string&accent);
   int MSPLogin(const std::string &app_id);
   int SessionBegin();
   int SessionEnd();
   void MSPLogout();
   int VDProcess(const int8_t*data_pcm,const int size);
private:
	void EndSrOnError(int errcode);
	void EndSrOnVad(int rec_stat);
private:

   const char * mSessionId;
   VoiceDictateDelegate * mDelegate;
   volatile int mState;
   int mAudioStatus;
   std::string mAccent;
};
NS_VH_END
#endif /* vioce_dictate_hpp */
