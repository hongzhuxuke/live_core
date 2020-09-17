#include "vhall_live_player.h"
#include "live_open_define.h"
#include <stdio.h>
#include <string.h>
#include <live_sys.h>
#include <vhall_log.h>

class myObs :public LiveObs{
public:
	myObs(){};
	virtual ~myObs(){};
	virtual int OnEvent(int type, const std::string content);

	virtual int OnRawVideo(const char *data, int size, int w, int h);
	/**
	*  抛出原始音频数据
	*
	*  @param data 音频PCM数据
	*  @param size 音频数据大小
	*
	*  @return 是否成功
	*/
	virtual int OnRawAudio(const char *data, int size) ;

	virtual int OnHWDecodeVideo(const char *data, int size, int w, int h, int64_t ts);

	virtual DecodedVideoInfo * GetHWDecodeVideo(){ return NULL; };


};

int  myObs::OnEvent(int type, const std::string content){
	printf("OnEvent : type:%d  content%s\n", type, content.c_str());
	return 0;
}

int myObs::OnRawVideo(const char *data, int size, int w, int h)
{
	printf("OnRawVideo : size:%d  w:%d   h:%d\n", size, w, h);
	return 0;
}

int myObs::OnRawAudio(const char *data, int size)
{
	printf("OnRawAudio : size:%d\n", size);
	return 0;
}

int myObs::OnHWDecodeVideo(const char *data, int size, int w, int h, int64_t ts)
{
	printf("OnHWDecodeVideo : size:%d  w:%d   h:%d  ts:%d\n", size, w, h, ts);
	return 0;
}

int main() {
	SetPlayModuleLog("D://play_test.log", 1);
	VHallLivePlayer *player = new VHallLivePlayer();

	myObs *obs = new myObs();
	player->AddObs(obs);
	LivePlayerParam param;

	//param.buffer_time = 10;
	player->LiveSetParam(&param);
	//player->SetBufferTime(10); //http-flv need more delay
	player->SetDemuxer(HTTP_FLV_MUXER);
	//player->Start("rtmp://172.20.1.44/vhall/ss");
	while (1){
		player->Start("[\"http://t-cnflvlive01.e.vhall.com/vhall/430435101.flv\"]");
		msleep(1000 * 20);
		player->Stop();
	}
	system("pause");
}
