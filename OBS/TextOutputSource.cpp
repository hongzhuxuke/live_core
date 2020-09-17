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

#include <memory>

#include <gdiplus.h>
using namespace Gdiplus;

#define ClampVal(val, minVal, maxVal) \
    if(val < minVal) val = minVal; \
    else if(val > maxVal) val = maxVal;

#define MAX_TEX_SIZE_W 8192
#define MAX_TEX_SIZE_H 8192
#define MIN_TEX_SIZE_W 32
#define MIN_TEX_SIZE_H 32
inline DWORD GetAlphaVal(UINT opacityLevel)
{
    return ((opacityLevel*255/100)&0xFF) << 24;
}

class TextOutputSource : public ImageSource
{
    bool        bUpdateTexture = false;

    String      strCurrentText;
    Texture     *texture = nullptr;
    float       scrollValue;
    float       showExtentTime;

    int         mode = -1;
    String      strText;
    String      strFile;

    String      strFont;
    int         size;
    DWORD       color;
    UINT        opacity;
    UINT        globalOpacity;
    int         scrollSpeed;
    bool        bBold = false, bItalic = false, bUnderline = false, bVertical = false;

    UINT        backgroundOpacity;
    DWORD       backgroundColor;

    bool        bUseOutline = false;
    float       outlineSize;
    DWORD       outlineColor;
    UINT        outlineOpacity;

    UINT        extentWidth, extentHeight;

    bool        bWrap = false;
    bool        bScrollMode = false;
    int         align;

    Vect2       baseSize;
    SIZE        textureSize;
    bool        bUsePointFiltering = false;

    bool        bMonitoringFileChanges = false;
    OSFileChangeData *fileChangeMonitor = nullptr;

    std::unique_ptr<SamplerState> sampler = nullptr;

    bool        bDoUpdate = false;

    SamplerState *ss = nullptr;

    XElement    *data = nullptr;

    void DrawOutlineText(Gdiplus::Graphics *graphics,
                         Gdiplus::Font &font,
                         const Gdiplus::GraphicsPath &path,
                         const Gdiplus::StringFormat &format,
                         const Gdiplus::Brush *brush)
    {
                
        Gdiplus::GraphicsPath *outlinePath;

        outlinePath = path.Clone();

        // Outline color and size
        UINT tmpOpacity = (UINT)((((float)opacity * 0.01f) * ((float)outlineOpacity * 0.01f)) * 100.0f);
        Gdiplus::Pen pen(Gdiplus::Color(GetAlphaVal(tmpOpacity) | (outlineColor&0xFFFFFF)), outlineSize);
        pen.SetLineJoin(Gdiplus::LineJoinRound);

        // Widen the outline
        // It seems that Widen has a huge performance impact on DrawPath call, screw it! We're talking about freaking seconds in some extreme cases...
        //outlinePath->Widen(&pen);

        // Draw the outline
        graphics->DrawPath(&pen, outlinePath);

        // Draw the text        
        graphics->FillPath(brush, &path);


        delete outlinePath;
    }

    HFONT GetFont()
    {
        HFONT hFont = NULL;

        LOGFONT lf;
        zero(&lf, sizeof(lf));
        lf.lfHeight = size;
        lf.lfWeight = bBold ? FW_BOLD : FW_DONTCARE;
        lf.lfItalic = bItalic;
        lf.lfUnderline = bUnderline;
        lf.lfQuality = ANTIALIASED_QUALITY;
        lf.lfCharSet = GB2312_CHARSET;
        if(strFont.IsValid())
        {
            scpy_n(lf.lfFaceName, strFont, 31);

            hFont = CreateFontIndirect(&lf);

        }
        
        if(!hFont)
        {
            scpy_n(lf.lfFaceName, TEXT("Arial"), 31);
            hFont = CreateFontIndirect(&lf);
            
        }

        

        return hFont;
    }

