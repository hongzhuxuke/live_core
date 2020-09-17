========================================================================
                        静态库： multitcp 模块概述
========================================================================
1，cJSON.h, cJSON.c 文件 C 语言编写的json库；
2，m_io.h, m_io.cpp 文件 此模块对外的C接口，主要是为了适配srslibrtmp接口；
3，m_io_defines.h, m_io_defines.cpp 文件 定义 MPacket 和 MPacketPool；
4，m_io_log.h, m_io_log.cpp 文件 log 模块；
5，m_io_peer.h, m_io_peer.cpp 文件 此模块的主要业务类；
6，m_io_rate_control.h, m_io_rate_control.cpp 文件 码率控制模块；
7，m_io_single_conn.h, m_io_single_conn.cpp 文件 单个tcp连接；
8，m_io_socket.h, m_io_socket.cpp 文件 底层socket的封装；
9，m_io_sys.h, m_io_sys.cpp 文件  跨平台接口；