#include<stdio.h>
#include<string.h>
#include "ImageEnhance.h"


const uint32_t kKeyPointX[3] = { 0, 85, 240 };
const uint32_t kKeyPointY[3] = { 0, 128, 240 };

#define RED(x) ((x & 0x00FF0000) >> 16)
#define GREEN(x) ((x & 0x0000FF00) >> 8)
#define BLUE(x) (x & 0x000000FF)

#define DN_K1_Q20 3355444// 3355444 * 0.5 = 1677722
#define DN_K2_A_Q20 1572864 //k2 = A - alpha * B
#define DN_K2_B_Q20 1677722 //1677722 * 0.5 = 838861

#define DN_Q 20

ImageEnhance::ImageEnhance():
   mReferenceFrame(nullptr),
   mIsInited(false)
{

}
ImageEnhance::~ImageEnhance() {
   Clear();
}

void ImageEnhance::Clear() {
   mCtx.height = -1;
   mCtx.width = -1;
   mFramePixels = -1;
   if (mReferenceFrame) {
      delete[] mReferenceFrame;
      mReferenceFrame = NULL;
   }
}

bool ImageEnhance::Init(VideoFilterParam * param) {
   mIsInited = false;
   Clear();
   if (param == nullptr) {
#ifdef IS_DEBUG
      printf("[ERROR] Init param is null!\n");
#endif
      return false;
   }
   mCtx.height = param->height;
   mCtx.width = param->width;

   if (mCtx.width <= 0 || mCtx.height <= 0) {
#ifdef IS_DEBUG
      printf("[ERROR] resolution info is invalid !\n");
#endif
      return false;
   }
	mFramePixels = param->width * param->height;
	mReferenceFrame = new pixel[mFramePixels * 3 / 2];
	memset(mReferenceFrame, 0, mFramePixels * 3 / 2);
   mIsInited = true;
	return true;
}

