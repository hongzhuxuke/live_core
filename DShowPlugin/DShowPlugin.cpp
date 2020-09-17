/********************************************************************************
 Copyright (C) 2012 Hugh Bailey <obs.jim@gmail.com>

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 ********************************************************************************/
#include "Utility.h"
#include "DShowPlugin.h"
#include "Logging.h"  
#include "dshow-media-type.hpp"
#define DSHOW_CONFIG_FILE "/dshowVh.list"
static DShowVideoFilterDeviceManager *G_DShowVideoFilterManager = NULL;
static VideoDeviceAttributeManager *G_VideoDeviceAttributeManager = NULL;
Logger *gLogger = NULL;
DShowVideoFilterDevice::DShowVideoFilterDevice() :
IDShowVideoFilterDevice() {
   m_pDevice = new DShow::Device(DShow::InitGraph::True);
   m_audioMutex = OSCreateMutex();
   m_videoMutex = OSCreateMutex();
   memset(m_videoBuffer, 0, sizeof(m_videoBuffer));
   m_videoBufferLocked = false;
   m_videoBufferIndex = 0;
   m_videoDataLen = 0;
   m_videoDataBuffer = NULL;
}
DShowVideoFilterDevice::~DShowVideoFilterDevice() {
   if (m_pDevice) {
      m_pDevice->Stop();
      delete m_pDevice;
      m_pDevice = NULL;
   }
   if (m_audioMutex) {
      OSCloseMutex(m_audioMutex);
      m_audioMutex = NULL;
   }

   if (m_videoMutex) {
      OSCloseMutex(m_videoMutex);
      m_videoMutex = NULL;
   }

   if (m_videoDataBuffer){
      delete []m_videoDataBuffer;
      m_videoDataBuffer = NULL;
   }
}
bool DShowVideoFilterDevice::GetNextAudioBuffer(void **buffer, unsigned int *numFrames, unsigned long long *timestamp) {
   return false;
}

bool DShowVideoFilterDevice::GetNextVideoBuffer
(void **buffer, unsigned long long *bufferSize, unsigned long long *timestamp,
VideoDeviceSetting &setting) {
   bool bRet = true;
   OSEnterMutex(m_videoMutex);
   m_videoOutIndex = m_videoBufferIndex;
   if (!m_videoBufferLocked) {
      m_videoBufferLocked = true;
      m_videoBufferIndex++;
      m_videoBufferIndex %= VIDEOBUFFERCOUNT;
   }

   if (m_videoBuffer[m_videoOutIndex].size > 0) {
      *bufferSize = m_videoBuffer[m_videoOutIndex].size;
      *timestamp = m_videoBuffer[m_videoOutIndex].endTime;
      setting.cx = m_videoBuffer[m_videoOutIndex].cx;
      setting.cy = m_videoBuffer[m_videoOutIndex].cy;

      if (m_videoDataBuffer != NULL && m_videoDataLen != m_videoBuffer[m_videoOutIndex].size) {
         delete []m_videoDataBuffer;
         m_videoDataBuffer = NULL;
         m_videoDataBuffer = new unsigned char[m_videoBuffer[m_videoOutIndex].size + 1];
      }
      if (m_videoDataBuffer == NULL) {
         m_videoDataBuffer = new unsigned char[m_videoBuffer[m_videoOutIndex].size + 1];
      }

      m_videoDataLen = m_videoBuffer[m_videoOutIndex].size;
      if (m_videoDataBuffer != NULL) {
         memset(m_videoDataBuffer, 0, m_videoBuffer[m_videoOutIndex].size + 1);
         memcpy(m_videoDataBuffer, m_videoBuffer[m_videoOutIndex].data, m_videoBuffer[m_videoOutIndex].size);
         *buffer = m_videoDataBuffer;
      }

   } else {
      bRet = false;
   }
   OSLeaveMutex(m_videoMutex);
   return bRet;
}
void DShowVideoFilterDevice::ReleaseVideoBuffer() {
   OSEnterMutex(m_videoMutex);
   m_videoBufferLocked = false;
   m_videoBuffer[m_videoOutIndex].Clear();
   OSLeaveMutex(m_videoMutex);
}

//视频到达
void DShowVideoFilterDevice::OnVideoData(const DShow::VideoConfig &config,
                                         unsigned char *data,
                                         size_t size,
                                         long long startTime,
                                         long long endTime) {
   OSEnterMutex(m_videoMutex);
   if (startTime == 0 && endTime == 0) {
      if (wstring(m_deviceInfo.m_sDeviceName) != wstring(L"YY开播")) {
         return;
      }
   }
   m_videoBufferLocked = false;
   m_videoBuffer[m_videoBufferIndex].SetData(data, size, startTime, endTime,
                                             config.format, config.cx, config.cy);
   //VHLogDebug(L"DShowVideoFilterDevice::OnVideoData(): size:%d , startTime:%I64d endTime:%I64d cx:%d cy:%d",size, startTime, endTime, config.cx, config.cy);
   OSLeaveMutex(m_videoMutex);

}
//音频到达
void DShowVideoFilterDevice::OnAudioData(const DShow::AudioConfig &config,
                                         unsigned char *data,
                                         size_t size,
                                         long long startTime,
                                         long long endTime) {

   if (!m_audioFunc || !m_audioParam) {
      return;
   }

   ((void(*)(unsigned char *, size_t, long long, long long, HANDLE))m_audioFunc)(
      data,
      size,
      startTime,
      endTime,
      m_audioParam
      );

}


