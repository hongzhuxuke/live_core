========================================================================
    静态库：srs-librtmp 项目概述
========================================================================
编译采用CMAKE
1 代码来自srs ./configure --export-librtmp-project 单独提取的工程。并对部分头文件路径和引用做了修改。

CMAKE 编译过程。
1 安装CMAKE 3.7 及以上。

2 进入srs-librtmp目录
   linux 下：
	mkdir build
	cd build
	cmake .. -G "Unix Makefiles"
	make
   windows 下:
	mkdir build
	cd build
	cmake .. -G "Visual Studio 12"    //means vs2013
	打开build 目录下的ALL_BUILD.vcxproj 编译srs_librtmp工程。

	
说明：
	1 输出目录为srs_librtmp 目录下的 libs include 和bin 。
		libs 下是生产.a 或者.lib。
		bin 下是一些测试程序，windows下编译会出错，需要手动修改头文件，linux 下可以。
 		include 下是使用库的头文件。windows 下 需要手动copy 从src/libs/srs_librtmp.hpp copy 到 include中，并更名为 srs_librtmp.h
	

	2 windows 下最好使用vs2013 使用vs2015 生成文件会有类型重定义的bug，需要手动修改头文件。
