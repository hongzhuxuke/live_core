#ifndef RTMP_PROTOCOL_AMF0_HPP
#define RTMP_PROTOCOL_AMF0_HPP

#include <stdint.h>
#include <string>
#include <vector>

#define VHALL_CONSTS_RTMP_SET_DATAFRAME            "@setDataFrame"
#define VHALL_CONSTS_RTMP_ON_METADATA              "onMetaData"
#define VHALL_CONSTS_RTMP_ON_CUEPOINT              "onCuePoint"

#define vhall_freep(p) \
if (p) { \
delete p; \
p = NULL; \
} \
(void)0

class ByteStream;
class VhallAmf0Object;
class VhallAmf0EcmaArray;
class VhallAmf0StrictArray;

class VhallUnSortedHashtable;
class VhallAmf0ObjectEOF;
class VhallAmf0Date;


class VhallAmf0Any
{
public:
    char marker;
public:
    VhallAmf0Any();
    virtual ~VhallAmf0Any();
// type identify, user should identify the type then convert from/to value.
public:
    /**
    * whether current instance is an AMF0 string.
    * @return true if instance is an AMF0 string; otherwise, false.
    * @remark, if true, use to_string() to get its value.
    */
    virtual bool is_string();
    /**
    * whether current instance is an AMF0 boolean.
    * @return true if instance is an AMF0 boolean; otherwise, false.
    * @remark, if true, use to_boolean() to get its value.
    */
    virtual bool is_boolean();
    /**
    * whether current instance is an AMF0 number.
    * @return true if instance is an AMF0 number; otherwise, false.
    * @remark, if true, use to_number() to get its value.
    */
    virtual bool is_number();
    /**
    * whether current instance is an AMF0 null.
    * @return true if instance is an AMF0 null; otherwise, false.
    */
    virtual bool is_null();
    /**
    * whether current instance is an AMF0 undefined.
    * @return true if instance is an AMF0 undefined; otherwise, false.
    */
    virtual bool is_undefined();
    /**
    * whether current instance is an AMF0 object.
    * @return true if instance is an AMF0 object; otherwise, false.
    * @remark, if true, use to_object() to get its value.
    */
    virtual bool is_object();
    /**
    * whether current instance is an AMF0 object-EOF.
    * @return true if instance is an AMF0 object-EOF; otherwise, false.
    */
    virtual bool is_object_eof();
    /**
    * whether current instance is an AMF0 ecma-array.
    * @return true if instance is an AMF0 ecma-array; otherwise, false.
    * @remark, if true, use to_ecma_array() to get its value.
    */
    virtual bool is_ecma_array();
    /**
    * whether current instance is an AMF0 strict-array.
    * @return true if instance is an AMF0 strict-array; otherwise, false.
    * @remark, if true, use to_strict_array() to get its value.
    */
    virtual bool is_strict_array();
    /**
    * whether current instance is an AMF0 date.
    * @return true if instance is an AMF0 date; otherwise, false.
    * @remark, if true, use to_date() to get its value.
    */
    virtual bool is_date();
    /**
    * whether current instance is an AMF0 object, object-EOF, ecma-array or strict-array.
    */
    virtual bool is_complex_object();
// get value of instance
public:
    /**
    * get a string copy of instance.
    * @remark assert is_string(), user must ensure the type then convert.
    */
    virtual std::string to_str();
    /**
    * get the raw str of instance,
    * user can directly set the content of str.
    * @remark assert is_string(), user must ensure the type then convert.
    */
    virtual const char* to_str_raw();
    /**
    * convert instance to amf0 boolean,
    * @remark assert is_boolean(), user must ensure the type then convert.
    */
    virtual bool to_boolean();
    /**
    * convert instance to amf0 number,
    * @remark assert is_number(), user must ensure the type then convert.
    */
    virtual double to_number();
    /**
    * convert instance to date,
    * @remark assert is_date(), user must ensure the type then convert.
    */
    virtual int64_t to_date();
    virtual int16_t to_date_time_zone();
    /**
    * convert instance to amf0 object,
    * @remark assert is_object(), user must ensure the type then convert.
    */
    virtual VhallAmf0Object* to_object();
    /**
    * convert instance to ecma array,
    * @remark assert is_ecma_array(), user must ensure the type then convert.
    */
    virtual VhallAmf0EcmaArray* to_ecma_array();
    /**
    * convert instance to strict array,
    * @remark assert is_strict_array(), user must ensure the type then convert.
    */
    virtual VhallAmf0StrictArray* to_strict_array();
// set value of instance
public:
    /**
    * set the number of any when is_number() indicates true.
    * user must ensure the type is a number, or assert failed.
    */
    virtual void set_number(double value);
// serialize/deseriaize instance.
public:
    /**
    * get the size of amf0 any, including the marker size.
    * the size is the bytes which instance serialized to.
    */
    virtual int total_size() = 0;
    /**
    * read AMF0 instance from stream.
    */
    virtual int read(ByteStream* stream) = 0;
    /**
    * write AMF0 instance to stream.
    */
    virtual int write(ByteStream* stream) = 0;
    /**
    * copy current AMF0 instance.
    */
    virtual VhallAmf0Any* copy() = 0;
    /**
    * human readable print 
    * @param pdata, output the heap data, NULL to ignore.
    * @return return the *pdata for print. NULL to ignore.
    * @remark user must free the data returned or output by pdata.
    */
    virtual char* human_print(char** pdata, int* psize);
// create AMF0 instance.
public:
    /**
    * create an AMF0 string instance, set string content by value.
    */
    static VhallAmf0Any* str(const char* value = NULL); 
    /**
    * create an AMF0 boolean instance, set boolean content by value.
    */
    static VhallAmf0Any* boolean(bool value = false);
    /**
    * create an AMF0 number instance, set number content by value.
    */
    static VhallAmf0Any* number(double value = 0.0);
    /**
    * create an AMF0 date instance
    */
    static VhallAmf0Any* date(int64_t value = 0);
    /**
    * create an AMF0 null instance
    */
    static VhallAmf0Any* null();
    /**
    * create an AMF0 undefined instance
    */
    static VhallAmf0Any* undefined();
    /**
    * create an AMF0 empty object instance
    */
    static VhallAmf0Object* object();
    /**
    * create an AMF0 object-EOF instance
    */
    static VhallAmf0Any* object_eof();
    /**
    * create an AMF0 empty ecma-array instance
    */
    static VhallAmf0EcmaArray* ecma_array();
    /**
    * create an AMF0 empty strict-array instance
    */
    static VhallAmf0StrictArray* strict_array();
// discovery instance from stream
public:
    /**
    * discovery AMF0 instance from stream
    * @param ppvalue, output the discoveried AMF0 instance.
    *       NULL if error.
    * @remark, instance is created without read from stream, user must
    *       use (*ppvalue)->read(stream) to get the instance.
    */
    static int discovery(ByteStream* stream, VhallAmf0Any** ppvalue);
};