//音视频同步采集
bool DShowVideoFilterDevice::Start(DeviceInfo info,
                                   VideoDeviceSetting &setting,
                                   DShowDeviceType deviceType,
                                   DShowDevicePinType pinType,
                                   HANDLE handle,
                                   HANDLE pFunc) {
   VHLogDebug(L"DShowVideoFilterDevice::Start");
   if (!m_pDevice) {
      return false;
   }
   if (m_pDevice) {
      m_pDevice->Stop();
      VHLogDebug(L"DShowVideoFilterDevice::Start  m_pDevice Stop");
   }
   if (pinType == DShowDeviceType_Video) {
      VHLogDebug(L"DShowVideoFilterDevice::Start DShowDeviceType_Video");
      DShow::VideoConfig videoConfig;
      videoConfig.name = info.m_sDeviceName;
      videoConfig.path = info.m_sDeviceID;
      videoConfig.callback = std::bind(&DShowVideoFilterDevice::OnVideoData, this,
                                       placeholders::_1, placeholders::_2,
                                       placeholders::_3, placeholders::_4,
                                       placeholders::_5);

      if (wcslen(info.m_sDeviceName) != 0) {
         VHLogDebug(L"DShowVideoFilterDevice::Start DShowDeviceType_Video Name=[%s]", info.m_sDeviceName);
      } else {
         VHLogDebug(L"DShowVideoFilterDevice::Start DShowDeviceType_Video Name=[NULL]");
      }

      if (wcslen(info.m_sDeviceID) != 0) {
         VHLogDebug(L"DShowVideoFilterDevice::Start DShowDeviceType_Video Id=[%s]", info.m_sDeviceID);
      } else {
         VHLogDebug(L"DShowVideoFilterDevice::Start DShowDeviceType_Video Id=[NULL]");
      }

      UINT width = 1280;
      UINT height = 720;
      int  frameInternal = 333333;
      DeinterlacingType type = DEINTERLACING_NONE;
      VideoFormat format = VideoFormat::Any;
      if (!GetDeviceDefaultAttribute(info, width, height, frameInternal, type, format)) {
         VHLogDebug(L"DShowVideoFilterDevice::Start useDefaultConfig");
         videoConfig.useDefaultConfig = true;
      } else {
         videoConfig.useDefaultConfig = false;
         videoConfig.cx = width;
         videoConfig.cy = height;
         videoConfig.frameInterval = frameInternal;
         videoConfig.internalFormat = format;
         if (videoConfig.internalFormat == VideoFormat::XRGB) {
            videoConfig.format = VideoFormat::ARGB;
         }
         VHLogDebug(L"DShowVideoFilterDevice::Start useDefaultConfig %u,%u,%d", width, height, frameInternal);
      }

      if (!m_pDevice->SetVideoConfig(&videoConfig)) {
         VHLogDebug(L"DShowVideoFilterDevice::Start SetVideoConfig Failed!");
      }

      if (videoConfig.internalFormat == VideoFormat::MJPEG) {
         videoConfig.format = VideoFormat::XRGB;
         if (!m_pDevice->SetVideoConfig(&videoConfig)) {
            VHLogDebug(L"DShowVideoFilterDevice::Start SetVideoConfig Failed!2");
         }
      }

      VHLogDebug(L"DShowVideoFilterDevice::Start m_pDevice->GetVideoMediaType(");
      VideoOutputType outtype;

      m_pDevice->GetVideoMediaType(&outtype);

      if (outtype == VideoOutputType_I420)
         setting.colorType = DeviceOutputType_I420;
      else if (outtype == VideoOutputType_YV12)
         setting.colorType = DeviceOutputType_YV12;
      else if (outtype == VideoOutputType_YVYU)
         setting.colorType = DeviceOutputType_YVYU;
      else if (outtype == VideoOutputType_YUY2)
         setting.colorType = DeviceOutputType_YUY2;
      else if (outtype == VideoOutputType_UYVY)
         setting.colorType = DeviceOutputType_UYVY;
      else if (outtype == VideoOutputType_HDYC) {
         setting.colorType = DeviceOutputType_HDYC;
         setting.use709 = true;
	  }
	  else if(VideoOutputType_Y41P== outtype || VideoOutputType_YVU9==outtype ||
		  (outtype>=VideoOutputType_MPEG2_VIDEO && outtype <= VideoOutputType_MJPG)
		  ||(outtype>VideoOutputType_None&&outtype<= VideoOutputType_ARGB32) )
	  {
			  setting.colorType = DeviceOutputType_RGB;
	  }
	  /*else if (VideoOutputType_RGB24 == outtype)
      {
         setting.colorType = DeviceOutputType_RGB24;
      }
	  else if (VideoOutputType_RGB32 == outtype)
	  {
		  setting.colorType = DeviceOutputType_RGB32;
	  }
	  else if (VideoOutputType_ARGB32 == outtype)
	  {
		  setting.colorType = DeviceOutputType_ARGB32;
	  }
	  else if (VideoOutputType_MPEG2_VIDEO == outtype )
	  {
		  setting.colorType = DeviceOutputType_MPEG2_VIDEO;
	  }
	  else if (VideoOutputType_H264 == outtype)
	  {
		  setting.colorType = DeviceOutputType_H264;
	  }
	  else if (VideoOutputType_dvsl == outtype)
	  {
		  setting.colorType = DeviceOutputType_dvsl;
	  }
	  else if (VideoOutputType_dvsd == outtype)
	  {
		  setting.colorType = DeviceOutputType_dvsd;
	  }
	  else if (VideoOutputType_dvhd == outtype)
	  {
		  setting.colorType = DeviceOutputType_dvhd;
	  }
	  else if (DeviceOutputType_MJPG == outtype)
	  {
		  setting.colorType = DeviceOutputType_MJPG;
	  }
	  else {
		  setting.colorType = DeviceOutputType_UNKNOWN;
	  }*/
      VHLogDebug(L"DShowVideoFilterDevice::Start outtype=%d", (int)type);

   } else if (pinType == DShowDeviceType_Audio) {
      VHLogDebug(L"DShowVideoFilterDevice::Start DShowDeviceType_Audio");
      m_audioConfig.callback = std::bind(&DShowVideoFilterDevice::OnAudioData, this,
                                         placeholders::_1, placeholders::_2,
                                         placeholders::_3, placeholders::_4,
                                         placeholders::_5);

      m_audioConfig.deviceType = deviceType;
      m_audioConfig.pinType = pinType;
      m_audioConfig.useDefaultConfig = true;
      m_audioConfig.name = info.m_sDeviceName;
      m_audioConfig.path = info.m_sDeviceID;

      if (wcslen(info.m_sDeviceName) != 0) {
         VHLogDebug(L"DShowVideoFilterDevice::Start DShowDeviceType_Video Name=[%s]", info.m_sDeviceName);
      } else {
         VHLogDebug(L"DShowVideoFilterDevice::Start DShowDeviceType_Video Name=[NULL]");
      }

      if (wcslen(info.m_sDeviceID) != 0) {
         VHLogDebug(L"DShowVideoFilterDevice::Start DShowDeviceType_Video Id=[%s]", info.m_sDeviceID);
      } else {
         VHLogDebug(L"DShowVideoFilterDevice::Start DShowDeviceType_Video Id=[NULL]");
      }
      if (!m_pDevice->SetAudioConfig(&m_audioConfig)) {

         VHLogDebug(L"DShowVideoFilterDevice::Start DShowDeviceType_Audio Failed!");
         return false;
      }

      m_audioFunc = pFunc;
      m_audioParam = handle;
      VHLogDebug(L"DShowVideoFilterDevice::Start DShowDeviceType_Audio Successed!");
   }

   this->m_deviceInfo = info;
   this->m_deviceType = deviceType;
   m_pDevice->Start();
   VHLogDebug(L"DShowVideoFilterDevice::Start End");
   return true;
}
bool DShowVideoFilterDevice::GetAudioConfig(
   UINT &mInputBitsPerSample,
   bool &bFloat,
   UINT &mInputSamplesPerSec,
   UINT &mInputChannels,
   UINT &mInputBlockSize
   ) {


   if (m_audioConfig.format == DShow::AudioFormat::WaveFloat) {
      mInputBitsPerSample = 32;
      bFloat = true;
   } else if (m_audioConfig.format == DShow::AudioFormat::Wave16bit) {
      mInputBitsPerSample = 16;
      bFloat = false;
   }

   mInputSamplesPerSec = m_audioConfig.sampleRate;
   mInputChannels = m_audioConfig.channels;
   mInputBlockSize = (m_audioConfig.channels*mInputBitsPerSample) / 8;

   return true;
}


