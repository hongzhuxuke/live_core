#include "vhall_log.h"
#include "json.h"
#include "MediaCore.h"
#include "Logging.h"
#include "json.h"
#include "pub.Const.h"
#include "ffmpeg_demuxer.h"
#include "ImageEnhance.h"
#include <windows.h>
#include <math.h>
#include <Iphlpapi.h>
Logger *g_pLogger = NULL;



typedef struct MediaCoreJsonData_st {
   struct {
      struct {
         std::string Name;
         int bit_rate;
         int channel_layout;
         int channels;
         int codec_id;
         std::string dst_sample_fmt;
         int frame_faild_count;
         int frame_success_count;
         int profile;
         int sample_rate;
         std::string src_sample_fmt;
         int strict_std_compliance;
      }
      AudioEncoder;

      std::string Name;

      struct {
         std::string Name;
         int bitrate;
         int frame_faild_count;
         int frame_rate;
         int frame_success_count;
         int gop_size;
         int height;
         std::string orientation;
         std::string preset;
         std::string profile;
         int width;

      }VideoEncoder;

      int video_frame_count;

   }MediaEncoder;

   typedef struct type_muxer_st {
      std::string Name;
      std::string dest;
      int drop_frames_count;
      std::string drop_type;
      int id;
      int send_buffer_size;
      int send_bytes;
      int speed;
      int start_duration;
      int video_frame_count;
      int audio_frame_success_count;
      std::string status;
      std::string tag;
      int send_frames_count;
   } type_muxer;

   struct {
      std::list<type_muxer>Muxers;
      std::string Name;
      int audio_queue_size;
      int data_pool_free_size;
      int data_pool_size;
      int muxers_size;
      int video_queue_size;

   }MediaMuxer;

   std::string Name;
   struct {
      std::string Name;
      int audio_timestamp;
      int m_audio_data_size;
      int m_audio_time_pres;
      int video_duration;
      int video_timestamp;
   }TSSync;

   void Deserialization(std::string &txt) {
      VHJson::Reader jsonReader;
      VHJson::Value mediaCoreJsonData;
      if (jsonReader.parse(txt, mediaCoreJsonData)) {
         type_muxer muxer;
         VHJson::Value MediaEncoder = mediaCoreJsonData["MediaEncoder"];
         if (MediaEncoder.isObject()) {
            muxer.video_frame_count = MediaEncoder["video_frame_count"].asInt();
         }

         VHJson::Value AudioEncoder = MediaEncoder["AudioEncoder"];
         if (AudioEncoder.isObject()) {
            muxer.audio_frame_success_count = AudioEncoder["frame_success_count"].asInt();
         }

         VHJson::Value MediaMuxerJson = mediaCoreJsonData["MediaMuxer"];
         VHJson::Value Muxers = MediaMuxerJson["Muxers"];
         if (Muxers.isArray()) {
            for (auto itor = Muxers.begin(); itor != Muxers.end(); itor++) {
               VHJson::Value v = *itor;
               muxer.Name = v["Name"].asString();
               if (muxer.Name != "SrsFlvRecorder") {
                  muxer.dest = v["dest"].asString();
                  muxer.drop_frames_count = v["drop_frames_count"].asInt();
                  muxer.drop_type = v["drop_type"].asString();
                  muxer.id = v["id"].asInt();
                  muxer.send_buffer_size = v["send_buffer_size"].asInt();
                  muxer.send_frames_count = v["send_frames_count"].asInt();
                  muxer.send_bytes = v["send_bytes"].asInt();
                  muxer.speed = v["speed"].asInt();
                  muxer.start_duration = v["start_duration"].asInt();
                  muxer.status = v["status"].asString();
                  muxer.tag = v["tag"].asString();
                  MediaMuxer.Muxers.push_back(muxer);
               }
            }
         }
      }
   }
}MediaCoreJsonData;

HANDLE OSCreateMutex()
{
   CRITICAL_SECTION *pSection = (CRITICAL_SECTION*)malloc(sizeof(CRITICAL_SECTION));
   InitializeCriticalSection(pSection);

   return (HANDLE)pSection;
}

void  OSEnterMutex(HANDLE hMutex)
{
   //assert(hMutex);
   EnterCriticalSection((CRITICAL_SECTION*)hMutex);
}

BOOL OSTryEnterMutex(HANDLE hMutex)
{
   //assert(hMutex);
   return TryEnterCriticalSection((CRITICAL_SECTION*)hMutex);
}

void OSLeaveMutex(HANDLE hMutex)
{
   //assert(hMutex);
   LeaveCriticalSection((CRITICAL_SECTION*)hMutex);
}

void OSCloseMutex(HANDLE hMutex)
{
   //assert(hMutex);
   DeleteCriticalSection((CRITICAL_SECTION*)hMutex);
   free(hMutex);
}
void GetNetworkAddress(char *mac) {
   DWORD stSize = 0;
   GetAdaptersInfo(NULL, &stSize);
   if (stSize <= 0) {
      return;
   }

   PIP_ADAPTER_INFO info = (PIP_ADAPTER_INFO)new unsigned char[stSize];
   GetAdaptersInfo(info, &stSize);

   PIP_ADAPTER_INFO infoPtr = info;
   bool find = false;
   while (infoPtr && !find) {

      IP_ADDR_STRING *pIpAddrString = &(infoPtr->IpAddressList);
      do
      {
         if (strcmp(pIpAddrString->IpAddress.String, "0.0.0.0") != 0 && strcmp(pIpAddrString->IpAddress.String, "127.0.0.1") != 0) {

            sprintf_s(mac, 18, "%02X-%02X-%02X-%02X-%02X-%02X",
               infoPtr->Address[0],
               infoPtr->Address[1],
               infoPtr->Address[2],
               infoPtr->Address[3],
               infoPtr->Address[4],
               infoPtr->Address[5]);
            find = true;
            break;
         }
         pIpAddrString = pIpAddrString->Next;
      } while (pIpAddrString);

      infoPtr = infoPtr->Next;
   }

   delete info;
}

