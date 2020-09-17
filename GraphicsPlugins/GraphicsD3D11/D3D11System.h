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


#pragma once
#include <comdef.h>
#include <D3D11.h>
#include <D3DX11.h>
#include <DXGI.h>
#include <dxgi1_2.h>
#include "Utility/XT.h"
#include <GraphicsSystem.h>
#include <CodeTokenizer.h>
class D3D11VertexShader;

//=============================================================================

class D3D11VertexBuffer : public VertexBuffer
{
    friend class D3D11System;

    ID3D11Buffer *vertexBuffer = nullptr;
    ID3D11Buffer *normalBuffer = nullptr;
    ID3D11Buffer *colorBuffer = nullptr;
    ID3D11Buffer *tangentBuffer = nullptr;
    List<ID3D11Buffer*> UVBuffers;

    UINT vertexSize = 0;
    UINT normalSize = 0;
    UINT colorSize = 0;
    UINT tangentSize = 0;
    List<UINT> UVSizes;

    BOOL bDynamic = false;
    UINT numVerts = 0;
    VBData *data = nullptr;

    static VertexBuffer* CreateVertexBuffer(VBData *vbData, BOOL bStatic);

    void MakeBufferList(D3D11VertexShader *vShader, List<ID3D11Buffer*> &bufferList, List<UINT> &strides) const;

public:
    ~D3D11VertexBuffer();

    virtual void FlushBuffers();
    virtual VBData* GetData();
};

//=============================================================================

class D3D11SamplerState : public SamplerState
{
    friend class D3D11System;

    ID3D11SamplerState *state = nullptr;

    static SamplerState* CreateSamplerState(SamplerInfo &info);

public:
    ~D3D11SamplerState();
};

//--------------------------------------------------

class D3D11Texture : public Texture
{
    friend class D3D11OutputDuplicator;
    friend class D3D11System;

    ID3D11Texture2D          *texture = nullptr;
    ID3D11ShaderResourceView *resource = nullptr;
    ID3D11RenderTargetView   *renderTarget = nullptr;

    UINT width = 0, height = 0;
    GSColorFormat format;
    IDXGISurface1 *surface = nullptr;
    bool bGDICompatible = false;
    bool bDynamic = false;
    bool mIsReadOnly = false;
    static Texture* CreateFromSharedHandle(unsigned int width, unsigned int height, HANDLE handle);
    static Texture* CreateTexture(unsigned int width, unsigned int height, GSColorFormat colorFormat, void *lpData, BOOL bGenMipMaps, BOOL bStatic);
    static Texture* CreateFromFile(CTSTR lpFile, BOOL bBuildMipMaps);
    static Texture* CreateRenderTarget(unsigned int width, unsigned int height, GSColorFormat colorFormat, BOOL bGenMipMaps);
    static Texture* CreateGDITexture(unsigned int width, unsigned int height);
    static Texture* CreateShared(unsigned int width, unsigned int height);
    static Texture* CreateReadTexture(unsigned int width, unsigned int height, GSColorFormat colorFormat); 
public:
    D3D11Texture();
    ~D3D11Texture();

    virtual DWORD Width() const;
    virtual DWORD Height() const;
    virtual BOOL HasAlpha() const;
    virtual void SetImage(void *lpData, GSImageFormat imageFormat, UINT pitch);
    virtual bool Map(BYTE *&lpData, UINT &pitch);
    virtual void Unmap();
    virtual GSColorFormat GetFormat() const;

    virtual bool GetDC(HDC &hDC);
    virtual void ReleaseDC();

    LPVOID GetD3DTexture() {return texture;}
    virtual HANDLE GetSharedHandle();
};

//=============================================================================

struct ShaderParam
{
    ShaderParameterType type;
    String name;

    UINT samplerID;
    UINT textureID;

    int arrayCount;

    List<BYTE> curValue;
    List<BYTE> defaultValue;
    BOOL bChanged;

    inline ~ShaderParam() {FreeData();}

    inline void FreeData()
    {
        name.Clear();
        curValue.Clear();
        defaultValue.Clear();
    }
};

struct ShaderSampler
{
    String name;
    SamplerState *sampler = nullptr;

    inline ~ShaderSampler() {FreeData();}

    inline void FreeData()
    {
        name.Clear();
        delete sampler;
    }
};

//--------------------------------------------------

