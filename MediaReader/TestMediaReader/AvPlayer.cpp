

//#include "stdafx.h"
#include <stdio.h>
//#include "safe_def.h"
#include "AvPlayer.h"
#include "Logging.h"
//#include <atlbase.h>
//#include <atlapp.h>
//#include <atlgdi.h>
#include <time.h>
#include <windows.h>

#include <GdiPlus.h>
//#ifdef _DEBUG
//#define new DEBUG_NEW
//#undef THIS_FILE
//static char THIS_FILE[] = __FILE__;
//#endif
#define MP4IMP_LIB
#pragma comment(lib,"ws2_32.lib")
#ifndef MP4IMP_LIB
#include "AudioMeter.h"
#include "MediaImpl.h"
#endif

//int __cdecl usleep(__int64 useconds)
//{
//	__int64 time1 = 0, time2 = 0, sysFreq = 0;
//
//	QueryPerformanceCounter((LARGE_INTEGER *) &time1);
//	QueryPerformanceFrequency((LARGE_INTEGER *)&sysFreq );
//
//	do {
//		QueryPerformanceCounter((LARGE_INTEGER *) &time2);
//	} while((time2-time1) < useconds);
//}
#pragma comment(lib,"winmm")

BOOL win32_grab_bitmap2dib(HDC hdc, HBITMAP hbitmap, UINT bits,
                           LPBITMAPINFOHEADER *ppBitInfo, LPVOID *ppData) {
   BITMAP              bitmap;
   UINT                wLineLen;
   DWORD               wColSize;
   DWORD               dwSize;
   LPBITMAPINFOHEADER  lpbi;
   LPBYTE              lpBits;
   int                  ret = 0;
   ret = GetObject(hbitmap, sizeof(BITMAP), &bitmap);

   //
   // DWORD align the width of the DIB
   // Figure out the size of the colour table
   // Calculate the size of the DIB
   //
   wLineLen = (bitmap.bmWidth*bits + 31) / 32 * 4;
   wColSize = sizeof(RGBQUAD)*((bits <= 8) ? 1 << bits : 0);
   dwSize = sizeof(BITMAPINFOHEADER)+wColSize +
      (DWORD)(UINT)wLineLen*(DWORD)(UINT)bitmap.bmHeight;

   //
   // Allocate room for a DIB and set the LPBI fields
   //
   if (*ppBitInfo) {
      lpbi = *ppBitInfo;
   } else {
      lpbi = (LPBITMAPINFOHEADER)HeapAlloc(GetProcessHeap(), 0, dwSize);
   }
   if (!lpbi)
      return FALSE;

   lpbi->biSize = sizeof(BITMAPINFOHEADER);
   lpbi->biWidth = bitmap.bmWidth;
   lpbi->biHeight = bitmap.bmHeight;
   lpbi->biPlanes = 1;
   lpbi->biBitCount = (WORD)bits;
   lpbi->biCompression = BI_RGB;
   lpbi->biSizeImage = dwSize - sizeof(BITMAPINFOHEADER)-wColSize;
   lpbi->biXPelsPerMeter = 0;
   lpbi->biYPelsPerMeter = 0;
   lpbi->biClrUsed = (bits <= 8) ? 1 << bits : 0;
   lpbi->biClrImportant = 0;
   lpBits = (LPBYTE)lpbi;
   lpBits += lpbi->biSize + wColSize;

   GetDIBits(hdc, hbitmap, 0, bitmap.bmHeight, lpBits, (LPBITMAPINFO)lpbi, DIB_RGB_COLORS);
   lpbi->biClrUsed = (bits <= 8) ? 1 << bits : 0;

   *ppBitInfo = lpbi;
   *ppData = lpBits;

   return TRUE;
}

