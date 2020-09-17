#include "m_io_rate_control.h"
#include "cJSON.h"
//#ifdef _WIN32
//#include <windows.h>
//#endif

int publisher_multitcp_connection_control_init(connection_id* con_id, connection_publisher *con_publisher)
{
//   OutputDebugString(TEXT(__FUNCTION__));
//   OutputDebugString(TEXT("\n"));

   connection_publisher_multitcp *con_pub_multitcp = new connection_publisher_multitcp;
   *con_publisher = con_pub_multitcp;
   con_pub_multitcp->con_id = con_id;
   con_pub_multitcp->bandwidth.clear();
   con_pub_multitcp->send_rate = 160;
   con_pub_multitcp->packet_counter_left = 10;

   con_pub_multitcp->owd.clear();
   con_pub_multitcp->send_owd = 0;
   con_pub_multitcp->less_owd = 0;
   con_pub_multitcp->less_owd_count = 0;
   con_pub_multitcp->less_data.clear();
   con_pub_multitcp->less_data_because_network = 0;
   con_pub_multitcp->last_time_feedback_time = 0;
   con_pub_multitcp->last_time_not_allow_send = 0;
   con_pub_multitcp->should_close = 0;
   con_pub_multitcp->stop_send = 0;
   con_pub_multitcp->last_time_stop_send = 0;
   con_pub_multitcp->isstandby_con = 0;
   return 0;
}

int publisher_multitcp_connection_control_destroy(connection_publisher *con_publisher)
{
//   OutputDebugString(TEXT(__FUNCTION__));
//   OutputDebugString(TEXT("\n"));

   if (con_publisher != NULL)
   {
	   connection_publisher_multitcp *con_pub_multitcp = (connection_publisher_multitcp *)(*con_publisher);
	   con_pub_multitcp->bandwidth.clear();
	   con_pub_multitcp->owd.clear(); 
	   con_pub_multitcp->less_data.clear();
	   delete con_pub_multitcp;
      con_pub_multitcp = NULL;
   }
   return 0;
}

int publisher_multitcp_session_control_init(session_publisher *sess_publisher)
{
//   OutputDebugString(TEXT(__FUNCTION__));
//   OutputDebugString(TEXT("\n"));

   session_publisher_multitcp *sess_pub_multitcp = new session_publisher_multitcp;
   *sess_publisher = sess_pub_multitcp;
   sess_pub_multitcp->con_pub_multitcp.clear();
   sess_pub_multitcp->total_packet_counter = 0;
   sess_pub_multitcp->total_bw = 0;
   sess_pub_multitcp->should_add_conn = 0;
   sess_pub_multitcp->time_last_try_add_conn = get_systime_ms();
   sess_pub_multitcp->time_sess_begin = get_systime_ms();
   sess_pub_multitcp->init_state = 1;
   sess_pub_multitcp->last_total_bw = 0;
   return 0;
}

int publisher_multitcp_session_control_destroy(session_publisher *sess_publisher)
{
//   OutputDebugString(TEXT(__FUNCTION__));
//   OutputDebugString(TEXT("\n"));

   if (sess_publisher != NULL)
   {
	   session_publisher_multitcp *sess_pub_multitcp = static_cast<session_publisher_multitcp *>(*sess_publisher);
	   sess_pub_multitcp->con_pub_multitcp.clear();
	   delete sess_pub_multitcp;
      sess_pub_multitcp = NULL;
   }
   return 0;
}

