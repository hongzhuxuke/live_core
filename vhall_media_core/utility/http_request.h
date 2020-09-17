#ifndef HTTP_REQUEST_H__
#define HTTP_REQUEST_H__

#include <stdlib.h>
#include <string>
#include <stdio.h>
#include <live_sys.h>

// http://host[:port]/app[/appinstance][/...]
class HttpRequest
{
public:
   int SyncRequest(const std::string &url);
private:
   int ParseUrl(const std::string &url);
   int Connect(const struct addrinfo * addr);
   int Send();
   std::string GetHost(const std::string &url);
   /**
    *  host:域名
    *  port:端口号
    */
   struct addrinfo * GetAddrInfo(const std::string &host ,const int port);
   std::string GetAddrIp(struct addrinfo * addr);
private:
   std::string mHost;
   std::string mIp;
   int         mPort;
   std::string mUri;
   int         mSocketId;
};
#endif
