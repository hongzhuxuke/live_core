//
//  vioce_dictate.cpp
//  VhallLiveApi
//
//  Created by ilong on 2017/10/10.
//  Copyright © 2017年 vhall. All rights reserved.
//

#include "voice_dictate_packing.h"
#include "vhall_log.h"

#if defined(_WIN32)||defined(_LINUX)
#include "qisr.h"
#include "msp_cmn.h"
#include "msp_errors.h"
#endif

#define SLEEP_TIME              200 //ms 
#define END_REASON_VAD_DETECT	0	/* detected speech done  */

/* internal state */
enum {
	SR_STATE_INIT = 0,
	SR_STATE_STARTED
};

/*
* sub:				请求业务类型
* domain:			领域
* language:			语言
* accent:			方言
* sample_rate:		音频采样率
* result_type:		识别结果格式
* result_encoding:	结果编码格式
*
*/
const char* session_begin_params = "sub = iat, domain = iat, language = zh_cn, accent = %s, sample_rate = 16000, result_type = plain, result_encoding = utf-8";

NS_VH_BEGIN
VoiceDictatePacking::VoiceDictatePacking():
mSessionId(NULL),
mDelegate(NULL){
   mAccent = "mandarin";
}

VoiceDictatePacking::~VoiceDictatePacking(){
#if defined(_WIN32)||defined(_LINUX)
	::MSPLogout(); //退出登录
#endif
	mSessionId = nullptr;
}

int VoiceDictatePacking::Init(){
#if defined(_WIN32)||defined(_LINUX)
	mAudioStatus = MSP_AUDIO_SAMPLE_INIT;
#endif
   return 0;
}

void VoiceDictatePacking::SetDelegate(VoiceDictateDelegate *_delegate){
	mDelegate = _delegate;
}

void VoiceDictatePacking::SetAccent(std::string&accent){
   mAccent = accent;
}

int VoiceDictatePacking::MSPLogin(const std::string &app_id){
	//const char* params = "appid = 59e035c3, work_dir = ."; // 登录参数，appid与msc库绑定,请勿随意改动
	char login_params[256] = { 0 };
	sprintf(login_params,"appid = %s, work_dir = .",app_id.c_str());
#if defined(_WIN32)||defined(_LINUX)
	int ret = MSP_SUCCESS;
	/* 用户登录 */
	ret = ::MSPLogin(NULL, NULL, login_params); //第一个参数是用户名，第二个参数是密码，均传NULL即可，第三个参数是登录参数	
	if (MSP_SUCCESS != ret)	{
		LOGE("MSPLogin failed , Error code %d.\n", ret);
		::MSPLogout(); //登录失败，退出登录
		return ret;
	}
   LOGW("MSPLogout succeed!");
#endif
	return 0;
}

void VoiceDictatePacking::MSPLogout(){
#if defined(_WIN32)||defined(_LINUX)
	::MSPLogout(); //退出登录
#endif
}

int VoiceDictatePacking::SessionBegin(){
#if defined(_WIN32)||defined(_LINUX)
	if (mState >= SR_STATE_STARTED){
		LOGI("already STARTED.");
	}else{
      int errcode = MSP_SUCCESS;
      char sessionParams[512] = {0};
      sprintf(sessionParams, session_begin_params,mAccent.c_str());
	  mSessionId = QISRSessionBegin(NULL, sessionParams, &errcode); //听写不需要语法，第一个参数为NULL
		if (MSP_SUCCESS != errcode){
			LOGE("QISRSessionBegin failed! error code:%d", errcode);
			return errcode;
		}
      LOGW("QISRSessionBegin succeed!");
      mAudioStatus = MSP_AUDIO_SAMPLE_INIT;
		mState = SR_STATE_STARTED;
		if (mDelegate){
			mDelegate->OnSpeedBegin();
		}
	}
#endif
	return 0;
}