bool ImageEnhance::Denoise(pixel *inputFrame, pixel *outputFrame, PixelFmt type){
   if (!mIsInited) {
      return false;
   }
   if (!inputFrame || !outputFrame) {
      return false;
   }
   if (type == PixelFmt_UNKNOWN || type == PixelFmt_RGB) {
      return false;
   }
	pixel *pYData;
	pixel *pUData;
	pixel *pVData;
	pixel *pYRef;
	pixel *pURef;
	pixel *pVRef;
	pixel *pYOut;
	pixel *pUOut;
	pixel *pVOut;
	int iDiffY;
	int iDiffU;
	int iDiffV;
	int iLumaLength;
	int iChromaLength;
	int i, j;

   if (type == PixelFmt_I420 || type == PixelFmt_YV12) {
      iLumaLength = mFramePixels;
      iChromaLength = iLumaLength >> 2;
      //Y Planar
      for (i = 0; i < mCtx.height; i++)
      {
         pYData = inputFrame + i * mCtx.width;
         pYRef = mReferenceFrame + i * mCtx.width;
         pYOut = outputFrame + i * mCtx.width;
         for (j = 0; j < mCtx.width; j++)
         {
            iDiffY = pYRef[j] - pYData[j];
            if (iDiffY < 4 && iDiffY > -4) {
               pYOut[j] = pYRef[j];
            }
            else if (iDiffY < 6 && iDiffY > 0) {
               pYOut[j] = pYData[j] + (6 - iDiffY);
            }
            else if (iDiffY > -6 && iDiffY < 0) {
               pYOut[j] = pYData[j] - (6 + iDiffY);
            }
            else {
               pYOut[j] = pYData[j];
            }
         }
      }
      //UV Planar
      for (i = 0; i < mCtx.height / 2; i++) {
         pUData = inputFrame + mFramePixels + i * mCtx.width / 2;
         pURef = mReferenceFrame + mFramePixels + i * mCtx.width / 2;
         pUOut = outputFrame + mFramePixels + i * mCtx.width / 2;
         pVData = pUData + iChromaLength;
         pVRef = pURef + iChromaLength;
         pVOut = pUOut + iChromaLength;
         for (j = 0; j < mCtx.width / 2; j++) {
            iDiffU = pURef[j] - pUData[j];
            if (iDiffU < 2 && iDiffU > -2) {
               pUOut[j] = pURef[j];
            }
            else if (iDiffU < 3 && iDiffU > 0) {
               pUOut[j] = pUData[j] + 1;
            }
            else if (iDiffU < 0 && iDiffU > -3) {
               pUOut[j] = pUData[j] - 1;
            }
            else {
               pUOut[j] = pUData[j];
            }
            iDiffV = pVRef[j] - pVData[j];
            if (iDiffV < 2 && iDiffV > -2) {
               pVOut[j] = pVRef[j];
            }
            else if (iDiffV < 3 && iDiffV > 0) {
               pVOut[j] = pVData[j] + 1;
            }
            else if (iDiffV < 0 && iDiffV > -3) {
               pVOut[j] = pVData[j] - 1;
            }
            else {
               pVOut[j] = pVData[j];
            }
         }
      }
   }
   else if (type == PixelFmt_YVYU || type == PixelFmt_YUY2) {
      iLumaLength = mFramePixels;
      iChromaLength = iLumaLength >> 1;
      //YVYU or YUYV Packed
      for (i = 0; i < mCtx.height; i++) {
         pYData = inputFrame + i * mCtx.width * 2;
         pYRef = mReferenceFrame + i * mCtx.width * 2;
         pYOut = outputFrame + i * mCtx.width * 2;
         pUData = pYData + 1;
         pURef = pYRef + 1;
         pUOut = pYOut + 1;
         pVData = pYData + 3;
         pVRef = pYRef + 3;
         pVOut = pYOut + 3;
         for (j = 0; j < mCtx.width ; j = j + 2) {
            int n = j * 2;
            //Y0
            iDiffY = pYRef[n] - pYData[n];
            if (iDiffY < 4 && iDiffY > -4) {
               pYOut[n] = pYRef[n];
            }
            else if (iDiffY < 6 && iDiffY > 0) {
               pYOut[n] = pYData[n] + (6 - iDiffY);
            }
            else if (iDiffY > -6 && iDiffY < 0) {
               pYOut[n] = pYData[n] - (6 + iDiffY);
            }
            else {
               pYOut[n] = pYData[n];
            }
            //Y1
            iDiffY = pYRef[n + 2] - pYData[n + 2];
            if (iDiffY < 4 && iDiffY > -4) {
               pYOut[n + 2] = pYRef[n + 2];
            }
            else if (iDiffY < 6 && iDiffY > 0) {
               pYOut[n + 2] = pYData[n + 2] + (6 - iDiffY);
            }
            else if (iDiffY > -6 && iDiffY < 0) {
               pYOut[n + 2] = pYData[n + 2] - (6 + iDiffY);
            }
            else {
               pYOut[n + 2] = pYData[n + 2];
            }
            //UV
            iDiffU = pURef[n] - pUData[n];
            if (iDiffU < 2 && iDiffU > -2) {
               pUOut[n] = pURef[n];
            }
            else if (iDiffU < 3 && iDiffU > 0) {
               pUOut[n] = pUData[n] + 1;
            }
            else if (iDiffU < 0 && iDiffU > -3) {
               pUOut[n] = pUData[n] - 1;
            }
            else {
               pUOut[n] = pUData[n];
            }
            iDiffV = pVRef[n] - pVData[n];
            if (iDiffV < 2 && iDiffV > -2) {
               pVOut[n] = pVRef[n];
            }
            else if (iDiffV < 3 && iDiffV > 0) {
               pVOut[n] = pVData[n] + 1;
            }
            else if (iDiffV < 0 && iDiffV > -3) {
               pVOut[n] = pVData[n] - 1;
            }
            else {
               pVOut[n] = pVData[n];
            }
         }
      }
   }
   else if (type == PixelFmt_UYVY || type == PixelFmt_HDYC) {
      iLumaLength = mFramePixels;
      iChromaLength = iLumaLength >> 1;
      //UYVY or HDYC packed 
      for (i = 0; i < mCtx.height; i++) {
         pUData = inputFrame + i * mCtx.width * 2;
         pURef = mReferenceFrame + i * mCtx.width * 2;
         pUOut = outputFrame + i * mCtx.width * 2;
         pYData = pUData + 1;
         pYRef = pURef + 1;
         pYOut = pUOut + 1;
         pVData = pUData + 2;
         pVRef = pURef + 2;
         pVOut = pUOut + 2;
         for (j = 0; j < mCtx.width; j = j + 2) {
            int n = j * 2;
            //Y0
            iDiffY = pYRef[n] - pYData[n];
            if (iDiffY < 4 && iDiffY > -4) {
               pYOut[n] = pYRef[n];
            }
            else if (iDiffY < 6 && iDiffY > 0) {
               pYOut[n] = pYData[n] + (6 - iDiffY);
            }
            else if (iDiffY > -6 && iDiffY < 0) {
               pYOut[n] = pYData[n] - (6 + iDiffY);
            }
            else {
               pYOut[n] = pYData[n];
            }
            //Y1
            iDiffY = pYRef[n + 2] - pYData[n + 2];
            if (iDiffY < 4 && iDiffY > -4) {
               pYOut[n + 2] = pYRef[n + 2];
            }
            else if (iDiffY < 6 && iDiffY > 0) {
               pYOut[n + 2] = pYData[n + 2] + (6 - iDiffY);
            }
            else if (iDiffY > -6 && iDiffY < 0) {
               pYOut[n + 2] = pYData[n + 2] - (6 + iDiffY);
            }
            else {
               pYOut[n + 2] = pYData[n + 2];
            }
            //UV
            iDiffU = pURef[n] - pUData[n];
            if (iDiffU < 2 && iDiffU > -2) {
               pUOut[n] = pURef[n];
            }
            else if (iDiffU < 3 && iDiffU > 0) {
               pUOut[n] = pUData[n] + 1;
            }
            else if (iDiffU < 0 && iDiffU > -3) {
               pUOut[n] = pUData[n] - 1;
            }
            else {
               pUOut[n] = pUData[n];
            }
            iDiffV = pVRef[n] - pVData[n];
            if (iDiffV < 2 && iDiffV > -2) {
               pVOut[n] = pVRef[n];
            }
            else if (iDiffV < 3 && iDiffV > 0) {
               pVOut[n] = pVData[n] + 1;
            }
            else if (iDiffV < 0 && iDiffV > -3) {
               pVOut[n] = pVData[n] - 1;
            }
            else {
               pVOut[n] = pVData[n];
            }
         }
      }
   }
   memcpy(mReferenceFrame, outputFrame, (iLumaLength + 2 * iChromaLength) * sizeof(pixel));
	return 0;
}

