#include "srs_http_handler.h"
#include "../common/vhall_log.h"
#include <assert.h>

#define ERROR_SUCCESS                    0L
#define FIFOBUFFER_CAPACITY_LENGTH       5 * 1024 * 1024
enum {
   MSG_TIMEOUT = SignalThread::ST_MSG_FIRST_AVAILABLE,
   MSG_LAUNCH_REQUEST,
   MSG_CONNECT_SUCCESS,
   MSG_CONNECT_FAILED
};

static const int kDefaultHTTPTimeout = 10 * 1000;  // 30 sec, should be less for streaming

///////////////////////////////////////////////////////////////////////////////
// SrsAsyncHttpRequest
///////////////////////////////////////////////////////////////////////////////

SrsAsyncHttpRequest::SrsAsyncHttpRequest(std::string url, const std::string &user_agent)
: start_delay_(0),
available(false),
mUrl(url),
firewall_(NULL),
port_(80),
secure_(false),
timeout_(kDefaultHTTPTimeout),
fail_redirect_(false),
factory_(Thread::Current()->socketserver(), user_agent),
pool_(&factory_),
client_(user_agent.c_str(), &pool_),
error_(HE_NONE) {
   client_.SignalHttpClientComplete.connect(this,
      &SrsAsyncHttpRequest::OnComplete);
   client_.SignalHttpClientConnectOK.connect(this,
      &SrsAsyncHttpRequest::OnConnect);

}

SrsAsyncHttpRequest::SrsAsyncHttpRequest(std::string url, talk_base::MessageHandler* muxer, talk_base::Thread* thread, const std::string &user_agent)
: start_delay_(0),
available(false),
mUrl(url),
firewall_(NULL),
port_(80),
secure_(false),
timeout_(kDefaultHTTPTimeout),
fail_redirect_(false),
factory_(Thread::Current()->socketserver(), user_agent),
pool_(&factory_),
client_(user_agent.c_str(), &pool_),
error_(HE_NONE) {
   client_.SignalHttpClientComplete.connect(this,
      &SrsAsyncHttpRequest::OnComplete);
   client_.SignalHttpClientConnectOK.connect(this,
      &SrsAsyncHttpRequest::OnConnect);

   _muxer = muxer;
   _thread = thread;
}

SrsAsyncHttpRequest::~SrsAsyncHttpRequest() {
}


int SrsAsyncHttpRequest::init(std::string url){
   mUrl = url;
   return 0;
}
int SrsAsyncHttpRequest::doConnect(){
   mFlvBuffer = new talk_base::FifoBuffer(FIFOBUFFER_CAPACITY_LENGTH);

   talk_base::Url<char> url(mUrl);
   set_host(url.host());
   set_port(url.port());
   request().verb = talk_base::HV_POST;
   request().path = url.path();
   request().setContent("application/octet-stream", mFlvBuffer);
   request().setHeader(talk_base::HH_CONNECTION, "Keep-Alive", false);
   response().document.reset(new talk_base::MemoryStream());
   Start();

   return 0;
}
int SrsAsyncHttpRequest::reConnect(){
   Start();
   return 0;
}
void SrsAsyncHttpRequest::stop(){
   client_.reset();
   worker()->Quit();
}

void SrsAsyncHttpRequest::OnMessage(Message* message) {
   switch (message->message_id) {

   case MSG_TIMEOUT:
      LOG(LS_INFO) << "HttpRequest timed out";
      client_.reset();
      worker()->Quit();
      break;
   case MSG_LAUNCH_REQUEST:
      LaunchRequest();
      break;
   default:
      SignalThread::OnMessage(message);
      break;
   }
}

void SrsAsyncHttpRequest::OnComplete(HttpClient* client, HttpErrorType error) {
   // todo: fix the line crush
   //Thread::Current()->Clear(this, MSG_TIMEOUT);
   Thread *current = Thread::Current();
   if (current) {
      current->Clear(this, MSG_TIMEOUT);
   }

   set_error(error);
   if (!error) {
      LOG(LS_INFO) << "HttpRequest completed successfully";

      std::string value;
      if (client_.response().hasHeader(HH_LOCATION, &value)) {
         response_redirect_ = value.c_str();
      }
      _thread->Post(_muxer, MSG_CONNECT_FAILED);
   }
   else {
      LOG(LS_INFO) << "HttpRequest completed with error: " << error;
      _thread->Post(_muxer, MSG_CONNECT_FAILED);
   }

   available = false;
   worker()->Quit();
}

