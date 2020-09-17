#include "vhall_live_push.h"
#include "live_open_define.h"
#include <stdio.h>
#include <string.h>
#include <live_sys.h>
#include <vhall_log.h>

class myObs :public LivePushListener{
public:
	myObs(){};
	virtual ~myObs(){};
	virtual int OnEvent(int type, const std::string content);
};

int  myObs::OnEvent(int type, const std::string content){
	printf("OnEvent : type:%d  content%s", type, content.c_str());
	return 0;
}


int main() {
	LivePushParam param;
	//memset(&param, 0, sizeof(LiveParam));
	param.frame_rate = 25;
	param.width = 1364;
	param.height = 768;
	param.bit_rate = 500000;
	param.gop_interval = 4;
	param.publish_timeout = 2000000; //发起超时
	param.publish_reconnect_times = 99999; //发起重练次数
	param.is_adjust_bitrate = true;
	//param.orientation = 0;    //0 代表横屏，1 代表竖屏
	param.encode_type = ENCODE_TYPE_SOFTWARE;    //0 代表软编，1 代表硬编码
	param.encode_pix_fmt = ENCODE_PIX_FMT_YUV420SP_NV12; //软编码时输入的数据格式  0代表NV21 1代表YUV420sp
	param.live_publish_model = LIVE_PUBLISH_TYPE_VIDEO_ONLY; //直播推流的模式
	//param.gop_interval = 2;
	param.drop_frame_type = DROP_GOPS;

	param.extra_metadata.insert(std::make_pair("upversion", "zhushou2.0"));
	param.extra_metadata.insert(std::make_pair("mykey1", "myvalue1"));
	param.extra_metadata.insert(std::make_pair("mykey2", "myvalue2"));
	FILE *file = fopen("F:/IDE.1364x768.5fps.yuv", "rb");
	if(file == NULL){
		printf("open input file failed!\n");
		return -1;	
	}

	int one_frame_size = 1364 * 768 * 3 / 2;

	VHallLivePush *push = new VHallLivePush();
	myObs *obs = new myObs();

	push->LiveSetParam(&param);
	push->SetListener(obs);
	//SetModuleLog();

	//char path[] = { "f://out.flv" };


	//char url[] = { "http://172.20.1.44:8936/vhall/s2.flv" };
	//char url[] = { "http://172.20.1.2:8936/vhall/sea.flv" };

	char url[] = { "aestp://rs.aestp.t.vhou.net/vhall?token=eyJ0eXAiOiJKV1QiLCJhbGciOiJIUzI1NiJ9.eyJzdHJlYW1uYW1lIjoiNDMwNDM1MTAxIn0.kbiZrpfilLfsRls94S73ZhRqWvqvTqh454ag2BoSwwA/430435101"};
	//char url[] = { "aestp://192.168.1.121:1945/app/stream" };
	int id_rtmp = push->AddMuxer(RTMP_MUXER, url);
	//int id_rtmp = push->AddMuxer(HTTP_FLV_MUXER, url);
	//push ->StartMuxer(id_flv);
	push ->StartMuxer(id_rtmp);
	
	char *frame = new char[one_frame_size];
	memset(frame, 0, one_frame_size);

	int index = 0;

A:
	index = 0;
	fseek(file, 0, SEEK_SET);
	while (index++ < 10000){
		fread(frame, 1, one_frame_size, file);
		push->LivePushVideo(frame, one_frame_size, (uint64_t)0);
		msleep(40);
		
		if (index % 10 == 0){
			printf("stats:\n%s", push->LiveGetRealTimeStatus().c_str());
		}

		//printf("send frame %d\n", index);
	}
	goto A; 

	push->RemoveAllMuxer();
	//getchar();

	system("pause");
}
