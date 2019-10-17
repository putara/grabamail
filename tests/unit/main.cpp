#include <TestHarness.h>
#include <TestResult.h>

#include <Failure.cpp>
#include <SimpleString.cpp>
#include <Test.cpp>
#include <TestRegistry.cpp>
#include <TestResult.cpp>

int main()
{
    TestResult result;
    TestRegistry::runAllTests(result);
    return (result.getFailureCount());
}
