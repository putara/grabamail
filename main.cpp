#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strsafe.h>

#pragma comment(lib, "ws2_32.lib")

void error(const char* what)
{
    int error = WSAGetLastError();
    char buffer[1024];
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                            NULL, error, 0, buffer, _countof(buffer), NULL);
    fprintf(stderr, "%s() failed with error %d\n%s\n", what, error, buffer);
}

class WSAInitialiser
{
private:
    int result;
    WSADATA wsaData;

public:
    WSAInitialiser(WORD version)
    {
        ZeroMemory(&this->wsaData, sizeof(this->wsaData));
        this->result = ::WSAStartup(version, &this->wsaData);
    }
    ~WSAInitialiser()
    {
        if (this->result != SOCKET_ERROR) {
            ::WSACleanup();
        }
    }
    operator int() const
    {
        return this->result;
    }
};

class Socket
{
private:
    SOCKET  socket;

public:
    Socket()
        : socket(INVALID_SOCKET)
    {
    }
    ~Socket()
    {
        this->Close();
    }
    bool IsValid() const
    {
        return this->socket != INVALID_SOCKET;
    }
    void Close()
    {
        if (this->IsValid()) {
            ::closesocket(this->socket);
            this->socket = INVALID_SOCKET;
        }
    }
    bool Open(int af, int type, int protocol)
    {
        if (this->IsValid()) {
            return false;
        }
        this->socket = ::socket(af, type, protocol);
        return this->IsValid();
    }
    bool Accept(const Socket& src, __out_bcount_opt(*addrlen) struct sockaddr* addr, __inout_opt int* addrlen)
    {
        if (this->IsValid()) {
            return false;
        }
        this->socket = ::accept(src.socket, addr, addrlen);
        return this->IsValid();
    }
    bool Accept(const Socket& src)
    {
        return this->Accept(src, NULL, NULL);
    }
    bool Bind(__in_bcount(addrlen) const struct sockaddr* addr, int addrlen)
    {
        return ::bind(this->socket, addr, addrlen) != SOCKET_ERROR;
    }
    bool Bind(__in_bcount(addrlen) const struct sockaddr_in& addr)
    {
        return this->Bind(reinterpret_cast<const struct sockaddr*>(&addr), sizeof(struct sockaddr_in));
    }
    bool Listen(int backlog)
    {
        return ::listen(this->socket, backlog) != SOCKET_ERROR;
    }
    bool SetOpt(int level, int optname, __in_bcount_opt(optlen) const void* optval, int optlen)
    {
        return ::setsockopt(this->socket, level, optname, static_cast<const char*>(optval), optlen) != SOCKET_ERROR;
    }
    bool Shutdown(int how)
    {
        return ::shutdown(this->socket, how) != SOCKET_ERROR;
    }
    int Send(__in_bcount(len) const void* buf, int len, int flags = 0)
    {
        return ::send(this->socket, static_cast<const char*>(buf), len, flags);
    }
    int Send(__in_z const char* buf)
    {
        return this->Send(buf, strlen(buf), 0);
    }
    int Recv(__out_bcount_part(len, return) __out_data_source(NETWORK) void* buf, int len, int flags = 0)
    {
        return ::recv(this->socket, static_cast<char*>(buf), len, flags);
    }
};

class ThreadData
{
public:
    Socket& lisocket;
    Socket msgsock;
    volatile BOOL quit;

    ThreadData(Socket& s)
        : lisocket(s)
        , quit(false)
    {
    }
    ~ThreadData()
    {
    }

private:
    ThreadData(const ThreadData&);
    ThreadData& operator =(const ThreadData&);
};

#define CMDEQ(x, y) (strncmp(x, y, 4) == 0)

int Echo(Socket& msgsock, const char* cmd)
{
    char buffer[513];
    printf("<%s\n", cmd);
    StringCchCopyA(buffer, _countof(buffer), cmd);
    StringCchCatA(buffer, _countof(buffer), "\r\n");
    return msgsock.Send(buffer);
}

