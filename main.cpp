#define WINVER 0x0600
#define _WIN32_WINNT 0x0600
#define _WIN32_IE 0x0700
#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#include <sdkddkver.h>
#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <commctrl.h>
#include <shlwapi.h>
#include <winsock2.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strsafe.h>
#include <uxtheme.h>
#include <tchar.h>

#define USETRACE

#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "ws2_32.lib")

#pragma warning(disable: 4127 4355)

// https://blogs.msdn.microsoft.com/oldnewthing/20041025-00/?p=37483/
extern "C" IMAGE_DOS_HEADER     __ImageBase;
#define HINST_THISCOMPONENT     ((HINSTANCE)(&__ImageBase))

/* void Cls_OnSettingChange(HWND hwnd, UINT uiParam, LPCTSTR lpszSectionName) */
#define HANDLE_WM_SETTINGCHANGE(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), (UINT)(wParam), (LPCTSTR)(lParam)), 0L)
#define FORWARD_WM_SETTINGCHANGE(hwnd, uiParam, lpszSectionName, fn) \
    (void)(fn)((hwnd), WM_WININICHANGE, (WPARAM)(UINT)(uiParam), (LPARAM)(LPCTSTR)(lpszSectionName))

// HANDLE_WM_CONTEXTMENU is buggy
// https://blogs.msdn.microsoft.com/oldnewthing/20040921-00/?p=37813
/* void Cls_OnContextMenu(HWND hwnd, HWND hwndContext, int xPos, int yPos) */
#undef HANDLE_WM_CONTEXTMENU
#undef FORWARD_WM_CONTEXTMENU
#define HANDLE_WM_CONTEXTMENU(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), (HWND)(wParam), (int)GET_X_LPARAM(lParam), (int)GET_Y_LPARAM(lParam)), 0L)
#define FORWARD_WM_CONTEXTMENU(hwnd, hwndContext, xPos, yPos, fn) \
    (void)(fn)((hwnd), WM_CONTEXTMENU, (WPARAM)(HWND)(hwndContext), MAKELPARAM((int)(xPos), (int)(yPos)))

#undef TRACE
#undef ASSERT

#ifdef USETRACE
#define TRACE(s, ...)       Trace(s, __VA_ARGS__)
#define ASSERT(e)           do if (!(e)) { TRACE("%s(%d): Assertion failed\n", __FILE__, __LINE__); if (::IsDebuggerPresent()) { ::DebugBreak(); } } while(0)
#else
#define TRACE(s, ...)
#define ASSERT(e)
#endif

inline void* operator new(size_t size) throw()
{
    return ::malloc(size);
}

inline void* operator new[](size_t size) throw()
{
    return ::malloc(size);
}

inline void operator delete(void* ptr) throw()
{
    ::free(ptr);
}

inline void operator delete[](void* ptr) throw()
{
    ::free(ptr);
}

inline int RECTWIDTH(const RECT& rect) throw()
{
    return rect.right - rect.left;
}

inline int RECTHEIGHT(const RECT& rect) throw()
{
    return rect.bottom - rect.top;
}

void Trace(__in __format_string const char* format, ...)
{
    TCHAR buffer[512];
    va_list argPtr;
    va_start(argPtr, format);
#ifdef _UNICODE
    WCHAR wformat[512];
    MultiByteToWideChar(CP_ACP, 0, format, -1, wformat, _countof(wformat));
    StringCchVPrintfW(buffer, _countof(buffer), wformat, argPtr);
#else
    StringCchVPrintfA(buffer, _countof(buffer), format, argPtr);
#endif
    WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), buffer, lstrlen(buffer), NULL, NULL);
    va_end(argPtr);
}

void error(const char* what, DWORD code)
{
    what, code;
#ifdef USETRACE
    TCHAR codestr[30];
    if (code & 0x80000000) {
        ::StringCchPrintf(codestr, _countof(codestr), _T("0x%08x"), code);
    } else {
        ::StringCchPrintf(codestr, _countof(codestr), _T("%u"), code);
    }
    TCHAR buffer[1024];
    ::FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, code, 0, buffer, _countof(buffer), NULL);
    TRACE("%hs() failed with error %s\n%s\n", what, codestr, buffer);
#endif
}

void serror(const char* what)
{
    what;
#ifdef USETRACE
    error(what, WSAGetLastError());
#endif
}

