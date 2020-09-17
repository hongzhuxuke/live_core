#include "DSDemo.h"
FILE* mPlayerAudioFile = nullptr;
#include "libyuv/convert_from.h"
#include "libyuv/convert.h"
#include "libyuv/convert_from_argb.h"
#include "libyuv/convert_argb.h"

vlive::IDShowEngine* gDShow;
HWND DSDemo::mHwnd = nullptr;
BITMAPINFO bmi_;
uint8_t* rgb = nullptr;
enum class VideoFormat {

};

QString GetStringName(DShow::VideoFormat format) {
   switch (format)
   {
   case DShow::VideoFormat::Any:
      return QString("Any");
      break;
   case DShow::VideoFormat::Unknown:
      return QString("Unknown");
      break;
   case DShow::VideoFormat::ARGB:
      return QString("ARGB");
      break;
   case DShow::VideoFormat::XRGB:
      return QString("XRGB");
      break;
   case DShow::VideoFormat::I420:
      return QString("I420");
      break;
   case DShow::VideoFormat::NV12:
      return QString("NV12");
      break;
   case DShow::VideoFormat::YV12:
      return QString("YV12");
      break;
   case DShow::VideoFormat::Y800:
      return QString("Y800");
      break;
   case DShow::VideoFormat::YVYU:
      return QString("YVYU");
      break;
   case DShow::VideoFormat::YUY2:
      return QString("YUY2");
      break;
   case DShow::VideoFormat::UYVY:
      return QString("UYVY");
      break;
   case DShow::VideoFormat::HDYC:
      return QString("HDYC");
      break;
   case DShow::VideoFormat::MJPEG:
      return QString("MJPEG");
      break;
   case DShow::VideoFormat::H264:
      return QString("H264");
      break;
   default:
      break;
   }
}

struct CustomDevInfo {
   QString id;
   QString name;
   int width;
   int height;
   DShow::VideoFormat format;
   int frameInterval;
};

Q_DECLARE_METATYPE(CustomDevInfo)


DSDemo::DSDemo(QWidget *parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);
    gDShow = vlive::CreateEngine();
    std::vector<DShow::VideoDevice> videos;
    gDShow->GetVideoDevice(videos);
    mHwnd = (HWND)ui.widget->winId();
    for (int i = 0; i < videos.size(); i++) {
       DShow::VideoDevice dev = videos.at(i);
       QString dev_info_name = QString::fromStdWString(dev.name);
       for (int m = 0; m < dev.caps.size(); m++) {
          DShow::VideoInfo info = dev.caps.at(m);
          QString dev_info_cap = dev_info_name + QString(" %1 * %2 maxInterval:%3 format:%4").arg(info.maxCX).arg(info.maxCY).arg(info.minInterval).arg(GetStringName(info.format));
          CustomDevInfo c_info;
          c_info.name = QString::fromStdWString(dev.name);
          c_info.id = QString::fromStdWString(dev.path);
          c_info.width = info.maxCX;
          c_info.height = info.maxCY;
          c_info.frameInterval = info.minInterval;
          c_info.format = info.format;
          ui.comboBox->addItem(dev_info_cap, QVariant::fromValue(c_info));
       }
    }
    connect(ui.comboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(OnDeviceSelect(int)));

    connect(ui.pushButton_create, SIGNAL(clicked()), this, SLOT(slot_Create()));
    connect(ui.pushButton_destory, SIGNAL(clicked()), this, SLOT(slot_Destory()));
    connect(ui.pushButton_start, SIGNAL(clicked()), this, SLOT(slot_Start()));
    connect(ui.pushButton_stop, SIGNAL(clicked()), this, SLOT(slot_Stop()));

    connect(ui.comboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(OnDeviceSelect(int)));
  /*  for (int i = 0; i < videos.size(); i++) {
       if (i == 2) {
          DShow::VideoDevice dev = videos.at(i);
          DShow::VideoConfig video_config;
          video_config.recvVideoCallback = DSDemo::RecvVideoCallBack;
          video_config.cx = dev.caps.at(0).maxCX;
          video_config.cy_abs = dev.caps.at(0).maxCY;
          video_config.format = dev.caps.at(0).format;
          video_config.frameInterval = dev.caps.at(0).maxInterval;
          video_config.internalFormat = dev.caps.at(0).format;
          video_config.name = dev.name;
          video_config.path = dev.path;
          gDShow->SetCaptureDevice(video_config);
          gDShow->StartCapture();
          return;
       }
    }*/


}