int CAVPlayer::initBufferPool(RENDER_AUDIO_BUFFER_POOL* bufferPool, int dataSize) {
   if (mInitFlag == 1) {
      WaitForSingleObject(bufferPool->mutex, INFINITE);
      bufferPool->iObjectNum++;
      ReleaseMutex(bufferPool->mutex);
      return 0;
   }
   int i = 0;

   //	bufferPool                 = ( RENDER_AUDIO_BUFFER_POOL* )malloc( sizeof( RENDER_AUDIO_BUFFER_POOL ) );
   bufferPool->p_aRenderQueue = (RENDER_BUFFER_QUEUE*)malloc(sizeof(RENDER_BUFFER_QUEUE));
   bufferPool->mutex = CreateMutex(NULL, FALSE, NULL);
   bufferPool->iObjectNum = 1;

   bufferPool->p_aRenderQueue->head = NULL;
   bufferPool->p_aRenderQueue->tail = NULL;
   bufferPool->p_aRenderQueue->num = 0;

   for (i = 0; i < LPDATA_NUM; i++) {
      mBuffer[i] = (LPWAVEHDR)malloc(sizeof(WAVEHDR));
      mBuffer[i]->lpData = (char*)malloc(dataSize);

      if (mBuffer[i]->lpData == NULL) {
         return -1;
      }

      mBuffer[i]->dwBufferLength = dataSize;
      mBuffer[i]->lpNext = NULL;

      if (bufferPool->p_aRenderQueue->num == 0) {
         bufferPool->p_aRenderQueue->head = mBuffer[i];
         bufferPool->p_aRenderQueue->tail = mBuffer[i];
         bufferPool->p_aRenderQueue->num = 1;
      } else {
         bufferPool->p_aRenderQueue->tail->lpNext = mBuffer[i];
         bufferPool->p_aRenderQueue->tail = mBuffer[i];
         bufferPool->p_aRenderQueue->num++;
      }
   }
   mInitFlag = 1;
   return 0;
}

int CAVPlayer::destroyBufferPool(RENDER_AUDIO_BUFFER_POOL* bufferPool) {
   WaitForSingleObject(bufferPool->mutex, INFINITE);
   bufferPool->iObjectNum--;
   ReleaseMutex(bufferPool->mutex);

   int i = 0;
   if (bufferPool->iObjectNum <= 0) {
      for (i = 0; i < LPDATA_NUM; i++) {
         if (mBuffer[i] != NULL) {
            free(mBuffer[i]->lpData);
            mBuffer[i]->lpData = NULL;
            free(mBuffer[i]);
            mBuffer[i] = NULL;
         }
      }

      if (bufferPool->p_aRenderQueue != NULL) {
         free(bufferPool->p_aRenderQueue);
         bufferPool->p_aRenderQueue = NULL;
      }
      CloseHandle(bufferPool->mutex);

      //free( bufferPool );
      delete bufferPool;
      bufferPool = NULL;
   }
   mInitFlag = 0;
   return 0;
}

LPWAVEHDR CAVPlayer::getBuffer(RENDER_AUDIO_BUFFER_POOL* bufferPool) {
   EnterCriticalSection(&mAudioDeviceCs);
   if (bufferPool == NULL) {
      LeaveCriticalSection(&mAudioDeviceCs);
      return NULL;
   }

   LPWAVEHDR tempBuffer;

   WaitForSingleObject(bufferPool->mutex, INFINITE);

   if (bufferPool->p_aRenderQueue->num == 0) {
      //there is no free buffer
      ReleaseMutex(bufferPool->mutex);
      LeaveCriticalSection(&mAudioDeviceCs);
      return NULL;
   } else {
      //delete the return node from the free queue 
      tempBuffer = bufferPool->p_aRenderQueue->head;
      bufferPool->p_aRenderQueue->head = tempBuffer->lpNext;
      bufferPool->p_aRenderQueue->num--;
   }
   ReleaseMutex(bufferPool->mutex);
   LeaveCriticalSection(&mAudioDeviceCs);
   return tempBuffer;
}

int CAVPlayer::putBuffer(RENDER_AUDIO_BUFFER_POOL* bufferPool, LPWAVEHDR bufferNode) {
   if (bufferPool == NULL || bufferNode == NULL) {
      return -1;
   }
   EnterCriticalSection(&mAudioDeviceCs);
   WaitForSingleObject(bufferPool->mutex, INFINITE);
   if (bufferPool->p_aRenderQueue->num == 0) {
      //the data buffer queue is empty
      bufferPool->p_aRenderQueue->head = bufferNode;
      bufferPool->p_aRenderQueue->tail = bufferNode;
      bufferPool->p_aRenderQueue->num = 1;
   } else {
      //add one node for the data buffer queue
      bufferPool->p_aRenderQueue->tail->lpNext = bufferNode;
      bufferPool->p_aRenderQueue->tail = bufferNode;
      bufferPool->p_aRenderQueue->num++;
   }
   ReleaseMutex(bufferPool->mutex);
   LeaveCriticalSection(&mAudioDeviceCs);
   return 0;
}




