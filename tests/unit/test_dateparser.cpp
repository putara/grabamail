#include <TestHarness.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <intrin.h>

int getday(const char* date)
{
    static const char c_days[7][4] = {
        {"Mon"}, {"Tue"}, {"Wed"}, {"Thu"},
        {"Fri"}, {"Sat"}, {"Sun"}
    };
    for (int i = 0; i < _countof(c_days); i++) {
        if (strncmp(date, c_days[i], 3) == 0) {
            return i;
        }
    }
    return -1;
}

int getmonth(const char* date)
{
    static const char c_months[12][4] = {
        {"Jan"}, {"Feb"}, {"Mar"}, {"Apr"},
        {"May"}, {"Jun"}, {"Jul"}, {"Aug"},
        {"Sep"}, {"Oct"}, {"Nov"}, {"Dec"},
    };
    for (int i = 0; i < _countof(c_months); i++) {
        if (strncmp(date, c_months[i], 3) == 0) {
            return i;
        }
    }
    return -1;
}

int strtoint(const char* str, UINT digits)
{
    if (digits <= 0 || digits >= 10) {
        return -1;
    }
    UINT retval = 0;
    while (digits-- > 0) {
        if (*str < '0' || *str > '9') {
            return -1;
        }
        retval = retval * 10 + (*str - '0');
        str++;
    }
    return static_cast<int>(retval);
}

const char* skip_crlf(const char* str)
{
    while (str[0] == '\r' && str[1] == '\n') {
        str += 2;
    }
    return str;
}

const char* skip_fws(const char* str)
{
    str = skip_crlf(str);
    if (*str != '\t' && *str != ' ') {
        return NULL;
    }
    do {
        str++;
        str = skip_crlf(str);
    } while (*str == '\t' || *str == ' ');
    return str;
}

// RFC822, RFC2822
bool ParseDate(__in const char* date, __in const char* format, __out SYSTEMTIME* utc)
{
    int retval;
    bool neg;
    int tz = 0;
    SYSTEMTIME st;
    ZeroMemory(&st, sizeof(st));

    for (; *format != '\0'; format++) {
        switch (*format) {
        case 'D':   // weekday
            retval = getday(date);
            if (retval < 0) {
                return false;
            }
            st.wDayOfWeek = retval;
            date += 3;
            break;
        case 'd':   // day (2 digits)
            retval = strtoint(date, 2);
            if (retval <= 0 || retval > 31) {
                return false;
            }
            st.wDay = retval;
            date += 2;
            break;
        case 'M':   // month
            retval = getmonth(date);
            if (retval < 0) {
                return false;
            }
            st.wMonth = retval + 1;
            date += 3;
            break;
        case 'm':   // month (2 digits)
            retval = strtoint(date, 2);
            if (retval <= 0 || retval > 12) {
                return false;
            }
            st.wMonth = retval;
            date += 2;
            break;
        case 'y':   // year (2 digits)
            retval = strtoint(date, 2);
            if (retval <= 0 || retval > 99) {
                return false;
            }
            if (0 <= retval && retval <= 49) {
                st.wYear = 2000 + retval;
            } else {
                st.wYear = 1900 + retval;
            }
            date += 2;
            break;
        case 'Y':   // year (4 digits)
            retval = strtoint(date, 4);
            if (retval < 0) {
                return false;
            }
            st.wYear = retval;
            date += 4;
            break;
        case 'H':   // hour (2 digits)
            retval = strtoint(date, 2);
            if (retval < 0 || retval > 23) {
                return false;
            }
            st.wHour = retval;
            date += 2;
            break;
        case 'i':   // minute (2 digits)
            retval = strtoint(date, 2);
            if (retval < 0 || retval > 59) {
                return false;
            }
            st.wMinute = retval;
            date += 2;
            break;
        case 's':   // second (2 digits)
            retval = strtoint(date, 2);
            if (retval < 0 || retval > 59) {
                return false;
            }
            st.wSecond = retval;
            date += 2;
            break;
        case 'z':   // zone
            if (*date == '+') {
                neg = false;
                date++;
            } else if (*date == '-') {
                neg = true;
                date++;
            } else {
                // TODO: support ASCII zones
                return false;
            }
            retval = strtoint(date, 4);
            if (retval < 0) {
                return false;
            }
            if (retval % 100 > 59) {
                return false;
            }
            tz = neg ? -retval : retval;
            date += 4;
            break;
        case ' ':   // space
            date = skip_fws(date);
            if (date == NULL) {
                return false;
            }
            break;
        default:
            if (*date != *format) {
                return false;
            }
            date++;
            break;
        }
    }
    if (*date != 0) {
        return false;
    }
    FILETIME ft;
    if (::SystemTimeToFileTime(&st, &ft) == FALSE) {
        return false;
    }
    LARGE_INTEGER x;
    x.LowPart = ft.dwLowDateTime;
    x.HighPart = ft.dwHighDateTime;
    x.QuadPart -= __emul((tz / 100 * 60) + (tz % 100), 600000000);
    if (x.QuadPart < 0) {
        x.QuadPart = 0;
    }
    ft.dwLowDateTime = x.LowPart;
    ft.dwHighDateTime = x.HighPart;
    return ::FileTimeToSystemTime(&ft, utc) != FALSE;
}


TEST(Good, TestDateTimeParser)
{
    static const char* c_dataSet[][3] = {
        { "13 Oct 2019 02:21:54 +1300", "d M Y H:i:s z", "20191012132154" },
        { "Fri, 21 Nov 1997 09:55:06 -0600", "D, d M Y H:i:s z", "19971121155506" },
    };
    for (size_t i = 0; i < _countof(c_dataSet); i++) {
        const char* input = c_dataSet[i][0];
        const char* format = c_dataSet[i][1];
        const char* expected = c_dataSet[i][2];
        SYSTEMTIME st = {};
        bool result = ParseDate(input, format, &st);
        CHECK_TRUE(result);
        char buffer[100];
        ::sprintf_s(buffer, _countof(buffer), "%04u%02u%02u%02u%02u%02u", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
        CHECK_STRINGS_EQUAL(buffer, expected);
    }
}


#if 0
void RunInteractive()
{
    char buffer[100];
    printf(": ");
    while (fgets(buffer, _countof(buffer), stdin) != NULL) {
        *strchr(buffer, '\n') = 0;
        if (*buffer == 0) {
            break;
        }
        SYSTEMTIME utc = {};
        bool retval = ParseDate(buffer, "d M Y H:i:s z", &utc);
        printf("%d ", retval);
        if (retval) {
            SYSTEMTIME loc = {};
            ::SystemTimeToTzSpecificLocalTime(NULL, &utc, &loc);
            int ret = ::GetDateFormatA(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &loc, NULL, buffer, _countof(buffer));
            if (ret > 0) {
                buffer[ret - 1] = ' ';
                buffer[ret] = 0;
                ::GetTimeFormatA(LOCALE_USER_DEFAULT, 0/*TIME_FORCE24HOURFORMAT*/, &loc, NULL, buffer + ret, _countof(buffer) - ret);
                printf("%s", buffer);
            }
        }
        printf("\n: ");
    }
}
#endif
