#include <unity.h>

#include <string>

#include "Log.h"

// Mirrors the logPrefix()-based logging pattern used by HovalMessage.cpp /
// HovalProtocolDecoder.cpp, to prove the stub Log.h satisfies it off-target.
class LoggingClass
{
public:
    std::string logPrefix() { return "LoggingClass"; }

    void doWork()
    {
        logInfoP("informational message: %d", 42);
        logErrorP("error message: %s", "bad");
        logDebugP("debug message");
        logTraceP("trace message");
    }
};

void test_log_macros_compile_and_run(void)
{
    LoggingClass logging;
    logging.doWork();
    logInfo("Standalone", "prefixed info message");
    logError("Standalone", "prefixed error message");
    TEST_PASS();
}

void setUp(void) {}
void tearDown(void) {}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_log_macros_compile_and_run);
    UNITY_END();

    return 0;
}
