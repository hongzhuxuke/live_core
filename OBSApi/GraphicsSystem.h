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


#pragma  once

#pragma warning(disable: 4530)
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "VH_ConstDeff.h"
/*=========================================================
    Enums
==========================================================*/

//---------------------------------
//drawing types
enum GSDrawMode {GS_POINTS, GS_LINES, GS_LINESTRIP, GS_TRIANGLES, GS_TRIANGLESTRIP};


//---------------------------------
//texture formats
enum GSColorFormat {
   GS_UNKNOWNFORMAT, 
   GS_ALPHA, 
   GS_GRAYSCALE,
   GS_RGB, 
   GS_RGBA, 
   GS_BGR, 
   GS_BGRA, 
   GS_RGBA16F, 
   GS_RGBA32F, 
   GS_B5G5R5A1, 
   GS_B5G6R5, 
   GS_R10G10B10A2, 
   GS_DXT1, 
   GS_DXT3, 
   GS_DXT5,
   GS_COLOR_FORMAT_END};


//---------------------------------
//index size
enum GSIndexType {GS_UNSIGNED_SHORT, GS_UNSIGNED_LONG};


//---------------------------------
//blend functions
enum GSBlendType {GS_BLEND_ZERO, GS_BLEND_ONE, GS_BLEND_SRCCOLOR, GS_BLEND_INVSRCCOLOR, GS_BLEND_SRCALPHA, GS_BLEND_INVSRCALPHA, GS_BLEND_DSTCOLOR, GS_BLEND_INVDSTCOLOR, GS_BLEND_DSTALPHA, GS_BLEND_INVDSTALPHA, GS_BLEND_FACTOR, GS_BLEND_INVFACTOR};


//---------------------------------
//sampling filters
enum GSSampleFilter
{
    GS_FILTER_LINEAR,
    GS_FILTER_POINT,
    GS_FILTER_ANISOTROPIC,
    GS_FILTER_MIN_MAG_POINT_MIP_LINEAR,
    GS_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT,
    GS_FILTER_MIN_POINT_MAG_MIP_LINEAR,
    GS_FILTER_MIN_LINEAR_MAG_MIP_POINT,
    GS_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR,
    GS_FILTER_MIN_MAG_LINEAR_MIP_POINT,

    GS_FILTER_MIN_MAG_MIP_POINT=GS_FILTER_POINT,
    GS_FILTER_MIN_MAG_MIP_LINEAR=GS_FILTER_LINEAR
};

//---------------------------------
//sampling address mode
enum GSAddressMode
{
    GS_ADDRESS_CLAMP,
    GS_ADDRESS_WRAP,
    GS_ADDRESS_MIRROR,
    GS_ADDRESS_BORDER,
    GS_ADDRESS_MIRRORONCE,

    GS_ADDRESS_NONE=GS_ADDRESS_CLAMP,
    GS_ADDRESS_REPEAT=GS_ADDRESS_WRAP
};


/*=========================================================
    Vertex Buffer Data struct
==========================================================*/

class UVCoordList : public List<UVCoord> {};

struct VBData
{
    List<Vect>      VertList;
    List<Vect>      NormalList;
    List<DWORD>     ColorList;
    List<Vect>      TangentList;
    List<UVCoordList> UVList;

    inline VBData() {}
    inline ~VBData() {Clear();}

    inline void Clear()
    {
        VertList.Clear();
        NormalList.Clear();
        ColorList.Clear();
        TangentList.Clear();

        for(DWORD i=0;i<UVList.Num();i++)
            UVList[i].Clear();

        UVList.Clear();
    }

    inline void Serialize(Serializer &s)
    {
        Vect::SerializeList(s, VertList);
        Vect::SerializeList(s, NormalList);
        s << ColorList;
        Vect::SerializeList(s, TangentList);

        DWORD dwSize;

        if(s.IsLoading())
        {
            //FIXME: ???
            s << dwSize;
            UVList.SetSize(dwSize);
        }
        else
        {
            dwSize = UVList.Num();
            s << dwSize;
        }

        for(DWORD i=0; i<dwSize; i++)
            s << UVList[i];
    }
};