    void UpdateCurrentText()
    {
        if(bMonitoringFileChanges)
        {
            OSMonitorFileDestroy(fileChangeMonitor);
            fileChangeMonitor = NULL;

            bMonitoringFileChanges = false;
        }

        if(mode == 0)
            strCurrentText = strText;

        else if(mode == 1 && strFile.IsValid())
        {
            XFile textFile;
            if(textFile.Open(strFile, XFILE_READ | XFILE_SHARED, XFILE_OPENEXISTING))
            {
                textFile.ReadFileToString(strCurrentText);
            }
            else
            {
                strCurrentText = TEXT("");
                AppWarning(TEXT("TextSource::UpdateTexture: could not open specified file (invalid file name or access violation)"));
            }

            if (fileChangeMonitor = OSMonitorFileStart(strFile))
                bMonitoringFileChanges = true;
        }
        else
            strCurrentText = TEXT("");
    }

    void SetStringFormat(Gdiplus::StringFormat &format)
    {
        UINT formatFlags;

        formatFlags = Gdiplus::StringFormatFlagsNoFitBlackBox
                    | Gdiplus::StringFormatFlagsMeasureTrailingSpaces;


        if(bVertical)
            formatFlags |= Gdiplus::StringFormatFlagsDirectionVertical
                         | Gdiplus::StringFormatFlagsDirectionRightToLeft;

        format.SetFormatFlags(formatFlags);
        format.SetTrimming(Gdiplus::StringTrimmingWord);

         if(bVertical)
           format.SetLineAlignment(Gdiplus::StringAlignmentFar);

    }

    float ProcessScrollMode(Gdiplus::Graphics *graphics, Gdiplus::Font *font, Gdiplus::RectF &layoutBox, Gdiplus::StringFormat *format)
    {
        StringList strList;
        Gdiplus::RectF boundingBox;
        
        float offset = layoutBox.Height;

        Gdiplus::RectF l2(0.0f ,0.0f , layoutBox.Width, 32000.0f); // Really, it needs to be OVER9000

        strCurrentText.FindReplace(L"\n\r", L"\n");
        strCurrentText.GetTokenList(strList,'\n');

        if(strList.Num() != 0)
            strCurrentText.Clear();
        else 
            return 0.0f;

        for(int i = strList.Num() - 1; i >= 0; i--)
        {
            strCurrentText.InsertString(0, TEXT("\n"));
            strCurrentText.InsertString(0, strList.GetElement((unsigned int)i).Array());

            if(strCurrentText.IsValid())
            {
                graphics->MeasureString(strCurrentText, -1, font, l2, &boundingBox);
                offset = layoutBox.Height - boundingBox.Height;
            }
            
            if(offset < 0)
                break;
        }

        return offset;
    }

    void UpdateTexture()
    {
        return ;
        HFONT hFont;
        Gdiplus::Status stat;
        Gdiplus::RectF layoutBox;
        SIZE textSize;
        float offset;

        Gdiplus::RectF boundingBox(0.0f, 0.0f, 32.0f, 32.0f);

        UpdateCurrentText();

        hFont = GetFont();
        if(!hFont)
            return;

        Gdiplus::StringFormat format(Gdiplus::StringFormat::GenericTypographic());

        SetStringFormat(format);

        HDC hdc = CreateCompatibleDC(NULL);
                                       
        Gdiplus::Font font(hdc, hFont);
        Gdiplus::Graphics *graphics = new Gdiplus::Graphics(hdc);
        
        graphics->SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);

        if(strCurrentText.IsValid())
        {
            if(!bWrap)
            {
                stat = graphics->MeasureString(strCurrentText, -1, &font, Gdiplus::PointF(0.0f, 0.0f), &format, &boundingBox);
                if(stat != Gdiplus::Ok)
                    AppWarning(TEXT("TextSource::UpdateTexture: Gdiplus::Graphics::MeasureString failed: %u"), (int)stat);
                if(bUseOutline)
                {
                    //Note: since there's no path widening in DrawOutlineText the padding is half than what it was supposed to be.
                    boundingBox.Width  += outlineSize;
                    boundingBox.Height += outlineSize;
				}
			}
        }

