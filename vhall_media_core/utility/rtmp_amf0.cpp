#include "rtmp_amf0.h"
#include <utility>
#include <vector>
#include <sstream>
#include <assert.h>
#include<string.h>
#include "byte_stream.h"
#include "vhall_log.h"
#include "live_define.h"
using namespace std;
// AMF0 marker
#define RTMP_AMF0_Number                     0x00
#define RTMP_AMF0_Boolean                     0x01
#define RTMP_AMF0_String                     0x02
#define RTMP_AMF0_Object                     0x03
#define RTMP_AMF0_MovieClip                 0x04 // reserved, not supported
#define RTMP_AMF0_Null                         0x05
#define RTMP_AMF0_Undefined                 0x06
#define RTMP_AMF0_Reference                 0x07
#define RTMP_AMF0_EcmaArray                 0x08
#define RTMP_AMF0_ObjectEnd                 0x09
#define RTMP_AMF0_StrictArray                 0x0A
#define RTMP_AMF0_Date                         0x0B
#define RTMP_AMF0_LongString                 0x0C
#define RTMP_AMF0_UnSupported                 0x0D
#define RTMP_AMF0_RecordSet                 0x0E // reserved, not supported
#define RTMP_AMF0_XmlDocument                 0x0F
#define RTMP_AMF0_TypedObject                 0x10
// AVM+ object is the AMF3 object.
#define RTMP_AMF0_AVMplusObject             0x11
// origin array whos data takes the same form as LengthValueBytes
#define RTMP_AMF0_OriginStrictArray         0x20

// User defined
#define RTMP_AMF0_Invalid                     0x3F

VhallAmf0Any::VhallAmf0Any()
{
	marker = RTMP_AMF0_Invalid;
}

VhallAmf0Any::~VhallAmf0Any()
{
}

bool VhallAmf0Any::is_string()
{
	return marker == RTMP_AMF0_String;
}

bool VhallAmf0Any::is_boolean()
{
	return marker == RTMP_AMF0_Boolean;
}

bool VhallAmf0Any::is_number()
{
	return marker == RTMP_AMF0_Number;
}

bool VhallAmf0Any::is_null()
{
	return marker == RTMP_AMF0_Null;
}

bool VhallAmf0Any::is_undefined()
{
	return marker == RTMP_AMF0_Undefined;
}

bool VhallAmf0Any::is_object()
{
	return marker == RTMP_AMF0_Object;
}

bool VhallAmf0Any::is_ecma_array()
{
	return marker == RTMP_AMF0_EcmaArray;
}

bool VhallAmf0Any::is_strict_array()
{
	return marker == RTMP_AMF0_StrictArray;
}

bool VhallAmf0Any::is_date()
{
	return marker == RTMP_AMF0_Date;
}

bool VhallAmf0Any::is_complex_object()
{
	return is_object() || is_object_eof() || is_ecma_array() || is_strict_array();
}

std::string VhallAmf0Any::to_str()
{
	VhallAmf0String* p = dynamic_cast<VhallAmf0String*>(this);
	assert(p != NULL);
	return p->value;
}

const char* VhallAmf0Any::to_str_raw()
{
	VhallAmf0String* p = dynamic_cast<VhallAmf0String*>(this);
	assert(p != NULL);
	return p->value.data();
}

bool VhallAmf0Any::to_boolean()
{
	VhallAmf0Boolean* p = dynamic_cast<VhallAmf0Boolean*>(this);
	assert(p != NULL);
	return p->value;
}

double VhallAmf0Any::to_number()
{
	VhallAmf0Number* p = dynamic_cast<VhallAmf0Number*>(this);
	assert(p != NULL);
	return p->value;
}

int64_t VhallAmf0Any::to_date()
{
	VhallAmf0Date* p = dynamic_cast<VhallAmf0Date*>(this);
	assert(p != NULL);
	return p->date();
}

int16_t VhallAmf0Any::to_date_time_zone()
{
	VhallAmf0Date* p = dynamic_cast<VhallAmf0Date*>(this);
	assert(p != NULL);
	return p->time_zone();
}

VhallAmf0Object* VhallAmf0Any::to_object()
{
	VhallAmf0Object* p = dynamic_cast<VhallAmf0Object*>(this);
	assert(p != NULL);
	return p;
}

VhallAmf0EcmaArray* VhallAmf0Any::to_ecma_array()
{
	VhallAmf0EcmaArray* p = dynamic_cast<VhallAmf0EcmaArray*>(this);
	assert(p != NULL);
	return p;
}

VhallAmf0StrictArray* VhallAmf0Any::to_strict_array()
{
	VhallAmf0StrictArray* p = dynamic_cast<VhallAmf0StrictArray*>(this);
	assert(p != NULL);
	return p;
}

void VhallAmf0Any::set_number(double value)
{
	VhallAmf0Number* p = dynamic_cast<VhallAmf0Number*>(this);
	assert(p != NULL);
	p->value = value;
}

bool VhallAmf0Any::is_object_eof()
{
	return marker == RTMP_AMF0_ObjectEnd;
}