/*=========================================================
    SamplerState class
==========================================================*/

struct SamplerInfo
{
    inline SamplerInfo()
    {
        zero(this, sizeof(SamplerInfo));
        maxAnisotropy = 16;
    }

    GSSampleFilter filter;
    GSAddressMode addressU;
    GSAddressMode addressV;
    GSAddressMode addressW;
    UINT maxAnisotropy;
    Color4 borderColor;
};

class BASE_EXPORT SamplerState
{
protected:
    SamplerInfo info;

public:
    virtual ~SamplerState() {}

    const SamplerInfo& GetSamplerInfo() const {return info;}
};


/*=========================================================
    Texture class
==========================================================*/

class BASE_EXPORT BaseTexture
{
public:
    virtual ~BaseTexture() {}
};


/*=========================================================
    Texture class
==========================================================*/

enum GSImageFormat
{
    GS_IMAGEFORMAT_A8,
    GS_IMAGEFORMAT_L8,
    GS_IMAGEFORMAT_RGB,
    GS_IMAGEFORMAT_RGBX,
    GS_IMAGEFORMAT_RGBA,
    GS_IMAGEFORMAT_RGBA16F,
    GS_IMAGEFORMAT_RGBA32F,
    GS_IMAGEFORMAT_BGR,
    GS_IMAGEFORMAT_BGRX,
    GS_IMAGEFORMAT_BGRA,
};

class BASE_EXPORT Texture : public BaseTexture
{
public:
    virtual DWORD Width() const=0;
    virtual DWORD Height() const=0;
    virtual BOOL HasAlpha() const=0;
    virtual void SetImage(void *lpData, GSImageFormat imageFormat, UINT pitch)=0;
    virtual bool Map(BYTE *&lpData, UINT &pitch)=0;
    virtual void Unmap()=0;
    virtual GSColorFormat GetFormat() const=0;

    virtual bool GetDC(HDC &hDC)=0;
    virtual void ReleaseDC()=0;

    virtual LPVOID GetD3DTexture()=0;
    virtual HANDLE GetSharedHandle()=0;
};


/*=========================================================
    Shader class
==========================================================*/

enum ShaderParameterType
{
    Parameter_Unknown,
    Parameter_Bool,
    Parameter_Float,
    Parameter_Int,
    Parameter_String,
    Parameter_Vector2,
    Parameter_Vector3,
    Parameter_Vector=Parameter_Vector3,
    Parameter_Vector4,
    Parameter_Matrix3x3,
    Parameter_Matrix,
    Parameter_Texture
};

struct ShaderParameterInfo
{
    String name;
    ShaderParameterType type;
};

enum ShaderType
{
    ShaderType_Vertex,
    ShaderType_Pixel,
    ShaderType_Geometry
};

class BASE_EXPORT Shader
{
protected:
    HANDLE hViewProj;

    inline HANDLE  GetViewProj() const {return hViewProj;}

public:
    virtual ~Shader() {}

    virtual ShaderType GetType() const=0;

    virtual int    NumParams() const=0;
    virtual HANDLE GetParameter(UINT parameter) const=0;
    virtual HANDLE GetParameterByName(CTSTR lpName) const=0;
    virtual void   GetParameterInfo(HANDLE hObject, ShaderParameterInfo &paramInfo) const=0;

    virtual void   SetBool(HANDLE hObject, BOOL bValue)=0;
    virtual void   SetFloat(HANDLE hObject, float fValue)=0;
    virtual void   SetInt(HANDLE hObject, int iValue)=0;
    virtual void   SetMatrix(HANDLE hObject, float *matrix)=0;
    virtual void   SetVector(HANDLE hObject, const Vect &value)=0;
    virtual void   SetVector2(HANDLE hObject, const Vect2 &value)=0;
    virtual void   SetVector4(HANDLE hObject, const Vect4 &value)=0;
    virtual void   SetTexture(HANDLE hObject, BaseTexture *texture)=0;
    virtual void   SetValue(HANDLE hObject, const void *val, DWORD dwSize)=0;

