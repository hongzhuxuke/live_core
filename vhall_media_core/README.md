## Vhall Media Core library
### library build

read README.build

### Media_core 模块说明

<pre>
-vhall_media_core         - 主目录 
|-3rdparty                - 包含第三方库
  |-json                  - json库
  |-libjingle             - libjingle库
  |-noisesuppression      - noisesuppression库,噪音抑制
  |-srslibrtmp            - srslibrtmo库
  |-ffmpeg                - ffmpeg 头文件 (linux and windows will not use this, it will use the same files in build dir with different platform)
  |-ffmpeg3.2             - ffmpeg3.2 头文件 (linux and windows will not use this, it will use the same files in build dir with different platform)
  |-x264                  - x264头文件 (linux and windows will not use this, it will use the same files in build dir with different platform)
  
|-build                   - 编译目录，里面包含ios和android的编译工程和脚本
  |-ios                   - ios编译工程
    |-Vhall3rdLib.xcodeproj -第三方库编译工程
    |-VhallLiveApi.xcodeproj -media_core编译工程            
  |-macosx                - macos编译工程
    |-Vhall3rdLib.xcodeproj -第三方库编译工程
    |-VhallLiveApi.xcodeproj -media_core编译工程 
  |-android               - android编译工程
    |-jni                 - jni文件
    |-libs                - 生成库的存放目录
    |-build_jni.sh        - 编译脚本
  |-windows
	  |-libsx86
		  |-ffmpeg3.2       - ffmpeg3.2 头文件和编译好windows_x86的库
		  |-X264            - x264 头文件和编译好windows_x86的库
		  |-vmaf			- 封装后的vmaf 头文件和编译好的windows_x86的库（含Debug版本）
	  |-libsx86_64          - (现在还没有建好，目前windows下只编译32位的x86)
		  |-ffmpeg3.2       - ffmpeg3.2 头文件和编译好windows_x64的库 
		  |-X264            - x264 头文件和编译好windows_x64的库
		  |-vmaf			- 封装后的vmaf 头文件和编译好的windows_x64的库（含Debug版本）
  |-linux
	  |-libsx86_64
		  |-ffmpeg          - ffmpeg 头文件和编译好linux_x86_64的库
		  |-X264            - x264 头文件和编译好linux_x86_64的库
	  |-libsx86             - (目前还没有建好，目前linux 下只编译64位的x86_64)
		  |-ffmpeg          - ffmpeg 头文件和编译好linux_x86的库
		  |-X264            - x264 头文件和编译好linux_x86的库	
	
|-common                  - 公共类目录
|-encoder                 - 媒体编码目录
|-muxers                  - 媒体分发传输目录
|-os                      - 因平台导致业务不同的文件目录
|-rtmpplayer              - 播放相关目录
|-rtmppush                - 推流相关目录
|-mediaquality			  - 媒体质量目录(测试中)
|-utility                 - 工具类目录
|-api                     - 接口目录，里面放着接口类
|-test                    - 测试目录(目前部分不可用)

</pre>