MediaCore::MediaCore() :
   mbIsStartPushStream(false)
{
   g_pLogger->logInfo("MediaCore::MediaCore()\n");
   mLivePusher = new VHallLivePush();
   mLivePusher->SetListener(this);
   //SetConsoleDebugLog("D://media_core_log.log");
   mLiveParam.publish_timeout = 10000; //发起超时
   mLiveParam.publish_reconnect_times = 0; //发起重练次数
   //mLiveParam.watch_timeout = 0;   //观看超时
   //mLiveParam.watch_reconnect_times = INT_MAX; // 观看重练次数
   //mLiveParam.buffer_time = 5;    //s
   //mLiveParam.orientation = 0;    //0 代表横屏，1 代表竖屏
   //mLiveParam.g_hw_enable =         ENCODE_TYPE_SOFTWARE;    //0 代表软编，1 代表硬编码
   mLiveParam.encode_pix_fmt = ENCODE_PIX_FMT_YUV420SP_NV12; //ENCODE_PIX_FMT_NV21 软编码时输入的数据格式  0代表NV21 1代表YUV420sp     ENCODE_PIX_FMT_YUV420SP
   mLiveParam.live_model = LIVE_TYPE_VIDEO_AND_AUDIO; //直播推流的模式 0代表只是推流，1代表推流和数据回调，2代表只是数据回调，3是只推音频
   //mLiveParam.video_decoder_mode =  1;   //1.soft;2, hard; 3, auto(soft->hard)
   mLiveParam.src_sample_fmt = VH_AV_SAMPLE_FMT_FLT;
   mLiveParam.encode_sample_fmt = VH_AV_SAMPLE_FMT_FLTP;
   mLiveParam.drop_frame_type = DROP_GOPS;
   mLiveParam.is_http_proxy = false;
   mLiveParam.platform = 6; //平台类型 0代表iOS 1代表Android
   //mLiveParam.audio_bitrate = 48000;
   mLiveParam.device_type = "";
   mLiveParam.device_identifier = "";
   mLiveParam.is_assess_video_quality = false;
   mLiveParam.is_adjust_bitrate = true;
   mVoiceTransition = new VoiceTransition;
   if (mVoiceTransition) {
      mVoiceTransition->SetDelegate((VoiceTransition::VoiceTransitionDelegate*)this);
   }
   mbEnableVoiceTransition = false;
   char mac[18] = { 0 };
   GetNetworkAddress(mac);
   mLiveParam.device_identifier = mac;
   g_pLogger->logInfo("MediaCore::MediaCore() GetNetworkAddress [%s]\n", mac);

   //mLiveParam.extra_metadata.insert(std::make_pair("upversion", "zhushou2.0"));

   mVideoPush_mutex = OSCreateMutex();
   mVideoPush_event = CreateEvent(NULL, FALSE, FALSE, NULL);

   mRtmpUrlsMutex = OSCreateMutex();
   //开启vhallMediaCore日志输出。
   VHALL_LOG_ENABLE(true);
   //PCM = fopen("E:\\a.pcm", "wb");
   //mPushStreamMutex = OSCreateMutex();

   //mMixYUV = fopen("E:\\1\\aa.yuv", "wb");
}

void MediaCore::SetVersion(const char *v) {
   char version[128] = { 0 };
   sprintf_s(version, 128, "Vhall Mutex %s", v);
   mLiveParam.extra_metadata.insert(std::make_pair("encoder", version));
   g_pLogger->logInfo("MediaCore::SetVersion [%s]\n", version);
}

MediaCore::~MediaCore() {
   if (mVoiceTransition) {
      delete mVoiceTransition;
      mVoiceTransition = NULL;
   }

   g_pLogger->logInfo("MediaCore::~MediaCore");
   if (mVideoPush_event) {
      CloseHandle(mVideoPush_event);
      mVideoPush_event = NULL;
   }

   if (mVideoPush_mutex) {
      OSCloseMutex(mVideoPush_mutex);
      mVideoPush_mutex = NULL;
   }

   if (mLivePusher != NULL) {
      delete mLivePusher;
      mLivePusher = NULL;
   }

   if (yuvData != NULL) {
      delete[]yuvData;
      yuvData = NULL;
   }

   if (g_pLogger) {
      delete g_pLogger;
      g_pLogger = NULL;
   }
   //if (mPushStreamMutex) {
   //   CloseHandle(mPushStreamMutex);
   //   mPushStreamMutex = NULL;
   //}
   if (mRtmpUrlsMutex) {
      OSCloseMutex(mRtmpUrlsMutex);
      mRtmpUrlsMutex = nullptr;
   }
}

int SetMediaCoreModuleLog(std::string path, int level) {
   int logId = 0;
   if (path == "") {
      ConsoleInitParam param;
      memset(&param, 0, sizeof(ConsoleInitParam));
      param.nType = 0;
      logId = ADD_NEW_LOG(VHALL_LOG_TYPE_CONSOLE, &param, level);
   }
   else {
      FileInitParam param;
      memset(&param, 0, sizeof(FileInitParam));
      param.pFilePathName = path.c_str();
      logId = ADD_NEW_LOG(VHALL_LOG_TYPE_FILE, &param, level);
   }
   return logId;
}

void MediaCore::SetLogLevel(char *path, int level) {
   switch (level) {
   case VHALL_LOG_LEVEL_DEBUG:
   case VHALL_LOG_LEVEL_INFO:
   case VHALL_LOG_LEVEL_WARN:
   case VHALL_LOG_LEVEL_ERROR:
      mLogID = SetMediaCoreModuleLog(path, level);
      break;
   default:
      mLogID = SetMediaCoreModuleLog("", VHALL_LOG_LEVEL_NO_LOG);
      break;
   }
}