    inline  void   SetColor(HANDLE hObject, const Color4 &value)
    {
        SetVector4(hObject, value);
    }
    inline  void   SetColor(HANDLE hObject, float fR, float fB, float fG, float fA=1.0f)
    {
        SetVector4(hObject, Color4(fR, fB, fG, fA));
    }
    inline  void   SetColor(HANDLE hObject, DWORD color)
    {
        SetVector4(hObject, RGBA_to_Vect4(color));
    }

    inline  void   SetColor3(HANDLE hObject, const Color3 &value)
    {
        SetVector(hObject, value);
    }
    inline  void   SetColor3(HANDLE hObject, float fR, float fB, float fG)
    {
        SetVector(hObject, Color3(fR, fB, fG));
    }
    inline  void   SetColor3(HANDLE hObject, DWORD color)
    {
        SetVector(hObject, RGB_to_Vect(color));
    }

    inline  void   SetVector4(HANDLE hObject, float fX, float fY, float fZ, float fW)
    {
        SetVector4(hObject, Vect4(fX, fY, fZ, fW));
    }

    inline  void   SetVector(HANDLE hObject, float fX, float fY, float fZ)
    {
        SetVector(hObject, Vect(fX, fY, fZ));
    }

    inline void SetMatrix(HANDLE hObject, const Matrix &mat)
    {
        float out[16];
        Matrix4x4Convert(out, mat);
        SetMatrix(hObject, out);
    }

    inline void SetMatrixIdentity(HANDLE hObject)
    {
        float out[16];
        Matrix4x4Identity(out);
        SetMatrix(hObject, out);
    }
};

using ShaderBlob = std::vector<char>;

template <Shader *CreateShader(ShaderBlob const&, CTSTR lpShader, CTSTR lpFileName)>
class BASE_EXPORT FutureShader
{
    ShaderBlob const *shaderData = nullptr;
    std::wstring const *fileData = nullptr;
    std::wstring fileName;
    std::unique_ptr<Shader> shader;
    HANDLE shaderReadyEvent;
    bool isReady = false;

    FutureShader(FutureShader const&) = delete;
    FutureShader &operator=(FutureShader const&) = delete;

    FutureShader(HANDLE event_, ShaderBlob const &data, std::wstring const &fileData, std::wstring fileName) : shaderData(&data), fileData(&fileData), fileName(fileName), shaderReadyEvent(event_) {}
public:
    FutureShader() {}
    FutureShader(FutureShader &&other)
        : shaderData(other.shaderData), fileData(other.fileData), fileName(std::move(other.fileName)), shader(std::move(other.shader)), shaderReadyEvent(other.shaderReadyEvent),
          isReady(other.isReady)
    {
        other.shaderData = nullptr;
        other.isReady = false;
    }

    FutureShader &operator=(FutureShader &&other)
    {
        shaderData = other.shaderData;
        other.shaderData = nullptr;
        fileData = other.fileData;
        fileName = other.fileName;
        shader = std::move(other.shader);
        shaderReadyEvent = other.shaderReadyEvent;
        isReady = other.isReady;
        return *this;
    }

    inline Shader *Shader()
    {
        if (!shaderData)
            return nullptr;

        if (!isReady)
        {
            if (WaitForSingleObject(shaderReadyEvent, 0) != WAIT_OBJECT_0)
                return shader.get();

            shader.reset(CreateShader(*shaderData, fileData->c_str(), fileName.c_str()));

            isReady = true;
        }
        return shader.get();
    }

    friend class GraphicsSystem;
};
BASE_EXPORT Shader* CreateVertexShaderFromBlob(ShaderBlob const& blob, CTSTR lpShader, CTSTR lpFileName);
BASE_EXPORT Shader* CreatePixelShaderFromBlob(ShaderBlob const& blob, CTSTR lpShader, CTSTR lpFileName);

using FutureVertexShader = FutureShader<CreateVertexShaderFromBlob>;
using FuturePixelShader = FutureShader<CreatePixelShaderFromBlob>;

/*=========================================================
    Output Duplication class
==========================================================*/