void werror(const char* what)
{
    what;
#ifdef USETRACE
    error(what, GetLastError());
#endif
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

template <class T>
class TPtrArrayDestroyHelper
{
public:
    static void DeleteAllPtrs(HDPA hdpa)
    {
        ::DPA_DeleteAllPtrs(hdpa);
    }
    static void Destroy(HDPA hdpa)
    {
        ::DPA_Destroy(hdpa);
    }
};

template <class T>
class TPtrArrayAutoDeleteHelper
{
    static int CALLBACK sDeletePtrCB(void* p, void*)
    {
        delete static_cast<T*>(p);
        return 1;
    }

public:
    static void DeleteAllPtrs(HDPA hdpa)
    {
        ::DPA_EnumCallback(hdpa, sDeletePtrCB, NULL);
        ::DPA_DeleteAllPtrs(hdpa);
    }
    static void Destroy(HDPA hdpa)
    {
        ::DPA_DestroyCallback(hdpa, sDeletePtrCB, NULL);
    }
};

template <class T>
class TPtrArrayAutoFreeHelper
{
    static int CALLBACK sDeletePtrCB(void* p, void*)
    {
        ::free(p);
        return 1;
    }

public:
    static void DeleteAllPtrs(HDPA hdpa)
    {
        ::DPA_EnumCallback(hdpa, sDeletePtrCB, NULL);
        ::DPA_DeleteAllPtrs(hdpa);
    }
    static void Destroy(HDPA hdpa)
    {
        ::DPA_DestroyCallback(hdpa, sDeletePtrCB, NULL);
    }
};

template <class T>
class TPtrArrayAutoCoFreeHelper
{
    static int CALLBACK sDeletePtrCB(void* p, void*)
    {
        ::SHFree(p);
        return 1;
    }

public:
    static void DeleteAllPtrs(HDPA hdpa)
    {
        ::DPA_EnumCallback(hdpa, sDeletePtrCB, NULL);
        ::DPA_DeleteAllPtrs(hdpa);
    }
    static void Destroy(HDPA hdpa)
    {
        ::DPA_DestroyCallback(hdpa, sDeletePtrCB, NULL);
    }
};

template <class T, int cGrow, typename TDestroy>
class PtrArray
{
private:
    HDPA hdpa;

    PtrArray(const PtrArray&) throw();
    PtrArray& operator =(const PtrArray&) throw();

public:
    PtrArray() throw()
        : hdpa()
    {
    }
    ~PtrArray() throw()
    {
        TDestroy::Destroy(this->hdpa);
    }

    int GetCount() const throw()
    {
        if (this->hdpa == NULL) {
            return 0;
        }
        return DPA_GetPtrCount(this->hdpa);
    }

    T* FastGetAt(int index) const throw()
    {
        return static_cast<T*>(DPA_FastGetPtr(this->hdpa, index));
    }

    T* GetAt(int index) const throw()
    {
        return static_cast<T*>(::DPA_GetPtr(this->hdpa, index));
    }

    T** GetData() const throw()
    {
        return reinterpret_cast<T**>(DPA_GetPtrPtr(this->hdpa));
    }

    bool Grow(int newSize) throw()
    {
        return (::DPA_Grow(this->hdpa, newSize) != FALSE);
    }

    bool SetAtGrow(int index, T* p) throw()
    {
        return (::DPA_SetPtr(this->hdpa, index, p) != FALSE);
    }

    bool Create() throw()
    {
        if (this->hdpa != NULL) {
            return true;
        }
        this->hdpa = ::DPA_Create(cGrow);
        return (this->hdpa != NULL);
    }

    bool Create(HANDLE hheap) throw()
    {
        if (this->hdpa != NULL) {
            return true;
        }
        this->hdpa = ::DPA_CreateEx(cGrow, hheap);
        return (this->hdpa != NULL);
    }

    int Add(T* p) throw()
    {
        return InsertAt(DA_LAST, p);
    }

    int InsertAt(int index, T* p) throw()
    {
        return ::DPA_InsertPtr(this->hdpa, index, p);
    }

    T* RemoveAt(int index) throw()
    {
        return static_cast<T*>(::DPA_DeletePtr(this->hdpa, index));
    }

    void RemoveAll()
    {
        TDestroy::DeleteAllPtrs(this->hdpa);
    }

    void Enumerate(__in PFNDAENUMCALLBACK callback, __in_opt void* data) const
    {
        ::DPA_EnumCallback(this->hdpa, callback, data);
    }

    int Search(__in_opt void* key, __in int start, __in PFNDACOMPARE comparator, __in LPARAM lParam, __in UINT options)
    {
        return ::DPA_Search(this->hdpa, key, start, comparator, lParam, options);
    }

    bool Sort(__in PFNDACOMPARE comparator, __in LPARAM lParam)
    {
        return (::DPA_Sort(this->hdpa, comparator, lParam) != FALSE);
    }

    T* operator [](int index) const throw()
    {
        return GetAt(index);
    }
};

enum SmtpCommand
{
    Smtp_HELO,
    Smtp_EHLO,
    Smtp_MAIL,
    Smtp_RCPT,
    Smtp_DATA,
    Smtp_RSET,
    Smtp_SEND,
    Smtp_SOML,
    Smtp_SAML,
    Smtp_VRFY,
    Smtp_EXPN,
    Smtp_HELP,
    Smtp_NOOP,
    Smtp_QUIT,
    SMTP_TURN,
};

enum ReplyCommand
{
    ReplyCmd_Ready = 220,
    ReplyCmd_Closing = 221,
    ReplyCmd_Okay = 250,
    ReplyCmd_StartMail = 354,
    ReplyCmd_OutOfMemory = 452,
    ReplyCmd_SyntaxError = 500,
    ReplyCmd_InvalidParam = 501,
    ReplyCmd_NotImpl = 502,
    ReplyCmd_BadSeqCmd = 503,
};

class MailMessage
{
public:
    enum APPENDRESULT
    {
        Ok,
        End,
        BadCmd,
        NoMem,
    };

private:
    char* sender;
    char* recipient;
    char* body;
    wchar_t* subject;
    bool inData;
    bool read;
    FILETIME time;

    static char* StrDupN(__in_ecount(cch) const char* src, size_t cch) throw()
    {
        char* dst = NULL;
        if (cch < SIZE_MAX) {
            dst = static_cast<char*>(::malloc(cch + 1));
            if (dst != NULL) {
                ::memcpy(dst, src, cch);
                dst[cch] = 0;
            }
        }
        return dst;
    }

    static char* Address(const char* data, const char* tag) throw()
    {
        size_t c = ::strlen(tag);
        if (::strncmp(data, tag, c) == 0 && data[c] == ':' && data[c + 1] == '<') {
            const char* start = data + c + 2;
            const char* end = start;
            for (; *end != 0 && *end != '>'; end++);
            if (*end == 0) {
                return NULL;
            }
            return StrDupN(start, end - start);
        }
        return NULL;
    }

public:
    MailMessage() throw()
        : sender()
        , recipient()
        , body()
        , subject()
        , inData()
        , read()
    {
        ZeroMemory(&this->time, sizeof(this->time));
    }
    ~MailMessage()
    {
        ::free(this->subject);
        ::free(this->sender);
        ::free(this->recipient);
        ::free(this->body);
    }
    const char* GetSender() const throw()
    {
        return this->sender;
    }
    const char* GetRecipient() const throw()
    {
        return this->recipient;
    }
    const char* GetBody() const throw()
    {
        return this->body;
    }
    const wchar_t* GetSubject() const throw()
    {
        return this->subject;
    }
    FILETIME GetTime() const throw()
    {
        return this->time;
    }
    bool IsRead() const throw()
    {
        return this->read;
    }
    bool SetRead(bool read = true) throw()
    {
        bool oldState = this->read;
        this->read = read;
        return oldState;
    }
    bool SetSender(const char* data) throw()
    {
        this->sender = Address(data, "FROM");
        return this->sender != NULL;
    }
    bool SetRecipient(const char* data) throw()
    {
        this->recipient = Address(data, "TO");
        return this->recipient != NULL;
    }
    bool StartBody() throw()
    {
        this->inData = true;
        return true;
    }
    APPENDRESULT AppendBody(const char* data) throw()
    {
        if (this->inData == false) {
            return BadCmd;
        }
        size_t curlen = this->body ? ::strlen(this->body) : 0;
        size_t addlen = ::strlen(data);
        if (SIZE_MAX - curlen <= addlen) {
            return NoMem;
        }
        char* dst = static_cast<char*>(::realloc(this->body, curlen + addlen + 1));
        if (dst == NULL) {
            return NoMem;
        }
        ::memcpy(dst + curlen, data, addlen);
        dst[curlen + addlen] = 0;
        this->body = dst;
        if (this->EndOfBody()) {
            return End;
        }
        return Ok;
    }
    bool EndOfBody() throw()
    {
        if (this->body == NULL) {
            return false;
        }
        char* p = this->body;
        while (*p != 0) {
            if (::strncmp(p, "\r\n.\r\n", 5) == 0) {
                *p = 0;
                ::GetSystemTimeAsFileTime(&this->time);
                this->inData = false;
                this->ParseSubject();
                return true;
            }
            p++;
        }
        return false;
    }
    static wchar_t* ParseUnstructured(__inout wchar_t* string)
    {
        const wchar_t* src = string;
        wchar_t* dst = string;

        while (*src != 0) {
            if (src[0] == L'\r' && src[1] == L'\n') {
                if (src[2] != L' ') {
                    break;
                }
                src += 2;
                continue;
            }
            *dst++ = *src++;
        }
        *dst = 0;
        return string;
    }
    void ParseSubject() throw()
    {
        if (this->body == NULL) {
            return;
        }
        const char* head = this->body;
        for (; *head != 0; head++) {
            if (head[0] != '\r' || head[1] != '\n') {
                continue;
            }
            if (head[0] == '\r' && head[1] == '\n' && head[2] == '\r' && head[3] == '\n') {
                return;
            }
            for (head += 2; *head == '\t' || *head == ' '; head++);
            if (::strncmp("Subject", head, 7) != 0) {
                continue;
            }
            break;
        }
        if (*head == 0) {
            return;
        }
        for (head += 7; *head == '\t' || *head == ' '; head++);
        if (*head != ':') {
            return;
        }
        for (head++; *head == '\t' || *head == ' '; head++);
        const char* tail = head;
        while (*tail != 0) {
            if (tail[0] == '\r' && tail[1] == '\n') {
                if (tail[2] != ' ') {
                    break;
                }
                tail += 3;
                continue;
            }
            tail++;
        }
        const UINT CP_ASCII = 20127;
        int cch = ::MultiByteToWideChar(CP_ASCII, 0, head, tail - head, NULL, 0);
        if (cch <= 0 || cch >= INT_MAX) {
            return;
        }
        this->subject = static_cast<wchar_t*>(::calloc(cch + 1, sizeof(wchar_t)));
        if (this->subject != NULL) {
            int ret = ::MultiByteToWideChar(CP_ASCII, 0, head, tail - head, this->subject, cch + 1);
            this->subject[__min(cch, __max(ret, 0))] = 0;
            ParseUnstructured(this->subject);
        }
    }
};

struct DECLSPEC_NOVTABLE IMessageSink
{
    virtual void GotMail(MailMessage* message) = 0;
};

#define CMDEQ(x, y) (strncmp(x, y, 4) == 0)

class SockerListener
{
private:
    Socket listenSocket;
    Socket clientSocket;
    volatile BOOL quit;
    IMessageSink* sink;
    HANDLE thread;
    MailMessage* message;

    SockerListener(const SockerListener&);
    SockerListener& operator =(const SockerListener&);

public:
    SockerListener(IMessageSink* ms) throw()
        : quit()
        , sink(ms)
        , thread()
        , message()
    {
    }

    ~SockerListener() throw()
    {
        this->Close();
    }

    void Close() throw()
    {
        TRACE("Terminating connection\n");
        this->quit = true;
        if (this->listenSocket.IsValid()) {
            LINGER linger;
            linger.l_onoff = 1;
            linger.l_linger = 0;
            this->listenSocket.SetOpt(SOL_SOCKET, SO_LINGER, &linger, sizeof(linger));
            this->listenSocket.Close();
        }
        this->clientSocket.Close();
        if (this->thread != NULL) {
            ::WaitForSingleObject(this->thread, INFINITE);
            ::CloseHandle(this->thread);
            this->thread = NULL;
        }
    }

    bool Listen(UINT16 port) throw()
    {
        this->quit = false;
        if (this->listenSocket.Open(AF_INET, SOCK_STREAM, IPPROTO_TCP) == false){
            serror("socket");
            return false;
        }

        struct sockaddr_in local;
        local.sin_family = AF_INET;
        local.sin_addr.s_addr = INADDR_ANY;
        local.sin_port = htons(port);
        if (this->listenSocket.Bind(local) == false) {
            serror("bind");
            return false;
        }

        if (this->listenSocket.Listen(1) == false) {
            serror("listen");
            return false;
        }

        TRACE("Listening on port %u\n", port);
        this->thread = ::CreateThread(NULL, 0, sThreadFunc, this, 0, NULL);
        if (thread == NULL) {
            TRACE("CreateThread() failed\n");
            return false;
        }
        return true;
    }

private:
    static DWORD WINAPI sThreadFunc(void* p) throw()
    {
        SockerListener* self = static_cast<SockerListener*>(p);
        return self->ThreadFunc() ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    int Echo(ReplyCommand code, const char* message) throw()
    {
        TRACE("<%u %hs\n", code, message);
        char buffer[1024];
        ::StringCchPrintfA(buffer, _countof(buffer), "%u %s\r\n", code, message);
        return this->clientSocket.Send(buffer);
    }

    int Echo(ReplyCommand code) throw()
    {
        const char* message = "";
        switch (code) {
        case ReplyCmd_Closing:
            message = "Bye";
            break;
        case ReplyCmd_Okay:
            message = "Ok";
            break;
        case ReplyCmd_StartMail:
            message = "End data with <CRLF>.<CRLF>";
            break;
        case ReplyCmd_OutOfMemory:
            message = "Out of memory";
            break;
        case ReplyCmd_SyntaxError:
        case ReplyCmd_InvalidParam:
            message = "Syntax error";
            break;
        case ReplyCmd_NotImpl:
            message = "Command not implemented";
            break;
        case ReplyCmd_BadSeqCmd:
            message = "Bad sequence of commands";
            break;
        }
        return this->Echo(code, message);
    }

    int EchoOk() throw()
    {
        return this->Echo(ReplyCmd_Okay);
    }

    int EchoOOM() throw()
    {
        return this->Echo(ReplyCmd_OutOfMemory);
    }

    int EchoParamError() throw()
    {
        return this->Echo(ReplyCmd_InvalidParam);
    }

    bool OpenSession() throw()
    {
        this->clientSocket.Close();
        if (this->clientSocket.Accept(this->listenSocket) == false) {
            serror("accept");
            return false;
        }

        const char* welcome = "127.0.0.1 SMTP";
        if (this->Echo(ReplyCmd_Ready, welcome) == SOCKET_ERROR) {
            serror("send");
            return false;
        }

        int zero = 0;
        if (this->clientSocket.SetOpt(SOL_SOCKET, SO_SNDBUF, &zero, sizeof(zero)) == false) {
            serror("setsockopt");
            return false;
        }

        return true;
    }

    void CloseSession() throw()
    {
        delete this->message;
        this->message = NULL;
        this->clientSocket.Shutdown(SD_SEND);
        this->clientSocket.Close();
    }

    void PassMessage() throw()
    {
        this->sink->GotMail(this->message);
        this->message = NULL;
    }

    bool ThreadFunc() throw()
    {
        this->message = NULL;
        bool inDataLoop = false;
        while (this->quit == false) {
            if (this->clientSocket.IsValid() == false) {
                if (this->OpenSession() == false) {
                    return false;
                }
            }

            TRACE("receiving...\n");
            char buffer[513];
            int retval = this->clientSocket.Recv(buffer, sizeof(buffer) - 1);
            if (retval == SOCKET_ERROR) {
                serror("recv");
                break;
            }
            if (retval == 0) {
                TRACE("Client closed connection\n");
                this->CloseSession();
                continue;
            }

            buffer[retval] = 0;
            TRACE("> %hs\n", buffer);
            if (inDataLoop == false) {
                if (CMDEQ(buffer, "HELO") || CMDEQ(buffer, "EHLO")) {
                    retval = this->EchoOk();
                } else if (CMDEQ(buffer, "QUIT")) {
                    retval = this->Echo(ReplyCmd_Closing);
                    this->CloseSession();
                } else if (CMDEQ(buffer, "MAIL")) {
                    delete this->message;
                    this->message = new MailMessage();
                    if (this->message != NULL) {
                        if (message->SetSender(buffer + 5)) {
                            retval = this->EchoOk();
                        } else {
                            retval = this->EchoParamError();
                        }
                    } else {
                        retval = this->EchoOOM();
                    }
                } else if (CMDEQ(buffer, "RCPT")) {
                    if (message == NULL) {
                        retval = this->Echo(ReplyCmd_BadSeqCmd);
                    } else if (message->SetRecipient(buffer + 5)) {
                        retval = this->EchoOk();
                    } else {
                        retval = this->EchoParamError();
                    }
                } else if (CMDEQ(buffer, "NOOP")) {
                    retval = this->EchoOk();
                } else if (CMDEQ(buffer, "DATA")) {
                    if (message == NULL) {
                        retval = this->Echo(ReplyCmd_BadSeqCmd);
                    } else if (message->StartBody()) {
                        retval = this->Echo(ReplyCmd_StartMail);
                    } else {
                        retval = this->EchoOOM();
                    }
                    inDataLoop = true;
                } else {
                    retval = this->Echo(ReplyCmd_NotImpl);
                }
            } else {
                switch (this->message->AppendBody(buffer)) {
                case MailMessage::Ok:
                    continue;
                case MailMessage::NoMem:
                    retval = this->EchoOOM();
                    break;
                case MailMessage::End:
                    this->PassMessage();
                    retval = this->EchoOk();
                    inDataLoop = false;
                    break;
                case MailMessage::BadCmd:
                    retval = this->Echo(ReplyCmd_BadSeqCmd);
                    break;
                }
            }
            if (retval <= 0) {
                serror("send");
                this->CloseSession();
            }
        }
        this->CloseSession();
        return 0;
    }
};


BOOL GetWorkAreaRect(HWND hwnd, __out RECT* prc) throw()
{
    HMONITOR hMonitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    if (hMonitor != NULL) {
        MONITORINFO mi = { sizeof(MONITORINFO) };
        BOOL bRet = GetMonitorInfo(hMonitor, &mi);
        if (bRet) {
            *prc = mi.rcWork;
            return bRet;
        }
    }
    return FALSE;
}

void SetWindowPosition(HWND hwnd, int x, int y, int cx, int cy) throw()
{
    RECT rcWork;
    if (GetWorkAreaRect(hwnd, &rcWork)) {
        int dx = 0, dy = 0;
        if (x < rcWork.left) {
            dx = rcWork.left - x;
        }
        if (y < rcWork.top) {
            dy = rcWork.top - y;
        }
        if (x + cx + dx > rcWork.right) {
            dx = rcWork.right - x - cx;
            if (x + dx < rcWork.left) {
                dx += rcWork.left - x;
            }
        }
        if (y + cy + dy > rcWork.bottom) {
            dy = rcWork.bottom - y - cy;
            if (y + dx < rcWork.top) {
                dy += rcWork.top - y;
            }
        }
        ::MoveWindow(hwnd, x + dx, y + dy, cx, cy, TRUE);
    }
}

void MyAdjustWindowRect(HWND hwnd, __inout LPRECT lprc) throw()
{
    ::AdjustWindowRect(lprc, WS_OVERLAPPEDWINDOW, TRUE);
    RECT rect = *lprc;
    rect.bottom = 0x7fff;
    FORWARD_WM_NCCALCSIZE(hwnd, FALSE, &rect, ::SendMessage);
    lprc->bottom += rect.top;
}

HMENU GetSubMenuById(HMENU hm, UINT id)
{
    MENUITEMINFO mi = { sizeof(mi), MIIM_SUBMENU };
    if (::GetMenuItemInfo(hm, id, false, &mi) == FALSE) {
        return NULL;
    }
    return mi.hSubMenu;
}

bool SetSubMenuById(HMENU hm, UINT id, HMENU hsub)
{
    MENUITEMINFO mi = { sizeof(mi), MIIM_SUBMENU };
    mi.hSubMenu = hsub;
    return ::SetMenuItemInfo(hm, id, false, &mi) != FALSE;
}

template <class T>
class BaseWindow
{
private:
    static LRESULT CALLBACK sWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) throw()
    {
        LPCREATESTRUCT lpcs = reinterpret_cast<LPCREATESTRUCT>(lParam);
        T* self;
        if (message == WM_NCCREATE && lpcs != NULL) {
            self = static_cast<T*>(lpcs->lpCreateParams);
            ::SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        } else {
            self = reinterpret_cast<T*>(::GetWindowLongPtr(hwnd, GWLP_USERDATA));
        }
        if (self != NULL) {
            return self->WindowProc(hwnd, message, wParam, lParam);
        } else {
            return ::DefWindowProc(hwnd, message, wParam, lParam);
        }
    }

protected:
    static ATOM Register(__inout WNDCLASS* wc) throw()
    {
        wc->lpfnWndProc = sWindowProc;
        wc->hInstance = HINST_THISCOMPONENT;
        return ::RegisterClass(wc);
    }

    static ATOM Register(LPCTSTR className, LPCTSTR menu = NULL) throw()
    {
        WNDCLASS wc = {
            0, sWindowProc, 0, 0,
            HINST_THISCOMPONENT,
            NULL, ::LoadCursor(NULL, IDC_ARROW),
            0, menu,
            className };
        return ::RegisterClass(&wc);
    }

    HWND Create(ATOM atom, LPCTSTR name) throw()
    {
        return ::CreateWindow(MAKEINTATOM(atom), name,
            WS_VISIBLE | WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT,
            CW_USEDEFAULT, CW_USEDEFAULT,
            NULL, NULL, HINST_THISCOMPONENT,
            static_cast<T*>(this));
    }
};

class SearchEdit
{
private:
    HFONT font;
    bool inSize;

    static LRESULT CALLBACK sWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR ref) throw()
    {
        SearchEdit* self = reinterpret_cast<SearchEdit*>(ref);
        return self->WindowProc(hwnd, message, wParam, lParam);
    }

    LRESULT WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) throw()
    {
        switch (message) {
        case WM_DESTROY:
            ::RemoveWindowSubclass(hwnd, sWindowProc, 0);
            break;

        case WM_SIZE:
            this->OnSize(hwnd);
            break;

        case WM_CHAR:
            if (this->OnChar(hwnd, wParam)) {
                return 0;
            }
            break;
        }
        return ::DefSubclassProc(hwnd, message, wParam, lParam);
    }

    void OnSize(HWND hwnd) throw()
    {
        if (this->inSize) {
            return;
        }
        int height = GetFontHeight(hwnd);
        RECT rect = { 0, 0, 1, height + 2 };
        ::AdjustWindowRectEx(&rect, GetWindowStyle(hwnd), false, GetWindowExStyle(hwnd));
        int y = rect.bottom - rect.top;
        ::GetClientRect(::GetParent(hwnd), &rect);
        this->inSize = true;
        ::MoveWindow(hwnd, 0, 0, rect.right, y, TRUE);
        this->inSize = false;
    }

    bool OnChar(HWND hwnd, WPARAM wParam) throw()
    {
        if (wParam == VK_ESCAPE) {
            Edit_SetText(hwnd, NULL);
            return true;
        }
        if (wParam == VK_RETURN) {
            return true;
        }
        return false;
    }

    static int GetFontHeight(HWND hwnd) throw()
    {
        TEXTMETRIC tm;
        tm.tmHeight = 0;
        HDC hdc = ::CreateCompatibleDC(NULL);
        if (hdc != NULL) {
            HFONT fontOld = SelectFont(hdc, GetWindowFont(hwnd));
            ::GetTextMetrics(hdc, &tm);
            SelectFont(hdc, fontOld);
            ::DeleteDC(hdc);
        }
        return tm.tmHeight > 0 ? tm.tmHeight : 16;
    }