void MediaCore::SetUploadLogParam(int bu, std::string vid) {
   if (mLivePusher) {
      VHJson::Value param;
      param["bu"] = VHJson::Value(bu);
      param["pf"] = VHJson::Value(8);
      param["vid"] = VHJson::Value(vid);

      VHJson::FastWriter root;
      std::string json_str = root.write(param);
      mLivePusher->SetMonitorLogParam(json_str.c_str());
   }
}

int MediaCore::OnEvent(int type, const std::string content) {
   if (!mMediaCoreEvent) {
      return 0;
   }
   char dbg[1024] = { 0 };
   sprintf_s(dbg, 1024, "[MediaCore::OnEvent] type=%d content[%s]\n", type, content.c_str());
   mMediaCoreEvent->OnMediaCoreLog(dbg);
   g_pLogger->logInfo("MediaCore::OnEvent [%d][%s]", type, dbg);

   switch (type) {
   case OK_PUBLISH_CONNECT:
      mMediaCoreEvent->OnMediaReportLog("OK_PUBLISH_CONNECT");
      mMediaCoreEvent->OnNetworkEvent(Network_to_push_connect);
      break;
   case OK_WATCH_CONNECT:
   case START_BUFFERING:
   case STOP_BUFFERING:
   case VIDEO_ENCODE_OK: {
      mMediaCoreEvent->OnMediaReportLog("VIDEO_ENCODE_OK");
      break;
   }
   case RC_BITRATE_FIRST_DOWN: {
      //码率自适应
      mMediaCoreEvent->OnNetworkEvent(Network_to_push_birate_down);
      break;
   }
   case INFO_SPEED_UPLOAD:
   case INFO_SPEED_DOWNLOAD:
   case INFO_NETWORK_STATUS:
   case INFO_DECODED_VIDEO:
   case INFO_DECODED_AUDIO:
   case UPLOAD_NETWORK_OK:
   case RECV_STREAM_TYPE:
      break;
   case ERROR_PUBLISH_CONNECT: {
      string errInfo = string("ERROR_PUBLISH_CONNECT_") + content;
      mMediaCoreEvent->OnMediaReportLog(errInfo.c_str());
      mMediaCoreEvent->OnNetworkEvent(Network_to_connect);
      break;
   }
   case ERROR_WATCH_CONNECT: {
      mMediaCoreEvent->OnMediaReportLog("ERROR_WATCH_CONNECT");
      mMediaCoreEvent->OnNetworkEvent(Network_to_connect);
      break;
   }
   case ERROR_PARAM: {
      mMediaCoreEvent->OnMediaReportLog("ERROR_PARAM");
      mMediaCoreEvent->OnNetworkEvent(Network_to_connect);
      break;
   }
   case ERROR_RECV: {
      mMediaCoreEvent->OnMediaReportLog("ERROR_RECV");
      mMediaCoreEvent->OnNetworkEvent(Network_to_connect);
      break;
   }
   case ERROR_SEND: {
      mMediaCoreEvent->OnMediaReportLog("ERROR_SEND");
      mMediaCoreEvent->OnNetworkEvent(Network_to_connect);
      break;
   }
                    //重连
   case RECONNECTING: {
      mMediaCoreEvent->OnMediaReportLog("RECONNECTING");
      mMediaCoreEvent->OnNetworkEvent(Network_to_connect);
      break;
   }
   case UPLOAD_NETWORK_EXCEPTION:
      mMediaCoreEvent->OnMediaReportLog("UPLOAD_NETWORK_EXCEPTION");
      break;
   case VIDEO_QUEUE_FULL:
      mMediaCoreEvent->OnMediaReportLog("VIDEO_QUEUE_FULL");
      break;
   case AUDIO_QUEUE_FULL:
      mMediaCoreEvent->OnMediaReportLog("AUDIO_QUEUE_FULL");
      break;
   case VIDEO_ENCODE_BUSY:
      mMediaCoreEvent->OnMediaReportLog("VIDEO_ENCODE_BUSY");
      break;
   case VIDEO_HWDECODER_INIT:
   case VIDEO_HWDECODER_DESTORY:
   default:
      break;
   }
   return 0;
}

void MediaCore::SetSceneInfo(VideoSceneType sceneType) {
   mSceneType = sceneType;

}
void MediaCore::GetSceneInfo(VideoSceneType& sceneType) {
   sceneType = mSceneType;
}

bool MediaCore::InitCapture(IMediaCoreEvent* mediaCoreEvent, IGraphics *graphic) {
   mMediaCoreEvent = mediaCoreEvent;
   mGraphic = graphic;
   return true;
}
void MediaCore::UninitCapture() {
   mMediaCoreEvent = NULL;
   mGraphic = NULL;
}

void FileRename(std::string fileName) {
   if (fileName.size() >= 12) {
      SYSTEMTIME st;
      GetLocalTime(&st);
      char flvFileName[512] = { 0 };
      sprintf_s(flvFileName, 512, "%02u-%02u-%02u.flv", st.wHour, st.wMinute, st.wSecond);

      char newFileName[1024] = { 0 };
      strncpy_s(newFileName, 512, fileName.c_str(), 512);
      strncpy_s(newFileName + strlen(newFileName) - 12, 512 + strlen(newFileName) - 12, flvFileName, 512);
      MoveFileA(fileName.c_str(), newFileName);
   }
}

void MediaCore::StartRecord(const char *fileName) {
   int id = mLivePusher->AddMuxer(FILE_FLV_MUXER, (void *)fileName);
   g_pLogger->logInfo("MediaCore::StartRecord [%s] id = [%d]", fileName, id);
   if (id >= 0) {
      m_fileUris[id] = std::string(fileName);
      mIRecordFileId = id;
      mLivePusher->StartMuxer(id);
   }
}

//暂停录制
void MediaCore::SuspendRecord()
{
   mLivePusher->StopMuxer(mIRecordFileId);
}