/**
* 2.5 Object Type
* anonymous-object-type = object-marker *(object-property)
* object-property = (UTF-8 value-type) | (UTF-8-empty object-end-marker)
*/
class VhallAmf0Object : public VhallAmf0Any
{
private:
    VhallUnSortedHashtable* properties;
    VhallAmf0ObjectEOF* eof;
private:
    friend class VhallAmf0Any;
    /**
    * make amf0 object to private,
    * use should never declare it, use VhallAmf0Any::object() to create it.
    */
    VhallAmf0Object();
public:
    virtual ~VhallAmf0Object();
// serialize/deserialize to/from stream.
public:
    virtual int total_size();
    virtual int read(ByteStream* stream);
    virtual int write(ByteStream* stream);
    virtual VhallAmf0Any* copy();
// properties iteration
public:
    /**
    * clear all propergies.
    */
    virtual void clear();
    /**
    * get the count of properties(key:value).
    */
    virtual int count();
    /**
    * get the property(key:value) key at index.
    * @remark: max index is count().
    */
    virtual std::string key_at(int index);
    /**
    * get the property(key:value) key raw bytes at index.
    * user can directly set the key bytes.
    * @remark: max index is count().
    */
    virtual const char* key_raw_at(int index);
    /**
    * get the property(key:value) value at index.
    * @remark: max index is count().
    */
    virtual VhallAmf0Any* value_at(int index);
// property set/get.
public:
    /**
    * set the property(key:value) of object,
    * @param key, string property name.
    * @param value, an AMF0 instance property value.
    * @remark user should never free the value, this instance will manage it.
    */
    virtual void set(std::string key, VhallAmf0Any* value);
    /**
    * get the property(key:value) of object,
    * @param name, the property name/key
    * @return the property AMF0 value, NULL if not found.
    * @remark user should never free the returned value, copy it if needed.
    */
    virtual VhallAmf0Any* get_property(std::string name);
    /**
    * get the string property, ensure the property is_string().
    * @return the property AMF0 value, NULL if not found, or not a string.
    * @remark user should never free the returned value, copy it if needed.
    */
    virtual VhallAmf0Any* ensure_property_string(std::string name);
    /**
    * get the number property, ensure the property is_number().
    * @return the property AMF0 value, NULL if not found, or not a number.
    * @remark user should never free the returned value, copy it if needed.
    */
    virtual VhallAmf0Any* ensure_property_number(std::string name);
    /**
     * remove the property specified by name.
     */
    virtual void remove(std::string name);
};