bool ImageEnhance::Brighter(pixel *inputFrame, pixel *outputFrame, PixelFmt type, float alpha){
   bool sameBuf;
   if (!mIsInited) {
      return false;
   }
   if (!inputFrame || !outputFrame) {
      return false;
   }
   if (type == PixelFmt_UNKNOWN) {
      return false;
   }
   if (alpha > 1.0 || alpha < 0) {
      alpha = 0.5;
   }
	pixel *pixelIn, *pixelOut;
	uint32_t point;
	int i, j;
	/*float k1, k2;
   k1 = (float)(kKeyPointY[1] - kKeyPointY[0]) / (kKeyPointX[1] - kKeyPointX[0]);
   k2 = (float)(kKeyPointY[2] - kKeyPointY[1]) / (kKeyPointX[2] - kKeyPointX[1]);*/
   uint32_t k1, k2, y1;
   k1 = DN_K1_Q20 * alpha;
   k2 = DN_K2_A_Q20 - DN_K2_B_Q20 * alpha;
   y1 = kKeyPointY[1] + 20 * (alpha - 0.5);
   if (inputFrame == outputFrame) {
      sameBuf = true;
   }
   else {
      sameBuf = false;
   }
   if (type == PixelFmt_I420 || type == PixelFmt_YV12) {
      for (i = 0; i < mCtx.height; i++) {
         pixelIn = inputFrame + i * mCtx.width;
         pixelOut = outputFrame + i * mCtx.width;
         for (j = 0; j < mCtx.width; j++) {
            point = pixelIn[j];
            if (point >= kKeyPointX[0] && point <= kKeyPointX[1]) {
               point = k1 * (point - kKeyPointX[0]) + (kKeyPointY[0] << DN_Q);
            }
            else if (point >= kKeyPointX[1] && point <= kKeyPointX[2]) {
               point = k2 * (point - kKeyPointX[1]) + (kKeyPointY[1] << DN_Q);
            }
            point = point >> DN_Q; //Q0
            if (point < 0) {
               pixelOut[j] = 0;
            }
            else if (point > 255) {
               pixelOut[j] = 255;
            }
            else {
               pixelOut[j] = (pixel)point;
            }
         }
      }
      if (sameBuf == false) {
         memcpy(outputFrame + mFramePixels, inputFrame + mFramePixels, sizeof(pixel) * mFramePixels / 2);
      }
   }
   else if (type == PixelFmt_YVYU || type == PixelFmt_YUY2) {
      if (sameBuf == false) {
         memcpy(outputFrame, inputFrame, sizeof(pixel) * mFramePixels * 2 );
      }
      for (i = 0; i < mCtx.height; i++) {
         pixelIn = inputFrame + i * mCtx.width * 2;
         pixelOut = outputFrame + i * mCtx.width * 2;
         for (j = 0; j < mCtx.width; j++) {
            point = pixelIn[j << 1];
            if (point >= kKeyPointX[0] && point <= kKeyPointX[1]) {
               point = k1 * (point - kKeyPointX[0]) + (kKeyPointY[0] << DN_Q);
            }
            else if (point >= kKeyPointX[1] && point <= kKeyPointX[2]) {
               point = k2 * (point - kKeyPointX[1]) + (kKeyPointY[1] << DN_Q);
            }
            point = point >> DN_Q; //Q0
            if (point < 0) {
               pixelOut[j << 1] = 0;
            }
            else if (point > 255) {
               pixelOut[j << 1] = 255;
            }
            else {
               pixelOut[j << 1] = (pixel)point;
            }
         }
      }
   }
   else if (type == PixelFmt_UYVY || type == PixelFmt_HDYC) {
      if (sameBuf == false) {
         memcpy(outputFrame, inputFrame, sizeof(pixel) * mFramePixels * 2);
      }
      for (i = 0; i < mCtx.height; i++) {
         pixelIn = inputFrame + i * mCtx.width * 2 + 1;
         pixelOut = outputFrame + i * mCtx.width * 2 + 1;
         for (j = 0; j < mCtx.width; j++) {
            point = pixelIn[j << 1];
            if (point >= kKeyPointX[0] && point <= kKeyPointX[1]) {
               point = k1 * (point - kKeyPointX[0]) + (kKeyPointY[0] << DN_Q);
            }
            else if (point >= kKeyPointX[1] && point <= kKeyPointX[2]) {
               point = k2 * (point - kKeyPointX[1]) + (kKeyPointY[1] << DN_Q);
            }
            if (point < 0) {
               pixelOut[j << 1] = 0;
            }
            else if (point > 255) {
               pixelOut[j << 1] = 255;
            }
            else {
               pixelOut[j << 1] = (pixel)point;
            }
         }
      }
   }
	return true;
}

