
#ifndef _INC_MP_MEDIA_PARAM_H
#define _INC_MP_MEDIA_PARAM_H

#if defined ( _MSC_VER ) && ( _MSC_VER >= 1000 )
#pragma once
#endif
#include <string>
#include <vector>

#define AUDIO_BASE_SENDPORT 20000
#define VIDEO_BASE_SENDPORT 30000
#define MAX_CAMERA_NUMBER 16

#define SYNC_INTERVAL_MAX_S 0.4f //0.4s
#define SYNC_INTERVAL_MAX_MS 300 //0.3s

using namespace std;

//buffer size define
#define AUDIO_RAW_BUF_SIZE 10000         //for aac: 1024 samples/frame per channel, for stereo 1024*16/8*2
#define AUDIO_ENC_BUF_SIZE 10000         //different between encoders
#define VIDEO_RAW_SCREEN_BUF_SIZE 2646000   //support maximum for yuv420 (1680x1050*3/2) support 23inch
#define VIDEO_RAW_CIF_BUF_SIZE 153000   //support maximum for yuv420 (352*288*3/2)
#define VIDEO_RAW_QCIF_BUF_SIZE 39000   //support maximum for yuv420 (176*144*3/2)
#define VIDEO_RAW_QVGA_BUF_SIZE 230400//116000  //support maximum for yuv420 (320*240*3/2)
#define VIDEO_RAW_QQVGA_BUF_SIZE 29000  //support maximum for yuv420 (160*120*3/2)
#define VIDEO_ENC_CIF_BUF_SIZE 50000    //different between encoders max for cif
#define VIDEO_ENC_QCIF_BUF_SIZE 20000   //different between encoders max for qcif
#define VIDEO_ENC_SCREEN_BUF_SIZE 50000   //
#define RTP_BUF_SIZE 1100               //for 1024 byte rtp packet
#define HTTP_BUF_SIZE 2100              //for virtual driver http audio data

//-------------------communication and transmission parameter------------------

//Communication protocol
typedef enum EnumCommuProto {
   COMMUN_PROTO_SOCK
} COMMU_PROTO_TE;

//net address
typedef struct StructNetAddr {
   string 		  gstrIpAddr;
   unsigned short	  usPort;
} NET_ADDR_TS;

//Communication parameter
typedef struct StructCommuParam {
   COMMU_PROTO_TE	eCommuProto;
   NET_ADDR_TS		tCommuAddr;
} COMMUN_PARAM_TS;

//Transfer protocol
typedef enum EnumTransProto {
   TRANS_PROTO_TYPE_NONE,
   TRANS_PROTO_RTP,
   TRANS_PROTO_HTTP,
   TRANS_PROTO_FTP
} TRANS_PROTO_TE;

//Transfer mode
typedef enum EnumTransMode {
   TRANS_MODE_NONE,
   TRANS_MODE_UNICAST,
   TRANS_MODE_MULTICAST
} TRANS_MODE_TE;

//Transfer parameter
typedef struct StructTransParam {
   string		   gstrName;
   string		   gstrCrossConfer;
   bool           bNatCross;
   NET_ADDR_TS    stNetAddr;
   TRANS_MODE_TE  eTransMode;

   StructTransParam() {
      bNatCross = false;
   }
} TRANS_PARAM_TS;

//---------------------------basic media parameter-------------------------

//Media type
typedef enum EnumMediaType {
   MEDIA_TYPE_NONE,
   MEDIA_TYPE_AUDIO,
   MEDIA_TYPE_VIDEO,
   MEDIA_TYPE_MIX
} MEDIA_TYPE_TE;

//Codec type
typedef enum EnumCodecType {
   CODEC_TYPE_NONE = -1,
   CODEC_AUDIO_PCM,
   CODEC_AUDIO_ADPCM,
   CODEC_AUDIO_MP3,
   CODEC_AUDIO_G711A,
   CODEC_AUDIO_G711U,
   CODEC_AUDIO_G723,
   CODEC_AUDIO_G7231,
   CODEC_AUDIO_G726,
   CODEC_AUDIO_FFMPEG_G726,
   CODEC_AUDIO_G729,
   CODEC_AUDIO_AAC,
   CODEC_AUDIO_VORBIS,
   CODEC_AUDIO_SPEEX,
   CODEC_VIDEO_RGB24,
   CODEC_VIDEO_YUV420P,
   CODEC_VIDEO_MJPEG,
   CODEC_VIDEO_MPEG4,
   CODEC_VIDEO_H264,
   CODEC_TYPE_BUTT
} CODEC_TYPE_TE;