void vhall_fill_level_spaces(stringstream& ss, int level)
{
	for (int i = 0; i < level; i++) {
		ss << "    ";
	}
}
void vhall_amf0_do_print(VhallAmf0Any* any, stringstream& ss, int level)
{
	if (any->is_boolean()) {
		ss << "Boolean " << (any->to_boolean() ? "true" : "false") << endl;
	}
	else if (any->is_number()) {
		ss << "Number " << std::fixed << any->to_number() << endl;
	}
	else if (any->is_string()) {
		ss << "String " << any->to_str() << endl;
	}
	else if (any->is_date()) {
		ss << "Date " << std::hex << any->to_date()
			<< "/" << std::hex << any->to_date_time_zone() << endl;
	}
	else if (any->is_null()) {
		ss << "Null" << endl;
	}
	else if (any->is_ecma_array()) {
		VhallAmf0EcmaArray* obj = any->to_ecma_array();
		ss << "EcmaArray " << "(" << obj->count() << " items)" << endl;
		for (int i = 0; i < obj->count(); i++) {
			vhall_fill_level_spaces(ss, level + 1);
			ss << "Elem '" << obj->key_at(i) << "' ";
			if (obj->value_at(i)->is_complex_object()) {
				vhall_amf0_do_print(obj->value_at(i), ss, level + 1);
			}
			else {
				vhall_amf0_do_print(obj->value_at(i), ss, 0);
			}
		}
	}
	else if (any->is_strict_array()) {
		VhallAmf0StrictArray* obj = any->to_strict_array();
		ss << "StrictArray " << "(" << obj->count() << " items)" << endl;
		for (int i = 0; i < obj->count(); i++) {
			vhall_fill_level_spaces(ss, level + 1);
			ss << "Elem ";
			if (obj->at(i)->is_complex_object()) {
				vhall_amf0_do_print(obj->at(i), ss, level + 1);
			}
			else {
				vhall_amf0_do_print(obj->at(i), ss, 0);
			}
		}
	}
	else if (any->is_object()) {
		VhallAmf0Object* obj = any->to_object();
		ss << "Object " << "(" << obj->count() << " items)" << endl;
		for (int i = 0; i < obj->count(); i++) {
			vhall_fill_level_spaces(ss, level + 1);
			ss << "Property '" << obj->key_at(i) << "' ";
			if (obj->value_at(i)->is_complex_object()) {
				vhall_amf0_do_print(obj->value_at(i), ss, level + 1);
			}
			else {
				vhall_amf0_do_print(obj->value_at(i), ss, 0);
			}
		}
	}
	else {
		ss << "Unknown" << endl;
	}
}

char* VhallAmf0Any::human_print(char** pdata, int* psize)
{
	stringstream ss;

	ss.precision(1);

	vhall_amf0_do_print(this, ss, 0);

	string str = ss.str();
	if (str.empty()) {
		return NULL;
	}

	char* data = new char[str.length() + 1];
	memcpy(data, str.data(), str.length());
	data[str.length()] = 0;

	if (pdata) {
		*pdata = data;
	}
	if (psize) {
		*psize = str.length();
	}

	return data;
}

VhallAmf0Any* VhallAmf0Any::str(const char* value)
{
	return new VhallAmf0String(value);
}

VhallAmf0Any* VhallAmf0Any::boolean(bool value)
{
	return new VhallAmf0Boolean(value);
}

VhallAmf0Any* VhallAmf0Any::number(double value)
{
	return new VhallAmf0Number(value);
}

VhallAmf0Any* VhallAmf0Any::null()
{
	return new VhallAmf0Null();
}

VhallAmf0Any* VhallAmf0Any::undefined()
{
	return new VhallAmf0Undefined();
}

VhallAmf0Object* VhallAmf0Any::object()
{
	return new VhallAmf0Object();
}

VhallAmf0Any* VhallAmf0Any::object_eof()
{
	return new VhallAmf0ObjectEOF();
}

VhallAmf0EcmaArray* VhallAmf0Any::ecma_array()
{
	return new VhallAmf0EcmaArray();
}

VhallAmf0StrictArray* VhallAmf0Any::strict_array()
{
	return new VhallAmf0StrictArray();
}

VhallAmf0Any* VhallAmf0Any::date(int64_t value)
{
	return new VhallAmf0Date(value);
}

int VhallAmf0Any::discovery(ByteStream* stream, VhallAmf0Any** ppvalue)
{
	int ret = 0;

	// detect the object-eof specially
	if (vhall_amf0_is_object_eof(stream)) {
		*ppvalue = new VhallAmf0ObjectEOF();
		return ret;
	}

	// marker
	if (!stream->require(1)) {
		ret = -1;
		LOGE("amf0 read any marker failed. ret=%d", ret);
		return ret;
	}

	char marker = stream->read_1bytes();
	LOGI("amf0 any marker success");

	// backward the 1byte marker.
	stream->skip(-1);

	switch (marker) {
	case RTMP_AMF0_String: {
		*ppvalue = VhallAmf0Any::str();
		return ret;
	}
	case RTMP_AMF0_Boolean: {
		*ppvalue = VhallAmf0Any::boolean();
		return ret;
	}
	case RTMP_AMF0_Number: {
		*ppvalue = VhallAmf0Any::number();
		return ret;
	}
	case RTMP_AMF0_Null: {
		*ppvalue = VhallAmf0Any::null();
		return ret;
	}
	case RTMP_AMF0_Undefined: {
		*ppvalue = VhallAmf0Any::undefined();
		return ret;
	}
	case RTMP_AMF0_Object: {
		*ppvalue = VhallAmf0Any::object();
		return ret;
	}
	case RTMP_AMF0_EcmaArray: {
		*ppvalue = VhallAmf0Any::ecma_array();
		return ret;
	}
	case RTMP_AMF0_StrictArray: {
		*ppvalue = VhallAmf0Any::strict_array();
		return ret;
	}
	case RTMP_AMF0_Date: {
		*ppvalue = VhallAmf0Any::date();
		return ret;
	}
	case RTMP_AMF0_Invalid:
	default: {
		ret = -1;
		LOGE("invalid amf0 message type. marker=%#x, ret=%d", marker, ret);
		return ret;
	}
	}
}

VhallUnSortedHashtable::VhallUnSortedHashtable()
{
}

VhallUnSortedHashtable::~VhallUnSortedHashtable()
{
	clear();
}

int VhallUnSortedHashtable::count()
{
	return (int)properties.size();
}

void VhallUnSortedHashtable::clear()
{
	std::vector<VhallAmf0ObjectPropertyType>::iterator it;
	for (it = properties.begin(); it != properties.end(); ++it) {
		VhallAmf0ObjectPropertyType& elem = *it;
		VhallAmf0Any* any = elem.second;
		VHALL_DEL(any);
	}
	properties.clear();
}

string VhallUnSortedHashtable::key_at(int index)
{
	assert(index < count());
	VhallAmf0ObjectPropertyType& elem = properties[index];
	return elem.first;
}

const char* VhallUnSortedHashtable::key_raw_at(int index)
{
	assert(index < count());
	VhallAmf0ObjectPropertyType& elem = properties[index];
	return elem.first.data();
}