public:
    SearchEdit() throw()
        : font()
        , inSize()
    {
    }

    ~SearchEdit() throw()
    {
        if (this->font != NULL) {
            DeleteFont(this->font);
        }
    }

    HWND Create(HWND hwndParent, int id) throw()
    {
        HWND hwnd = ::CreateWindowEx(WS_EX_CLIENTEDGE, WC_EDIT, NULL,
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN
            | ES_AUTOHSCROLL,
            0, 0, 0, 0,
            hwndParent,
            reinterpret_cast<HMENU>(id),
            HINST_THISCOMPONENT, NULL);
        if (hwnd != NULL) {
            NONCLIENTMETRICS ncm = { sizeof(ncm) };
            ::SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
            this->font = ::CreateFontIndirect(&ncm.lfStatusFont);
            SetWindowFont(hwnd, this->font, FALSE);
            ::SetWindowSubclass(hwnd, sWindowProc, 0, reinterpret_cast<DWORD_PTR>(this));
            Edit_SetCueBannerTextFocused(hwnd, L"Search", true);
        }
        return hwnd;
    }
};

class StatusBar
{
public:
    enum Parts
    {
        PART_STAT,
        PART_SEL,
        PART_ALL,
        PART_LAST,
    };

private:
    static LRESULT CALLBACK sWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR) throw()
    {
        LRESULT lr = ::DefSubclassProc(hwnd, message, wParam, lParam);
        switch (message) {
        case WM_DESTROY:
            ::RemoveWindowSubclass(hwnd, sWindowProc, 0);
            break;

        case WM_SIZE:
            OnSize(hwnd);
            break;
        }
        return lr;
    }

    static void OnSize(HWND hwnd) throw()
    {
        TEXTMETRIC tm;
        tm.tmAveCharWidth = 0;
        HDC hdc = ::GetDC(hwnd);
        if (hdc != NULL) {
            ::GetTextMetrics(hdc, &tm);
            ::ReleaseDC(hwnd, hdc);
        }
        RECT rect;
        ::GetClientRect(hwnd, &rect);
        const int cx = tm.tmAveCharWidth;
        int x = rect.right;
        int widths[PART_LAST];
        widths[PART_ALL] = x;
        x -= cx * 24;
        widths[PART_SEL] = x;
        x -= cx * 24;
        widths[PART_STAT] = x;
        ::SendMessage(hwnd, SB_SETPARTS, PART_LAST, reinterpret_cast<LPARAM>(widths));
    }

