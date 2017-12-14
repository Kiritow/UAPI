#include <string>

class Socket
{
public:
    Socket();
    ~Socket();
    int connect(const std::string& IPStr,int Port);
    int send(const void* Buffer,int Length);
    int recv(void* Buffer,int MaxToRecv);
private:
    class _impl;
    _impl* _pp;
};

/// forward declaration.
class HTTPResponse;

class HTTPRequest
{
public:
    HTTPRequest();

    enum class Method { Post,Get };
    enum class Connection { KeepAlive,Close };

    Method method;
    Connection connection;

    std::string url;
    std::string host;
    std::string user_agent;

    /// This value will only be evaluated when using POST method.
    std::string content_type;
    std::string content;

    /// This function will try to connect 'host':80
    int send(HTTPResponse& res);
};

class HTTPResponse
{
public:
    std::string protocol;
    int status;
    std::string content_type;
    int content_length;

    std::string content;
};