VhallAmf0Any* VhallUnSortedHashtable::value_at(int index)
{
	assert(index < count());
	VhallAmf0ObjectPropertyType& elem = properties[index];
	return elem.second;
}

void VhallUnSortedHashtable::set(string key, VhallAmf0Any* value)
{
	std::vector<VhallAmf0ObjectPropertyType>::iterator it;

	for (it = properties.begin(); it != properties.end(); ++it) {
		VhallAmf0ObjectPropertyType& elem = *it;
		std::string name = elem.first;
		VhallAmf0Any* any = elem.second;

		if (key == name) {
			VHALL_DEL(any);
			properties.erase(it);
			break;
		}
	}

	if (value) {
		properties.push_back(std::make_pair(key, value));
	}
}

VhallAmf0Any* VhallUnSortedHashtable::get_property(string name)
{
	std::vector<VhallAmf0ObjectPropertyType>::iterator it;

	for (it = properties.begin(); it != properties.end(); ++it) {
		VhallAmf0ObjectPropertyType& elem = *it;
		std::string key = elem.first;
		VhallAmf0Any* any = elem.second;
		if (key == name) {
			return any;
		}
	}

	return NULL;
}

VhallAmf0Any* VhallUnSortedHashtable::ensure_property_string(string name)
{
	VhallAmf0Any* prop = get_property(name);

	if (!prop) {
		return NULL;
	}

	if (!prop->is_string()) {
		return NULL;
	}

	return prop;
}

VhallAmf0Any* VhallUnSortedHashtable::ensure_property_number(string name)
{
	VhallAmf0Any* prop = get_property(name);

	if (!prop) {
		return NULL;
	}

	if (!prop->is_number()) {
		return NULL;
	}

	return prop;
}

void VhallUnSortedHashtable::remove(string name)
{
	std::vector<VhallAmf0ObjectPropertyType>::iterator it;

	for (it = properties.begin(); it != properties.end();) {
		std::string key = it->first;
		VhallAmf0Any* any = it->second;

		if (key == name) {
			VHALL_DEL(any);

			it = properties.erase(it);
		}
		else {
			++it;
		}
	}
}

void VhallUnSortedHashtable::copy(VhallUnSortedHashtable* src)
{
	std::vector<VhallAmf0ObjectPropertyType>::iterator it;
	for (it = src->properties.begin(); it != src->properties.end(); ++it) {
		VhallAmf0ObjectPropertyType& elem = *it;
		std::string key = elem.first;
		VhallAmf0Any* any = elem.second;
		set(key, any->copy());
	}
}

VhallAmf0ObjectEOF::VhallAmf0ObjectEOF()
{
	marker = RTMP_AMF0_ObjectEnd;
}

VhallAmf0ObjectEOF::~VhallAmf0ObjectEOF()
{
}

int VhallAmf0ObjectEOF::total_size()
{
	return VhallAmf0Size::object_eof();
}

int VhallAmf0ObjectEOF::read(ByteStream* stream)
{
	int ret = 0;

	// value
	if (!stream->require(2)) {
		ret = -1;
		LOGE("amf0 read object eof value failed. ret=%d", ret);
		return ret;
	}
	int16_t temp = stream->read_2bytes();
	if (temp != 0x00) {
		ret = -1;
		LOGE("amf0 read object eof value check failed. "
			"must be 0x00, actual is %#x, ret=%d", temp, ret);
		return ret;
	}

	// marker
	if (!stream->require(1)) {
		ret = -1;
		LOGE("amf0 read object eof marker failed. ret=%d", ret);
		return ret;
	}

	char marker = stream->read_1bytes();
	if (marker != RTMP_AMF0_ObjectEnd) {
		ret = -1;
		LOGE("amf0 check object eof marker failed. "
			"marker=%#x, required=%#x, ret=%d", marker, RTMP_AMF0_ObjectEnd, ret);
		return ret;
	}
	LOGI("amf0 read object eof marker success");

	LOGI("amf0 read object eof success");

	return ret;
}
int VhallAmf0ObjectEOF::write(ByteStream* stream)
{
	int ret = 0;

	// value
	if (!stream->require(2)) {
		ret = -1;
		LOGE("amf0 write object eof value failed. ret=%d", ret);
		return ret;
	}
	stream->write_2bytes(0x00);
	LOGI("amf0 write object eof value success");

	// marker
	if (!stream->require(1)) {
		ret = -1;
		LOGE("amf0 write object eof marker failed. ret=%d", ret);
		return ret;
	}

	stream->write_1bytes(RTMP_AMF0_ObjectEnd);

	LOGI("amf0 read object eof success");

	return ret;
}

VhallAmf0Any* VhallAmf0ObjectEOF::copy()
{
	return new VhallAmf0ObjectEOF();
}

VhallAmf0Object::VhallAmf0Object()
{
	properties = new VhallUnSortedHashtable();
	eof = new VhallAmf0ObjectEOF();
	marker = RTMP_AMF0_Object;
}

VhallAmf0Object::~VhallAmf0Object()
{
	VHALL_DEL(properties);
	VHALL_DEL(eof);
}

int VhallAmf0Object::total_size()
{
	int size = 1;

	for (int i = 0; i < properties->count(); i++){
		std::string name = key_at(i);
		VhallAmf0Any* value = value_at(i);

		size += VhallAmf0Size::utf8(name);
		size += VhallAmf0Size::any(value);
	}

	size += VhallAmf0Size::object_eof();

	return size;
}

