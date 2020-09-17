#ifndef __M_IO_H__
#define __M_IO_H__

typedef   void* m_io_content_t;

#define M_IO_DEFULT_SEND_BUF_SIZE 65535
#define M_IO_DEFULT_RECV_BUF_SIZE 65535
#define M_IO_DEFULT_CONN_NUM      5

#include <stdint.h>

#ifdef __cplusplus
extern "C"{
#endif

	m_io_content_t m_io_create(int conn_num = M_IO_DEFULT_CONN_NUM, 
		int send_buf_size = M_IO_DEFULT_SEND_BUF_SIZE, int recv_buf_size = M_IO_DEFULT_RECV_BUF_SIZE);

	int m_io_connect(m_io_content_t m_io, const char* ip, int port);
	int m_io_read(m_io_content_t m_io, void* buf, int size, int &nread);

	int m_io_read_fully(m_io_content_t m_io, void* buf, int size, int& nread);
	int m_io_write(m_io_content_t m_io, void* buf, int size, int& nwrite);

	void m_io_destroy(m_io_content_t m_io);

	void m_io_set_recv_timeout(m_io_content_t m_io, int64_t timeout_us);
	void m_io_set_send_timeout(m_io_content_t m_io, int64_t timeout_us);
	int64_t m_io_get_recv_timeout(m_io_content_t m_io);
	int64_t m_io_get_send_timeout(m_io_content_t m_io);

	int64_t m_io_get_recv_bytes(m_io_content_t m_io);
	int64_t m_io_get_send_bytes(m_io_content_t m_io);
	
#ifdef __cplusplus
};
#endif


#endif //__M_IO_H__