enum DuplicatorInfo
{
    DuplicatorInfo_Error,
    DuplicatorInfo_Timeout,
    DuplicatorInfo_Lost,
    DuplicatorInfo_Acquired
};

class BASE_EXPORT OutputDuplicator
{
public:
    virtual ~OutputDuplicator() {}

    virtual DuplicatorInfo AcquireNextFrame(UINT timeout)=0;
    virtual Texture* GetCopyTexture()=0;
    virtual Texture* GetCursorTex(POINT* pos)=0;
};

/*=========================================================
    Vertex Buffer class
==========================================================*/

class BASE_EXPORT VertexBuffer
{
public:
    virtual ~VertexBuffer() {}

    virtual void FlushBuffers()=0;
    virtual VBData* GetData()=0;
};


/*=========================================================
    Graphics System Class
==========================================================*/

struct TextureInfo
{
    GSColorFormat type;
    int width, height;
};
class OBSRenderView{
public:   
   virtual ~OBSRenderView(){}
};

class BASE_EXPORT GraphicsSystem
{
public:
    virtual void Flush()=0;
    virtual void Present()=0;
    virtual void ResizeView()=0;
    virtual void UnloadAllData()=0;
    virtual long GetDeviceRemovedReason()=0;
    //----------------------------------------------------
    //Initialization/Destruction
    GraphicsSystem();
    virtual ~GraphicsSystem();

    virtual LPVOID GetDevice()=0;
    virtual LPVOID GetContent() = 0;
    virtual void Init();


    //----------------------------------------------------
    //Matrix Functions
    void  MatrixPush();
    void  MatrixPop();
    void  MatrixSet(const Matrix &m);
    void  MatrixGet(Vect &v, Quat &q);
    void  MatrixGet(Matrix &m);

    void  MatrixIdentity();

    void  MatrixMultiply(const Matrix &m);
    void  MatrixRotate(float x, float y, float z, float a);
    void  MatrixRotate(const AxisAngle &aa);
    void  MatrixRotate(const Quat &q);

    void  MatrixTranslate(float x, float y);
    void  MatrixTranslate(const Vect2 &pos);
    void  MatrixScale(const Vect2 &scale);
    void  MatrixScale(float x, float y);
    void  MatrixTranspose();


    //----------------------------------------------------
    //Texture Functions
    virtual Texture*        CreateTextureFromSharedHandle(unsigned int width, unsigned int height, HANDLE handle)=0;
    virtual Texture*        CreateTexture(unsigned int width, unsigned int height, GSColorFormat colorFormat, void *lpData, BOOL bBuildMipMaps, BOOL bStatic)=0;
    virtual Texture*        CreateTextureFromFile(CTSTR lpFile, BOOL bBuildMipMaps)=0;
    virtual Texture*        CreateRenderTarget(unsigned int width, unsigned int height, GSColorFormat colorFormat, BOOL bGenMipMaps)=0;
    virtual Texture*        CreateGDITexture(unsigned width, unsigned int height)=0;

    virtual bool            GetTextureFileInfo(CTSTR lpFile, TextureInfo &info)=0;

    virtual SamplerState*   CreateSamplerState(SamplerInfo &info)=0;

    virtual UINT            GetNumOutputs()=0;
    virtual OutputDuplicator *CreateOutputDuplicator(UINT outputID)=0;


    //----------------------------------------------------
    //Shader Functions
    virtual Shader*         CreateVertexShader(CTSTR lpShader, CTSTR lpFileName)=0;
    virtual Shader*         CreatePixelShader(CTSTR lpShader, CTSTR lpFileName)=0;
    Shader*                 CreateVertexShaderFromFile(CTSTR lpFileName);
    Shader*                 CreatePixelShaderFromFile(CTSTR lpFileName);
    FuturePixelShader       CreatePixelShaderFromFileAsync(CTSTR fileName);


    //----------------------------------------------------
    //Vertex Buffer Functions
    virtual VertexBuffer* CreateVertexBuffer(VBData *vbData, BOOL bStatic=1)=0;