DWORD WINAPI ThreadFunc(void* p)
{
    ThreadData* data = static_cast<ThreadData*>(p);
    Socket& msgsock = data->msgsock;

    if (msgsock.Accept(data->lisocket) == false) {
        error("accept");
        return 1;
    }

    const char* welcome = "220 127.0.0.1 SMTP\r\n";
    if (msgsock.Send(welcome) == SOCKET_ERROR) {
        error("send");
        return 1;
    }

    int zero = 0;
    if (msgsock.SetOpt(SOL_SOCKET, SO_SNDBUF, &zero, sizeof(zero)) == false) {
        error("setsockopt");
        return 1;
    }

    bool inDataLoop = false;
    while (data->quit == false) {
        printf("receiving...\n");
        char buffer[513];
        int retval = msgsock.Recv(buffer, sizeof(buffer));
        if (retval == SOCKET_ERROR) {
            error("recv");
            break;
        }
        if (retval == 0) {
            printf("Client closed connection\n");
            continue;
        }
        buffer[retval] = 0;
        printf("> %s\n", buffer);
        if (inDataLoop == false) {
            if (CMDEQ(buffer, "EHLO") || CMDEQ(buffer, "HELO")) {
                retval = Echo(msgsock, "250 Ok");
            } else if (CMDEQ(buffer, "QUIT")) {
                retval = Echo(msgsock, "221 Bye");
                data->quit = true;
            } else if (CMDEQ(buffer, "MAIL")) {
                retval = Echo(msgsock, "250 Ok");
            } else if (CMDEQ(buffer, "RCPT")) {
                retval = Echo(msgsock, "250 Ok");
            } else if (CMDEQ(buffer, "NOOP")) {
                retval = Echo(msgsock, "250 Ok");
            } else if (CMDEQ(buffer, "DATA")) {
                retval = Echo(msgsock, "354 End data with <CRLF>.<CRLF>");
                inDataLoop = true;
            } else {
                retval = Echo(msgsock, "502 Command not implemented");
            }
        } else {
            if (retval >= 5 && strcmp(buffer + retval - 5, "\r\n.\r\n") == 0) {
                retval = Echo(msgsock, "250 Ok");
                inDataLoop = false;
            }
        }
        if (retval <= 0) {
            error("send");
            break;
        }
    }
    msgsock.Shutdown(SD_SEND);
    return 0;
}

int main()
{
    unsigned short port = 25;

    WSAInitialiser init(MAKEWORD(2, 2));
    if (init == SOCKET_ERROR) {
        error("WSAStartup");
        return EXIT_FAILURE;
    }

    Socket socket;
    if (socket.Open(AF_INET, SOCK_STREAM, IPPROTO_TCP) == false){
        error("socket");
        return EXIT_FAILURE;
    }

    struct sockaddr_in local;
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = INADDR_ANY;
    local.sin_port = htons(port);
    if (socket.Bind(local) == false) {
        error("bind");
        return -1;
    }

    if (socket.Listen(1) == false) {
        error("listen");
        return -1;
    }

    printf("Listening on port %u\n", port);
    ThreadData data(socket);
    HANDLE thread = CreateThread(NULL, 0, ThreadFunc, &data, 0, NULL);
    if (thread == NULL) {
        printf("CreateThread() failed\n");
        return -1;
    }

    printf("Press enter key to exit\n");
    char buffer[100];
    StringCchGetsA(buffer, _countof(buffer));

    printf("Terminating connection\n");
    data.quit = true;
    LINGER linger;
    linger.l_onoff = 1;
    linger.l_linger = 0;
    socket.SetOpt(SOL_SOCKET, SO_LINGER, &linger, sizeof(linger));
    socket.Close();
    data.msgsock.Close();
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
    return 0;
}
