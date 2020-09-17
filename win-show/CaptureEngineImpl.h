#pragma once
#include "IDshowEngine.h"
#include <atomic>


class DShowInput;

class CaptureEngineImpl:public vlive::IDShowEngine{
public:
   CaptureEngineImpl();
   virtual ~CaptureEngineImpl();
   /*
   * 获取视频设备列表
   */
   virtual void GetVideoDevice(std::vector<DShow::VideoDevice>&);
   /*
   * 获取音频设备列表
   */
   virtual void GetAudioDevice(std::vector<DShow::AudioDevice>&);
   /*
   * 创建设备采集对象
   */
   virtual int CreateInputDevice(DShow::DeviceInputType input_type);
   /*
   *  设置采集设备id,当已经开始采集时可以通过此接口更改采集属性
   */
   virtual int StartCaptureDevice(DShow::VideoConfig &video_config);
   /*
   *  设置采集设备id,当已经开始采集时可以通过此接口更改采集属性
   */
   virtual int SetCaptureDevice(DShow::AudioConfig &audio_config);
   /*
   *  开始采集
   */
   virtual int StartCapture();
   /*
   *  停止采集
   */
   virtual int StopCapture();
private:
   DShowInput* mInput = nullptr;
   DShow::VideoConfig mVideoConfig;
   DShow::AudioConfig mAudioConfig;
   std::atomic_bool mIsCapturing = false;
};