    //----------------------------------------------------
    //Main Rendering Functions
    virtual void  LoadVertexBuffer(VertexBuffer* vb)=0;
    virtual void  LoadTexture(Texture *texture, UINT idTexture=0)=0;
    virtual void  LoadSamplerState(SamplerState *sampler, UINT idSampler=0)=0;
    virtual void  LoadVertexShader(Shader *vShader)=0;
    virtual void  LoadPixelShader(Shader *pShader)=0;

    virtual Shader* GetCurrentPixelShader()=0;
    virtual Shader* GetCurrentVertexShader()=0;

    virtual void  SetRenderTarget(Texture *texture)=0;
    virtual void  Draw(GSDrawMode drawMode, DWORD startVert=0, DWORD nVerts=0)=0;


    //----------------------------------------------------
    // Drawing mode functions

    virtual void  EnableBlending(BOOL bEnable)=0;
    virtual void  BlendFunction(GSBlendType srcFactor, GSBlendType destFactor, float fFactor=1.0f)=0;

    virtual void  ClearColorBuffer(DWORD color=0xFF000000)=0;


    //----------------------------------------------------

    void  StartVertexBuffer();
    VertexBuffer* SaveVertexBuffer();
    void  Vertex(float x, float y, float z=0);
    void  Vertex(const Vect &v);
    void  Normal(float x, float y, float z);
    void  Normal(const Vect &v);
    void  Color(DWORD dwRGBA);
    void  Color(const Color4 &v);
    void  TexCoord(float u, float v, int idTexture=0);
    void  TexCoord(const UVCoord &uv, int idTexture=0);

    void  Vertex(const Vect2 &v2) {Vertex(Vect(v2));}

    void DrawSprite(Texture *texture, DWORD color, float x, float y, float x2, float y2);
    virtual void DrawSpriteEx(Texture *texture, DWORD color, float x, float y, float x2, float y2, float u, float v, float u2, float v2)=0;
    virtual void DrawBox(const Vect2 &upperLeft, const Vect2 &size)=0;
    virtual void SetCropping(float top, float left, float bottom, float right)=0;


    //----------------------------------------------------
    //Other Functions
    virtual void  Ortho(float left, float right, float top, float bottom, float znear, float zfar)=0;
    virtual void  Frustum(float left, float right, float top, float bottom, float znear, float zfar)=0;

    virtual void  SetViewport(float x, float y, float width, float height)=0;

    virtual void  SetScissorRect(XRect *pRect=NULL)=0;

    // To prevent breaking the API, put this at the end instead of with the other Texture functions
    virtual Texture*        CreateSharedTexture(unsigned int width, unsigned int height)=0;
    virtual Texture*       CreateReadTexture(unsigned int width, unsigned int height, GSColorFormat colorFormat)=0;

   virtual OBSRenderView *CreateRenderTargetView(HWND hwnd,int cx,int cy){return NULL;}
   virtual void DestoryRenderTargetView(OBSRenderView *view){}
   virtual void SetRenderTargetView(OBSRenderView *view){}


protected:
    //manual coordinate generation
    VBData *vbd = nullptr;
    DWORD dwCurPointVert = 0;
    DWORD dwCurTexVert = 0;
    DWORD dwCurColorVert = 0;
    DWORD dwCurNormVert = 0;
    BitList TexCoordSetList;
    BOOL bNormalSet = FALSE;
    BOOL bColorSet = FALSE;

    //matrix stuff
    List<Matrix> MatrixStack;
    int curMatrix;

    struct FutureShaderContainer
    {
        struct FutureShaderContext
        {
            ShaderBlob shaderData;
            std::unique_ptr<void, EventDeleter> readyEvent;
            std::unique_ptr<void, ThreadDeleter<>> thread;
            std::wstring fileData, fileName;
        };
        std::map<std::wstring, FutureShaderContext> contexts;
        std::unique_ptr<void, MutexDeleter> lock;
        FutureShaderContainer() : lock(OSCreateMutex()) {}
    } futureShaders;

    virtual void ResetViewMatrix()=0;

public:
    virtual void CopyTexture(Texture *texDest, Texture *texSrc)=0;
    virtual void DrawSpriteExRotate(Texture *texture, DWORD color, float x, float y, float x2, float y2, float degrees, float u, float v, float u2, float v2, float texDegrees)=0;