void DShowVideoFilterDevice::Stop(DShowDevicePinType type) {
   if (!m_pDevice) {
      return;
   }
   if (type == DShowDevicePinType_Video) {
      m_pDevice->Stop();
      m_pDevice->SetVideoConfig(NULL);
   } else if (type == DShowDevicePinType_Audio) {
      m_pDevice->Stop();
      m_pDevice->SetAudioConfig(NULL);
   }
}

bool DShowVideoFilterDevice::InternalStart() {
   if (!m_pDevice->ConnectFilters()) {
      return false;
   }
   return m_pDevice->Start() == DShow::Result::Success;
}

bool DShowVideoFilterDevice::Equal(DeviceInfo info, DShowDeviceType type) {
   if (m_deviceInfo == info) {
      if (m_deviceType == type) {
         return true;
      }
   }
   return false;
}

//设置视频配置属性
bool DShowVideoFilterDevice::SetVideoConfig(VideoDeviceSetting setting) {
   /*
      //帧率
      UINT64 frameInfo;
      //分辨率
      UINT cx;
      UINT cy;
      //去交错类型
      DeinterlacingType type;
      */
   return true;
}
const GUID MEDIASUBTYPE_I420 = { 0x30323449, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };

bool DShowVideoFilterDevice::GetVideoDeviceSettingsWithColorType(VideoDeviceSetting *setting) {
   if (m_pDevice) {
      VideoOutputType type = VideoOutputType_RGB24;
      if (m_pDevice->GetVideoMediaType(&type)) {

         if (type == VideoOutputType_I420)
            setting->colorType = DeviceOutputType_I420;
         else if (type == VideoOutputType_YV12)
            setting->colorType = DeviceOutputType_YV12;
         else if (type == VideoOutputType_YVYU)
            setting->colorType = DeviceOutputType_YVYU;
         else if (type == VideoOutputType_YUY2)
            setting->colorType = DeviceOutputType_YUY2;
         else if (type == VideoOutputType_UYVY)
            setting->colorType = DeviceOutputType_UYVY;
         else if (type == VideoOutputType_HDYC) {
            setting->colorType = DeviceOutputType_HDYC;
            setting->use709 = true;
		 }
		 else {
				 setting->colorType = DeviceOutputType_RGB;
		 }
		 /*else if (VideoOutputType_RGB24 == type)
		 {
			 setting->colorType = DeviceOutputType_RGB24;
		 }
		 else if (VideoOutputType_RGB32 == type)
		 {
			 setting->colorType = DeviceOutputType_RGB32;
		 }
		 else if (VideoOutputType_ARGB32 == type)
		 {
			 setting->colorType = DeviceOutputType_ARGB32;
		 }
		 else if (VideoOutputType_MPEG2_VIDEO == type)
		 {
			 setting->colorType = DeviceOutputType_MPEG2_VIDEO;
		 }
		 else if (VideoOutputType_H264 == type)
		 {
			 setting->colorType = DeviceOutputType_H264;
		 }
		 else if (VideoOutputType_dvsl == type)
		 {
			 setting->colorType = DeviceOutputType_dvsl;
		 }
		 else if (VideoOutputType_dvsd == type)
		 {
			 setting->colorType = DeviceOutputType_dvsd;
		 }
		 else if (VideoOutputType_dvhd == type)
		 {
			 setting->colorType = DeviceOutputType_dvhd;
		 }
		 else if (DeviceOutputType_MJPG == type)
		 {
			 setting->colorType = DeviceOutputType_MJPG;
		 }
		 else {
			 setting->colorType = DeviceOutputType_UNKNOWN;
		 }*/

         return true;

      }
   }
   return false;
}
bool DShowVideoFilterDevice::GetDShowVideoFrameInfoList
(FrameInfoList *frameInfoList,
FrameInfo *currentFrameInfo) {
   if (!m_pDevice) {
      return false;
   }

   return m_pDevice->GetCurrDeviceFrameInfoList(frameInfoList, currentFrameInfo);
}
bool DShowVideoFilterDevice::SetDeviceDefaultAttribute(int width, int height, int frameInternal, VideoFormat format) {
   if (!m_pDevice) {
      return false;
   }

   return m_pDevice->SetDeviceDefaultAttribute(width, height, frameInternal, format);

}