int VhallAmf0Object::read(ByteStream* stream)
{
	int ret = 0;

	// marker
	if (!stream->require(1)) {
		ret = -1;
		LOGE("amf0 read object marker failed. ret=%d", ret);
		return ret;
	}

	char marker = stream->read_1bytes();
	if (marker != RTMP_AMF0_Object) {
		ret = -1;
		LOGE("amf0 check object marker failed. "
			"marker=%#x, required=%#x, ret=%d", marker, RTMP_AMF0_Object, ret);
		return ret;
	}
	LOGI("amf0 read object marker success");

	// value
	while (!stream->empty()) {
		// detect whether is eof.
		if (vhall_amf0_is_object_eof(stream)) {
			VhallAmf0ObjectEOF pbj_eof;
			if ((ret = pbj_eof.read(stream)) != 0) {
				LOGE("amf0 object read eof failed. ret=%d", ret);
				return ret;
			}
			LOGI("amf0 read object EOF.");
			break;
		}

		// property-name: utf8 string
		std::string property_name;
		if ((ret = vhall_amf0_read_utf8(stream, property_name)) != 0) {
			LOGE("amf0 object read property name failed. ret=%d", ret);
			return ret;
		}
		// property-value: any
		VhallAmf0Any* property_value = NULL;
		if ((ret = vhall_amf0_read_any(stream, &property_value)) != 0) {
			LOGE("amf0 object read property_value failed. "
				"name=%s, ret=%d", property_name.c_str(), ret);
			VHALL_DEL(property_value);
			return ret;
		}

		// add property
		this->set(property_name, property_value);
	}

	return ret;
}

int VhallAmf0Object::write(ByteStream* stream)
{
	int ret = 0;

	// marker
	if (!stream->require(1)) {
		ret = -1;
		LOGE("amf0 write object marker failed. ret=%d", ret);
		return ret;
	}

	stream->write_1bytes(RTMP_AMF0_Object);
	LOGI("amf0 write object marker success");

	// value
	for (int i = 0; i < properties->count(); i++) {
		std::string name = this->key_at(i);
		VhallAmf0Any* any = this->value_at(i);

		if ((ret = vhall_amf0_write_utf8(stream, name)) != 0) {
			LOGE("write object property name failed. ret=%d", ret);
			return ret;
		}

		if ((ret = vhall_amf0_write_any(stream, any)) != 0) {
			LOGE("write object property value failed. ret=%d", ret);
			return ret;
		}

		LOGI("write amf0 property success. name=%s", name.c_str());
	}

	if ((ret = eof->write(stream)) != 0) {
		LOGE("write object eof failed. ret=%d", ret);
		return ret;
	}

	LOGI("write amf0 object success.");

	return ret;
}

VhallAmf0Any* VhallAmf0Object::copy()
{
	VhallAmf0Object* copy = new VhallAmf0Object();
	copy->properties->copy(properties);
	return copy;
}

void VhallAmf0Object::clear()
{
	properties->clear();
}

int VhallAmf0Object::count()
{
	return properties->count();
}

string VhallAmf0Object::key_at(int index)
{
	return properties->key_at(index);
}

const char* VhallAmf0Object::key_raw_at(int index)
{
	return properties->key_raw_at(index);
}

VhallAmf0Any* VhallAmf0Object::value_at(int index)
{
	return properties->value_at(index);
}

void VhallAmf0Object::set(string key, VhallAmf0Any* value)
{
	properties->set(key, value);
}

VhallAmf0Any* VhallAmf0Object::get_property(string name)
{
	return properties->get_property(name);
}

VhallAmf0Any* VhallAmf0Object::ensure_property_string(string name)
{
	return properties->ensure_property_string(name);
}

VhallAmf0Any* VhallAmf0Object::ensure_property_number(string name)
{
	return properties->ensure_property_number(name);
}

void VhallAmf0Object::remove(string name)
{
	properties->remove(name);
}

VhallAmf0EcmaArray::VhallAmf0EcmaArray()
{
	_count = 0;
	properties = new VhallUnSortedHashtable();
	eof = new VhallAmf0ObjectEOF();
	marker = RTMP_AMF0_EcmaArray;
}

VhallAmf0EcmaArray::~VhallAmf0EcmaArray()
{
	VHALL_DEL(properties);
	VHALL_DEL(eof);
}

int VhallAmf0EcmaArray::total_size()
{
	int size = 1 + 4;

	for (int i = 0; i < properties->count(); i++){
		std::string name = key_at(i);
		VhallAmf0Any* value = value_at(i);

		size += VhallAmf0Size::utf8(name);
		size += VhallAmf0Size::any(value);
	}

	size += VhallAmf0Size::object_eof();

	return size;
}

int VhallAmf0EcmaArray::read(ByteStream* stream)
{
	int ret = 0;

	// marker
	if (!stream->require(1)) {
		ret = -1;
		LOGE("amf0 read ecma_array marker failed. ret=%d", ret);
		return ret;
	}

	char marker = stream->read_1bytes();
	if (marker != RTMP_AMF0_EcmaArray) {
		ret = -1;
		LOGE("amf0 check ecma_array marker failed. "
			"marker=%#x, required=%#x, ret=%d", marker, RTMP_AMF0_EcmaArray, ret);
		return ret;
	}
	LOGI("amf0 read ecma_array marker success");

	// count
	if (!stream->require(4)) {
		ret = -1;
		LOGE("amf0 read ecma_array count failed. ret=%d", ret);
		return ret;
	}

	int32_t count = stream->read_4bytes();
	LOGI("amf0 read ecma_array count success. count=%d", count);

	// value
	this->_count = count;

	while (!stream->empty()) {
		// detect whether is eof.
		if (vhall_amf0_is_object_eof(stream)) {
			VhallAmf0ObjectEOF pbj_eof;
			if ((ret = pbj_eof.read(stream)) != 0) {
				LOGE("amf0 ecma_array read eof failed. ret=%d", ret);
				return ret;
			}
			LOGI("amf0 read ecma_array EOF.");
			break;
		}

		// property-name: utf8 string
		std::string property_name;
		if ((ret = vhall_amf0_read_utf8(stream, property_name)) != 0) {
			LOGE("amf0 ecma_array read property name failed. ret=%d", ret);
			return ret;
		}
		// property-value: any
		VhallAmf0Any* property_value = NULL;
		if ((ret = vhall_amf0_read_any(stream, &property_value)) != 0) {
			LOGE("amf0 ecma_array read property_value failed. "
				"name=%s, ret=%d", property_name.c_str(), ret);
			return ret;
		}

		// add property
		this->set(property_name, property_value);
	}

	return ret;
}
int VhallAmf0EcmaArray::write(ByteStream* stream)
{
	int ret = 0;

	// marker
	if (!stream->require(1)) {
		ret = -1;
		LOGE("amf0 write ecma_array marker failed. ret=%d", ret);
		return ret;
	}

	stream->write_1bytes(RTMP_AMF0_EcmaArray);
	LOGI("amf0 write ecma_array marker success");

	// count
	if (!stream->require(4)) {
		ret = -1;
		LOGE("amf0 write ecma_array count failed. ret=%d", ret);
		return ret;
	}

	stream->write_4bytes(this->_count);
	LOGI("amf0 write ecma_array count success. count=%d", _count);

	// value
	for (int i = 0; i < properties->count(); i++) {
		std::string name = this->key_at(i);
		VhallAmf0Any* any = this->value_at(i);

		if ((ret = vhall_amf0_write_utf8(stream, name)) != 0) {
			LOGE("write ecma_array property name failed. ret=%d", ret);
			return ret;
		}

		if ((ret = vhall_amf0_write_any(stream, any)) != 0) {
			LOGE("write ecma_array property value failed. ret=%d", ret);
			return ret;
		}

		LOGI("write amf0 property success. name=%s", name.c_str());
	}

	if ((ret = eof->write(stream)) != 0) {
		LOGE("write ecma_array eof failed. ret=%d", ret);
		return ret;
	}

	LOGI("write ecma_array object success.");

	return ret;
}