//恢复录制
void MediaCore::RecoveryRecord()
{
   mLivePusher->StartMuxer(mIRecordFileId);
}

void MediaCore::StopRecord(/*const char *fileName*/) {
   g_pLogger->logInfo("MediaCore::StopRecord fileName is NULL");
   for (auto itor = m_fileUris.begin(); itor != m_fileUris.end(); itor++) {
      mLivePusher->StopMuxer(itor->first);
      mLivePusher->RemoveMuxer(itor->first);
      FileRename(itor->second);
   }
   m_fileUris.clear();
}

bool MediaCore::StartRtmp(const char *rtmpURI) {
   Sleep(1000);
   g_pLogger->logInfo("MediaCore::StartRtmp [%s]", rtmpURI);
   int id;
   std::string url = rtmpURI;
   if (url.find("http://") != std::string::npos) {
      id = mLivePusher->AddMuxer(HTTP_FLV_MUXER, (void *)rtmpURI);
   }
   else {
      id = mLivePusher->AddMuxer(RTMP_MUXER, (void *)rtmpURI);
   }
   g_pLogger->logInfo("%s AddMuxer end ", __FUNCTION__);
   if (id < 0) {
      g_pLogger->logInfo("MediaCore::StartRtmp id(%d) < 0", id);
      return false;
   }
   OSEnterMutex(mRtmpUrlsMutex);
   m_rtmpUris[id] = std::string(rtmpURI);
   OSLeaveMutex(mRtmpUrlsMutex);

   //std::map<int, std::string> currentUrl = m_fileUris;
   //m_fileUris.clear();
   //for (auto itor = currentUrl.begin(); itor != currentUrl.end(); itor++) {
   //   if (itor->second.length() > 0) {
   //      g_pLogger->logInfo("MediaCore::StartRtmp [%s]", itor->second.c_str());
   //      StartRecord(itor->second.c_str());
   //   }
   //}

   mLivePusher->StartMuxer(id);
   if (!mbIsStartPushStream) {
      firstAudioTimeStamp = 0;
      mLastAudioTimeStamp = 0;
      mVideoIndex = 0;
      mAudioFrameCount = 0;
      mAudioSendCount = 0;
      StartPushVideo();
   }
   mbIsStartPushStream = true;
   g_pLogger->logInfo("%s end ", __FUNCTION__);
   return true;
}

bool MediaCore::StopRtmp(const char *rtmpURI) {
   g_pLogger->logInfo("%s ", __FUNCTION__);
   if (!mbIsStartPushStream) {
      return true;
   }
   mbIsStartPushStream = false;
   if (rtmpURI == NULL) {
      g_pLogger->logInfo("MediaCore::StopRtmp rtmpURI is NULL");
      OSEnterMutex(mRtmpUrlsMutex);
      g_pLogger->logInfo("MediaCore::StopRtmp to stop");
      //if (mLivePusher) {
      //   mLivePusher->StopAllMuxer();
      //   g_pLogger->logInfo("MediaCore::StopRtmp StopAllMuxer end");
      //   mLivePusher->RemoveAllMuxer();
      //   g_pLogger->logInfo("MediaCore::StopRtmp RemoveAllMuxer end");
      //}
      for (auto itor = m_rtmpUris.begin(); itor != m_rtmpUris.end(); itor++) {
         mLivePusher->StopMuxer(itor->first);
         g_pLogger->logInfo("MediaCore::StopRtmp StopMuxer strRtmp:%s", itor->second.c_str());
         mLivePusher->RemoveMuxer(itor->first);
         g_pLogger->logInfo("MediaCore::StopRtmp RemoveMuxer strRtmp:%s", itor->second.c_str());
      }
      m_rtmpUris.clear();
      OSLeaveMutex(mRtmpUrlsMutex);
      g_pLogger->logInfo("%s RemoveMuxer end ", __FUNCTION__);
   }
   else {
      g_pLogger->logInfo("MediaCore::StopRtmp rtmpURI is [%s]", rtmpURI);
      std::string strRtmp = rtmpURI;
      OSEnterMutex(mRtmpUrlsMutex);
      int nCount = m_rtmpUris.size();
      g_pLogger->logInfo("MediaCore::StopRtmp nCount is %d", nCount);
      if (nCount > 0) {
         for (auto itor = m_rtmpUris.begin(); itor != m_rtmpUris.end(); itor++) {
            if (itor->second == strRtmp) {
               mLivePusher->StopMuxer(itor->first);
               g_pLogger->logInfo("MediaCore::StopRtmp StopMuxer strRtmp:%s", strRtmp.c_str());
               mLivePusher->RemoveMuxer(itor->first);
               g_pLogger->logInfo("MediaCore::StopRtmp RemoveMuxer strRtmp:%s", strRtmp.c_str());
               m_rtmpUris.erase(itor++);
            }
            else {
               itor++;
            }
         }
      }
      OSLeaveMutex(mRtmpUrlsMutex);
      g_pLogger->logInfo("%s rtmpURI end ", __FUNCTION__);
   }
   bool isEmpty = false;
   OSEnterMutex(mRtmpUrlsMutex);
   isEmpty = m_rtmpUris.size() == 0 ? true : false;
   OSLeaveMutex(mRtmpUrlsMutex);
   g_pLogger->logInfo("%s StopPushVideo", __FUNCTION__);
   if (isEmpty) {
      StopPushVideo();
      g_pLogger->logInfo("%s StopPushVideo end", __FUNCTION__);
   }
   mAudioFrameCount = 0;
   g_pLogger->logInfo("%s end", __FUNCTION__);
   return true;
}


int  MediaCore::RtmpCount() {
   OSEnterMutex(mRtmpUrlsMutex);
   int size = m_rtmpUris.size();
   OSLeaveMutex(mRtmpUrlsMutex);
   return  size;
}

