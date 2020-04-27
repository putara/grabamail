#include <TestHarness.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <intrin.h>

size_t __cdecl my_strnlen(__in_ecount(cchMax) LPCSTR p, size_t cchMax)
{
    size_t n;
    for (n = 0; n < cchMax && *p; n++, p++);
    return n;
}

bool IsWhiteSpace(char ch)
{
    return ch == '\t' || ch == ' ';
}

const char* SkipWhiteSpace(__in const char* string)
{
    while (IsWhiteSpace(*string)) {
        string++;
    }
    return string;
}

char* SkipWhiteSpace(__in char* string)
{
    return const_cast<char*>(SkipWhiteSpace(const_cast<const char*>(string)));
}

char* ParseUnstructured(__inout char* string)
{
    string = SkipWhiteSpace(string);
    if (*string == 0) {
        return string;
    }
    const char* src = string;
    char* dst = string;

    while (*src != 0) {
        if (src[0] == '\r' && src[1] == '\n') {
            if (src[2] != ' ') {
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

bool ParseField(__inout char* string, __deref_out char** identifier, __deref_out char** value)
{
    string = SkipWhiteSpace(string);
    char* end = ::strchr(string, ':');
    if (end == NULL || end <= string) {
        return false;
    }
    for (--end; end >= string && IsWhiteSpace(*end); end--);
    if (end <= string) {
        return false;
    }
    *++end = 0;
    *identifier = string;
    *value = ParseUnstructured(end + 1);
    return true;
}


TEST(Good, TestFieldParser)
{
    static const char* c_dataSet[][3] = {
        { "Subject: This\r\n is a test", "Subject", "This is a test" },
    };
    for (size_t i = 0; i < _countof(c_dataSet); i++) {
        const char* input = c_dataSet[i][0];
        const char* exfld = c_dataSet[i][1];
        const char* exval = c_dataSet[i][2];
        char *actualfld = NULL, *actualval = NULL;
        char* dup = ::strdup(input);
        bool result = ParseField(dup, &actualfld, &actualval);
        CHECK_TRUE(result);
        CHECK_STRINGS_EQUAL(actualfld, exfld);
        CHECK_STRINGS_EQUAL(actualval, exval);
        ::free(dup);
    }
}