/**
* 2.10 ECMA Array Type
* ecma-array-type = associative-count *(object-property)
* associative-count = U32
* object-property = (UTF-8 value-type) | (UTF-8-empty object-end-marker)
*/
class VhallAmf0EcmaArray : public VhallAmf0Any
{
private:
    VhallUnSortedHashtable* properties;
    VhallAmf0ObjectEOF* eof;
    int32_t _count;
private:
    friend class VhallAmf0Any;
    /**
    * make amf0 object to private,
    * use should never declare it, use VhallAmf0Any::ecma_array() to create it.
    */
    VhallAmf0EcmaArray();
public:
    virtual ~VhallAmf0EcmaArray();
// serialize/deserialize to/from stream.
public:
    virtual int total_size();
    virtual int read(ByteStream* stream);
    virtual int write(ByteStream* stream);
    virtual VhallAmf0Any* copy();
// properties iteration
public:
    /**
    * clear all propergies.
    */
    virtual void clear();
    /**
    * get the count of properties(key:value).
    */
    virtual int count();
    /**
    * get the property(key:value) key at index.
    * @remark: max index is count().
    */
    virtual std::string key_at(int index);
    /**
    * get the property(key:value) key raw bytes at index.
    * user can directly set the key bytes.
    * @remark: max index is count().
    */
    virtual const char* key_raw_at(int index);
    /**
    * get the property(key:value) value at index.
    * @remark: max index is count().
    */
    virtual VhallAmf0Any* value_at(int index);
// property set/get.
public:
    /**
    * set the property(key:value) of array,
    * @param key, string property name.
    * @param value, an AMF0 instance property value.
    * @remark user should never free the value, this instance will manage it.
    */
    virtual void set(std::string key, VhallAmf0Any* value);
    /**
    * get the property(key:value) of array,
    * @param name, the property name/key
    * @return the property AMF0 value, NULL if not found.
    * @remark user should never free the returned value, copy it if needed.
    */
    virtual VhallAmf0Any* get_property(std::string name);
    /**
    * get the string property, ensure the property is_string().
    * @return the property AMF0 value, NULL if not found, or not a string.
    * @remark user should never free the returned value, copy it if needed.
    */
    virtual VhallAmf0Any* ensure_property_string(std::string name);
    /**
    * get the number property, ensure the property is_number().
    * @return the property AMF0 value, NULL if not found, or not a number.
    * @remark user should never free the returned value, copy it if needed.
    */
    virtual VhallAmf0Any* ensure_property_number(std::string name);
};

/**
* 2.12 Strict Array Type
* array-count = U32 
* strict-array-type = array-count *(value-type)
*/
class VhallAmf0StrictArray : public VhallAmf0Any
{
private:
    std::vector<VhallAmf0Any*> properties;
    int32_t _count;
private:
    friend class VhallAmf0Any;
    /**
    * make amf0 object to private,
    * use should never declare it, use VhallAmf0Any::strict_array() to create it.
    */
    VhallAmf0StrictArray();
public:
    virtual ~VhallAmf0StrictArray();
// serialize/deserialize to/from stream.
public:
    virtual int total_size();
    virtual int read(ByteStream* stream);
    virtual int write(ByteStream* stream);
    virtual VhallAmf0Any* copy();
// properties iteration
public:
    /**
    * clear all elements.
    */
    virtual void clear();
    /**
    * get the count of elements
    */
    virtual int count();
    /**
    * get the elements key at index.
    * @remark: max index is count().
    */
    virtual VhallAmf0Any* at(int index);
// property set/get.
public:
    /**
    * append new element to array
    * @param any, an AMF0 instance property value.
    * @remark user should never free the any, this instance will manage it.
    */
    virtual void append(VhallAmf0Any* any);
};