    virtual Shader *CreateVertexShaderFromBlob(ShaderBlob const &blob, CTSTR lpShader, CTSTR lpFileName) = 0;
    virtual Shader *CreatePixelShaderFromBlob(ShaderBlob const &blob, CTSTR lpShader, CTSTR lpFileName) = 0;
private:
    virtual void CreateVertexShaderBlob(ShaderBlob &blob, CTSTR lpShader, CTSTR lpFileName) = 0;
    virtual void CreatePixelShaderBlob(ShaderBlob &blob, CTSTR lpShader, CTSTR lpFileName) = 0;
};



BASE_EXPORT OBSRenderView *OBSGraphics_CreateRenderTargetView(HWND,int,int);
BASE_EXPORT void OBSGraphics_DestoryRenderTargetView(OBSRenderView *);
BASE_EXPORT void OBSGraphics_SetRenderTargetView(OBSRenderView *);


BASE_EXPORT void MatrixSet(const Matrix &m);
BASE_EXPORT void MatrixGet(Matrix &m);
BASE_EXPORT void MatrixMultiply(const Matrix &m);
BASE_EXPORT void MatrixRotate(float x, float y, float z, float a);
BASE_EXPORT void MatrixRotate(const AxisAngle &aa);
BASE_EXPORT void MatrixRotate(const Quat &q);
BASE_EXPORT void MatrixTranslate(float x, float y);
BASE_EXPORT void MatrixTranslate(const Vect2 &pos);
BASE_EXPORT void MatrixScale(const Vect2 &scale);
BASE_EXPORT void MatrixScale(float x, float y);
BASE_EXPORT void MatrixTranspose();
BASE_EXPORT void MatrixIdentity();
BASE_EXPORT Texture* CreateTexture(unsigned int width, unsigned int height, GSColorFormat colorFormat, void *lpData=NULL, BOOL bGenMipMaps=1, BOOL bStatic=1);
BASE_EXPORT Texture* CreateTextureFromFile(CTSTR lpFile, BOOL bBuildMipMaps);
BASE_EXPORT Texture* CreateRenderTarget(unsigned int width, unsigned int height, GSColorFormat colorFormat, BOOL bGenMipMaps);
BASE_EXPORT Texture* CreateReadTexture(unsigned int width, unsigned int height, GSColorFormat colorFormat);
BASE_EXPORT Texture* CreateGDITexture(unsigned int width, unsigned int height);