struct ShaderProcessor : CodeTokenizer
{
    BOOL ProcessShader(CTSTR input, CTSTR filename);
    BOOL AddState(SamplerInfo &info, String &stateName, String &stateVal);

    UINT nTextures;
    List<ShaderSampler> Samplers;
    List<ShaderParam>   Params;

    List<D3D11_INPUT_ELEMENT_DESC> generatedLayout;

    bool bHasNormals;
    bool bHasColors;
    bool bHasTangents;
    UINT numTextureCoords;

    inline ShaderProcessor()  {zero(this, sizeof(ShaderProcessor));}
    inline ~ShaderProcessor() {FreeData();}

    inline void FreeData()
    {
        UINT i;
        for(i=0; i<Samplers.Num(); i++)
            Samplers[i].FreeData();
        Samplers.Clear();
        for(i=0; i<Params.Num(); i++)
            Params[i].FreeData();
        Params.Clear();
    }

    inline UINT GetSamplerID(CTSTR lpSampler)
    {
        for(UINT i=0; i<Samplers.Num(); i++)
        {
            if(Samplers[i].name.Compare(lpSampler))
                return i;
        }

        return INVALID;
    }
};

//--------------------------------------------------

class D3D11Shader : public Shader
{
    friend class D3D11System;

    List<ShaderParam>   Params;
    List<ShaderSampler> Samplers;

    ID3D11Buffer *constantBuffer = nullptr;
    UINT constantSize;

protected:
    bool ProcessData(ShaderProcessor &processor, CTSTR lpFileName);

    void UpdateParams();

public:
    ~D3D11Shader();

    virtual int    NumParams() const;
    virtual HANDLE GetParameter(UINT parameter) const;
    virtual HANDLE GetParameterByName(CTSTR lpName) const;
    virtual void   GetParameterInfo(HANDLE hObject, ShaderParameterInfo &paramInfo) const;

    virtual void   LoadDefaults();

    virtual void   SetBool(HANDLE hObject, BOOL bValue);
    virtual void   SetFloat(HANDLE hObject, float fValue);
    virtual void   SetInt(HANDLE hObject, int iValue);
    virtual void   SetMatrix(HANDLE hObject, float *matrix);
    virtual void   SetVector(HANDLE hObject, const Vect &value);
    virtual void   SetVector2(HANDLE hObject, const Vect2 &value);
    virtual void   SetVector4(HANDLE hObject, const Vect4 &value);
    virtual void   SetTexture(HANDLE hObject, BaseTexture *texture);
    virtual void   SetValue(HANDLE hObject, const void *val, DWORD dwSize);
};

//--------------------------------------------------

class D3D11VertexShader : public D3D11Shader
{
    friend class D3D11System;
    friend class D3D11VertexBuffer;

    ID3D11VertexShader *vertexShader = nullptr;
    ID3D11InputLayout  *inputLayout = nullptr;

    bool bHasNormals = false;
    bool bHasColors = false;
    bool bHasTangents = false;
    UINT nTextureCoords;

    inline UINT NumBuffersExpected() const
    {
        UINT count = 1;
        if(bHasNormals)  count++;
        if(bHasColors)   count++;
        if(bHasTangents) count++;
        count += nTextureCoords;

        return count;
    }

    static Shader* CreateVertexShaderFromBlob(std::vector<char> const &blob, CTSTR lpShader, CTSTR lpFileName);
    static Shader* CreateVertexShader(CTSTR lpShader, CTSTR lpFileName);
    static void CreateVertexShaderBlob(std::vector<char> &blob, CTSTR lpShader, CTSTR lpFileName);

public:
    ~D3D11VertexShader();

    virtual ShaderType GetType() const {return ShaderType_Vertex;}
};

//--------------------------------------------------

class D3D11PixelShader : public D3D11Shader
{
    friend class D3D11System;

    ID3D11PixelShader *pixelShader = nullptr;

    static Shader* CreatePixelShaderFromBlob(ShaderBlob const &blob, CTSTR lpShader, CTSTR lpFileName);
    static Shader* CreatePixelShader(CTSTR lpShader, CTSTR lpFileName);
    static void CreatePixelShaderBlob(ShaderBlob &blob, CTSTR lpShader, CTSTR lpFileName);

public:
    ~D3D11PixelShader();

    virtual ShaderType GetType() const {return ShaderType_Pixel;}
};