bool MediaCore::GetStreamStatus(int index, StreamStatus *status) {
   if (!status) {
      return false;
   }

   int id = -1;
   int i = 0;
   OSEnterMutex(mRtmpUrlsMutex);
   if (m_rtmpUris.size() > 0) {
      for (auto itor = m_rtmpUris.begin(); itor != m_rtmpUris.end(); itor++, i++) {
         if (index == i) {
            id = itor->first;
            break;
         }
      }
   }
   OSLeaveMutex(mRtmpUrlsMutex);

   if (id == -1) {
      return false;
   }

   //std::string txt = "{\"MediaEncoder\":{\"AudioEncoder\":{\"Name\":\"AACEncoder\",\"bit_rate\":96000,\"channel_layout\":3,\"channels\":2,\"codec_id\":86018,\"dst_sample_fmt\":\"AV_SAMPLE_FMT_FLTP\",\"frame_faild_count\":0,\"frame_success_count\":312,\"profile\":1,\"sample_rate\":44100,\"src_sample_fmt\":\"AV_SAMPLE_FMT_FLT\",\"strict_std_compliance\":-2},\"Name\":\"MediaEncoder\",\"VideoEncoder\":{\"Name\":\"X264Encoder\",\"bitrate\":1250,\"frame_faild_count\":0,\"frame_rate\":25,\"frame_success_count\":180,\"gop_size\":2,\"height\":720,\"preset\":\"superfast\",\"profile\":\"high\",\"width\":1280},\"video_frame_count\":180},\"MediaMuxer\":{\"Muxers\":[{\"Name\":\"SrsRtmpPublisher\",\"dest\":\"2001:8004 : 11d0 : 4e2a::2f59 : ddb\",\"drop_frames_count\":0,\"drop_type\":\"DROP_GOPS\",\"id\":0,\"send_buffer_size\":0,\"send_bytes\":135536,\"send_frames_count\":490,\"speed\":150,\"start_duration\":10007,\"status\":\"started\",\"tag\":\"a84c91a3aa1ee2cc41370ee6c303718d\"}],\"Name\":\"MediaMuxer\",\"amf0_queue_size\":0,\"audio_queue_size\":0,\"data_pool_free_size\":26,\"data_pool_size\":30,\"muxers_size\":1,\"video_queue_size\":1},\"Name\":\"LivePush\",\"TSSync\":{\"Name\":\"TimestampSync\",\"audio_timestamp\":0,\"m_audio_data_size\":0,\"m_audio_time_pres\":2822400,\"video_duration\":40,\"video_timestamp\":0}}";// mLivePusher->LiveGetRealTimeStatus();
   std::string txt = mLivePusher->LiveGetRealTimeStatus();
   MediaCoreJsonData JsonData;
   JsonData.Deserialization(txt);
   // g_pLogger->logInfo("MediaCore::GetStreamStatus txt:%s", txt.c_str());

   for (auto itor = JsonData.MediaMuxer.Muxers.begin(); itor != JsonData.MediaMuxer.Muxers.end(); itor++) {
      if (id == itor->id) {
         //strncpy_s(status->serverIP,itor->dest.c_str(),20);
         //status->streamID;
         status->currentDroppedFrames = status->droppedFrames;
         status->bytesSpeed += itor->speed;
         status->currentDroppedFrames = status->droppedFrames - status->currentDroppedFrames;
         status->droppedFrames += itor->drop_frames_count;
         status->sumFrames += itor->send_frames_count;
      }
   }
   return true;
}
int MediaCore::GetSumSpeed() {

   return mSumSpeed;
}

bool MediaCore::ResetPublishInfo(const char *currentUrl, const char *nextUrl) {
   g_pLogger->logInfo("MediaCore::ResetPublishInfo currentUrl[%s] nextUrl[%s]", currentUrl, nextUrl);

   std::string current = currentUrl;
   std::string next = nextUrl;
   OSEnterMutex(mRtmpUrlsMutex);
   if (m_rtmpUris.size() > 0) {
      for (auto itor = m_rtmpUris.begin(); itor != m_rtmpUris.end();) {
         if (itor->second == current) {
            mLivePusher->StopMuxer(itor->first);
            //mLivePusher->RemoveMuxer(itor->first);
            int id = mLivePusher->AddMuxer(RTMP_MUXER, (void *)nextUrl);
            if (id >= 0) {
               m_rtmpUris[id] = next;
               m_rtmpUris.erase(itor++);
               continue;
            }
         }
         itor++;
      }
   }
   OSLeaveMutex(mRtmpUrlsMutex);
   return true;
}

UINT64 MediaCore::GetSendVideoFrameCount(int index) {
   return mSumVideoFrameCount;
}

IDataReceiver *MediaCore::GetDataReceiver() {
   return this;
}
void MediaCore::PushAudioSegment(float *buffer, unsigned int numFrames, unsigned long long timestamp) {
   long size = numFrames * mLiveParam.ch_num * 4;
   if (!mbIsStartPushStream) {
      if (IsEnableVoiceTransition()) {
         mVoiceTransition->InputAudioData((const int8_t*)buffer, size, timestamp);
      }
      return;
   }

   //if (PCM){
   //   fwrite(buffer, size, 1, PCM);
   //}

   if (IsEnableVoiceTransition() && mVoiceTransition) {
      mVoiceTransition->InputAudioData((const int8_t*)buffer, size, timestamp);
      mAudioFrameCount++;
   }
   else {
      mLivePusher->LivePushAudio((const char *)buffer, size, timestamp);
      mAudioFrameCount++;

   }
   if (mAudioFrameCount == 1000 / mFps / 10) {
      ::SetEvent(mVideoPush_event);
      mAudioFrameCount = 0;
   }
   if (mLastAudioTimeStamp == 0) {
      firstAudioTimeStamp = timestamp;
   }
   mLastAudioTimeStamp = timestamp;
}
void MediaCore::CreateImageHance(int width, int height) {
   VideoFilterParam param;
   param.width = width;
   param.height = height;
   if (g_ImageEnhance != nullptr) {
      delete g_ImageEnhance;
      g_ImageEnhance = nullptr;
   }
   g_ImageEnhance = new ImageEnhance;
   g_ImageEnhance->Init(&param);
}

