#include "VHGPUImageBrightnessFilter.h"

namespace vhall {

  std::string VHGPUImageBrightnessFilter::BRIGHTNESS_FRAGMENT_SHADER = ""
    "varying highp vec2 textureCoordinate;\n"
    " \n"
    " uniform sampler2D inputImageTexture;\n"
    " uniform lowp float brightness;\n"
    " \n"
    " void main()\n"
    " {\n"
    "     lowp vec4 textureColor = texture2D(inputImageTexture, textureCoordinate);\n"
    "     \n"
    "     gl_FragColor = vec4((textureColor.rgb + vec3(brightness)), textureColor.w);\n"
    " }";

  VHGPUImageBrightnessFilter::VHGPUImageBrightnessFilter() :
    VHGPUImageBrightnessFilter(0.0f) {

  }

  VHGPUImageBrightnessFilter::VHGPUImageBrightnessFilter(float b) :
    VHGPUImageFilter(NO_FILTER_VERTEX_SHADER, BRIGHTNESS_FRAGMENT_SHADER),
    brightness(b) {

  }

  VHGPUImageBrightnessFilter::~VHGPUImageBrightnessFilter() {

  }

  void VHGPUImageBrightnessFilter::onInit() {
    VHGPUImageFilter::onInit();
    brightnessLocation = glGetUniformLocation(getProgram(), "brightness");
  }

  void VHGPUImageBrightnessFilter::onInitialized() {
    VHGPUImageFilter::onInitialized();
    setBrightness(brightness);
  }

  void VHGPUImageBrightnessFilter::setBrightness(float b) {
    this->brightness = b;
    setFloat(brightnessLocation, this->brightness);
  }
}