int publisher_multitcp_connection_control_on_feedback(char *feedback,int feedbackLength,connection_publisher *con_publisher)
{

   if (con_publisher == NULL || NULL == feedback)
   {
	   return 0;
   }
   connection_publisher_multitcp *con_pub_multitcp = static_cast<connection_publisher_multitcp *> (*con_publisher);
   if (con_pub_multitcp == NULL)
   {
	   return 0;
   }
   if (con_pub_multitcp->isstandby_con == 1)   //only for retransmission, without collecting statistic
   {
	   return 0;
   }

   if (con_pub_multitcp->last_time_feedback_time == 0)
   {
	   con_pub_multitcp->last_time_feedback_time = get_systime_ms();
   }
   uint64_t current_time = get_systime_ms();
   uint64_t diff_time = current_time - con_pub_multitcp->last_time_feedback_time;

   cJSON * pJson = cJSON_Parse(feedback);
   if (NULL == pJson)
   {
	   M_IO_Log(M_IO_LOGINFO, "parse faild");
	   return 0;
   }
   //==================bw======================//
   cJSON * pSub = cJSON_GetObjectItem(pJson, "bw");
   if (NULL == pSub)
   {
	   M_IO_Log(M_IO_LOGINFO, "get bw from json failed");
   } 
   else
   {
	   if (diff_time > 1000)
	   {
		   for (int i = 0; i < static_cast<int> (diff_time / 500); i++)
		   {
			   con_pub_multitcp->bandwidth.push_back(32);
		   }
	   }
	   con_pub_multitcp->bandwidth.push_back(pSub->valueint < 32 ? 32 : pSub->valueint);
   }
   while (con_pub_multitcp->bandwidth.size() > AVERAGE_SIZE_BW)
   {
	   con_pub_multitcp->bandwidth.pop_front();
   }
   con_pub_multitcp->send_rate = 0;
   for (std::list<int16_t>::iterator iter = con_pub_multitcp->bandwidth.begin(); iter != con_pub_multitcp->bandwidth.end(); iter++)
   {
	   con_pub_multitcp->send_rate += (*iter);
   }
   if (con_pub_multitcp->bandwidth.size() > 0)
   {
	   con_pub_multitcp->send_rate = con_pub_multitcp->send_rate / static_cast<int16_t> (con_pub_multitcp->bandwidth.size());
   }
   else
   {
	   con_pub_multitcp->send_rate = 32;
   }
   //==================bw======================//
   //==================less data======================//
   pSub = cJSON_GetObjectItem(pJson, "les");
   if (NULL == pSub)
   {
	   M_IO_Log(M_IO_LOGCRIT, "get less data from json failed");
   } 
   else
   {   
	   if (diff_time > 1000)
	   {
		   for (int i = 0; i < static_cast<int> (diff_time / 500); i++)
		   {
			   con_pub_multitcp->less_data.push_back(1);
		   }
	   } 
	   con_pub_multitcp->less_data.push_back(pSub->valueint);
   }
   while (con_pub_multitcp->less_data.size() > LESS_DATA_SIZE)
   {
	   con_pub_multitcp->less_data.pop_front();
   }
   con_pub_multitcp->less_data_because_network = 0;
   for (std::list<int>::iterator iter = con_pub_multitcp->less_data.begin(); iter != con_pub_multitcp->less_data.end(); iter++)
   {
	   if ((*iter) == 1)
	   {
		   con_pub_multitcp->less_data_because_network++;
	   }
   }
   if (con_pub_multitcp->less_data_because_network > (con_pub_multitcp->less_data.size() * 2 / 3))
   {
	   con_pub_multitcp->less_data_because_network = 1;
   }
   else
   {
	   con_pub_multitcp->less_data_because_network = 0;
   }
   //==================less data======================//
   //==================owd======================//
   pSub = cJSON_GetObjectItem(pJson, "owd");  //-1 no data;  else corrent data
   if (NULL == pSub)
   {
	   M_IO_Log(M_IO_LOGINFO, "get owd from json failed");
   } 
   else
   {
	   if (diff_time > 1000)
	   {
		   for (int i = 0; i < static_cast<int> (diff_time / 500); i++)
		   {
			   con_pub_multitcp->owd.push_back(1000);
		   }
	   }
	   if (pSub->valueint == -1)
	   {
		   con_pub_multitcp->owd.push_back(30);
		   
		   con_pub_multitcp->less_owd_count++;
		   if (con_pub_multitcp->less_owd_count >= 4)
		   {
			   con_pub_multitcp->less_owd = 1;
		   }
	   }
	   else if (pSub->valueint >= 0)
	   {
		   con_pub_multitcp->less_owd_count = 0;
		   con_pub_multitcp->less_owd = 0;
		   con_pub_multitcp->owd.push_back(pSub->valueint);
	   }
   }
   while (con_pub_multitcp->owd.size() > AVERAGE_SIZE_OWD)
   {
	   con_pub_multitcp->owd.pop_front();
   }
   con_pub_multitcp->send_owd = 0;
   for (std::list<int>::iterator iter = con_pub_multitcp->owd.begin(); iter != con_pub_multitcp->owd.end(); iter++)
   {
	   con_pub_multitcp->send_owd += (*iter);
   }
   if (con_pub_multitcp->owd.size() > 0)
   {
	   con_pub_multitcp->send_owd = con_pub_multitcp->send_owd / static_cast<int> (con_pub_multitcp->owd.size());
   }
   //==================owd======================//
   cJSON_Delete(pJson);
   con_pub_multitcp->last_time_feedback_time = get_systime_ms();
   return 0;
}

