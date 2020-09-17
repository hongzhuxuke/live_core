#include "VHBeautifyFilter.h"

namespace vhall {
  std::string VHBeautifyFilter::BEAUTIFY_FRAGMENT_SHADER =
    std::string("") +
    "uniform sampler2D inputImageTexture;\n" +
    "uniform vec2 singleStepOffset;\n" +
    "uniform vec4 params;\n" +
    "varying vec2 textureCoordinate;\n" +

    "const vec3 W = vec3(0.299,0.587,0.114);\n" +
    "const mat3 saturateMatrix = mat3(\n" +
    "1.1102,-0.0598,-0.061,\n" +
    "-0.0774,1.0826,-0.1186,\n" +
    "-0.0228,-0.0228,1.1772);\n" +
    "\n" +

    "float hardlight(float color)\n" +
    "{\n" +
    "if(color <= 0.5)\n" +
    "{\n" +
    "color = color * color * 2.0;\n" +
    "}\n" +
    "else\n" +
    "{\n" +
    "color = 1.0 - ((1.0 - color)*(1.0 - color) * 2.0);\n" +
    "}\n" +
    "return color;\n" +
    "}\n" +

    "void main(){\n" +
    "vec2 blurCoordinates[24];\n" +
    "blurCoordinates[0] = textureCoordinate.xy + singleStepOffset * vec2(0.0, -10.0);\n" +
    "blurCoordinates[1] = textureCoordinate.xy + singleStepOffset * vec2(0.0, 10.0);\n" +
    "blurCoordinates[2] = textureCoordinate.xy + singleStepOffset * vec2(-10.0, 0.0);\n" +
    "blurCoordinates[3] = textureCoordinate.xy + singleStepOffset * vec2(10.0, 0.0);\n" +
    "blurCoordinates[4] = textureCoordinate.xy + singleStepOffset * vec2(5.0, -8.0);\n" +
    "blurCoordinates[5] = textureCoordinate.xy + singleStepOffset * vec2(5.0, 8.0);\n" +
    "blurCoordinates[6] = textureCoordinate.xy + singleStepOffset * vec2(-5.0, 8.0);\n" +
    "blurCoordinates[7] = textureCoordinate.xy + singleStepOffset * vec2(-5.0, -8.0);\n" +
    "blurCoordinates[8] = textureCoordinate.xy + singleStepOffset * vec2(8.0, -5.0);\n" +
    "blurCoordinates[9] = textureCoordinate.xy + singleStepOffset * vec2(8.0, 5.0);\n" +
    "blurCoordinates[10] = textureCoordinate.xy + singleStepOffset * vec2(-8.0, 5.0);\n" +
    "blurCoordinates[11] = textureCoordinate.xy + singleStepOffset * vec2(-8.0, -5.0);\n" +
    "blurCoordinates[12] = textureCoordinate.xy + singleStepOffset * vec2(0.0, -6.0);\n" +
    "blurCoordinates[13] = textureCoordinate.xy + singleStepOffset * vec2(0.0, 6.0);\n" +
    "blurCoordinates[14] = textureCoordinate.xy + singleStepOffset * vec2(6.0, 0.0);\n" +
    "blurCoordinates[15] = textureCoordinate.xy + singleStepOffset * vec2(-6.0, 0.0);\n" +
    "blurCoordinates[16] = textureCoordinate.xy + singleStepOffset * vec2(-4.0, -4.0);\n" +
    "blurCoordinates[17] = textureCoordinate.xy + singleStepOffset * vec2(-4.0, 4.0);\n" +
    "blurCoordinates[18] = textureCoordinate.xy + singleStepOffset * vec2(4.0, -4.0);\n" +
    "blurCoordinates[19] = textureCoordinate.xy + singleStepOffset * vec2(4.0, 4.0);\n" +
    "blurCoordinates[20] = textureCoordinate.xy + singleStepOffset * vec2(-2.0, -2.0);\n" +
    "blurCoordinates[21] = textureCoordinate.xy + singleStepOffset * vec2(-2.0, 2.0);\n" +
    "blurCoordinates[22] = textureCoordinate.xy + singleStepOffset * vec2(2.0, -2.0);\n" +
    "blurCoordinates[23] = textureCoordinate.xy + singleStepOffset * vec2(2.0, 2.0);\n" +