VhallAmf0Any* VhallAmf0EcmaArray::copy()
{
	VhallAmf0EcmaArray* copy = new VhallAmf0EcmaArray();
	copy->properties->copy(properties);
	copy->_count = _count;
	return copy;
}

void VhallAmf0EcmaArray::clear()
{
	properties->clear();
}

int VhallAmf0EcmaArray::count()
{
	return properties->count();
}

string VhallAmf0EcmaArray::key_at(int index)
{
	return properties->key_at(index);
}

const char* VhallAmf0EcmaArray::key_raw_at(int index)
{
	return properties->key_raw_at(index);
}

VhallAmf0Any* VhallAmf0EcmaArray::value_at(int index)
{
	return properties->value_at(index);
}

void VhallAmf0EcmaArray::set(string key, VhallAmf0Any* value)
{
	properties->set(key, value);
}

VhallAmf0Any* VhallAmf0EcmaArray::get_property(string name)
{
	return properties->get_property(name);
}

VhallAmf0Any* VhallAmf0EcmaArray::ensure_property_string(string name)
{
	return properties->ensure_property_string(name);
}

VhallAmf0Any* VhallAmf0EcmaArray::ensure_property_number(string name)
{
	return properties->ensure_property_number(name);
}

VhallAmf0StrictArray::VhallAmf0StrictArray()
{
	marker = RTMP_AMF0_StrictArray;
	_count = 0;
}

VhallAmf0StrictArray::~VhallAmf0StrictArray()
{
	std::vector<VhallAmf0Any*>::iterator it;
	for (it = properties.begin(); it != properties.end(); ++it) {
		VhallAmf0Any* any = *it;
		VHALL_DEL(any);
	}
	properties.clear();
}

int VhallAmf0StrictArray::total_size()
{
	int size = 1 + 4;

	for (int i = 0; i < (int)properties.size(); i++){
		VhallAmf0Any* any = properties[i];
		size += any->total_size();
	}

	return size;
}

int VhallAmf0StrictArray::read(ByteStream* stream)
{
	int ret = 0;

	// marker
	if (!stream->require(1)) {
		ret = -1;
		LOGE("amf0 read strict_array marker failed. ret=%d", ret);
		return ret;
	}

	char marker = stream->read_1bytes();
	if (marker != RTMP_AMF0_StrictArray) {
		ret = -1;
		LOGE("amf0 check strict_array marker failed. "
			"marker=%#x, required=%#x, ret=%d", marker, RTMP_AMF0_StrictArray, ret);
		return ret;
	}
	LOGI("amf0 read strict_array marker success");

	// count
	if (!stream->require(4)) {
		ret = -1;
		LOGE("amf0 read strict_array count failed. ret=%d", ret);
		return ret;
	}

	int32_t count = stream->read_4bytes();
	LOGI("amf0 read strict_array count success. count=%d", count);

	// value
	this->_count = count;

	for (int i = 0; i < count && !stream->empty(); i++) {
		// property-value: any
		VhallAmf0Any* elem = NULL;
		if ((ret = vhall_amf0_read_any(stream, &elem)) != 0) {
			LOGE("amf0 strict_array read value failed. ret=%d", ret);
			return ret;
		}

		// add property
		properties.push_back(elem);
	}

	return ret;
}
int VhallAmf0StrictArray::write(ByteStream* stream)
{
	int ret = 0;

	// marker
	if (!stream->require(1)) {
		ret = -1;
		LOGE("amf0 write strict_array marker failed. ret=%d", ret);
		return ret;
	}

	stream->write_1bytes(RTMP_AMF0_StrictArray);
	LOGI("amf0 write strict_array marker success");

	// count
	if (!stream->require(4)) {
		ret = -1;
		LOGE("amf0 write strict_array count failed. ret=%d", ret);
		return ret;
	}

	stream->write_4bytes(this->_count);
	LOGI("amf0 write strict_array count success. count=%d", _count);

	// value
	for (int i = 0; i < (int)properties.size(); i++) {
		VhallAmf0Any* any = properties[i];

		if ((ret = vhall_amf0_write_any(stream, any)) != 0) {
			LOGE("write strict_array property value failed. ret=%d", ret);
			return ret;
		}

		LOGI("write amf0 property success.");
	}

	LOGI("write strict_array object success.");

	return ret;
}

VhallAmf0Any* VhallAmf0StrictArray::copy()
{
	VhallAmf0StrictArray* copy = new VhallAmf0StrictArray();

	std::vector<VhallAmf0Any*>::iterator it;
	for (it = properties.begin(); it != properties.end(); ++it) {
		VhallAmf0Any* any = *it;
		copy->append(any->copy());
	}

	copy->_count = _count;
	return copy;
}

