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

#include <srs_lib_flv.hpp>

#include <fcntl.h>
#include <sstream>
#include <algorithm>
using namespace std;

#include <srs_rtmp_stack.hpp>
#include <srs_core_autofree.hpp>
#include <srs_kernel_utility.hpp>
#include <srs_kernel_codec.hpp>
#include <srs_kernel_flv.hpp>
#include <srs_kernel_file.hpp>
#include <srs_rtmp_amf0.hpp>
#include <srs_kernel_stream.hpp>
#include <srs_protocol_json.hpp>

// update the flv duration and filesize every this interval in ms.
#define SRS_DVR_UPDATE_DURATION_INTERVAL 60000
#define CONST_MAX_JITTER_MS         250
#define CONST_MAX_JITTER_MS_NEG         -250
#define DEFAULT_FRAME_TIME_MS         10

// for 26ms per audio packet,
// 115 packets is 3s.
#define SRS_PURE_AUDIO_GUESS_COUNT 115

// when got these videos or audios, pure audio or video, mix ok.
#define SRS_MIX_CORRECT_PURE_AV 10
// the type of msgqueue 
#define SRS_MSGQUEUE_TYPE_OTHER 0
#define SRS_MSGQUEUE_TYPE_CONSUMER 1
#define SRS_MSGQUEUE_TYPE_FORWARDER 2
#define SRS_MSGQUEUE_TYPE_EDGER     3

int _srs_time_jitter_string2int(std::string time_jitter) {
   if (time_jitter == "full") {
      return SrsRtmpJitterAlgorithmFULL;
   } else if (time_jitter == "zero") {
      return SrsRtmpJitterAlgorithmZERO;
   } else {
      return SrsRtmpJitterAlgorithmOFF;
   }
}

SrsRtmpJitter::SrsRtmpJitter() {
   last_pkt_correct_time = -1;
   last_pkt_time = 0;
}

SrsRtmpJitter::~SrsRtmpJitter() {
}

int SrsRtmpJitter::correct(SrsSharedPtrMessage* msg, SrsRtmpJitterAlgorithm ag) {
   int ret = ERROR_SUCCESS;

   // for performance issue
   if (ag != SrsRtmpJitterAlgorithmFULL) {
      // all jitter correct features is disabled, ignore.
      if (ag == SrsRtmpJitterAlgorithmOFF) {
         return ret;
      }

      // start at zero, but donot ensure monotonically increasing.
      if (ag == SrsRtmpJitterAlgorithmZERO) {
         // for the first time, last_pkt_correct_time is -1.
         if (last_pkt_correct_time == -1) {
            last_pkt_correct_time = msg->timestamp;
         }
         msg->timestamp -= last_pkt_correct_time;
         return ret;
      }

      // other algorithm, ignore.
      return ret;
   }

   // full jitter algorithm, do jitter correct.
   // set to 0 for metadata.
   if (!msg->is_av()) {
      msg->timestamp = 0;
      return ret;
   }

   /**
   * we use a very simple time jitter detect/correct algorithm:
   * 1. delta: ensure the delta is positive and valid,
   *     we set the delta to DEFAULT_FRAME_TIME_MS,
   *     if the delta of time is nagative or greater than CONST_MAX_JITTER_MS.
   * 2. last_pkt_time: specifies the original packet time,
   *     is used to detect next jitter.
   * 3. last_pkt_correct_time: simply add the positive delta,
   *     and enforce the time monotonically.
   */
   int64_t time = msg->timestamp;
   int64_t delta = time - last_pkt_time;

   // if jitter detected, reset the delta.
   if (delta < CONST_MAX_JITTER_MS_NEG || delta > CONST_MAX_JITTER_MS) {
      // use default 10ms to notice the problem of stream.
      // @see https://github.com/ossrs/srs/issues/425
      delta = DEFAULT_FRAME_TIME_MS;

      srs_info("jitter detected, last_pts=%" PRId64", pts=%" PRId64", diff=%" PRId64", last_time=%" PRId64", time=%" PRId64", diff=%" PRId64"",
               last_pkt_time, time, time - last_pkt_time, last_pkt_correct_time, last_pkt_correct_time + delta, delta);
   } else {
      srs_verbose("timestamp no jitter. time=%" PRId64", last_pkt=%" PRId64", correct_to=%" PRId64"",
                  time, last_pkt_time, last_pkt_correct_time + delta);
   }

   last_pkt_correct_time = srs_max(0, last_pkt_correct_time + delta);

   msg->timestamp = last_pkt_correct_time;
   last_pkt_time = time;

   return ret;
}