/////////////////////////////////////////////////////////////////////////////
CAVPlayer::CAVPlayer() :
mAudioPlay(false),
mSampleBits(0),
mSampleRate(0),
mChannels(1),
mBufferPool(NULL),
mPlayAudioMute(true),
mWaveOut(NULL),
mcurMembmp(NULL),
mCurFrameData(NULL) {
   mVideoWnd = NULL;
   mWindowDC = NULL;
   SetRectEmpty(&mTargetRect);
   SetRectEmpty(&mSourceRect);
   InitializeCriticalSection(&mPreLockCS);
   InitializeCriticalSection(&mAudioDeviceCs);

   InitializeCriticalSection(&mVideoBufferDataCs);

   mVideoStop = true;
   mAudioStop = true;
   /*initialize the bmp size*/
   ZeroMemory(&mBmpHeader, sizeof(mBmpHeader));
   mBmpHeader.biSize = sizeof(mBmpHeader);
   mBmpHeader.biWidth = 320;
   mBmpHeader.biHeight = 240;
   mBmpHeader.biPlanes = 1;
   mBmpHeader.biBitCount = 32;
   mBmpHeader.biCompression = BI_RGB;
   mBmpHeader.biSizeImage = mBmpHeader.biWidth*mBmpHeader.biHeight*mBmpHeader.biBitCount / 8;

   mVideoBuff = NULL;
   mVideoBuffSize = 0;
   mMemDC = 0;
   mSrcDib = 0;
   mDstDib = 0;

   mLayerText = NULL;
#ifndef MP4IMP_LIB
   mMediaExteral              = NULL;
   mAudioMeter                = NULL;
#endif
}

CAVPlayer::~CAVPlayer() {
   if (mMemDC) {
      DeleteDC(mMemDC);
      mMemDC = NULL;
   }
   if (mWindowDC) {
      ::ReleaseDC(mVideoWnd, mWindowDC);
      mWindowDC = 0;
   }
   CloseVideoRender();
   DeleteCriticalSection(&mPreLockCS);
   if (mBufferPool)
      destroyBufferPool(mBufferPool);
   mBufferPool = NULL;
   DeleteCriticalSection(&mAudioDeviceCs);
   DeleteCriticalSection(&mVideoBufferDataCs);

}

void CAVPlayer::OnNewVideoFrame(LPVOID frameData, size_t dataSize) {
   if (frameData) {
      if (dataSize > mVideoBuffSize) {
         if (mVideoBuff) free(mVideoBuff);
         mVideoBuff = (BYTE*)malloc(dataSize);
         mVideoBuffSize = dataSize;
      }
      memcpy(mVideoBuff, (BYTE*)frameData, dataSize);
      RenderVideo(mVideoBuff, dataSize, 0);
   }
}

void CAVPlayer::OnYuvVideoFrame(LPVOID frameData, size_t dataSize) {
#ifndef MP4IMP_LIB
   if(mMediaExteral){
      mMediaExteral->WriteVideoFrame((LPBYTE)frameData, dataSize, 0);
   }
#endif
}

void CAVPlayer::OnNewAudioSample(LPVOID audioSample, size_t dataSize) {
   if (mAudioPlay) {
      PlayerAudio((LPBYTE)audioSample, dataSize);
   }
#ifndef MP4IMP_LIB
   if (mAudioMeter)   {
      mAudioMeter->UpdateData((LPBYTE)audioSample, dataSize);
   }
   if(mMediaExteral){
      mMediaExteral->WriteAudioUnit((LPBYTE)audioSample, dataSize, 0);
   }
#endif
}
#ifndef MP4IMP_LIB
void CAVPlayer::OnScriptData(LPVOID data, size_t dataSize)
{
   if(mMediaExteral){
      mMediaExteral->WriteScript((LPBYTE)data, dataSize, 0);
   }
}
#endif
void CAVPlayer::SetVideoWindow(HWND inWindow, int width, int height) {
   if (mWindowDC && mVideoWnd != inWindow) {
      //::ReleaseDC(mVideoWnd,mWindowDC);
      CloseVideoRender();
   }
   mVideoWnd = inWindow;
   // Get attributes of video window
   mWindowDC = ::GetDC(mVideoWnd);
   mMemDC = CreateCompatibleDC(mWindowDC);
   ::GetClientRect(mVideoWnd, &mTargetRect);
   mcurMembmp = CreateCompatibleBitmap(mWindowDC, width, height);

   mSourceRect.left = 0;
   mSourceRect.top = 0;
   mSourceRect.right = width;
   mSourceRect.bottom = height;

   mBmpHeader.biWidth = width;
   mBmpHeader.biHeight = height;
   mBmpHeader.biSizeImage = mBmpHeader.biWidth*mBmpHeader.biHeight*mBmpHeader.biBitCount / 8;
   mCurFrameData = malloc(width*height * 3);
}

