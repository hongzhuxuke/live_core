/**
* John Bradley (jrb@turrettech.com)
*/

#include "MediaVideoSource.h"
#include "IMediaReader.h"
#include <process.h>

MediaVideoSource::MediaVideoSource(XElement *data) {
   Log(TEXT("Using Video Source"));
   isInScene = true;
   mVideoSize.x = float(0);
   mVideoSize.y = float(0);
   InitializeCriticalSection(&mTextureLock);
   mMediaOutput = CreateMediaReader()->GetMediaOut();
   mMediaOutput->SetVideoNotify(this);
   isRendering = true;
   mMediaBuffer = NULL;
   mediaWidth = 0;
   mediaHeight = 0;
   mMediaBuffer = NULL;
}

MediaVideoSource::~MediaVideoSource() {
   mMediaOutput->SetVideoNotify(NULL);
   EnterCriticalSection(&mTextureLock);
   if (mTexture) {
      delete mTexture;
      mTexture = nullptr;
   }
   LeaveCriticalSection(&mTextureLock);

   DeleteCriticalSection(&mTextureLock);
   if (mMediaBuffer) {
      delete mMediaBuffer;
      mMediaBuffer = NULL;
   }
}

void MediaVideoSource::Tick(float fSeconds) {
}

void MediaVideoSource::GlobalSourceEnterScene() {
   isInScene = true;
}

void MediaVideoSource::GlobalSourceLeaveScene() {
   isInScene = false;
}

// you must lock the mTexture before you call this
void MediaVideoSource::ClearTexture() {
   BYTE *lpData;
   UINT pitch;
   if (mTexture) {
      mTexture->Map(lpData, pitch);
      memset(lpData, 0, pitch * mTexture->Height());
      mTexture->Unmap();
   }
}
void MediaVideoSource::Render(const Vect2 &pos, const Vect2 &size,SceneRenderType renderType) {
   if(SceneRenderType::SceneRenderType_Desktop == renderType) {
      return ;
   }

   if (mediaWidth == 0 || mediaHeight==0)
   {
      return ;
   }
   
   if (size.x==0||size.y==0)
   {
      return ;
   }

   EnterCriticalSection(&mTextureLock);

   mMediaOffset.x = (float)mediaWidthOffset;
   mMediaOffset.y = (float)mediaHeightOffset;

   mMediaSize.x = (float)mediaWidth;
   mMediaSize.y = (float)mediaHeight;

   float k1=mMediaSize.x/mMediaSize.y;
   float k2=size.x/size.y;

   if (k1>k2)
   {
      mMediaSize.x=size.x;
      mMediaSize.y=size.x/k1;
   }
   else  {
      mMediaSize.y=size.y;
      mMediaSize.x=size.y*k1;
   }


   if (mediaWidth > 0 && mediaHeight > 0) {

      if (mTexture) {
         if (mTexture->Width() != mediaWidth || mTexture->Height() != mediaHeight) {
            delete mTexture;
            mTexture = NULL;
         }
      }

      if (mTexture == NULL) {
          mTexture = CreateTexture(mediaWidth, mediaHeight, GS_BGRA, nullptr, FALSE, FALSE);
      }
      if (mTexture != NULL&&isRendering)
      {
         mTexture->SetImage(mMediaBuffer, GS_IMAGEFORMAT_BGRA, mTexture->Width() * 4);
         DrawSprite(mTexture, 0xFFFFFFFF, pos.x + mMediaOffset.x, pos.y + mMediaOffset.y, pos.x + mMediaSize.x, pos.y + mMediaSize.y);
      }
   }

   LeaveCriticalSection(&mTextureLock);
}



Vect2 MediaVideoSource::GetSize() const {
   return mVideoSize;
}

void MediaVideoSource::OnVideoChanged(const unsigned long& videoWidth, const unsigned long& videoHeight, const long long & frameDuration, const long long &timeScale) {

}
bool MediaVideoSource::OnVideoFrame() {


   void *buffer = NULL;
   //unsigned long long bufferSize = 0;
   unsigned long long videoTime = 0;

   unsigned long videoWidth = 0;
   unsigned long videoHeight = 0;
   long long  frameDuration = 0;
   long long timeScale = 0;
   mMediaOutput->GetVideoParam(videoWidth, videoHeight, frameDuration, timeScale);


//   EnterCriticalSection(&mTextureLock);
   if(TryEnterCriticalSection(&mTextureLock))
   {
      if (videoWidth>0 && videoHeight>0)
      {
         bool isRefit=false;
         if (videoWidth!=mediaWidth||videoHeight!=mediaHeight) {
            if (mMediaBuffer)  {
               delete mMediaBuffer   ;
               mMediaBuffer = NULL;
            }   
            isRefit=true;
         }

         if (!mMediaBuffer) {
            mMediaBuffer = new unsigned char[videoWidth*videoHeight * 4];
            unsigned int *data=(unsigned int *)mMediaBuffer;
            for(int i=0;i<videoWidth*videoHeight;i++) {
               data[i]=0xFF000000;
            }
         }

         bool ret = mMediaOutput->GetNextVideoBuffer(mMediaBuffer, videoWidth*videoHeight*4, &videoTime);


         mediaWidth=videoWidth;
         mediaHeight=videoHeight;

         mVideoSize.x=mediaWidth;
         mVideoSize.y=mediaHeight;
         mediaWidthOffset = 0;
         mediaHeightOffset = 0;
         LeaveCriticalSection(&mTextureLock);

         if (mMediaOutput!=NULL&&isRefit) {
            mMediaOutput->Refit();
         }
      }
   }
   return true;
}
ImageSource* STDCALL CreateMediaVideoSource(XElement *data) {
   MediaVideoSource* newMediaSource = new MediaVideoSource(data);
   return newMediaSource;
}



