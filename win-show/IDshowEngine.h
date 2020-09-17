#pragma once
#include "format.hpp"
#include <vector>
#include <windows.h>

#ifdef  VH_IDSENGINE_EXPORT
#define VH_IDSENGINE_EXPORT     __declspec(dllimport)
#else
#define VH_IDSENGINE_EXPORT     __declspec(dllexport)
#endif

namespace vlive {

class IDShowEngine {
public:
   /*
   * 获取视频设备列表
   */
   virtual void GetVideoDevice(std::vector<DShow::VideoDevice>&) = 0;
   /*
   * 获取音频设备列表
   */
   virtual void GetAudioDevice(std::vector<DShow::AudioDevice>&) = 0;
   /*
   * 创建设备采集对象
   */
   virtual int CreateInputDevice(DShow::DeviceInputType input_type) = 0;
   /*
   *  设置并开始采集，需要在调用【CreateInputDevice】之后调用此接口。当已经开始采集时可以通过此接口更改采集属性
   */
   virtual int StartCaptureDevice(DShow::VideoConfig &video_config) = 0;
   /*
   *  设置并开始采集，需要在调用【CreateInputDevice】之后调用此接口。当已经开始采集时可以通过此接口更改采集属性
   */
   virtual int SetCaptureDevice(DShow::AudioConfig &audio_config) = 0;
   /*
   *  停止采集
   */
   virtual int StopCapture() = 0;
};
/*
*  构造DShowEngine对象
*/
VH_IDSENGINE_EXPORT IDShowEngine* CreateEngine();
/*
*  释放DShowEngine对象
*/
VH_IDSENGINE_EXPORT void DestoryEngine(IDShowEngine*);
}


