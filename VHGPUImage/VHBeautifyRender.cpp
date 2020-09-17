#include <windows.h>
#include "GL/glew.h"
#include "GL/glut.h"
#include "GL/wglew.h"
#include "VHBeautifyRender.h"
#include "VHShaderUtil.h"
#include "VHOpenGLUtil.h"
#include "wglContext/VHGLContext.h"
#include "log.h"
#include "talk/base/thread.h"

namespace vhall {

  class VHRenderMessage : public talk_base::MessageData {
  public:
    VHRenderMessage(std::shared_ptr<unsigned char[]> rgbData, int width, int height, int64_t ts) {
      if (rgbData && (width * height > 0)) {
        mRgbData.reset(new unsigned char[width * height * 3]);
        memcpy(mRgbData.get(), rgbData.get(), width * height * 3);
      }
      mWidth = width;
      mHeight = height;
      mTimeStamp = ts;
    }

    VHRenderMessage(int level) {
      mLevel = level;
    }

    ~VHRenderMessage() {}
    int mWidth = 0;
    int mHeight = 0;
    int64_t mTimeStamp = 0;
    std::shared_ptr<unsigned char[]> mRgbData = nullptr;
    int mLevel = 0;
  };

  VHBeautifyRender::VHBeautifyRender(int Level, const FrameCB cb) {
    level = Level;
    frame_cb_.push_back(cb);
  }

  VHBeautifyRender::~VHBeautifyRender() {
    destroy();
    if (mThread) {
      mThread->Stop();
      mThread->Clear(this);
      mThread.reset();
    }

    OnDestroy();
  }

  void VHBeautifyRender::createInstance(int width, int height) {
    mThread.reset(new talk_base::Thread());
    if (!mThread) {
      LOGE("Init thread failed");
      return;
    }
    mThread->Restart();
    mThread->Start();

    /* ³õÊ¼»¯ */
    mThread->Post(this, TaskGLInit, new VHRenderMessage(nullptr, width, height, 0));
    bInitialized = true;
  }

  void VHBeautifyRender::init(int width, int height) {
    /* init GLContext */
    glContext.reset(new VHGLContext());
    if (!glContext) {
      return;
    }
    bool result = glContext->Initialize();
    
    if (!result) {
      LOGE("glContext->Initialize fail");
      return;
    }

    /* glew */
    glewExperimental = GL_TRUE;
    GLenum status = glewInit();
    if (GLEW_OK != status) {
      LOGE("glewInit error");
    }

    vhall::VHShaderUtil::checkGlError("glewInit");

    glShadeModel(GL_SMOOTH);                    // shading mathod: GL_SMOOTH or GL_FLAT
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);      // 4-byte pixel alignment

    filter = std::make_shared<vhall::VHBeautifyFilter>(level);
    vhall::VHOpenGLUtil::loadTexture(nullptr, width, height, loadTextureId);
    vhall::VHOpenGLUtil::InitFBO(fboTextureId, renderbufferId, framebufferId, width, height);

    glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
    filter->ifNeedInit();
    glDisable(GL_DEPTH_TEST);

    glViewport(0, 0, width, height);
    glUseProgram(filter->getProgram());
    filter->onOutputSizeChanged(width, height);