DShowVideoFilterDeviceManager::DShowVideoFilterDeviceManager() {
   hDeviceFilterMutex = OSCreateMutex();
}
DShowVideoFilterDeviceManager::~DShowVideoFilterDeviceManager() {
   if (hDeviceFilterMutex) {
      OSCloseMutex(hDeviceFilterMutex);
      hDeviceFilterMutex = NULL;
   }
}

DShowVideoFilterDevice *DShowVideoFilterDeviceManager::GetDeviceFilter
(
IDShowVideoFilterDevice *&device,
DeviceInfo deviceInfo,
VideoDeviceSetting &setting,
DShowDeviceType deviceType,
DShowDevicePinType pinType,
HANDLE handle,
HANDLE pFunc) {

   EnterMutex();
   DShowVideoFilterDevice *filter = NULL;
   for (auto itor = m_DeviceList.begin(); itor != m_DeviceList.end(); itor++) {
      if (itor->second->Equal(deviceInfo, deviceType)) {

         VHLogDebug(L" DShowVideoFilterDeviceManager::ReleaseDShowVideoFilter  get from m_DeviceList  input pinType:%d", pinType);
         VHLogDebug(L" DShowVideoFilterDeviceManager::ReleaseDShowVideoFilter  get from m_DeviceList  m_sDeviceType.:%d", deviceInfo.m_sDeviceType);
         VHLogDebug(L" DShowVideoFilterDeviceManager::ReleaseDShowVideoFilter  get from m_DeviceList  m_sDeviceName:%s", deviceInfo.m_sDeviceName);
         VHLogDebug(L" DShowVideoFilterDeviceManager::ReleaseDShowVideoFilter  get from m_DeviceList  m_sDeviceID:%s", deviceInfo.m_sDeviceID);
         VHLogDebug(L" DShowVideoFilterDeviceManager::ReleaseDShowVideoFilter  get from m_DeviceList  m_sDeviceDisPlayName:%s", deviceInfo.m_sDeviceDisPlayName);

         //有一种场景：如果音频设备采用了audioDeviceByVideoFilter获取，则共用一个filter.这两个filter分别给AudioCaptureThread中micDevice, 还有MainCaptureLoopThread中场景使用。
         filter = itor->second;
         m_DeviceList[handle] = filter; 
         m_DevicePinTypeList[handle] = pinType;
         filter->Start(deviceInfo, setting, deviceType, pinType, handle, pFunc);
         LeaveMutex();
         return filter;
      }
   }

   filter = new DShowVideoFilterDevice();
   m_DeviceList[handle] = filter;
   m_DevicePinTypeList[handle] = pinType;

   VHLogDebug(L" DShowVideoFilterDeviceManager::ReleaseDShowVideoFilter  new DShowVideoFilterDevice  pinType:%d", pinType);
   VHLogDebug(L" DShowVideoFilterDeviceManager::ReleaseDShowVideoFilter  new DShowVideoFilterDevicet  input pinType:%d", pinType);
   VHLogDebug(L" DShowVideoFilterDeviceManager::ReleaseDShowVideoFilter  new DShowVideoFilterDevice  m_sDeviceType.:%d", deviceInfo.m_sDeviceType);
   VHLogDebug(L" DShowVideoFilterDeviceManager::ReleaseDShowVideoFilter  new DShowVideoFilterDevice  m_sDeviceName:%s", deviceInfo.m_sDeviceName);
   VHLogDebug(L" DShowVideoFilterDeviceManager::ReleaseDShowVideoFilter  new DShowVideoFilterDevice  m_sDeviceID:%s", deviceInfo.m_sDeviceID);
   VHLogDebug(L" DShowVideoFilterDeviceManager::ReleaseDShowVideoFilter  new DShowVideoFilterDevice  m_sDeviceDisPlayName:%s", deviceInfo.m_sDeviceDisPlayName);
   if (pinType == DShowDevicePinType_Audio) {
      filter->Stop(DShowDevicePinType_Video);
      filter->Start(deviceInfo, setting, deviceType, DShowDevicePinType_Video, handle, pFunc);

   }
   if (!filter->Start(deviceInfo, setting, deviceType, pinType, handle, pFunc)) {
      LeaveMutex();
      return NULL;
   }
   LeaveMutex();
   return filter;
}
bool DShowVideoFilterDeviceManager::ReleaseDShowVideoFilter(HANDLE handle) {
   EnterMutex();
   bool ret = false;
   do {//查找当前句柄对设备的引用
      auto itor = m_DeviceList.find(handle);
      if (itor == m_DeviceList.end()) {
         break;
      }
	  //从列表中删除句柄对设备的引用
      DShowVideoFilterDevice *device = itor->second;
      m_DeviceList.erase(itor);
	  //查找句柄对应的类型
      auto itorType = m_DevicePinTypeList.find(handle);
      if (itorType == m_DevicePinTypeList.end()) {
         delete device;
         device = NULL;
         break;
      }

      DShowDevicePinType type = itorType->second;
      m_DevicePinTypeList.erase(itorType);

      //VHLogDebug(L"DShowVideoFilterDeviceManager::ReleaseDShowVideoFilter  DShowDevicePinType:%d m_DeviceList.size:%d", type, m_DeviceList.size());

      if (device) {
         auto itor = m_DeviceList.begin();
         while (itor != m_DeviceList.end()) {
            if (itor->second == device) {
				//停止设备对应当前句柄的类型
               device->Stop(type);
			   //启动设备的另外类型
               device->InternalStart();
               VHLogDebug(L" DShowVideoFilterDeviceManager::ReleaseDShowVideoFilter  remove froma m_DeviceList");
               ret = true;
               m_DeviceList.erase(itor);
               itor = m_DeviceList.begin();
            }
            else {
               itor++;
            }
         }
      }
      if (device) {//设引用备无其他
         delete device;
         device = NULL;
      }
      
      ret = true;
   } while (false);
   LeaveMutex();
   return ret;
}