//Rtp payload type
typedef enum EnumPayloadType {
   PAYLOAD_AUDIO_NONE	= 0,	//PCMU 8kHz mono
   PAYLOAD_AUDIO_ADPCM = 101,
   PAYLOAD_AUDIO_AAC   = 102,
   PAYLOAD_AUDIO_SPEEX	= 96,
   PAYLOAD_AUDIO_MP3	= 97,
   PAYLOAD_AUDIO_G711A	= 98,
   PAYLOAD_AUDIO_G711U	= 99,
   PAYLOAD_AUDIO_G723	= 4,	//8kHz mono
   PAYLOAD_AUDIO_G726	= 100,
   PAYLOAD_AUDIO_G729	= 18,	//8kHz mono
   PAYLOAD_VIDEO_MJPEG	= 26,	//90kHz
   PAYLOAD_VIDEO_H263	= 34,	//90kHz
   PAYLOAD_VIDEO_H264	= 105,
   PAYLOAD_VIDEO_X264	= 126,
   PAYLOAD_VIDEO_MPEG4	= 127,
   PAYLOAD_APL			= 128
} PAYLOAD_TYPE_TE;

//Video size
typedef enum EnumVideoSizeType {
   VIDEO_SIZE_NONE		= 0,
   VIDEO_SIZE_P_QQCIF	= 1,	//PAL 88x72
   VIDEO_SIZE_P_QCIF	= 2,	//176x144
   VIDEO_SIZE_P_CIF	= 3,	//352x288
   VIDEO_SIZE_P_2CIF	= 4,	//704x288
   VIDEO_SIZE_P_4CIF	= 5,	//704x576
   VIDEO_SIZE_P_DCIF	= 6,	//528x384
   VIDEO_SIZE_N_QQCIF	= 7,	//NTSC 88x60
   VIDEO_SIZE_N_QCIF	= 8,	//176x120
   VIDEO_SIZE_N_CIF	= 9,	//352x240
   VIDEO_SIZE_N_2CIF	= 10,	//704x240
   VIDEO_SIZE_N_4CIF	= 11,	//704x480
   VIDEO_SIZE_N_DCIF	= 12,	//528x320
   VIDEO_SIZE_VGA		= 13,	//640x480
   VIDEO_SIZE_QVGA		= 14,	//320x240
   VIDEO_SIZE_QQVGA	= 15,	//160x120
   VIDEO_SIZE_SVGA		= 16,	//800x600
   VIDEO_SIZE_XGA		= 17,	//1024x768
   VIDEO_SIZE_SCREEN	= 18	//max :1680x1050

} VIDEO_SIZE_TYPE_TE;


//Audio parameter
typedef struct StructAudioParam {
   CODEC_TYPE_TE	eCodec;
   int				iBitRate;
   int				iSampleRate;
   int				iSampleBits;
   int				iChannels;
} AUDIO_PARAM_TS;

//Video paramter
typedef struct StructVideoParam {
   CODEC_TYPE_TE		eCodec;
   int					iBitRate;
   VIDEO_SIZE_TYPE_TE	eVideoSize;
   int					iFrameRate;
} VIDEO_PARAM_TS;


//------------------process parameter----------------------

//process parameter type
typedef enum EnumProcessType {
   PROCESS_TYPE_NONE,
   PROCESS_AUDIO_RENDER,
   PROCESS_VIDEO_RENDER,
   PROCESS_AUDIO_SEND,
   PROCESS_VIDEO_SEND,
} PROCESS_TE;

//Capture parameter
typedef struct StructAudioCaptureParam {
   DWORD		threadID;
   int			iAudioDeviceID;
   int			iReserved;
} AUDIO_CAPTURE_PARAM_TS;

typedef struct StructVideoCaptureParam {
   int			iVideoDeviceID;
   int			iScreenX;
   int			iScreenY;
   int			iWidth;
   int			iHeight;
   int			iReserved;
} VIDEO_CAPTURE_PARAM_TS;

//Render parameter
typedef struct StructAudioRenderParam {
   PROCESS_TE  eProcessType;
   int	        iReserved;
} AUDIO_RENDER_PARAM_TS;