int SrsRtmpJitter::get_time() {
   return (int)last_pkt_correct_time;
}

SrsFlvSegment::SrsFlvSegment()
{
    req = NULL;
    jitter = NULL;
    //plan = p;

    fs = new SrsFileWriter();
    enc = new SrsFlvEncoder();
    jitter_algorithm = SrsRtmpJitterAlgorithmOFF;

    path = "";
    has_keyframe = false;
    duration = 0;
    starttime = -1;
    stream_starttime = 0;
    stream_previous_pkt_time = -1;
    stream_duration = 0;

    duration_offset = 0;
    filesize_offset = 0;

   
}

SrsFlvSegment::~SrsFlvSegment()
{
    srs_freep(jitter);
    srs_freep(fs);
    srs_freep(enc);
}

int SrsFlvSegment::initialize(SrsRequest* r)
{
    int ret = ERROR_SUCCESS;

    req = r;

    return ret;
}

bool SrsFlvSegment::is_overflow(int64_t max_duration)
{
    return duration >= max_duration;
}

int SrsFlvSegment::open(bool use_tmp_file)
{
    int ret = ERROR_SUCCESS;
    
    // ignore when already open.
    if (fs->is_open()) {
        return ret;
    }

    path = generate_path();
    bool fresh_flv_file = !srs_path_exists(path);
    
    // create dir first.
    std::string dir = path.substr(0, path.rfind("/"));
    if ((ret = srs_create_dir_recursively(dir)) != ERROR_SUCCESS) {
        srs_error("create dir=%s failed. ret=%d", dir.c_str(), ret);
        return ret;
    }
    srs_info("create dir=%s ok", dir.c_str());

    // create jitter.
    if ((ret = create_jitter(!fresh_flv_file)) != ERROR_SUCCESS) {
        srs_error("create jitter failed, path=%s, fresh=%d. ret=%d", path.c_str(), fresh_flv_file, ret);
        return ret;
    }
    
    // generate the tmp flv path.
    if (!fresh_flv_file || !use_tmp_file) {
        // when path exists, always append to it.
        // so we must use the target flv path as output flv.
        tmp_flv_file = path;
    } else {
        // when path not exists, dvr to tmp file.
        tmp_flv_file = path + ".tmp";
    }
    
    // open file writer, in append or create mode.
    if (!fresh_flv_file) {
        if ((ret = fs->open_append(tmp_flv_file)) != ERROR_SUCCESS) {
            srs_error("append file stream for file %s failed. ret=%d", path.c_str(), ret);
            return ret;
        }
        srs_trace("dvr: always append to when exists, file=%s.", path.c_str());
    } else {
        if ((ret = fs->open(tmp_flv_file)) != ERROR_SUCCESS) {
            srs_error("open file stream for file %s failed. ret=%d", path.c_str(), ret);
            return ret;
        }
    }

    // initialize the encoder.
    if ((ret = enc->initialize(fs)) != ERROR_SUCCESS) {
        srs_error("initialize enc by fs for file %s failed. ret=%d", path.c_str(), ret);
        return ret;
    }
    
    // when exists, donot write flv header.
    if (fresh_flv_file) {
        // write the flv header to writer.
        if ((ret = enc->write_header()) != ERROR_SUCCESS) {
            srs_error("write flv header failed. ret=%d", ret);
            return ret;
        }
    }

    // update the duration and filesize offset.
    duration_offset = 0;
    filesize_offset = 0;
    
    srs_trace("dvr stream %s to file %s", req->stream.c_str(), path.c_str());

    return ret;
}

int SrsFlvSegment::close()
{
    int ret = ERROR_SUCCESS;
    
    // ignore when already closed.
    if (!fs->is_open()) {
        return ret;
    }

    // update duration and filesize.
    if ((ret = update_flv_metadata()) != ERROR_SUCCESS) {
        return ret;
    }
    
    fs->close();
    
    std::string final_path = path;
  
    
    // when tmp flv file exists, reap it.
    if (tmp_flv_file != final_path) {
        if (rename(tmp_flv_file.c_str(), final_path.c_str()) < 0) {
            ret = ERROR_SYSTEM_FILE_RENAME;
            srs_error("rename flv file failed, %s => %s. ret=%d", 
                tmp_flv_file.c_str(), final_path.c_str(), ret);
            return ret;
        }
    }

    // TODO: FIXME: the http callback is async, which will trigger thread switch,
    //          so the on_video maybe invoked during the http callback, and error.
   /* if ((ret = plan->on_reap_segment()) != ERROR_SUCCESS) {
        srs_error("dvr: notify plan to reap segment failed. ret=%d", ret);
        return ret;
    }*/

    return ret;
}

