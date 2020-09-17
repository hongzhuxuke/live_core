/*
The MIT License (MIT)

Copyright (c) 2013-2015 SRS(ossrs)

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <srs_lib_multitcp_socket.hpp>

#include <srs_kernel_error.hpp>
#include <atomic>
#include <string>
#include "multitcp/m_io_log.h"
static int log_count = 0;
MultitcpSocketStream::MultitcpSocketStream()
{
	//char FilePath[255];
	//sprintf(FilePath, "%s_%d.txt", "D:/multitcp_log", log_count);
	//FILE *file = fopen(FilePath, "w+");
	//M_IO_LogSetOutput(file);
	//M_IO_debuglevel = M_IO_LOGALL;
	//log_count++;

	io = m_io_create();
	
}

MultitcpSocketStream::~MultitcpSocketStream()
{
	if (io) {
		m_io_destroy(io);
		io = NULL;
	}
}

void* MultitcpSocketStream::get_io()
{
	return io;
}

//int MultitcpSocketStream::create_socket()
//{
//	//no need to create
//	return 0;
//}

int MultitcpSocketStream::connect(const char* server_ip, int port)
{
	srs_assert(io);
	return m_io_connect(io, server_ip, port);
}

void MultitcpSocketStream::async_close()
{
	//TODO if need to add this
	return;
}
// ISrsBufferReader
int MultitcpSocketStream::read(void* buf, size_t size, ssize_t* nread)
{
	srs_assert(io);
	int mnread = 0;
	int ret = m_io_read(io, buf, size, mnread);
	*nread = mnread;
	return ret;
}

// ISrsProtocolReader
void MultitcpSocketStream::set_recv_timeout(int64_t timeout_us)
{
	srs_assert(io);
	m_io_set_recv_timeout(io, timeout_us/1000);
}

int64_t MultitcpSocketStream::get_recv_timeout()
{
	srs_assert(io);
	return m_io_get_recv_timeout(io);
}

int64_t MultitcpSocketStream::get_recv_bytes()
{
	srs_assert(io);
	return m_io_get_recv_bytes(io);
}

// ISrsProtocolWriter
void MultitcpSocketStream::set_send_timeout(int64_t timeout_us)
{
	srs_assert(io);
	m_io_set_send_timeout(io, timeout_us/1000);
}

int64_t MultitcpSocketStream::get_send_timeout()
{
	srs_assert(io);
	return m_io_get_send_timeout(io);
}

int64_t MultitcpSocketStream::get_send_bytes()
{
	srs_assert(io);
	return m_io_get_send_bytes(io);
}

int MultitcpSocketStream::writev(const iovec *iov, int iov_size, ssize_t* nwrite)
{
	srs_assert(io);
	//TODO 
	int ret = 0;
	int mnwrite = 0;

	for (int i = 0; i < iov_size; i++){
		int temp = 0;
		if ((ret = m_io_write(io, iov[i].iov_base, iov[i].iov_len, temp)) != 0){
			break;
		}
		mnwrite += temp;
	}
	if (nwrite){
		*nwrite = mnwrite;
	}
	return ret;
}

// ISrsProtocolReaderWriter
bool MultitcpSocketStream::is_never_timeout(int64_t timeout_us)
{
	srs_assert(io);
	//TODO
	return  timeout_us == -1;
}

int MultitcpSocketStream::read_fully(void* buf, size_t size, ssize_t* nread)
{
	srs_assert(io);
	int mnread = 0;
	int ret = m_io_read_fully(io, buf, size, mnread);
	*nread = mnread;
	return ret;
}

int MultitcpSocketStream::write(void* buf, size_t size, ssize_t* nwrite)
{
	srs_assert(io);
	int mnwrit = 0;
	int ret = m_io_write(io, buf, size, mnwrit);
	*nwrite = mnwrit;
	return ret;
}