        delete graphics;

        DeleteDC(hdc);
        hdc = NULL;
        DeleteObject(hFont);

        if(bVertical)
        {
            if(boundingBox.Width<size)
            {
                textSize.cx = size;
                boundingBox.Width = float(size);
            }
            else
                textSize.cx = LONG(boundingBox.Width + EPSILON);

            textSize.cy = LONG(boundingBox.Height + EPSILON);
        }
        else
        {
            if(boundingBox.Height<size)
            {
                textSize.cy = size;
                boundingBox.Height = float(size);
            }
            else
                textSize.cy = LONG(boundingBox.Height + EPSILON);

            textSize.cx = LONG(boundingBox.Width + EPSILON);
        }

        //textSize.cx &= 0xFFFFFFFE;
        //textSize.cy &= 0xFFFFFFFE;

        textSize.cx += textSize.cx%2;
        textSize.cy += textSize.cy%2;

        ClampVal(textSize.cx, MIN_TEX_SIZE_W, MAX_TEX_SIZE_W);
        ClampVal(textSize.cy, MIN_TEX_SIZE_H, MAX_TEX_SIZE_H);

        //----------------------------------------------------------------------
        // write image

        {
            HDC hTempDC = CreateCompatibleDC(NULL);

            BITMAPINFO bi;
            zero(&bi, sizeof(bi));

            void* lpBits;

            BITMAPINFOHEADER &bih = bi.bmiHeader;
            bih.biSize = sizeof(bih);
            bih.biBitCount = 32;
            bih.biWidth  = textSize.cx;
            bih.biHeight = textSize.cy;
            bih.biPlanes = 1;

            HBITMAP hBitmap = CreateDIBSection(hTempDC, &bi, DIB_RGB_COLORS, &lpBits, NULL, 0);

            Gdiplus::Bitmap      bmp(textSize.cx, textSize.cy, 4*textSize.cx, PixelFormat32bppARGB, (BYTE*)lpBits);

            graphics = new Gdiplus::Graphics(&bmp); 

            Gdiplus::SolidBrush  *brush = new Gdiplus::SolidBrush(Gdiplus::Color(GetAlphaVal(opacity)|(color&0x00FFFFFF)));

            DWORD bkColor;

		    if(backgroundOpacity == 0 && scrollSpeed !=0)
                bkColor = 1<<24 | (color&0x00FFFFFF);
            else
                bkColor = ((strCurrentText.IsValid()) ? GetAlphaVal(backgroundOpacity) : GetAlphaVal(0)) | (backgroundColor&0x00FFFFFF);

            if((textSize.cx > boundingBox.Width  || textSize.cy > boundingBox.Height))
            {
                stat = graphics->Clear(Gdiplus::Color( 0x00000000));
                if(stat != Gdiplus::Ok)
                    AppWarning(TEXT("TextSource::UpdateTexture: Graphics::Clear failed: %u"), (int)stat);

                Gdiplus::SolidBrush *bkBrush = new Gdiplus::SolidBrush(Gdiplus::Color( bkColor ));

                graphics->FillRectangle(bkBrush, boundingBox);

                delete bkBrush;
            }
            else
            {
                stat = graphics->Clear(Gdiplus::Color( bkColor ));
                if(stat != Gdiplus::Ok)
                    AppWarning(TEXT("TextSource::UpdateTexture: Graphics::Clear failed: %u"), (int)stat);
            }

            graphics->SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);
            graphics->SetCompositingMode(Gdiplus::CompositingModeSourceOver);
            graphics->SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

