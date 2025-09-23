#ifndef EASYWSCLIENT_HPP_20120819_MIO
#define EASYWSCLIENT_HPP_20120819_MIO

#include <string>
#include <vector>

#ifdef _WIN32
#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS // _CRT_SECURE_NO_WARNINGS for sscanf errors in MSVC2013 Express
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment( lib, "ws2_32" )
#include <stddef.h> // for size_t
#include <stdio.h>  // for fprintf, snprintf
#include <string.h> // for memset, strlen
#include <errno.h>  // for errno
#include <io.h>     // for _close, _read, _write
#ifndef _SSIZE_T_DEFINED
typedef int ssize_t;
#define _SSIZE_T_DEFINED
#endif
#ifndef _SOCKET_T_DEFINED
typedef SOCKET socket_t;
#define _SOCKET_T_DEFINED
#endif
#ifndef snprintf
#define snprintf _snprintf_s
#endif
#if _MSC_VER >=1600 // vs2010 or later
#include <stdint.h>
#else
typedef __int8 int8_t;
typedef unsigned __int8 uint8_t;
typedef __int32 int32_t;
typedef unsigned __int32 uint32_t;
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;
#endif
#define socketerrno WSAGetLastError()
#define SOCKET_EAGAIN_EINPROGRESS WSAEINPROGRESS
#define SOCKET_EWOULDBLOCK WSAEWOULDBLOCK
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/tcp.h> // for TCP_NODELAY
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <stddef.h> // for size_t
#include <stdio.h>  // for fprintf, snprintf
#include <string.h> // for memset, strlen
#include <errno.h>  // for errno
#ifndef _SOCKET_T_DEFINED
typedef int socket_t;
#define _SOCKET_T_DEFINED
#endif
#ifndef INVALID_SOCKET
#define INVALID_SOCKET (-1)
#endif
#ifndef SOCKET_ERROR
#define SOCKET_ERROR (-1)
#endif
#define closesocket(s) ::close(s)
#include <stdint.h>
#define socketerrno errno
#define SOCKET_EAGAIN_EINPROGRESS EAGAIN
#define SOCKET_EWOULDBLOCK EWOULDBLOCK
#endif
#include <vector>
#include <string>
#include <algorithm> // for std::min

namespace easywsclient {

struct Callback_Imp {
    virtual void operator()(const std::string& message) = 0;
};

struct BytesCallback_Imp {
    virtual void operator()(const std::vector<uint8_t>& message) = 0;
};

struct WebSocket {
    typedef WebSocket * pointer;
    typedef enum {
        CLOSED,
        CLOSING,
        CONNECTING,
        OPEN
    } State;

    // Factories:
    static pointer from_url(const std::string& url, const std::string& origin = std::string());
    static pointer from_url_no_mask(const std::string& url, const std::string& origin = std::string());

    // Interfaces:
    virtual ~WebSocket() { }
    virtual void poll(int timeout = 0) = 0; // timeout in milliseconds
    virtual void send(const std::string& message) = 0;
    virtual void sendBinary(const std::string& message) = 0;
    virtual void sendBinary(const std::vector<uint8_t>& message) = 0;
    virtual void sendPing() = 0;
    virtual void close() = 0;
    virtual State getReadyState() const = 0;

    template<class C>
    void dispatch(C& callable) {
        struct _Callback : public Callback_Imp {
            C& callable;
            _Callback(C& callable) : callable(callable) { }
            void operator()(const std::string& message) {
                callable(message);
            }
        };
        _Callback callback(callable);
        _dispatch(callback);
    }

  protected:
    virtual void _dispatch(Callback_Imp& callable) = 0;
};

namespace { // private module-only namespace

inline socket_t hostname_connect(const std::string& hostname, int port) {
    struct addrinfo hints;
    struct addrinfo *result;
    struct addrinfo *p;
    int ret;
    socket_t sockfd = INVALID_SOCKET;
    char sport[16];

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    snprintf(sport, 16, "%d", port);

    if ((ret = getaddrinfo(hostname.c_str(), sport, &hints, &result)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
        return 1;
    }

    for(p = result; p != NULL; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == INVALID_SOCKET) {
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) != SOCKET_ERROR) {
            break;
        }

        closesocket(sockfd);
        sockfd = INVALID_SOCKET;
    }

