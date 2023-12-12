#pragma once
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <cstring>
#include <string>
#include "util.h"

using namespace std;
const uint32_t MAXLINE = 1500;

class UDPSocket {
   protected:
    //  tcp:   socket(AF_INET, SOCK_STREAM, 0))
    int sock = socket(AF_INET, SOCK_DGRAM, 0);  // 申请一个UDP的socket
    struct sockaddr_in addr;                    // 描述监听的地址
    void init_addr(const int port, const string& ip = "127.0.0.1") {
        memset(&addr, 0, sizeof(sockaddr_in));
        addr.sin_port = htons(port);
        addr.sin_family = AF_INET;  // 表示使用AF_INET地址族
        addr.sin_addr.s_addr = INADDR_ANY;
        inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);  // 监听
    }

   public:
    string receive(uint32_t length = MAXLINE) {
        string buf(MAXLINE, '\0');
        int n = ::recvfrom(sock, &buf[0], length, MSG_WAITALL,
                           (struct sockaddr*)&addr, (socklen_t*)&length);
        if (n == -1)
            return "";
        buf.resize(n);
        return buf;
    }

    int send(const string& buf, uint64_t length = MAXLINE) {
        length = (length == MAXLINE) ? buf.size() : length;
        return ::sendto(sock, &buf[0], length, MSG_CONFIRM,
                        (struct sockaddr*)&addr, sizeof(addr));
    }
    void close() { ::close(sock); }

    void set_timeout(int sec, int usec) {
        struct timeval timeout;
        timeout.tv_sec = sec;
        timeout.tv_usec = usec;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    }
};

class UDPServer : public UDPSocket {
   public:
    UDPServer(const int port) {
        init_addr(port);
        if (::bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            LOG_FATAL("bind failed");
            exit(EXIT_FAILURE);
        };
    }
};

class UDPClient : public UDPSocket {
   public:
    UDPClient(const string& ip, int port) { init_addr(port, ip); }
};