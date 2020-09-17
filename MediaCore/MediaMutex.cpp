#include "Utility.h"
#include "IEncoder.h"
#include "Encoder_x264.h"
#include "Encoder_AAC.h"
#include "FLVFileStream.h"
#include "RTMPStuff.h"
#include "RTMPPublisherReal.h"
#include "MediaCapture.h"
#include "MediaMutex.h"


MediaMutex::MediaMutex(IMediaCoreEvent* mediaCoreEvent) {
   mSoundDataMutex = OSCreateMutex();
   mAudioEncoder = NULL;
   mVideoEncoder = NULL;
   mFileStream.reset(NULL);
   mMediaCoreEvent = mediaCoreEvent;
   mRtmpCount=0;
   mNetwork.Clear();
}
MediaMutex::~MediaMutex() {
   for(UINT i=0;i<mNetwork.Num();i++)
   {
      delete mNetwork[i];
   }
   mNetwork.Clear();
   mFileStream.reset(NULL);
   OSCloseMutex(mSoundDataMutex);


   for(UINT i=0;i<mPendingAudioFrames.Num();i++)
   {
      mPendingAudioFrames[i].audioData.Clear();
   }
   mPendingAudioFrames.Clear();
}
bool MediaMutex::InitEncoder(const VideoInfo &videoInfo, const AudioInfo &audioInfo) {
   mIsSentHeaders = true;
   mLastAudioTimestamp = 0;
   mVideoInfo = videoInfo;
   mAudioInfo = audioInfo;
   if (mAudioEncoder) {
      delete mAudioEncoder;
      mAudioEncoder = NULL;
   }
   if (mVideoEncoder) {
      delete mVideoEncoder;
      mVideoEncoder = NULL;
   }
   mBufferedVideo.Clear();
   mBufferedTimes.Clear();
   mPendingAudioFrames.Clear();

   if (videoInfo.enable) {
      ColorDescription colorDesc;
      int maxBitRate = videoInfo.bitrate;
      int bufferSize = maxBitRate; // local buffer size         
      int quality = 8;
      String preset = L"veryfast";
      bool bUsing444 = false;
      bool bUseCFR = true;
      SceneType sceneType = videoInfo.sceneType;
      colorDesc.fullRange = 0;
      colorDesc.primaries = ColorPrimaries_BT709;
      colorDesc.transfer = ColorTransfer_IEC6196621;
      colorDesc.matrix = videoInfo.width >= 1280 || videoInfo.height > 576 ? ColorMatrix_BT709 : ColorMatrix_SMPTE170M;
	  mVideoEncoder = CreateX264Encoder(videoInfo.fps, videoInfo.gop, videoInfo.high_profile_oepn, videoInfo.width, videoInfo.height, quality, preset, bUsing444, colorDesc, maxBitRate, bufferSize, bUseCFR, sceneType);
	  mVideoEncoder->SetBitRate(maxBitRate, maxBitRate);
   }
   if (audioInfo.enable) {
      mAudioEncoder = CreateAACEncoder(audioInfo.bitRate, audioInfo.sample, audioInfo.channels);
   }
   mRtmpCount = 0;
   return true;
}
bool MediaMutex::AddRtmpPublisher(const RtmpInfo &rtmpInfo) {
   //copy RTMP information
   if (rtmpInfo.rtmpURL == NULL
       || rtmpInfo.streamname == NULL) {
      return false;
   }

    
   mRtmpURL.push_back(std::string(rtmpInfo.rtmpURL));
   mRtmpPlaypath.push_back(std::string(rtmpInfo.streamname));
   mUserName.push_back(std::string(rtmpInfo.strUser));
   mUserPass.push_back(std::string(rtmpInfo.strPass));
   mToken.push_back(std::string(rtmpInfo.token));

   mMultiConnNum.push_back(rtmpInfo.iMultiConnNum);
   mMultiConnBufSize.push_back(rtmpInfo.iMultiConnBufSize);

   mRtmpCount++;

   m_bDispatch = rtmpInfo.m_bDispatch;
   m_dispatchParam = rtmpInfo.m_dispatchParam;

   return true;
}
bool MediaMutex::AddFileStream(String &recordFile) {
   if (recordFile.IsEmpty() == false) {
      mFileStream.reset(CreateFLVFileStream(recordFile, mAudioEncoder, mVideoEncoder));
      return true;
   }
   return false;
}