typedef struct StructVideoRenderParam {
   PROCESS_TE  eProcessType;
   HDC         hDC;
   int         iWnd_x;
   int         iWnd_y;
   int         iWndWidth;
   int         iWndHeigth;
   int	        iReserved;
} VIDEO_RENDER_PARAM_TS;

//Data receive parameter
typedef struct StructRecvParam {
   TRANS_PROTO_TE		eTransProto;
   string		 	 	gstrCName;
   string		        gstrCrossConfer;
   NET_ADDR_TS     	stRecvAddr;
   NET_ADDR_TS     	stSendSrcAddr;
   bool                bNatCross;
   int		            iCamType;
   string             nc_GstrUserName;
   string             nc_GstrUserPwd;
   string             nc_GstrCameraAgent;
   string 			nc_GstrCameraXmlPath;
   string             nc_GstrLocalIP;
   int                 nc_GstrLocalAudioPort;
   int                 nc_GstrLocalVideoPort;

   StructRecvParam() {
      bNatCross = false;
   }
} RECV_PARAM_TS;

typedef struct StructAudioRecvParam {
   RECV_PARAM_TS           stRecvParam;
   int                     iReserved;
} AUDIO_RECV_PARAM_TS;

typedef struct StructVideoRecvParam {
   RECV_PARAM_TS           stRecvParam;
   int                     iReserved;
} VIDEO_RECV_PARAM_TS;

//Data send parameter
typedef struct StructSendParam {
   string					gstrCName;
   unsigned short			usLocalTransPort;
   TRANS_PROTO_TE		    eTransProto;
   vector<TRANS_PARAM_TS>	vecDstTransParam;
   int		                iCamType;
   string                 nc_GstrUserName;
   string			        nc_GstrUserPwd;
   string                 nc_GstrCameraAgent;
   string 			    nc_GstrCameraXmlPath;
   string                 nc_GstrLocalIP;
   int                     nc_GstrDestAudioPort;
} SEND_PARAM_TS;

typedef struct StructAudioSendParam {
   PROCESS_TE    eProcessType;
   SEND_PARAM_TS stSendParam;
   bool          bAddDestination;
   int           iReserved;
} AUDIO_SEND_PARAM_TS;

typedef struct StructVideoSendParam {
   PROCESS_TE    eProcessType;
   SEND_PARAM_TS stSendParam;
   bool          bAddDestination;
   int           iReserved;
} VIDEO_SEND_PARAM_TS;

//Sort parameter
typedef struct StructAudioSortParam {
   int	iWaitDelayTime;
   int	iWaitPacketNum;
} AUDIO_SORT_PARAM_TS;

typedef struct StructVideoSortParam {
   int	iWaitDelayTime;
   int	iWaitPacketNum;
} VIDEO_SORT_PARAM_TS;

//Jitter parameter
typedef struct StructAudioJitterParam {
   int	iReserved;
} AUDIO_JITTER_PARAM_TS;

typedef struct StructVideoJitterParam {
   int	iReserved;
} VIDEO_JITTER_PARAM_TS;

//Encode parameter
typedef struct StructAudioEncParam {
   int	iReserved;
} AUDIO_ENC_PARAM_TS;

typedef struct StructVideoEncParam {
   int	iReserved;
} VIDEO_ENC_PARAM_TS;

//Decode parameter
typedef struct StructAudioDecParam {
   int	iReserved;
} AUDIO_DEC_PARAM_TS;

typedef struct StructVideoDecParam {
   int	iReserved;
} VIDEO_DEC_PARAM_TS;

//Store parameter
typedef struct StructAudioStoreParam {
   int		      iFileLength;
   string	      gstrRootDir;
} STORE_PARAM_TS;

typedef struct StructDatabaseOperateParam {
   unsigned short usServerPort;
   string	      gstrServerIP;
   DWORD		  dwDbOprThreadId;
   string		  gstrSQLUrl;
} DBOPR_PATAM_TS;

typedef struct StructStoreParamEx {
   int		       iFileLength;
   string	       gstrRootDir;
   string		   gstrSQLUrl;
   string		   gstrServerIp;
   string		   gstrCameraName;
   unsigned short usServerPort;
   DWORD		   dwDbOprThreadId;
} STORE_PARAMEX_TS;

