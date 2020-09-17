# Image Enhancement Module
## Structure
``` 
|   CMakelists.txt
|   README.md
|                   
+---src
|       ImageEnhance.cpp
|       ImageEnhance.h
|       
\---test
        example.cpp
```
## Brief
This module is used to optimize the visual effect of the video:<\br>
1. Edge enhancement, which could clarify the character or other symbol
2. Brightness improvement, which helps to brighter the dark part
3. Denoise, which suppress the flicker of noise pixel

## Build Guide
Please make sure you have installed **CMAKE** correctly, before you build it.  
<windows>
Visual Studio is recommended at Windows Platform, here is a build command example(using *vs 2017*):
``` bash
	cd video_process
	md vsproject
	cd vsproject
	cmake .. -G "Visual Studio 15 2017"
```
*test/example.cpp* is a demo to help quick start with this module, if you want to generate demo, please uncomment last part in CMakelist.txt

``` shell
 add_executable(demo ${PROJECT_SOURCE_DIR}/test/example.cpp)
 target_link_libraries(demo vhall_video_enhance)
 if(MSVC)
    set_target_properties(demo PROPERTIES LINK_FLAGS "/SAFESEH:NO")
 endif()
```
	
	
	