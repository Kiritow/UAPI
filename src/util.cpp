#include "util.h"

/// The following code is part of General Socket Wrapper (GSock) Project
/// Under MIT License
#define _WIN32_WINNT 0x0603
#include <stdexcept>
#include <cstring>
#include <string>
#include <sstream>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#ifdef DEBUG
/// Debug Output API
#define dprintf(fmt,args...) printf(fmt,##args)
#else
/// Debug Output API
#define dprintf(fmt,args...)
#endif

class _init_winsock2_2_class
{
public:
    _init_winsock2_2_class()
    {
        /// Windows Platform need WinSock2.DLL initialization.
        WORD wd;
        WSAData wdt;
        wd=MAKEWORD(2,2);
        if(WSAStartup(wd,&wdt)<0)
        {
            throw std::runtime_error("Unable to load winsock2.dll. ");
        }
    }
    ~_init_winsock2_2_class()
    {
        /// Windows Platform need WinSock2.DLL clean up.
        WSACleanup();
    }
};

static _init_winsock2_2_class _init_winsock2_2_obj;

class Socket::_impl
{
public:
    int sfd;
    sockaddr_in saddr;
    bool created;
};

Socket::Socket() : _pp(new _impl)
{
    _pp->created=false;
}

Socket::~Socket()
{
    if(_pp)
    {
        if(_pp->created)
        {
            closesocket(_pp->sfd);
        }

        delete _pp;
    }
}

int Socket::connect(const std::string& IPStr,int Port)
{
    if(_pp->created)
    {
        return -2;
    }
    _pp->sfd=socket(AF_INET,SOCK_STREAM,0);
    if(_pp->sfd<0)
    {
        return -3;
    }
    _pp->created=true;

    // refs
    int& sfd=_pp->sfd;
    sockaddr_in& saddr=_pp->saddr;

    memset(&saddr,0,sizeof(saddr));
    saddr.sin_addr.s_addr=inet_addr(IPStr.c_str());
    saddr.sin_port=htons(Port);
    saddr.sin_family=AF_INET;

    return ::connect(sfd,(sockaddr*)&saddr,sizeof(saddr));
}

int Socket::send(const void* Buffer,int Length)
{
    return ::send(_pp->sfd,(const char*)Buffer,Length,0);
}

int Socket::recv(void* Buffer,int MaxToRecv)
{
    return ::recv(_pp->sfd,(char*)Buffer,MaxToRecv,0);
}

int DNSResolve(const std::string& HostName, std::string& _out_IPStr)
{
    /// Use getaddrinfo instead
    struct addrinfo hints;
    memset(&hints,0,sizeof(hints));
    hints.ai_family=AF_UNSPEC;
    hints.ai_socktype=SOCK_STREAM;
    hints.ai_protocol=IPPROTO_TCP;

    struct addrinfo* result=nullptr;

    int ret=getaddrinfo(HostName.c_str(),NULL,&hints,&result);
    if(ret!=0)
    {
        return -1;/// API Call Failed.
    }
    for(struct addrinfo* ptr=result; ptr!=nullptr; ptr=ptr->ai_next)
    {
        switch(ptr->ai_family)
        {
        case AF_INET:
            sockaddr_in* addr=(struct sockaddr_in*) (ptr->ai_addr) ;
            _out_IPStr=inet_ntoa(addr->sin_addr);
            return 0;
            break;
        }
    }
    /// Unknown error.
    return -2;
}

HTTPRequest::HTTPRequest()
{
    method=Method::Get;
    connection=Connection::Close;
    content_type="application/x-www-form-urlencoded";
}

