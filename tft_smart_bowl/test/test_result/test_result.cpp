#include <unity.h>
#include "core/Result.h"

using namespace Core;

void test_result_success_holds_value() {
    Result<int> res(42);
    TEST_ASSERT_TRUE(res.isOk());
    TEST_ASSERT_FALSE(res.isError());
    TEST_ASSERT_EQUAL_INT(42, res.value());
}

void test_result_error_holds_error() {
    Error err(ErrorCategory::Sensor, ErrorCode::DeviceNotFound, "Sensor not found");
    Result<int> res(err);
    TEST_ASSERT_FALSE(res.isOk());
    TEST_ASSERT_TRUE(res.isError());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ErrorCategory::Sensor), static_cast<uint8_t>(res.error().category));
    TEST_ASSERT_EQUAL_UINT32(static_cast<uint32_t>(ErrorCode::DeviceNotFound), static_cast<uint32_t>(res.error().code));
}

void test_result_void_success() {
    Result<void> res;
    TEST_ASSERT_TRUE(res.isOk());
    TEST_ASSERT_FALSE(res.isError());
}

void test_result_void_error() {
    Error err(ErrorCategory::Storage, ErrorCode::FileNotFound, "File not found");
    Result<void> res(err);
    TEST_ASSERT_FALSE(res.isOk());
    TEST_ASSERT_TRUE(res.isError());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ErrorCategory::Storage), static_cast<uint8_t>(res.error().category));
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_result_success_holds_value);
    RUN_TEST(test_result_error_holds_error);
    RUN_TEST(test_result_void_success);
    RUN_TEST(test_result_void_error);
    return UNITY_END();
}
