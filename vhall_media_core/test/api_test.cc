#include <VinnyLiveApi.h>
#include <LiveObs.h>
#include <LiveDefine.h>
#include <stdio.h>
#include <string.h>
#include <Live_sys.h>
#include <vhall_log.h>
class myObs:public LiveObs{
public:
    myObs(){};
   virtual ~myObs(){};
   /**
    *  ????
    *
    *  @param type    ????
    *  @param content ????
    *
    *  @return ????
    */
   virtual int OnEvent(int type, const std::string content);
   /**
    *  ????????
    *
    *  @param data ????
    *  @param size ????
    *  @param w    ???
    *  @param h    ???
    *
    *  @return ????
    */
   virtual int OnRawVideo(const char *data, int size, int w, int h);
   /**
    *  ????????
    *
    *  @param data ??PCM??
    *  @param size ??????
    *
    *  @return ????
    */
   virtual int OnRawAudio(const char *data, int size);
   
   virtual int OnHWDecodeVideo(const char *data, int size, int w, int h, int64_t ts);
   
   virtual DecodedVideoInfo * GetHWDecodeVideo();	
};

int  myObs::OnEvent(int type, const std::string content){
	printf("OnEvent : type:%d  content%s", type, content.c_str());
	return 0;
}

int myObs::OnRawVideo(const char *data, int size, int w, int h){
	printf("OnRawVideo : size:%d  width:%d  height:%d", size, w, h);
	return 0;
}

int myObs::OnRawAudio(const char *data, int size){
	printf("OnRawAudio : size:%d", size);
	return 0;
}

int myObs::OnHWDecodeVideo(const char *data, int size, int w, int h, int64_t ts){
	printf("OnHWDecodeVideo : size:%d width:%d  height:%d ts:%d", size, w, h, ts);
	return 0;	
}

DecodedVideoInfo * myObs::GetHWDecodeVideo(){
	 return NULL;
}


//const char* live_param = "\{\                                                         
//    \"audio_bitrate\" :\"48\",                                     \
//    \"bit_rate\" :\"200\",                                     \
//    \"ch_num\" : \"2\",                                     \
//    \"crf\" :\"28\",                                     \
//    \"frame_rate\" :\"30\",                                     \
//    \"height\" :\"640\",                                     \
//    \"is_hw_encoder\" :\"no\",                                     \
//    \"orientation\" : \"no\",                                     \
//    \"publish_reconnect_times\" : \"10\",                                     \
//    \"publish_timeout\" : \"5000\",                                     \
//    \"watch_timeout\":\"3000\",                                     \
//    \"watch_reconnect_times\":\"10\",                                     \
//    \"buffer_time\" : \"5\",                                     \
//    \"sample_rate\" :\"48000\",                                     \
//    \"width\" :\"720\",                                     \
//    \"live_publish_model\":\"0\",                                     \
//    \"encode_pix_fmt\":\"1\",                                     \
//    \"gop_szie\":\"3\",                                     \  
//    \"video_decoder_mode\":\"0\",                                     \	
//    \"platform\":\"2\",                                     \
//    \"device_type\":\"windows\",                                     \
//    \"device_identifier\":\"xxxxxxxxxxx\",                                     \
//    }                                                                      \
//"
const char *play_param = "{\"buffer_time\":2,\"watch_timeout\":2000,\"watch_reconnect_times\":3,\"device_type\":\"iPhone7,2\",\"device_identifier\":\"C74FF7D4-9F82-4806-AF44-AE74458682C5\",\"platform\":0}";

int main() {
	
	myObs *obs = new myObs();
	VinnyLiveApi::LiveEnableDebug(true);	
	//Test Log 
	FileInitParam param;
	param.nPartionSize = 0;
	param.nPartionTime = 0;
	param.pFilePathName = "E://vhall_logERROR";
	ADD_NEW_LOG(VhallLogType::LogTypeFile, &param, VhallLogLevel::Error);
	
	param.nPartionSize = 0;
	param.nPartionTime = 0;
	param.pFilePathName = "E://vhall_logWARN";
	ADD_NEW_LOG(VhallLogType::LogTypeFile, &param, VhallLogLevel::Warn);

	param.nPartionSize = 0;
	param.nPartionTime = 0;
	param.pFilePathName = "E://vhall_logINFO";
	ADD_NEW_LOG(VhallLogType::LogTypeFile, &param, VhallLogLevel::Info);

	LOGD("%s:%d", "xxxx", 2000);
	LOGI("%s:%f", "xxxx", 1.0);
	LOGW("%s:%d", "xxxx", 2000);
	LOGE("%s:%d", "xxxx", 10);
	

	
	//test play
	VinnyLiveApi * vapi = new VinnyLiveApi(kLivePlayer, "./logs");
	int ret = 0;
	if ((ret=vapi->LiveAddObs(obs)) != 0) {	
		printf("LiveAddObs falied\n");
	}
	
	
	if ((ret=vapi->LiveSetParam(play_param)) != 0) {	
		printf("LiveSetParam falied\n");
	}
	const char* url = "[\"rtmp://cnrtmplive01.open.vhall.com/vhall/e0b151ca0d69664e32cfc009c2338d26\"]";
	
	if ((ret=vapi->LiveStartRecv(url)) != 0) {	
		printf("LiveSetParam falied\n");
	}
	while (1){
		sleep(10);
	}
	if ((ret=vapi->LiveStopRecv()) != 0) {	
		printf("LiveStopRecv falied\n");
	}
}