void CAVPlayer::SetAudioOpt(WORD wBitsPerSample, int channels, int sampleRate, int lengthInMs) {
   if (mSampleBits != wBitsPerSample || mChannels != channels || mSampleRate != sampleRate || mLengthInMs != lengthInMs) {
      if (mBufferPool) {
         destroyBufferPool(mBufferPool);
      }

      mSampleBits = wBitsPerSample;
      mSampleRate = sampleRate;
      mChannels = channels;
      mLengthInMs = lengthInMs;
      mBufferPool = new RENDER_AUDIO_BUFFER_POOL;
      initBufferPool(mBufferPool, mSampleBits*mSampleRate*mChannels / 8 * mLengthInMs / 1000);
   }
#ifndef MP4IMP_LIB
   if(mAudioMeter)   {
      mAudioMeter->SetAudioOpt(wBitsPerSample, channels);
   }
#endif
}
#ifndef MP4IMP_LIB
void CAVPlayer::SetAudioMeter(CAudioMeter* audioMeter)
{
   mAudioMeter = audioMeter;
}
#endif


bool CAVPlayer::OpenVideoRender(void) {
   // Calculate the overall rectangle dimensions
   BOOL ret = FALSE;
   LONG sourceWidth = mSourceRect.right - mSourceRect.left;
   LONG targetWidth = mTargetRect.right - mTargetRect.left;
   LONG sourceHeight = mSourceRect.bottom - mSourceRect.top;
   LONG targetHeight = mTargetRect.bottom - mTargetRect.top;
   mVideoStop = false;
   mSrcDib = DrawDibOpen();
   if (mSrcDib != NULL) {
      ret = DrawDibBegin(mSrcDib,
                         mMemDC,
                         sourceHeight,
                         sourceHeight,
                         &mBmpHeader,
                         mBmpHeader.biWidth,
                         mBmpHeader.biHeight,
                         DDF_JUSTDRAWIT
                         );
      if (ret == FALSE) {
         return false;
      }
   } else {
      return false;
   }

   mDstDib = DrawDibOpen();
   if (mDstDib != NULL) {
      ret = DrawDibBegin(mDstDib,
                         mWindowDC,
                         targetWidth,
                         targetHeight,
                         &mBmpHeader,
                         mBmpHeader.biWidth,
                         mBmpHeader.biHeight,
                         DDF_JUSTDRAWIT
                         );
      if (ret == FALSE) {
         return false;
      }
   } else {
      return false;
   }
   return true;
}
BOOL win32_grab_bitmap2dib(HDC hdc, HBITMAP hbitmap, UINT bits,
                           LPBITMAPINFOHEADER *ppBitInfo, LPVOID *ppData);