void DSDemo::RecvVideoCallBack(const DShow::VideoConfig &config, unsigned char *data, size_t size, long long startTime, long long stopTime, long rotation) {
   //if (mPlayerAudioFile == nullptr) {
   //   mPlayerAudioFile = fopen("I:\\mPlayerAudioFile.yuv", "wb");
   //}
   //if (mPlayerAudioFile) {
   //   fwrite(data, sizeof(uint8_t), size, mPlayerAudioFile);
   //}
   DSDemo::HDCRender((int)config.format,mHwnd, data, config.cx, config.cy_abs);
}

void DSDemo::slot_Create(){
   if (gDShow) {
      gDShow->CreateInputDevice(DShow::DeviceInputType::Video_Input);
   }

}
void DSDemo::slot_Destory() {
   if (gDShow) {
      gDShow->StopCapture();
      vlive::DestoryEngine(gDShow);
      gDShow = nullptr;
   }

}
void DSDemo::slot_Start() {
   OnDeviceSelect(0);
}

void DSDemo::slot_Stop() {
   if (gDShow) {
      gDShow->StopCapture();
   }
}


void DSDemo::OnDeviceSelect(int index) {
   ui.comboBox->currentData();
   CustomDevInfo devClass = ui.comboBox->currentData().value<CustomDevInfo>();

   DShow::VideoConfig video_config;
   video_config.recvVideoCallback = DSDemo::RecvVideoCallBack;
   video_config.cx = devClass.width;
   video_config.cy_abs = devClass.height;
   video_config.format = devClass.format;
   video_config.frameInterval = devClass.frameInterval;
   video_config.internalFormat = devClass.format;
   video_config.name = devClass.name.toStdWString();
   video_config.path = devClass.id.toStdWString() ;
   gDShow->StartCaptureDevice(video_config);
   //gDShow->StartCapture();
}

void SetSize(int type, int width, int height) {
   if (width == bmi_.bmiHeader.biWidth && height == bmi_.bmiHeader.biHeight) {
      return;
   }
   if (rgb) {
      delete []rgb;
   }
   rgb = new uint8_t[width * height * 4];
   bmi_.bmiHeader.biWidth = width;
   bmi_.bmiHeader.biHeight = height;
   bmi_.bmiHeader.biSizeImage = (width * height * 4);
   bmi_.bmiHeader.biBitCount =  32;
   bmi_.bmiHeader.biPlanes = 1;
   bmi_.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
}

