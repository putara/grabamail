#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <intrin.h>

#include "unittest.h"
#include "fldtest.h"
#include "datetest.h"

int wmain(int, wchar_t**)
{
    TestFieldParser().Run();
    TestDateTime().Run();
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