bool CAVPlayer::RenderVideo(BYTE * inData, DWORD inLength, unsigned __int64 inSampleTime) {
   if (mVideoStop == true) return false;
   RECT crc;
   ::GetClientRect(mVideoWnd, &mTargetRect);
   HBITMAP oldbmp = (HBITMAP)SelectObject(mMemDC, mcurMembmp);
   if (1) {
      DrawDibDraw(
         mSrcDib,
         mMemDC,
         0,		// dest : left pos
         0,		// dest : top pos
         mSourceRect.right - mSourceRect.left,
         mSourceRect.bottom - mSourceRect.top,
         &mBmpHeader,			 // bmp header info
         (BYTE*)inData,  // bmp data
         0,					 // src :left
         0,					 // src :top
         mSourceRect.right - mSourceRect.left,		// src : width
         mSourceRect.bottom - mSourceRect.top,		// src : height
         DDF_SAME_DRAW			 // use prev params....
         );
   } else { //for test     
      //mcurMembmp = CreateCompatibleBitmap(mWindowDC, mTargetRect.right-mTargetRect.left, mTargetRect.bottom-mTargetRect.top);


      if (FALSE) {
         // Put the image straight into the window
         SetDIBitsToDevice(
            (HDC)mMemDC,                            // Target device HDC
            mTargetRect.left,                       // X sink position
            mTargetRect.top,                        // Y sink position
            mTargetRect.right - mTargetRect.left,                            // Destination width
            mTargetRect.bottom - mTargetRect.top,                           // Destination height
            mSourceRect.left,                       // X source position
            mSourceRect.top,                        // Y source position
            (UINT)0,                               // Start scan line
            mBmpHeader.biHeight,         // Scan lines present
            inData,                                 // Image data
            (BITMAPINFO *)&mBmpHeader,  // DIB header
            DIB_RGB_COLORS);                        // Type of palette
      } else {
         // Stretch the image when copying to the window
         StretchDIBits(
            (HDC)mMemDC,                             // Target device HDC
            mTargetRect.left,                        // X sink position
            mTargetRect.top,                         // Y sink position
            mTargetRect.right - mTargetRect.left,       // Destination width
            mTargetRect.bottom - mTargetRect.top,        // Destination height
            mSourceRect.left,                        // X source position
            mSourceRect.top,                         // Y source position
            mSourceRect.right - mSourceRect.left,    // Source width
            mSourceRect.bottom - mSourceRect.top,    // Source height
            inData,                                  // Image data
            (BITMAPINFO *)&mBmpHeader,   // DIB header
            DIB_RGB_COLORS,                          // Type of palette
            SRCCOPY);                                // Simple image copy

      }
      if (mLayerText) {
         SetBkMode(mMemDC, TRANSPARENT);
         RECT	titleRect = mTargetRect;
         DrawTextW(mMemDC, mLayerText, -1, &titleRect, DT_LEFT);
      }

      BitBlt(mWindowDC, 0, 0, mTargetRect.right - mTargetRect.left, mTargetRect.bottom - mTargetRect.top, mMemDC, 0, 0, SRCCOPY);
      SelectObject(mMemDC, oldbmp);
   }

   if (mLayerText) {
      SetBkMode(mMemDC, TRANSPARENT);
      RECT	titleRect = mTargetRect;
      DrawTextW(mMemDC, mLayerText, -1, &titleRect, DT_LEFT);
   }

   StretchBlt(mWindowDC, 0, 0,mTargetRect.right-mTargetRect.left, mTargetRect.bottom-mTargetRect.top,  mMemDC, 0, 0,mSourceRect.right-mSourceRect.left, mSourceRect.bottom-mSourceRect.top, SRCCOPY);
   SelectObject(mMemDC, oldbmp);

   //get the data
   //EnterCriticalSection(&mVideoBufferDataCs);

   //LPBITMAPINFOHEADER lppi = NULL;
   //size_t frameDataSize = mBmpHeader.biWidth*mBmpHeader.biHeight*mBmpHeader.biBitCount / 8;
   //LPVOID ppData = NULL;
   ////testBmp(mVideoBmp);
   //win32_grab_bitmap2dib(mMemDC, mcurMembmp, mBmpHeader.biBitCount, &lppi, &ppData);
   //memcpy(mCurFrameData, ppData, frameDataSize);
   //if (lppi)
   //   HeapFree(GetProcessHeap(), 0, lppi);
   //BOOL bRet = DrawDibDraw(mDstDib,
   //                        mWindowDC,
   //                        0,		// dest : left pos
   //                        0,		// dest : top pos
   //                        mTargetRect.right - mTargetRect.left,                            // Destination width
   //                        mTargetRect.bottom - mTargetRect.top,

   //                        &mBmpHeader,				 // bmp header info
   //                        (BYTE*)mCurFrameData,  // bmp data
   //                        0,					 // src :left
   //                        0,					 // src :top
   //                        mSourceRect.right - mSourceRect.left,		// src : width
   //                        mSourceRect.bottom - mSourceRect.top,		// src : height      
   //                        DDF_SAME_DRAW			 // use prev params....
   //                        );
   //// CDCHandle dc(mWindowDC);  


   //LeaveCriticalSection(&mVideoBufferDataCs);



   return true;
}
bool CAVPlayer::CloseVideoRender() {
   gLogger->logInfo("CAVPlayer::CloseVideoRender will close video render");
   bool bRet = false;
   EnterCriticalSection(&mPreLockCS);
   mVideoStop = true;
   LeaveCriticalSection(&mPreLockCS);
   DrawDibEnd(mSrcDib);
   DrawDibClose(mSrcDib);

   if (mcurMembmp) {
      DeleteObject(mcurMembmp);
      mcurMembmp = NULL;
   }

   if (mMemDC) {
      DeleteDC(mMemDC);
      mMemDC = NULL;
   }
   if (mWindowDC) {
      ::ReleaseDC(mVideoWnd, mWindowDC);
   }
   mSrcDib = NULL;

   return bRet;
}

