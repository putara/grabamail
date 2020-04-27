#include <TestHarness.h>
#include <TestResult.h>

#include <Test.cpp>

using namespace CppUnitLite;

int main()
{
    TestResult result;
    TestRegistry::runAllTests(result);
    return (result.getFailureCount());
}