void VhallAmf0StrictArray::clear()
{
	properties.clear();
}

int VhallAmf0StrictArray::count()
{
	return properties.size();
}

VhallAmf0Any* VhallAmf0StrictArray::at(int index)
{
	assert(index < (int)properties.size());
	return properties.at(index);
}

void VhallAmf0StrictArray::append(VhallAmf0Any* any)
{
	properties.push_back(any);
	_count = (int32_t)properties.size();
}

int VhallAmf0Size::utf8(string value)
{
	return 2 + value.length();
}

int VhallAmf0Size::str(string value)
{
	return 1 + VhallAmf0Size::utf8(value);
}

int VhallAmf0Size::number()
{
	return 1 + 8;
}

int VhallAmf0Size::date()
{
	return 1 + 8 + 2;
}

int VhallAmf0Size::null()
{
	return 1;
}

int VhallAmf0Size::undefined()
{
	return 1;
}

int VhallAmf0Size::boolean()
{
	return 1 + 1;
}

int VhallAmf0Size::object(VhallAmf0Object* obj)
{
	if (!obj) {
		return 0;
	}

	return obj->total_size();
}

int VhallAmf0Size::object_eof()
{
	return 2 + 1;
}

int VhallAmf0Size::ecma_array(VhallAmf0EcmaArray* arr)
{
	if (!arr) {
		return 0;
	}

	return arr->total_size();
}

int VhallAmf0Size::strict_array(VhallAmf0StrictArray* arr)
{
	if (!arr) {
		return 0;
	}

	return arr->total_size();
}

int VhallAmf0Size::any(VhallAmf0Any* o)
{
	if (!o) {
		return 0;
	}

	return o->total_size();
}

VhallAmf0String::VhallAmf0String(const char* _value)
{
	marker = RTMP_AMF0_String;
	if (_value) {
		value = _value;
	}
}

VhallAmf0String::~VhallAmf0String()
{
}

int VhallAmf0String::total_size()
{
	return VhallAmf0Size::str(value);
}

int VhallAmf0String::read(ByteStream* stream)
{
	return vhall_amf0_read_string(stream, value);
}

int VhallAmf0String::write(ByteStream* stream)
{
	return vhall_amf0_write_string(stream, value);
}

VhallAmf0Any* VhallAmf0String::copy()
{
	VhallAmf0String* copy = new VhallAmf0String(value.c_str());
	return copy;
}

VhallAmf0Boolean::VhallAmf0Boolean(bool _value)
{
	marker = RTMP_AMF0_Boolean;
	value = _value;
}

VhallAmf0Boolean::~VhallAmf0Boolean()
{
}

int VhallAmf0Boolean::total_size()
{
	return VhallAmf0Size::boolean();
}

int VhallAmf0Boolean::read(ByteStream* stream)
{
	return vhall_amf0_read_boolean(stream, value);
}

int VhallAmf0Boolean::write(ByteStream* stream)
{
	return vhall_amf0_write_boolean(stream, value);
}

VhallAmf0Any* VhallAmf0Boolean::copy()
{
	VhallAmf0Boolean* copy = new VhallAmf0Boolean(value);
	return copy;
}

VhallAmf0Number::VhallAmf0Number(double _value)
{
	marker = RTMP_AMF0_Number;
	value = _value;
}

VhallAmf0Number::~VhallAmf0Number()
{
}

int VhallAmf0Number::total_size()
{
	return VhallAmf0Size::number();
}

int VhallAmf0Number::read(ByteStream* stream)
{
	return vhall_amf0_read_number(stream, value);
}

int VhallAmf0Number::write(ByteStream* stream)
{
	return vhall_amf0_write_number(stream, value);
}

VhallAmf0Any* VhallAmf0Number::copy()
{
	VhallAmf0Number* copy = new VhallAmf0Number(value);
	return copy;
}

VhallAmf0Date::VhallAmf0Date(int64_t value)
{
	marker = RTMP_AMF0_Date;
	_date_value = value;
	_time_zone = 0;
}

VhallAmf0Date::~VhallAmf0Date()
{
}

int VhallAmf0Date::total_size()
{
	return VhallAmf0Size::date();
}

int VhallAmf0Date::read(ByteStream* stream)
{
	int ret = 0;

	// marker
	if (!stream->require(1)) {
		ret = -1;
		LOGE("amf0 read date marker failed. ret=%d", ret);
		return ret;
	}

	char marker = stream->read_1bytes();
	if (marker != RTMP_AMF0_Date) {
		ret = -1;
		LOGE("amf0 check date marker failed. "
			"marker=%#x, required=%#x, ret=%d", marker, RTMP_AMF0_Date, ret);
		return ret;
	}
	LOGI("amf0 read date marker success");

	// date value
	// An ActionScript Date is serialized as the number of milliseconds 
	// elapsed since the epoch of midnight on 1st Jan 1970 in the UTC 
	// time zone.
	if (!stream->require(8)) {
		ret = -1;
		LOGE("amf0 read date failed. ret=%d", ret);
		return ret;
	}

	_date_value = stream->read_8bytes();
	LOGI("amf0 read date success. date=%d", _date_value);

	// time zone
	// While the design of this type reserves room for time zone offset 
	// information, it should not be filled in, nor used, as it is unconventional 
	// to change time zones when serializing dates on a network. It is suggested 
	// that the time zone be queried independently as needed.
	if (!stream->require(2)) {
		ret = -1;
		LOGE("amf0 read time zone failed. ret=%d", ret);
		return ret;
	}

	_time_zone = stream->read_2bytes();
	LOGI("amf0 read time zone success. zone=%d", _time_zone);

	return ret;
}
int VhallAmf0Date::write(ByteStream* stream)
{
	int ret = 0;

	// marker
	if (!stream->require(1)) {
		ret = -1;
		LOGE("amf0 write date marker failed. ret=%d", ret);
		return ret;
	}

	stream->write_1bytes(RTMP_AMF0_Date);
	LOGI("amf0 write date marker success");

	// date value
	if (!stream->require(8)) {
		ret = -1;
		LOGE("amf0 write date failed. ret=%d", ret);
		return ret;
	}

	stream->write_8bytes(_date_value);
	LOGI("amf0 write date success. date=%", _date_value);

	// time zone
	if (!stream->require(2)) {
		ret = -1;
		LOGE("amf0 write time zone failed. ret=%d", ret);
		return ret;
	}

	stream->write_2bytes(_time_zone);
	LOGI("amf0 write time zone success. date=%d", _time_zone);

	LOGI("write date object success.");

	return ret;
}