    "float sampleColor = texture2D(inputImageTexture, textureCoordinate).g * 22.0;\n" +
    "sampleColor += texture2D(inputImageTexture, blurCoordinates[0]).g;\n" +
    "sampleColor += texture2D(inputImageTexture, blurCoordinates[1]).g;\n" +
    "sampleColor += texture2D(inputImageTexture, blurCoordinates[2]).g;\n" +
    "sampleColor += texture2D(inputImageTexture, blurCoordinates[3]).g;\n" +
    "sampleColor += texture2D(inputImageTexture, blurCoordinates[4]).g;\n" +
    "sampleColor += texture2D(inputImageTexture, blurCoordinates[5]).g;\n" +
    "sampleColor += texture2D(inputImageTexture, blurCoordinates[6]).g;\n" +
    "sampleColor += texture2D(inputImageTexture, blurCoordinates[7]).g;\n" +
    "sampleColor += texture2D(inputImageTexture, blurCoordinates[8]).g;\n" +
    "sampleColor += texture2D(inputImageTexture, blurCoordinates[9]).g;\n" +
    "sampleColor += texture2D(inputImageTexture, blurCoordinates[10]).g;\n" +
    "sampleColor += texture2D(inputImageTexture, blurCoordinates[11]).g;\n" +
    "sampleColor += texture2D(inputImageTexture, blurCoordinates[12]).g * 2.0;\n" +
    "sampleColor += texture2D(inputImageTexture, blurCoordinates[13]).g * 2.0;\n" +
    "sampleColor += texture2D(inputImageTexture, blurCoordinates[14]).g * 2.0;\n" +
    "sampleColor += texture2D(inputImageTexture, blurCoordinates[15]).g * 2.0;\n" +
    "sampleColor += texture2D(inputImageTexture, blurCoordinates[16]).g * 2.0;\n" +
    "sampleColor += texture2D(inputImageTexture, blurCoordinates[17]).g * 2.0;\n" +
    "sampleColor += texture2D(inputImageTexture, blurCoordinates[18]).g * 2.0;\n" +
    "sampleColor += texture2D(inputImageTexture, blurCoordinates[19]).g * 2.0;\n" +
    "sampleColor += texture2D(inputImageTexture, blurCoordinates[20]).g * 3.0;\n" +
    "sampleColor += texture2D(inputImageTexture, blurCoordinates[21]).g * 3.0;\n" +
    "sampleColor += texture2D(inputImageTexture, blurCoordinates[22]).g * 3.0;\n" +
    "sampleColor += texture2D(inputImageTexture, blurCoordinates[23]).g * 3.0;\n" +

    "sampleColor = sampleColor / 62.0;\n" +
    "vec3 centralColor = texture2D(inputImageTexture, textureCoordinate).rgb;\n" +
    "float highpass = centralColor.g - sampleColor + 0.5;\n" +
    "for(int i = 0; i < 5 ; i++){\n" +
    "highpass = hardlight(highpass);\n" +
    "}\n" +
    "float lumance = dot(centralColor, W);\n" +
    "float alpha = pow(lumance, params.r);\n" +
    "vec3 smoothColor = centralColor + (centralColor-vec3(highpass))*alpha*0.1;\n" +
    "smoothColor.r = clamp(pow(smoothColor.r, params.g),0.0,1.0);\n" +
    "smoothColor.g = clamp(pow(smoothColor.g, params.g),0.0,1.0);\n" +
    "smoothColor.b = clamp(pow(smoothColor.b, params.g),0.0,1.0);\n" +
    "vec3 lvse = vec3(1.0)-(vec3(1.0)-smoothColor)*(vec3(1.0)-centralColor);\n" +
    "vec3 bianliang = max(smoothColor, centralColor);\n" +
    "vec3 rouguang = 2.0*centralColor*smoothColor + centralColor*centralColor - 2.0*centralColor*centralColor*smoothColor;\n" +
    "vec4 tmpcolor = vec4(mix(centralColor, lvse, alpha), 1.0);\n" +
    "tmpcolor.rgb = mix(tmpcolor.rgb, bianliang, alpha);\n" +
    "tmpcolor.rgb = mix(tmpcolor.rgb, rouguang, params.b);\n" +
    "vec3 satcolor = tmpcolor.rgb * saturateMatrix;\n" +
    "gl_FragColor.rgb = mix(tmpcolor.rgb, satcolor, params.a);\n" +
    "}\n";

  VHBeautifyFilter::VHBeautifyFilter() :
    VHBeautifyFilter(0) {
  }

  VHBeautifyFilter::VHBeautifyFilter(int level) :
    VHGPUImageFilter(NO_FILTER_VERTEX_SHADER, BEAUTIFY_FRAGMENT_SHADER),
    beautifyLevel(level) {
  
  }

  VHBeautifyFilter::~VHBeautifyFilter() {

  }

  void VHBeautifyFilter::SetBeautifyLevel(int level) {
    beautifyLevel = level;
    switch (beautifyLevel) {
    case 1:
      setFloatVec4(beautifyLocation, new float[4] {1.5f, 1.5f, 0.15f, 0.15f});
      break;
    case 2:
      setFloatVec4(beautifyLocation, new float[4] {0.8f, 0.9f, 0.2f, 0.2f});
      break;
    case 3:
      setFloatVec4(beautifyLocation, new float[4] {0.6f, 0.8f, 0.25f, 0.25f});
      break;
    case 4:
      setFloatVec4(beautifyLocation, new float[4] {0.4f, 0.7f, 0.38f, 0.3f});
      break;
    case 5:
      setFloatVec4(beautifyLocation, new float[4] {0.33f, 0.63f, 0.4f, 0.35f});
      break;
    default:
      break;
    }
  }

  void VHBeautifyFilter::onOutputSizeChanged(int width, int height) {
    VHGPUImageFilter::onOutputSizeChanged(width, height);
    setTexelSize(width, height);
  }

  void VHBeautifyFilter::onInit() {
    VHGPUImageFilter::onInit();
    beautifyLocation = glGetUniformLocation(getProgram(), "params");
    singleStepOffsetLocation = glGetUniformLocation(getProgram(), "singleStepOffset");
  }

  void VHBeautifyFilter:: onInitialized() {
    VHGPUImageFilter::onInitialized();
    SetBeautifyLevel(beautifyLevel);
  }

  void VHBeautifyFilter::setTexelSize(float w, float h) {
    setFloatVec2(singleStepOffsetLocation, new float[2] {2.0f / w, 2.0f / h});
  }
}