public:
    static HWND Create(HWND hwndParent, int id) throw()
    {
        HWND hwnd = ::CreateStatusWindow(WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | SBARS_SIZEGRIP, NULL, hwndParent, id);
        if (hwnd != NULL) {
            ::SetWindowSubclass(hwnd, sWindowProc, 0, 0);
        }
        return hwnd;
    }

    static bool SetText(HWND hwnd, Parts part, LPCWSTR text) throw()
    {
        return ::SendMessage(hwnd, SB_SETTEXTW, part, reinterpret_cast<LPARAM>(text)) != FALSE;
    }
};


template <class T, bool t_vertical>
class SplitterWindow
{
private:
    int splitterPos;
    int splitterDragPos;
    int splitterDragPosOld;
    int splitterDragHotspot;
    int splitterProportionalPos;
    bool splitterDragging;

    static const int SPLITTER_UNDEF = -0x77;
    static const int SPLITTER_MIN = 30;
    static const int SPLITTER_WIDTH = 2 * 2 + 1;
    static const int SPLITTER_MULTIPLIER = 32768;

    int CalcPos(HWND hwnd, int x, int y) throw()
    {
        const RECT rect = static_cast<T*>(this)->GetViewRect(hwnd);
        int desiredPos, minSize, maxSize;
        if (t_vertical) {
            desiredPos = x - rect.left - this->splitterDragHotspot + (SPLITTER_WIDTH / 2);
            minSize = SPLITTER_MIN;
            maxSize = RECTWIDTH(rect) - SPLITTER_MIN;
        } else {
            desiredPos = y - rect.top - this->splitterDragHotspot + (SPLITTER_WIDTH / 2);
            minSize = SPLITTER_MIN;
            maxSize = RECTHEIGHT(rect) - SPLITTER_MIN;
        }
        maxSize = __max(maxSize, minSize);
        return __max(__min(desiredPos, maxSize), minSize);
    }

    void DrawGhost(HDC hdc, const RECT& rect, int pos) throw()
    {
        int x, y, cx, cy;
        if (t_vertical) {
            x = rect.left + pos - (SPLITTER_WIDTH / 2);
            y = rect.top;
            cx = SPLITTER_WIDTH;
            cy = RECTHEIGHT(rect);
        } else {
            x = rect.left;
            y = rect.top + pos - (SPLITTER_WIDTH / 2);
            cx = RECTWIDTH(rect);
            cy = SPLITTER_WIDTH;
        }
        ::PatBlt(hdc, x, y, cx, cy, DSTINVERT);
    }

    void Draw(HWND hwnd, int splitterPos, int splitterPosOld) throw()
    {
        RECT rect = static_cast<T*>(this)->GetViewRect(hwnd);
        MapWindowRect(hwnd, HWND_DESKTOP, &rect);

        RECT windowRect = {};
        ::GetWindowRect(hwnd, &windowRect);
        ::OffsetRect(&rect, -windowRect.left, -windowRect.top);

        HDC hdc = ::GetWindowDC(hwnd);
        if (hdc != NULL) {
            if (splitterPosOld != SPLITTER_UNDEF) {
                this->DrawGhost(hdc, rect, splitterPosOld);
            }
            this->DrawGhost(hdc, rect, splitterPos);
            ::ReleaseDC(hwnd, hdc);
        }
    }

    inline static void SetRectLTWH(__out RECT* rect, int left, int top, int width, int height) throw()
    {
        rect->left = left;
        rect->top = top;
        rect->right = left + __max(0, width);
        rect->bottom = top + __max(0, height);
    }

    void ApplyProportionalPos(HWND hwnd) throw()
    {
        const RECT rect = static_cast<T*>(this)->GetViewRect(hwnd);
        if (this->splitterProportionalPos != SPLITTER_UNDEF) {
            int size, offset;
            if (t_vertical) {
                size = RECTWIDTH(rect);
                offset = rect.left;
            } else {
                size = RECTHEIGHT(rect);
                offset = rect.top;
            }
            const int newPos = offset + ::MulDiv(size, this->splitterProportionalPos, SPLITTER_MULTIPLIER);
            if (t_vertical) {
                this->splitterPos = this->CalcPos(hwnd, newPos, 0);
            } else {
                this->splitterPos = this->CalcPos(hwnd, 0, newPos);
            }
            static_cast<T*>(this)->SplitterRepositioned(hwnd);
        }
    }

