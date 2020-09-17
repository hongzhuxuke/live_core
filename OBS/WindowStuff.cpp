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
#include <shellapi.h>
#include <ShlObj.h>
#include <uxtheme.h>
#include <vsstyle.h>
#include <MMSystem.h>

#include <memory>
#include <vector>

ItemModifyType GetItemModifyType(const Vect2 &mousePos, const Vect2 &itemPos, const Vect2 &itemSize, const Vect4 &crop, const Vect2 &scaleVal)
{
    Vect2 lowerRight = itemPos+itemSize;
    float epsilon = 10.0f;

    Vect2 croppedItemPos = itemPos + Vect2(crop.x / scaleVal.x, crop.y / scaleVal.y);
    Vect2 croppedLowerRight = lowerRight - Vect2(crop.w / scaleVal.x, crop.z / scaleVal.y);

    if( mousePos.x < croppedItemPos.x    ||
        mousePos.y < croppedItemPos.y    ||
        mousePos.x > croppedLowerRight.x ||
        mousePos.y > croppedLowerRight.y )
    {
        return ItemModifyType_None;
    }    

    // Corner sizing
    if(mousePos.CloseTo(croppedItemPos, epsilon))
        return ItemModifyType_ScaleTopLeft;
    else if(mousePos.CloseTo(croppedLowerRight, epsilon))
        return ItemModifyType_ScaleBottomRight;
    else if(mousePos.CloseTo(Vect2(croppedLowerRight.x, croppedItemPos.y), epsilon))
        return ItemModifyType_ScaleTopRight;
    else if(mousePos.CloseTo(Vect2(croppedItemPos.x, croppedLowerRight.y), epsilon))
        return ItemModifyType_ScaleBottomLeft;

    epsilon = 4.0f;

    // Edge sizing
    if(CloseFloat(mousePos.x, croppedItemPos.x, epsilon))
        return ItemModifyType_ScaleLeft;
    else if(CloseFloat(mousePos.x, croppedLowerRight.x, epsilon))
        return ItemModifyType_ScaleRight;
    else if(CloseFloat(mousePos.y, croppedItemPos.y, epsilon))
        return ItemModifyType_ScaleTop;
    else if(CloseFloat(mousePos.y, croppedLowerRight.y, epsilon))
        return ItemModifyType_ScaleBottom;


    return ItemModifyType_Move;
}

/**
 * Maps a point in window coordinates to frame coordinates.
 */
Vect2 OBS::MapWindowToFramePos(Vect2 mousePos)
{
    if(renderFrameIn1To1Mode)
        return (mousePos - GetRenderFrameOffset()) * (GetBaseSize() / GetOutputSize());
    return (mousePos - GetRenderFrameOffset()) * (GetBaseSize() / GetRenderFrameSize());
}

/**
 * Maps a point in frame coordinates to window coordinates.
 */
Vect2 OBS::MapFrameToWindowPos(Vect2 framePos)
{
    if(renderFrameIn1To1Mode)
        return framePos * (GetOutputSize() / GetBaseSize()) + GetRenderFrameOffset();
    return framePos * (GetRenderFrameSize() / GetBaseSize()) + GetRenderFrameOffset();
}

/**
 * Maps a size in window coordinates to frame coordinates.
 */
Vect2 OBS::MapWindowToFrameSize(Vect2 windowSize)
{
    if(renderFrameIn1To1Mode)
        return windowSize * (GetBaseSize() / GetOutputSize());
    return windowSize * (GetBaseSize() / GetRenderFrameSize());
}

/**
 * Maps a size in frame coordinates to window coordinates.
 */
Vect2 OBS::MapFrameToWindowSize(Vect2 frameSize)
{
    if(renderFrameIn1To1Mode)
        return frameSize * (GetOutputSize() / GetBaseSize());
    return frameSize * (GetRenderFrameSize() / GetBaseSize());
}

/**
 * Returns the scale of the window relative to the actual frame size. E.g.
 * if the window is twice the size of the frame this will return "0.5".
 */
Vect2 OBS::GetWindowToFrameScale()
{
    return MapWindowToFrameSize(Vect2(1.0f, 1.0f));
}

/**
 * Returns the scale of the frame relative to the window size. E.g.
 * if the window is twice the size of the frame this will return "2.0".
 */
Vect2 OBS::GetFrameToWindowScale()
{
    return MapFrameToWindowSize(Vect2(1.0f, 1.0f));
}

