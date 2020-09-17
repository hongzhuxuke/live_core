#ifndef _INTERFACE_ENCODER_INCLUDE_H__
#define _INTERFACE_ENCODER_INCLUDE_H__

#include "Utility.h"
#include "VH_ConstDeff.h"
#include <functional>
#include <list>


struct ClosableStream {
   virtual ~ClosableStream() {}
};

//-------------------------------------------------------------------

struct DataPacket {
   LPBYTE lpPacket;
   UINT size;
};

//-------------------------------------------------------------------

enum PacketType {
   PacketType_VideoDisposable,
   PacketType_VideoLow,
   PacketType_VideoHigh,
   PacketType_VideoHighest,
   PacketType_Audio
};

class NetworkStream : public ClosableStream {
public:
   virtual void SetStreamSleepReInit(){};
   virtual DWORD GetTimeSlot(){return 0;};
   
   virtual ~NetworkStream() {}
   virtual void AddArgument(const char* key, const char* value) = 0;
   virtual void SendPacket(BYTE *data, UINT size, DWORD timestamp, PacketType type) = 0;
   virtual void BeginPublishing() {}
   virtual double GetPacketStrain() const = 0;
   virtual QWORD GetCurrentSentBytes() = 0;
   virtual unsigned long GetByteSpeed()=0;
   virtual UINT64 GetSendVideoFrameCount() = 0;
   virtual uint64_t GetUpdateTime() = 0;
   virtual UINT GetChunkSize() = 0;
   virtual UINT QueueNum() = 0;
   virtual UINT GetConnectCount() = 0;
   virtual DWORD NumDroppedFrames() const = 0;
   virtual DWORD NumDroppedFramesByReconnected() const = 0;
   virtual DWORD NumTotalVideoFrames() const = 0;
   virtual bool IsCanRestart() = 0;
   virtual void Shutdown() = 0;
   virtual void WaitForShutdownComplete() = 0;
   virtual void ResetEncoder(void* audioEncoder, void* videoEncoder)=0;
   virtual void SetAutoSpeed(bool){}
   virtual std::string GetServerIP()=0;
   virtual std::string GetStreamID()=0;
   virtual int GetSpeedLevel()=0;
   virtual void ResetMultiConn( int multiConnNum, int multiConnBufSize) = 0;
   virtual void SetDispath(bool,const Dispatch_Param &) = 0;
};

//-------------------------------------------------------------------

struct TimedPacket {
   List<BYTE> data;
   DWORD timestamp;
   PacketType type;
};

//-------------------------------------------------------------------

class VideoFileStream : public ClosableStream {
public:
   virtual ~VideoFileStream() {}
   virtual void AddPacket(const BYTE *data, UINT size, DWORD timestamp, DWORD pts, PacketType type) = 0;
   virtual void AddPacket(std::shared_ptr<const std::vector<BYTE>> data, DWORD timestamp, DWORD pts, PacketType type) {
      AddPacket(data->data(), static_cast<UINT>(data->size()), timestamp, pts, type);
   }

};

String ExpandRecordingFilename(String);
String GetExpandedRecordingDirectoryBase(String);




//-------------------------------------------------------------------

class AudioEncoder {

public:
   virtual bool    Encode(float *input, UINT numInputFrames, DataPacket &packet, QWORD &timestamp) = 0;
   virtual void    GetHeaders(DataPacket &packet) = 0;


   virtual ~AudioEncoder() {}

   virtual UINT    GetFrameSize() const = 0;

   virtual int     GetBitRate() const = 0;
   virtual CTSTR   GetCodec() const = 0;
   virtual UINT GetSampleRateHz() const = 0;
   virtual UINT NumAudioChannels() const = 0;

   virtual String  GetInfoString() const = 0;
};

//-------------------------------------------------------------------

class VideoEncoder {
public:
   virtual ~VideoEncoder() {}
   virtual bool Encode(LPVOID *picInPtr[], List<DataPacket> &packets, List<PacketType> &packetTypes, DWORD timestamp, DWORD &out_pts, SceneType sceneType, int maxBitrate) = 0;
   virtual void RequestBuffers(LPVOID buffers) {}
   virtual int  GetBitRate() const = 0;
   virtual bool DynamicBitrateSupported() const = 0;
   virtual bool SetBitRate(DWORD maxBitrate, DWORD bufferSize) = 0;
   virtual void GetHeaders(DataPacket &packet) = 0;
   virtual void GetSEI(DataPacket &packet) {}
   virtual void SetNeedRequestKeyframe(const bool& isNeedRequestKeyframe) = 0;
   virtual bool IsNeedRequestKeyframe() const = 0;
   virtual String GetInfoString() const = 0;
   virtual UINT   GetFps() = 0;
   virtual UINT   GetResolutionCX() = 0;
   virtual UINT   GetResolutionCY() = 0;
   virtual bool isQSV() { return false; }
   virtual int GetBufferedFrames() { if (HasBufferedFrames()) return -1; return 0; }
   virtual bool HasBufferedFrames() { return false; }
};

struct VideoPacket {
   List<BYTE> Packet;
   inline void FreeData() { Packet.Clear(); }
};

enum ColorPrimaries {
   ColorPrimaries_BT709 = 1,
   ColorPrimaries_Unspecified,
   ColorPrimaries_BT470M = 4,
   ColorPrimaries_BT470BG,
   ColorPrimaries_SMPTE170M,
   ColorPrimaries_SMPTE240M,
   ColorPrimaries_Film,
   ColorPrimaries_BT2020
};

enum ColorTransfer {
   ColorTransfer_BT709 = 1,
   ColorTransfer_Unspecified,
   ColorTransfer_BT470M = 4,
   ColorTransfer_BT470BG,
   ColorTransfer_SMPTE170M,
   ColorTransfer_SMPTE240M,
   ColorTransfer_Linear,
   ColorTransfer_Log100,
   ColorTransfer_Log316,
   ColorTransfer_IEC6196624,
   ColorTransfer_BT1361,
   ColorTransfer_IEC6196621,
   ColorTransfer_BT202010,
   ColorTransfer_BT202012
};

enum ColorMatrix {
   ColorMatrix_GBR = 0,
   ColorMatrix_BT709,
   ColorMatrix_Unspecified,
   ColorMatrix_BT470M = 4,
   ColorMatrix_BT470BG,
   ColorMatrix_SMPTE170M,
   ColorMatrix_SMPTE240M,
   ColorMatrix_YCgCo,
   ColorMatrix_BT2020NCL,
   ColorMatrix_BT2020CL
};


struct ColorDescription {
   int fullRange;
   int primaries;
   int transfer;
   int matrix;
};

char* EncMetaData(char *enc, char *pend, bool bFLVFile, AudioEncoder*audioEncoder, VideoEncoder* videoEncoder);



#endif //_INTERFACE_ENCODER_INCLUDE_H__
//----------------------------
