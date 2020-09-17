语音智能转换模块说明文档

结构说明：
voicetransition               - 主目录 
 |-voice_dictate_packing.h    - 科大语音转换接口封装的头文件
 |-voice_dictate_packing.cpp  - 科大语音转换接口封装的实现文件
 |-voice_transition.h         - 语音转换流程控制的头文件
 |-voice_transition.cpp       - 语音转换流程控制的实现文件
 |-image_text_mixing.h        - 图片文本融合封装的头文件 注：此文件在Utility文件夹
 |-image_text_mixing.cpp      - 图片文本融合封装的实现文件 注：此文件在Utility文件夹
 
使用说明：
一，VS工程配置说明：
  1，拉取media_core dev分支代码；
  2，使用cmake工具 Configure->Generate；
  3，打开cmake的输出文件夹，找到bin文件夹，将里面的msc文件夹，msc.dll文件，boleyou.ttf文件复制到小助手项目的运行目录中；
  4，打开小助手工程：右键点击VhallMediaCore文件夹->添加->现有项目->选择vhall_voice_transition项目；
  5，打开小助手工程OBSWraped文件夹：右键点击MediaCore项目->属性->配置属性->链接器->输入->附加依赖项->添加vhall_voice_transition.lib msc.lib(科大语音听写库) avfilter.lib；
  6，右键点击MediaCore项目->属性->配置属性->VC++目录->包含目录中添加voicetransition的文件路径；
二，接口使用说明：
   
   USING_NS_VH

   auto voiceTransition = new(std::nothrow) VoiceTransition();
   if(voiceTransition==NULL){
     LOGE("VoiceTransition new error.");
     return;
   }
   voiceTransition->SetDelegate(VoiceTransitionDelegate *delegate);
   voiceTransition->SetAudioInfo(const int channel_num,const VHAVSampleFormat sample_fmt, const int sample_rate);
   voiceTransition->SetVideoInfo(const int width,const int height,const EncodePixFmt in_pix_fmt,const int frame_rate);
   int ret = voiceTransition->StartPrepare(const std::string &app_id); //可使用“59e035c3”作为测试Id，发布时的id需要产品提供
   if(ret<0){
      LOGW("start error:%d",ret);
   }
   //输入音视频数据
   voiceTransition->InputVideoData(const int8_t *data,const int size);
   voiceTransition->InputAudioData(const int8_t *data,const int size);

   //监听回调接口数据音视频数据
   virtual void OnOutputVideoData(const int8_t *data, const int size) = 0 ;
   virtual void OnOutputAudioData(const int8_t *data, const int size) = 0;
   virtual void OnResult(const std::string &result, bool is_last){};

   //其他接口设置 字体大小，字体颜色，字体库，设置口音，





