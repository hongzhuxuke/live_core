#ifndef __VH_LIVE_EVENT_DEF_INCLUDE__
#define __VH_LIVE_EVENT_DEF_INCLUDE__
enum LiveEventID {
   /*
   push event
   */
   PUSH_EVENT_Connect_Success = 52001,                // 已经连接推流服务器
   PUSH_EVENT_Stream_Start = 52003,                   // 已经开始推流
   PUSH_EVENT_Stream_Info = 52005,                    // 推流信息
   PUSH_EVENT_Upstream_Kps = 52007,                   // 推流速率

   PUSH_ERROR_Net_Disconnect = -54001,                // 网络断连,且经多次重连无效,更多重试请自行重启推流
   PUSH_ERROR_Rejected_InvalidToken = -54101,         // 推流拒绝，token不合法
   PUSH_ERROR_Rejected_NotInWhiteList = -54102,       // 推流拒绝，不在白名单
   PUSH_ERROR_Rejected_InBlackList = -54103,          // 推流拒绝，在黑名单
   PUSH_ERROR_Rejected_AlreadyExists = -54104,        // 推流拒绝，存在相同流名
   PUSH_ERROR_Rejected_Prohibit = -54105,             // 推流拒绝,被管理员禁止 
   PUSH_ERROR_Unsupported_Resolution = -54006,        // 不支持的视频分辨率
   PUSH_ERROR_Unsupported_Samplerate = -54007,        // 不支持的音频采样率

   PUSH_WARNING_Net_Busy = 51001,                     // 网络状况不佳：上行带宽太小，上传数据受阻
   PUSH_WARNING_Reconnect = 51002,                    // 网络断连, 已启动自动重连 (自动重连连续失败超过三次会放弃)
   PUSH_WARNING_HW_Acceleration_Failed = 51004,       // 硬编码启动失败，采用软编码
   PUSH_WARNING_Connect_Failed = 51006,               // RTMP服务器连接失败
   PUSH_WARNING_Handshake_Failed = 51008,             // RTMP服务器握手失败
   PUSH_WARNING_Server_Disconnect = 51010,	         // RTMP服务器主动断开

   /*
   play event
   */
   PLAY_EVENT_Connect_Success = 62001,                // 已经连接播放服务器
   PLAY_EVENT_Stream_Start = 62003,                   // 已经连接播放服务器，开始播放
   PLAY_EVENT_Stream_Info = 62005,                    // 已经连接播放服务器，开始播放,获取到播放流信息      
   PUSH_EVENT_Downstream_Kps = 62007,                 // 播放码流速率
   PLAY_EVTNT_FIRST_I_FRAME = 62009,                  // 渲染首个视频数据包(IDR)
   PLAY_EVTNT_FIRST_A_FRAME = 62011,                  // 渲染首个音频数据包
   PLAY_EVENT_Stream_BufferEmpty = 62013,             // 播放中，播放buffer为空,开始缓冲
   PLAY_EVENT_Stream_BufferFull = 62014,              // 缓冲中，播放buffer满   

   PLAY_ERROR_Net_Disconnect = -64101,                // 网络断连,且经多次重连无效,更多重试请自行重启播放 

   PLAY_WARNING_Reconnect = 61002,                    // 网络断连, 已启动自动重连 (自动重连连续失败超过三次会放弃)
   PLAY_WARNING_HW_Acceleration_Failed = 61004,       // 硬解启动失败，采用软解
   PLAY_WARNING_Video_Discontinuity = 61005,          // 当前视频帧不连续，可能丢帧
   PLAY_WARNING_Connect_Failed = 61006,               // 播放服务器连接失败
   PLAY_WARNING_Handshake_Failed = 61008,             // 播放服务器握手失败
   PLAY_WARNING_Server_Disconnect = 61010,	          // 播放服务器主动断开   
   PLAY_WARNING_Video_DecodeFailed = 61012,           // 当前视频帧解码失败
   PLAY_WARNING_Audio_DecodeFailed = 61014,           // 当前音频帧解码失败

};

#endif