void MediaCore::PushVideoSegment(unsigned char *buffer, unsigned int size, unsigned long long timestamp, bool bSame) {
   if (!mbIsStartPushStream) {
      return;
   }

   if (!yuvData) {
      return;
   }
   if (g_ImageEnhance) {
      if (mLiveParam.width != g_ImageEnhance->GetParam().width || mLiveParam.height != g_ImageEnhance->GetParam().height) {
         CreateImageHance(mLiveParam.width, mLiveParam.height);
      }
   }
   else {
      CreateImageHance(mLiveParam.width, mLiveParam.height);
   }

   if (g_ImageEnhance && mbIsDesktopImageHance && mbIsDesktopShare) {
      g_ImageEnhance->EdgeEnhance((uint32_t*)(buffer), (uint32_t*)(buffer), PixelFmt_I420);
   }
   static int audioSegmentIndex = 0;
   LiveExtendParam exParam;
   exParam.same_last = bSame ? 1 : 0;
   exParam.scene_type = mSceneType;
   if (IsEnableVoiceTransition() && mVoiceTransition) {
      mVoiceTransition->InputVideoData((const int8_t *)buffer, mLiveParam.width * mLiveParam.height * 3 / 2, timestamp, &exParam);
   }
   else {
      //int n;
      //if (mMixYUV) {
      //   n = fwrite((unsigned char *)buffer, sizeof(unsigned char), mLiveParam.width*mLiveParam.height * 3 / 2, mMixYUV);
      //}
      int pushSize = mLiveParam.width * mLiveParam.height * 3 / 2;
      if (mLivePusher && pushSize > 0) {
         mLivePusher->LivePushVideo((const char *)buffer, pushSize, timestamp, &exParam);
      }
   }
}

void MediaCore::SetAudioParam(int channels, int samplesPerSecond, int samplesBits) {
   //mLiveParam.sample_rate = samplesPerSecond; //采样率
   mLiveParam.ch_num = channels;       //声道
   mLiveParam.audio_bitrate = samplesBits * 1000; //音频编码码率
   if (mLivePusher) {
      mLivePusher->LiveSetParam(&mLiveParam);
   }
}

void MediaCore::SetAudioParamReduction(const int& samplesPerSecond, const int& samplesBits, const bool& bNoiseReduction)
{
   if (NULL != mLivePusher)
   {
      //mLiveParam.sample_rate = samplesPerSecond; //采样率
      mLiveParam.audio_bitrate = samplesBits * 1000; //音频编码码率
      mLivePusher->LiveSetParam(&mLiveParam);
   }
}

bool MediaCore::IsSamplePerSecondChange(const int& samplesPerSecond)
{
   return mLiveParam.sample_rate != samplesPerSecond;
}

bool MediaCore::IsSamplesBitsChanged(const int& samplesBits)
{
   return mLiveParam.audio_bitrate != (samplesBits * 1000);
}

int MediaCore::GetSamplesBits() {
   return mLiveParam.audio_bitrate;
}

void MediaCore::SetProxy(bool isProxy, std::string host, int port, std::string username, std::string password) {
   ProxyDetail pd;
   if (isProxy) {
      pd.host = host;
      pd.port = port;
      pd.username = username;
      pd.password = password;
   }
   mLiveParam.proxy = pd;

   mLiveParam.is_http_proxy = isProxy;
   mLivePusher->LiveSetParam(&mLiveParam);
}

void MediaCore::SetVideoParamEx(int video_process_filters,
   bool is_adjust_bitrate,
   bool is_quality_limited,
   bool is_encoder_debug,
   bool is_saving_data_debug,
   int high_codec_open) {

   /*
      //赋值为0表示关闭所有预处理功能，默认打开场景检测和宏块COPY分析功能，考虑到性能问题非高噪声环境应关闭去噪处理
      mLiveParam.video_process_filters = VIDEO_PROCESS_SCENETYPE | VIDEO_PROCESS_DIFFCHECK;
      //不开启推流过程中动态改变期望编码码率功能，可依据业务实际情况进行调整
      mLiveParam.is_adjust_bitrate = false;
      //默认开启编码质量限制，可依据业务实际情况进行调整
      mLiveParam.is_quality_limited = true;
      //是否保存X264日志以及编码后h264码流，依据是否需要测试相关功能决定是否开启
      mLiveParam.is_encoder_debug = true;
      mLiveParam.is_saving_data_debug = true;
   */
   mLiveParam.video_process_filters = video_process_filters;
   //mLiveParam.is_adjust_bitrate = is_adjust_bitrate;    
   mLiveParam.is_quality_limited = is_quality_limited;
   mLiveParam.is_encoder_debug = is_encoder_debug;
   mLiveParam.is_saving_data_debug = is_saving_data_debug;
   mLiveParam.high_codec_open = high_codec_open;
}

void MediaCore::SetVideoParam(float gop, int fps, int bits, int w, int h) {
   mLiveParam.width = w; //视频宽度
   mLiveParam.height = h; //视频高度
   mLiveParam.frame_rate = fps; //视频采样率
   mLiveParam.bit_rate = bits * 1000; // 编码码率

   mLiveParam.gop_interval = gop;
   mLiveParam.is_encoder_debug = false;
   //mLiveParam.crf = 23.0f;    // 码率相关  
   mLivePusher->LiveSetParam(&mLiveParam);

   if (yuvData != NULL) {
      delete[]yuvData;
      yuvData = NULL;
   }

   yuvData = new unsigned char[w*h * 3 / 2];
   memset(yuvData, 0, w*h * 3 / 2);
}

