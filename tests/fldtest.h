#pragma once


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


class TestFieldParser : public UnitTest
{
public:
    void Run()
    {
        this->TestGood("Subject: This\r\n is a test", "Subject", "This is a test");
    }

    void TestGood(const char* input, const char* efield, const char* evalue)
    {
        char *rfield = NULL, *rvalue = NULL;
        char* dup = ::strdup(input);
        bool result = ParseField(dup, &rfield, &rvalue);
        this->AssertTrue(result, input);
        this->AssertEquals(rfield, efield);
        this->AssertEquals(rvalue, evalue);
        ::free(dup);
    }
};