void MediaMutex::DeleteFileStream() {
   mFileStream.reset(NULL);
}
void MediaMutex::EncodeAudioSegment(float *buffer, UINT numFrames, QWORD timestamp) {
   DataPacket packet;
   if (mAudioEncoder&&mAudioEncoder->Encode(buffer, numFrames, packet, timestamp)) {
      OSEnterMutex(mSoundDataMutex);
      FrameAudio *frameAudio = mPendingAudioFrames.CreateNew();
      frameAudio->audioData.CopyArray(packet.lpPacket, packet.size);
      frameAudio->timestamp = timestamp;
      OSLeaveMutex(mSoundDataMutex);
   }
}
bool MediaMutex::EncodeVideo(LPVOID *inputPic[], QWORD Timestamp, SceneType sceneType, int maxBitrate) {     //encode loop
   List<DataPacket> videoPackets;
   List<PacketType> videoPacketTypes;

   //------------------------------------
   // encode

   mBufferedTimes << Timestamp;

   VideoSegment curSegment;
   bool bProcessedFrame, bSendFrame = false;


   DWORD out_pts = 0;
   mVideoEncoder->Encode(inputPic, videoPackets, videoPacketTypes, mBufferedTimes[0], out_pts, sceneType, maxBitrate);

   bProcessedFrame = (videoPackets.Num() != 0);

   //buffer video data before sending out
   if (bProcessedFrame) {
      bSendFrame = BufferVideoData(videoPackets, videoPacketTypes, mBufferedTimes[0], out_pts, mFirstFrameTime, curSegment);
      mBufferedTimes.Remove(0);
   } else
      nop();

   //profileOut;

   //------------------------------------
   // upload

   profileIn("sending stuff out");

   //send headers before the first frame if not yet sent
   if (bSendFrame)
      SendFrame(curSegment, mFirstFrameTime);

   //解决视频队列缓存过大bug(导致观看端延时过大)
   if (mBufferedVideo.Num() > 0) {
      SendVideoFrames();
   }
   

   profileOut;

   return bProcessedFrame;
}