void MediaCore::StartPushVideo() {
   g_pLogger->logInfo("MediaCore::StartPushVideo");
   mVideoPush_stop = false;
   mVideoPush_thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)VideoPushThread, this, 0, NULL);
   if (mVideoPush_thread != NULL) {

      g_pLogger->logInfo("MediaCore::StartPushVideo CreateThread Successed");
   }
   else {
      g_pLogger->logInfo("MediaCore::StartPushVideo CreateThread Failed");
   }

   SetEventPushVideo(0);
}

void MediaCore::StopPushVideo() {
   g_pLogger->logInfo("MediaCore::StopPushVideo");
   if (mVideoPush_thread) {
      mVideoPush_stop = true;
      SetEventPushVideo(0);
      WaitForSingleObject(mVideoPush_thread, INFINITE);
      mVideoPush_thread = NULL;
   }
   g_pLogger->logInfo("MediaCore::StopPushVideo end");

}

DWORD MediaCore::VideoPushThread(MediaCore *_this) {
   _this->VideoPushThreadLoop();
   return 0;
}

uint64_t GetTimestampMs() {
   auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
   return ms.count();
}

void MediaCore::VideoPushThreadLoop() {

   while (!mVideoPush_stop) {
      DWORD ret = WaitForSingleObject(mVideoPush_event, 10000 / mFps);
      switch (ret)
      {
      case WAIT_OBJECT_0:
      case WAIT_TIMEOUT: {
         VideoPush();
         break;
      }
      default:
         break;
      }
   }
}

void MediaCore::VideoPush() {
   unsigned long long currentTime = 0;
   currentTime = mLastAudioTimeStamp;
   unsigned long long t = 0;
   unsigned char **yuvData = mGraphic->LockCurrentFramePic(t, currentTime);
   if (yuvData) {
      if (*yuvData) {
         if (abs((long long)(t - currentTime)) > 400) {
            g_pLogger->logInfo("VideoPush :  t - currentTime:%llu", abs((long long)(t - currentTime)));
         }
         PushVideoSegment(*yuvData, 0, t, mLastVideoTimeStamp == t);
         mLastVideoTimeStamp = t;
         mVideoIndex++;
      }
   }
   else {
      g_pLogger->logInfo("VideoPush :no data t:%llu", t);
   }
   mGraphic->UnlockCurrentFramePic();
}

void MediaCore::SetEventPushVideo(unsigned long long timestamp) {
   SetEvent(mVideoPush_event);
}

void MediaCore::RateControlSwitch(bool on) {
   if (mLivePusher) {
      mLivePusher->RateControlSwitch(on);
   }
}

void MediaCore::GetVideoParamInfo(wchar_t *info) {

   wsprintf(info,
      L"width=%d&height=%d&frame_rate=%d&bit_rate=%d"
      L"&gop_interval=%d&drop_frame_type=%d&sample_rate=%d"
      L"&ch_num=%d&audio_bitrate=%d&src_sample_fmt=%d&encode_sample_fmt=%d"
      L"&publish_timeout=%d&publish_reconnect_times=%d&encode_type=%d&encode_pix_fmt=%d"
      L"&live_publish_model=%d&video_process_filters=%d&audio_process_filters=%d"
      L"&is_quality_limited=%d&is_adjust_bitrate=%d&high_codec_open=%d"
      L"&is_encoder_debug=%d&is_saving_data_debug=%d&platform=%d&is_assess_video_quality=%d"
      L"&is_http_proxy=%d",
      mLiveParam.width,
      mLiveParam.height,
      mLiveParam.frame_rate,
      mLiveParam.bit_rate,
      mLiveParam.gop_interval,
      (int)mLiveParam.drop_frame_type,
      mLiveParam.sample_rate,
      mLiveParam.ch_num,
      mLiveParam.audio_bitrate,
      mLiveParam.src_sample_fmt,
      mLiveParam.encode_sample_fmt,
      mLiveParam.publish_timeout,
      mLiveParam.publish_reconnect_times,
      (int)mLiveParam.encode_type,
      (int)mLiveParam.encode_pix_fmt,
      (int)mLiveParam.live_model,
      mLiveParam.video_process_filters,
      mLiveParam.audio_process_filters,
      mLiveParam.is_quality_limited ? 1 : 0,
      mLiveParam.is_adjust_bitrate ? 1 : 0,
      mLiveParam.high_codec_open,
      mLiveParam.is_encoder_debug ? 1 : 0,
      mLiveParam.is_saving_data_debug ? 1 : 0,
      mLiveParam.platform,
      mLiveParam.is_assess_video_quality ? 1 : 0,
      mLiveParam.is_http_proxy ? 1 : 0
   );

   g_pLogger->logInfo(L"MediaCore::GetVideoParamInfo %s", info);
}

void MediaCore::SetVolumeAmplificateSize(float size)
{
   if (NULL != mLivePusher)
   {
      mLivePusher->SetVolumeAmplificateSize(size);
   }
}

int MediaCore::GetHighCodec()
{
   return mLiveParam.high_codec_open;
}

void MediaCore::SetVideoQuality(bool quality) {
   mLiveParam.is_assess_video_quality = quality;
   //mLiveParam.is_adjust_bitrate = adjust;
   //mLiveParam.bit_rate = kbps;
}

int MediaCore::LivePushAmf0Msg(const char* data, int len) {
   if (mLivePusher) {
      mLivePusher->LivePushAmf0Msg(string(data, len));
   }
   return 0;
}

void MediaCore::SetOpenNoiseCancelling(bool bNoiseReduction)
{
   if (mLivePusher) {
      mLivePusher->OpenNoiseCancelling(bNoiseReduction);
   }
   return;
}

void MediaCore::OnOutputVideoData(const int8_t *data, const int size, const uint64_t timestamp, const LiveExtendParam *extendParam) {
   if (extendParam) {
      LiveExtendParam exParam;
      exParam.same_last = extendParam->same_last;
      exParam.scene_type = extendParam->scene_type;
      mLivePusher->LivePushVideo((const char *)data, mLiveParam.width * mLiveParam.height * 3 / 2,timestamp, &exParam);
   }
}