bool ImageEnhance::EdgeEnhance(pixel *inputFrame, pixel *outputFrame, PixelFmt type, float alpha){
   if (!mIsInited) {
      return false;
   }
   if (inputFrame == nullptr || outputFrame == nullptr) {
      return false;
   }
   if (type != PixelFmt_I420 && type != PixelFmt_YV12) {
      return false;
   }
   bool sameBuf;
	int iTempPixel;
	// p0   p1   p2
	// p3 £¨p4£© p5
	// p6   p7   p8
	pixel *filter[9];
	pixel *pixelIn, *pixelOut;
	int i, j, k;
   int level;
   if (alpha < 0.33) {
      level = 5;
   }
   else if (alpha < 0.67) {
      level = 4;
   }
   else {
      level = 3;
   }

   if (inputFrame == outputFrame) {
      sameBuf = true;
   }
   else {
      sameBuf = false;
   }

	for (i = 1; i < mCtx.height -1 ; i++){
      pixelOut = outputFrame + i * mCtx.width;
      pixelIn = inputFrame + i * mCtx.width;
      if (i == 0 || i == mCtx.height - 1) {
         memcpy(pixelOut, pixelIn, mCtx.width * sizeof(pixel));
      }
      else {
         filter[0] = pixelIn - mCtx.width;
         filter[1] = pixelIn - mCtx.width + 1;
         filter[2] = pixelIn - mCtx.width + 2;
         filter[3] = pixelIn;
         filter[4] = pixelIn + 1;
         filter[5] = pixelIn + 2;
         filter[6] = pixelIn + mCtx.width;
         filter[7] = pixelIn + mCtx.width + 1;
         filter[8] = pixelIn + mCtx.width + 2;
         for (j = 1; j < mCtx.width -1; j++) {
            if (j == 0 || j == mCtx.width - 1) {
               pixelOut[j] = pixelIn[j];
            }
            else {
              iTempPixel = 0;
              pixelOut[j] = pixelIn[j];
               for (k = 0; k < 9; k++) {
                  if (k != 4) {
                     iTempPixel += (filter[k][j - 1] - filter[4][j - 1]);
                  }
               }
               if (iTempPixel > 50 || iTempPixel < -50) {
                  iTempPixel = filter[4][j - 1] - (iTempPixel >> level);
                  if (iTempPixel < 0) {
                     iTempPixel = 0;
                  }
                  else if (iTempPixel > 255) {
                     iTempPixel = 255;
                  }
                  pixelOut[j] = (pixel)iTempPixel;
               }
            }
         }
      }
	}
   if (sameBuf == false) {
      memcpy(outputFrame + mFramePixels, inputFrame + mFramePixels, sizeof(pixel) * mFramePixels / 2);
   }
	return true;
}