bool  DShowVideoFilterDeviceManager::IsDShowVideoFilterExist(HANDLE handle) {
   EnterMutex();
   bool ret = false;
   auto itor = m_DeviceList.find(handle);
   if (itor != m_DeviceList.end()) {
      ret = true;
   }
   LeaveMutex();
   return ret;
}

bool DShowVideoFilterDeviceManager::GetDShowVideoFrameInfoList(DeviceInfo deviceInfo, FrameInfoList *frameInfoList, FrameInfo *currentFrameInfo) {
   EnterMutex();
   bool ret = false;
   for (auto itor = m_DeviceList.begin(); itor != m_DeviceList.end(); itor++) {
      DShowVideoFilterDevice *device = itor->second;
      if (device->Equal(deviceInfo, DShowDeviceType_Video)) {
         ret = device->GetDShowVideoFrameInfoList(frameInfoList, currentFrameInfo);
         break;
      }
   }
   LeaveMutex();
   return ret;
}
//设置属性
bool DShowVideoFilterDeviceManager::SetDeviceDefaultAttribute(
   DeviceInfo deviceInfo,
   UINT width,
   UINT height,
   int frameInternal, VideoFormat format) {
   EnterMutex();
   bool bRet = false;
   for (auto itor = m_DeviceList.begin(); itor != m_DeviceList.end(); itor++) {
      DShowVideoFilterDevice *device = itor->second;
      if (device->Equal(deviceInfo, DShowDeviceType_Video)) {
         bRet = device->SetDeviceDefaultAttribute(width, height, frameInternal, format);
      }
   }
   LeaveMutex();
   return bRet;
}

