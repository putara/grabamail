#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strsafe.h>

#pragma comment(lib, "wsock32.lib")
#pragma comment(lib, "ws2_32.lib")

void error(const char* what)
{
    int error = WSAGetLastError();
    char buffer[1024];
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                            NULL, error, 0, buffer, _countof(buffer), NULL);
    fprintf(stderr, "%s() failed with error %d\n%s\n", what, error, buffer);
}

void errorw(const char* what)
{
    int error = GetLastError();
    char buffer[1024];
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                            NULL, error, 0, buffer, _countof(buffer), NULL);
    fprintf(stderr, "%s() failed with error %d\n%s\n", what, error, buffer);
}

inline void* operator new(size_t cb) throw()
{
    return ::HeapAlloc(::GetProcessHeap(), 0, cb);
}

inline void operator delete(void* p) throw()
{
    ::HeapFree(::GetProcessHeap(), 0, p);
}

inline void* operator new[](size_t cb) throw()
{
    return ::operator new(cb);
}

inline void operator delete[](void* p) throw()
{
    ::operator delete(p);
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

/*
class SocketListener
{
private:
    enum TYPE
    {
        OVL_LISTEN = 1,
        OVL_CLIENT = 2,
    };
    struct SOCKADDR_STORAGE_IPv4
    {
        char buffer[sizeof(SOCKADDR_IN) + 16];
    };
    struct MYOVERLAPPED
    {
        OVERLAPPED overlapped;
        TYPE type;

        MYOVERLAPPED()
        {
            ZeroMemory(this, sizeof(*this));
        }
    };
    SOCKET  listenSocket;
    SOCKET  clientSocket;
    TP_CALLBACK_ENVIRON tpcbenv;
    PTP_CLEANUP_GROUP tpclean;
    PTP_IO volatile tpIo;
    DWORD idThread;
    SOCKADDR_STORAGE_IPv4 acceptBuffer[2];

public:
    SocketListener()
        : listenSocket(INVALID_SOCKET)
        , clientSocket(INVALID_SOCKET)
        , tpIo()
        , idThread()
    {
        ::InitializeThreadpoolEnvironment(&this->tpcbenv);
    }

    ~SocketListener()
    {
        this->StopAccepting(false);
        ::DestroyThreadpoolEnvironment(&this->tpcbenv);
    }

    static HRESULT ResultFromWSALastError()
    {
        return HRESULT_FROM_WIN32(::WSAGetLastError());
    }

    static HRESULT ResultFromWin32LastError()
    {
        return HRESULT_FROM_WIN32(::GetLastError());
    }

    HRESULT Bind(UINT16 port)
    {
        this->listenSocket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (this->listenSocket == INVALID_SOCKET) {
            error("socket");
            return ResultFromWSALastError();
        }
        SOCKADDR_IN local;
        local.sin_family = AF_INET;
        local.sin_addr.s_addr = INADDR_ANY;
        local.sin_port = htons(port);
        if (::bind(this->listenSocket, reinterpret_cast<const struct sockaddr*>(&local), sizeof(local)) == SOCKET_ERROR) {
            error("bind");
            return ResultFromWSALastError();
        }
        return S_OK;
    }

    HRESULT EnableAccepting()
    {
        this->tpclean = ::CreateThreadpoolCleanupGroup();
        ::SetThreadpoolCallbackCleanupGroup(&this->tpcbenv, this->tpclean, NULL);
        this->tpIo = ::CreateThreadpoolIo(reinterpret_cast<HANDLE>(this->listenSocket), sAcceptThreadProc, this, NULL);
        if (this->tpIo == NULL) {
            printf("CreateThreadpoolIo() failed\n");
            return ResultFromWin32LastError();
        }
        ::StartThreadpoolIo(tpIo);

        int backlog = 2;
        if (::listen(this->listenSocket, backlog) == SOCKET_ERROR) {
            error("listen");
            return ResultFromWSALastError();
        }

        printf("Listening...\n");
        return this->BeginAccept(false);
    }

    HRESULT BeginAccept(bool thread)
    {
        this->clientSocket = ::WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
        if (this->clientSocket == INVALID_SOCKET) {
            error("socket");
            return ResultFromWSALastError();
        }
        PTP_IO tpIo = this->tpIo;
        DWORD cbReceived = 0;
        MYOVERLAPPED* ovl = new MYOVERLAPPED;
        ovl->type = OVL_LISTEN;
        if (::AcceptEx(this->listenSocket, this->clientSocket, &this->acceptBuffer, 0, sizeof(this->acceptBuffer[0]), sizeof(this->acceptBuffer[1]), &cbReceived, &ovl->overlapped) == FALSE) {
            HRESULT hr = ResultFromWSALastError();
            if (hr != __HRESULT_FROM_WIN32(ERROR_IO_PENDING)) {
                // Failed to accept, so cleanup
                ::CancelThreadpoolIo(tpIo);
                this->StopAccepting(thread);
                delete ovl;
                return hr;
            }
        }

        ovl = new MYOVERLAPPED;
        ovl->type = OVL_CLIENT;
        PTP_IO ptp = ::CreateThreadpoolIo(reinterpret_cast<HANDLE>(this->clientSocket), sAcceptThreadProc, this, NULL);
        ::StartThreadpoolIo(ptp);
        return S_OK;
    }

    template <typename T>
    static T MyInterlockedExchangePointer(__deref_inout T volatile* pointer, T value)
    {
        C_ASSERT(sizeof(T) == sizeof(void*));
        return reinterpret_cast<T>(InterlockedExchangePointer(reinterpret_cast<void* volatile*>(pointer), reinterpret_cast<void*>(value)));
    }

    void StopAccepting(bool thread)
    {
        SOCKET listenSocket = MyInterlockedExchangePointer(&this->listenSocket, INVALID_SOCKET);
        if (listenSocket != INVALID_SOCKET) {
            ::closesocket(listenSocket);
            ::CancelIoEx(reinterpret_cast<HANDLE>(listenSocket), NULL);
        }

        PTP_IO tpIo = MyInterlockedExchangePointer(&this->tpIo, reinterpret_cast<PTP_IO>(NULL));
        if (tpIo != NULL) {
            // Don't wait for threadpool callbacks when this thread is actually the threadpool callback
            if (thread == false) {
                ::WaitForThreadpoolIoCallbacks(tpIo, false);
            }
            ::CloseThreadpoolIo(tpIo);
        }

        if (this->clientSocket != INVALID_SOCKET) {
            ::closesocket(this->clientSocket);
            this->clientSocket = INVALID_SOCKET;
        }

        ::CloseThreadpoolCleanupGroupMembers(this->tpclean, false, NULL);
        ::CloseThreadpoolCleanupGroup(this->tpclean);
    }

    static void CALLBACK sAcceptThreadProc(
        __inout     PTP_CALLBACK_INSTANCE Instance,
        __inout_opt PVOID                 Context,
        __inout_opt PVOID                 Overlapped,
                    ULONG                 IoResult,
                    ULONG_PTR             NumberOfBytesTransferred,
        __inout     PTP_IO                Io)
    {
        SocketListener* self = static_cast<SocketListener*>(Context);
        self->idThread = ::GetCurrentThreadId();
        self->AcceptThreadProc(Overlapped, IoResult, NumberOfBytesTransferred);
        self->idThread = 0;
    }

    static void Dump(const void* data, size_t cb)
    {
        const BYTE* p = static_cast<const BYTE*>(data);
        UINT i = 0;
        while (cb--) {
            printf("%02X ", *p++);
            if (++i == 16) {
                printf("\n");
                i = 0;
            }
        }
        if (i != 0) {
            printf("\n");
        }
    }

    void AcceptThreadProc(PVOID Overlapped, ULONG result, ULONG_PTR cbReceived)
    {
        MYOVERLAPPED* ovl = CONTAINING_RECORD(Overlapped, MYOVERLAPPED, overlapped);
        if (result != ERROR_OPERATION_ABORTED) {
            printf("%u %p %u\n", result, cbReceived, ovl->type);
            //Dump(this->acceptBuffer, sizeof(this->acceptBuffer));
            char buffer[48];
            ZeroMemory(buffer, sizeof(buffer));
            WSABUF bufd;
            bufd.buf = buffer;
            bufd.len = sizeof(buffer);
            DWORD cb = 0, flags = MSG_PARTIAL;
            WSAOVERLAPPED wsaovl = {};
            int ret = WSARecv(this->clientSocket, &bufd, 1, &cb, &flags, &wsaovl, NULL);
            printf("%d %u\n", ret, cb);
            Dump(buffer, cb);
        } else {
            printf("%u %p\n", result, cbReceived);
        }
        ::closesocket(this->clientSocket);
        this->clientSocket = NULL;
        if (result == ERROR_OPERATION_ABORTED) {
            return;
        }
        // Start up another accept request
        this->BeginAccept(true);
    }
};
*/

/*
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

template <class T>
class RefObject
{
private:
    long volatile cRef;
    static const long INVALID_COUNT = INT_MAX / 2;

public:
    RefObject() throw()
        : cRef(1)
    {
    }
    ~RefObject() throw()
    {
        if (this->cRef != INVALID_COUNT) {
            fprintf(stderr, "refcount destroyed: %p\n", this);
        }
    }
    ULONG AddRef() throw()
    {
        return static_cast<ULONG>(::_InterlockedIncrement(&this->cRef));
    }
    ULONG Release() throw()
    {
        long ret = ::_InterlockedDecrement(&this->cRef);
        if (ret == 0) {
            this->cRef = INVALID_COUNT;
            delete static_cast<T*>(this);
        }
        return static_cast<ULONG>(ret);
    }
};


class Server : public RefObject<Server>
{
private:
    Socket& listenSocket;
    Socket mailSocket;

    Server(Socket& s) throw()
        : listenSocket(s)
    {
    }

    ~Server() throw()
    {
    }

    static DWORD WINAPI sThreadFunc(void* p)
    {
        Server* self = static_cast<Server*>(p);
        DWORD ret = self->ThreadFunc();
        self->Release();
        return ret;
    }

    int Echo(const char* cmd)
    {
        char buffer[513];
        printf("<%s\n", cmd);
        if (FAILED(StringCchCopyA(buffer, _countof(buffer), cmd))
            || FAILED(StringCchCatA(buffer, _countof(buffer), "\r\n"))) {
            fprintf(stderr, "buffer overrun!!\n");
            return SOCKET_ERROR;
        }
        return msgsock.Send(buffer);
    }

    bool StartThread()
    {
    }

    HANDLE InitThreads()
    {
        HANDLE hCompletionPort = ::CreateIoCompletionPort(NULL, NULL, 0, 0);
        SYSTEM_INFO systemInfo;
        ::GetSystemInfo(&systemInfo);

        for (UINT i = 0; i < systemInfo.dwNumberOfProcessors * 2; i++) {
            HANDLE hThreadHandle = ::CreateThread(NULL, 0, WorkerThread, hCompletionPort, 0, NULL);
            if (hThreadHandle == NULL) {
                ::CloseHandle(hCompletionPort);
                return NULL;
            }

            ::CloseHandle(hThreadHandle);
        }

        return hCompletionPort;
    }

    static DWORD WINAPI WorkerThread(void* p)
    {
        HANDLE hCompletionPort = static_cast<HANDLE>(p);
        BOOL bSuccess;
        DWORD dwIoSize;
        LPOVERLAPPED lpOverlapped;
        PCLIENT_CONTEXT lpClientContext;
        for (;;) {
            bSuccess = GetQueuedCompletionStatus(
                           hCompletionPort,
                           &dwIoSize,
                           (LPDWORD)&lpClientContext,
                           &lpOverlapped,
                           (DWORD)-1
                           );

            //
            // If the IO failed, close the socket and free context.
            //

            if ( !bSuccess) {
                CloseClient( lpClientContext, FALSE );
                continue;
            }

            //
            // If the request was a read, process the client request.
            //

            if ( lpClientContext->LastClientIo == ClientIoRead) {

                //
                // BUGBUG: if this were a real production piece of code,
                // we would check here for an incomplete read.  Because
                // TCP/IP is a stream oriented protocol, it is feasible
                // that we could receive part of a client request.
                // Therefore, we should check for the CRLF that ends a
                // client request.
                //

                //
                // Process the request.  Pop3Dispatch() handles all aspects
                // of the request and tells us how to respond to the client.
                //

                Disposition = Pop3Dispatch(
                                  lpClientContext->Context,
                                  lpClientContext->Buffer,
                                  dwIoSize,
                                  &hFile,
                                  &OutputBuffer,
                                  &OutputBufferLen
                                  );

                //
                // Act based on the Disposition.
                //

                switch ( Disposition) {

                case Pop3_Discard:
                    break;

                case Pop3_SendError:
                case Pop3_SendBuffer:

                    // --- DavidTr: Slide 7(a) -----------------------------------
                    //
                    // Set up context information and perform an overlapped
                    // write on the socket.
                    //

                    lpClientContext->LastClientIo = ClientIoWrite;
                    lpClientContext->TransmittedBuffer = OutputBuffer;

                    bSuccess = WriteFile(
                                   (HANDLE)lpClientContext->Socket,
                                   OutputBuffer,
                                   OutputBufferLen,
                                   &dwIoSize,
                                   &lpClientContext->Overlapped
                                   );
                    if ( !bSuccess && GetLastError( ) != ERROR_IO_PENDING) {
                        CloseClient( lpClientContext, FALSE );
                        continue;
                    }

                    //
                    // Continue looping to get completed IO requests--we
                    // do not want to pend another read now.
                    //

                    continue;

                case Pop3_SendFile:
                case Pop3_SendBufferThenFile:

                    //
                    // Determine based on the disposition whether we will
                    // need to send a head or tail buffer.
                    //

                    if ( Disposition == Pop3_SendFile) {
                        TranfileBuffers.Head = NULL;
                        TranfileBuffers.HeadLength = 0;
                    } else if ( Disposition == Pop3_SendBufferThenFile) {
                        TranfileBuffers.Head = OutputBuffer;
                        TranfileBuffers.HeadLength = OutputBufferLen;
                    }

                    //
                    // After the file, we're going to send a .CRLF sequence
                    // so that the client detects EOF.  Note that
                    // TransmitFile() will send this terminator in the same
                    // packet as the last chunk of the file, thereby saving
                    // network traffic.
                    //

                    TranfileBuffers.Tail = ".\r\n";
                    TranfileBuffers.TailLength = 3;

                    //
                    // Set up context for the I/O so that we know how to act
                    // when the I/O completes.
                    //

                    lpClientContext->LastClientIo = ClientIoTransmitFile;
                    lpClientContext->TransmittedFile = hFile;
                    lpClientContext->TransmittedBuffer = OutputBuffer;

                    // --- DavidTr: Slide 21 ---------------------------------
                    //
                    // Now transmit the file and the data buffers.
                    //

                    bSuccess = TransmitFile(
                                   lpClientContext->Socket,
                                   hFile,
                                   0,
                                   0,
                                   &lpClientContext->Overlapped,
                                   &TranfileBuffers,
                                   0
                                   );
                    if ( !bSuccess && GetLastError( ) != ERROR_IO_PENDING) {
                        CloseClient( lpClientContext, FALSE );
                        continue;
                    }

                    //
                    // Continue looping to get completed IO requests--we
                    // do not want to pend another read now.
                    //

                    continue;
                }

            } else if ( lpClientContext->LastClientIo == ClientIoWrite) {

                //
                // Clean up after the WriteFile().
                //

                LocalFree( lpClientContext->TransmittedBuffer );

            } else if ( lpClientContext->LastClientIo == ClientIoTransmitFile) {

                //
                // Clean up after the TransmitFile().
                //

                CloseHandle( lpClientContext->TransmittedFile );
                LocalFree( lpClientContext->TransmittedBuffer );
            }

            // --- DavidTr: Slide 7(b) ---------------------------------------
            //
            // Pend another read request to get the next client request.
            //

            lpClientContext->LastClientIo = ClientIoRead;
            lpClientContext->BytesReadSoFar = 0;

            bSuccess = ReadFile(
                           (HANDLE)lpClientContext->Socket,
                           lpClientContext->Buffer,
                           sizeof(lpClientContext->Buffer),
                           &dwIoSize,
                           &lpClientContext->Overlapped
                           );
            if ( !bSuccess && GetLastError( ) != ERROR_IO_PENDING) {
                CloseClient( lpClientContext, FALSE );
                continue;
            }

            //
            // Loop around to get another completed IO request.
            //
        }
    }

    DWORD ThreadFunc()
    {
        if (this->mailSocket.Accept(this->listenSocket) == false) {
            error("accept");
            return 1;
        }
        int zero = 0;
        if (this->mailSocket.SetOpt(SOL_SOCKET, SO_SNDBUF, &zero, sizeof(zero)) == false) {
            error("setsockopt");
            return 1;
        }

        const char* welcome = "220 127.0.0.1 SMTP";
        if (Echo(welcome) == SOCKET_ERROR) {
            error("send");
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

public:
    static Server* Run(Socket& listenSocket) throw()
    {
        Server* self = new Server(listenSocket);
        if (self != NULL && self->StartThread() == false) {
            delete self;
            self = NULL;
        }
        return self;
    }
};
*/


typedef enum _IO_OPERATION {
    ClientIoAccept,
    ClientIoRead,
    ClientIoWrite
} IO_OPERATION, *PIO_OPERATION;

typedef struct _PER_IO_CONTEXT {
    WSAOVERLAPPED               Overlapped;
    char                        Buffer[MAX_BUFF_SIZE];
    WSABUF                      wsabuf;
    int                         nTotalBytes;
    int                         nSentBytes;
    IO_OPERATION                IOOperation;
    SOCKET                      SocketAccept;

    struct _PER_IO_CONTEXT      *pIOContextForward;
} PER_IO_CONTEXT, *PPER_IO_CONTEXT;


typedef struct _PER_SOCKET_CONTEXT {
    SOCKET                      Socket;

    //
    //linked list for all outstanding i/o on the socket
    //
    PPER_IO_CONTEXT             pIOContext;
    struct _PER_SOCKET_CONTEXT  *pCtxtBack;
    struct _PER_SOCKET_CONTEXT  *pCtxtForward;
} PER_SOCKET_CONTEXT, *PPER_SOCKET_CONTEXT;

SRWLOCK g_lock;
HANDLE g_iocp;
TP_CALLBACK_ENVIRON g_tpcbenv;
PTP_CLEANUP_GROUP g_tpclean;
SOCKET g_listenSocket;
PPER_SOCKET_CONTEXT g_ctxListenSocket;
PPER_SOCKET_CONTEXT g_ctxList;
bool g_endServer;

SOCKET CreateSocket()
{
    SOCKET socket = ::WSASocket(AF_INET, SOCK_STREAM, IPPROTO_IP, NULL, 0, WSA_FLAG_OVERLAPPED);
    if (socket == INVALID_SOCKET) {
        error("WSASocket");
        return socket;
    }

    //
    // Disable send buffering on the socket.  Setting SO_SNDBUF
    // to 0 causes winsock to stop buffering sends and perform
    // sends directly from our buffers, thereby save one memory copy.
    //
    // However, this does prevent the socket from ever filling the
    // send pipeline. This can lead to packets being sent that are
    // not full (i.e. the overhead of the IP and TCP headers is
    // great compared to the amount of data being carried).
    //
    // Disabling the send buffer has less serious repercussions
    // than disabling the receive buffer.
    //
    int zero = 0;
    int retval = ::setsockopt(sdSocket, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<char*>(&zero), sizeof(zero));
    if (retval == SOCKET_ERROR) {
        error("setsockopt");
    }

    return socket;
}

bool CreateListenSocket(UINT16 port)
{
    //
    // Resolve the interface
    //
    struct addrinfo hints = {};
    hints.ai_flags  = AI_PASSIVE;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_IP;
    struct addrinfo *addrlocal = NULL;
    if (::getaddrinfo(NULL, port, &hints, &addrlocal) != 0) {
        error("getaddrinfo");
        return false;
    }

    if (addrlocal == NULL) {
        printf("getaddrinfo() failed to resolve/convert the interface\n");
        return false;
    }

    g_listenSocket = CreateSocket();
    if (g_listenSocket == INVALID_SOCKET) {
        ::freeaddrinfo(addrlocal);
        return false;
    }

    int retval = ::bind(g_listenSocket, addrlocal->ai_addr, static_cast<int>(addrlocal->ai_addrlen));
    if (retval == SOCKET_ERROR) {
        error("bind");
        ::freeaddrinfo(addrlocal);
        return false;
    }

    retval = ::listen(g_listenSocket, 5);
    if (retval == SOCKET_ERROR) {
        error("listen");
        ::freeaddrinfo(addrlocal);
        return false;
    }

    ::freeaddrinfo(addrlocal);
    return true;
}


void CtxListDeleteFrom(PPER_SOCKET_CONTEXT context)
{
    ::AcquireSRWLockExclusive(&g_lock);

    if (context != NULL) {
        PPER_SOCKET_CONTEXT pBack       = context->pCtxtBack;
        PPER_SOCKET_CONTEXT pForward    = context->pCtxtForward;

        if (pBack == NULL && pForward == NULL) {
            //
            // This is the only node in the list to delete
            //
            g_ctxList = NULL;
        } else if (pBack == NULL && pForward != NULL) {

            //
            // This is the start node in the list to delete
            //
            pForward->pCtxtBack = NULL;
            g_ctxList = pForward;
        } else if (pBack != NULL && pForward == NULL) {
            //
            // This is the end node in the list to delete
            //
            pBack->pCtxtForward = NULL;
        } else if (pBack && pForward) {
            //
            // Neither start node nor end node in the list
            //
            pBack->pCtxtForward = pForward;
            pForward->pCtxtBack = pBack;
        }

        //
        // Free all i/o context structures per socket
        //
        PPER_IO_CONTEXT pTempIO = (PPER_IO_CONTEXT)(context->pIOContext);
        PPER_IO_CONTEXT pNextIO = NULL;
        do {
            pNextIO = (PPER_IO_CONTEXT)(pTempIO->pIOContextForward);
            if (pTempIO) {
                //
                //The overlapped structure is safe to free when only the posted i/o has
                //completed. Here we only need to test those posted but not yet received
                //by PQCS in the shutdown process.
                //
                if (g_endServer) {
                    while (!HasOverlappedIoCompleted((LPOVERLAPPED)pTempIO)) {
                        Sleep(0);
                    }
                }
                delete pTempIO;
                pTempIO = NULL;
            }
            pTempIO = pNextIO;
        } while (pNextIO != NULL);

        delete context;
        context = NULL;
    } else {
        printf("CtxtListDeleteFrom: context is NULL\n");
    }

    ::ReleaseSRWLockExclusive(&g_lock);
}

void CloseClient(PPER_SOCKET_CONTEXT context, BOOL graceful)
{
    if (context) {
        printf("CloseClient: Socket(%d) connection closing (graceful=%s)\n", context->Socket, (graceful?"TRUE":"FALSE"));
        if (!graceful) {
            //
            // force the subsequent closesocket to be abortative.
            //
            LINGER  lingerStruct;
            lingerStruct.l_onoff = 1;
            lingerStruct.l_linger = 0;
            ::setsockopt(context->Socket, SOL_SOCKET, SO_LINGER, (char *)&lingerStruct, sizeof(lingerStruct));
        }
        if (context->pIOContext->SocketAccept != INVALID_SOCKET) {
            ::closesocket(context->pIOContext->SocketAccept);
            context->pIOContext->SocketAccept = INVALID_SOCKET;
        };

        ::closesocket(context->Socket);
        context->Socket = INVALID_SOCKET;
        CtxtListDeleteFrom(context);
        context = NULL;
    } else {
        printf("CloseClient: context is NULL\n");
    }
}

PPER_SOCKET_CONTEXT CtxAllocate(SOCKET socket, IO_OPERATION ClientIO)
{
    PPER_SOCKET_CONTEXT context;

    context = new PER_SOCKET_CONTEXT;
    context->pIOContext = new PER_IO_CONTEXT;
    context->Socket = socket;
    context->pCtxtBack = NULL;
    context->pCtxtForward = NULL;

    context->pIOContext->Overlapped.Internal = 0;
    context->pIOContext->Overlapped.InternalHigh = 0;
    context->pIOContext->Overlapped.Offset = 0;
    context->pIOContext->Overlapped.OffsetHigh = 0;
    context->pIOContext->Overlapped.hEvent = NULL;
    context->pIOContext->IOOperation = ClientIO;
    context->pIOContext->pIOContextForward = NULL;
    context->pIOContext->nTotalBytes = 0;
    context->pIOContext->nSentBytes  = 0;
    context->pIOContext->wsabuf.buf  = context->pIOContext->Buffer;
    context->pIOContext->wsabuf.len  = sizeof(context->pIOContext->Buffer);
    context->pIOContext->SocketAccept = INVALID_SOCKET;

    ZeroMemory(context->pIOContext->wsabuf.buf, context->pIOContext->wsabuf.len);

    return context;
}

void CtxListAddTo(PPER_SOCKET_CONTEXT context)
{
    ::AcquireSRWLockExclusive(&g_lock);

    if (g_ctxList == NULL) {
        //
        // add the first node to the linked list
        //
        context->pCtxtBack    = NULL;
        context->pCtxtForward = NULL;
        g_ctxList = context;
    } else {
        //
        // add node to head of list
        //
        PPER_SOCKET_CONTEXT temp = g_ctxList;

        g_ctxList = context;
        context->pCtxtBack    = temp;
        context->pCtxtForward = NULL;

        temp->pCtxtForward = context;
    }

    ::ReleaseSRWLockExclusive(&g_lock);
}

void CtxtListFree()
{
    ::AcquireSRWLockExclusive(&g_lock);

    PPER_SOCKET_CONTEXT pTemp1 = g_ctxList;
    while (pTemp1 != NULL) {
        PPER_SOCKET_CONTEXT pTemp2 = pTemp1->pCtxtBack;
        CloseClient(pTemp1, false);
        pTemp1 = pTemp2;
    }

    ::ReleaseSRWLockExclusive(&g_lock);
}

PPER_SOCKET_CONTEXT UpdateCompletionPort(SOCKET socket, IO_OPERATION ClientIo, bool addToList)
{
    PPER_SOCKET_CONTEXT context = CtxtAllocate(socket, ClientIo);

    g_iocp = ::CreateIoCompletionPort((HANDLE)socket, g_iocp, (DWORD_PTR)context, 0);
    if (g_iocp == NULL) {
        errorw("CreateIoCompletionPort");
        delete context->pIOContext;
        delete context;
        return NULL;
    }

    //
    //The listening socket context (bAddToList is FALSE) is not added to the list.
    //All other socket contexts are added to the list.
    //
    if (addToList) {
        CtxListAddTo(context);
    }

    printf("UpdateCompletionPort: Socket(%d) added to IOCP\n", context->Socket);
    return context;
}

bool CreateAcceptSocket(bool updateIOCP)
{
    //
    //The context for listening socket uses the SockAccept member to store the
    //socket for client connection.
    //
    if (updateIOCP) {
        g_ctxListenSocket = UpdateCompletionPort(g_listenSocket, ClientIoAccept, false);
        if (g_ctxListenSocket == NULL) {
            printf("failed to update listen socket to IOCP\n");
            return false;
        }
    }

    g_ctxListenSocket->pIOContext->SocketAccept = CreateSocket();
    if (g_ctxListenSocket->pIOContext->SocketAccept == INVALID_SOCKET) {
        myprintf("failed to create new accept socket\n");
        return false;
    }

    //
    // pay close attention to these parameters and buffer lengths
    //
    DWORD dwRecvNumBytes = 0;
    DWORD bytes = 0;
    int retval = ::AcceptEx(g_listenSocket, g_ctxListenSocket->pIOContext->SocketAccept,
                    (LPVOID)(g_ctxListenSocket->pIOContext->Buffer),
                    0,
                    sizeof(SOCKADDR_STORAGE) + 16, sizeof(SOCKADDR_STORAGE) + 16,
                    &dwRecvNumBytes,
                    (LPOVERLAPPED) &(g_ctxListenSocket->pIOContext->Overlapped));
    if (retval == SOCKET_ERROR && (ERROR_IO_PENDING != WSAGetLastError())) {
        error("AcceptEx");
        return false;
    }

    return true;
}



//
// Worker thread that handles all I/O requests on any socket handle added to the IOCP.
//
DWORD WINAPI WorkerThread (LPVOID WorkThreadContext)	{

    HANDLE hIOCP = (HANDLE)WorkThreadContext;
    BOOL bSuccess = FALSE;
    int nRet = 0;
    LPWSAOVERLAPPED lpOverlapped = NULL;
    PPER_SOCKET_CONTEXT lpPerSocketContext = NULL;
    PPER_SOCKET_CONTEXT lpAcceptSocketContext = NULL;
    PPER_IO_CONTEXT lpIOContext = NULL;
    WSABUF buffRecv;
    WSABUF buffSend;
    DWORD dwRecvNumBytes = 0;
    DWORD dwSendNumBytes = 0;
    DWORD dwFlags = 0;
    DWORD dwIoSize = 0;
    HRESULT hRet;

    while( TRUE ) {

        //
        // continually loop to service io completion packets
        //
        bSuccess = GetQueuedCompletionStatus(
                                            hIOCP,
                                            &dwIoSize,
                                            (PDWORD_PTR)&lpPerSocketContext,
                                            (LPOVERLAPPED *)&lpOverlapped,
                                            INFINITE
                                            );
        if( !bSuccess )
            myprintf("GetQueuedCompletionStatus() failed: %d\n", GetLastError());

        if( lpPerSocketContext == NULL ) {

            //
            // CTRL-C handler used PostQueuedCompletionStatus to post an I/O packet with
            // a NULL CompletionKey (or if we get one for any reason).  It is time to exit.
            //
            return(0);
        }

        if( g_bEndServer ) {

            //
            // main thread will do all cleanup needed - see finally block
            //
            return(0);
        }

        lpIOContext = (PPER_IO_CONTEXT)lpOverlapped;

        //
        //We should never skip the loop and not post another AcceptEx if the current
        //completion packet is for previous AcceptEx
        //
        if( lpIOContext->IOOperation != ClientIoAccept ) {
            if( !bSuccess || (bSuccess && (0 == dwIoSize)) ) {

                //
                // client connection dropped, continue to service remaining (and possibly
                // new) client connections
                //
                CloseClient(lpPerSocketContext, FALSE);
                continue;
            }
        }

        //
        // determine what type of IO packet has completed by checking the PER_IO_CONTEXT
        // associated with this socket.  This will determine what action to take.
        //
        switch( lpIOContext->IOOperation ) {
        case ClientIoAccept:

            //
            // When the AcceptEx function returns, the socket sAcceptSocket is
            // in the default state for a connected socket. The socket sAcceptSocket
            // does not inherit the properties of the socket associated with
            // sListenSocket parameter until SO_UPDATE_ACCEPT_CONTEXT is set on
            // the socket. Use the setsockopt function to set the SO_UPDATE_ACCEPT_CONTEXT
            // option, specifying sAcceptSocket as the socket handle and sListenSocket
            // as the option value.
            //
            nRet = setsockopt(
                             lpPerSocketContext->pIOContext->SocketAccept,
                             SOL_SOCKET,
                             SO_UPDATE_ACCEPT_CONTEXT,
                             (char *)&g_sdListen,
                             sizeof(g_sdListen)
                             );

            if( nRet == SOCKET_ERROR ) {

                //
                //just warn user here.
                //
                myprintf("setsockopt(SO_UPDATE_ACCEPT_CONTEXT) failed to update accept socket\n");
                WSASetEvent(g_hCleanupEvent[0]);
                return(0);
            }

            lpAcceptSocketContext = UpdateCompletionPort(
                                                        lpPerSocketContext->pIOContext->SocketAccept,
                                                        ClientIoAccept, TRUE);

            if( lpAcceptSocketContext == NULL ) {

                //
                //just warn user here.
                //
                myprintf("failed to update accept socket to IOCP\n");
                WSASetEvent(g_hCleanupEvent[0]);
                return(0);
            }

            if( dwIoSize ) {
                lpAcceptSocketContext->pIOContext->IOOperation = ClientIoWrite;
                lpAcceptSocketContext->pIOContext->nTotalBytes  = dwIoSize;
                lpAcceptSocketContext->pIOContext->nSentBytes   = 0;
                lpAcceptSocketContext->pIOContext->wsabuf.len   = dwIoSize;
                hRet = StringCbCopyN(lpAcceptSocketContext->pIOContext->Buffer,
                                    MAX_BUFF_SIZE,
                                    lpPerSocketContext->pIOContext->Buffer,
                                    sizeof(lpPerSocketContext->pIOContext->Buffer)
                                    );
                lpAcceptSocketContext->pIOContext->wsabuf.buf = lpAcceptSocketContext->pIOContext->Buffer;

                nRet = WSASend(
                              lpPerSocketContext->pIOContext->SocketAccept,
                              &lpAcceptSocketContext->pIOContext->wsabuf, 1,
                              &dwSendNumBytes,
                              0,
                              &(lpAcceptSocketContext->pIOContext->Overlapped), NULL);

                if( nRet == SOCKET_ERROR && (ERROR_IO_PENDING != WSAGetLastError()) ) {
                    myprintf ("WSASend() failed: %d\n", WSAGetLastError());
                    CloseClient(lpAcceptSocketContext, FALSE);
                } else if( g_bVerbose ) {
                    myprintf("WorkerThread %d: Socket(%d) AcceptEx completed (%d bytes), Send posted\n",
                           GetCurrentThreadId(), lpPerSocketContext->Socket, dwIoSize);
                }
            } else {

                //
                // AcceptEx completes but doesn't read any data so we need to post
                // an outstanding overlapped read.
                //
                lpAcceptSocketContext->pIOContext->IOOperation = ClientIoRead;
                dwRecvNumBytes = 0;
                dwFlags = 0;
                buffRecv.buf = lpAcceptSocketContext->pIOContext->Buffer,
                buffRecv.len = MAX_BUFF_SIZE;
                nRet = WSARecv(
                              lpAcceptSocketContext->Socket,
                              &buffRecv, 1,
                              &dwRecvNumBytes,
                              &dwFlags,
                              &lpAcceptSocketContext->pIOContext->Overlapped, NULL);
                if( nRet == SOCKET_ERROR && (ERROR_IO_PENDING != WSAGetLastError()) ) {
                    myprintf ("WSARecv() failed: %d\n", WSAGetLastError());
                    CloseClient(lpAcceptSocketContext, FALSE);
                }
            }

            //
            //Time to post another outstanding AcceptEx
            //
            if( !CreateAcceptSocket(FALSE) ) {
                myprintf("Please shut down and reboot the server.\n");
                WSASetEvent(g_hCleanupEvent[0]);
                return(0);
            }
            break;


        case ClientIoRead:

            //
            // a read operation has completed, post a write operation to echo the
            // data back to the client using the same data buffer.
            //
            lpIOContext->IOOperation = ClientIoWrite;
            lpIOContext->nTotalBytes = dwIoSize;
            lpIOContext->nSentBytes  = 0;
            lpIOContext->wsabuf.len  = dwIoSize;
            dwFlags = 0;
            nRet = WSASend(
                          lpPerSocketContext->Socket,
                          &lpIOContext->wsabuf, 1, &dwSendNumBytes,
                          dwFlags,
                          &(lpIOContext->Overlapped), NULL);
            if( nRet == SOCKET_ERROR && (ERROR_IO_PENDING != WSAGetLastError()) ) {
                myprintf("WSASend() failed: %d\n", WSAGetLastError());
                CloseClient(lpPerSocketContext, FALSE);
            } else if( g_bVerbose ) {
                myprintf("WorkerThread %d: Socket(%d) Recv completed (%d bytes), Send posted\n",
                       GetCurrentThreadId(), lpPerSocketContext->Socket, dwIoSize);
            }
            break;

        case ClientIoWrite:

            //
            // a write operation has completed, determine if all the data intended to be
            // sent actually was sent.
            //
            lpIOContext->IOOperation = ClientIoWrite;
            lpIOContext->nSentBytes  += dwIoSize;
            dwFlags = 0;
            if( lpIOContext->nSentBytes < lpIOContext->nTotalBytes ) {

                //
                // the previous write operation didn't send all the data,
                // post another send to complete the operation
                //
                buffSend.buf = lpIOContext->Buffer + lpIOContext->nSentBytes;
                buffSend.len = lpIOContext->nTotalBytes - lpIOContext->nSentBytes;
                nRet = WSASend (
                               lpPerSocketContext->Socket,
                               &buffSend, 1, &dwSendNumBytes,
                               dwFlags,
                               &(lpIOContext->Overlapped), NULL);
                if( nRet == SOCKET_ERROR && (ERROR_IO_PENDING != WSAGetLastError()) ) {
                    myprintf ("WSASend() failed: %d\n", WSAGetLastError());
                    CloseClient(lpPerSocketContext, FALSE);
                } else if( g_bVerbose ) {
                    myprintf("WorkerThread %d: Socket(%d) Send partially completed (%d bytes), Recv posted\n",
                           GetCurrentThreadId(), lpPerSocketContext->Socket, dwIoSize);
                }
            } else {

                //
                // previous write operation completed for this socket, post another recv
                //
                lpIOContext->IOOperation = ClientIoRead;
                dwRecvNumBytes = 0;
                dwFlags = 0;
                buffRecv.buf = lpIOContext->Buffer,
                buffRecv.len = MAX_BUFF_SIZE;
                nRet = WSARecv(
                              lpPerSocketContext->Socket,
                              &buffRecv, 1, &dwRecvNumBytes,
                              &dwFlags,
                              &lpIOContext->Overlapped, NULL);
                if( nRet == SOCKET_ERROR && (ERROR_IO_PENDING != WSAGetLastError()) ) {
                    myprintf ("WSARecv() failed: %d\n", WSAGetLastError());
                    CloseClient(lpPerSocketContext, FALSE);
                } else if( g_bVerbose ) {
                    myprintf("WorkerThread %d: Socket(%d) Send completed (%d bytes), Recv posted\n",
                           GetCurrentThreadId(), lpPerSocketContext->Socket, dwIoSize);
                }
            }
            break;

        } //switch
    } //while
    return(0);
}

int main()
{
    unsigned short port = 25;

    WSAInitialiser init(0x202);
    if (init == SOCKET_ERROR) {
        error("WSAStartup");
        return EXIT_FAILURE;
    }

    ::InitializeSRWLock(&g_lock);
    ::InitializeThreadpoolEnvironment(&g_tpcbenv);
    g_tpclean = ::CreateThreadpoolCleanupGroup();
    if (g_tpclean == NULL) {
        errorw("CreateThreadpoolCleanupGroup");
        return EXIT_FAILURE;
    }
    ::SetThreadpoolCallbackCleanupGroup(&g_tpcbenv, g_tpclean, NULL);

    g_iocp = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    if (g_iocp == NULL) {
        errorw("CreateIoCompletionPort");
        return EXIT_FAILURE;
    }

    if (CreateListenSocket() && CreateAcceptSocket()) {
        printf("Press enter key to exit\n");
        char buffer[100];
        StringCchGetsA(buffer, _countof(buffer));
    }

    return 0;
}