    freeaddrinfo(result);

    return sockfd;
}

class _RealWebSocket : public easywsclient::WebSocket {
public:
    struct wsheader_type {
        unsigned header_size;
        bool fin;
        bool mask;
        enum opcode_type {
            CONTINUATION = 0x0,
            TEXT_FRAME = 0x1,
            BINARY_FRAME = 0x2,
            CLOSE = 8,
            PING = 9,
            PONG = 0xa,
        } opcode;
        int N0;
        uint64_t N;
        uint8_t masking_key[4];
    };

    std::vector<uint8_t> rxbuf;
    std::vector<uint8_t> txbuf;
    std::vector<uint8_t> receivedData;
    socket_t sockfd;
    State readyState;
    bool useMask;
    bool isRxBad;

    _RealWebSocket(socket_t sockfd, bool useMask)
        : sockfd(sockfd)
        , readyState(OPEN)
        , useMask(useMask)
        , isRxBad(false)
    {
    }
    ~_RealWebSocket() { if (sockfd != INVALID_SOCKET) closesocket(sockfd); }

    State getReadyState() const { return readyState; }

    void poll(int timeout) { // timeout in milliseconds
        if (readyState == CLOSED) {
            if (timeout > 0) {
#ifdef _WIN32
                Sleep(timeout);
#else
                timeval tv = { timeout/1000, (timeout%1000) * 1000 };
                select(0, NULL, NULL, NULL, &tv);
#endif
            }
            return;
        }

        if (timeout != 0) {
            fd_set rfds;
            fd_set wfds;
            timeval tv = { timeout/1000, (timeout%1000) * 1000 };

            FD_ZERO(&rfds);
            FD_ZERO(&wfds);
            FD_SET(sockfd, &rfds);
            if (txbuf.size()) {
                FD_SET(sockfd, &wfds);
            }

            select(sockfd + 1, &rfds, &wfds, 0, timeout > 0 ? &tv : 0);
        }

        while (true) { // FD_ISSET(0, &rfds) will be true
            int N = rxbuf.size();
            ssize_t ret;
            rxbuf.resize(N + 1500);
#ifdef _WIN32
            ret = ::recv(sockfd, (char*)&rxbuf[0] + N, 1500, 0);
#else
            ret = recv(sockfd, (char*)&rxbuf[0] + N, 1500, 0);
#endif
            if (false) {
            } else if (ret < 0 && (socketerrno == SOCKET_EWOULDBLOCK || socketerrno == SOCKET_EAGAIN_EINPROGRESS)) {
                rxbuf.resize(N);
                break;
            } else if (ret <= 0) {
                rxbuf.resize(N);
                closesocket(sockfd);
                sockfd = INVALID_SOCKET;
                readyState = CLOSED;
                fputs(ret < 0 ? "Connection error!\n" : "Connection closed!\n", stderr);
                break;
            } else {
                rxbuf.resize(N + ret);
            }
        }

        while (txbuf.size()) {
#ifdef _WIN32
            int ret = ::send(sockfd, (char*)&txbuf[0], txbuf.size(), 0);
#else
            int ret = ::send(sockfd, (char*)&txbuf[0], txbuf.size(), 0);
#endif
            if (false) {
            } // ??
            else if (ret < 0 && (socketerrno == SOCKET_EWOULDBLOCK || socketerrno == SOCKET_EAGAIN_EINPROGRESS)) {
                break;
            } else if (ret <= 0) {
                closesocket(sockfd);
                sockfd = INVALID_SOCKET;
                readyState = CLOSED;
                fputs(ret < 0 ? "Connection error!\n" : "Connection closed!\n", stderr);
                break;
            } else {
                txbuf.erase(txbuf.begin(), txbuf.begin() + ret);
            }
        }

        if (!txbuf.size() && readyState == CLOSING) {
            closesocket(sockfd);
            sockfd = INVALID_SOCKET;
            readyState = CLOSED;
        }
    }