            if(strCurrentText.IsValid())
            {
                if(bUseOutline)
                {
                    boundingBox.Offset(outlineSize/2, outlineSize/2);

                    Gdiplus::FontFamily fontFamily;
                    Gdiplus::GraphicsPath path;

                    font.GetFamily(&fontFamily);

                    path.AddString(strCurrentText, -1, &fontFamily, font.GetStyle(), font.GetSize(), boundingBox, &format);

                    DrawOutlineText(graphics, font, path, format, brush);
                }
                else
                {

                    stat = graphics->DrawString(strCurrentText, -1, &font, boundingBox, &format, brush);
                    if(stat != Gdiplus::Ok)
                        AppWarning(TEXT("TextSource::UpdateTexture: Graphics::DrawString failed: %u"), (int)stat);
                }
            }

            delete brush;
            delete graphics;

            //----------------------------------------------------------------------
            // upload texture

            if(textureSize.cx != textSize.cx || textureSize.cy != textSize.cy)
            {
                if(texture)
                {
                    delete texture;
                    texture = NULL;
                }

                mcpy(&textureSize, &textSize, sizeof(textureSize));
                texture = CreateTexture(textSize.cx, textSize.cy, GS_BGRA, lpBits, FALSE, FALSE);
            }
            else if(texture)
                texture->SetImage(lpBits, GS_IMAGEFORMAT_BGRA, 4*textSize.cx);

            if(!texture)
            {
                AppWarning(TEXT("TextSource::UpdateTexture: could not create texture"));
                DeleteObject(hFont);
            }

            DeleteDC(hTempDC);
            DeleteObject(hBitmap);
        }
    }