//Distribute parameter
typedef struct StructDistributeParam {
   int	iBufferSize;
} DISTRIBUTE_PARAM_TS;

//Editor parameter
typedef struct StructAudioEditorParam {
   int	iReserved;
   bool bMuxer;
   string gstrFimeName;
   int  iIntervalTime;
} AUDIO_EDITOR_PARAM_TS;

typedef struct StructVideoEditorParam {
   int	iReserved;
   bool bMuxer;
   string gstrFimeName;
   int  iIntervalTime; //ms
   int  iFrameNum;
} VIDEO_EDITOR_PARAM_TS;

//------------------stream parameter-----------------------

#define UPDATE_AUDIO_CAPTURE_PARAM 1
#define UPDATE_VIDEO_CAPTURE_PARAM 2
#define UPDATE_AUDIO_RENDER_PARAM 4
#define UPDATE_VIDEO_RENDER_PARAM 8
#define UPDATE_AUDIO_RECV_PARAM 16
#define UPDATE_VIDEO_RECV_PARAM 32
#define UPDATE_AUDIO_SEND_PARAM 64
#define UPDATE_VIDEO_SEND_PARAM 128
#define UPDATE_ALL 0xFFFF

//Stream ID
typedef unsigned long HSTREAM;

//Receive render stream parameter
typedef struct StructStreamRecvRenderParam {
   MEDIA_TYPE_TE		    eMediaType;
   AUDIO_PARAM_TS		    stRecvAudioParam;
   VIDEO_PARAM_TS		    stRecvVideoParam;
   AUDIO_RECV_PARAM_TS	    stAudioRecvParam;
   VIDEO_RECV_PARAM_TS	    stVideoRecvParam;
   //AUDIO_SORT_PARAM_TS	stAudioSortParam;
   //VIDEO_SORT_PARAM_TS	stVideoSortParam;
   //AUDIO_JITTER_PARAM_TS	stAudioJitterParam;
   //VIDEO_JITTER_PARAM_TS	stVideoJitterParam;
   //AUDIO_DEC_PARAM_TS	stAudioDecParam;
   //VIDEO_DEC_PARAM_TS	stVideoDecParam;
   AUDIO_PARAM_TS		    stRenderAudioParam;
   VIDEO_PARAM_TS		    stRenderVideoParam;
   AUDIO_RENDER_PARAM_TS   stAudioRenderParam;
   VIDEO_RENDER_PARAM_TS   stVideoRenderParam;
} STREAM_RECV_RENDER_PARAM_TS;

//Receive render editor stream parameter
typedef struct StructStreamRecvRenderEditorParam {
   MEDIA_TYPE_TE		    eMediaType;
   AUDIO_PARAM_TS		    stRecvAudioParam;
   VIDEO_PARAM_TS		    stRecvVideoParam;
   AUDIO_RECV_PARAM_TS	    stAudioRecvParam;
   VIDEO_RECV_PARAM_TS	    stVideoRecvParam;
   //AUDIO_SORT_PARAM_TS	stAudioSortParam;
   //VIDEO_SORT_PARAM_TS	stVideoSortParam;
   //AUDIO_JITTER_PARAM_TS	stAudioJitterParam;
   //VIDEO_JITTER_PARAM_TS	stVideoJitterParam;
   //AUDIO_DEC_PARAM_TS	stAudioDecParam;
   //VIDEO_DEC_PARAM_TS	stVideoDecParam;
} STREAM_RECV_RENDER_EDITOR_PARAM_TS;

//Render uadate parameter
typedef struct StructRenderUpdateParam {
   MEDIA_TYPE_TE		  eMediaType;
   AUDIO_PARAM_TS		  stRenderAudioParam;
   VIDEO_PARAM_TS		  stRenderVideoParam;
   AUDIO_RENDER_PARAM_TS stAudioRenderParam;
   VIDEO_RENDER_PARAM_TS stVideoRenderParam;
} RENDER_UPDATE_PARAM_TS;

//Render uadate parameter
typedef struct StructEditorUpdateParam {
   MEDIA_TYPE_TE		  eMediaType;
   AUDIO_PARAM_TS		  stEditorAudioParam;
   VIDEO_PARAM_TS		  stEditorVideoParam;
   AUDIO_EDITOR_PARAM_TS stAudioEditorParam;
   VIDEO_EDITOR_PARAM_TS stVideoEditorParam;
} EDITOR_UPDATE_PARAM_TS;