void DShowVideoFilterDeviceManager::EnterMutex() {
   if (hDeviceFilterMutex) {
      OSEnterMutex(hDeviceFilterMutex);
   }
}

void DShowVideoFilterDeviceManager::LeaveMutex() {
   if (hDeviceFilterMutex) {
      OSLeaveMutex(hDeviceFilterMutex);
   }
}

VideoDeviceAttributeManager::VideoDeviceAttributeManager
(FUNCFileOpen vhopen,
FUNCFileClear vhclear,
FUNCFileRead vhread,
FUNCFileWrite vhwrite,
FUNCFileClose vhclose) {
   m_vhopen = vhopen;
   m_vhclear = vhclear;
   m_vhread = vhread;
   m_vhwrite = vhwrite;
   m_vhclose = vhclose;
   LoadConfig();
}
VideoDeviceAttributeManager::~VideoDeviceAttributeManager() {

}

bool VideoDeviceAttributeManager::SetDeviceDefaultAttribute(
   DeviceInfo deviceInfo,
   UINT width,
   UINT height,
   int frameInternal,
   DeinterlacingType type, VideoFormat format) {
   DeviceInfoAttribute attr;
   attr.width = width;
   attr.height = height;
   attr.frameInternal = frameInternal;
   attr.type = type;
   attr.format = format;


   if (!(deviceInfo == DeviceInfo()) && width != 0 && height != 0) {
      m_DeviceAttributes[deviceInfo] = attr;
   }

   SaveConfig();
   return true;
}

//获取设备属性
bool VideoDeviceAttributeManager::GetDeviceDefaultAttribute(
   DeviceInfo deviceInfo,
   UINT &width,
   UINT &height,
   int &frameInternal,
   DeinterlacingType &type,
   VideoFormat &format) {
   auto itor = m_DeviceAttributes.find(deviceInfo);
   if (itor == m_DeviceAttributes.end()) {
      VHLogDebug(L"VideoDeviceAttributeManager::GetDeviceDefaultAttribute Failed!1");
      return false;
   }
   DeviceInfoAttribute attr = itor->second;
   if (attr.isEmpty()) {
      VHLogDebug(L"VideoDeviceAttributeManager::GetDeviceDefaultAttribute Failed!2");
      return false;
   }

   width = attr.width;
   height = attr.height;
   frameInternal = attr.frameInternal;
   type = attr.type;
   format = attr.format;
   VHLogDebug("%s add deviceName:%s format:%d", __FUNCTION__, deviceInfo.m_sDeviceDisPlayName, format);
   return true;
}

int VideoDeviceAttributeManager::GetDeviceDisplayNumber(DeviceInfo device) {
   unsigned int index = 0;
   for (auto itor = m_deviceList.begin(); itor != m_deviceList.end(); itor++, index++) {
      if (device == *itor) {
         break;
      }
   }
   if (index >= m_deviceList.size()) {
      index = 0;
   }
   return index;
}
//更新设备列表
void VideoDeviceAttributeManager::SyncDeviceList(DeviceList &deviceList) {
   m_deviceList.clear();
   for (auto itor = deviceList.begin(); itor != deviceList.end(); itor++) {
      m_deviceList.push_back(*itor);
   }
   SaveConfig();
}
void VideoDeviceAttributeManager::LoadConfig() {
   if (!m_vhopen ||
       !m_vhclear ||
       !m_vhread ||
       !m_vhwrite ||
       !m_vhclose) {
      return;
   }

   if (!m_vhopen(DSHOW_CONFIG_FILE)) {
      return;
   }

   DeviceInfo deviceInfo;
   UINT width;
   UINT height;
   int frameInternal;
   DeinterlacingType type;
   VideoFormat format;

   while (m_vhread(deviceInfo, width, height, frameInternal, type, format)) {
      DeviceInfoAttribute attr;
      attr.width = width;
      attr.height = height;
      attr.frameInternal = frameInternal;
      attr.type = type;
      attr.format = format;
      if (!(deviceInfo == DeviceInfo()) && width != 0 && height != 0) {
         m_DeviceAttributes[deviceInfo] = attr;
      }
      memset(&deviceInfo, 0, sizeof(DeviceInfo));
   }

   m_vhclose();
}
void VideoDeviceAttributeManager::SaveConfig() {
   if (!m_vhopen ||
       !m_vhclear ||
       !m_vhread ||
       !m_vhwrite ||
       !m_vhclose) {
      return;
   }

   if (!m_vhopen(DSHOW_CONFIG_FILE)) {
      return;
   }

   m_vhclear();

   for (auto itor = m_DeviceAttributes.begin(); itor != m_DeviceAttributes.end(); itor++) {
      DeviceInfo deviceInfo = itor->first;
      DeviceInfoAttribute attr = itor->second;
      m_vhwrite(deviceInfo, attr.width, attr.height, attr.frameInternal, attr.type, attr.format);
   }

   m_vhclose();
}