void CAVPlayer::MuteAudio(bool mute) {
   mPlayAudioMute = mute;
}

bool CAVPlayer::IsAudioMute() {
   return mPlayAudioMute;
}

void CALLBACK waveOutProc(HWAVEOUT hwo, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2) {
   CAVPlayer* obj = (CAVPlayer*)dwInstance;
   // EnterCriticalSection(&obj->mAudioDeviceCs);
   if (!obj->IsAudioPlay()) {
      return;
   }
   LPWAVEHDR lpHdr;
   switch (uMsg) {
   case WOM_OPEN:
      break;
   case WOM_DONE:
      lpHdr = (LPWAVEHDR)dwParam1;
      //waveOutUnprepareHeader(hwo,lpHdr,sizeof(WAVEHDR) );      
      lpHdr->dwUser = 1;
      obj->putBuffer(obj->mBufferPool, lpHdr);
      break;
   case WOM_CLOSE:
      break;
   default:
      break;
   }
   // LeaveCriticalSection(&obj->mAudioDeviceCs);
}


bool CAVPlayer::OpenAudioPlayer(bool waveOut) {
   if (waveOut) {
      mWaveForm.wFormatTag = WAVE_FORMAT_PCM;
      mWaveForm.nChannels = mChannels;
      mWaveForm.nSamplesPerSec = mSampleRate;
      mWaveForm.wBitsPerSample = mSampleBits;
      mWaveForm.nAvgBytesPerSec = (mSampleBits*mSampleRate*mChannels) / 8;
      mWaveForm.nBlockAlign = 4;
      mWaveForm.cbSize = 0;

      mResult = waveOutOpen(&mWaveOut, WAVE_MAPPER, &mWaveForm, (DWORD_PTR)waveOutProc, (DWORD_PTR)this, CALLBACK_FUNCTION);

      if (mResult != MMSYSERR_NOERROR)
         return false;

      DWORD volume = 0xffffffff;
      waveOutSetVolume(mWaveOut, volume);
      mAudioPlay = true;
      mAudioStop = false;
   } else {
      mAudioPlay = false;
      mAudioStop = true;
   }
   return true;
}
void CAVPlayer::PlayerAudio(LPBYTE audioData, DWORD dataSize) {
   int iRry = 0;
   while (mAudioStop == false) {
      LPWAVEHDR     lpHdr;
      lpHdr = getBuffer(mBufferPool);
      if (lpHdr == NULL) {
         Sleep(/*mLengthInMs*LPDATA_NUM/2*/100);
         iRry++;
         if (iRry > 1)
            break;
         else
            continue;
      }
      if (mPlayAudioMute) {
         memset(lpHdr->lpData, 0, dataSize);
      } else {
         memcpy(lpHdr->lpData, audioData, dataSize);
      }
      if (lpHdr->dwUser == 1) {
         waveOutUnprepareHeader(mWaveOut, lpHdr, sizeof(WAVEHDR));
      }
      lpHdr->dwBufferLength = dataSize;
      lpHdr->dwBytesRecorded = 0;
      lpHdr->dwUser = 0;
      lpHdr->dwFlags = 0;
      lpHdr->dwLoops = 1;
      lpHdr->lpNext = NULL;
      lpHdr->reserved = 0;
      //EnterCriticalSection(&mAudioDeviceCs);
      waveOutPrepareHeader(mWaveOut, lpHdr, sizeof(WAVEHDR));
      waveOutWrite(mWaveOut, lpHdr, sizeof(WAVEHDR));
      //LeaveCriticalSection(&mAudioDeviceCs);
      break;
   }
}
void  CAVPlayer::CloseAudioPlayer() {
   gLogger->logInfo("CAVPlayer::CloseAudioPlayer will close audio player");
   mAudioStop = true;
}
void CAVPlayer::WaitAudioShutdown() {
   if (mWaveOut) {
      EnterCriticalSection(&mAudioDeviceCs);
      Sleep(100);
      waveOutReset(mWaveOut);
      Sleep(100);
      for (int i = 0; i < LPDATA_NUM; i++) {
         if (mBuffer[i] != NULL) {
            if (mBuffer[i]->dwUser == 1) {
               waveOutUnprepareHeader(mWaveOut, mBuffer[i], sizeof(WAVEHDR));
            }
            mBuffer[i]->dwUser = 0;
         }
      }
      waveOutClose(mWaveOut);
      // CloseHandle(mWaveOut);
      mWaveOut = NULL;
      LeaveCriticalSection(&mAudioDeviceCs);
      EnterCriticalSection(&mPreLockCS);
#ifndef MP4IMP_LIB
      if(mAudioMeter){
         mAudioMeter->Stop();
         mAudioMeter = NULL;
      }
#endif
      LeaveCriticalSection(&mPreLockCS);
   }
}
#ifndef MP4IMP_LIB
void CAVPlayer::SetMediaExteral(CMediaImpl* mediaExteral)
{
   mMediaExteral = mediaExteral;
}
#endif