VhallAmf0Any* VhallAmf0Date::copy()
{
	VhallAmf0Date* copy = new VhallAmf0Date(0);

	copy->_date_value = _date_value;
	copy->_time_zone = _time_zone;

	return copy;
}

int64_t VhallAmf0Date::date()
{
	return _date_value;
}

int16_t VhallAmf0Date::time_zone()
{
	return _time_zone;
}

VhallAmf0Null::VhallAmf0Null()
{
	marker = RTMP_AMF0_Null;
}

VhallAmf0Null::~VhallAmf0Null()
{
}

int VhallAmf0Null::total_size()
{
	return VhallAmf0Size::null();
}

int VhallAmf0Null::read(ByteStream* stream)
{
	return vhall_amf0_read_null(stream);
}

int VhallAmf0Null::write(ByteStream* stream)
{
	return vhall_amf0_write_null(stream);
}

VhallAmf0Any* VhallAmf0Null::copy()
{
	VhallAmf0Null* copy = new VhallAmf0Null();
	return copy;
}

VhallAmf0Undefined::VhallAmf0Undefined()
{
	marker = RTMP_AMF0_Undefined;
}

VhallAmf0Undefined::~VhallAmf0Undefined()
{
}

int VhallAmf0Undefined::total_size()
{
	return VhallAmf0Size::undefined();
}

int VhallAmf0Undefined::read(ByteStream* stream)
{
	return vhall_amf0_read_undefined(stream);
}

int VhallAmf0Undefined::write(ByteStream* stream)
{
	return vhall_amf0_write_undefined(stream);
}

VhallAmf0Any* VhallAmf0Undefined::copy()
{
	VhallAmf0Undefined* copy = new VhallAmf0Undefined();
	return copy;
}

int vhall_amf0_read_any(ByteStream* stream, VhallAmf0Any** ppvalue)
{
	int ret = 0;

	if ((ret = VhallAmf0Any::discovery(stream, ppvalue)) != 0) {
		LOGE("amf0 discovery any elem failed. ret=%d", ret);
		return ret;
	}

	assert(*ppvalue);

	if ((ret = (*ppvalue)->read(stream)) != 0) {
		LOGE("amf0 parse elem failed. ret=%d", ret);
		VHALL_DEL(*ppvalue);
		return ret;
	}

	return ret;
}

int vhall_amf0_read_string(ByteStream* stream, string& value)
{
	int ret = 0;

	// marker
	if (!stream->require(1)) {
		ret = -1;
		LOGE("amf0 read string marker failed. ret=%d", ret);
		return ret;
	}

	char marker = stream->read_1bytes();
	if (marker != RTMP_AMF0_String) {
		ret = -1;
		LOGE("amf0 check string marker failed. "
			"marker=%#x, required=%#x, ret=%d", marker, RTMP_AMF0_String, ret);
		return ret;
	}
	LOGI("amf0 read string marker success");

	return vhall_amf0_read_utf8(stream, value);
}

int vhall_amf0_write_string(ByteStream* stream, string value)
{
	int ret = 0;

	// marker
	if (!stream->require(1)) {
		ret = -1;
		LOGE("amf0 write string marker failed. ret=%d", ret);
		return ret;
	}

	stream->write_1bytes(RTMP_AMF0_String);
	LOGI("amf0 write string marker success");

	return vhall_amf0_write_utf8(stream, value);
}

int vhall_amf0_read_boolean(ByteStream* stream, bool& value)
{
	int ret = 0;

	// marker
	if (!stream->require(1)) {
		ret = -1;
		LOGE("amf0 read bool marker failed. ret=%d", ret);
		return ret;
	}

	char marker = stream->read_1bytes();
	if (marker != RTMP_AMF0_Boolean) {
		ret = -1;
		LOGE("amf0 check bool marker failed. "
			"marker=%#x, required=%#x, ret=%d", marker, RTMP_AMF0_Boolean, ret);
		return ret;
	}
	LOGI("amf0 read bool marker success");

	// value
	if (!stream->require(1)) {
		ret = -1;
		LOGE("amf0 read bool value failed. ret=%d", ret);
		return ret;
	}

	value = (stream->read_1bytes() != 0);

	LOGI("amf0 read bool value success. value=%d", value);

	return ret;
}
int vhall_amf0_write_boolean(ByteStream* stream, bool value)
{
	int ret = 0;

	// marker
	if (!stream->require(1)) {
		ret = -1;
		LOGE("amf0 write bool marker failed. ret=%d", ret);
		return ret;
	}
	stream->write_1bytes(RTMP_AMF0_Boolean);
	LOGI("amf0 write bool marker success");

	// value
	if (!stream->require(1)) {
		ret = -1;
		LOGE("amf0 write bool value failed. ret=%d", ret);
		return ret;
	}

	if (value) {
		stream->write_1bytes(0x01);
	}
	else {
		stream->write_1bytes(0x00);
	}

	LOGI("amf0 write bool value success. value=%d", value);

	return ret;
}