int VoiceDictatePacking::SessionEnd(){
#if defined(_WIN32)||defined(_LINUX)
	const char * rslt = NULL;
	int ep_stat = 0;
	int rec_stat = 0;
	int ret = 0;
	if (mState < SR_STATE_STARTED) {
		LOGW("Not started or already stopped.");
		return 0;
	}
   mAudioStatus = MSP_AUDIO_SAMPLE_LAST;
	ret = QISRAudioWrite(mSessionId, NULL, 0, mAudioStatus, &ep_stat, &rec_stat);
	if (ret != 0) {
		LOGE("write LAST_SAMPLE failed: %d", ret);
		QISRSessionEnd(mSessionId, "write err");
      mSessionId = NULL;
      mState = SR_STATE_INIT;
		return ret;
	}
   
	while (rec_stat != MSP_REC_STATUS_COMPLETE) {
		rslt = QISRGetResult(mSessionId, &rec_stat, 0, &ret);
		if (MSP_SUCCESS != ret)	{
			LOGW("\nQISRGetResult failed! error code: %d", ret);
			EndSrOnError(ret);
			return ret;
		}
		if (NULL != rslt && mDelegate)
			mDelegate->OnResult(rslt, rec_stat == MSP_REC_STATUS_COMPLETE ? true : false);
		Sleep(SLEEP_TIME); /* for cpu occupy, should sleep here */
	}
	if (mSessionId) {
		QISRSessionEnd(mSessionId, "normal");
		mSessionId = NULL;
		mState = SR_STATE_INIT;
      LOGW("QISRSessionEnd succeed!");
	}
#endif
	return 0;
}

int VoiceDictatePacking::VDProcess(const int8_t*data_pcm, const int size){
#if defined(_WIN32)||defined(_LINUX)
	const char * rslt = NULL;
	int ep_stat = 0;
	int rec_stat = 0;
	int ret = 0;
   SessionBegin();
	if (data_pcm==NULL||size<=0){
		LOGE("data_pcm==NULL||size<=0.");
		return-1;
	}
	if (mAudioStatus == MSP_AUDIO_SAMPLE_INIT){
		mAudioStatus = MSP_AUDIO_SAMPLE_FIRST;
	}
	ret = QISRAudioWrite(mSessionId, data_pcm, size, mAudioStatus, &ep_stat, &rec_stat);
	if (ret) {
		EndSrOnError(ret);
		return ret;
	}
	mAudioStatus = MSP_AUDIO_SAMPLE_CONTINUE;
   
	if (MSP_REC_STATUS_SUCCESS == rec_stat) { //已经有部分听写结果
		rslt = QISRGetResult(mSessionId, &rec_stat, 0,&ret);
		if (MSP_SUCCESS != ret)	{
			LOGE("\nQISRGetResult failed! error code: %d", ret);
			EndSrOnError(ret);
			return ret;
		}
		if (NULL != rslt && mDelegate)
		    mDelegate->OnResult(rslt, rec_stat == MSP_REC_STATUS_COMPLETE ? true : false);
	}
	if (MSP_EP_AFTER_SPEECH == ep_stat)
		EndSrOnVad(rec_stat);
#endif
   return 0;
}

void VoiceDictatePacking::EndSrOnVad(int rec_stat){
#if defined(_WIN32)||defined(_LINUX)
	int errcode = MSP_SUCCESS;
	const char * rslt = NULL;
	while (rec_stat != MSP_REC_STATUS_COMPLETE){
		rslt = QISRGetResult(mSessionId, &rec_stat, 0, &errcode);
		if (MSP_SUCCESS != errcode){
			LOGE("\nQISRGetResult failed! error code: %d", errcode);
			EndSrOnError(errcode);
			return;
		}
		if (NULL != rslt && mDelegate)
			mDelegate->OnResult(rslt, rec_stat == MSP_REC_STATUS_COMPLETE ? true : false);

		Sleep(SLEEP_TIME); /* for cpu occupy, should sleep here */
	}
	if (mSessionId) {
		if (mDelegate)
			mDelegate->OnSpeedEnd(END_REASON_VAD_DETECT);
		QISRSessionEnd(mSessionId, "VAD Normal");
		mSessionId = NULL;
		mState = SR_STATE_INIT;
	}
#endif
}

void VoiceDictatePacking::EndSrOnError(int errcode){
#if defined(_WIN32)||defined(_LINUX)
	if (mSessionId) {
		if (mDelegate)
			mDelegate->OnSpeedEnd(errcode);

		QISRSessionEnd(mSessionId, "err");
		mSessionId = NULL;
		mState = SR_STATE_INIT;
	}
#endif
}
NS_VH_END
