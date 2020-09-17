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
   * ��ȡ��Ƶ�豸�б�
   */
   virtual void GetVideoDevice(std::vector<DShow::VideoDevice>&) = 0;
   /*
   * ��ȡ��Ƶ�豸�б�
   */
   virtual void GetAudioDevice(std::vector<DShow::AudioDevice>&) = 0;
   /*
   * �����豸�ɼ�����
   */
   virtual int CreateInputDevice(DShow::DeviceInputType input_type) = 0;
   /*
   *  ���ò���ʼ�ɼ�����Ҫ�ڵ��á�CreateInputDevice��֮����ô˽ӿڡ����Ѿ���ʼ�ɼ�ʱ����ͨ���˽ӿڸ��Ĳɼ�����
   */
   virtual int StartCaptureDevice(DShow::VideoConfig &video_config) = 0;
   /*
   *  ���ò���ʼ�ɼ�����Ҫ�ڵ��á�CreateInputDevice��֮����ô˽ӿڡ����Ѿ���ʼ�ɼ�ʱ����ͨ���˽ӿڸ��Ĳɼ�����
   */
   virtual int SetCaptureDevice(DShow::AudioConfig &audio_config) = 0;
   /*
   *  ֹͣ�ɼ�
   */
   virtual int StopCapture() = 0;
};
/*
*  ����DShowEngine����
*/
VH_IDSENGINE_EXPORT IDShowEngine* CreateEngine();
/*
*  �ͷ�DShowEngine����
*/
VH_IDSENGINE_EXPORT void DestoryEngine(IDShowEngine*);
}