    bInitialized = true;
    vhall::VHShaderUtil::checkGlError("init done");
  }

  void VHBeautifyRender::ifNeedInit(int width, int height) {
    if (!bInitialized) {
      createInstance(width, height);
    }
  }

  void VHBeautifyRender::destroy() {
    if (mThread) {
      mThread->Send(this, TaskGLDestroy, nullptr);
    }
  }

  void VHBeautifyRender::onOutputSizeChanged(int width, int height) {

  }

  void VHBeautifyRender::loadFrame(std::shared_ptr<unsigned char[]> data, int length, int width, int height, int64_t ts) {
    ifNeedInit(width, height);
    /*  */
    std::unique_lock<std::mutex> countLock(countMtx);
    if (countToProcess > 1) {
      LOGD("loadFrame Data buffer to process:%d", countToProcess);
      return;
    }
    
    if (mThread) {
      countToProcess++; // record buffer length
      countLock.unlock();
      mThread->Post(this, TaskGLLoadFrame, new VHRenderMessage(data, width, height, ts));
    }
  }

  void VHBeautifyRender::SetBeautifyLevel(int level) {
    this->level = level;
    if (mThread) {
      mThread->Post(this, TaskGLSetLevel, new VHRenderMessage(level));
    }
  }

  void VHBeautifyRender::OnLoadFrame(std::shared_ptr<unsigned char[]> data, int length, int width, int height, int64_t timeStamp) {
    if (!bInitialized) {
      LOGE("render not inited");
      return;
    }

    std::unique_lock<std::mutex> countLock(countMtx);
    countToProcess--; // record buffer length
    countLock.unlock();

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glViewport(0, 0, width, height);
    glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
    glActiveTexture(GL_TEXTURE0);
    std::chrono::time_point<std::chrono::steady_clock> ts = std::chrono::steady_clock::now();
    int64_t curTime = std::chrono::duration_cast<std::chrono::microseconds>(ts.time_since_epoch()).count();
    vhall::VHOpenGLUtil::loadTexture(data.get(), width, height, loadTextureId);
    filter->onDraw(loadTextureId, vhall::VHGPUImageFilter::cubes, vhall::VHGPUImageFilter::textureCoords);

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindTexture(GL_TEXTURE_2D, fboTextureId);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);

    OnReadFrameBuffer(data, width, height, timeStamp);
    std::chrono::time_point<std::chrono::steady_clock> ts2 = std::chrono::steady_clock::now();
    int64_t curTime2 = std::chrono::duration_cast<std::chrono::microseconds>(ts2.time_since_epoch()).count();
    int64_t time = curTime2 - curTime;
    printf("process duration: %ld\n", time);
    if (frame_cb_.size()) {
      frame_cb_.at(0)(data, width*height * 3, width, height, timeStamp);
    }
  }

  void VHBeautifyRender::OnReadFrameBuffer(std::shared_ptr<unsigned char[]> data, int widht, int height, int64_t ts) {
    glBindTexture(GL_TEXTURE_2D, fboTextureId);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glReadPixels(0, 0, widht, height, GL_BGR_EXT, GL_UNSIGNED_BYTE, data.get());
    glBindTexture(GL_TEXTURE_2D, 0);
  }

  void VHBeautifyRender::OnSetBeautifyLevel(int level) {
    this->level = level;
    if (filter) {
      filter->SetBeautifyLevel(level);
    }
  }
  
  void VHBeautifyRender::OnDestroy() {
    filter.reset();
    if (-1 != loadTextureId) {
      glDeleteTextures(1, &loadTextureId);
      loadTextureId = -1;
    }

    if (-1 != fboTextureId) {
      glDeleteTextures(1, &fboTextureId);
      fboTextureId = -1;
    }
    if (-1 != framebufferId) {
      glDeleteFramebuffers(1, &framebufferId);
      framebufferId = -1;
    }
    if (-1 != renderbufferId) {
      glDeleteRenderbuffers(1, &renderbufferId);
      renderbufferId = -1;
    }
    glContext.reset();
    bInitialized = false;
  }

  void VHBeautifyRender::OnMessage(talk_base::Message * msg) {
    VHRenderMessage* msgPayLoad = (static_cast<VHRenderMessage*>(msg->pdata));
    switch (msg->message_id) {
    case TaskGLInit:
      init(msgPayLoad->mWidth, msgPayLoad->mHeight);
      break;
    case TaskGLLoadFrame:
      OnLoadFrame(msgPayLoad->mRgbData, msgPayLoad->mWidth * msgPayLoad->mHeight, msgPayLoad->mWidth, msgPayLoad->mHeight, msgPayLoad->mTimeStamp);
      break;
    case TaskGLDestroy:
      OnDestroy();
      break;
    case TaskGLSetLevel:
      OnSetBeautifyLevel(msgPayLoad->mLevel);
      break;
    default:
      break;
    }

    if (msgPayLoad) {
      delete msgPayLoad;
      msgPayLoad = nullptr;
    }
  }
}