//--------------------------------------------------

class D3D11OutputDuplicator : public OutputDuplicator
{
   IDXGIOutputDuplication *duplicator = nullptr;
    Texture *copyTex = nullptr;

    POINT cursorPos;
    Texture *cursorTex = nullptr;
    BOOL bCursorVis = false;

public:
    bool Init(UINT output);
    virtual ~D3D11OutputDuplicator();

    virtual DuplicatorInfo AcquireNextFrame(UINT timeout);
    virtual Texture* GetCopyTexture();
    virtual Texture* GetCursorTex(POINT* pos);
};

class D3D11RenderView:public OBSRenderView{
public :
   D3D11RenderView();
   ~D3D11RenderView();
   void Init(HWND hwnd,int cx,int cy,D3D11System *gs);
   virtual void *GetRenderView();
   virtual void *GetRenderSwap();
private:
   HWND mHwnd = NULL;
   int mWidth = 0;
   int mHeight = 0;   
   ComPtr<IDXGISwapChain> swap;
   ComPtr<ID3D11RenderTargetView> swapRenderView;
};

//=============================================================================

struct SavedBlendState
{
    GSBlendType srcFactor, destFactor;
    ID3D11BlendState *blendState = nullptr;;
};

class D3D11System : public GraphicsSystem
{
    friend class D3D11VertexShader;
    friend class D3D11PixelShader;
    friend class D3D11RenderView;
    D3D_DRIVER_TYPE driverType;
    D3D_FEATURE_LEVEL featureLevel;
    ID3D11Device           *d3d = nullptr;
    ID3D11DeviceContext* d3dContext = nullptr;

    IDXGISwapChain          *swap = nullptr;
    ID3D11RenderTargetView  *swapRenderView = nullptr;

    ID3D11DepthStencilState *depthState = nullptr;
    ID3D11RasterizerState   *rasterizerState = nullptr;
    ID3D11RasterizerState   *scissorState = nullptr;

    bool bDisableCompatibilityMode;

    //---------------------------

    D3D11Texture            *curRenderTarget = nullptr;
    D3D11Texture            *curTextures[8] = { nullptr };
    D3D11SamplerState       *curSamplers[8] = { nullptr };
    D3D11VertexBuffer       *curVertexBuffer = nullptr;
    D3D11VertexShader       *curVertexShader = nullptr;
    D3D11PixelShader        *curPixelShader = nullptr;

    D3D11_PRIMITIVE_TOPOLOGY curTopology;

    List<SavedBlendState>   blends;
    ID3D11BlendState        *curBlendState = nullptr;;
    ID3D11BlendState        *disabledBlend = nullptr;;
    BOOL                    bBlendingEnabled = false;
    ComPtr<IDXGIFactory1>   factory;

    //---------------------------

    VertexBuffer            *spriteVertexBuffer = nullptr, *boxVertexBuffer = nullptr;

    //---------------------------
    IDXGISwapChain          *swap_bak = nullptr;
    ID3D11RenderTargetView  *swapRenderView_bak = nullptr;

    float                   curProjMatrix[16];
    float                   curViewMatrix[16];
    float                   curViewProjMatrix[16];

    float                   curBlendFactor[4];

    float                   curCropping[4];

    virtual void ResetViewMatrix();

    virtual void ResizeView();
    virtual void UnloadAllData();

    virtual void CreateVertexShaderBlob(std::vector<char> &blob, CTSTR lpShader, CTSTR lpFileName) override;
    virtual void CreatePixelShaderBlob(std::vector<char> &blob, CTSTR lpShader, CTSTR lpFileName) override;
    int mRenderWidth;
    int mRenderHeight;
    D3D11RenderView *currentView = NULL;
public:
    D3D11System(HWND renderHwnd,int renderWidth,int renderHeight,bool &ok);
    ~D3D11System();    
    
    virtual long GetDeviceRemovedReason();
    virtual void Flush();
    virtual void Present();
    virtual LPVOID GetDevice();
    virtual LPVOID GetContent();
    virtual void CopyTexture(Texture *texDest, Texture *texSrc);
    virtual void Init();