    bool OnSetCursor(HWND hwnd, WPARAM wParam, LPARAM lParam, __out_opt LRESULT* result) throw()
    {
        if (reinterpret_cast<HWND>(wParam) == hwnd && LOWORD(lParam) == HTCLIENT) {
            const DWORD messagePos = ::GetMessagePos();
            const RECT rect = this->GetSplitterRect(hwnd);
            POINT pt = { GET_X_LPARAM(messagePos), GET_Y_LPARAM(messagePos) };
            ::MapWindowPoints(HWND_DESKTOP, hwnd, &pt, 1);
            if (::PtInRect(&rect, pt)) {
                HCURSOR hcur = ::LoadCursor(NULL, t_vertical ? IDC_SIZEWE : IDC_SIZENS);
                ::SetCursor(hcur);
                *result = TRUE;
                return true;
            }
        }
        return false;
    }

    bool OnSize(HWND hwnd, WPARAM, LPARAM, __out_opt LRESULT*) throw()
    {
        this->ApplyProportionalPos(hwnd);
        return true;
    }

    bool OnEnterSizeMove(HWND hwnd, WPARAM, LPARAM, __out_opt LRESULT*) throw()
    {
        const RECT rect = static_cast<T*>(this)->GetViewRect(hwnd);
        const int size = t_vertical ? RECTWIDTH(rect) : RECTHEIGHT(rect);
        if (size > 0) {
            this->splitterProportionalPos = ::MulDiv(this->splitterPos, SPLITTER_MULTIPLIER, size);
        } else {
            this->splitterProportionalPos = SPLITTER_UNDEF;
        }
        return true;
    }

    bool OnExitSizeMove(HWND, WPARAM, LPARAM, __out_opt LRESULT*) throw()
    {
        this->splitterProportionalPos = SPLITTER_UNDEF;
        return true;
    }

    bool OnLButtonDown(HWND hwnd, WPARAM, LPARAM lParam, __out_opt LRESULT*) throw()
    {
        const RECT rect = this->GetSplitterRect(hwnd);
        const POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        if (::PtInRect(&rect, pt)) {
            if (t_vertical) {
                this->splitterDragHotspot = pt.x - rect.left;
            } else {
                this->splitterDragHotspot = pt.y - rect.top;
            }
            const int pos = this->CalcPos(hwnd, pt.x, pt.y);
            this->splitterDragPos = pos;
            this->splitterDragPosOld = pos;
            this->splitterDragging = true;
            this->Draw(hwnd, pos, SPLITTER_UNDEF);
            static_cast<T*>(this)->SplitterRepositioned(hwnd);
            ::SetCapture(hwnd);
            return true;
        }
        return false;
    }

    bool OnLButtonUp(HWND, WPARAM, LPARAM, __out_opt LRESULT*) throw()
    {
        if (this->splitterDragging) {
            ::ReleaseCapture();
            return true;
        }
        return false;
    }

    bool OnMouseMove(HWND hwnd, WPARAM, LPARAM lParam, __out_opt LRESULT*) throw()
    {
        if (this->splitterDragging) {
            const POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            const int pos = this->CalcPos(hwnd, pt.x, pt.y);
            if (this->splitterDragPos != pos) {
                this->splitterDragPos = pos;
                if (this->splitterDragPos != this->splitterDragPosOld) {
                    this->Draw(hwnd, this->splitterDragPos, this->splitterDragPosOld);
                    this->splitterDragPosOld = this->splitterDragPos;
                    static_cast<T*>(this)->SplitterRepositioned(hwnd);
                }
            }
            return true;
        }
        return false;
    }

    bool OnCaptureChanged(HWND hwnd, WPARAM, LPARAM, __out_opt LRESULT*) throw()
    {
        if (::GetCapture() != hwnd && this->splitterDragging) {
            if (this->splitterDragPos != SPLITTER_UNDEF) {
                this->Draw(hwnd, this->splitterDragPos, SPLITTER_UNDEF);
                if (this->splitterPos != this->splitterDragPos) {
                    this->splitterPos = this->splitterDragPos;
                    static_cast<T*>(this)->SplitterRepositioned(hwnd);
                }
                this->splitterDragPos = SPLITTER_UNDEF;
            }
            this->splitterDragHotspot = 0;
            this->splitterDragging = false;
        }
        return true;
    }

protected:
    SplitterWindow() throw()
        : splitterPos()
        , splitterDragPos(SPLITTER_UNDEF)
        , splitterDragPosOld(SPLITTER_UNDEF)
        , splitterDragHotspot()
        , splitterProportionalPos(SPLITTER_UNDEF)
        , splitterDragging()
    {
    }

    ~SplitterWindow() throw()
    {
    }

    RECT GetViewRect(HWND hwnd) throw()
    {
        RECT rect = {};
        return rect;
    }

    void SplitterRepositioned(HWND hwnd) throw()
    {
    }

    void InitSplitter(HWND hwnd, int proportionalPos) throw()
    {
        const RECT rect = static_cast<T*>(this)->GetViewRect(hwnd);
        int cxy = t_vertical ? RECTWIDTH(rect) : RECTHEIGHT(rect);
        this->splitterPos = ::MulDiv(cxy, proportionalPos, 100);
        this->splitterDragPos = SPLITTER_UNDEF;
        this->splitterDragPosOld = SPLITTER_UNDEF;
        this->splitterProportionalPos = SPLITTER_UNDEF;
    }

    RECT GetSplitterRect(HWND hwnd, bool drag = false) throw()
    {
        const RECT rect = static_cast<T*>(this)->GetViewRect(hwnd);
        int splitterPos;
        if (drag && this->splitterDragging) {
            splitterPos = this->splitterDragPos;
        } else {
            splitterPos = this->splitterPos;
        }
        RECT splitterRect;
        if (t_vertical) {
            SetRectLTWH(&splitterRect,
                rect.left + splitterPos - (SPLITTER_WIDTH / 2),
                rect.top,
                SPLITTER_WIDTH,
                RECTHEIGHT(rect));
        } else {
            SetRectLTWH(&splitterRect,
                rect.left,
                rect.top + splitterPos - (SPLITTER_WIDTH / 2),
                RECTWIDTH(rect),
                SPLITTER_WIDTH);
        }
        return splitterRect;
    }

    void GetChildRects(HWND hwnd, __out_ecount(2) RECT (&childRects)[2]) throw()
    {
        const RECT rect = static_cast<T*>(this)->GetViewRect(hwnd);
        const int splitterPos = this->splitterPos;
        if (t_vertical) {
            const int size = RECTHEIGHT(rect);
            SetRectLTWH(&childRects[0],
                rect.left,
                rect.top,
                splitterPos - (SPLITTER_WIDTH / 2),
                size);
            SetRectLTWH(&childRects[1],
                rect.left + splitterPos + (SPLITTER_WIDTH / 2),
                rect.top,
                RECTWIDTH(rect) - splitterPos - (SPLITTER_WIDTH / 2),
                size);
        } else {
            const int size = RECTWIDTH(rect);
            SetRectLTWH(&childRects[0],
                rect.left,
                rect.top,
                size,
                splitterPos - (SPLITTER_WIDTH / 2));
            SetRectLTWH(&childRects[1],
                rect.left,
                rect.top + splitterPos + (SPLITTER_WIDTH / 2),
                size,
                RECTHEIGHT(rect) - splitterPos - (SPLITTER_WIDTH / 2));
        }
    }

    bool HandleWindowMessages(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, __out LRESULT* result) throw()
    {
        *result = 0;
        switch (message) {
        case WM_SETCURSOR:
            return this->OnSetCursor(hwnd, wParam, lParam, result);
        case WM_SIZE:
            return this->OnSize(hwnd, wParam, lParam, result);
        case WM_ENTERSIZEMOVE:
            return this->OnEnterSizeMove(hwnd, wParam, lParam, result);
        case WM_EXITSIZEMOVE:
            return this->OnExitSizeMove(hwnd, wParam, lParam, result);
        case WM_LBUTTONDOWN:
            return this->OnLButtonDown(hwnd, wParam, lParam, result);
        case WM_LBUTTONUP:
            return this->OnLButtonUp(hwnd, wParam, lParam, result);
        case WM_MOUSEMOVE:
            return this->OnMouseMove(hwnd, wParam, lParam, result);
        case WM_CAPTURECHANGED:
            return this->OnCaptureChanged(hwnd, wParam, lParam, result);
        }
        return false;
    }
};


#include "res.rc"

enum ControlID
{
    IDC_LISTVIEW = 1,
    IDC_STATUSBAR,
    IDC_SEARCH,
    IDC_CONTENT,
    IDC_LAST,
};

class MainWindow : public BaseWindow<MainWindow>, public SplitterWindow<MainWindow, false>, public IMessageSink
{
private:
    friend BaseWindow<MainWindow>;
    friend SplitterWindow<MainWindow, false>;

    typedef BaseWindow<MainWindow> base;
    typedef SplitterWindow<MainWindow, false> splitter;

    typedef PtrArray<MailMessage, 16, TPtrArrayAutoDeleteHelper<MailMessage>> MailMessageArray;
    typedef PtrArray<MailMessage, 16, TPtrArrayDestroyHelper<MailMessage>> FilteredMailMessageArray;

