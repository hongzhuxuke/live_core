#pragma once
#include "VHGPUImageFilter.h"

namespace vhall {
  class VHBeautifyFilter : public VHGPUImageFilter {
  public:
    VHBeautifyFilter();
    VHBeautifyFilter(int level);
    ~VHBeautifyFilter();
    void SetBeautifyLevel(int level);
    virtual void onOutputSizeChanged(int width, int height);
  protected:
    virtual void onInit() override;
    virtual void onInitialized() override;
  private:
    void setTexelSize(float w, float h);
  private:
    int beautifyLocation;
    int singleStepOffsetLocation;
    int beautifyLevel;
    static std::string BEAUTIFY_FRAGMENT_SHADER;
  };

}