    ////////////////////////////
    //Texture Functions
    virtual Texture*        CreateTextureFromSharedHandle(unsigned int width, unsigned int height, HANDLE handle);
    virtual Texture*        CreateTexture(unsigned int width, unsigned int height, GSColorFormat colorFormat, void *lpData, BOOL bBuildMipMaps, BOOL bStatic);
    virtual Texture*        CreateTextureFromFile(CTSTR lpFile, BOOL bBuildMipMaps);
    virtual Texture*        CreateRenderTarget(unsigned int width, unsigned int height, GSColorFormat colorFormat, BOOL bGenMipMaps);
    virtual Texture*        CreateGDITexture(unsigned int width, unsigned int height);

    virtual bool            GetTextureFileInfo(CTSTR lpFile, TextureInfo &info);

    virtual SamplerState*   CreateSamplerState(SamplerInfo &info);

    virtual UINT            GetNumOutputs();
    virtual OutputDuplicator *CreateOutputDuplicator(UINT outputID);


    ////////////////////////////
    //Shader Functions
    virtual Shader*         CreateVertexShaderFromBlob(ShaderBlob const &blob, CTSTR lpShader, CTSTR lpFileName) override;
    virtual Shader*         CreatePixelShaderFromBlob(ShaderBlob const &blob, CTSTR lpShader, CTSTR lpFileName) override;
    virtual Shader*         CreateVertexShader(CTSTR lpShader, CTSTR lpFileName);
    virtual Shader*         CreatePixelShader(CTSTR lpShader, CTSTR lpFileName);


    ////////////////////////////
    //Vertex Buffer Functions
    virtual VertexBuffer* CreateVertexBuffer(VBData *vbData, BOOL bStatic=1);


    ////////////////////////////
    //Main Rendering Functions
    virtual void  LoadVertexBuffer(VertexBuffer* vb);
    virtual void  LoadTexture(Texture *texture, UINT idTexture=0);
    virtual void  LoadSamplerState(SamplerState *sampler, UINT idSampler=0);
    virtual void  LoadVertexShader(Shader *vShader);
    virtual void  LoadPixelShader(Shader *pShader);

    virtual Shader* GetCurrentPixelShader();
    virtual Shader* GetCurrentVertexShader();

    virtual void  SetRenderTarget(Texture *texture);
    virtual void  Draw(GSDrawMode drawMode, DWORD startVert=0, DWORD nVerts=0);


    ////////////////////////////
    //Drawing mode functions
    virtual void  EnableBlending(BOOL bEnable);
    virtual void  BlendFunction(GSBlendType srcFactor, GSBlendType destFactor, float fFactor);

    virtual void  ClearColorBuffer(DWORD color=0xFF000000);


    ////////////////////////////
    //Other Functions
    void  Ortho(float left, float right, float top, float bottom, float znear, float zfar);
    void  Frustum(float left, float right, float top, float bottom, float znear, float zfar);

    virtual void  SetViewport(float x, float y, float width, float height);

    virtual void  SetScissorRect(XRect *pRect=NULL);


    virtual void  DrawSpriteEx(Texture *texture, DWORD color, float x, float y, float x2, float y2, float u, float v, float u2, float v2);
    virtual void  DrawBox(const Vect2 &upperLeft, const Vect2 &size);
    virtual void  SetCropping(float left, float top, float right, float bottom);
    virtual void  DrawSpriteExRotate(Texture *texture, DWORD color, float x, float y, float x2, float y2, float degrees, float u, float v, float u2, float v2, float texDegrees);

    // To prevent breaking the API, put this at the end instead of with the other Texture functions
    virtual Texture*        CreateSharedTexture(unsigned int width, unsigned int height);
    virtual Texture*       CreateReadTexture(unsigned int width, unsigned int height, GSColorFormat colorFormat);    
    virtual OBSRenderView *CreateRenderTargetView(HWND hwnd,int cx,int cy);
    virtual void DestoryRenderTargetView(OBSRenderView *view);
    virtual void SetRenderTargetView(OBSRenderView *view);
};

bool D3D11ResourceMap(ID3D11Resource *resource, BYTE *&lpData, UINT &pitch, D3D11_MAP mapType);
void D3D11ResourceUnMap(ID3D11Resource *resource);
ID3D11DeviceContext *GetContent();



//----------------------------------------------------------------------------
extern "C" __declspec(dllexport) bool GraphicsInit(HWND mRenderWidgetHwnd,int renderWidth,int renderHeight,ConfigFile *);
extern "C" __declspec(dllexport) void GraphicsDestory();