BASE_EXPORT void OBSAPI_SetCreateTextTextureMemory(FuncCreateTextureByText,FuncFreeMemory);
BASE_EXPORT void OBSAPI_SetCreateTextureFromFileHook(FuncCheckFileTexture);
BASE_EXPORT Texture* CreateTextureFromText(wchar_t * text);
BASE_EXPORT Texture* CreateFromSharedHandle(unsigned int width, unsigned int height, HANDLE handle);
BASE_EXPORT Texture* CreateSharedTexture(unsigned int width, unsigned int height);
BASE_EXPORT SamplerState* CreateSamplerState(SamplerInfo &info);
BASE_EXPORT Shader* CreateVertexShaderFromBlob(ShaderBlob const& blob, CTSTR lpShader, CTSTR lpFileName);
BASE_EXPORT Shader* CreatePixelShaderFromBlob(ShaderBlob const& blob, CTSTR lpShader, CTSTR lpFileName);
BASE_EXPORT Shader* CreateVertexShader(CTSTR lpShader, CTSTR lpFileName);
BASE_EXPORT Shader* CreatePixelShader(CTSTR lpShader, CTSTR lpFileName);
BASE_EXPORT Shader* CreateVertexShaderFromFile(CTSTR lpFileName);
BASE_EXPORT Shader* CreatePixelShaderFromFile(CTSTR lpFileName);
BASE_EXPORT FuturePixelShader CreatePixelShaderFromFileAsync(CTSTR fileName);
BASE_EXPORT VertexBuffer* CreateVertexBuffer(VBData *vbData, BOOL bStatic=1);
BASE_EXPORT void LoadVertexBuffer(VertexBuffer* vb);
BASE_EXPORT void LoadTexture(Texture *texture, UINT idTexture=0);
BASE_EXPORT void LoadSamplerState(SamplerState *sampler, UINT idSampler=0);
BASE_EXPORT void LoadVertexShader(Shader *vShader);
BASE_EXPORT void LoadPixelShader(Shader *pShader);
BASE_EXPORT Shader* GetCurrentPixelShader();
BASE_EXPORT Shader* GetCurrentVertexShader();
BASE_EXPORT void SetRenderTarget(Texture *texture);
BASE_EXPORT void Draw(GSDrawMode drawMode, DWORD StartVert=0, DWORD nVerts=0);
BASE_EXPORT void EnableBlending(BOOL bEnabled);
BASE_EXPORT void BlendFunction(GSBlendType srcFactor, GSBlendType destFactor, float fFactor=1.0f);
BASE_EXPORT void ClearColorBuffer(DWORD color=0xFF000000);
BASE_EXPORT void SetCropping(float top,float left,float bottom,float right);
BASE_EXPORT void StartVertexBuffer();
BASE_EXPORT VertexBuffer* SaveVertexBuffer();
BASE_EXPORT void Vertex(float x, float y, float z=0);
BASE_EXPORT void Vertex(const Vect &v);
BASE_EXPORT void Vertex(const Vect2 &v2);
BASE_EXPORT void Normal(float x, float y, float z);
BASE_EXPORT void Normal(const Vect &v);
BASE_EXPORT void Color(DWORD dwRGBA);
BASE_EXPORT void Color(const Color4 &v);
BASE_EXPORT void Color(float R, float G, float B, float A);
BASE_EXPORT void TexCoord(float u, float v, int idTexture=0);
BASE_EXPORT void TexCoord(const UVCoord &uv, int idTexture=0);
BASE_EXPORT void Ortho(float left, float right, float top, float bottom, float znear, float zfar);
BASE_EXPORT void Frustum(float left, float right, float top, float bottom, float znear, float zfar);
BASE_EXPORT void SetViewport(float x, float y, float width, float height);
BASE_EXPORT void SetScissorRect(XRect *pRect=NULL);
BASE_EXPORT void DrawSprite(Texture *texture, DWORD color, float x, float y);
BASE_EXPORT void DrawSprite(Texture *texture, DWORD color, float x, float y, float x2, float y2);
BASE_EXPORT void DrawSpriteEx(Texture *texture, DWORD color, float x, float y, float x2, float y2, float u, float v, float u2, float v2);
BASE_EXPORT void DrawBox(const Vect2 &upperLeft, const Vect2 &size);
BASE_EXPORT void DrawSpriteExRotate(Texture *texture, DWORD color, float x, float y, float x2, float y2, float degrees, float u, float v, float u2, float v2, float texDegrees);
BASE_EXPORT LPVOID GetD3D();
BASE_EXPORT LPVOID GetD3DContent();
BASE_EXPORT void OBSGraphics_Register(GraphicsSystem*);
BASE_EXPORT void OBSGraphics_UnRegister();
BASE_EXPORT void OBSGraphics_Init();
BASE_EXPORT void OBSGraphics_ResizeView();
BASE_EXPORT void OBSGraphics_Present();
BASE_EXPORT void OBSGraphics_CopyTexture(Texture *,Texture *);
BASE_EXPORT void OBSGraphics_UnloadAllData();
BASE_EXPORT void OBSGraphics_Flush();
BASE_EXPORT GraphicsSystem *OBSGraphics_GetGraphicsSystem();
BASE_EXPORT long OBSGraphics_GetDeviceRemovedReason();
BASE_EXPORT void OBSGraphics_Lock();
BASE_EXPORT void OBSGraphics_UnLock();


BASE_EXPORT bool OBSGraphics_Load(HWND mRenderWidgetHwnd, int renderWidth,int renderHeight,ConfigFile *configFile);
BASE_EXPORT void OBSGraphics_Unload();
BASE_EXPORT void OBSApiLog(const char *format, ...);