bool CAVPlayer::IsPlay() {
   bool bRet = false;
   EnterCriticalSection(&mPreLockCS);
   bRet = mVideoStop == false;
   LeaveCriticalSection(&mPreLockCS);
   return bRet;
}
void testBmp(HBITMAP hBmp);
BOOL CAVPlayer::ReDraw() {
   ::GetClientRect(mVideoWnd, &mTargetRect);
   EnterCriticalSection(&mVideoBufferDataCs);
   if (mCurFrameData) {
      BOOL bRet = DrawDibDraw(mDstDib,
                              mWindowDC,
                              0,		// dest : left pos
                              0,		// dest : top pos
                              mTargetRect.right - mTargetRect.left,                            // Destination width
                              mTargetRect.bottom - mTargetRect.top,

                              &mBmpHeader,				 // bmp header info
                              (BYTE*)mCurFrameData,  // bmp data
                              0,					 // src :left
                              0,					 // src :top
                              mSourceRect.right - mSourceRect.left,		// src : width
                              mSourceRect.bottom - mSourceRect.top,		// src : height      
                              DDF_SAME_DRAW			 // use prev params....
                              );
   }
   LeaveCriticalSection(&mVideoBufferDataCs);
   // ::GetClientRect(mVideoWnd, &mTargetRect);
   //INT targetWidth  = mTargetRect.right - mTargetRect.left;
   //INT targetHeight = mTargetRect.bottom - mTargetRect.top;    
   //if ( mcurMembmp != NULL)   {
   //   // testBmp(mcurMembmp);
   //   HBITMAP oldbmp = (HBITMAP) SelectObject(mMemDC, mcurMembmp);

   //   //StretchBlt(mWindowDC, 0, 0, targetWidth, targetHeight, mMemDC, 0, 0, SRCCOPY);         
   //  // StretchBlt(mWindowDC, 0, 0,mTargetRect.right-mTargetRect.left, mTargetRect.bottom-mTargetRect.top,  mMemDC, 0, 0,mSourceRect.right-mSourceRect.left, mSourceRect.bottom-mSourceRect.top, SRCCOPY);
   //   StretchBlt(mWindowDC, 0, 0,mTargetRect.right-mTargetRect.left, mTargetRect.bottom-mTargetRect.top,  mMemDC, 0, 0,mSourceRect.right-mSourceRect.left, mSourceRect.bottom-mSourceRect.top, SRCCOPY);
   //   SelectObject(mMemDC, oldbmp);     
   //}  

   return TRUE;
}
DWORD CAVPlayer::GetAudioUnitSize() {
   return mSampleBits*mSampleRate*mChannels / 8 * mLengthInMs / 1000;
}

bool CAVPlayer::IsAudioPlay() {
   bool bRet = false;
   EnterCriticalSection(&mPreLockCS);
   bRet = !mAudioStop;
   LeaveCriticalSection(&mPreLockCS);
   return bRet;
}
void CAVPlayer::SetLayerText(const WCHAR* str) {
   if (mLayerText) {
      free((void*)mLayerText);
   }
   mLayerText = (WCHAR*)malloc(sizeof(WCHAR)* (lstrlenW(str) + 1));
   /*  if(mLayerText){
        wcscpy( mLayerText, str);
        }*/
}