/**
* the class to get amf0 object size
*/
class VhallAmf0Size
{
public:
    static int utf8(std::string value);
    static int str(std::string value);
    static int number();
    static int date();
    static int null();
    static int undefined();
    static int boolean();
    static int object(VhallAmf0Object* obj);
    static int object_eof();
    static int ecma_array(VhallAmf0EcmaArray* arr);
    static int strict_array(VhallAmf0StrictArray* arr);
    static int any(VhallAmf0Any* o);
};

/**
* read anything from stream.
* @param ppvalue, the output amf0 any elem.
*         NULL if error; otherwise, never NULL and user must free it.
*/
extern int vhall_amf0_read_any(ByteStream* stream, VhallAmf0Any** ppvalue);

/**
* read amf0 string from stream.
* 2.4 String Type
* string-type = string-marker UTF-8
*/
extern int vhall_amf0_read_string(ByteStream* stream, std::string& value);
extern int vhall_amf0_write_string(ByteStream* stream, std::string value);

/**
* read amf0 boolean from stream.
* 2.4 String Type
* boolean-type = boolean-marker U8
*         0 is false, <> 0 is true
*/
extern int vhall_amf0_read_boolean(ByteStream* stream, bool& value);
extern int vhall_amf0_write_boolean(ByteStream* stream, bool value);

/**
* read amf0 number from stream.
* 2.2 Number Type
* number-type = number-marker DOUBLE
*/
extern int vhall_amf0_read_number(ByteStream* stream, double& value);
extern int vhall_amf0_write_number(ByteStream* stream, double value);

/**
* read amf0 null from stream.
* 2.7 null Type
* null-type = null-marker
*/
extern int vhall_amf0_read_null(ByteStream* stream);
extern int vhall_amf0_write_null(ByteStream* stream);

/**
* read amf0 undefined from stream.
* 2.8 undefined Type
* undefined-type = undefined-marker
*/
extern int vhall_amf0_read_undefined(ByteStream* stream);
extern int vhall_amf0_write_undefined(ByteStream* stream);