bool OBS::EnsureCropValid(SceneItem *&scaleItem, Vect2 &minSize, Vect2 &snapSize, bool bControlDown, int cropEdges, bool cropSymmetric) 
{
    Vect2 scale = (scaleItem->GetSource() ? scaleItem->GetSource()->GetSize() : scaleItem->GetSize()) / scaleItem->GetSize();

    // When keep aspect is on, cropping can only be half size - 2 * minsize
    if (cropSymmetric)
    {
        if (cropEdges & (edgeLeft | edgeRight))
        {
            if (scaleItem->GetCrop().x > (scaleItem->size.x / 2 ) - 2 * minSize.x)
            {
                scaleItem->crop.x = ((scaleItem->size.x / 2 ) - 2 * minSize.x) * scale.x;
                scaleItem->crop.w = ((scaleItem->size.x / 2 ) - 2 * minSize.x) * scale.x;
            }
            scaleItem->crop.x = (scaleItem->crop.x < 0.0f) ? 0.0f : scaleItem->crop.x;
            scaleItem->crop.w = (scaleItem->crop.w < 0.0f) ? 0.0f : scaleItem->crop.w;
        }
        if (cropEdges & (edgeTop | edgeBottom))
        {
            if (scaleItem->GetCrop().y > (scaleItem->size.y / 2 ) - 2 * minSize.y)
            {
                scaleItem->crop.y = ((scaleItem->size.y / 2 ) - 2 * minSize.y) * scale.y;
                scaleItem->crop.z = ((scaleItem->size.y / 2 ) - 2 * minSize.y) * scale.y;
            }
            scaleItem->crop.y = (scaleItem->crop.y < 0.0f) ? 0.0f : scaleItem->crop.y;
            scaleItem->crop.z = (scaleItem->crop.z < 0.0f) ? 0.0f : scaleItem->crop.z;
        }
    }
    else 
    {
        // left
        if (scaleItem->GetCrop().x > (scaleItem->size.x - scaleItem->GetCrop().w - 32) - minSize.x && cropEdges & edgeLeft)
        {
            scaleItem->crop.x = ((scaleItem->size.x - scaleItem->GetCrop().w - 32) - minSize.x) * scale.x;
        }
        scaleItem->crop.x = (scaleItem->crop.x < 0.0f) ? 0.0f : scaleItem->crop.x;

        // top
        if (scaleItem->GetCrop().y > (scaleItem->size.y - scaleItem->GetCrop().z - 32) - minSize.y && cropEdges & edgeTop)
        {
            scaleItem->crop.y = ((scaleItem->size.y - scaleItem->GetCrop().z - 32) - minSize.y) * scale.y;
        }
        scaleItem->crop.y = (scaleItem->crop.y < 0.0f) ? 0.0f : scaleItem->crop.y;

        // right
        if (scaleItem->GetCrop().w > (scaleItem->size.x - scaleItem->GetCrop().x - 32) - minSize.x && cropEdges & edgeRight)
        {
            scaleItem->crop.w = ((scaleItem->size.x - scaleItem->GetCrop().x - 32) - minSize.x) * scale.x;
        }
        scaleItem->crop.w = (scaleItem->crop.w < 0.0f) ? 0.0f : scaleItem->crop.w;

        // bottom
        if (scaleItem->GetCrop().z > (scaleItem->size.y - scaleItem->GetCrop().y - 32) - minSize.y && cropEdges & edgeBottom)
        {
            scaleItem->crop.z = ((scaleItem->size.y - scaleItem->GetCrop().y - 32) - minSize.y) * scale.y;
        }
        scaleItem->crop.z = (scaleItem->crop.z < 0.0f) ? 0.0f : scaleItem->crop.z;
    }
    if (!bControlDown) 
    {
        // left
        if(CloseFloat(scaleItem->GetCrop().x, 0.0f, snapSize.x))
        {
            scaleItem->crop.x = 0.0f;
        }
        // top
        if(CloseFloat(scaleItem->GetCrop().y, 0.0f, snapSize.y))
        {
            scaleItem->crop.y = 0.0f;
        }
        // right
        if(CloseFloat(scaleItem->GetCrop().w, 0.0f, snapSize.x))
        {
            scaleItem->crop.w = 0.0f;
        }
        // bottom
        if(CloseFloat(scaleItem->GetCrop().z, 0.0f, snapSize.y))
        {
            scaleItem->crop.z = 0.0f;
        }
    }

    return true;
}
