
#ifndef __H_CGdiPainter__
#define __H_CGdiPainter__

//#include "CAVPlayer.h"
#include <Windows.h>
#include <Vfw.h>
//#include <atlbase.h>
//#include <atlapp.h>
//#include <atlwin.h>
//#include <atlwinx.h>
//#include <atlmisc.h>
#include "mediaparam.h"

#define LPDATA_NUM    10

typedef struct RENDER_BUFFER_QUEUE_TS RENDER_BUFFER_QUEUE;
struct RENDER_BUFFER_QUEUE_TS
{
   LPWAVEHDR    head;
   LPWAVEHDR    tail;
   int          num;
};
struct RENDER_AUDIO_BUFFER_POOL
{
   RENDER_BUFFER_QUEUE* p_aRenderQueue;
   HANDLE               mutex;
   int                  iObjectNum;
};


/*
 1.render the video 
 2.show the audio meter
*/
void CALLBACK waveOutProc( HWAVEOUT hwo, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2 );
class CAudioMeter;
#ifndef MP4IMP_LIB
class CMediaImpl;
#endif
class CAVPlayer
{

public:
	CAVPlayer();
	~CAVPlayer(); 
	void SetVideoWindow(HWND inWindow, int width, int height);	
   void SetAudioOpt(WORD wBitsPerSample = 16,int channels = 1,int sampleRate = 16000, int lengthInMs = 20 );
   
   virtual void OnNewVideoFrame(LPVOID frameData, size_t dataSize);

   virtual void OnYuvVideoFrame(LPVOID frameData, size_t dataSize);
  
   void OnScriptData(LPVOID audioSample, size_t dataSize);
   virtual void OnNewAudioSample(LPVOID audioSample, size_t dataSize);
	bool OpenVideoRender(void);   
	bool RenderVideo(BYTE * inData, DWORD inLength, unsigned __int64  inSampleTime);
	bool CloseVideoRender();
   void MuteAudio(bool mute);
   bool IsAudioMute();
   bool OpenAudioPlayer(bool waveOut);
   void PlayerAudio(LPBYTE audioData, DWORD dataSize);
   void CloseAudioPlayer();
   void WaitAudioShutdown();
#ifndef MP4IMP_LIB
   void SetMediaExteral(CMediaImpl* mediaExteral);
   void SetAudioMeter(CAudioMeter* audioMeter);
#endif
	
	bool IsPlay();
   BOOL ReDraw();
   DWORD GetAudioUnitSize();
   bool IsAudioPlay();
   void SetLayerText( const WCHAR* str);
private:
   int initBufferPool( RENDER_AUDIO_BUFFER_POOL* bufferPool ,int dataSize );
   int destroyBufferPool( RENDER_AUDIO_BUFFER_POOL* bufferPool );
   LPWAVEHDR getBuffer( RENDER_AUDIO_BUFFER_POOL* bufferPool );
   int putBuffer( RENDER_AUDIO_BUFFER_POOL* bufferPool, LPWAVEHDR bufferNode );
		
private:
	CRITICAL_SECTION			mPreLockCS;	
	bool						   mVideoStop;	
   bool						   mAudioStop;	
   BYTE*                   mVideoBuff;
   size_t                  mVideoBuffSize;
   CAudioMeter*            mAudioMeter;
#ifndef MP4IMP_LIB
   CMediaImpl*             mMediaExteral;
#endif
   bool                    mPlayAudioMute;
   
   BITMAPINFOHEADER	mBmpHeader;	
   HWND			mVideoWnd;
   HDC			mWindowDC;
   HDC         mMemDC;
   RECT			mTargetRect;
   RECT			mSourceRect;
   HDRAWDIB    mSrcDib;
   HDRAWDIB    mDstDib;
   LPVOID     mCurFrameData;
   CRITICAL_SECTION   mVideoBufferDataCs;

   int           mChannels;
   int           mSampleBits;
   int           mSampleRate;
   int           mLengthInMs;
  
   HWAVEOUT      mWaveOut;
   WAVEFORMATEX  mWaveForm;
   MMRESULT      mResult;	
   bool           mAudioPlay;
   RENDER_AUDIO_BUFFER_POOL* mBufferPool;  
   LPWAVEHDR     mBuffer[ LPDATA_NUM ];
   int          mInitFlag;
	CRITICAL_SECTION   mAudioDeviceCs;
   friend void CALLBACK waveOutProc( HWAVEOUT hwo, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2 );

   HBITMAP			mcurMembmp;
   WCHAR*         mLayerText;
};

#endif // __H_CGdiPainter__