// internal objects, user should never use it.
//namespace _vhall_internal
//{
    /**
    * read amf0 string from stream.
    * 2.4 String Type
    * string-type = string-marker UTF-8
    * @return default value is empty string.
    * @remark: use VhallAmf0Any::str() to create it.
    */
    class VhallAmf0String : public VhallAmf0Any
    {
    public:
        std::string value;
    private:
        friend class VhallAmf0Any;
        /**
        * make amf0 string to private,
        * use should never declare it, use VhallAmf0Any::str() to create it.
        */
        VhallAmf0String(const char* _value);
    public:
        virtual ~VhallAmf0String();
    public:
        virtual int total_size();
        virtual int read(ByteStream* stream);
        virtual int write(ByteStream* stream);
        virtual VhallAmf0Any* copy();
    };
    
    /**
    * read amf0 boolean from stream.
    * 2.4 String Type
    * boolean-type = boolean-marker U8
    *         0 is false, <> 0 is true
    * @return default value is false.
    */
    class VhallAmf0Boolean : public VhallAmf0Any
    {
    public:
        bool value;
    private:
        friend class VhallAmf0Any;
        /**
        * make amf0 boolean to private,
        * use should never declare it, use VhallAmf0Any::boolean() to create it.
        */
        VhallAmf0Boolean(bool _value);
    public:
        virtual ~VhallAmf0Boolean();
    public:
        virtual int total_size();
        virtual int read(ByteStream* stream);
        virtual int write(ByteStream* stream);
        virtual VhallAmf0Any* copy();
    };
    
    /**
    * read amf0 number from stream.
    * 2.2 Number Type
    * number-type = number-marker DOUBLE
    * @return default value is 0.
    */
    class VhallAmf0Number : public VhallAmf0Any
    {
    public:
        double value;
    private:
        friend class VhallAmf0Any;
        /**
        * make amf0 number to private,
        * use should never declare it, use VhallAmf0Any::number() to create it.
        */
        VhallAmf0Number(double _value);
    public:
        virtual ~VhallAmf0Number();
    public:
        virtual int total_size();
        virtual int read(ByteStream* stream);
        virtual int write(ByteStream* stream);
        virtual VhallAmf0Any* copy();
    };
    
    /**
    * 2.13 Date Type
    * time-zone = S16 ; reserved, not supported should be set to 0x0000
    * date-type = date-marker DOUBLE time-zone
    * @see: https://github.com/ossrs/srs/issues/185
    */
    class VhallAmf0Date : public VhallAmf0Any
    {
    private:
        int64_t _date_value;
        int16_t _time_zone;
    private:
        friend class VhallAmf0Any;
        /**
        * make amf0 date to private,
        * use should never declare it, use VhallAmf0Any::date() to create it.
        */
        VhallAmf0Date(int64_t value);
    public:
        virtual ~VhallAmf0Date();
    // serialize/deserialize to/from stream.
    public:
        virtual int total_size();
        virtual int read(ByteStream* stream);
        virtual int write(ByteStream* stream);
        virtual VhallAmf0Any* copy();
    public:
        /**
        * get the date value.
        */
        virtual int64_t date();
        /**
        * get the time_zone.
        */
        virtual int16_t time_zone();
    };
    
    /**
    * read amf0 null from stream.
    * 2.7 null Type
    * null-type = null-marker
    */
    class VhallAmf0Null : public VhallAmf0Any
    {
    private:
        friend class VhallAmf0Any;
        /**
        * make amf0 null to private,
        * use should never declare it, use VhallAmf0Any::null() to create it.
        */
        VhallAmf0Null();
    public:
        virtual ~VhallAmf0Null();
    public:
        virtual int total_size();
        virtual int read(ByteStream* stream);
        virtual int write(ByteStream* stream);
        virtual VhallAmf0Any* copy();
    };
    
    /**
    * read amf0 undefined from stream.
    * 2.8 undefined Type
    * undefined-type = undefined-marker
    */
    class VhallAmf0Undefined : public VhallAmf0Any
    {
    private:
        friend class VhallAmf0Any;
        /**
        * make amf0 undefined to private,
        * use should never declare it, use VhallAmf0Any::undefined() to create it.
        */
        VhallAmf0Undefined();
    public:
        virtual ~VhallAmf0Undefined();
    public:
        virtual int total_size();
        virtual int read(ByteStream* stream);
        virtual int write(ByteStream* stream);
        virtual VhallAmf0Any* copy();
    };
    
    /**
    * to ensure in inserted order.
    * for the FMLE will crash when AMF0Object is not ordered by inserted,
    * if ordered in map, the string compare order, the FMLE will creash when
    * get the response of connect app.
    */
    class VhallUnSortedHashtable
    {
    private:
        typedef std::pair<std::string, VhallAmf0Any*> VhallAmf0ObjectPropertyType;
        std::vector<VhallAmf0ObjectPropertyType> properties;
    public:
        VhallUnSortedHashtable();
        virtual ~VhallUnSortedHashtable();
    public:
        virtual int count();
        virtual void clear();
        virtual std::string key_at(int index);
        virtual const char* key_raw_at(int index);
        virtual VhallAmf0Any* value_at(int index);
        /**
        * set the value of hashtable.
        * @param value, the value to set. NULL to delete the property.
        */
        virtual void set(std::string key, VhallAmf0Any* value);
    public:
        virtual VhallAmf0Any* get_property(std::string name);
        virtual VhallAmf0Any* ensure_property_string(std::string name);
        virtual VhallAmf0Any* ensure_property_number(std::string name);
        virtual void remove(std::string name);
    public:
        virtual void copy(VhallUnSortedHashtable* src);
    };
    
    /**
    * 2.11 Object End Type
    * object-end-type = UTF-8-empty object-end-marker
    * 0x00 0x00 0x09
    */
    class VhallAmf0ObjectEOF : public VhallAmf0Any
    {
    public:
        VhallAmf0ObjectEOF();
        virtual ~VhallAmf0ObjectEOF();
    public:
        virtual int total_size();
        virtual int read(ByteStream* stream);
        virtual int write(ByteStream* stream);
        virtual VhallAmf0Any* copy();
    };

    /**
    * read amf0 utf8 string from stream.
    * 1.3.1 Strings and UTF-8
    * UTF-8 = U16 *(UTF8-char)
    * UTF8-char = UTF8-1 | UTF8-2 | UTF8-3 | UTF8-4
    * UTF8-1 = %x00-7F
    * @remark only support UTF8-1 char.
    */
    extern int vhall_amf0_read_utf8(ByteStream* stream, std::string& value);
    extern int vhall_amf0_write_utf8(ByteStream* stream, std::string value);
    
    extern bool vhall_amf0_is_object_eof(ByteStream* stream);
    extern int vhall_amf0_write_object_eof(ByteStream* stream, VhallAmf0ObjectEOF* value);
    
    extern int vhall_amf0_write_any(ByteStream* stream, VhallAmf0Any* value);
//};

#endif