//Receive render stream uadate parameter
typedef struct StructStreamRecvRenderUpdateParam {
   AUDIO_RENDER_PARAM_TS stAudioRenderParam;
   VIDEO_RENDER_PARAM_TS stVideoRenderParam;
} STREAM_RECV_RENDER_UPDATE_PARAM_TS;

//Capture send stream parameter
typedef struct StructStreamCaptureSendParam {
   MEDIA_TYPE_TE		    eMediaType;
   AUDIO_PARAM_TS		    stCapAudioParam;
   VIDEO_PARAM_TS		    stCapVideoParam;
   AUDIO_CAPTURE_PARAM_TS	stAudioCaptureParam;
   VIDEO_CAPTURE_PARAM_TS	stVideoCaptureParam;
   //AUDIO_ENC_PARAM_TS	stAudioEncParam;
   //VIDEO_ENC_PARAM_TS	stVideoEncParam;
   AUDIO_PARAM_TS		    stSendAudioParam;
   VIDEO_PARAM_TS		    stSendVideoParam;
   AUDIO_SEND_PARAM_TS		stAudioSendParam;
   VIDEO_SEND_PARAM_TS		stVideoSendParam;
} STREAM_CAPTURE_SEND_PARAM_TS;

//Capture sned stream uadate parameter
typedef struct StructStreamCapturSendUpdateParam {
   unsigned int            uiUpdateFlag;
   AUDIO_SEND_PARAM_TS		stAudioSendParam;
   VIDEO_SEND_PARAM_TS		stVideoSendParam;
} STREAM_CAPTURE_SEND_UPDATE_PARAM_TS;

//Receive transmit stream parameter
typedef struct StructStreamRecvTransmitParam {
   MEDIA_TYPE_TE			eMediaType;
   AUDIO_PARAM_TS			stRecvAudioParam;
   VIDEO_PARAM_TS			stRecvVideoParam;
   AUDIO_RECV_PARAM_TS	    stAudioRecvParam;
   VIDEO_RECV_PARAM_TS	    stVideoRecvParam;
   AUDIO_PARAM_TS		    stSendAudioParam;
   VIDEO_PARAM_TS		    stSendVideoParam;
   AUDIO_SEND_PARAM_TS		stAudioSendParam;
   VIDEO_SEND_PARAM_TS		stVideoSendParam;
} STREAM_RECV_TRANSMIT_PARAM_TS;

//Receive store stream parameter
typedef struct StructStreamRecvStoreParam {
   MEDIA_TYPE_TE			eMediaType;
   AUDIO_PARAM_TS		    stRecvAudioParam;
   VIDEO_PARAM_TS			stRecvVideoParam;
   AUDIO_RECV_PARAM_TS	    stAudioRecvParam;
   VIDEO_RECV_PARAM_TS	    stVideoRecvParam;
   //AUDIO_SORT_PARAM_TS	stAudioSortParam;
   //VIDEO_SORT_PARAM_TS	stVideoSortParam;
   //AUDIO_JITTER_PARAM_TS	stAudioJitterParam;
   //VIDEO_JITTER_PARAM_TS	stVideoJitterParam;
   AUDIO_PARAM_TS			stStoreAudioParam;
   VIDEO_PARAM_TS			stStoreVideoParam;
   STORE_PARAMEX_TS		stStoreParam;
} STREAM_RECV_STORE_PARAM_TS;

//Receive editor stream parameter
typedef struct StructStreamRecvEditorParam {
   MEDIA_TYPE_TE		    eMediaType;
   AUDIO_PARAM_TS		    stRecvAudioParam;
   VIDEO_PARAM_TS		    stRecvVideoParam;
   AUDIO_RECV_PARAM_TS	    stAudioRecvParam;
   VIDEO_RECV_PARAM_TS	    stVideoRecvParam;
   //AUDIO_SORT_PARAM_TS	stAudioSortParam;
   //VIDEO_SORT_PARAM_TS	stVideoSortParam;
   //AUDIO_JITTER_PARAM_TS	stAudioJitterParam;
   //VIDEO_JITTER_PARAM_TS	stVideoJitterParam;
   //AUDIO_DEC_PARAM_TS	stAudioDecParam;
   //VIDEO_DEC_PARAM_TS	stVideoDecParam;
   AUDIO_PARAM_TS		    stEditorAudioParam;
   VIDEO_PARAM_TS		    stEditorVideoParam;
   AUDIO_EDITOR_PARAM_TS   stAudioEditorParam;
   VIDEO_EDITOR_PARAM_TS   stVideoEditorParam;
} STREAM_RECV_EDITOR_PARAM_TS;

