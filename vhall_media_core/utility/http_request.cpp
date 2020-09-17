#include "http_request.h"
#include <live_sys.h>
#include <string.h>
#include <stdlib.h>

int HttpRequest::SyncRequest(const std::string &url)
{
   if (ParseUrl(url) != 0)
      return -1;
   
   struct addrinfo * addr = GetAddrInfo(mHost, mPort);
   if (addr == NULL) {
      return -1;
   }
   
   if (Connect(addr) != 0){
      freeaddrinfo(addr);
      return -1;
   }
   
   freeaddrinfo(addr);
   
   return Send();
}

int HttpRequest::ParseUrl(const std::string &url){
   
   int i = 0, j = 0, k = 0;
   std::string tdomain, turi, tport ,streamName;
   if (url.compare( 0, 6, "http://", 0, 6) == 0)
      for (i = 7; i < url.length(); i++) {
         if (url.at(i) == ':') {
            k = 1;
            j = 0;
            continue;
         }
         if (url.at(i) == '/') {
            k = 2;
            j = 0;
         }
         // read domain
         if (k == 0)
            tdomain += url.at(i);
         // read port
         if (k == 1)
            tport += url.at(i);
         // read uri
         if (k == 2)
            turi += url.at(i);
         
         j++;
   }
   
   this->mHost = tdomain;
   
   this->mUri = turi;
   
   this->mPort = 80;
   if (tport.length() > 0)
      this->mPort = atoi(tport.c_str());
   
   return 0;
}

std::string HttpRequest::GetHost(const std::string &url){
   
   if (ParseUrl(url)==0) {
      return this->mHost;
   }
   return "";
}

int HttpRequest::Connect(const struct addrinfo * addr)
{
   mSocketId = -1;
   while(addr!=NULL)
   {
      //建立socket
      mSocketId =socket(addr->ai_family,addr->ai_socktype,addr->ai_protocol);
      if(mSocketId<0)
         continue;
      
      struct timeval timeout = {5, 0};
      setsockopt(mSocketId,SOL_SOCKET, SO_SNDTIMEO, &timeout,sizeof(timeout));
      if(connect(mSocketId,addr->ai_addr,addr->ai_addrlen)==0)
      {
         //链接成功，就中断循环
         break;
      }
      //没有链接成功，就继续尝试下一个
      closesocket(mSocketId);
      mSocketId=-1;
      addr=addr->ai_next;
   }
   if (mSocketId==-1) {
      return -1;
   }
   return 0;
}

int HttpRequest::Send()
{
   char sbuf[4096];
   char rbuf[4096];
   
   memset(sbuf, 0, sizeof(sbuf));
   memset(rbuf, 0, sizeof(rbuf));
   sprintf(sbuf,"GET %s HTTP/1.1\r\nHost: %s\r\n\r\n",
           this->mUri.c_str(), this->mHost.c_str());
   if (send(mSocketId, sbuf, strlen(sbuf), 0) == -1) {
      return -1;
   };
   int ret = (int)recv(mSocketId, rbuf, sizeof(rbuf), 0);
   closesocket(mSocketId);
   if (ret == -1)
      return -1;
   
   char *p = NULL;
   p = strtok(rbuf, " ");
   p = strtok(NULL, " ");
   if (p==NULL) {
      return -1;
   }
   return atoi(p);
}

struct addrinfo* HttpRequest::GetAddrInfo(const std::string &host, const int port){
   struct addrinfo *answer, hint;
   //llc change bzero to memset
   memset(&hint, 0, sizeof(hint));
   hint.ai_family = AF_UNSPEC;
   hint.ai_socktype = SOCK_STREAM;
   char portstr[16]={0};
   snprintf(portstr, sizeof(portstr),"%d",port);
   int ret = ::getaddrinfo(host.c_str(), portstr,&hint, &answer);

   if (ret != 0) {
      return NULL;
   }
   return answer;
}

std::string HttpRequest::GetAddrIp(struct addrinfo *addr)
{
   char ip[128];
   for (const struct addrinfo * curr = addr; curr != NULL; curr = curr->ai_next) {
      switch (curr->ai_family){
         case AF_UNSPEC:
            //do something here
            break;
         case AF_INET:
            inet_ntop(AF_INET, &((struct sockaddr_in *)curr->ai_addr)->sin_addr, ip,sizeof(ip));
            return std::string(ip);
         case AF_INET6:
            inet_ntop(AF_INET6, &((struct sockaddr_in6 *)curr->ai_addr)->sin6_addr, ip,sizeof(ip));
            return std::string(ip);
      }
   }
   return "";
}
