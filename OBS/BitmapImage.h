#pragma once

#include "Main.h"
#include "libnsgif.h"


class BitmapImage{
    Texture *texture = nullptr;
    Vect2 fullSize;

    bool bIsAnimatedGif = false;
    gif_animation gif;
    LPBYTE lpGifData = nullptr;
    List<float> animationTimes;
    BYTE **animationFrameCache = nullptr;
    BYTE *animationFrameData = nullptr;
    UINT curFrame = 0 , curLoop = 0, lastDecodedFrame = 0;
    float curTime;
    float updateImageTime;

    String filePath;
    OSFileChangeData *changeMonitor = nullptr;

    gif_bitmap_callback_vt bitmap_callbacks;

    void CreateErrorTexture(void);

public:
    BitmapImage();
    ~BitmapImage();

    void SetPath(String path);
    void EnableFileMonitor(bool bMonitor);
    void Init(void);

    Vect2 GetSize(void) const;
    Texture* GetTexture(void) const;

    void Tick(float fSeconds);
};