int vhall_amf0_read_number(ByteStream* stream, double& value)
{
	int ret = 0;

	// marker
	if (!stream->require(1)) {
		ret = -1;
		LOGE("amf0 read number marker failed. ret=%d", ret);
		return ret;
	}

	char marker = stream->read_1bytes();
	if (marker != RTMP_AMF0_Number) {
		ret = -1;
		LOGE("amf0 check number marker failed. "
			"marker=%#x, required=%#x, ret=%d", marker, RTMP_AMF0_Number, ret);
		return ret;
	}
	LOGI("amf0 read number marker success");

	// value
	if (!stream->require(8)) {
		ret = -1;
		LOGE("amf0 read number value failed. ret=%d", ret);
		return ret;
	}

	int64_t temp = stream->read_8bytes();
	memcpy(&value, &temp, 8);

	LOGI("amf0 read number value success. value=%.2f", value);

	return ret;
}
int vhall_amf0_write_number(ByteStream* stream, double value)
{
	int ret = 0;

	// marker
	if (!stream->require(1)) {
		ret = -1;
		LOGE("amf0 write number marker failed. ret=%d", ret);
		return ret;
	}

	stream->write_1bytes(RTMP_AMF0_Number);
	LOGI("amf0 write number marker success");

	// value
	if (!stream->require(8)) {
		ret = -1;
		LOGE("amf0 write number value failed. ret=%d", ret);
		return ret;
	}

	int64_t temp = 0x00;
	memcpy(&temp, &value, 8);
	stream->write_8bytes(temp);

	LOGI("amf0 write number value success. value=%.2f", value);

	return ret;
}

int vhall_amf0_read_null(ByteStream* stream)
{
	int ret = 0;

	// marker
	if (!stream->require(1)) {
		ret = -1;
		LOGE("amf0 read null marker failed. ret=%d", ret);
		return ret;
	}

	char marker = stream->read_1bytes();
	if (marker != RTMP_AMF0_Null) {
		ret = -1;
		LOGE("amf0 check null marker failed. "
			"marker=%#x, required=%#x, ret=%d", marker, RTMP_AMF0_Null, ret);
		return ret;
	}
	LOGI("amf0 read null success");

	return ret;
}
int vhall_amf0_write_null(ByteStream* stream)
{
	int ret = 0;

	// marker
	if (!stream->require(1)) {
		ret = -1;
		LOGE("amf0 write null marker failed. ret=%d", ret);
		return ret;
	}

	stream->write_1bytes(RTMP_AMF0_Null);
	LOGI("amf0 write null marker success");

	return ret;
}

int vhall_amf0_read_undefined(ByteStream* stream)
{
	int ret = 0;

	// marker
	if (!stream->require(1)) {
		ret = -1;
		LOGE("amf0 read undefined marker failed. ret=%d", ret);
		return ret;
	}

	char marker = stream->read_1bytes();
	if (marker != RTMP_AMF0_Undefined) {
		ret = -1;
		LOGE("amf0 check undefined marker failed. "
			"marker=%#x, required=%#x, ret=%d", marker, RTMP_AMF0_Undefined, ret);
		return ret;
	}
	LOGI("amf0 read undefined success");

	return ret;
}
int vhall_amf0_write_undefined(ByteStream* stream)
{
	int ret = 0;

	// marker
	if (!stream->require(1)) {
		ret = -1;
		LOGE("amf0 write undefined marker failed. ret=%d", ret);
		return ret;
	}

	stream->write_1bytes(RTMP_AMF0_Undefined);
	LOGI("amf0 write undefined marker success");

	return ret;
}


//namespace _vhall_internal
//{
int vhall_amf0_read_utf8(ByteStream* stream, string& value)
{
	int ret = 0;

	// len
	if (!stream->require(2)) {
		ret = -1;
		LOGE("amf0 read string length failed. ret=%d", ret);
		return ret;
	}
	int16_t len = stream->read_2bytes();
	LOGI("amf0 read string length success. len=%d", len);

	// empty string
	if (len <= 0) {
		LOGI("amf0 read empty string. ret=%d", ret);
		return ret;
	}

	// data
	if (!stream->require(len)) {
		ret = -1;
		LOGE("amf0 read string data failed. ret=%d", ret);
		return ret;
	}
	std::string str = stream->read_string(len);

	// support utf8-1 only
	// 1.3.1 Strings and UTF-8
	// UTF8-1 = %x00-7F
	// TODO: support other utf-8 strings
	/*for (int i = 0; i < len; i++) {
		char ch = *(str.data() + i);
		if ((ch & 0x80) != 0) {
		ret = -1;
		LOGE("ignored. only support utf8-1, 0x00-0x7F, actual is %#x. ret=%d", (int)ch, ret);
		ret = 0;
		}
		}*/

	value = str;
	LOGI("amf0 read string data success. str=%s", str.c_str());

	return ret;
}
int vhall_amf0_write_utf8(ByteStream* stream, string value)
{
	int ret = 0;

	// len
	if (!stream->require(2)) {
		ret = -1;
		LOGE("amf0 write string length failed. ret=%d", ret);
		return ret;
	}
	stream->write_2bytes(value.length());
	LOGI("amf0 write string length success. len=%d", (int)value.length());

	// empty string
	if (value.length() <= 0) {
		LOGI("amf0 write empty string. ret=%d", ret);
		return ret;
	}

	// data
	if (!stream->require(value.length())) {
		ret = -1;
		LOGE("amf0 write string data failed. ret=%d", ret);
		return ret;
	}
	stream->write_string(value);
	LOGI("amf0 write string data success. str=%s", value.c_str());

	return ret;
}

bool vhall_amf0_is_object_eof(ByteStream* stream)
{
	// detect the object-eof specially
	if (stream->require(3)) {
		int32_t flag = stream->read_3bytes();
		stream->skip(-3);

		return 0x09 == flag;
	}

	return false;
}

int vhall_amf0_write_object_eof(ByteStream* stream, VhallAmf0ObjectEOF* value)
{
	int ret = 0;

	assert(value != NULL);

	// value
	if (!stream->require(2)) {
		ret = -1;
		LOGE("amf0 write object eof value failed. ret=%d", ret);
		return ret;
	}
	stream->write_2bytes(0x00);
	LOGI("amf0 write object eof value success");

	// marker
	if (!stream->require(1)) {
		ret = -1;
		LOGE("amf0 write object eof marker failed. ret=%d", ret);
		return ret;
	}

	stream->write_1bytes(RTMP_AMF0_ObjectEnd);

	LOGI("amf0 read object eof success");

	return ret;
}

int vhall_amf0_write_any(ByteStream* stream, VhallAmf0Any* value)
{
	assert(value != NULL);
	return value->write(stream);
}
//}