void DSDemo::HDCRender(int type, HWND desHwnd, LPBYTE bytes, int cx, int cy) {
   if (desHwnd == NULL) {
      return;
   }

   SetSize(type, cx, cy);
   HWND wnd_ = desHwnd;
   RECT rc;
   ::GetClientRect(wnd_, &rc);

   BITMAPINFO& bmi = bmi_;
   int height = abs(bmi.bmiHeader.biHeight);
   int width = bmi.bmiHeader.biWidth;

   const uint8_t* image = bytes;
   if (image != NULL) {
      //auto mWindowDC = ::GetDC(wnd_);
      //HDC dc_mem = ::CreateCompatibleDC(mWindowDC);
      //::SetStretchBltMode(dc_mem, HALFTONE);


      //HBITMAP bmp_mem = ::CreateCompatibleBitmap(mWindowDC, rc.right, rc.bottom);
      //HGDIOBJ bmp_old = ::SelectObject(dc_mem, bmp_mem);

      //POINT logical_area = { rc.right, rc.bottom };
      //DPtoLP(mWindowDC, &logical_area, 1);

      //HBRUSH brush = ::CreateSolidBrush(RGB(0, 0, 0));
      //RECT logical_rect = { 0, 0, logical_area.x, logical_area.y };
      //::FillRect(dc_mem, &logical_rect, brush);
      //::DeleteObject(brush);

      //int x = (logical_area.x >> 1) - (width >> 1);
      //int y = (logical_area.y >> 1) - (height >> 1);

      if (DShow::VideoFormat::XRGB == (DShow::VideoFormat)type) {
         //StretchDIBits(dc_mem, x, y, width, height, 0, 0, width, height, image, &bmi, DIB_RGB_COLORS, SRCCOPY);
         //BitBlt(mWindowDC, 0, 0, logical_area.x, logical_area.y, dc_mem, 0, 0, SRCCOPY);
      }
      else if (DShow::VideoFormat::I420 == (DShow::VideoFormat)type || DShow::VideoFormat::YV12 == (DShow::VideoFormat)type) {
         uint8_t* src_u = bytes + width * height;
         uint8_t* src_v = src_u + width * (height >> 2);
         bmi.bmiHeader.biHeight = -bmi.bmiHeader.biHeight;
         if (DShow::VideoFormat::I420 == (DShow::VideoFormat)type) {
            libyuv::I420ToARGB(bytes, width, src_u, width >> 1, src_v, width >> 1, rgb, width * 4, width, height);
         }
         else {
            libyuv::I420ToARGB(bytes, width, src_v, width >> 1, src_u, width >> 1, rgb, width * 4, width, height);
         }
         //StretchDIBits(dc_mem, x, y, width, height, 0, 0, width, height, rgb, &bmi, DIB_RGB_COLORS, SRCCOPY);
         //BitBlt(mWindowDC, 0, 0, logical_area.x, logical_area.y, dc_mem, 0, 0, SRCCOPY);
      }
      else if ((DShow::VideoFormat)type == DShow::VideoFormat::YVYU || (DShow::VideoFormat)type == DShow::VideoFormat::YUY2) {
         uint8_t* src_u = bytes + width * height;
         uint8_t* src_v = src_u + width * (height >> 2);
         bmi.bmiHeader.biHeight = -bmi.bmiHeader.biHeight;
         libyuv::YUY2ToARGB(bytes, width * 2, rgb, width * 4, width, height);
         //StretchDIBits(dc_mem, x, y, width, height, 0, 0, width, height, rgb, &bmi, DIB_RGB_COLORS, SRCCOPY);
         //BitBlt(mWindowDC, 0, 0, logical_area.x, logical_area.y, dc_mem, 0, 0, SRCCOPY);
      }
      else if ((DShow::VideoFormat)type == DShow::VideoFormat::UYVY || (DShow::VideoFormat)type == DShow::VideoFormat::HDYC) {
         uint8_t* src_u = bytes + width * height;
         uint8_t* src_v = src_u + width * (height >> 2);
         bmi.bmiHeader.biHeight = -bmi.bmiHeader.biHeight;
         libyuv::UYVYToARGB(bytes, width * 2, rgb, width * 4, width, height);
         //StretchDIBits(dc_mem, x, y, width, height, 0, 0, width, height, rgb, &bmi, DIB_RGB_COLORS, SRCCOPY);
         //BitBlt(mWindowDC, 0, 0, logical_area.x, logical_area.y, dc_mem, 0, 0, SRCCOPY);
      }
     
      //::SelectObject(dc_mem, bmp_old);
      //::DeleteObject(bmp_mem);
      //::DeleteDC(dc_mem);
      //::ReleaseDC(wnd_, mWindowDC);
      //delete []dst_argb;
   }
}