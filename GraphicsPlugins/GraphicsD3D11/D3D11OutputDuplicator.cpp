/********************************************************************************
 Copyright (C) 2012 Hugh Bailey <obs.jim@gmail.com>

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
********************************************************************************/
#include "D3D11System.h"


#define DXGI_ERROR_ACCESS_LOST      _HRESULT_TYPEDEF_(0x887A0026L)
#define DXGI_ERROR_WAIT_TIMEOUT     _HRESULT_TYPEDEF_(0x887A0027L)

//#define SafeRelease(punk)  \
//if ((punk) != NULL)  \
//{ (punk)->Release(); (punk) = NULL; }

bool D3D11OutputDuplicator::Init(UINT output)
{
    HRESULT hRes;

    bool bSuccess = false;

    IDXGIDevice *device;
    ID3D11Device *d3d=static_cast<ID3D11Device *>(GetD3D());
    if(SUCCEEDED(hRes = d3d->QueryInterface(__uuidof(IDXGIDevice), (void**)&device)))
    {
        IDXGIAdapter *adapter;
        if(SUCCEEDED(hRes = device->GetAdapter(&adapter)))
        {
            IDXGIOutput *outputInterface;
            if(SUCCEEDED(hRes = adapter->EnumOutputs(output, &outputInterface)))
            {
                IDXGIOutput1 *output1;

                if(SUCCEEDED(hRes = outputInterface->QueryInterface(__uuidof(IDXGIOutput1), (void**)&output1)))
                {
                    if(SUCCEEDED(hRes = output1->DuplicateOutput(d3d, &duplicator)))
                        bSuccess = true;
                    /*else
                        AppWarning(TEXT("D3D11OutputDuplicator::Init: output1->DuplicateOutput failed, result = %u"), (UINT)hRes);*/

                    output1->Release();
                }
                /*else
                    AppWarning(TEXT("D3D11OutputDuplicator::Init: outputInterface->QueryInterface failed, result = %u"), (UINT)hRes);*/

                outputInterface->Release();
            }
            /*else
                AppWarning(TEXT("D3D11OutputDuplicator::Init: adapter->EnumOutputs failed, result = %u"), (UINT)hRes);*/

            adapter->Release();
        }
        /*else
            AppWarning(TEXT("D3D11OutputDuplicator::Init: device->GetAdapter failed, result = %u"), (UINT)hRes);*/

        device->Release();
    }
    /*else
        AppWarning(TEXT("D3D11OutputDuplicator::Init: GetD3D()->QueryInterface failed, result = %u"), (UINT)hRes);*/

    return bSuccess;
}

D3D11OutputDuplicator::~D3D11OutputDuplicator()
{
    SafeRelease(duplicator);
    delete copyTex;
}
static inline GSColorFormat ConvertGIBackBufferFormat(DXGI_FORMAT format) {
   switch (format) {
   case DXGI_FORMAT_R10G10B10A2_UNORM: return GS_R10G10B10A2;
   case DXGI_FORMAT_R8G8B8A8_UNORM:    return GS_RGBA;
   case DXGI_FORMAT_B8G8R8A8_UNORM:    return GS_BGRA;
   case DXGI_FORMAT_B8G8R8X8_UNORM:    return GS_BGR;
   case DXGI_FORMAT_B5G5R5A1_UNORM:    return GS_B5G5R5A1;
   case DXGI_FORMAT_B5G6R5_UNORM:      return GS_B5G6R5;
   }

   return GS_UNKNOWNFORMAT;
}
DuplicatorInfo D3D11OutputDuplicator::AcquireNextFrame(UINT timeout)
{
    if(!duplicator)
    {
        AppWarning(TEXT("D3D11OutputDuplicator::AcquireNextFrame: Well, apparently there's no duplicator."));
        return DuplicatorInfo_Error;
    }

    //------------------------------------------

    DXGI_OUTDUPL_FRAME_INFO frameInfo;
    IDXGIResource *tempResource = NULL;

    HRESULT hRes = duplicator->AcquireNextFrame(timeout, &frameInfo, &tempResource);
    if(hRes == DXGI_ERROR_ACCESS_LOST)
        return DuplicatorInfo_Lost;
    else if(hRes == DXGI_ERROR_WAIT_TIMEOUT)
        return DuplicatorInfo_Timeout;
    else if(FAILED(hRes))
        return DuplicatorInfo_Error;

    //------------------------------------------

    ID3D11Texture2D *texVal;
    if(FAILED(hRes = tempResource->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&texVal)))
    {
        SafeRelease(tempResource);
        AppWarning(TEXT("D3D11OutputDuplicator::AcquireNextFrame: could not query interface, result = 0x%08lX"), hRes);
        return DuplicatorInfo_Error;
    }

    tempResource->Release();

    //------------------------------------------

    D3D11_TEXTURE2D_DESC texDesc;
    texVal->GetDesc(&texDesc);

    if(!copyTex || copyTex->Width() != texDesc.Width || copyTex->Height() != texDesc.Height)
    {
        delete copyTex;
        copyTex = CreateTexture(texDesc.Width, texDesc.Height, ConvertGIBackBufferFormat(texDesc.Format), NULL, FALSE, TRUE);
    }

    //------------------------------------------

    if(copyTex)
    {
        D3D11Texture *d3dCopyTex = (D3D11Texture*)copyTex;
        GetContent()->CopyResource(d3dCopyTex->texture, texVal);
    }

    SafeRelease(texVal);
    duplicator->ReleaseFrame();

    return DuplicatorInfo_Acquired;
}

Texture* D3D11OutputDuplicator::GetCopyTexture()
{
    return copyTex;
}

Texture* D3D11OutputDuplicator::GetCursorTex(POINT* pos)
{
    if(pos)
        mcpy(pos, &cursorPos, sizeof(POINT));

    if(bCursorVis)
        return cursorTex;

    return NULL;
}
