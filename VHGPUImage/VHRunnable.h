#pragma once

#include "GL/glew.h"
#include "GL/glut.h"

namespace vhall {
  class IRunnable {
  public:
    virtual ~IRunnable() {};
    virtual void run() = 0;
  };

  class VHRunnable1i : public IRunnable {
  public:
    VHRunnable1i(int location, int intValue);
    virtual void run();
  protected:
  private:
    int location = 0;
    int intValue = 0;
  };

  class VHRunnable1f : public IRunnable {
  public:
    VHRunnable1f(int location, float floatValue);
    virtual void run();
  protected:
  private:
    int location = 0;
    float floatValue = 0;
  };


  class VHRunnable2fv : public IRunnable {
  public:
    VHRunnable2fv(int location, float* valueptr);
    virtual void run();
  protected:
  private:
    int loc = 0;
    float* ptr = nullptr;
  };

  class VHRunnable3fv : public IRunnable {
  public:
    VHRunnable3fv(int location, float* valueptr);
    virtual void run();
  protected:
  private:
    int loc = 0;
    float* ptr = nullptr;
  };


  class VHRunnable4fv : public IRunnable {
  public:
    VHRunnable4fv(int location, float* valueptr);
    virtual void run();
  protected:
  private:
    int loc = 0;
    float* ptr = nullptr;
  };

  class VHRunnableXfv : public IRunnable {
  public:
    VHRunnableXfv(int location, float* valueptr, int len);
    virtual void run();
  protected:
  private:
    int loc = 0;
    float* ptr = nullptr;
    int len = 0;
  };

  class VHRunnablePt : public IRunnable {
  public:
    VHRunnablePt(int location, float x, float y);
    virtual void run();
  protected:
  private:
    int loc = 0;
    float x = 0;
    float y = 0;
  };


  class VHRunnableMatrix3f : public IRunnable {
  public:
    VHRunnableMatrix3f(int location, float* matrix);
    virtual void run();
  protected:
  private:
    int loc = 0;
    float* matrix = nullptr;
  };

  class VHRunnableMatrix4f : public IRunnable {
  public:
    VHRunnableMatrix4f(int location, float* matrix);
    virtual void run();
  protected:
  private:
    int loc = 0;
    float* matrix = nullptr;
  };
}