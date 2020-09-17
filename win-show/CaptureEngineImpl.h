#pragma once
#include "IDshowEngine.h"
#include <atomic>


class DShowInput;

class CaptureEngineImpl:public vlive::IDShowEngine{
public:
   CaptureEngineImpl();
   virtual ~CaptureEngineImpl();
   /*
   * ��ȡ��Ƶ�豸�б�
   */
   virtual void GetVideoDevice(std::vector<DShow::VideoDevice>&);
   /*
   * ��ȡ��Ƶ�豸�б�
   */
   virtual void GetAudioDevice(std::vector<DShow::AudioDevice>&);
   /*
   * �����豸�ɼ�����
   */
   virtual int CreateInputDevice(DShow::DeviceInputType input_type);
   /*
   *  ���òɼ��豸id,���Ѿ���ʼ�ɼ�ʱ����ͨ���˽ӿڸ��Ĳɼ�����
   */
   virtual int StartCaptureDevice(DShow::VideoConfig &video_config);
   /*
   *  ���òɼ��豸id,���Ѿ���ʼ�ɼ�ʱ����ͨ���˽ӿڸ��Ĳɼ�����
   */
   virtual int SetCaptureDevice(DShow::AudioConfig &audio_config);
   /*
   *  ��ʼ�ɼ�
   */
   virtual int StartCapture();
   /*
   *  ֹͣ�ɼ�
   */
   virtual int StopCapture();
private:
   DShowInput* mInput = nullptr;
   DShow::VideoConfig mVideoConfig;
   DShow::AudioConfig mAudioConfig;
   std::atomic_bool mIsCapturing = false;
};