int SrsFlvSegment::write_metadata(SrsSharedPtrMessage* metadata)
{
    int ret = ERROR_SUCCESS;

    if (duration_offset || filesize_offset) {
        return ret;
    }

    SrsStream stream;
    if ((ret = stream.initialize(metadata->payload, metadata->size)) != ERROR_SUCCESS) {
        return ret;
    }

    SrsAmf0Any* name = SrsAmf0Any::str();
    SrsAutoFree(SrsAmf0Any, name);
    if ((ret = name->read(&stream)) != ERROR_SUCCESS) {
        return ret;
    }

    SrsAmf0Object* obj = SrsAmf0Any::object();
    SrsAutoFree(SrsAmf0Object, obj);
    if ((ret = obj->read(&stream)) != ERROR_SUCCESS) {
        return ret;
    }

    // remove duration and filesize.
    obj->set("filesize", NULL);
    obj->set("duration", NULL);

    // add properties.
    obj->set("service", SrsAmf0Any::str(RTMP_SIG_SRS_SERVER));
    obj->set("filesize", SrsAmf0Any::number(0));
    obj->set("duration", SrsAmf0Any::number(0));
    
    int size = name->total_size() + obj->total_size();
    char* payload = new char[size];
    SrsAutoFreeA(char, payload);

    // 11B flv header, 3B object EOF, 8B number value, 1B number flag.
    duration_offset = fs->tellg() + size + 11 - SrsAmf0Size::object_eof() - SrsAmf0Size::number();
    // 2B string flag, 8B number value, 8B string 'duration', 1B number flag
    filesize_offset = duration_offset - SrsAmf0Size::utf8("duration") - SrsAmf0Size::number();

    // convert metadata to bytes.
    if ((ret = stream.initialize(payload, size)) != ERROR_SUCCESS) {
        return ret;
    }
    if ((ret = name->write(&stream)) != ERROR_SUCCESS) {
        return ret;
    }
    if ((ret = obj->write(&stream)) != ERROR_SUCCESS) {
        return ret;
    }
    
    // to flv file.
    if ((ret = enc->write_metadata(18, payload, size)) != ERROR_SUCCESS) {
        return ret;
    }
    
    return ret;
}

int SrsFlvSegment::write_audio(SrsSharedPtrMessage* shared_audio)
{
    int ret = ERROR_SUCCESS;

    SrsSharedPtrMessage* audio = shared_audio->copy();
    SrsAutoFree(SrsSharedPtrMessage, audio);
    
    char* payload = audio->payload;
    int size = audio->size;
    int64_t timestamp = audio->timestamp;
    if ((ret = enc->write_audio(timestamp, payload, size)) != ERROR_SUCCESS) {
        return ret;
    }

    if ((ret = on_update_duration(audio)) != ERROR_SUCCESS) {
        return ret;
    }
    
    return ret;
}

int SrsFlvSegment::write_video(SrsSharedPtrMessage* shared_video)
{
    int ret = ERROR_SUCCESS;

    SrsSharedPtrMessage* video = shared_video->copy();
    SrsAutoFree(SrsSharedPtrMessage, video);
    
    char* payload = video->payload;
    int size = video->size;
    
    bool is_sequence_header = SrsFlvCodec::video_is_sequence_header(payload, size);

    // accept the sequence header here.
    // when got no keyframe, ignore when should wait keyframe.
    if (!has_keyframe && !is_sequence_header) {
        bool wait_keyframe = true;//_srs_config->get_dvr_wait_keyframe(req->vhost);
        if (wait_keyframe) {
            srs_info("dvr: ignore when wait keyframe.");
            return ret;
        }
    }   
    
    // update segment duration, session plan just update the duration,
    // the segment plan will reap segment if exceed, this video will write to next segment.
    if ((ret = on_update_duration(video)) != ERROR_SUCCESS) {
        return ret;
    }
    
    int32_t timestamp = (int32_t)video->timestamp;
    if ((ret = enc->write_video(timestamp, payload, size)) != ERROR_SUCCESS) {
        return ret;
    }
    
    return ret;
}