bool MediaMutex::BufferVideoData(const List<DataPacket> &inputPackets,
                                 const List<PacketType> &inputTypes,
                                 DWORD timestamp,
                                 DWORD out_pts,
                                 QWORD firstFrameTime,
                                 VideoSegment &segmentOut) {

   VideoSegment &segmentIn = *mBufferedVideo.CreateNew();
   segmentIn.timestamp = timestamp;
   segmentIn.pts = out_pts;

   segmentIn.packets.SetSize(inputPackets.Num());
   for (UINT i = 0; i < inputPackets.Num(); i++) {
      segmentIn.packets[i].data.CopyArray(inputPackets[i].lpPacket, inputPackets[i].size);
      segmentIn.packets[i].type = inputTypes[i];
   }

   bool dataReady = false;

   OSEnterMutex(mSoundDataMutex);
   for (UINT i = 0; i < mPendingAudioFrames.Num(); i++) {
      if (firstFrameTime < mPendingAudioFrames[i].timestamp
          && mPendingAudioFrames[i].timestamp - firstFrameTime >= mBufferedVideo[0].timestamp) {
         dataReady = true;
         break;
      }
   }
   OSLeaveMutex(mSoundDataMutex);

   if (dataReady) {
      segmentOut.packets.TransferFrom(mBufferedVideo[0].packets);
      segmentOut.timestamp = mBufferedVideo[0].timestamp;
      segmentOut.pts = mBufferedVideo[0].pts;
      mBufferedVideo.Remove(0);

      return true;
   }

   return false;
}
void MediaMutex::SendVideoFrames() {


   
   while (mBufferedVideo.Num() > 0) {
      VideoSegment curSegment;
      curSegment.packets.TransferFrom(mBufferedVideo[0].packets);
      curSegment.timestamp = mBufferedVideo[0].timestamp;
      curSegment.pts = mBufferedVideo[0].pts;
      mBufferedVideo.Remove(0);
      //发送视频
      for (UINT i = 0; i < curSegment.packets.Num(); i++) {
         VideoPacketData &packet = curSegment.packets[i];
         if (packet.type == PacketType_VideoHighest) {
            if (mVideoEncoder)
               mVideoEncoder->SetNeedRequestKeyframe(false);
         }

         for(UINT j=0;j<mNetwork.Num();j++)
         {
            if (!HandleStreamStopInfo(mNetworkStop, packet.type, curSegment))
            {
               mNetwork[j]->SendPacket(packet.data.Array(), packet.data.Num(), curSegment.timestamp, packet.type);
            }
         }        

         if (mFileStream || mReplayBufferStream) {
            auto shared_data = std::make_shared<const std::vector<BYTE>>(packet.data.Array(), packet.data.Array() + packet.data.Num());
            if (mFileStream) {
               if (!HandleStreamStopInfo(mFileStreamStop, packet.type, curSegment))
                  mFileStream->AddPacket(shared_data, curSegment.timestamp, curSegment.pts, packet.type);
            }
            if (mReplayBufferStream) {
               if (!HandleStreamStopInfo(mReplayBufferStop, packet.type, curSegment))
                  mReplayBufferStream->AddPacket(shared_data, curSegment.timestamp, curSegment.pts, packet.type);
            }
         }
      }
      //发送音频

      OSEnterMutex(mSoundDataMutex);

      //音频缓存数据包 大于0
      if (mPendingAudioFrames.Num()) {
         //如果音频数据包 大于0
         while (mPendingAudioFrames.Num()) {
            if (mPendingAudioFrames[0].timestamp) {
               //队列里第一个音频包的时间戳
               UINT audioTimestamp = UINT(mPendingAudioFrames[0].timestamp);
               //如果第一个音频包的时间戳大于视频时间戳   跳出
               //stop sending audio packets when we reach an audio timestamp greater than the video timestamp
               if (audioTimestamp > curSegment.timestamp)
                  break;
               //如果第一个音频包的时间戳为0或者第一个音频包的时间戳大于上一个音频包时间戳
               if (audioTimestamp == 0 || audioTimestamp > mLastAudioTimestamp) {
                  //取第一个音频包的时间戳
                  List<BYTE> &audioData = mPendingAudioFrames[0].audioData;
                  if (audioData.Num()) {
                     //Log(TEXT("a:%u, %llu"), audioTimestamp, frameInfo.firstFrameTime+audioTimestamp);
                     //发送 第一个音频包的时间戳
                     for (UINT i = 0; i<mNetwork.Num(); i++)
                     {
                        if (mNetwork[i]){
                           mNetwork[i]->SendPacket(audioData.Array(), audioData.Num(), audioTimestamp, PacketType_Audio);
                        }
                     }



                     if (mFileStream || mReplayBufferStream) {
                        auto shared_data = std::make_shared<const std::vector<BYTE>>(audioData.Array(), audioData.Array() + audioData.Num());
                        if (mFileStream)
                           mFileStream->AddPacket(shared_data, audioTimestamp, audioTimestamp, PacketType_Audio);
                        if (mReplayBufferStream)
                           mReplayBufferStream->AddPacket(shared_data, audioTimestamp, audioTimestamp, PacketType_Audio);
                     }

                     audioData.Clear();

                     mLastAudioTimestamp = audioTimestamp;
                  }
               }
            } else
               nop();

            mPendingAudioFrames[0].audioData.Clear();
            mPendingAudioFrames.Remove(0);
         }
      }

      OSLeaveMutex(mSoundDataMutex);
   }
}
void MediaMutex::SendFrame(VideoSegment &curSegment, QWORD firstFrameTime) {
   #if 0
   //低速
   std::list<int> lowerSpeedIndexList;
   //高速
   std::list<int> heigherSpeedIndexList;

   
   
   for(int i=0;i<mNetwork.Num();i++)
   {
      DWORD timeSlot=mNetwork[i]->GetTimeSlot();
      //Log(TEXT("network[%d] timeSlot [%lu]"),i,timeSlot);
   }
   #endif

   if (!mIsSentHeaders) {
      for (UINT i = 0; i<mNetwork.Num(); i++)
      {
         if (mNetwork[i] && curSegment.packets[0].data[0] == 0x17) {
            mNetwork[i]->BeginPublishing();
            mIsSentHeaders = true;
         }
      }
   }

   OSEnterMutex(mSoundDataMutex);

   if (mPendingAudioFrames.Num()) {
      while (mPendingAudioFrames.Num()) {
         if (firstFrameTime < mPendingAudioFrames[0].timestamp) {
            UINT audioTimestamp = UINT(mPendingAudioFrames[0].timestamp - firstFrameTime);

            //stop sending audio packets when we reach an audio timestamp greater than the video timestamp
            if (audioTimestamp > curSegment.timestamp)
               break;

            if (audioTimestamp == 0 || audioTimestamp > mLastAudioTimestamp) {
               List<BYTE> &audioData = mPendingAudioFrames[0].audioData;
               if (audioData.Num()) {
                  //Log(TEXT("a:%u, %llu"), audioTimestamp, frameInfo.firstFrameTime+audioTimestamp);

                  for (UINT i = 0; i<mNetwork.Num(); i++)
                  {
                     if (mNetwork[i]){
                        mNetwork[i]->SendPacket(audioData.Array(), audioData.Num(), audioTimestamp, PacketType_Audio);
                        }
                  }

                  if (mFileStream || mReplayBufferStream) {
                     auto shared_data = std::make_shared<const std::vector<BYTE>>(audioData.Array(), audioData.Array() + audioData.Num());
                     if (mFileStream)
                        mFileStream->AddPacket(shared_data, audioTimestamp, audioTimestamp, PacketType_Audio);
                     if (mReplayBufferStream)
                        mReplayBufferStream->AddPacket(shared_data, audioTimestamp, audioTimestamp, PacketType_Audio);
                  }

                  audioData.Clear();

                  mLastAudioTimestamp = audioTimestamp;
               }
            }
         } else
            nop();

         mPendingAudioFrames[0].audioData.Clear();
         mPendingAudioFrames.Remove(0);
      }
   }

   OSLeaveMutex(mSoundDataMutex);

   for (UINT i = 0; i < curSegment.packets.Num(); i++) {
      VideoPacketData &packet = curSegment.packets[i];

      if (packet.type == PacketType_VideoHighest) {
         if (mVideoEncoder)
            mVideoEncoder->SetNeedRequestKeyframe(false);
      }
      
      for (UINT j = 0; j<mNetwork.Num(); j++)
      {
         if (mNetwork[j]) 
         {
            if (!HandleStreamStopInfo(mNetworkStop, packet.type, curSegment))
            {
               mNetwork[j]->SendPacket(packet.data.Array(), packet.data.Num(), curSegment.timestamp, packet.type);
            }
         }
      }

      if (mFileStream || mReplayBufferStream) {
         auto shared_data = std::make_shared<const std::vector<BYTE>>(packet.data.Array(), packet.data.Array() + packet.data.Num());
         if (mFileStream) {
            if (!HandleStreamStopInfo(mFileStreamStop, packet.type, curSegment))
               mFileStream->AddPacket(shared_data, curSegment.timestamp, curSegment.pts, packet.type);
         }
         if (mReplayBufferStream) {
            if (!HandleStreamStopInfo(mReplayBufferStop, packet.type, curSegment))
               mReplayBufferStream->AddPacket(shared_data, curSegment.timestamp, curSegment.pts, packet.type);
         }
      }
   }
}
void MediaMutex::Shutdown() {

   for (UINT i = 0; i<mNetwork.Num(); i++)
   {
      if (mNetwork[i])
         mNetwork[i]->Shutdown();
   }
   mFileStream.reset(NULL);
}
void MediaMutex::Destory() {



   for (UINT i = 0; i<mNetwork.Num(); i++)
   {
      if (mNetwork[i])
         mNetwork[i]->Shutdown();
   }
   mFileStream.reset(NULL);
   for (UINT i = 0; i<mNetwork.Num(); i++)
   {
      if (mNetwork[i])
         mNetwork[i]->WaitForShutdownComplete();
      delete mNetwork[i];
   }
   mNetwork.Clear();

   if (mVideoEncoder) {
      delete mVideoEncoder;
      mVideoEncoder = NULL;
   }
   if (mAudioEncoder) {
      delete mAudioEncoder;
      mAudioEncoder = NULL;
   }
}
void MediaMutex::RestartNetwork() {
   #if 0
   for(int i=0;i<mNetwork.Num();i++)
   {
      if (mNetwork[i]&& mNetwork[i]->IsCanRestart()) {
         delete mNetwork[i];
         mNetwork[i]=NULL;
         mIsSentHeaders = false;
      }
   }
   mNetwork.Clear();
   #endif
   ReInitRtmpPublisher();
}
void MediaMutex::UpdateStats(AudioStats *audioStats, VideoStats *videoStats) {
   OSEnterMutex(mSoundDataMutex);
   if (mPendingAudioFrames.Num() > 0) {
      audioStats->lastTimesToEncodeInMs = mPendingAudioFrames.Last().timestamp;
      audioStats->lastTimesWillSend = mPendingAudioFrames[0].timestamp;
   }
   OSLeaveMutex(mSoundDataMutex);
   if (mBufferedVideo.Num() > 0) {
      videoStats->lastTimesToEncodeInMs = mBufferedVideo.Last().timestamp;
      videoStats->lastTimesWillSend = mBufferedVideo[0].timestamp;
   }
}
int MediaMutex::GetSumSpeed() {

   int sumSpeed=0;
   for (UINT i = 0; i<mNetwork.Num(); i++)
   {
      sumSpeed += mNetwork[i]->GetByteSpeed();
   }
   
   return sumSpeed;
}
UINT64 MediaMutex::GetSendVideoFrameCount() {
   UINT64 sum = 0;
   for (UINT i = 0; i<mNetwork.Num(); i++)
   {
      sum += mNetwork[i]->GetSendVideoFrameCount();
   }

   return sum;
}
int MediaMutex::GetSpeedLevel() {
   int level=0;
   for (UINT i = 0; i<mNetwork.Num(); i++) {
      int tlevel = mNetwork[i]->GetSpeedLevel();
      if(tlevel > level) {
         level = tlevel;
      }
   }
   return level;
}