int isSend(connection_publisher *con_publisher, session_publisher *sess_publisher)
{
	if (con_publisher == NULL || sess_publisher == NULL)
	{
		return 0;
	}

	connection_publisher_multitcp *con_pub_multitcp = static_cast<connection_publisher_multitcp *> (*con_publisher);
	session_publisher_multitcp *sess_pub_multitcp = static_cast<session_publisher_multitcp *> (*sess_publisher);
	if (con_pub_multitcp == NULL || sess_pub_multitcp == NULL)
	{
		return 0;
	}
	if (con_pub_multitcp->isstandby_con == 1)
	{
		return 1;
	}

	int res = 1;

#ifdef OWD_CONTROL
	uint64_t cur_time = get_systime_ms();
	int avg_owd = getOverallAvgOWD(sess_publisher);
	if (con_pub_multitcp->less_data_because_network == 1 && con_pub_multitcp->send_owd > 5000  && con_pub_multitcp->send_owd >= 5 * avg_owd)
	{
		if (sess_pub_multitcp->con_pub_multitcp.size() <= MIN_CONN_NUM)
		{
			con_pub_multitcp->should_close = 0;
		}
		else
		{
			M_IO_Log(M_IO_LOGCRIT, "Connection should be closed!");
			con_pub_multitcp->should_close = 1;
			res = 0;
			return res;
		}
	}
	con_pub_multitcp->should_close = 0;
//	M_IO_Log(M_IO_LOGCRIT, "==================%d, %d", con_pub_multitcp->send_owd, con_pub_multitcp->stop_send);
	for (std::list<connection_publisher_multitcp *>::iterator iter = sess_pub_multitcp->con_pub_multitcp.begin(); iter != sess_pub_multitcp->con_pub_multitcp.end(); iter++)
	{
		if ((*iter)->stop_send == 1)
		{
			con_pub_multitcp->stop_send = 0;
			return res;
		}
	}

	if (con_pub_multitcp->send_owd >= 3000)
	{
		if (con_pub_multitcp->stop_send == 0)
		{
			if (isLargestOWD(con_publisher, sess_publisher))
			{
				con_pub_multitcp->stop_send = 1;
				con_pub_multitcp->last_time_stop_send = cur_time;
			}
		}
		else
		{
			if (cur_time - con_pub_multitcp->last_time_stop_send >= 500)
			{
				con_pub_multitcp->stop_send = 0;
			}
		}
	}
	else
	{
		con_pub_multitcp->stop_send = 0;
	}

	if (con_pub_multitcp->stop_send == 0)
	{
		res = 1;
	}
	else
	{
		res = 0;
	}
	return res;

	if (con_pub_multitcp->less_owd == 1)
	{
		res = 1;
	}
	else
	{
		if (con_pub_multitcp->send_owd <= 1000)
		{
			res = 1;
		}
		else if (con_pub_multitcp->send_owd <= 2000)
		{
			if (con_pub_multitcp->send_owd < avg_owd || con_pub_multitcp->less_data_because_network == 0)
			{
				res = sendDecision(con_publisher, 10, cur_time);
				M_IO_Log(M_IO_LOGCRIT, "sendDecision1 res = %d", res);
			}
			else
			{
				res = sendDecision(con_publisher, 20, cur_time);
				M_IO_Log(M_IO_LOGCRIT, "sendDecision2 res = %d", res);
			}
		}
		else if (con_pub_multitcp->send_owd <= 3000)
		{
			if (con_pub_multitcp->send_owd < avg_owd)
			{
				res = sendDecision(con_publisher, 10, cur_time);
				M_IO_Log(M_IO_LOGCRIT, "sendDecision3 res = %d", res);
			}
			else if (con_pub_multitcp->less_data_because_network == 0 && con_pub_multitcp->send_owd <= 2 * avg_owd)
			{
				res = sendDecision(con_publisher, 20, cur_time);
				M_IO_Log(M_IO_LOGCRIT, "sendDecision4 res = %d", res);
			}
			else
			{
				res = sendDecision(con_publisher, 40, cur_time);
				M_IO_Log(M_IO_LOGCRIT, "sendDecision5 res = %d", res);
			}
		}
		else if (con_pub_multitcp->send_owd <= 5000)
		{
			if (con_pub_multitcp->send_owd < avg_owd)
			{
				res = sendDecision(con_publisher, 10, cur_time);
				M_IO_Log(M_IO_LOGCRIT, "sendDecision6 res = %d", res);
			}
			else if (con_pub_multitcp->less_data_because_network == 0 && con_pub_multitcp->send_owd <= 2 * avg_owd)
			{
				res = sendDecision(con_publisher, 40, cur_time);
				M_IO_Log(M_IO_LOGCRIT, "sendDecision7 res = %d", res);
			}
			else
			{
				res = sendDecision(con_publisher, 60, cur_time);
				M_IO_Log(M_IO_LOGCRIT, "sendDecision8 res = %d", res);
			}
		}
		else
		{
			res = sendDecision(con_publisher, 80, cur_time);
			M_IO_Log(M_IO_LOGCRIT, "sendDecision9 res = %d", res);
		}
	}

	return res;

#endif 
   
   if (sess_pub_multitcp->total_packet_counter <= 0)
   {
	   for (std::list<connection_publisher_multitcp *>::iterator iter = sess_pub_multitcp->con_pub_multitcp.begin(); iter != sess_pub_multitcp->con_pub_multitcp.end(); iter++)
	   {
		   (*iter)->packet_counter_left = (*iter)->send_rate / 32 * 2;
		   sess_pub_multitcp->total_packet_counter += (*iter)->packet_counter_left;
		   (*iter)->bandwidth.clear();
	   }
   }
   if (con_pub_multitcp->packet_counter_left <= 0)
   {
	   res = 0;
   }
   else
   {
	   con_pub_multitcp->packet_counter_left -= 2;
	   sess_pub_multitcp->total_packet_counter -= 2;
	   res = 1;
   }
   return res;
}

