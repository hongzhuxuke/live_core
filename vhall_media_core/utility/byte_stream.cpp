#include<string.h>
#include "byte_stream.h"
#include "assert.h"
#include "vhall_log.h"

#define ERROR_SUCCESS 0

bool is_little_endian()
{
	// convert to network(big-endian) order, if not equals, 
	// the system is little-endian, so need to convert the int64
	static int little_endian_check = -1;

	if (little_endian_check == -1) {
		union {
			int32_t i;
			int8_t c;
		} little_check_union;

		little_check_union.i = 0x01;
		little_endian_check = little_check_union.c;
	}

	return (little_endian_check == 1);
}

ByteStream::ByteStream()
{
	p = bytes = NULL;
	nb_bytes = 0;

	// TODO: support both little and big endian.
	assert(is_little_endian());
}

ByteStream::~ByteStream()
{
   
}

int ByteStream::initialize(char* b, int nb)
{
	int ret = ERROR_SUCCESS;

	if (!b) {
		ret = -1;
		LOGE("stream param bytes must not be NULL. ret=%d", ret);
		return ret;
	}

	if (nb <= 0) {
		ret = -1;
		LOGE("stream param size must be positive. ret=%d", ret);
		return ret;
	}

	nb_bytes = nb;
	p = bytes = b;
	LOGD("init stream ok, size=%d", size());

	return ret;
}

char* ByteStream::data()
{
	return bytes;
}

int ByteStream::size()
{
	return nb_bytes;
}

int ByteStream::pos()
{
	return (int)(p - bytes);
}

bool ByteStream::empty()
{
	return !bytes || (p >= bytes + nb_bytes);
}

bool ByteStream::require(int required_size)
{
	assert(required_size >= 0);

	return required_size <= nb_bytes - (p - bytes);
}

void ByteStream::skip(int size)
{
	assert(p);

	p += size;
}

int8_t ByteStream::read_1bytes()
{
	assert(require(1));

	return (int8_t)*p++;
}

int16_t ByteStream::read_2bytes()
{
	assert(require(2));

	int16_t value;
	char* pp = (char*)&value;
	pp[1] = *p++;
	pp[0] = *p++;

	return value;
}

int32_t ByteStream::read_3bytes()
{
	assert(require(3));

	int32_t value = 0x00;
	char* pp = (char*)&value;
	pp[2] = *p++;
	pp[1] = *p++;
	pp[0] = *p++;

	return value;
}

int32_t ByteStream::read_4bytes()
{
	assert(require(4));

	int32_t value;
	char* pp = (char*)&value;
	pp[3] = *p++;
	pp[2] = *p++;
	pp[1] = *p++;
	pp[0] = *p++;

	return value;
}

int64_t ByteStream::read_8bytes()
{
	assert(require(8));

	int64_t value;
	char* pp = (char*)&value;
	pp[7] = *p++;
	pp[6] = *p++;
	pp[5] = *p++;
	pp[4] = *p++;
	pp[3] = *p++;
	pp[2] = *p++;
	pp[1] = *p++;
	pp[0] = *p++;

	return value;
}

std::string ByteStream::read_string(int len)
{
	assert(require(len));

	std::string value;
	value.append(p, len);

	p += len;

	return value;
}

void ByteStream::read_bytes(char* data, int size)
{
	assert(require(size));

	memcpy(data, p, size);

	p += size;
}

void ByteStream::write_1bytes(int8_t value)
{
	assert(require(1));

	*p++ = value;
}

void ByteStream::write_2bytes(int16_t value)
{
	assert(require(2));

	char* pp = (char*)&value;
	*p++ = pp[1];
	*p++ = pp[0];
}

void ByteStream::write_4bytes(int32_t value)
{
	assert(require(4));

	char* pp = (char*)&value;
	*p++ = pp[3];
	*p++ = pp[2];
	*p++ = pp[1];
	*p++ = pp[0];
}

void ByteStream::write_3bytes(int32_t value)
{
	assert(require(3));

	char* pp = (char*)&value;
	*p++ = pp[2];
	*p++ = pp[1];
	*p++ = pp[0];
}

void ByteStream::write_8bytes(int64_t value)
{
	assert(require(8));

	char* pp = (char*)&value;
	*p++ = pp[7];
	*p++ = pp[6];
	*p++ = pp[5];
	*p++ = pp[4];
	*p++ = pp[3];
	*p++ = pp[2];
	*p++ = pp[1];
	*p++ = pp[0];
}

void ByteStream::write_string(std::string value)
{
	assert(require((int)value.length()));

	memcpy(p, value.data(), value.length());
	p += value.length();
}

void ByteStream::write_bytes(char* data, int size)
{
	assert(require(size));

	memcpy(p, data, size);
	p += size;
}

BitStream::BitStream()
{
	cb = 0;
	cb_left = 0;
	stream = NULL;
}

BitStream::~BitStream()
{
}

int BitStream::initialize(ByteStream* s) {
	stream = s;
	return ERROR_SUCCESS;
}

bool BitStream::empty() {
	if (cb_left) {
		return false;
	}
	return stream->empty();
}

int8_t BitStream::read_bit() {
	if (!cb_left) {
		assert(!stream->empty());
		cb = stream->read_1bytes();
		cb_left = 8;
	}

	int8_t v = (cb >> (cb_left - 1)) & 0x01;
	cb_left--;
	return v;
}


