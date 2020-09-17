#ifndef __SRS_HTTP_HANDLER_H__
#define __SRS_HTTP_HANDLER_H__

#include <string>
#include <srs_librtmp.h>
#include "talk/base/event.h"
#include "talk/base/httpclient.h"
#include "talk/base/signalthread.h"
#include "talk/base/socketpool.h"
#include "talk/base/sslsocketfactory.h"
#include "talk/base/firewallsocketserver.h"
#include "talk/base/cryptstring.h"

using namespace talk_base;

class FirewallManager;

class SrsAsyncHttpRequest : public SignalThread{
public:
   SrsAsyncHttpRequest(std::string url, const std::string &user_agent = "vhall upload");
   SrsAsyncHttpRequest(std::string url, talk_base::MessageHandler* _muxer, talk_base::Thread* _thread, const std::string &user_agent = "vhall stream");
   ~SrsAsyncHttpRequest();
private:
   talk_base::MessageHandler* _muxer;
   talk_base::Thread* _thread;

public:
   // If start_delay is less than or equal to zero, this starts immediately.
   // Start_delay defaults to zero.
   int start_delay() const { return start_delay_; }
   void set_start_delay(int delay) { start_delay_ = delay; }

   const ProxyInfo& proxy() const { return proxy_; }
   void set_proxy(const ProxyInfo& proxy) {
      proxy_ = proxy;
   }
   void set_proxy(std::string host, int port, int type, std::string username, std::string password){
      ProxyInfo prox;
      prox.autodetect = false;
      prox.address = SocketAddress(host, port);
      prox.type = PROXY_UNKNOWN;
      prox.username = username;
      talk_base::InsecureCryptStringImpl ins_pw;
      ins_pw.password() = password;
      std::string a = ins_pw.password();
      talk_base::CryptString pw(ins_pw);
      prox.password = pw;

      proxy_ = prox;
   }
   
   void set_firewall(talk_base::FirewallManager* firewall) {
      firewall_ = firewall;
   }

   // The DNS name of the host to connect to.
   const std::string& host() { return host_; }
   void set_host(const std::string& host) { host_ = host; }

   // The port to connect to on the target host.
   int port() { return port_; }
   void set_port(int port) { port_ = port; }

   // Whether the request should use SSL.
   bool secure() { return secure_; }
   void set_secure(bool secure) { secure_ = secure; }

   // Time to wait on the download, in ms.
   int timeout() { return timeout_; }
   void set_timeout(int timeout) { timeout_ = timeout; }

   // Fail redirects to allow analysis of redirect urls, etc.
   bool fail_redirect() const { return fail_redirect_; }
   void set_fail_redirect(bool redirect) { fail_redirect_ = redirect; }

   // Returns the redirect when redirection occurs
   const std::string& response_redirect() { return response_redirect_; }

   HttpRequestData& request() { return client_.request(); }
   HttpResponseData& response() { return client_.response(); }
   HttpErrorType error() { return error_; }

   bool is_connected();

protected:
   void set_error(HttpErrorType error) { error_ = error; }
   virtual void OnWorkStart();
   virtual void OnWorkStop();
   void OnComplete(HttpClient* client, HttpErrorType error);
   void OnConnect(HttpClient* client, HttpErrorType error);
   virtual void OnMessage(Message* message);
   virtual void DoWork();

private:
   std::string mUrl;
   talk_base::FifoBuffer *mFlvBuffer;
   bool available;
public:
   virtual int init(std::string url);
   virtual int doConnect();
   virtual int reConnect();
   virtual void stop();
   virtual bool is_open();
   virtual int write(void* buf, size_t count, ssize_t* pnwrite);
   virtual int writev(iovec* iov, int iovcnt, ssize_t* pnwrite);

private:
   void LaunchRequest();

   int start_delay_;
   ProxyInfo proxy_;
   talk_base::FirewallManager* firewall_;
   std::string host_;
   int port_;
   bool secure_;
   int timeout_;
   bool fail_redirect_;
   SslSocketFactory factory_;
   ReuseSocketPool pool_;
   HttpClient client_;
   HttpErrorType error_;
   std::string response_redirect_;
};

class SrsDataStream
{
private:
   // current position at bytes.
   char* p;
   // the bytes data for stream to read or write.
   char* bytes;
   // the total number of bytes.
   int nb_bytes;
public:
   SrsDataStream();
   virtual ~SrsDataStream();
public:
   /**
   * initialize the stream from bytes.
   * @b, the bytes to convert from/to basic types.
   * @nb, the size of bytes, total number of bytes for stream.
   * @remark, stream never free the bytes, user must free it.
   * @remark, return error when bytes NULL.
   * @remark, return error when size is not positive.
   */
   virtual int initialize(char* b, int nb);
   // get the status of stream

public:
   virtual int size();
   virtual bool require(int required_size);
   /**
   * write 1bytes char to stream.
   */
   virtual void write_1bytes(int8_t value);
   /**
   * write 2bytes int to stream.
   */
   virtual void write_2bytes(int16_t value);
   /**
   * write 4bytes int to stream.
   */
   virtual void write_4bytes(int32_t value);
   /**
   * write 3bytes int to stream.
   */
   virtual void write_3bytes(int32_t value);
};


#endif