int add_connection(connection_publisher *con_publisher, session_publisher *sess_publisher)
{
//   OutputDebugString(TEXT(__FUNCTION__));
//   OutputDebugString(TEXT("\n"));

   if (con_publisher == NULL || sess_publisher == NULL)
   {
	   return -1;
   }
   connection_publisher_multitcp *con_pub_multitcp = static_cast<connection_publisher_multitcp *> (*con_publisher);
   if (con_pub_multitcp == NULL)
   {
	   return -1;
   }
   con_pub_multitcp->packet_counter_left = 10;
   session_publisher_multitcp *sess_pub_multitcp = static_cast<session_publisher_multitcp *> (*sess_publisher);
   sess_pub_multitcp->con_pub_multitcp.push_back(con_pub_multitcp);
   sess_pub_multitcp->total_packet_counter += 10;
   return 0;
}

int delete_connection(connection_publisher *con_publisher, session_publisher *sess_publisher)
{
//   OutputDebugString(TEXT(__FUNCTION__));
//   OutputDebugString(TEXT("\n"));

   if (con_publisher == NULL || sess_publisher == NULL)
   {
	   return -1;
   }
   connection_publisher_multitcp *con_pub_multitcp = static_cast<connection_publisher_multitcp *> (*con_publisher);
   session_publisher_multitcp *sess_pub_multitcp = static_cast<session_publisher_multitcp *> (*sess_publisher);
   if (con_pub_multitcp == NULL || sess_pub_multitcp == NULL)
   {
	   return -1;
   }
   for (std::list<connection_publisher_multitcp *>::iterator iter = sess_pub_multitcp->con_pub_multitcp.begin(); iter != sess_pub_multitcp->con_pub_multitcp.end(); iter++)
   {
	   if ((*iter) == con_pub_multitcp)
	   {
		   sess_pub_multitcp->con_pub_multitcp.erase(iter);
		   break;
	   }
   }
  
   return 0;
}

