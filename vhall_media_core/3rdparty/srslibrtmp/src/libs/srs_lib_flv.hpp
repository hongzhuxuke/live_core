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

#ifndef SRS_APP_DVR_HPP
#define SRS_APP_DVR_HPP

#include <srs_core.hpp>

#include <string>
#include <sstream>

class SrsRequest;
class SrsStream;
class SrsRtmpJitter;
class SrsOnMetaDataPacket;
class SrsSharedPtrMessage;
class SrsFileWriter;
class SrsFlvEncoder;

class SrsJsonAny;
class SrsJsonObject;

/**
* the time jitter algorithm:
* 1. full, to ensure stream start at zero, and ensure stream monotonically increasing.
* 2. zero, only ensure sttream start at zero, ignore timestamp jitter.
* 3. off, disable the time jitter algorithm, like atc.
*/
enum SrsRtmpJitterAlgorithm {
   SrsRtmpJitterAlgorithmFULL = 0x01,
   SrsRtmpJitterAlgorithmZERO,
   SrsRtmpJitterAlgorithmOFF
};
int _srs_time_jitter_string2int(std::string time_jitter);

/**
* time jitter detect and correct,
* to ensure the rtmp stream is monotonically.
*/
class SrsRtmpJitter {
private:
   int64_t last_pkt_time;
   int64_t last_pkt_correct_time;
public:
   SrsRtmpJitter();
   virtual ~SrsRtmpJitter();
public:
   /**
   * detect the time jitter and correct it.
   * @param ag the algorithm to use for time jitter.
   */
   virtual int correct(SrsSharedPtrMessage* msg, SrsRtmpJitterAlgorithm ag);
   /**
   * get current client time, the last packet time.
   */
   virtual int get_time();
};

/**
* a piece of flv segment.
* when open segment, support start at 0 or not.
*/
class SrsFlvSegment
{
private:
    SrsRequest* req;
    //SrsDvrPlan* plan;
private:
    /**
    * the underlayer dvr stream.
    * if close, the flv is reap and closed.
    * if open, new flv file is crote.
    */
    SrsFlvEncoder* enc;
    SrsRtmpJitter* jitter;
    SrsRtmpJitterAlgorithm jitter_algorithm;
    SrsFileWriter* fs;
private:
    /**
    * the offset of file for duration value.
    * the next 8 bytes is the double value.
    */
    int64_t duration_offset;
    /**
    * the offset of file for filesize value.
    * the next 8 bytes is the double value.
    */
    int64_t filesize_offset;
private:
    std::string tmp_flv_file;
private:
   /**
   *  flv file path partern .
   */
   std::string path_partern;
    /**
    * current segment flv file path.
    */
    std::string path;
    /**
    * whether current segment has keyframe.
    */
    bool has_keyframe;
    /**
    * current segment starttime, RTMP pkt time.
    */
    int64_t starttime;
    /**
    * current segment duration
    */
    int64_t duration;
    /**
    * stream start time, to generate atc pts. abs time.
    */
    int64_t stream_starttime;
    /**
    * stream duration, to generate atc segment.
    */
    int64_t stream_duration;
    /**
    * previous stream RTMP pkt time, used to calc the duration.
    * for the RTMP timestamp will overflow.
    */
    int64_t stream_previous_pkt_time;
public:
    SrsFlvSegment();
    virtual ~SrsFlvSegment();
public:
    /**
    * initialize the segment.
    */
    virtual int initialize(SrsRequest* r);
    /**
    * whether segment is overflow.
    */
    virtual bool is_overflow(int64_t max_duration);
    /**
    * open new segment file, timestamp start at 0 for fresh flv file.
    * @remark ignore when already open.
    * @param use_tmp_file whether use tmp file if possible.
    */
    virtual int open(bool use_tmp_file = true);
    /**
    * close current segment.
    * @remark ignore when already closed.
    */
    virtual int close();
    /**
    * write the metadata to segment.
    */
    virtual int write_metadata(SrsSharedPtrMessage* metadata);
    /**
    * @param shared_audio, directly ptr, copy it if need to save it.
    */
    virtual int write_audio(SrsSharedPtrMessage* shared_audio);
    /**
    * @param shared_video, directly ptr, copy it if need to save it.
    */
    virtual int write_video(SrsSharedPtrMessage* shared_video);
    /**
    * update the flv metadata.
    */
    virtual int update_flv_metadata();
    /**
    * get the current dvr path.
    */
    virtual std::string get_path();
    /**
    * get the current dvr path.
    */
    virtual void set_path_partern(std::string dvr_path_partern);
private:
    /**
    * generate the flv segment path.
    */
    virtual std::string generate_path();
    /**
    * create flv jitter. load jitter when flv exists.
    * @param loads_from_flv whether loads the jitter from exists flv file.
    */
    virtual int create_jitter(bool loads_from_flv);
    /**
    * when update the duration of segment by rtmp msg.
    */
    virtual int on_update_duration(SrsSharedPtrMessage* msg);
// interface ISrsReloadHandler
public:
    virtual int on_reload_vhost_dvr(std::string vhost);
};


#endif



