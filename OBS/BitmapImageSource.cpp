/********************************************************************************
 Copyright (C) 2012 Hugh Bailey <obs.jim@gmail.com>

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
********************************************************************************/


#include "Main.h"
#include "BitmapImage.h"


struct ColorSelectionData
{
    HDC hdcDesktop;
    HDC hdcDestination;
    HBITMAP hBitmap;
    bool bValid;

    inline ColorSelectionData() : hdcDesktop(NULL), hdcDestination(NULL), hBitmap(NULL), bValid(false) {}
    inline ~ColorSelectionData() {Clear();}

    inline bool Init()
    {
        hdcDesktop = GetDC(NULL);
        if(!hdcDesktop)
            return false;

        hdcDestination = CreateCompatibleDC(hdcDesktop);
        if(!hdcDestination)
            return false;

        hBitmap = CreateCompatibleBitmap(hdcDesktop, 1, 1);
        if(!hBitmap)
            return false;

        SelectObject(hdcDestination, hBitmap);
        bValid = true;

        return true;
    }

    inline void Clear()
    {
        if(hdcDesktop)
        {
            ReleaseDC(NULL, hdcDesktop);
            hdcDesktop = NULL;
        }

        if(hdcDestination)
        {
            DeleteDC(hdcDestination);
            hdcDestination = NULL;
        }

        if(hBitmap)
        {
            DeleteObject(hBitmap);
            hBitmap = NULL;
        }

        bValid = false;
    }

    inline DWORD GetColor()
    {
        POINT p;
        if(GetCursorPos(&p))
        {
            BITMAPINFO data;
            zero(&data, sizeof(data));

            data.bmiHeader.biSize = sizeof(data.bmiHeader);
            data.bmiHeader.biWidth = 1;
            data.bmiHeader.biHeight = 1;
            data.bmiHeader.biPlanes = 1;
            data.bmiHeader.biBitCount = 24;
            data.bmiHeader.biCompression = BI_RGB;
            data.bmiHeader.biSizeImage = 4;

            if(BitBlt(hdcDestination, 0, 0, 1, 1, hdcDesktop, p.x, p.y, SRCCOPY|CAPTUREBLT))
            {
                DWORD buffer;
                if(GetDIBits(hdcDestination, hBitmap, 0, 1, &buffer, &data, DIB_RGB_COLORS))
                    return 0xFF000000|buffer;
            }
            else
            {
                int err = GetLastError();
                nop();
            }
        }

        return 0xFF000000;
    }
};

struct ConfigDesktopSourceInfo
{
    CTSTR lpName;
    XElement *data;
    StringList strClasses;
};


class BitmapImageSource : public ImageSource
{
    BitmapImage bitmapImage;

    XElement *data = nullptr;

    bool     bUseColorKey = false;
    DWORD    keyColor;
    UINT     keySimilarity, keyBlend;

    DWORD opacity;
    DWORD color;

    Shader   *colorKeyShader = nullptr, *alphaIgnoreShader = nullptr;
public:
    BitmapImageSource(XElement *data)
    {
        this->data = data;
       
        UpdateSettings();

        colorKeyShader      = CreatePixelShaderFromFile(TEXT("shaders\\ColorKey_RGB.pShader"));
        alphaIgnoreShader   = CreatePixelShaderFromFile(TEXT("shaders\\AlphaIgnore.pShader"));
        
        //Log(TEXT("Using bitmap image"));
    }

    ~BitmapImageSource()
    {
        delete colorKeyShader;
        delete alphaIgnoreShader;
    }

    void Tick(float fSeconds)
    {
        bitmapImage.Tick(fSeconds);
    }

    void Render(const Vect2 &pos, const Vect2 &size,SceneRenderType renderType)
    {
      if(GetIsHide()) {
         return ;
      }
      
         if(SceneRenderType::SceneRenderType_Desktop == renderType) {
            return ;
         }
        Texture *texture = bitmapImage.GetTexture();

        if(texture)
        {
            if(bUseColorKey)
            {
                Shader *lastPixelShader = GetCurrentPixelShader();
                DWORD alpha = ((opacity*255/100)&0xFF);
                DWORD outputColor = (alpha << 24) | color&0xFFFFFF;
                LoadPixelShader(colorKeyShader);

                float fSimilarity = float(keySimilarity)*0.01f;
                float fBlend      = float(keyBlend)*0.01f;

                colorKeyShader->SetColor(colorKeyShader->GetParameter(2), keyColor);
                colorKeyShader->SetFloat(colorKeyShader->GetParameter(3), fSimilarity);
                colorKeyShader->SetFloat(colorKeyShader->GetParameter(4), fBlend);

                DrawSprite(texture, outputColor, pos.x, pos.y, pos.x+size.x, pos.y+size.y);
                LoadPixelShader(lastPixelShader);
            }
            else
            {
                DWORD alpha = ((opacity*255/100)&0xFF);
                DWORD outputColor = (alpha << 24) | color&0xFFFFFF;

                DrawSprite(texture, outputColor, pos.x, pos.y, pos.x+size.x, pos.y+size.y);
            }
        }
    }
    
    void UpdateSettings()
    {
        bitmapImage.SetPath(data->GetString(TEXT("path")));
        bitmapImage.EnableFileMonitor(data->GetInt(TEXT("monitor"), 0) == 1);
        bitmapImage.Init();

        //------------------------------------

        opacity = data->GetInt(TEXT("opacity"), 100);
        color = data->GetInt(TEXT("color"), 0xFFFFFFFF);
        if(opacity > 100)
            opacity = 100;

        bool bNewUseColorKey = data->GetInt(TEXT("useColorKey"), 0) != 0;
        keyColor        = data->GetInt(TEXT("keyColor"), 0xFFFFFFFF);
        keySimilarity   = data->GetInt(TEXT("keySimilarity"), 10);
        keyBlend        = data->GetInt(TEXT("keyBlend"), 0);

        bUseColorKey = bNewUseColorKey;
    }

    Vect2 GetSize() const {return bitmapImage.GetSize();}
};


ImageSource* STDCALL CreateBitmapSource(XElement *data)
{
    if(!data)
        return NULL;

    return new BitmapImageSource(data);
}

struct ConfigBitmapInfo
{
    XElement *data = nullptr;
};

bool STDCALL ConfigureBitmapSource(XElement *element, bool bCreating)
{
   return true;
}
