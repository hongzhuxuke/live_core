#pragma once
#include "VHGPUImageFilter.h" 

namespace vhall {
  class VHGPUImageBrightnessFilter : public VHGPUImageFilter {
  public:
    VHGPUImageBrightnessFilter();
    VHGPUImageBrightnessFilter(float brightness);
    ~VHGPUImageBrightnessFilter();

    void setBrightness(float b);
  protected:
    virtual void onInit() override;
    virtual void onInitialized() override;
  private:
    int brightnessLocation;
    float brightness;
    static std::string BRIGHTNESS_FRAGMENT_SHADER;
  };
}