int getOverallAvgOWD(session_publisher *sess_publisher)
{
	int overall_avg_owd = -1;
	int conn_count = 0;
	if (sess_publisher == NULL)
	{
		return overall_avg_owd;
	}
	session_publisher_multitcp *sess_pub_multitcp = static_cast<session_publisher_multitcp *> (*sess_publisher);
	if (sess_pub_multitcp == NULL)
	{
		return overall_avg_owd;
	}
	overall_avg_owd = 0;
	for (std::list<connection_publisher_multitcp *>::iterator iter = sess_pub_multitcp->con_pub_multitcp.begin(); iter != sess_pub_multitcp->con_pub_multitcp.end(); iter++)
	{
		if ((*iter)->isstandby_con != 1)
		{
			overall_avg_owd = overall_avg_owd + (*iter)->send_owd;
			conn_count++;
		}
	}
	if (conn_count > 0)
	{
		overall_avg_owd = overall_avg_owd / conn_count;
	}
	return overall_avg_owd;
}

int isNeedClose(connection_publisher *con_publisher)
{
	int res = 1;
	if (con_publisher == NULL)
	{
		return res;
	}
	connection_publisher_multitcp *con_pub_multitcp = static_cast<connection_publisher_multitcp *> (*con_publisher);
	if (con_pub_multitcp == NULL)
	{
		return res;
	}
	if (con_pub_multitcp->isstandby_con == 1)
	{
		return 0;
	}
	if (con_pub_multitcp->should_close == 1)
	{
		return res;
	}
	return 0;
}

int sendDecision(connection_publisher *con_publisher, int sleeptime, uint64_t cur_time)
{
	int res = 0;
	if (con_publisher == NULL)
	{
		return res;
	}
	connection_publisher_multitcp *con_pub_multitcp = static_cast<connection_publisher_multitcp *> (*con_publisher);
	if (con_pub_multitcp == NULL)
	{
		return res;
	}

	if (con_pub_multitcp->last_time_not_allow_send == 0)
	{
		con_pub_multitcp->last_time_not_allow_send = cur_time;
		res = 0;
	}
	else if (cur_time - con_pub_multitcp->last_time_not_allow_send > sleeptime)
	{
		con_pub_multitcp->last_time_not_allow_send = 0;
		res = 1;
	}
	return res;
}

int getTotalBW(session_publisher *sess_publisher)
{
	if (sess_publisher == NULL)
	{
		return 0;
	}
	session_publisher_multitcp *sess_pub_multitcp = static_cast<session_publisher_multitcp *> (*sess_publisher);
	if (sess_pub_multitcp == NULL)
	{
		return 0;
	}
	sess_pub_multitcp->total_bw = 0;
	for (std::list<connection_publisher_multitcp *>::iterator iter = sess_pub_multitcp->con_pub_multitcp.begin(); iter != sess_pub_multitcp->con_pub_multitcp.end(); iter++)
	{
		if ((*iter)->isstandby_con != 1)
		{
			sess_pub_multitcp->total_bw = sess_pub_multitcp->total_bw + (*iter)->send_rate;
		}
	}
	return sess_pub_multitcp->total_bw;
}


int isNeedAddConn(session_publisher *sess_publisher)
{
	if (sess_publisher == NULL)
	{
		return 0;
	}
	session_publisher_multitcp *sess_pub_multitcp = static_cast<session_publisher_multitcp *> (*sess_publisher);
	if (sess_pub_multitcp == NULL)
	{
		return 0;
	}
	AddConnDecision(sess_publisher);
	int res = sess_pub_multitcp->should_add_conn;
	if (sess_pub_multitcp->should_add_conn == 1)
	{
		sess_pub_multitcp->should_add_conn = 0;
		sess_pub_multitcp->time_last_try_add_conn = get_systime_ms();
		sess_pub_multitcp->last_total_bw = getTotalBW(sess_publisher);
		sess_pub_multitcp->last_total_connection_number = sess_pub_multitcp->con_pub_multitcp.size();
	}
	return res;
}