//All stream uadate parameter
typedef struct StructStreamUpdateParam {
   unsigned int            uiUpdateFlag;
   PVOID                   pUpdateParam;
} STREAM_UPDATE_PARAM_TS;

//----------------begin for test---------------------
//Read and write file process parameter
typedef struct StructFileReadParam {
   string	gstrFileName;
   int     iIntervalTime; //ms
   int     iReserved;
} FILE_READ_PARAM_TS;

typedef struct StructAudioFileWriteParam {
   string gstrFileName;
   int iReserved;
} AUDIO_FILE_WRITE_PARAM_TS;

typedef struct StructVideoFileWriteParam {
   string gstrFileName;
   int iReserved;
} VIDEO_FILE_WRITE_PARAM_TS;

//Recv write stream parameter
typedef struct StructStreamRecvWriteParam {
   MEDIA_TYPE_TE		      eMediaType;
   AUDIO_PARAM_TS		      stRecvAudioParam;
   VIDEO_PARAM_TS		      stRecvVideoParam;
   AUDIO_RECV_PARAM_TS	      stAudioRecvParam;
   VIDEO_RECV_PARAM_TS	      stVideoRecvParam;
   AUDIO_FILE_WRITE_PARAM_TS stAudioWriteParam;
   VIDEO_FILE_WRITE_PARAM_TS stVideoWriteParam;
} STREAM_RECV_WRITE_PARAM_TS;

//Read send stream parameter
typedef struct StructStreamReadSendParam {
   MEDIA_TYPE_TE		      eMediaType;
   FILE_READ_PARAM_TS        stFileReadParam;
   AUDIO_PARAM_TS		      stRecvAudioParam;
   VIDEO_PARAM_TS		      stRecvVideoParam;
   AUDIO_SEND_PARAM_TS	      stAudioRecvParam;
   VIDEO_SEND_PARAM_TS	      stVideoRecvParam;
} STREAM_READ_SEND_PARAM_TS;
//--------------end for test-----------------------

typedef struct StructStreamCaptureRenderParam {
   MEDIA_TYPE_TE		    eMediaType;
   AUDIO_PARAM_TS		    stCapAudioParam;
   VIDEO_PARAM_TS		    stCapVideoParam;
   AUDIO_CAPTURE_PARAM_TS	stAudioCaptureParam;
   VIDEO_CAPTURE_PARAM_TS	stVideoCaptureParam;
   VIDEO_RENDER_PARAM_TS   stVideoRenderParam;
} STREAM_CAPTURE_RENDER_PARAM_TS;

typedef struct StructPublishParam {
   string gstrFileName;
   int iReserved;
} PUBLISH_PARAM_TS;

typedef struct StructStreamCaptureRenderPublishParam {
   MEDIA_TYPE_TE		    eMediaType;
   AUDIO_PARAM_TS		    stCapAudioParam;
   VIDEO_PARAM_TS		    stCapVideoParam;
   AUDIO_CAPTURE_PARAM_TS	stAudioCaptureParam;
   VIDEO_CAPTURE_PARAM_TS	stVideoCaptureParam;
   VIDEO_RENDER_PARAM_TS   stVideoRenderParam;

   AUDIO_PARAM_TS			stPublishAudioParam;
   VIDEO_PARAM_TS			stPublishVideoParam;
   PUBLISH_PARAM_TS		stPublishParam;

} STREAM_CAPTURE_RENDER_PUBLISH_PARAM_TS;

typedef struct StructStreamCaptureAudioPublishParam {
   MEDIA_TYPE_TE		    eMediaType;
   AUDIO_PARAM_TS		    stCapAudioParam;
   AUDIO_CAPTURE_PARAM_TS	stAudioCaptureParam;

   AUDIO_PARAM_TS			stPublishAudioParam;

   PUBLISH_PARAM_TS		stPublishParam;

} STREAM_AUDIO_CAPTURE_PUBLISH_PARAM_TS;


