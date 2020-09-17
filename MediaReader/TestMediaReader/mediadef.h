#ifndef _MEDIA_DEF_INCLUDE_H__
#define _MEDIA_DEF_INCLUDE_H__
#include "scriptlist.h"


typedef void (*__DOING_CALLBACK)(int progress, const char* desc,void* data);
typedef void (*__ERROR_CALLBACK)(int code, const char* desc,void* data);
typedef void (*__DONE_CALLBACK)(void* data);

//#define MAX_FILE_NAME 256

#define METHOD_MEDIA_TOOL_CUT    1
#define METHOD_MEDIA_TOOL_MERGE  2
#define METHOD_MEDIA_TOOL_CONVERT  3
#define METHOD_MEDIA_TOOL_GENERATE 4

#define CMD_NOTIFY_WMF			20002
#define CMD_CODE_POS_UPDATE		1
#define CMD_CODE_PROCESS_UPDATE	2
#define CMD_CODE_PROCESS_DONE	   3
#define CMD_CODE_STATE_UPDATE	   4
#define  ID_TASK_START		1000
#define  ID_TASK_PROGRESS	CMD_NOTIFY_WMF
#define  ID_TASK_DONE		1002

#define ID_CMD_STATE    30000
#define DOING_CMD_CODE  30001
#define ERROR_CMD_CODE  30002
#define DONE_CMD_CODE   30003

#define  OP_TYPE_AUIDO_AND_DOCUMENT    1
#define  OP_TYPE_AUIDO_AND_DOC_VIDEO   2
#define  OP_TYPE_FILE_CONVERT          3

struct MediaFileInfo{
   __int64        audioDuration;
   unsigned int   audioSampleRate;
   unsigned int   audioSampleSize;
   unsigned int   audioChannelCount;      
   __int64        videoDuration;
   unsigned short videoWidth;
   unsigned short videoHeight;

   float          videoFrameRate;

   char           videoCompressorName[256];
   //for AVC
   BYTE           configurationVersion;
   BYTE           level;
   BYTE           profile;
   TScriptList*   scriptList;
};



struct CutMediaParam{
   char inputFileName[MAX_FILE_NAME];
 /*  unsigned __int64 cutBeginTime;
   unsigned __int64 cutEndTime;*/
   char outputFileName[MAX_FILE_NAME];  
   TScriptList* scriptList;
   TMarkList*   markList;
};

struct MergeMediaParam{
   char** inputFileNames;  
   int   inputCnt;
   char  outputFileName[MAX_FILE_NAME];  
};

struct ConvertMediaParam{
   char inputFileName[MAX_FILE_NAME];  
   char outputFileName[MAX_FILE_NAME];  
   HWND previewWnd; 
};

union ToolParam{
   CutMediaParam     cutParam;   
   MergeMediaParam   mergeParam;
   ConvertMediaParam convertParam;    
};


typedef struct _Media_Tool_Param{
   int               method;
   ToolParam         param;
   __DOING_CALLBACK  doingCallbackProc;
   __ERROR_CALLBACK  errorCallbackProc;
   __DONE_CALLBACK   doneCallbackProc;
   bool              isOutVpj;
   char              vpjFileName[MAX_FILE_NAME];  
   void*             obj;
}SMediaToolParam;
const UINT SMediaToolParamSize = sizeof(SMediaToolParam);

/*export the file reader*/
class CReader{
public:
   virtual int wmfOpen(LPCWSTR pwszUrl,HWND hPreviewWnd) = 0;
   virtual int wmfPause() = 0;
   virtual int wmfResume() = 0;  
   virtual int wmfClose() = 0;
   virtual int wmfPlay() = 0;
   virtual int wmfStop() = 0; 
   virtual int wmfSeek(const unsigned __int64& qwTime) = 0; 
   virtual int wmfSyncSeek(const unsigned __int64& qwTime) = 0;
   virtual int wmfGetPlayerState() = 0;
public:  
   virtual unsigned __int64	GetFileDuration(void) = 0;  
   virtual unsigned __int64	GetTimeElapsed(void) = 0;  
   virtual TScriptList*GetScriptList() = 0; 
   virtual void SetMsgWnd(const HWND& hWnd	)     = 0;
   virtual unsigned __int64	GetFileDuration(const char* file) = 0;  
   virtual bool ReDraw() = 0;
};

/*export the file writer*/
class CWriter
{
public:  
   virtual bool Init() = 0;
   virtual bool OutputMp42( ) = 0;
   virtual void ShutDown() = 0;
   virtual void Destory() = 0;    
};
#endif