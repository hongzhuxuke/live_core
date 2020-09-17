#ifndef __M_RATE_CONTROL__
#define __M_RATE_CONTROL__

#include <list>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
/*
#include <assert.h>
#include <ctype.h>
#include <stddef.h>
#include <errno.h>
#include <stdarg.h>
#include <time.h>
*/
#include <vector>

//#if defined(WIN32)
//#include <winsock2.h>
//#include <ws2tcpip.h>
//#include <Mstcpip.h>
//#endif
#include "m_io_sys.h"
#include "stdint.h"
#include "m_io_log.h"

//typedef short int16_t;

#define AVERAGE_SIZE_BW 40   //15s
#define AVERAGE_SIZE_PER 2
#define AVERAGE_SIZE_OWD 6
#define LESS_DATA_SIZE 10
#define MAX_CONN_NUM 5
#define MIN_CONN_NUM 5
#define OWD_CONTROL

typedef void * connection_publisher;
typedef void * session_publisher;
typedef int connection_id;

typedef struct 
{
	connection_id *con_id;
	std::list <int16_t> bandwidth;   //history
	int16_t send_rate;        //final
	int16_t packet_counter_left;   //remaining

	std::list <int> owd;
	int send_owd;
	int less_owd;
	int less_owd_count;
	std::list<int> less_data;
	int less_data_because_network;
	uint64_t last_time_feedback_time;
	uint64_t last_time_not_allow_send;
	int should_close;
	int stop_send;
	uint64_t last_time_stop_send;
	int isstandby_con;
}connection_publisher_multitcp;

typedef struct
{
	std::list<connection_publisher_multitcp *> con_pub_multitcp;
	int16_t total_packet_counter;
	int16_t total_bw;
	int should_add_conn;
	uint64_t time_last_try_add_conn;
	int16_t last_total_connection_number;
	int16_t last_total_bw;
	int16_t init_state;
	uint64_t time_sess_begin;
}session_publisher_multitcp;

//uint64_t os_gettime_ms(void);

int getOverallAvgOWD(session_publisher *sess_publisher);

int getTotalBW(session_publisher *sess_publisher);

int isNeedClose(connection_publisher *con_publisher);

int sendDecision(connection_publisher *con_publisher, int sleeptime, uint64_t cur_time);

int AddConnDecision(session_publisher *sess_publisher);

int isNeedAddConn(session_publisher *sess_publisher);

int isLargestOWD(connection_publisher *con_publisher, session_publisher *sess_publisher);

void setStandbyCon(connection_publisher *con_publisher, int standby);

//--------------------------多TCP中的一个-----------------------------
//called by publisher  connection
//初始化SOCKET调用
int publisher_multitcp_connection_control_init(connection_id* con_id, connection_publisher *con_publisher);
//销毁SOCKET调用
int publisher_multitcp_connection_control_destroy(connection_publisher *con_publisher);
//收到反馈消息的时候,载荷内容和长度
int publisher_multitcp_connection_control_on_feedback(char *feedback,int feedbackLength, connection_publisher *con_publisher);
//判断该SOCKET是否可以发送，返回1可以发送，返回0不可以发送
int isSend(connection_publisher *con_publisher, session_publisher *sess_publisher);
//-------------------------M_IO连接---------------------------
//called by publisher  session
int publisher_multitcp_session_control_init(session_publisher *sess_publisher);
//链接全部断开调用此函数
int publisher_multitcp_session_control_destroy(session_publisher *sess_publisher);

//每建立一个连接调用一次
int add_connection(connection_publisher *con_publisher, session_publisher *sess_publisher);
//销毁的时候先调用delete_connection 再调用publisher_multitcp_connection_control_destroy

//每销毁一个连接调用一次
int delete_connection(connection_publisher *con_publisher, session_publisher *sess_publisher);

#endif //__M_RATE_CONTROL__
