#include "VHDesktopCaptureImpl.h"
#include <mutex>

static VHDesktopCaptureImpl* gVHDesktopCaptureImplInstance = nullptr;
static std::mutex gSDKMutex;

VHDesktopCaptureImpl::VHDesktopCaptureImpl(){
   mDesktopRect = (DesktopRect::MakeLTRB(0, 0, 0, 0));
}


VHDesktopCaptureImpl::~VHDesktopCaptureImpl()
{
}


int VHDesktopCaptureImpl::Init() {
   webrtc::DesktopCaptureOptions options(webrtc::DesktopCaptureOptions::CreateDefault());
   options.set_allow_directx_capturer(true);
   std::unique_ptr<webrtc::DesktopCapturer> dc;
   dc = DesktopCapturer::CreateScreenCapturer(options);
   mDesktopCapture = std::make_unique<webrtc::DesktopAndCursorComposer>(std::move(dc), options);
   return 0;
}

void VHDesktopCaptureImpl::Start(CaputreCallback *call_back) {
   mCaptureCallback = call_back;
   mDesktopCapture->Start(this);
}

void VHDesktopCaptureImpl::GetScreenSource(std::vector<int> screens) {
   webrtc::DesktopCapturer::SourceList total_screens;
   if (mDesktopCapture) {
      mDesktopCapture->GetSourceList(&total_screens);
      for (int i = 0; i < total_screens.size(); i++) {
         int index = total_screens.at(i).id;
         screens.push_back(index);
      }
   }
}

void VHDesktopCaptureImpl::SetScreenSoucce(const int screen_id) {
   if (mDesktopCapture) {
      mDesktopCapture->SelectSource(screen_id);
   }
}

void VHDesktopCaptureImpl::CaputreFrame() {
   if (mDesktopCapture) {
      mDesktopCapture->CaptureFrame();
   }
}

void VHDesktopCaptureImpl::OnCaptureResult(webrtc::DesktopCapturer::Result result, std::unique_ptr<webrtc::DesktopFrame> frame) {
   if (result == DesktopCapturer::Result::SUCCESS && frame != nullptr) {
      uint8_t* src = frame->data();
      mCaptureCallback->OnCaptureResult((CaputreResult)result, src, frame->size().width(), frame->size().height());
   }
   else {
      mCaptureCallback->OnCaptureResult((CaputreResult)result, nullptr, 0, 0);
   }
}

namespace vlive_desktopcatpure{
   VH_DESKTOP_CAPTURE_EXPORT VHDesktopCaptureInterface* GetVHDesktopCaptureInstance() {
      //std::unique_lock<std::mutex> lock(gSDKMutex);
      //if (gVHDesktopCaptureImplInstance == nullptr) {
      //   gVHDesktopCaptureImplInstance = new VHDesktopCaptureImpl();
      //}
      //return (VHDesktopCaptureInterface*)gVHDesktopCaptureImplInstance;
      return new VHDesktopCaptureImpl();
   }

   VH_DESKTOP_CAPTURE_EXPORT void DestoryHDesktopCaptureInstance(VHDesktopCaptureInterface* obj) {
      delete obj;
      //std::unique_lock<std::mutex> lock(gSDKMutex);
      //if (gVHDesktopCaptureImplInstance) {
      //   delete gVHDesktopCaptureImplInstance;
      //   gVHDesktopCaptureImplInstance = nullptr;
      //}
   }
}