int AddConnDecision(session_publisher *sess_publisher)
{
	if (sess_publisher == NULL)
	{
		return 0;
	}
	session_publisher_multitcp *sess_pub_multitcp = static_cast<session_publisher_multitcp *> (*sess_publisher);
	if (sess_pub_multitcp == NULL)
	{
		return 0;
	}
	uint64_t diff_time = get_systime_ms() - sess_pub_multitcp->time_last_try_add_conn;

	if (sess_pub_multitcp->init_state == 1)
	{
		if (get_systime_ms() - sess_pub_multitcp->time_sess_begin < 60 * 1000)
		{
			sess_pub_multitcp->last_total_bw = getTotalBW(sess_publisher);
			sess_pub_multitcp->last_total_connection_number = sess_pub_multitcp->con_pub_multitcp.size();
			sess_pub_multitcp->should_add_conn = 0;
			return 0;
		}

		sess_pub_multitcp->init_state = 0;
		if (sess_pub_multitcp->con_pub_multitcp.size() < MAX_CONN_NUM)
		{
			sess_pub_multitcp->should_add_conn = 1;
		}
		else
		{
			sess_pub_multitcp->should_add_conn = 0;
		}
		return 0;
	}

	if (diff_time < 60 * 1000 || sess_pub_multitcp->con_pub_multitcp.size() >= MAX_CONN_NUM)
	{
		sess_pub_multitcp->should_add_conn = 0;
		return 0;
	}
	if (sess_pub_multitcp->con_pub_multitcp.size() < sess_pub_multitcp->last_total_connection_number)  //already delete some conns
	{
		if (diff_time >= 60 * 1000 && getTotalBW(sess_publisher) < (sess_pub_multitcp->last_total_bw * 8 / 10))
		{
			sess_pub_multitcp->should_add_conn = 1;
		}
		if (diff_time >= 300 * 1000 && getTotalBW(sess_publisher) < (sess_pub_multitcp->last_total_bw * 9 / 10))
		{
			sess_pub_multitcp->should_add_conn = 1;
		}
	}
	else
	{
		if (diff_time >= 60 * 1000 && getTotalBW(sess_publisher) > (sess_pub_multitcp->last_total_bw * 12 / 10))
		{
			sess_pub_multitcp->should_add_conn = 1;
		}
		if (diff_time >= 300 * 1000 && getTotalBW(sess_publisher) > (sess_pub_multitcp->last_total_bw * 11 / 10))
		{
			sess_pub_multitcp->should_add_conn = 1;
		}
		if (diff_time >= 600 * 1000)
		{
			sess_pub_multitcp->should_add_conn = 1;
		}
	}
	return 0;
}

int isLargestOWD(connection_publisher *con_publisher, session_publisher *sess_publisher)
{
	if (con_publisher == NULL || sess_publisher == NULL)
	{
		return 0;
	}
	connection_publisher_multitcp *con_pub_multitcp = static_cast<connection_publisher_multitcp *> (*con_publisher);
	session_publisher_multitcp *sess_pub_multitcp = static_cast<session_publisher_multitcp *> (*sess_publisher);
	if (con_pub_multitcp == NULL || sess_pub_multitcp == NULL)
	{
		return 0;
	}
	for (std::list<connection_publisher_multitcp *>::iterator iter = sess_pub_multitcp->con_pub_multitcp.begin(); iter != sess_pub_multitcp->con_pub_multitcp.end(); iter++)
	{
		if (con_pub_multitcp->send_owd < (*iter)->send_owd)
		{
			return 0;
		}
	}

	return 1;
}

void setStandbyCon(connection_publisher *con_publisher, int standby)
{
	if (con_publisher == NULL)
	{
		return;
	}
	connection_publisher_multitcp *con_pub_multitcp = static_cast<connection_publisher_multitcp *> (*con_publisher);
	con_pub_multitcp->isstandby_con = standby;
	return;
}