void MediaMutex::UpdateStreamStats(std::vector<StreamStats> *streamStatus) {
  
   for (UINT i = 0; i<mNetwork.Num(); i++)
   {
      (*streamStatus)[i].bytesSpeed = mNetwork[i]->GetByteSpeed();
      (*streamStatus)[i].currentDroppedFrames = (*streamStatus)[i].droppedFrames;
      (*streamStatus)[i].droppedFrames = mNetwork[i]->NumDroppedFrames();  
	  (*streamStatus)[i].droppedFramesByReconnected = mNetwork[i]->NumDroppedFramesByReconnected();
      (*streamStatus)[i].currentDroppedFrames =(*streamStatus)[i].droppedFrames-(*streamStatus)[i].currentDroppedFrames;
      (*streamStatus)[i].sumFrames=mNetwork[i]->NumTotalVideoFrames();
	  (*streamStatus)[i].last_update_time = mNetwork[i]->GetUpdateTime();
//	  (*streamStatus)[i].chunk_szie = mNetwork[i]->GetChunkSize();

	  (*streamStatus)[i].chunk_szie = mNetwork[i]->QueueNum();
	  (*streamStatus)[i].connect_count = mNetwork[i]->GetConnectCount();
	  (*streamStatus)[i].sumBytesSend = mNetwork[i]->GetCurrentSentBytes();
	  (*streamStatus)[i].m_DropFrameStatusCollect.totalTimeCount++;
	  (*streamStatus)[i].m_DropFrameStatusCollect.dropFrameRate_1sec = (*streamStatus)[i].currentDroppedFrames;
	  (*streamStatus)[i].m_DropFrameStatusCollect.dropFrameRate_total = (*streamStatus)[i].droppedFrames;
	  (*streamStatus)[i].m_DropFrameStatusCollect.dropFrame3[(*streamStatus)[i].m_DropFrameStatusCollect.totalTimeCount % 3] = (*streamStatus)[i].m_DropFrameStatusCollect.dropFrameRate_1sec;
	  (*streamStatus)[i].m_DropFrameStatusCollect.dropFrame5[(*streamStatus)[i].m_DropFrameStatusCollect.totalTimeCount % 5] = (*streamStatus)[i].m_DropFrameStatusCollect.dropFrameRate_1sec;
	  (*streamStatus)[i].m_DropFrameStatusCollect.dropFrame10[(*streamStatus)[i].m_DropFrameStatusCollect.totalTimeCount % 10] = (*streamStatus)[i].m_DropFrameStatusCollect.dropFrameRate_1sec;
	  (*streamStatus)[i].m_DropFrameStatusCollect.dropFrameRate_3sec = 0;
	  for (int j = 0; j < 3; j++)
	  {
		  (*streamStatus)[i].m_DropFrameStatusCollect.dropFrameRate_3sec = (*streamStatus)[i].m_DropFrameStatusCollect.dropFrameRate_3sec + (*streamStatus)[i].m_DropFrameStatusCollect.dropFrame3[j];
	  }
	  (*streamStatus)[i].m_DropFrameStatusCollect.dropFrameRate_3sec = (*streamStatus)[i].m_DropFrameStatusCollect.dropFrameRate_3sec / 3;
	  (*streamStatus)[i].m_DropFrameStatusCollect.dropFrameRate_5sec = 0;
	  for (int j = 0; j < 5; j++)
	  {
		  (*streamStatus)[i].m_DropFrameStatusCollect.dropFrameRate_5sec = (*streamStatus)[i].m_DropFrameStatusCollect.dropFrameRate_5sec + (*streamStatus)[i].m_DropFrameStatusCollect.dropFrame5[j];
	  }
	  (*streamStatus)[i].m_DropFrameStatusCollect.dropFrameRate_5sec = (*streamStatus)[i].m_DropFrameStatusCollect.dropFrameRate_5sec / 5;
	  (*streamStatus)[i].m_DropFrameStatusCollect.dropFrameRate_10sec = 0;
	  for (int j = 0; j < 10; j++)
	  {
		  (*streamStatus)[i].m_DropFrameStatusCollect.dropFrameRate_10sec = (*streamStatus)[i].m_DropFrameStatusCollect.dropFrameRate_10sec + (*streamStatus)[i].m_DropFrameStatusCollect.dropFrame10[j];
	  }
	  (*streamStatus)[i].m_DropFrameStatusCollect.dropFrameRate_10sec = (*streamStatus)[i].m_DropFrameStatusCollect.dropFrameRate_10sec / 10;
	  if ((*streamStatus)[i].m_DropFrameStatusCollect.totalTimeCount > 0)
	  {
		  (*streamStatus)[i].m_DropFrameStatusCollect.dropFrameRate_total = (*streamStatus)[i].m_DropFrameStatusCollect.dropFrameRate_total / (*streamStatus)[i].m_DropFrameStatusCollect.totalTimeCount;
	  }

      strcpy_s((*streamStatus)[i].serverIP ,20,mNetwork[i]->GetServerIP().c_str());
      strcpy_s((*streamStatus)[i].streamID ,128, mNetwork[i]->GetStreamID().c_str());
   }
}

