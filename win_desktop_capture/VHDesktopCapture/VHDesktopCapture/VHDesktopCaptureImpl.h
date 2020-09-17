#pragma once
#include "VHDesktopCaptureInterface.h"
#include "modules\desktop_capture\desktop_capture_options.h"
#include "modules\desktop_capture\desktop_capturer.h"
#include "modules\desktop_capture\desktop_and_cursor_composer.h"

using namespace vlive_desktopcatpure;
using namespace webrtc;

class VHDesktopCaptureImpl: public VHDesktopCaptureInterface, public DesktopCapturer::Callback
{
public:
   VHDesktopCaptureImpl();
   virtual ~VHDesktopCaptureImpl();

   virtual int Init();
   virtual void Start(CaputreCallback * callback);
   virtual void GetScreenSource(std::vector<int> screens);
   virtual void SetScreenSoucce(const int screen_id);
   virtual void CaputreFrame();

   // DesktopCapturer::Callback interface
   void OnCaptureResult(webrtc::DesktopCapturer::Result result,
      std::unique_ptr<webrtc::DesktopFrame> frame) override;
private:
   DesktopRect mDesktopRect;
   std::unique_ptr<webrtc::DesktopFrame> mDesktopframe;
   std::unique_ptr<webrtc::DesktopCapturer> mDesktopCapture;
   CaputreCallback *mCaptureCallback;
   int mCurrentScreenId = -1;
};