    virtual void _dispatch(Callback_Imp & callable) {
        struct CallbackAdapter : public BytesCallback_Imp
        {
            Callback_Imp& callable;
            CallbackAdapter(Callback_Imp& callable) : callable(callable) { }
            void operator()(const std::vector<uint8_t>& message) {
                std::string stringMessage(message.begin(), message.end());
                callable(stringMessage);
            }
        };
        CallbackAdapter bytesCallback(callable);
        _dispatchBinary(bytesCallback);
    }

    void _dispatchBinary(BytesCallback_Imp & callable) {
        if (isRxBad) { return; }

        while (true) {
            wsheader_type ws;

            if (rxbuf.size() < 2) { return; }
            const uint8_t * data = (uint8_t *) &rxbuf[0];

            ws.fin = (data[0] & 0x80) == 0x80;
            ws.opcode = (wsheader_type::opcode_type) (data[0] & 0x0f);
            ws.mask = (data[1] & 0x80) == 0x80;
            ws.N0 = (data[1] & 0x7f);
            ws.header_size = 2 + (ws.N0 == 126? 2 : 0) + (ws.N0 == 127? 8 : 0) + (ws.mask? 4 : 0);

            if (rxbuf.size() < ws.header_size) { return; }

            int i = 0;
            if (ws.N0 < 126) {
                ws.N = ws.N0;
                i = 2;
            } else if (ws.N0 == 126) {
                ws.N = 0;
                ws.N |= ((uint64_t) data[2]) << 8;
                ws.N |= ((uint64_t) data[3]) << 0;
                i = 4;
            } else if (ws.N0 == 127) {
                ws.N = 0;
                ws.N |= ((uint64_t) data[2]) << 56;
                ws.N |= ((uint64_t) data[3]) << 48;
                ws.N |= ((uint64_t) data[4]) << 40;
                ws.N |= ((uint64_t) data[5]) << 32;
                ws.N |= ((uint64_t) data[6]) << 24;
                ws.N |= ((uint64_t) data[7]) << 16;
                ws.N |= ((uint64_t) data[8]) << 8;
                ws.N |= ((uint64_t) data[9]) << 0;
                i = 10;
                if (ws.N & 0x8000000000000000ull) {
                    isRxBad = true;
                    fprintf(stderr, "ERROR: Frame has invalid frame length. Closing.\n");
                    close();
                    return;
                }
            }

            if (ws.mask) {
                ws.masking_key[0] = ((uint8_t) data[i+0]) << 0;
                ws.masking_key[1] = ((uint8_t) data[i+1]) << 0;
                ws.masking_key[2] = ((uint8_t) data[i+2]) << 0;
                ws.masking_key[3] = ((uint8_t) data[i+3]) << 0;
            } else {
                ws.masking_key[0] = 0;
                ws.masking_key[1] = 0;
                ws.masking_key[2] = 0;
                ws.masking_key[3] = 0;
            }

            if (rxbuf.size() < ws.header_size+ws.N) { return; }

            if (ws.opcode == wsheader_type::TEXT_FRAME || ws.opcode == wsheader_type::BINARY_FRAME || ws.opcode == wsheader_type::CONTINUATION) {
                if (ws.mask) {
                    for (size_t i = 0; i != ws.N; ++i) {
                        rxbuf[i+ws.header_size] ^= ws.masking_key[i&0x3];
                    }
                }
                receivedData.insert(receivedData.end(), rxbuf.begin()+ws.header_size, rxbuf.begin()+ws.header_size+(size_t)ws.N);
                if (ws.fin) {
                    callable(receivedData);
                    receivedData.clear();
                    std::vector<uint8_t>().swap(receivedData);
                }
            } else if (ws.opcode == wsheader_type::PING) {
                if (ws.mask) {
                    for (size_t i = 0; i != ws.N; ++i) {
                        rxbuf[i+ws.header_size] ^= ws.masking_key[i&0x3];
                    }
                }
                std::string data(rxbuf.begin()+ws.header_size, rxbuf.begin()+ws.header_size+(size_t)ws.N);
                sendData(wsheader_type::PONG, data.size(), data.begin(), data.end());
            } else if (ws.opcode == wsheader_type::PONG) {
            } else if (ws.opcode == wsheader_type::CLOSE) {
                close();
            } else {
                fprintf(stderr, "ERROR: Got unexpected WebSocket message.\n");
                close();
            }

            rxbuf.erase(rxbuf.begin(), rxbuf.begin() + ws.header_size+(size_t)ws.N);
        }
    }