void MediaMutex::SetAutoSpeed(bool autoSpeed) {
   for (UINT i = 0; i<mNetwork.Num(); i++)
   {
      if (mNetwork[i]) {
         mNetwork[i]->SetAutoSpeed(autoSpeed);
      }
   }
}
bool MediaMutex::ReInitRtmp()
{
   return ReInitRtmpPublisher();
}
bool MediaMutex::ResetPublishInfo(const char *currentUrl,const char *nextUrl)  {
   for(int i=0;i<mRtmpURL.size();i++) {
      if(mRtmpURL[i]==currentUrl) {
         mRtmpURL[i]=nextUrl;
      }
   }
   return true;
}

void MediaMutex::ClearRtmpInfo() {
   mRtmpURL.clear();
   mRtmpPlaypath.clear();
   mUserName.clear();
   mUserPass.clear();
   mToken.clear();
   mMultiConnNum.clear();
   mMultiConnBufSize.clear();
   mRtmpCount = 0;
}



bool MediaMutex::ReInitRtmpPublisher() {
   if (mNetwork.Num()==0) {
      for(int i=0;i<mRtmpCount;i++)
      {
         mNetwork.Add(CreateRTMPPublisherReal(mAudioEncoder, mVideoEncoder, mMediaCoreEvent, mMultiConnNum[i], mMultiConnBufSize[i]));
         mNetwork[i]->AddArgument(ARG_KEY_Publish, mRtmpURL[i].c_str());
         mNetwork[i]->AddArgument(ARG_KEY_PlayPath, mRtmpPlaypath[i].c_str());
         mNetwork[i]->AddArgument(ARG_KEY_Token, mToken[i].c_str());
         mNetwork[i]->SetDispath(m_bDispatch,m_dispatchParam);
      }
   }
   else 
   {
      for(int i=0;i<mRtmpCount;i++)
      {
         mNetwork[i]->ResetEncoder((void *)mAudioEncoder, (void *)mVideoEncoder);
         mNetwork[i]->AddArgument(ARG_KEY_Publish, mRtmpURL[i].c_str());
         mNetwork[i]->AddArgument(ARG_KEY_PlayPath, mRtmpPlaypath[i].c_str());
         mNetwork[i]->AddArgument(ARG_KEY_Token, mToken[i].c_str());
         mNetwork[i]->ResetMultiConn(mMultiConnNum[i], mMultiConnBufSize[i]);
         mNetwork[i]->SetDispath(m_bDispatch,m_dispatchParam);
      }
   }
   
   return true;
}
bool MediaMutex::HandleStreamStopInfo(StopInfo & info, PacketType type, const VideoSegment& segment) {
   if (type == PacketType_Audio || !info.func)
      return false;

   if (segment.pts < info.time)
      return false;

   if (!info.timeSeen) {
      info.timeSeen = true;
      return false;
   }

   info.func();
   info = {};
   return true;
}