void MediaCore::OnOutputAudioData(const int8_t *data, const int size, const uint64_t timestamp) {
   if (mLivePusher) {
      mLivePusher->LivePushAudio((const char *)data, size, timestamp);
   }
}

wchar_t * UTF8ToUnicode(const char* str) {
   int textlen;
   wchar_t * result;
   textlen = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0);
   result = (wchar_t *)malloc((textlen + 1) * sizeof(wchar_t));
   memset(result, 0, (textlen + 1) * sizeof(wchar_t));
   MultiByteToWideChar(CP_UTF8, 0, str, -1, (LPWSTR)result, textlen);
   return result;
}

void MediaCore::OnResult(const std::string &result, bool is_last) {
   WCHAR* pdata = UTF8ToUnicode(result.c_str());
   if (pdata && mMediaCoreEvent) {
      mMediaCoreEvent->OnMeidaTransition(pdata, lstrlenW(pdata));
      free(pdata);
      pdata = NULL;
   }
}

bool MediaCore::IsEnableVoiceTransition() {
   return mbEnableVoiceTransition;
}

void MediaCore::SetEnableVoiceTransition(bool enable) {
   mbEnableVoiceTransition = enable;
}

void MediaCore::StartVoiceTransition(bool start /*= false*/, int font /*= 15*/, int lan /*= 0*/) {
   g_pLogger->logInfo("StartVoiceTransition start : %d\n", start);
   if (mVoiceTransition) {
      if (start) {
         SetEnableVoiceTransition(false);
         mVoiceTransition->StopPrepare();
         int fontInVideoSize = font + 10;
         mVoiceTransition->SetFontSize(fontInVideoSize);
         switch (lan) {
         case SupportLanguageType_Mandarin: {
            mVoiceTransition->SetAccent(SupportLanguageStr_Mandarin);  //普通话
            break;
         }
         case SupportLanguageType_Cantonese: {
            mVoiceTransition->SetAccent(SupportLanguageStr_Cantonese);   //粤语
            break;
         }
         case  SupportLanguageType_Lmz: {
            mVoiceTransition->SetAccent(SupportLanguageStr_Lmz);   //四川话
            break;
         }
         default:
            break;
         }
         mVoiceTransitionFontSize = fontInVideoSize;
         mCurrentSupportLanguageType = (SupportLanguageType)lan;

         mVoiceTransition->SetAudioInfo(mLiveParam.ch_num, mLiveParam.src_sample_fmt, mLiveParam.sample_rate);
         mVoiceTransition->SetVideoInfo(mLiveParam.width, mLiveParam.height, mLiveParam.encode_pix_fmt, mLiveParam.frame_rate);
         int nRet = mVoiceTransition->StartPrepare(VoiceTransition_APPID);
         if (nRet != 0) {
            g_pLogger->logInfo("StartVoiceTransition errcode : %d\n", nRet);
         }
         else {
            SetEnableVoiceTransition(true);
         }
      }
      else {
         SetEnableVoiceTransition(false);
         mVoiceTransition->StopPrepare();
      }
   }
   g_pLogger->logInfo("StartVoiceTransition start ok\n");
}

void MediaCore::ResetVoiceTransition() {
   if (mbEnableVoiceTransition) {
      StartVoiceTransition(false);
      StartVoiceTransition(true, mVoiceTransitionFontSize, mCurrentSupportLanguageType);
   }
}

void MediaCore::DesktopEnhanceSwitchOn(bool open) {
   mbIsDesktopImageHance = open;
   g_pLogger->logInfo("DesktopEnhanceSwitchOn open %d\n", open);
}

void MediaCore::SetDesktopStatus(bool isDesktopShare) {
   mbIsDesktopShare = isDesktopShare;
   g_pLogger->logInfo("SetDesktopStatus open %d\n", isDesktopShare);
}

void MediaCore::GetMediaFileWidthAndHeight(const char* path, int &width, int& height) {
   vhall::FFmpegDemuxer ffmpegDemuxer;
   int nRet = ffmpegDemuxer.Init(path);
   if (nRet == 0 && ffmpegDemuxer.GetVideoPar() != NULL) {
      width = ffmpegDemuxer.GetVideoPar()->width;
      height = ffmpegDemuxer.GetVideoPar()->height;
   }
}

MEDIACORE_API IMediaCore* CreateMediaCore(const wchar_t* logPath/* = NULL*/) {
   if (g_pLogger == NULL) {
      SYSTEMTIME loSystemTime;
      GetLocalTime(&loSystemTime);
      wchar_t lwzLogFileName[255] = { 0 };
      if (logPath) {
         wsprintf(lwzLogFileName, L"%s%s_%4d_%02d_%02d_%02d_%02d%s", VH_LOG_DIR, L"MediaCore", loSystemTime.wYear, loSystemTime.wMonth, loSystemTime.wDay, loSystemTime.wHour, loSystemTime.wMinute, L".log");
         g_pLogger = new Logger(lwzLogFileName, USER);
      }
      else {
         if (!CreateDirectoryW(logPath, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
            OutputDebugStringW(L"Logger::Logger: CreateDirectoryW failed.");
         }
         SYSTEMTIME loSystemTime;
         GetLocalTime(&loSystemTime);
         wsprintf(lwzLogFileName, L"%s%s_%4d_%02d_%02d_%02d_%02d%s", VH_LOG_DIR, L"MediaCore", loSystemTime.wYear, loSystemTime.wMonth, loSystemTime.wDay, loSystemTime.wHour, loSystemTime.wMinute, L".log");
         g_pLogger = new Logger(lwzLogFileName, USER);
      }
   }
   return new MediaCore();
}
MEDIACORE_API void DestoryMediaCore(IMediaCore**mediaCore) {
   delete *mediaCore;
   *mediaCore = NULL;
}