int SrsFlvSegment::update_flv_metadata()
{
    int ret = ERROR_SUCCESS;

    // no duration or filesize specified.
    if (!duration_offset || !filesize_offset) {
        return ret;
    }

    int64_t cur = fs->tellg();

    // buffer to write the size.
    char* buf = new char[SrsAmf0Size::number()];
    SrsAutoFreeA(char, buf);

    SrsStream stream;
    if ((ret = stream.initialize(buf, SrsAmf0Size::number())) != ERROR_SUCCESS) {
        return ret;
    }

    // filesize to buf.
    SrsAmf0Any* size = SrsAmf0Any::number((double)cur);
    SrsAutoFree(SrsAmf0Any, size);

    stream.skip(-1 * stream.pos());
    if ((ret = size->write(&stream)) != ERROR_SUCCESS) {
        return ret;
    }

    // update the flesize.
    fs->lseek(filesize_offset);
    if ((ret = fs->write(buf, SrsAmf0Size::number(), NULL)) != ERROR_SUCCESS) {
        return ret;
    }
    
    // duration to buf
    SrsAmf0Any* dur = SrsAmf0Any::number((double)duration / 1000.0);
    SrsAutoFree(SrsAmf0Any, dur);
    
    stream.skip(-1 * stream.pos());
    if ((ret = dur->write(&stream)) != ERROR_SUCCESS) {
        return ret;
    }

    // update the duration
    fs->lseek(duration_offset);
    if ((ret = fs->write(buf, SrsAmf0Size::number(), NULL)) != ERROR_SUCCESS) {
        return ret;
    }

    // reset the offset.
    fs->lseek(cur);

    return ret;
}

string SrsFlvSegment::get_path()
{
    return path;
}

void SrsFlvSegment::set_path_partern(std::string dvr_path_partern){
   path = dvr_path_partern;
}

string SrsFlvSegment::generate_path()
{
    // the path in config, for example, 
    //      /data/[vhost]/[app]/[stream]/[2006]/[01]/[02]/[15].[04].[05].[999].flv
   std::string path_config = path_partern;
    
    // add [stream].[timestamp].flv as filename for dir
    if (path_config.find(".flv") != path_config.length() - 4) {
        path_config += "/[stream].[timestamp].flv";
    }
    
    // the flv file path
    std::string flv_path = path_config;
    //flv_path = srs_path_build_stream(flv_path, req->vhost, req->app, req->stream);
   // flv_path = srs_path_build_timestamp(flv_path);

    return flv_path;
}

int SrsFlvSegment::create_jitter(bool loads_from_flv)
{
    int ret = ERROR_SUCCESS;
    
    // when path exists, use exists jitter.
    if (!loads_from_flv) {
        // jitter when publish, ensure whole stream start from 0.
        srs_freep(jitter);
        jitter = new SrsRtmpJitter();

        // fresh stream starting.
        starttime = -1;
        stream_previous_pkt_time = -1;
        stream_starttime = srs_update_system_time_ms();
        stream_duration = 0;
    
        // fresh segment starting.
        has_keyframe = false;
        duration = 0;

        return ret;
    }

    // when jitter ok, do nothing.
    if (jitter) {
        return ret;
    }

    // always ensure the jitter crote.
    // for the first time, initialize jitter from exists file.
    jitter = new SrsRtmpJitter();

    // TODO: FIXME: implements it.

    return ret;
}

int SrsFlvSegment::on_update_duration(SrsSharedPtrMessage* msg)
{
    int ret = ERROR_SUCCESS;
    
    // we must assumpt that the stream timestamp is monotonically increase,
    // that is, always use time jitter to correct the timestamp.
    // except the time jitter is disabled in config.
    
    // set the segment starttime at first time
    if (starttime < 0) {
        starttime = msg->timestamp;
    }
    
    // no previous packet or timestamp overflow.
    if (stream_previous_pkt_time < 0 || stream_previous_pkt_time > msg->timestamp) {
        stream_previous_pkt_time = msg->timestamp;
    }
    
    // collect segment and stream duration, timestamp overflow is ok.
    duration += msg->timestamp - stream_previous_pkt_time;
    stream_duration += msg->timestamp - stream_previous_pkt_time;
    
    // update previous packet time
    stream_previous_pkt_time = msg->timestamp;
    
    return ret;
}

int SrsFlvSegment::on_reload_vhost_dvr(std::string /*vhost*/)
{
    int ret = ERROR_SUCCESS;
    
    jitter_algorithm = SrsRtmpJitterAlgorithmZERO;
    
    return ret;
}




