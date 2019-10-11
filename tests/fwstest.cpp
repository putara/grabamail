#if 0
fwstest.exe: fwstest.obj
 link /nologo /dynamicbase:no /machine:x86 /map /merge:.rdata=.text /nxcompat /opt:icf /opt:ref /release /out:fwstest.exe /debug /PDBALTPATH:"%_PDB%" fwstest.obj kernel32.lib
 @-erase fwstest.obj > nul 2> nul
 @-erase vc??.pdb > nul 2> nul

fwstest.obj: fwstest.cpp
 cl /nologo /c /GF /GR- /Gy /MD /O1ib1 /W3 /Zi /D "NDEBUG" /D "_WINDOWS" /D "_UNICODE" /D "UNICODE" fwstest.cpp

!IF 0
#else

#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <intrin.h>

size_t __cdecl my_strnlen(__in_ecount(cchMax) LPCSTR p, size_t cchMax)
{
    size_t n;
    for (n = 0; n < cchMax && *p; n++, p++);
    return n;
}

char* escape_string(const char* src)
{
    char* const dst = static_cast<char*>(::malloc(::strlen(src) * 2 + 1));
    char* p = dst;
    while (*src != '\0') {
        char ch = '\0';
        switch (*src) {
        case '\r': ch = 'r'; break;
        case '\n': ch = 'n'; break;
        case '\t': ch = 't'; break;
        case '\"': ch = '"'; break;
        case '\\': ch = '\\'; break;
        }
        if (ch != '\0') {
            *p++ = '\\';
            *p++ = ch;
            src++;
        } else {
            *p++ = *src++;
        }
    }
    *p = '\0';
    return dst;
}

void assert_equals(const char* input, const char* expected)
{
    if (strcmp(input, expected) != 0) {
        char* p = escape_string(input);
        char* q = escape_string(expected);
        printf("--- Failure ---\n- %s\n+ %s\n", p, q);
        free(p);
        free(q);
        exit(1);
    }
}

void test(const char* input, const char* expected)
{
}

int wmain(int, wchar_t**)
{
    test("Subject: This\r\n is a test", "Subject: This is a test");
    return 0;
}

#if defined(_MSC_VER) && defined(_DLL)
#include <process.h>
#define _CONSOLE_APP 1
#pragma comment(linker, "/entry:EntryPoint /subsystem:console")
EXTERN_C _CRTIMP void __cdecl __set_app_type(int);
EXTERN_C _CRTIMP void __cdecl __wgetmainargs(int*, wchar_t***, wchar_t***, int, int*);

__declspec(noinline) void IEntryPoint()
{
    int argc;
    wchar_t** argv;
    wchar_t** envp;
    int startinfo_newmode;
    __set_app_type(_CONSOLE_APP);
    startinfo_newmode = 0;
    __wgetmainargs(&argc, &argv, &envp, 0, &startinfo_newmode);
    exit(wmain(argc, argv));
}

void EntryPoint()
{
    __security_init_cookie();
    IEntryPoint();
}
#endif

#endif /*
!ENDIF
#*/