    void sendPing() {
        std::string empty;
        sendData(wsheader_type::PING, empty.size(), empty.begin(), empty.end());
    }

    void send(const std::string& message) {
        sendData(wsheader_type::TEXT_FRAME, message.size(), message.begin(), message.end());
    }

    void sendBinary(const std::string& message) {
        sendData(wsheader_type::BINARY_FRAME, message.size(), message.begin(), message.end());
    }

    void sendBinary(const std::vector<uint8_t>& message) {
        sendData(wsheader_type::BINARY_FRAME, message.size(), message.begin(), message.end());
    }

    template <typename Iterator>
    void sendData(wsheader_type::opcode_type type, uint64_t message_size, Iterator message_begin, Iterator message_end) {
        const uint8_t masking_key[4] = { 0x12, 0x34, 0x56, 0x78 };

        if (readyState == CLOSING || readyState == CLOSED) { return; }

        std::vector<uint8_t> header;
        header.assign(2 + (message_size >= 126 ? 2 : 0) + (message_size >= 65536 ? 6 : 0) + (useMask ? 4 : 0), 0);

        header[0] = 0x80 | type;

        if (message_size < 126) {
            header[1] = (message_size & 0xff) | (useMask ? 0x80 : 0);
            if (useMask) {
                header[2] = masking_key[0];
                header[3] = masking_key[1];
                header[4] = masking_key[2];
                header[5] = masking_key[3];
            }
        } else if (message_size < 65536) {
            header[1] = 126 | (useMask ? 0x80 : 0);
            header[2] = (message_size >> 8) & 0xff;
            header[3] = (message_size >> 0) & 0xff;
            if (useMask) {
                header[4] = masking_key[0];
                header[5] = masking_key[1];
                header[6] = masking_key[2];
                header[7] = masking_key[3];
            }
        } else {
            header[1] = 127 | (useMask ? 0x80 : 0);
            header[2] = (message_size >> 56) & 0xff;
            header[3] = (message_size >> 48) & 0xff;
            header[4] = (message_size >> 40) & 0xff;
            header[5] = (message_size >> 32) & 0xff;
            header[6] = (message_size >> 24) & 0xff;
            header[7] = (message_size >> 16) & 0xff;
            header[8] = (message_size >> 8) & 0xff;
            header[9] = (message_size >> 0) & 0xff;
            if (useMask) {
                header[10] = masking_key[0];
                header[11] = masking_key[1];
                header[12] = masking_key[2];
                header[13] = masking_key[3];
            }
        }

        txbuf.insert(txbuf.end(), header.begin(), header.end());
        txbuf.insert(txbuf.end(), message_begin, message_end);

        if (useMask) {
            size_t message_offset = txbuf.size() - message_size;
            for (size_t i = 0; i != message_size; ++i) {
                txbuf[message_offset + i] ^= masking_key[i&0x3];
            }
        }
    }

    void close() {
        if(readyState == CLOSING || readyState == CLOSED) { return; }
        readyState = CLOSING;
        uint8_t closeFrame[6] = {0x88, 0x80, 0x00, 0x00, 0x00, 0x00};
        std::vector<uint8_t> header(closeFrame, closeFrame+6);
        txbuf.insert(txbuf.end(), header.begin(), header.end());
    }
};

inline socket_t hostname_connect(const std::string& hostname, int port) {
    struct addrinfo hints;
    struct addrinfo *result;
    struct addrinfo *p;
    int ret;
    socket_t sockfd = INVALID_SOCKET;
    char sport[16];

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    snprintf(sport, 16, "%d", port);

    if ((ret = getaddrinfo(hostname.c_str(), sport, &hints, &result)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
        return 1;
    }

    for(p = result; p != NULL; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == INVALID_SOCKET) {
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) != SOCKET_ERROR) {
            break;
        }

        closesocket(sockfd);
        sockfd = INVALID_SOCKET;
    }

    freeaddrinfo(result);

    return sockfd;
}

