#pragma once


class UnitTest
{
private:
    ULONG successCount;
    ULONG failureCount;
    ULONG skipCount;

    static char* EscapeString(const char* src) throw()
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

    void ReportCount() throw()
    {
        printf("%lu success, %lu failure (%lu total)\n", this->successCount, this->failureCount, this->successCount + this->failureCount);
    }

    __declspec(noreturn) void ReportFailure(const char* result, const char* expected, __in_opt const char* what = NULL) throw()
    {
        char* res = EscapeString(result);
        char* exp = EscapeString(expected);
        printf("Failure: %s\n- %s\n+ %s\n", what != NULL ? what : "", res, exp);
        ::free(res);
        ::free(exp);
        this->ReportCount();
        ::exit(1);
    }

protected:
    void AssertTrue(bool result, __in_opt const char* what = NULL) throw()
    {
        if (result) {
            this->successCount++;
        } else {
            this->failureCount++;
            ReportFailure("false", "true", what);
        }
    }

    void AssertFalse(bool result, __in_opt const char* what = NULL) throw()
    {
        if (result == false) {
            this->successCount++;
        } else {
            this->failureCount++;
            ReportFailure("true", "false", what);
        }
    }

    void AssertEquals(const char* result, const char* expected, __in_opt const char* what = NULL) throw()
    {
        if (::strcmp(result, expected) == 0) {
            this->successCount++;
        } else {
            this->failureCount++;
            ReportFailure(result, expected, what);
        }
    }

public:
    UnitTest() throw()
        : successCount()
        , failureCount()
    {
    }

    ~UnitTest() throw()
    {
        this->ReportCount();
    }
};
