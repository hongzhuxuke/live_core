
#pragma once

#include "OBSApi.h"
#include "IMediaOutput.h"
#include <vector>



class __declspec(dllexport) MediaVideoSource : public ImageSource, public IVideoNotify {
public:
   MediaVideoSource(XElement *data);
   ~MediaVideoSource();
private:
   Vect2 mVideoSize;
   Texture *mTexture = nullptr;
   Vect2 mPreviousRenderSize;
   Vect2 mMediaOffset;
   Vect2 mMediaSize;
   IMediaOutput* mMediaOutput = nullptr;

   
public:

   bool isRenderingWithoutRef = false;
   volatile bool isInScene;
   bool hasSetAsyncProperties = false;

public:
   CRITICAL_SECTION mTextureLock;

   unsigned int mediaWidth = 0;
   unsigned int mediaHeight = 0;
   unsigned int mediaWidthOffset = 0;
   unsigned int mediaHeightOffset = 0;
   unsigned char *mMediaBuffer = nullptr;

   // should only be set inside texture lock
   bool isRendering = false;
   int remainingVideos;


public:
   Texture *GetTexture() { return mTexture; }
   void ClearTexture();

public:
   // ImageSource
   void Tick(float fSeconds);
   void Render(const Vect2 &pos, const Vect2 &size,SceneRenderType renderType);

   void GlobalSourceLeaveScene();
   void GlobalSourceEnterScene();

   void ChangeScene();
   Vect2 GetSize() const;

public:

   virtual void OnVideoChanged(const unsigned long& videoWidth, const unsigned long& videoHeight, const long long & frameDuration, const long long &timeScale);
   virtual bool OnVideoFrame();
};
ImageSource* STDCALL CreateMediaVideoSource(XElement *data);
