#include <unity.h>
#include "services/Logger.h"
#include <string.h>

// Include the source file directly so it compiles in the native test environment
#include "../../src/services/Logger.cpp"

using namespace Services;

void test_logger_secret_redaction() {
    Logger& logger = Logger::getInstance();
    logger.clearSecrets();
    logger.registerSecret("supersecret123");
    logger.registerSecret("mypassword");

    char buffer[128];
    strcpy(buffer, "This contains supersecret123 and mypassword!");
    logger.redact(buffer);

    TEST_ASSERT_EQUAL_STRING("This contains *************** and **********!", buffer);
}

void test_logger_multiple_secrets() {
    Logger& logger = Logger::getInstance();
    logger.clearSecrets();
    logger.registerSecret("secret");

    char buffer[128];
    strcpy(buffer, "secret message with secret content");
    logger.redact(buffer);

    TEST_ASSERT_EQUAL_STRING("****** message with ****** content", buffer);
}

void test_logger_no_secrets() {
    Logger& logger = Logger::getInstance();
    logger.clearSecrets();

    char buffer[128];
    strcpy(buffer, "no secret here");
    logger.redact(buffer);

    TEST_ASSERT_EQUAL_STRING("no secret here", buffer);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_logger_secret_redaction);
    RUN_TEST(test_logger_multiple_secrets);
    RUN_TEST(test_logger_no_secrets);
    return UNITY_END();
}
