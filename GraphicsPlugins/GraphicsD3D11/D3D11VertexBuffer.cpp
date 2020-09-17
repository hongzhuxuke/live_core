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
#include <DXGI.h>
#include <dxgi1_2.h>
#include "D3D11System.h"
//#define SafeRelease(punk)  \
//if ((punk) != NULL)  \
//{ (punk)->Release(); (punk) = NULL; }

VertexBuffer* D3D11VertexBuffer::CreateVertexBuffer(VBData *vbData, BOOL bStatic)
{
    if(!vbData)
    {
        AppWarning(TEXT("D3D11VertexBuffer::CreateVertexBuffer: vbData NULL"));
        return NULL;
    }

    HRESULT err;

    D3D11VertexBuffer *buf = new D3D11VertexBuffer;
    buf->numVerts = vbData->VertList.Num();

    D3D11_BUFFER_DESC bd;
    D3D11_SUBRESOURCE_DATA srd;
    zero(&bd, sizeof(bd));
    zero(&srd, sizeof(srd));

    bd.Usage = bStatic ? D3D11_USAGE_DEFAULT : D3D11_USAGE_DYNAMIC;
    bd.CPUAccessFlags = bStatic ? 0 : D3D11_CPU_ACCESS_WRITE;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    //----------------------------------------

    buf->vertexSize = sizeof(Vect);
    bd.ByteWidth = sizeof(Vect)*buf->numVerts;
    srd.pSysMem = vbData->VertList.Array();
    
    ID3D11Device *d3d=static_cast<ID3D11Device *>(GetD3D());
    err = d3d->CreateBuffer(&bd, &srd, &buf->vertexBuffer);
    if(FAILED(err))
    {
        AppWarning(TEXT("D3D11VertexBuffer::CreateVertexBuffer: Failed to create the vertex portion of the vertex buffer, result = %08lX"), err);
        delete buf;
        return NULL;
    }

    //----------------------------------------

    if(vbData->NormalList.Num())
    {
        buf->normalSize = sizeof(Vect);
        bd.ByteWidth = sizeof(Vect)*buf->numVerts;
        srd.pSysMem = vbData->NormalList.Array();
        err = d3d->CreateBuffer(&bd, &srd, &buf->normalBuffer);
        if(FAILED(err))
        {
            AppWarning(TEXT("D3D11VertexBuffer::CreateVertexBuffer: Failed to create the normal portion of the vertex buffer, result = %08lX"), err);
            delete buf;
            return NULL;
        }
    }

    //----------------------------------------

    if(vbData->ColorList.Num())
    {
        buf->colorSize = sizeof(DWORD);
        bd.ByteWidth = sizeof(DWORD)*buf->numVerts;
        srd.pSysMem = vbData->ColorList.Array();
        err = d3d->CreateBuffer(&bd, &srd, &buf->colorBuffer);
        if(FAILED(err))
        {
            AppWarning(TEXT("D3D11VertexBuffer::CreateVertexBuffer: Failed to create the color portion of the vertex buffer, result = %08lX"), err);
            delete buf;
            return NULL;
        }
    }

    //----------------------------------------

    if(vbData->TangentList.Num())
    {
        buf->tangentSize = sizeof(Vect);
        bd.ByteWidth = sizeof(Vect)*buf->numVerts;
        srd.pSysMem = vbData->TangentList.Array();
        err = d3d->CreateBuffer(&bd, &srd, &buf->tangentBuffer);
        if(FAILED(err))
        {
            AppWarning(TEXT("D3D11VertexBuffer::CreateVertexBuffer: Failed to create the tangent portion of the vertex buffer, result = %08lX"), err);
            delete buf;
            return NULL;
        }
    }

    //----------------------------------------

    if(vbData->UVList.Num())
    {
        buf->UVBuffers.SetSize(vbData->UVList.Num());
        buf->UVSizes.SetSize(vbData->UVList.Num());

        for(UINT i=0; i<vbData->UVList.Num(); i++)
        {
            List<UVCoord> &textureVerts = vbData->UVList[i];

            buf->UVSizes[i] = sizeof(UVCoord);
            bd.ByteWidth = buf->UVSizes[i]*buf->numVerts;
            srd.pSysMem = textureVerts.Array();

            ID3D11Buffer *tvBuffer;
            err = d3d->CreateBuffer(&bd, &srd, &tvBuffer);
            if(FAILED(err))
            {
                AppWarning(TEXT("D3D11VertexBuffer::CreateVertexBuffer: Failed to create the texture vertex %d portion of the vertex buffer, result = %08lX"), i, err);
                delete buf;
                return NULL;
            }

            buf->UVBuffers[i] = tvBuffer;
        }
    }

    //----------------------------------------

    buf->bDynamic = !bStatic;

    if(bStatic)
    {
        delete vbData;
        buf->data = NULL;
    }
    else
        buf->data = vbData;

    return buf;
}


D3D11VertexBuffer::~D3D11VertexBuffer()
{
    for(UINT i=0; i<UVBuffers.Num(); i++)
        SafeRelease(UVBuffers[i]);

    SafeRelease(tangentBuffer);
    SafeRelease(colorBuffer);
    SafeRelease(normalBuffer);
    SafeRelease(vertexBuffer);

    delete data;
}
ID3D11DeviceContext *GetContent()  {
   ID3D11DeviceContext *d3dContent = static_cast<ID3D11DeviceContext *>(GetD3DContent());
   return d3dContent;
}