\
#define  INPUT_AUDIO_DEVICE  0x0001
#define  INPUT_VIDEO_DEVICE  0x0002
#define  INPUT_AUDIO_FILE    0x0004
#define  INPUT_VIDEO_FILE    0x0008
#define  INPUT_VIDEO_SCREEN  0x0010   

#define  INPUT_TYPE_NONE     0
#define  INPUT_TYPE_FILE     1
#define  INPUT_TYPE_DEVICE   2
#define  INPUT_TYPE_SCREEN   3
#define  INPUT_TYPE_D3D      4

struct FileParam{
   __int64  startTimeStamp;
   __int64  endTimeStamp;
   char     filename[MAX_PATH];   
};
struct ScreenParam{
   int x;
   int y;
   int width;
   int height;
};
struct DeviceParam{
   int deviceId;
};

union InputParam{
   FileParam   fileParam;
   ScreenParam screenParam;
   DeviceParam deviceParam;
};

struct AudioVideoInputParam{
   int inputIndex;
   int audioInputType;
   InputParam audioParam;
   int videoInputType;
   InputParam videoParam;
   int  nextInputIndex;  //-1, will end
};
struct ScreenConfig {
   int bitRate;
   int x;
   int y;
   int width;
   int height;
   int framesPerSecond;
};
struct AvConfig {
   AudioConfig  audioConfig;
   VideoConfig  videoConfig;
};
struct AudioStats {
   int bufUsagePercentage;

   DWORD playHeadTimeInMs;
   DWORD dataLenInBufInMs;

   DWORD samplesGeneratedInMs;
   DWORD droppedDataLenInMs;
   DWORD compressedDataLenInMs;

   DWORD relativeWallClockInMs;
   /*
   * Equation:
   * playHeadTimeInMs + dataLenInBufInMs ==
   * samplesGeneratedInMs - droppedDataLenInMs - compressedDataLenInMs
   *
   * playHeadTimeInMs + dataLenInBufInMs <= relativeWallClockInMs
   * because there will be missing samples during sound capture.
   */

};

struct VideoStats {
   int bufUsagePercentage;
   DWORD playHeadTimeInMs;
   DWORD dataLenInBufInMs;

   DWORD droppedFramesInMs;
   DWORD droppedFramesForAudioSyncInMs;
   DWORD compressedDataLenInMs;
   DWORD totalFramesGeneratedInMs;
   DWORD relativeWallClockInMs;
};

struct MiscStats {
   size_t placeHolder; /* Don't have anything yet. */
};
//implement this function need  lock
typedef void (*NOTIFY_FME_UI)(int signalIntensity);  
typedef void (*NOTIFY_FME_STATUS)(int level,int code, const char* desc);

class IStatsCallback {
public:
   virtual void reportStats(AudioStats *audioStats, VideoStats *videoStats,
      MiscStats *miscStats) = 0;
};

class ICaptureCallback {
public:
   /*
   * After MediaEngine::Start() is called, use MediaEngine::GetVideoFrameSize()
   * to find out the size of the frame in RGB24 format.
   */
   virtual void OnNewVideoFrame(LPVOID frameData, size_t dataSize) = 0;
   virtual void OnNewAudioSample(LPVOID audioSample, size_t dataSize) = 0;
   virtual void OnYuvVideoFrame(LPVOID frameData, size_t dataSize) = 0;   
};

#define DATA_REC_EVENT_NAME   L"Local\\VHall_Recoder_Event_Name"
#define MEDIA_EXTERAL_EVENT_NAME   L"Local\\VHall_Exteral_Event_Name"

#define DATA_CONFIG_SHARED_MEM_NAME L"Local\\VHall_Config_Data_Mem"
#define DATA_Audio_SHARED_MEM_NAME L"Local\\VHall_Audio_Data_Mem"
#define DATA_Video_SHARED_MEM_NAME L"Local\\VHall_Video_Data_Mem"
#define DATA_Script_SHARED_MEM_NAME L"Local\\VHall_Script_Data_Mem"

#define DATA_CONFIG_MEM_LOCK_NAME L"VHall_Config_Data_Mem_Lock"
#define DATA_Audio_MEM_LOCK_NAME L"VHall_AudioData_Mem_Lock"
#define DATA_Video_MEM_LOCK_NAME L"VHall_Video_Data_Mem_Lock"
#define DATA_Script_MEM_LOCK_NAME L"VHall_Script_Data_Mem_Lock"