public:
    inline TextOutputSource(XElement *data)
    {
        texture=NULL;
        this->data = data;
        UpdateSettings();

        SamplerInfo si;
        zero(&si, sizeof(si));
        si.addressU = GS_ADDRESS_REPEAT;
        si.addressV = GS_ADDRESS_REPEAT;
        si.borderColor = 0;
        si.filter = GS_FILTER_LINEAR;
        ss = CreateSamplerState(si);
        globalOpacity = 100;

        Log(TEXT("Using text output"));
    }

    ~TextOutputSource()
    {
        if(texture)
        {
            delete texture;
            texture = NULL;
        }

        delete ss;

        if(bMonitoringFileChanges)
        {
            OSMonitorFileDestroy(fileChangeMonitor);
        }
    }

    bool Preprocess()
    {
        if(bMonitoringFileChanges)
        {
            if (OSFileHasChanged(fileChangeMonitor))
                bUpdateTexture = true;
        }

        if(bUpdateTexture)
        {
            bUpdateTexture = false;
            UpdateTexture();
        }
        return true;
    }

    void Tick(float fSeconds)
    {
        if(scrollSpeed != 0 && texture)
        {
            scrollValue += fSeconds*float(scrollSpeed)/(bVertical?(-1.0f)*float(textureSize.cy):float(textureSize.cx));
            while(scrollValue > 1.0f)
                scrollValue -= 1.0f;
            while(scrollValue < -1.0f)
                scrollValue += 1.0f;
        }

        if(showExtentTime > 0.0f)
            showExtentTime -= fSeconds;

        if(bDoUpdate)
        {
            bDoUpdate = false;
            bUpdateTexture = true;
        }
    }

    void Render(const Vect2 &pos, const Vect2 &size,SceneRenderType renderType)
    {
      if(SceneRenderType::SceneRenderType_Desktop == renderType) {
      return ;
      }
        if(texture)
        {
            Vect2 sizeMultiplier = size/baseSize;
            Vect2 newSize = Vect2(float(textureSize.cx), float(textureSize.cy))*sizeMultiplier;

            if(bUsePointFiltering) {
                if (!sampler) {
                    SamplerInfo samplerinfo;
                    samplerinfo.filter = GS_FILTER_POINT;
                    std::unique_ptr<SamplerState> new_sampler(CreateSamplerState(samplerinfo));
                    sampler = std::move(new_sampler);
                }

                LoadSamplerState(sampler.get(), 0);
            }

            DWORD alpha = DWORD(double(globalOpacity)*2.55);
            DWORD outputColor = (alpha << 24) | 0xFFFFFF;

            if(scrollSpeed != 0)
            {
                UVCoord ul(0.0f, 0.0f);
                UVCoord lr(1.0f, 1.0f);

                if(bVertical)
                {
                    /*float sizeVal = float(textureSize.cy);
                    float clampedVal = floorf(scrollValue*sizeVal)/sizeVal;*/
                    ul.y += scrollValue;
                    lr.y += scrollValue;
                }
                else
                {
                    /*float sizeVal = float(textureSize.cx);
                    float clampedVal = floorf(scrollValue*sizeVal)/sizeVal;*/
                    ul.x += scrollValue;
                    lr.x += scrollValue;
                }

                LoadSamplerState(ss);
                DrawSpriteEx(texture, outputColor, pos.x, pos.y, pos.x+newSize.x, pos.y+newSize.y, ul.x, ul.y, lr.x, lr.y);
            }
            else
                DrawSprite(texture, outputColor, pos.x, pos.y, pos.x+newSize.x, pos.y+newSize.y);

            if (bUsePointFiltering)
                LoadSamplerState(NULL, 0);
        }
    }

    Vect2 GetSize() const
    {
        return baseSize;
    }


    void wcharTochar(const wchar_t *wchar, char *chr, int length)
    {
       WideCharToMultiByte(CP_ACP, 0, wchar, -1, chr, length, NULL, NULL);
    }

    void UpdateSettings()
    {
        strFont     = data->GetString(TEXT("font"), TEXT("Arial"));
        color       = data->GetInt(TEXT("color"), 0xFFFFFFFF);
        size        = data->GetInt(TEXT("fontSize"), 48);
        opacity     = data->GetInt(TEXT("textOpacity"), 100);
        scrollSpeed = data->GetInt(TEXT("scrollSpeed"), 0);
        bBold       = data->GetInt(TEXT("bold"), 0) != 0;
        bItalic     = data->GetInt(TEXT("italic"), 0) != 0;
        bWrap       = data->GetInt(TEXT("wrap"), 0) != 0;
        bScrollMode = data->GetInt(TEXT("scrollMode"), 0) != 0;
        bUnderline  = data->GetInt(TEXT("underline"), 0) != 0;
        bVertical   = data->GetInt(TEXT("vertical"), 0) != 0;
        extentWidth = data->GetInt(TEXT("extentWidth"), 0);
        extentHeight= data->GetInt(TEXT("extentHeight"), 0);
        align       = data->GetInt(TEXT("align"), 0);
        strFile     = data->GetString(TEXT("file"));
        strText     = data->GetString(TEXT("text"));
        mode        = data->GetInt(TEXT("mode"), 0);
        bUsePointFiltering = data->GetInt(TEXT("pointFiltering"), 0) != 0;
        baseSize.x  = data->GetInt(TEXT("baseSizeCX"), MIN_TEX_SIZE_W);
        baseSize.y  = data->GetInt(TEXT("baseSizeCY"), MIN_TEX_SIZE_H);
        String imgData = data->GetString(TEXT("baseData"));

        if(texture){
            delete texture;
            texture = NULL;
        }
       
        texture = CreateTextureFromFile(imgData, TRUE);
        //texture = CreateTexture(baseSize.x,baseSize.y,GS_RGBA,imgData,FALSE,FALSE);
        if (texture == nullptr) {
           return;
        }
        //BYTE *lpData=NULL;
        //UINT pitch;
        //texture->Map(lpData,pitch);
        //for(int i=0;i<baseSize.y;i++){
        //    memcpy(lpData+i*pitch, imgData+(int)(i*baseSize.x*4),(int) baseSize.x*4);
        //}
        //texture->Unmap();

        textureSize.cx=baseSize.x;
        textureSize.cy=baseSize.y;
        
        bUseOutline    = data->GetInt(TEXT("useOutline")) != 0;
        outlineColor   = data->GetInt(TEXT("outlineColor"), 0xFF000000);
        outlineSize    = data->GetFloat(TEXT("outlineSize"), 2);
        outlineOpacity = data->GetInt(TEXT("outlineOpacity"), 100);

        backgroundColor   = data->GetInt(TEXT("backgroundColor"), 0xFF000000);
        backgroundOpacity = data->GetInt(TEXT("backgroundOpacity"), 0);

        bUpdateTexture = true;
    }

    void SetString(CTSTR lpName, CTSTR lpVal)
    {
        if(scmpi(lpName, TEXT("font")) == 0)
            strFont = lpVal;
        else if(scmpi(lpName, TEXT("text")) == 0)
            strText = lpVal;
        else if(scmpi(lpName, TEXT("file")) == 0)
            strFile = lpVal;

        bUpdateTexture = true;
    }

    void SetInt(CTSTR lpName, int iValue)
    {
        if(scmpi(lpName, TEXT("color")) == 0)
            color = iValue;
        else if(scmpi(lpName, TEXT("fontSize")) == 0)
            size = iValue;
        else if(scmpi(lpName, TEXT("textOpacity")) == 0)
            opacity = iValue;
        else if(scmpi(lpName, TEXT("scrollSpeed")) == 0)
        {
            if(scrollSpeed == 0)
                scrollValue = 0.0f;
            scrollSpeed = iValue;
        }
        else if(scmpi(lpName, TEXT("bold")) == 0)
            bBold = iValue != 0;
        else if(scmpi(lpName, TEXT("italic")) == 0)
            bItalic = iValue != 0;
        else if(scmpi(lpName, TEXT("wrap")) == 0)
            bWrap = iValue != 0;
        else if(scmpi(lpName, TEXT("scrollMode")) == 0)
            bScrollMode = iValue != 0;
        else if(scmpi(lpName, TEXT("underline")) == 0)
            bUnderline = iValue != 0;
        else if(scmpi(lpName, TEXT("vertical")) == 0)
            bVertical = iValue != 0;
        else if(scmpi(lpName, TEXT("extentWidth")) == 0)
        {
            showExtentTime = 2.0f;
            extentWidth = iValue;
        }
        else if(scmpi(lpName, TEXT("extentHeight")) == 0)
        {
            showExtentTime = 2.0f;
            extentHeight = iValue;
        }
        else if(scmpi(lpName, TEXT("align")) == 0)
            align = iValue;
        else if(scmpi(lpName, TEXT("mode")) == 0)
            mode = iValue;
        else if(scmpi(lpName, TEXT("useOutline")) == 0)
            bUseOutline = iValue != 0;
        else if(scmpi(lpName, TEXT("outlineColor")) == 0)
            outlineColor = iValue;
        else if(scmpi(lpName, TEXT("outlineOpacity")) == 0)
            outlineOpacity = iValue;
        else if(scmpi(lpName, TEXT("backgroundColor")) == 0)
            backgroundColor = iValue;
        else if(scmpi(lpName, TEXT("backgroundOpacity")) == 0)
            backgroundOpacity = iValue;

        bUpdateTexture = true;
    }

    void SetFloat(CTSTR lpName, float fValue)
    {
        if(scmpi(lpName, TEXT("outlineSize")) == 0)
            outlineSize = fValue;

        bUpdateTexture = true;
    }

    inline void ResetExtentRect() {showExtentTime = 0.0f;}
};



ImageSource* STDCALL CreateTextSource(XElement *data)
{
    if(!data)
        return NULL;

    return new TextOutputSource(data);
}

bool STDCALL ConfigureTextSource(XElement *element, bool bCreating)
{   
    return true;
}
