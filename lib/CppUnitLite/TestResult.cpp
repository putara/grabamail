
#include "TestResult.h"
#include "Failure.h"

#include <stdio.h>
#include <time.h>

static long long get_time();
static long long time_diff(long long t1, long long t2);


TestResult::TestResult ()
    : failureCount (0), testCount(0), secondsElapsed(0), startTime(0)
{
}


void TestResult::testWasRun()
{
    testCount++;
}

void TestResult::startTests ()
{
    startTime = get_time();
}


void TestResult::addFailure (const Failure& failure)
{
    fprintf (stdout, "%s%s%s%s%ld%s%s\n",
        "Failure: \"",
        failure.message.asCharString (),
        "\" " ,
        "line ",
        failure.lineNumber,
        " in ",
        failure.fileName.asCharString ());

    failureCount++;
}


void TestResult::endTests ()
{
    long long endTime = get_time();
    secondsElapsed = time_diff(endTime, startTime) / 1000000.0;

    fprintf(stdout, "%d tests run\n", testCount);
    if (failureCount > 0)
        fprintf(stdout, "****** There were %d failures\n", failureCount);
    else
        fprintf(stdout, "There were no test failures\n");
    fprintf(stdout, "(time: %f s)\n", secondsElapsed);
}


#ifndef _WINDOWS
#include <sys/time.h>
static long long get_time()
{
    struct timeval tv;
    ::gettimeofday(&tv, NULL);
    return static_cast<long long>(tv.tv_sec) * 1000000) + tv.tv_usec;
}
static long long time_diff(long long t1, long long t2)
{
    return t1 - t2;
}
#else
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <intrin.h>
static long long get_time()
{
    LARGE_INTEGER time;
    ::QueryPerformanceCounter(&time);
    return time.QuadPart;
}
static long long time_diff(long long t1, long long t2)
{
    LARGE_INTEGER freq;
    ::QueryPerformanceFrequency(&freq);
    return (t1 - t2) * 1000000 / freq.QuadPart;
}
#endif