int HTTPRequest::send(HTTPResponse& _out_res)
{
    if(host.empty())
    {
        /// Host is empty
        return -1;
    }
    if(url.empty())
    {
        url="/";
    }

    switch(method)
    {
    case Method::Get:
    case Method::Post:
        break;
    default:
        method=Method::Get;
        break;
    }

    switch(connection)
    {
    case Connection::KeepAlive:
    case Connection::Close:
        break;
    default:
        connection=Connection::Close;
        break;
    }

    std::string ip;
    int ret=DNSResolve(host,ip);
    if(ret!=0)
    {
        /// DNS resolve error.
        return -2;
    }

    std::string result_str;

    std::ostringstream ostr;
    if(method==Method::Post)
    {
        ostr<<"POST ";
    }
    else
    {
        ostr<<"GET ";
    }
    ostr<<url<<" HTTP/1.1\r\n";
    ostr<<"Host: "<<host<<"\r\n";
    if(!user_agent.empty())
    {
        ostr<<"User-Agent: "<<user_agent<<"\r\n";
    }
    ostr<<"Connection: ";
    if(connection==Connection::KeepAlive)
    {
        ostr<<"Keep-Alive";
    }
    else
    {
        ostr<<"Close";
    }
    ostr<<"\r\n";
    if(method==Method::Post)
    {
        int sz=content.size();
        ostr<<"Content-Type: "<<content_type<<"\r\n";
        ostr<<"Content-Length: "<<sz<<"\r\n";
        ostr<<"\r\n";

        result_str=ostr.str();
        result_str.append(content);
    }
    else
    {
        ostr<<"\r\n";
        result_str=ostr.str();
    }

    dprintf("RequestHeader:\n%s\n",result_str.c_str());

    Socket s;
    ret=s.connect(ip,80);
    if(ret!=0)
    {
        /// Socket connect error.
        return -3;
    }

    dprintf("Connected to server: %s\n",ip.c_str());

    int done=0;
    int total=result_str.size();
    while(done<total)
    {
        int ret=s.send(result_str.c_str()+done,total-done);

        dprintf("SendLoop: done=%d total=%d ret=%d\n",done,total,ret);

        if(ret<0)
        {
            /// Error while sending request headers.
            return -4;
        }
        else if(ret==0)
        {
            /// Connection is closed while sending request headers.
            return -5;
        }

        done+=ret;
    }

    /// Now we are waiting for response.
    char buff[1024];

    std::string response_str;
    int response_content_length=-1;

    std::string target_str="Content-Length: ";

    while(true)
    {
        memset(buff,0,1024);
        int ret=s.recv(buff,1024);

        dprintf("ReceiveLoop 1: ret=%d\n",ret);

        if(ret<0)
        {
            /// Error while receiving response.
            return -6;
        }
        else
        {
            if(ret>0)
            {
                response_str.append(std::string(buff,ret));
            }

            auto idx=response_str.find(target_str);
            if(idx!=std::string::npos)
            {
                /// Found Content-Length
                int tmp;
                if(sscanf(response_str.c_str()+idx+target_str.size(),"%d",&tmp)!=1)
                {
                    /// Failed to parse content-length.
                    /// Maybe it will success in next loop.
                    if(ret==0)
                    {
                        /// There is no next loop
                        /// Cannot find Content-Length.
                        return -7;
                    }
                }
                else
                {
                    /// Parsed!
                    response_content_length=tmp;
                    break;
                }
            }
        }
    }

    /// Try to find header-content separator.
    int last_flag=0;
    while(true)
    {
        auto idx=response_str.find("\r\n\r\n");
        dprintf("ReceiveCheck 2: idx=%d\n",(int)idx);
        if(idx==std::string::npos)
        {
            /// separator not found. Might not received.
            if(last_flag==1)
            {
                /// No next loop.
                /// Connection closed before separator received.
                return -8;
            }
            memset(buff,0,1024);
            int ret=s.recv(buff,1024);
            dprintf("ReceiveLoop 2: ret=%d\n",ret);
            if(ret<0)
            {
                /// Error while receiving response separator.
                return -9;
            }
            else if(ret==0)
            {
                last_flag=1;
            }
            else
            {
                response_str.append(std::string(buff,ret));
            }
        }
        else break;
    }

    int sidx=response_str.find("\r\n\r\n")+4;

    int received_content_length=response_str.size()-sidx;

    while(received_content_length<response_content_length)
    {
        memset(buff,0,1024);
        int ret=s.recv(buff,std::min(1024,response_content_length-received_content_length));
        if(ret<0)
        {
            /// Socket error while receiving content.
            return -10;
        }
        else if(ret==0)
        {
            /// Connection closed before content receiving finished.
            return -11;
        }
        else
        {
            response_str.append(std::string(buff,ret));
            received_content_length+=ret;
        }
    }

    HTTPResponse res;
    res.content_length=response_content_length;
    res.content=response_str.substr(sidx);

    std::string response_header=response_str.substr(0,sidx-4);

    auto kidx=std::string::npos;
    if((kidx=response_header.find("HTTP/1.1"))!=std::string::npos)
    {
        res.protocol="HTTP/1.1";
        if(sscanf(response_header.c_str()+kidx+strlen("HTTP/1.1")+1,
                  "%d",
                  &(res.status))<0)
        {
            /// Status Parse Failed. (HTTP 1.1)
            return -12;
        }
    }
    else if((kidx=response_header.find("HTTP/1.0"))!=std::string::npos)
    {
        res.protocol="HTTP/1.0";
        if(sscanf(response_header.c_str()+kidx+strlen("HTTP/1.0")+1,
                  "%d",
                  &(res.status))<0)
        {
            /// Status Parse Failed. (HTTP 1.0)
            return -13;
        }
    }
    else
    {
        /// We do not support HTTP 0.9
        /// Unable to parse http protocol version.
        return -14;
    }

    if((kidx=response_header.find("Content-Type:"))!=std::string::npos)
    {
        res.content_type=response_header.substr(kidx+strlen("Content-Type:"),
                                                response_header.find("\r\n",kidx)-kidx-strlen("Content-Type:"));
    }
    else
    {
        /// Failed to parse content type.
        return -15;
    }

    _out_res=res;

    /// Succeed.
    return 0;
}