bool ImageEnhance::EdgeEnhance(uint32_t * inputFrame, uint32_t * outputFrame, PixelFmt type, float alpha)
{
   if (!mIsInited) {
      return false;
   }
   if (inputFrame == nullptr || outputFrame == nullptr) {
      return false;
   }
   if (type != PixelFmt_RGB) {
      return false;
   }
   // p0   p1   p2
   // p3 £¨p4£© p5
   // p6   p7   p8
   uint32_t *filter[9];
   uint32_t *pixelIn, *pixelOut;
   int tmpR, tmpG, tmpB;
   int valueR, valueG, valueB;
   uint32_t temp;
   int i, j, k;
   int level;
   if (alpha < 0.33) {
      level = 5;
   }
   else if (alpha < 0.67) {
      level = 4;
   }
   else {
      level = 3;
   }
   for (i = 1; i < mCtx.height - 1; i++) {
      pixelOut = outputFrame + i * mCtx.width;
      pixelIn = inputFrame + i * mCtx.width;
      if (i == 0 || i == mCtx.height - 1) {
         memcpy(pixelOut, pixelIn, mCtx.width * sizeof(uint32_t));
      }
      else {
         filter[0] = pixelIn - mCtx.width;
         filter[1] = pixelIn - mCtx.width + 1;
         filter[2] = pixelIn - mCtx.width + 2;
         filter[3] = pixelIn;
         filter[4] = pixelIn + 1;
         filter[5] = pixelIn + 2;
         filter[6] = pixelIn + mCtx.width;
         filter[7] = pixelIn + mCtx.width + 1;
         filter[8] = pixelIn + mCtx.width + 2;
         for (j = 1; j < mCtx.width - 1; j++) {
            if (j == 0 || j == mCtx.width - 1) {
               pixelOut[j] = pixelIn[j];
            }
            else {
               tmpR = 0;
               tmpG = 0;
               tmpB = 0;
               valueR = RED(filter[4][j - 1]);
               valueG = GREEN(filter[4][j - 1]);
               valueB = BLUE(filter[4][j - 1]);
               pixelOut[j] = pixelIn[j];
               for (k = 0; k < 9; k++) {
                  if (k != 4) {
                     /*tmpR += (int)(RED(filter[k][j - 1]));
                     tmpG += (int)(GREEN(filter[k][j - 1]));
                     tmpB += (int)(BLUE(filter[k][j - 1]));*/
                     tmpR += (int)(RED(filter[k][j - 1])) - valueR;
                     tmpG += (int)(GREEN(filter[k][j - 1])) - valueG;
                     tmpB += (int)(BLUE(filter[k][j - 1])) - valueB;
                  }
                  
               }
               if (tmpR > 50 || tmpR < -50) {
                  tmpR = valueR  - (tmpR >> level);
                  if (tmpR < 0) {
                     tmpR = 0;
                  }
                  else if (tmpR > 255) {
                     tmpR = 255;
                  }
                  pixelOut[j] &= 0xFF00FFFF;
                  pixelOut[j] |= (tmpR << 16);
               }
               if (tmpG > 50 || tmpG < -50) {
                  tmpG = valueG - (tmpG >> level);
                  if (tmpG < 0) {
                     tmpG = 0;
                  }
                  else if (tmpG > 255) {
                     tmpG = 255;
                  }
                  pixelOut[j] &= 0xFFFF00FF;
                  pixelOut[j] |= (tmpG << 8);
               }
               if (tmpB > 50 || tmpB < -50) {
                  tmpB = valueB - (tmpB >> level);
                  if (tmpB < 0) {
                     tmpB = 0;
                  }
                  else if (tmpB > 255) {
                     tmpB = 255;
                  }
                  pixelOut[j] &= 0xFFFFFF00;
                  pixelOut[j] |= tmpB;
               }
            }
         }
      }
   }
   return true;
}