    enum ImageIndex
    {
        MAIL_UNREAD = 0,
        MAIL_READ = 1,
        MAIL_ATTACH = 2,
        MAIL_ERROR = 3,
        MAIL_WARN = 4,
    };

    SearchEdit search;
    HWND hwnd;
    HWND searchEdit;
    HWND listView;
    HWND status;
    HWND contentView;
    HMENU popup;
    HFONT monoFont;
    bool shown;
    SockerListener listener;
    MailMessageArray messages;
    FilteredMailMessageArray filter;
    INT ctlArray[2 * IDC_LAST];

    static const int SPLITTER_HEIGHT = 3;

    HRESULT InitView(HWND hwnd) throw()
    {
        HRESULT hr = S_OK;

        StatusBar::SetText(this->status, StatusBar::PART_STAT, L"Initialising...");

        LOGFONTW lf = {};
        HDC hdc = ::GetDC(hwnd);
        lf.lfHeight = -::MulDiv(21, ::GetDeviceCaps(hdc, LOGPIXELSY), 144); // 10.5 pt
        ::ReleaseDC(hwnd, hdc);
        lf.lfWidth = 0;
        lf.lfWeight = 0;
        lf.lfCharSet = ANSI_CHARSET;
        lf.lfQuality = DEFAULT_QUALITY;
        lf.lfPitchAndFamily = FIXED_PITCH;
        ::StringCchCopyW(lf.lfFaceName, LF_FACESIZE, L"Consolas");
        this->monoFont = ::CreateFontIndirect(&lf);
        SetWindowFont(this->contentView, this->monoFont, false);

        PCZZTSTR colNames = _T("Subject\0Sender\0Recipient\0Time\0Size\0");
        int colWidths[] = { 32, 20, 20, 20, 8 };

        RECT rect = {};
        ::GetClientRect(this->listView, &rect);
        if ((GetWindowStyle(this->listView) & WS_VSCROLL) == 0) {
            rect.right -= ::GetSystemMetrics(SM_CXVSCROLL);
            if (rect.right < 0) {
                rect.right = 0;
            }
        }

        int i;
        for (i = 0; *colNames != 0; i++) {
            LVCOLUMN lvcol;
            lvcol.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
            lvcol.fmt = i == 4 ? LVCFMT_RIGHT : LVCFMT_LEFT;
            lvcol.cx = ::MulDiv(colWidths[i], rect.right, 100);
            lvcol.pszText = (LPTSTR)(colNames);
            lvcol.cchTextMax = 0;
            lvcol.iSubItem = i;
            ListView_InsertColumn(this->listView, i, &lvcol);
            if (*colNames != 0) {
                colNames += _tcslen(colNames) + 1;
            }
        }

        HWND header = ListView_GetHeader(this->listView);
        if (header != NULL) {
            for (int i = 0; ; i++) {
                HDITEM item = { HDI_FORMAT };
                if (Header_GetItem(header, i, &item) == FALSE) {
                    break;
                }
                item.fmt &= ~(HDF_SORTUP | HDF_SORTDOWN);
                if (i == 3) {
                    item.fmt |= HDF_SORTDOWN;
                }
                Header_SetItem(header, i, &item);
            }
        }
        ListView_SetSelectedColumn(this->listView, 3);

        const UINT16 port = 1025;
        if (this->listener.Listen(port) == false) {
            ::PostMessage(hwnd, WM_CLOSE, 0, 0);
            return E_FAIL;
        }

        HIMAGELIST himl = ::ImageList_LoadImage(HINST_THISCOMPONENT, MAKEINTRESOURCE(IDB_MAILICONS), 16, 0, 0, IMAGE_BITMAP, LR_CREATEDIBSECTION);
        ListView_SetImageList(this->listView, himl, LVSIL_SMALL);

        this->PopulateItems(hwnd);

        StatusBar::SetText(this->status, StatusBar::PART_STAT, NULL);
        return hr;
    }

    // called by SplitterWindow
    RECT GetViewRect(HWND hwnd) throw()
    {
        RECT rect = {};
        ::GetEffectiveClientRect(hwnd, &rect, this->ctlArray);
        return rect;
    }

    void SplitterRepositioned(HWND hwnd) throw()
    {
        this->UpdateLayout(hwnd);
        ::RedrawWindow(hwnd, NULL, NULL, RDW_ERASE);
    }

    void PopulateItems(HWND hwnd) throw()
    {
        // wchar_t text[256];
        // Edit_GetText(this->searchEdit, text, _countof(text));
        // wchar_t* query = text[0] != L'\0' ? text : NULL;
        char text[256];
        ::GetWindowTextA(this->searchEdit, text, _countof(text));
        const char* query = text[0] != '\0' ? text : NULL;

        int index = ListView_GetNextItem(this->listView, -1, LVNI_SELECTED | LVNI_FOCUSED);
        if (index < 0) {
            index = ListView_GetNextItem(this->listView, -1, LVNI_SELECTED);
        }
        MailMessage* selectedMessage = this->filter.GetAt(index);
        int selectedIndex = -1;

        this->filter.RemoveAll();

        // const bool regex = query != NULL && ::wcspbrk(query, L"^$.?*+") != NULL;
        for (int n = this->messages.GetCount(); --n >= 0; ) {
            MailMessage* message = this->messages.FastGetAt(n);
            bool matches = true;
            if (query != NULL) {
                // const wchar_t* matched = regex ? ::match(name, query) : ::StrStrIW(name, query);
                const char* found;
                found = ::StrStrIA(message->GetSender(), query);
                if (found == NULL) {
                    found = ::StrStrIA(message->GetRecipient(), query);
                    if (found == NULL) {
                        found = ::StrStrIA(message->GetBody(), query);
                        if (found == NULL) {
                            matches = false;
                        }
                    }
                }
            }
            if (matches) {
                if (selectedMessage == message) {
                    selectedIndex = this->filter.GetCount();
                }
                this->filter.Add(message);
            }
        }

        const int listCount = this->filter.GetCount();
        SetWindowRedraw(this->listView, false);
        ListView_SetItemCount(this->listView, listCount);
        if (selectedIndex >= 0) {
            ListView_SetItemState(this->listView, selectedIndex, LVNI_SELECTED | LVNI_FOCUSED, LVNI_SELECTED | LVNI_FOCUSED);
            ListView_EnsureVisible(this->listView, selectedIndex, false);
        } else {
            ListView_SetItemState(this->listView, 0, LVIS_FOCUSED, LVIS_FOCUSED);
            ListView_EnsureVisible(this->listView, 0, false);
        }
        SetWindowRedraw(this->listView, true);

        wchar_t buffer[64];
        ::StringCchPrintfW(buffer, _countof(buffer), L"%d emails", listCount);
        StatusBar::SetText(this->status, StatusBar::PART_ALL, buffer);
        this->UpdateStatus(hwnd);
    }

    void UpdateStatus(HWND) throw()
    {
        UINT count = ListView_GetSelectedCount(this->listView);
        wchar_t text[64];
        if (count > 0) {
            ::StringCchPrintfW(text, _countof(text), L"%d selected", count);
        } else {
            *text = L'\0';
        }
        int index = ListView_GetNextItem(this->listView, -1, LVNI_SELECTED | LVNI_FOCUSED);
        if (index < 0) {
            index = ListView_GetNextItem(this->listView, -1, LVNI_SELECTED);
        }
        MailMessage* message = this->filter.GetAt(index);
        SetWindowRedraw(this->contentView, false);
        if (message != NULL) {
            ::SetWindowTextA(this->contentView, message->GetBody());
        } else {
            ::SetWindowTextA(this->contentView, NULL);
        }
        SetWindowRedraw(this->contentView, true);
        StatusBar::SetText(this->status, StatusBar::PART_SEL, text);
    }

    void UpdateLayout(HWND hwnd) throw()
    {
        ::SendMessage(this->searchEdit, WM_SIZE, 0, 0);
        ::SendMessage(this->status, WM_SIZE, 0, 0);

        RECT rects[2];
        splitter::GetChildRects(hwnd, rects);
        ::MoveWindow(this->listView, rects[0].left, rects[0].top, RECTWIDTH(rects[0]), RECTHEIGHT(rects[0]), TRUE);
        ::MoveWindow(this->contentView, rects[1].left, rects[1].top, RECTWIDTH(rects[1]), RECTHEIGHT(rects[1]), TRUE);
    }

    LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) throw()
    {
        LRESULT lRet;
        bool handled = splitter::HandleWindowMessages(hwnd, message, wParam, lParam, &lRet);

        switch (message) {
        HANDLE_MSG(hwnd, WM_CREATE, OnCreate);
        HANDLE_MSG(hwnd, WM_DESTROY, OnDestroy);
        HANDLE_MSG(hwnd, WM_COMMAND, OnCommand);
        HANDLE_MSG(hwnd, WM_ERASEBKGND, OnEraseBackground);
        HANDLE_MSG(hwnd, WM_CTLCOLORSTATIC, OnCtlColorStatic);
        HANDLE_MSG(hwnd, WM_WINDOWPOSCHANGED, OnWindowPosChanged);
        HANDLE_MSG(hwnd, WM_SETTINGCHANGE, OnSettingChange);
        HANDLE_MSG(hwnd, WM_ACTIVATE, OnActivate);
        HANDLE_MSG(hwnd, WM_NOTIFY, OnNotify);
        HANDLE_MSG(hwnd, WM_INITMENU, OnInitMenu);
        HANDLE_MSG(hwnd, WM_INITMENUPOPUP, OnInitMenuPopup);
        HANDLE_MSG(hwnd, WM_CONTEXTMENU, OnContextMenu);
        }

        if (handled) {
            return lRet;
        }
        return ::DefWindowProc(hwnd, message, wParam, lParam);
    }

    bool OnCreate(HWND hwnd, LPCREATESTRUCT lpcs) throw()
    {
        if (lpcs == NULL) {
            return false;
        }
        this->hwnd = hwnd;
        if (this->messages.Create() == false) {
            return false;
        }
        if (this->filter.Create() == false) {
            return false;
        }
        this->searchEdit = this->search.Create(hwnd, IDC_SEARCH);
        if (this->searchEdit == NULL) {
            return false;
        }
        this->listView = ::CreateWindowEx(0, WC_LISTVIEW, NULL,
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN
            | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_AUTOARRANGE | LVS_ALIGNTOP | LVS_OWNERDATA,
            0, 0, 0, 0,
            hwnd,
            reinterpret_cast<HMENU>(IntToPtr(IDC_LISTVIEW)),
            HINST_THISCOMPONENT, NULL);
        if (this->listView == NULL) {
            return false;
        }
        this->contentView = ::CreateWindowEx(0, WC_EDIT, NULL,
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN
            | ES_MULTILINE | ES_NOHIDESEL | ES_READONLY | WS_VSCROLL,
            0, 0, 0, 0,
            hwnd,
            reinterpret_cast<HMENU>(IntToPtr(IDC_CONTENT)),
            HINST_THISCOMPONENT, NULL);
        if (this->contentView == NULL) {
            return false;
        }
        this->status = StatusBar::Create(hwnd, IDC_STATUSBAR);
        if (this->status == NULL) {
            return false;
        }
        HMENU hm = ::GetMenu(hwnd);
        this->popup = GetSubMenuById(hm, IDM_POPUP);
        ::RemoveMenu(hm, IDM_POPUP, MF_BYCOMMAND);

        this->ctlArray[0] = -1;
        this->ctlArray[1] = PtrToInt(hm);
        this->ctlArray[2] = -1;
        this->ctlArray[3] = IDC_SEARCH;
        this->ctlArray[4] = IDM_STATUSBAR;
        this->ctlArray[5] = IDC_STATUSBAR;
        this->ctlArray[6] = 0;
        this->ctlArray[7] = 0;

        splitter::InitSplitter(hwnd, 33);

        ::SetWindowTheme(this->listView, L"Explorer", NULL);
        ListView_SetExtendedListViewStyle(this->listView, LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP | LVS_EX_UNDERLINEHOT | LVS_EX_DOUBLEBUFFER | LVS_EX_JUSTIFYCOLUMNS | LVS_EX_AUTOSIZECOLUMNS | LVS_EX_COLUMNSNAPPOINTS);

        this->UpdateLayout(hwnd);
        return true;
    }

    void OnDestroy(HWND) throw()
    {
        this->listener.Close();
        ::PostQuitMessage(0);
    }

    void OnCommand(HWND hwnd, int id, HWND, UINT code) throw()
    {
        switch (id) {
        case IDC_SEARCH:
            this->OnCommandEdit(hwnd, id, code);
            break;
        case IDM_COPY_NAME:
        case IDA_COPY_NAME:
            this->OnCommandCopyName(hwnd, id);
            break;
        case IDM_CLEAR:
            this->OnCommandClear(hwnd, id);
            break;
        case IDM_EXIT_APP:
            this->OnCommandExitApp(hwnd, id);
            break;
        case IDM_STATUSBAR:
            this->OnCommandStatusBar(hwnd, id);
            break;
        case IDM_OPTIONS:
            this->OnCommandOptions(hwnd, id);
            break;
        case IDA_FOCUS_EDIT:
            this->OnCommandFocusEdit(hwnd, id);
            break;
        case IDA_PREV_CONTROL:
        case IDA_NEXT_CONTROL:
            this->OnCommandFocusControl(hwnd, id);
            break;
        }
    }

    void OnCommandEdit(HWND hwnd, int, UINT code) throw()
    {
        if (code == EN_CHANGE) {
            this->PopulateItems(hwnd);
        }
    }

    void OnCommandCopyName(HWND, int id) throw()
    {
        if (id == IDA_COPY_NAME && ::GetFocus() != this->listView) {
            return;
        }

    }

    void OnCommandClear(HWND hwnd, int) throw()
    {
        this->messages.RemoveAll();
        this->PopulateItems(hwnd);
    }

    void OnWindowPosChanged(HWND hwnd, const WINDOWPOS* pwp) throw()
    {
        if (pwp == NULL) {
            return;
        }
        if (pwp->flags & SWP_SHOWWINDOW) {
            if (this->shown == false) {
                this->InitView(hwnd);
                this->shown = true;
            }
        }
        if ((pwp->flags & SWP_NOSIZE) == 0) {
            this->UpdateLayout(hwnd);
        }
    }

    void OnCommandSave(HWND, int) throw()
    {
        const UINT cSelected = ListView_GetSelectedCount(this->listView);
        if (cSelected == 0) {
            return;
        }
    }

    void OnCommandExitApp(HWND hwnd, int) throw()
    {
        ::SendMessage(hwnd, WM_CLOSE, 0, 0);
    }

    void OnCommandStatusBar(HWND hwnd, int id) throw()
    {
        ::ShowHideMenuCtl(hwnd, id, this->ctlArray);
        this->UpdateLayout(hwnd);
    }

    void OnCommandOptions(HWND hwnd, int) throw()
    {
        // TODO: Open an options dialog instead of a font dialog
        LOGFONT lf = {};
        ::GetObject(this->monoFont, sizeof(lf), &lf);
        CHOOSEFONT cf = { sizeof(cf) };
        cf.hwndOwner = hwnd;
        cf.lpLogFont = &lf;
        cf.Flags = CF_TTONLY | CF_FIXEDPITCHONLY | CF_INITTOLOGFONTSTRUCT;
        if (::ChooseFont(&cf)) {
            HFONT font = ::CreateFontIndirect(&lf);
            SetWindowFont(this->contentView, font, true);
            DeleteFont(this->monoFont);
            this->monoFont = font;
        }
    }

    void OnCommandFocusEdit(HWND, int) throw()
    {
        Edit_SetSel(this->searchEdit, 0, -1);
        ::SetFocus(this->searchEdit);
    }

    void OnCommandFocusControl(HWND, int id) throw()
    {
        HWND hwnd = ::GetFocus();
        HWND children[] = { this->searchEdit, this->listView, this->contentView };
        int delta = id == IDA_PREV_CONTROL ? _countof(children) - 1 : 1;
        for (int i = 0; i < _countof(children); i++) {
            if (children[i] == hwnd) {
                hwnd = children[(i + delta) % _countof(children)];
                ::SetFocus(hwnd);
                break;
            }
        }
    }

    inline static COLORREF half_color(COLORREF clr) throw()
    {
        return ((clr >> 1) & 0x7F7F7F);
    }

    static COLORREF BlendColor_1_1(COLORREF clr1, COLORREF clr2) throw()
    {
        // clr1 / 2 + clr2 / 2
        return half_color(clr1) + half_color(clr2);
    }

    BOOL OnEraseBackground(HWND hwnd, HDC hdc) throw()
    {
        const RECT rect = splitter::GetSplitterRect(hwnd);
        HBRUSH oldbrush = SelectBrush(hdc, GetStockBrush(DC_BRUSH));
        COLORREF oldcolour = ::SetDCBrushColor(hdc, ::GetSysColor(COLOR_WINDOW));
        ::PatBlt(hdc, rect.left, rect.top, RECTWIDTH(rect), RECTHEIGHT(rect), PATCOPY);
        COLORREF colour = BlendColor_1_1(::GetSysColor(COLOR_WINDOW), ::GetSysColor(COLOR_BTNFACE));
        ::SetDCBrushColor(hdc, colour);
        ::PatBlt(hdc, rect.left, rect.top + RECTHEIGHT(rect) / 2, RECTWIDTH(rect), 1, PATCOPY);
        ::SetDCBrushColor(hdc, oldcolour);
        SelectBrush(hdc, oldbrush);
        return true;
    }

    HBRUSH OnCtlColorStatic(HWND, HDC, HWND, int) throw()
    {
        return ::GetSysColorBrush(COLOR_WINDOW);
    }

    void OnSettingChange(HWND hwnd, UINT, LPCTSTR) throw()
    {
        this->UpdateLayout(hwnd);
    }

    void OnActivate(HWND, UINT state, HWND, BOOL) throw()
    {
        if (state != WA_INACTIVE) {
            ::SendMessage(this->listView, WM_CHANGEUISTATE, UIS_INITIALIZE, 0);
            ::SetFocus(this->searchEdit);
        }
    }

    LRESULT OnNotify(HWND hwnd, int idFrom, NMHDR* pnmhdr) throw()
    {
        if (idFrom != IDC_LISTVIEW || pnmhdr == NULL) {
            return 0;
        }

        switch (pnmhdr->code) {
        case LVN_ITEMCHANGED:
            return this->OnNotifyListViewItemChanged(hwnd, CONTAINING_RECORD(pnmhdr, NMLISTVIEW, hdr));
        case LVN_GETDISPINFOW:
            return this->OnNotifyListViewGetDispInfo(hwnd, CONTAINING_RECORD(pnmhdr, NMLVDISPINFOW, hdr));
        case LVN_GETEMPTYMARKUP:
            return this->OnGetEmptyMarkUp(hwnd, CONTAINING_RECORD(pnmhdr, NMLVEMPTYMARKUP, hdr));
        }
        return 0;
    }

    LRESULT OnNotifyListViewItemChanged(HWND hwnd, NMLISTVIEW* lplv) throw()
    {
        if ((lplv->uChanged & LVIF_STATE) != 0) {
            if (((lplv->uNewState ^ lplv->uOldState) & LVIS_SELECTED) == LVIS_SELECTED) {
                MailMessage* message = this->filter.GetAt(lplv->iItem);
                if (message != NULL && message->SetRead() == false) {
                    ListView_Update(this->listView, lplv->iItem);
                }
                this->UpdateStatus(hwnd);
            }
        }
        return 0;
    }

    LRESULT OnNotifyListViewGetDispInfo(HWND, NMLVDISPINFOW* lplvdi) throw()
    {
        MailMessage* message = this->filter.GetAt(lplvdi->item.iItem);
        if (message == NULL) {
            return 0;
        }
        const UINT CP_ASCII = 20127;
        if (lplvdi->item.mask & LVIF_TEXT) {
            switch (lplvdi->item.iSubItem) {
            case 0:
                ::StringCchCopyW(lplvdi->item.pszText, lplvdi->item.cchTextMax, message->GetSubject());
                break;
            case 1:
                ::MultiByteToWideChar(CP_ASCII, 0, message->GetSender(), -1, lplvdi->item.pszText, lplvdi->item.cchTextMax);
                break;
            case 2:
                ::MultiByteToWideChar(CP_ASCII, 0, message->GetRecipient(), -1, lplvdi->item.pszText, lplvdi->item.cchTextMax);
                break;
            case 3:
                {
                    FILETIME ft = message->GetTime();
                    SYSTEMTIME st, loc;
                    ::FileTimeToSystemTime(&ft, &st);
                    ::SystemTimeToTzSpecificLocalTime(NULL, &st, &loc);
                    int ret = ::GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &loc, NULL, lplvdi->item.pszText, lplvdi->item.cchTextMax);
                    if (ret > 0) {
                        lplvdi->item.pszText[ret - 1] = _T(' ');
                        lplvdi->item.pszText[ret] = 0;
                        ::GetTimeFormat(LOCALE_USER_DEFAULT, 0/*TIME_FORCE24HOURFORMAT*/, &loc, NULL, lplvdi->item.pszText + ret, lplvdi->item.cchTextMax - ret);
                    }
                }
                break;
            case 4:
                ::StrFormatKBSizeW(::strlen(message->GetBody()), lplvdi->item.pszText, lplvdi->item.cchTextMax);
                break;
            }
        }
        if (lplvdi->item.mask & LVIF_IMAGE) {
            if (message->IsRead()) {
                lplvdi->item.iImage = MAIL_READ;
            } else {
                lplvdi->item.iImage = MAIL_UNREAD;
            }
        }
        // lplvdi->item.mask |= LVIF_DI_SETITEM;
        return 0;
    }

    LRESULT OnGetEmptyMarkUp(HWND, NMLVEMPTYMARKUP* lplvem) throw()
    {
        lplvem->dwFlags = EMF_CENTERED;
        if (::LoadString(HINST_THISCOMPONENT, IDS_NOEMAILS, lplvem->szMarkup, _countof(lplvem->szMarkup)) <= 0) {
            return false;
        }
        return true;
    }

    void OnInitMenu(HWND, HMENU) throw()
    {
        // UINT count = ListView_GetSelectedCount(this->listView);
        // UINT flags = MF_BYCOMMAND | (count > 0 ? MF_ENABLED : MF_GRAYED);
        // ::EnableMenuItem(hMenu, IDM_COPY_NAME, flags);
    }

    void OnInitMenuPopup(HWND, HMENU, UINT, BOOL) throw()
    {
    }

    void OnContextMenu(HWND hwnd, HWND hwndContext, int xPos, int yPos) throw()
    {
        if (hwndContext != this->listView) {
            FORWARD_WM_CONTEXTMENU(hwnd, hwndContext, xPos, yPos, ::DefWindowProc);
            return;
        }
        int iItem = ListView_GetNextItem(this->listView, -1, LVNI_SELECTED | LVNI_FOCUSED);
        if (iItem < 0) {
            return;
        }
        POINT pt = { xPos, yPos };
        if (MAKELONG(pt.x, pt.y) == -1) {
            ListView_EnsureVisible(this->listView, iItem, FALSE);
            RECT rcItem = {};
            ListView_GetItemRect(this->listView, iItem, &rcItem, LVIR_ICON);
            pt.x = rcItem.left + (rcItem.right - rcItem.left) / 2;
            pt.y = rcItem.top  + (rcItem.bottom - rcItem.top) / 2;
            ::ClientToScreen(this->listView, &pt);
        }
        ::TrackPopupMenuEx(this->popup, TPM_RIGHTBUTTON | TPM_LEFTBUTTON, pt.x, pt.y, hwnd, NULL);
    }