bool D3D11ResourceMap(ID3D11Resource *resource, BYTE *&lpData, UINT &pitch, D3D11_MAP mapType) {

   ID3D11DeviceContext *d3dContent = static_cast<ID3D11DeviceContext *>(GetD3DContent());
   if (d3dContent == NULL) {
      return false;
   }
   HRESULT err;
   D3D11_MAPPED_SUBRESOURCE msr;
   if (FAILED(err = d3dContent->Map(resource, 0, mapType, 0, &msr))) {

      return false;

   }

   lpData = (BYTE*)msr.pData;
   pitch = msr.RowPitch;

   return true;
}
void D3D11ResourceUnMap(ID3D11Resource *resource) {
   ID3D11DeviceContext *d3dContent = static_cast<ID3D11DeviceContext *>(GetD3DContent());
   if (d3dContent == NULL) {
      return;
   }
   d3dContent->Unmap(resource, 0);
}
void D3D11VertexBuffer::FlushBuffers()
{
    if(!bDynamic)
    {
        AppWarning(TEXT("D3D11VertexBuffer::FlushBuffers: Cannot flush buffers on a non-dynamic vertex buffer"));
        return;
    }

    BYTE *outData;
    UINT pitch;
    if (!D3D11ResourceMap(vertexBuffer, outData, pitch, D3D11_MAP_WRITE_DISCARD))
    {
        AppWarning(TEXT("D3D11VertexBuffer::FlushBuffers: failed to map vertex buffer"));
        return;
    }

    mcpy(outData, data->VertList.Array(), sizeof(Vect)*numVerts);

    D3D11ResourceUnMap(vertexBuffer);
    //---------------------------------------------------

    if(normalBuffer)
    {
        if (!D3D11ResourceMap(normalBuffer, outData, pitch, D3D11_MAP_WRITE_DISCARD))
        {
            AppWarning(TEXT("D3D11VertexBuffer::FlushBuffers: failed to map normal buffer, result"));
            return;
        }

        mcpy(outData, data->NormalList.Array(), sizeof(Vect)*numVerts);
        D3D11ResourceUnMap(normalBuffer);
    }

    //---------------------------------------------------

    if(colorBuffer)
    {
        if (!D3D11ResourceMap(colorBuffer, outData, pitch, D3D11_MAP_WRITE_DISCARD))
        {
            AppWarning(TEXT("D3D11VertexBuffer::FlushBuffers: failed to map color buffer"));
            return;
        }

        mcpy(outData, data->ColorList.Array(), sizeof(Vect)*numVerts);
        D3D11ResourceUnMap(colorBuffer);
    }
    //---------------------------------------------------

    if(tangentBuffer)
    {
        if (!D3D11ResourceMap(tangentBuffer, outData, pitch, D3D11_MAP_WRITE_DISCARD))
        {
            AppWarning(TEXT("D3D11VertexBuffer::FlushBuffers: failed to map tangent buffer"));
            return;
        }

        mcpy(outData, data->TangentList.Array(), sizeof(Vect)*numVerts);
        D3D11ResourceUnMap(tangentBuffer);
    }

    //---------------------------------------------------
    if(UVBuffers.Num())
    {
        for(UINT i=0; i<UVBuffers.Num(); i++)
        {
            List<UVCoord> &textureVerts = data->UVList[i];

            ID3D11Buffer *buffer = UVBuffers[i];
            if (!D3D11ResourceMap(buffer, outData, pitch, D3D11_MAP_WRITE_DISCARD))
            {
                AppWarning(TEXT("D3D11VertexBuffer::FlushBuffers: failed to map texture vertex buffer %d"), i);
                return;
            }
            mcpy(outData, textureVerts.Array(), sizeof(UVCoord)*numVerts);
            D3D11ResourceUnMap(buffer);
        }
    }
}

VBData* D3D11VertexBuffer::GetData()
{
    if(!bDynamic)
    {
        AppWarning(TEXT("D3D11VertexBuffer::GetData: Cannot get vertex data of a non-dynamic vertex buffer"));
        return NULL;
    }

    return data;
}

void D3D11VertexBuffer::MakeBufferList(D3D11VertexShader *vShader, List<ID3D11Buffer*> &bufferList, List<UINT> &strides) const
{
    assert(vShader);

    bufferList << vertexBuffer;
    strides << vertexSize;

    if(vShader->bHasNormals)
    {
        if(normalBuffer)
        {
            bufferList << normalBuffer;
            strides << normalSize;
        }
        else
            AppWarning(TEXT("Trying to use a vertex buffer without normals with a vertex shader that requires normals"));
    }

    if(vShader->bHasColors)
    {
        if(colorBuffer)
        {
            bufferList << colorBuffer;
            strides << colorSize;
        }
        else
            AppWarning(TEXT("Trying to use a vertex buffer without colors with a vertex shader that requires colors"));
    }

    if(vShader->bHasTangents)
    {
        if(tangentBuffer)
        {
            bufferList << tangentBuffer;
            strides << tangentSize;
        }
        else
            AppWarning(TEXT("Trying to use a vertex buffer without tangents with a vertex shader that requires tangents"));
    }

    if(vShader->nTextureCoords <= UVBuffers.Num())
    {
        for(UINT i=0; i<vShader->nTextureCoords; i++)
        {
            bufferList << UVBuffers[i];
            strides << UVSizes[i];
        }
    }
    else
        AppWarning(TEXT("Trying to use a vertex buffer with insufficient texture coordinates compared to the vertex shader requirements"));
}
