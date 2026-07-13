#include <unity.h>

#include <crc/HovalCrc.h>

void test_calcCrc_with_123456789(void)
{
    // Test vector: CRC of "123456789" should be 0xff5a
    const uint8_t test_data[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    const size_t test_length = sizeof(test_data) / sizeof(test_data[0]);

    
    uint16_t result = HovalCrc::calcCrc(test_data, test_length);
    TEST_ASSERT_EQUAL_HEX16(0xff5a, result);
}

void test_calcCrc_with_error_msg(void)
{
    const uint8_t test_data[] = {0x42, 0x00, 0x00, 0x71, 0x72, 0x03, 0x00, 0xFC, 0x01, 0x08, 0x02, 0x32, 0x00, 0xF7, 0x04, 0xE4, 0xAB, 0x00, 0x00, 0x00, 0x00};
    const size_t test_length = sizeof(test_data) / sizeof(test_data[0]);

    
    uint16_t result = HovalCrc::calcCrc(test_data, test_length);
    TEST_ASSERT_EQUAL_HEX16(0xDF58, result);
}

void test_calcCrc_with_sequence_msg(void)
{
    // Test vector: CRC of "123456789" should be 0xff5a
    const uint8_t test_data[] = {0x74, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF};
    const size_t test_length = sizeof(test_data) / sizeof(test_data[0]);

    
    uint16_t result = HovalCrc::calcCrc(test_data, test_length);
    TEST_ASSERT_EQUAL_HEX16(0x71E5, result);
}

void setUp(void)
{
    // set stuff up here
}

void tearDown(void)
{
    // clean stuff up here
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_calcCrc_with_123456789);
    RUN_TEST(test_calcCrc_with_error_msg);
    RUN_TEST(test_calcCrc_with_sequence_msg);
    UNITY_END();
    
    return 0;
}