protected:
    virtual void GotMail(MailMessage* message)
    {
        if (this->messages.Add(message) == -1) {
            delete message;
        }
        this->PopulateItems(this->hwnd);
    }

public:
    MainWindow() throw()
        : hwnd()
        , searchEdit()
        , listView()
        , status()
        , contentView()
        , popup()
        , monoFont()
        , shown()
        , listener(this)
    {
    }

    ~MainWindow() throw()
    {
        if (this->popup != NULL) {
            ::DestroyMenu(this->popup);
        }
        if (this->monoFont != NULL) {
            DeleteFont(this->monoFont);
        }
    }

    HWND Create() throw()
    {
        ::InitCommonControls();
        WNDCLASS wc = {
            0, NULL, 0, 0,
            HINST_THISCOMPONENT,
            ::LoadIcon(HINST_THISCOMPONENT, MAKEINTRESOURCE(IDR_MAIN)),
            NULL,
            0, MAKEINTRESOURCE(IDR_MAIN),
            MAKEINTATOM(777) };
        const ATOM atom = base::Register(&wc);
        if (atom == 0) {
            return NULL;
        }
        return base::Create(atom, L"Grabamail");
    }
};

class Console
{
private:
    BOOL alloc;

public:
    Console() throw()
        : alloc()
    {
    }
    ~Console() throw()
    {
        this->Free();
    }
    BOOL Allocate() throw()
    {
        if (this->alloc) {
            return FALSE;
        }
        this->alloc = ::AllocConsole();
        return this->alloc;
    }
    void Free() throw()
    {
        if (this->alloc) {
            ::FreeConsole();
            this->alloc = false;
        }
    }
};

#include "option.h"

int WINAPI _tWinMain(HINSTANCE, HINSTANCE, LPTSTR, int)
{
#ifdef USETRACE
    Console con;
#endif

    option opt;
    int ch;
    while ((ch = opt.getopt(__argc, __wargv, L"d")) != -1) {
        switch (ch) {
        case 'd':
#ifdef USETRACE
            con.Allocate();
#endif
            break;
        }
    }

    WSAInitialiser init(MAKEWORD(2, 2));
    if (init == SOCKET_ERROR) {
        serror("WSAStartup");
        return EXIT_FAILURE;
    }

    MainWindow wnd;
    HWND hwnd = wnd.Create();
    if (hwnd == NULL) {
        return EXIT_FAILURE;
    }

    MSG msg;
    msg.wParam = EXIT_FAILURE;

    HACCEL hacc = ::LoadAccelerators(HINST_THISCOMPONENT, MAKEINTRESOURCE(IDR_MAIN));
    while (::GetMessage(&msg, NULL, 0, 0)) {
        if (hacc != NULL && (hwnd == msg.hwnd || ::IsChild(hwnd, msg.hwnd))) {
            if (::TranslateAccelerator(hwnd, hacc, &msg)) {
                continue;
            }
        }
        ::DispatchMessage(&msg);
        ::TranslateMessage(&msg);
    }
    return static_cast<int>(msg.wParam);
}