void SrsAsyncHttpRequest::OnConnect(HttpClient* client, HttpErrorType error){
   Thread::Current()->Clear(this, MSG_TIMEOUT);
   if (!error) {
      LOG(LS_INFO) << "Http connect successfully";
      _thread->Post(_muxer, MSG_CONNECT_SUCCESS);
      available = true;
   }
   else {
      LOG(LS_INFO) << "Http connect with error: " << error;
      _thread->Post(_muxer, MSG_CONNECT_FAILED);
      available = false;
      worker()->Quit();
   }
}

bool SrsAsyncHttpRequest::is_connected(){
   return true;
}

void SrsAsyncHttpRequest::OnWorkStart() {
   if (start_delay_ <= 0) {
      LaunchRequest();
   }
   else {
      Thread::Current()->PostDelayed(start_delay_, this, MSG_LAUNCH_REQUEST);
   }
}

void SrsAsyncHttpRequest::OnWorkStop() {
   // worker is already quitting, no need to explicitly quit
   LOG(LS_INFO) << "HttpRequest cancelled";
}

void SrsAsyncHttpRequest::DoWork() {
   // Do nothing while we wait for the request to finish. We only do this so
   // that we can be a SignalThread; in the future this class should not be
   // a SignalThread, since it does not need to spawn a new thread.
   Thread::Current()->ProcessMessages(kForever);
}

void SrsAsyncHttpRequest::LaunchRequest() {
   factory_.SetProxy(proxy_);
   if (secure_)
      factory_.UseSSL(host_.c_str());

   bool transparent_proxy = (port_ == 80) &&
      ((proxy_.type == PROXY_HTTPS) || (proxy_.type == PROXY_UNKNOWN));
   if (transparent_proxy) {
      client_.set_proxy(proxy_);
   }
   client_.set_fail_redirect(fail_redirect_);
   client_.set_server(SocketAddress(host_, port_));

   LOG(LS_INFO) << "HttpRequest start: " << host_ + client_.request().path;

   // changed for http-flv
   Thread::Current()->PostDelayed(timeout_, this, MSG_TIMEOUT);
   client_.start();
}

bool SrsAsyncHttpRequest::is_open(){
   return available;
}

int SrsAsyncHttpRequest::write(void* buf, size_t count, ssize_t* pnwrite){
   int ret = ERROR_SUCCESS;
   size_t written = 0;
   int error = 0;

   if (mFlvBuffer && available){
      talk_base::StreamResult sret;
      sret = mFlvBuffer->Write(buf, count, &written, &error);
      //if (sret != talk_base::SR_SUCCESS && sret != talk_base::SR_BLOCK){
      if (sret != talk_base::SR_SUCCESS){
         ret = -1;
         LOGE("srsHttpWriter write data error, ret=%d", ret);
         return ret;
      }
   }

   if (pnwrite != NULL){
      *pnwrite = written;
   }

   return ret;
}

int SrsAsyncHttpRequest::writev(iovec* iov, int iovcnt, ssize_t* pnwrite){
   int ret = ERROR_SUCCESS;

   ssize_t nwrite = 0;
   for (int i = 0; i < iovcnt; i++) {
      iovec* piov = iov + i;
      ssize_t this_nwrite = 0;
      if ((ret = this->write(piov->iov_base, piov->iov_len, &this_nwrite)) != ERROR_SUCCESS) {
         LOGE("srsHttpWriter write data error, ret=%d", ret);
         return ret;
      }
      nwrite += this_nwrite;
   }

   if (pnwrite) {
      *pnwrite = nwrite;
   }

   return ret;
}

SrsDataStream::SrsDataStream()
{
   p = bytes = NULL;
   nb_bytes = 0;

   // TODO: support both little and big endian.
   //assert(srs_is_little_endian());
}
SrsDataStream::~SrsDataStream()
{
}
int SrsDataStream::initialize(char* b, int nb)
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

   return ret;
}
int SrsDataStream::size()
{
   return nb_bytes;
}
bool SrsDataStream::require(int required_size)
{
   assert(required_size >= 0);

   return required_size <= nb_bytes - (p - bytes);
}
void SrsDataStream::write_1bytes(int8_t value)
{
   assert(require(1));

   *p++ = value;
}
void SrsDataStream::write_2bytes(int16_t value)
{
   assert(require(2));

   char* pp = (char*)&value;
   *p++ = pp[1];
   *p++ = pp[0];
}
void SrsDataStream::write_4bytes(int32_t value)
{
   assert(require(4));

   char* pp = (char*)&value;
   *p++ = pp[3];
   *p++ = pp[2];
   *p++ = pp[1];
   *p++ = pp[0];
}
void SrsDataStream::write_3bytes(int32_t value)
{
   assert(require(3));

   char* pp = (char*)&value;
   *p++ = pp[2];
   *p++ = pp[1];
   *p++ = pp[0];
}