#define FILE_ID_LEN 256
struct Data_Config_Buff{
   AudioConfig  audioConfig;
   VideoConfig  videoConfig;  
   char         fileId[FILE_ID_LEN];   // the id of the file   
   DWORD        intervalInSec;
   bool         newFile; 
   bool         swapInit;  //
   bool         pause;
   bool         shutDown;
   DWORD        audioSwapSize;
   DWORD        videoSwapSize;
   DWORD        scriptSwapSize;
};
struct Unit_Buff{
   __int64 size;      
   int inUse;
   int  nextUnitIndex;
   BYTE* data;  //
};
struct Unit_Buff_Ref{
   int size;  
   int inUse;
   int  nextUnitIndex;
};
const int UnitBuffRefSize = sizeof(Unit_Buff_Ref);
//struct Unit_Buff{
//   int size;   
//   BYTE* data;
//   int  nextUnitIndex;
//   BYTE data[unitsize]
//};
struct Data_Swap_Buff_Ref{
   int unitCnt;  
   int uniTotalCnt;
   __int64 unitSize;
   int readIndex;
   int writeIndex;
   //Unit_Buff_Ref* units; // the first data point
};
const int DataSwapBuffRefSize = sizeof(Data_Swap_Buff_Ref);
struct Data_Swap_Buff{
   int unitCnt;    
   int uniTotalCnt;
   __int64 unitSize;
   int readIndex;
   int writeIndex;
   Unit_Buff* units; // the first data point
};

#define UnitBuff(x,index) (Unit_Buff*)(((BYTE*)x)+DataSwapBuffRefSize+(UnitBuffRefSize+x->unitSize)*index)
#define UnitData(x) (BYTE*)(((BYTE*)x)+UnitBuffRefSize)

#define SCRIPT_DATA_SIZE 2048

#define CONNECT_PUBLISH bool ret = false;  \
mDataConfigLock = CreateMutexW(NULL,FALSE,DATA_CONFIG_MEM_LOCK_NAME);\
if(mDataConfigLock == NULL){\
   gLogger->logError(" ConnectPublish CreateMutexW  mDataConfigLock failed."); \
   goto ConnectPublish_Failed;\
}\
Data_Config_Buff* configRef = NULL; \
mDataConfigMap = CreateFileMappingW(NULL, NULL, PAGE_READWRITE,NULL,((sizeof(Data_Config_Buff)+1024)/1024)*1024, DATA_CONFIG_SHARED_MEM_NAME);\
if (mDataConfigMap == NULL) {\
   gLogger->logError("ConnectPublish Could not open file mapping object, error = 0x%08x.\n",\
      GetLastError());\
   goto ConnectPublish_Failed;\
}\
if(GetLastError() != ERROR_ALREADY_EXISTS )\
newSrc = true;\
mDataConfigBuff = (Data_Config_Buff *)MapViewOfFile(mDataConfigMap, FILE_MAP_ALL_ACCESS, 0, 0, ((sizeof(Data_Config_Buff)+1024)/1024)*1024);\
if (mDataConfigBuff == NULL) {\
   gLogger->logError("ConnectPublish Could not map view of file, error = 0x%08x.\n",\
      GetLastError());\
   goto ConnectPublish_Failed;\
}  \
mDataEvent = CreateEventW(NULL,TRUE,FALSE,DATA_REC_EVENT_NAME);\
if(mDataEvent == NULL){\
   DWORD error = GetLastError();\
   gLogger->logError("ConnectPublish CreateEventW  failed, error = 0x%08x.",\
      error);\
   goto ConnectPublish_Failed;\
}  \
   if(newSrc==true){\
      memset(mDataConfigBuff, 0, sizeof (Data_Config_Buff));\
      mDataConfigBuff->newFile = false;\
      mDataConfigBuff->swapInit = false;\
   }\
   mConnected = true;\
return true;\
ConnectPublish_Failed:\
DisconnectPublish();\
return false;


// for the error notify

#define  CODE_ERROR_AUDIO_DEVICE  -1
#define  CODE_ERROR_VIDEO_DEVICE  -2
#define  CODE_ERROR_FILE          -3
#define  CODE_ERROR_NETWORK       -4
#endif //_INC_MP_MEDIA_PARAM_H