inline WebSocket::pointer from_url(const std::string& url, bool useMask, const std::string& origin) {
    char host[512];
    int port;
    char path[512];

    if (url.size() >= 512) {
        fprintf(stderr, "ERROR: url size limit exceeded: %s\n", url.c_str());
        return NULL;
    }
    if (origin.size() >= 200) {
        fprintf(stderr, "ERROR: origin size limit exceeded: %s\n", origin.c_str());
        return NULL;
    }

    if (sscanf(url.c_str(), "ws://%[^:/]:%d/%s", host, &port, path) == 3) {
    } else if (sscanf(url.c_str(), "ws://%[^:/]/%s", host, path) == 2) {
        port = 80;
    } else if (sscanf(url.c_str(), "ws://%[^:/]:%d", host, &port) == 2) {
        path[0] = '\0';
    } else if (sscanf(url.c_str(), "ws://%[^:/]", host) == 1) {
        port = 80;
        path[0] = '\0';
    } else {
        fprintf(stderr, "ERROR: Could not parse WebSocket url: %s\n", url.c_str());
        return NULL;
    }

    socket_t sockfd = hostname_connect(host, port);
    if (sockfd == INVALID_SOCKET) {
        fprintf(stderr, "Unable to connect to %s:%d\n", host, port);
        return NULL;
    }

    {
        char line[1024];
        int status;
        int i;

        snprintf(line, 1024, "GET /%s HTTP/1.1\r\n", path);
        ::send(sockfd, line, strlen(line), 0);

        if (port == 80) {
            snprintf(line, 1024, "Host: %s\r\n", host);
            ::send(sockfd, line, strlen(line), 0);
        } else {
            snprintf(line, 1024, "Host: %s:%d\r\n", host, port);
            ::send(sockfd, line, strlen(line), 0);
        }

        snprintf(line, 1024, "Upgrade: websocket\r\n");
        ::send(sockfd, line, strlen(line), 0);

        snprintf(line, 1024, "Connection: Upgrade\r\n");
        ::send(sockfd, line, strlen(line), 0);

        if (!origin.empty()) {
            snprintf(line, 1024, "Origin: %s\r\n", origin.c_str());
            ::send(sockfd, line, strlen(line), 0);
        }

        snprintf(line, 1024, "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n");
        ::send(sockfd, line, strlen(line), 0);

        snprintf(line, 1024, "Sec-WebSocket-Version: 13\r\n");
        ::send(sockfd, line, strlen(line), 0);

        snprintf(line, 1024, "\r\n");
        ::send(sockfd, line, strlen(line), 0);

        for (i = 0; i < 2 || (i < 1023 && line[i-2] != '\r' && line[i-1] != '\n'); ++i) {
            if (recv(sockfd, line+i, 1, 0) == 0) { return NULL; }
        }
        line[i] = 0;

        if (i == 1023) {
            fprintf(stderr, "ERROR: Got invalid status line connecting to: %s\n", url.c_str());
            return NULL;
        }

        if (sscanf(line, "HTTP/1.1 %d", &status) != 1 || status != 101) {
            fprintf(stderr, "ERROR: Got bad status connecting to %s: %s", url.c_str(), line);
            return NULL;
        }

        while (true) {
            for (i = 0; i < 2 || (i < 1023 && line[i-2] != '\r' && line[i-1] != '\n'); ++i) {
                if (recv(sockfd, line+i, 1, 0) == 0) { return NULL; }
            }
            if (line[0] == '\r' && line[1] == '\n') { break; }
        }
    }

    int flag = 1;
    setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char*) &flag, sizeof(flag));

#ifdef _WIN32
    u_long on = 1;
    ioctlsocket(sockfd, FIONBIO, &on);
#else
    fcntl(sockfd, F_SETFL, O_NONBLOCK);
#endif

    return easywsclient::WebSocket::pointer(new _RealWebSocket(sockfd, useMask));
}

} // end of module-only namespace

inline WebSocket::pointer WebSocket::from_url(const std::string& url, const std::string& origin) {
    return from_url(url, true, origin);
}

inline WebSocket::pointer WebSocket::from_url_no_mask(const std::string& url, const std::string& origin) {
    return from_url(url, false, origin);
}

} // namespace easywsclient

#endif /* EASYWSCLIENT_HPP_20120819_MIO */