void DShowCaptureLogCallBack(int, const wchar_t *msg, void *) {
   VHLogDebug(msg);
   VHLogDebug(L"\r");
}




//初始化DShow
void InitDShowPlugin(FUNCFileOpen vhopen,
                     FUNCFileClear vhclear,
                     FUNCFileRead vhread,
                     FUNCFileWrite vhwrite,
                     FUNCFileClose vhclose,
   const wchar_t* logPath) {
   CoInitialize(0);
   DShowLogInit();


   InitXT(NULL, L"FastAlloc", L"DShowPlugin");
   InitDShowCapture();
   wchar_t lwzLogFileName[255] = { 0 };
   if (logPath == NULL) {
      SYSTEMTIME loSystemTime;
      GetLocalTime(&loSystemTime);
      wsprintf(lwzLogFileName, L"%s%s_%4d_%02d_%02d_%02d_%02d%s", VH_LOG_DIR, L"DShowPlugin", loSystemTime.wYear, loSystemTime.wMonth, loSystemTime.wDay, loSystemTime.wHour, loSystemTime.wMinute, L".log");
      gLogger = new Logger(lwzLogFileName, USER);
   }
   else {
      if (!CreateDirectoryW(logPath, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
         OutputDebugStringW(L"Logger::Logger: CreateDirectoryW failed.");
      }
      SYSTEMTIME loSystemTime;
      GetLocalTime(&loSystemTime);
      wsprintf(lwzLogFileName, L"%s%s_%4d_%02d_%02d_%02d_%02d%s", logPath, L"DShowPlugin", loSystemTime.wYear, loSystemTime.wMonth, loSystemTime.wDay, loSystemTime.wHour, loSystemTime.wMinute, L".log");
      gLogger = new Logger(lwzLogFileName, None);
   }


	//gLogger = new Logger(VH_LOG_DIR L"DShowPlugin.log", USER);
   VHLogDebug(L"InitDShowPlugin");
   G_DShowVideoFilterManager = new DShowVideoFilterDeviceManager();
   G_VideoDeviceAttributeManager = new VideoDeviceAttributeManager(vhopen,
                                                                   vhclear,
                                                                   vhread,
                                                                   vhwrite,
                                                                   vhclose);
   SetDShowcaptureLog(DShowCaptureLogCallBack, NULL);
}
void SetDShowcaptureLog(void *func, void *param) {
   DShow::SetLogCallback((DShow::LogCallback)func, param);
}

//析构DShow
void UnInitDShowPlugin() {
	VHLogDebug(L"UnInitDShowPlugin");
   if (G_VideoDeviceAttributeManager) {
      delete G_VideoDeviceAttributeManager;
      G_VideoDeviceAttributeManager = NULL;
   }
   if (G_DShowVideoFilterManager) {
      delete G_DShowVideoFilterManager;
      G_DShowVideoFilterManager = NULL;
   }
   if (gLogger) {
      delete gLogger;
      gLogger = NULL;
   }
   UnInitDShowCapture();

   TerminateXT();

   DShowLogUnInit();
   CoUninitialize();

}

//获得DSHOW VIDEO设备
bool GetDShowVideoFilter(
   IDShowVideoFilterDevice *&device,
   DeviceInfo deviceInfo,
   VideoDeviceSetting &setting,
   DShowDeviceType deviceType,
   DShowDevicePinType pinType,
   HANDLE handle,
   HANDLE pFunc) {
   VHLogDebug(L"GetDShowVideoFilter");
   if (!G_DShowVideoFilterManager) {
      VHLogDebug(L"GetDShowVideoFilter G_DShowVideoFilterManager null");
      return false;
   }



   //从已有的filter中获得设备，如果没有，新创建
   DShowVideoFilterDevice *filter = G_DShowVideoFilterManager->GetDeviceFilter(
      device,
      deviceInfo,
      setting,
      deviceType,
      pinType,
      handle,
      pFunc);

   if (!filter) {

      VHLogDebug(L"GetDShowVideoFilter filter NULL");
      return false;
   }

   device = filter;

   VHLogDebug(L"GetDShowVideoFilter filter not NULL,return ,success!");
   return true;
}

//释放DSHOW VIDEO设备
bool ReleaseDShowVideoFilter(HANDLE handle) {
   if (!G_DShowVideoFilterManager) {
      return false;
   }
   return G_DShowVideoFilterManager->ReleaseDShowVideoFilter(handle);
}

bool IsDShowVideoFilterExist(HANDLE handle) {
   if (!G_DShowVideoFilterManager) {
      return false;
   }
   return G_DShowVideoFilterManager->IsDShowVideoFilterExist(handle);
}

void DShowVideoManagerEnterMutex() {
   if (!G_DShowVideoFilterManager) {
      return;
   }
   return G_DShowVideoFilterManager->EnterMutex();
}

void DShowVideoManagerLeaveMutex() {
   if (!G_DShowVideoFilterManager) {
      return;
   }
   return G_DShowVideoFilterManager->LeaveMutex();
}

//获得视频设备配置信息
bool GetDShowVideoFrameInfoList(DeviceInfo deviceInfo,
                                FrameInfoList *frameInfoList,
                                FrameInfo *pcurrentFrameInfo,
                                DeinterlacingType &deinterType
                                ) {

   VHLogDebug(L"GetDShowVideoFrameInfoList");
   deinterType = DEINTERLACING_NONE;
   FrameInfo currentFrameInfo;
   bool ret = false;
   if (G_DShowVideoFilterManager) {
      VHLogDebug(L"GetDShowVideoFrameInfoList G_DShowVideoFilterManager GetDShowVideoFrameInfoList");
      if (G_DShowVideoFilterManager->GetDShowVideoFrameInfoList(deviceInfo, frameInfoList, &currentFrameInfo)) {
         VHLogDebug(L"GetDShowVideoFrameInfoList G_DShowVideoFilterManager GetDShowVideoFrameInfoList Success");
         ret = true;
      } else {
         VHLogDebug(L"GetDShowVideoFrameInfoList G_DShowVideoFilterManager GetDShowVideoFrameInfoList Failed!");
      }
   }
   else {
      VHLogDebug(L"GetDShowVideoFrameInfoList G_DShowVideoFilterManager null");
   }

   if (!ret) {
      VHLogDebug(L"GetDShowVideoFrameInfoList DShow::Device::GetDeviceFrameInfoList");
	  //枚举设备之后设定
      ret = DShow::Device::GetDeviceFrameInfoList(deviceInfo, frameInfoList, &currentFrameInfo);
      VHLogDebug(L"GetDShowVideoFrameInfoList DShow::Device::GetDeviceFrameInfoList end");
   }

   if (ret) {
      VHLogDebug(L"GetDShowVideoFrameInfoList GetDeviceDefaultAttribute");
      UINT width = 0;
      UINT height = 0;
      int frameInternal = 0;
      DeinterlacingType type = DEINTERLACING_NONE;
      VideoFormat format;
      if (GetDeviceDefaultAttribute(deviceInfo, width, height, frameInternal, type, format)) {
         VHLogDebug(L"GetDShowVideoFrameInfoList GetDeviceDefaultAttribute Successed!");
         if (pcurrentFrameInfo) {
            pcurrentFrameInfo->minFrameInterval = pcurrentFrameInfo->maxFrameInterval = frameInternal;
            pcurrentFrameInfo->minCX = pcurrentFrameInfo->maxCX = width;
            pcurrentFrameInfo->minCY = pcurrentFrameInfo->maxCY = height;
            pcurrentFrameInfo->format = format;
            deinterType = type;
         }
      } else {
         VHLogDebug(L"GetDShowVideoFrameInfoList GetDeviceDefaultAttribute Failed!!");
         SetDeviceDefaultAttribute(deviceInfo, currentFrameInfo.maxCX, currentFrameInfo.maxCY, currentFrameInfo.maxFrameInterval, DEINTERLACING_NONE, currentFrameInfo.format);
         *pcurrentFrameInfo = currentFrameInfo;
      }
   }
   VHLogDebug(L"GetDShowVideoFrameInfoList return!!");
   return ret;
}
//设置DSHOW设备的属性
bool SetDeviceAttribute(DeviceInfo deviceInfo, UINT width, UINT height, int frameInternal, VideoFormat format) {

   if (G_DShowVideoFilterManager) {
      if (G_DShowVideoFilterManager->SetDeviceDefaultAttribute
          (deviceInfo, width, height, frameInternal, format)) {
         return true;
      }
   }
   //不生效的时候设置的分辨率以及帧率生效后将在激活后可读最新配置
   return DShow::Device::SetDeviceDefaultAttribute(deviceInfo, width, height, frameInternal, format);
}

bool SetDeviceDefaultAttribute(DeviceInfo deviceInfo, UINT width, UINT height, int frameInternal, DeinterlacingType type, VideoFormat format) {
   if (!G_VideoDeviceAttributeManager) {
      return false;
   }
   return G_VideoDeviceAttributeManager->SetDeviceDefaultAttribute(deviceInfo, width, height, frameInternal, type, format);
}

bool GetDeviceDefaultAttribute(DeviceInfo device,
                               UINT &width, UINT &height, int &frameInternal, DeinterlacingType &type, VideoFormat &format) {
   if (!G_VideoDeviceAttributeManager) {
      VHLogDebug(L"GetDeviceDefaultAttribute Failed!1");
      return false;
   }

   if (G_VideoDeviceAttributeManager->GetDeviceDefaultAttribute(device,width,height,frameInternal,type,format)) {
      VHLogDebug(L"GetDeviceDefaultAttribute true!");
      return true;
   }
   VHLogDebug(L"GetDeviceDefaultAttribute Failed!2");
   return false;
}

//获取设备号
int GetDeviceDisplayNumber(DeviceInfo device) {
   if (!G_VideoDeviceAttributeManager) {
      return -1;
   }
   return G_VideoDeviceAttributeManager->GetDeviceDisplayNumber(device);
}

//更新设备列表
void SyncDeviceList(DeviceList &deviceList) {
   if (!G_VideoDeviceAttributeManager) {
      return;
   }

   G_VideoDeviceAttributeManager->SyncDeviceList(deviceList);
}
