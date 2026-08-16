#include <unity.h>

#include <cstdint>
#include <cstring>

#include "HovalMessage.h"
#include "crc/HovalCrc.h"

// HovalMessage.cpp is part of a private embedded library without its own
// build target for the native_test env (lib_ldf_mode = off). Pull its
// translation unit in directly so calculateCrc()/validateCrc() are the real
// implementation under test, not a reimplementation.
#include "HovalMessage.cpp"

// ---------------------------------------------------------------------------
// calculateCrc()
// ---------------------------------------------------------------------------

// Cross-checked against test_hovalcrc's "error msg" vector: the buffer
// calculateCrc()/validateCrc() build (functionCode, functionGroup,
// functionNumber, dataPointId hi/lo, ...body) is exactly that raw CRC test
// vector, split into a HovalMessage's fields.
void test_calculateCrc_matches_known_error_message_vector(void)
{
    HovalMessageType type(/*unitType*/ 0, /*functionGroup*/ 0x00, /*functionNumber*/ 0x00, /*dataPointId*/ 0x7172, HovalDataType::RAW, 0, DataType::ERRORTYPE);
    HovalMessage msg(1, &type, HovalFunctionCode::UPDATE, 0, 0, /*maxBodyLength*/ 16);

    const uint8_t payload[] = {0x03, 0x00, 0xFC, 0x01, 0x08, 0x02, 0x32, 0x00, 0xF7, 0x04, 0xE4, 0xAB, 0x00, 0x00, 0x00, 0x00};
    TEST_ASSERT_TRUE(msg.addToBody(payload, sizeof(payload)));

    TEST_ASSERT_EQUAL_HEX16(0xDF58, msg.calculateCrc());
}

// Cross-checked against test_hovalcrc's "sequence msg" vector.
void test_calculateCrc_matches_known_sequence_vector(void)
{
    HovalMessageType type(/*unitType*/ 0, /*functionGroup*/ 0x00, /*functionNumber*/ 0xFF, /*dataPointId*/ 0x0000, HovalDataType::RAW, 0, DataType::RAW);
    HovalMessage msg(1, &type, HovalFunctionCode::UNKNOWN6, 0, 0, /*maxBodyLength*/ 4);

    const uint8_t payload[] = {0x00, 0x00, 0xFF, 0xFF};
    TEST_ASSERT_TRUE(msg.addToBody(payload, sizeof(payload)));

    TEST_ASSERT_EQUAL_HEX16(0x71E5, msg.calculateCrc());
}

// No external oracle for this one; verifies calculateCrc() assembles
// [functionCode, functionGroup, functionNumber, dataPointId hi, dataPointId
// lo, ...body] the way HovalCrc::calcCrc() expects, independent of the two
// fixed vectors above.
void test_calculateCrc_is_self_consistent_with_manually_built_buffer(void)
{
    HovalMessageType type(/*unitType*/ 0, /*functionGroup*/ 0x12, /*functionNumber*/ 0x34, /*dataPointId*/ 0xABCD, HovalDataType::RAW, 0, DataType::RAW);
    HovalMessage msg(1, &type, HovalFunctionCode::READ_REQUEST, 0, 0, /*maxBodyLength*/ 1);

    const uint8_t payload[] = {0x99};
    TEST_ASSERT_TRUE(msg.addToBody(payload, sizeof(payload)));

    const uint8_t expectedBuffer[] = {HovalFunctionCode::READ_REQUEST, 0x12, 0x34, 0xAB, 0xCD, 0x99};
    uint16_t expectedCrc = HovalCrc::calcCrc(expectedBuffer, sizeof(expectedBuffer));

    TEST_ASSERT_EQUAL_HEX16(expectedCrc, msg.calculateCrc());
}

// ---------------------------------------------------------------------------
// validateCrc()
// ---------------------------------------------------------------------------

void test_validateCrc_is_trivially_true_for_bodies_of_two_bytes_or_less(void)
{
    HovalMessageType type(0, 0, 0, 0, HovalDataType::RAW, 0, DataType::RAW);

    HovalMessage empty(1, &type, HovalFunctionCode::PING, 0, 0, /*maxBodyLength*/ 0);
    TEST_ASSERT_TRUE(empty.validateCrc());
    TEST_ASSERT_EQUAL_UINT8(0, empty.messageBodyLength);
    TEST_ASSERT_EQUAL_HEX16(0x0000, empty.crc);

    HovalMessage twoBytes(1, &type, HovalFunctionCode::PING, 0, 0, /*maxBodyLength*/ 2);
    const uint8_t data[] = {0xAA, 0xBB};
    TEST_ASSERT_TRUE(twoBytes.addToBody(data, sizeof(data)));

    TEST_ASSERT_TRUE(twoBytes.validateCrc());
    // Bypassed entirely: body untouched, no CRC parsed out of it.
    TEST_ASSERT_EQUAL_UINT8(2, twoBytes.messageBodyLength);
    TEST_ASSERT_EQUAL_HEX16(0x0000, twoBytes.crc);
}

void test_validateCrc_succeeds_and_strips_crc_bytes_on_match(void)
{
    HovalMessageType type(0, 0x00, 0x00, 0x7172, HovalDataType::RAW, 0, DataType::ERRORTYPE);
    HovalMessage msg(1, &type, HovalFunctionCode::UPDATE, 0, 0, /*maxBodyLength*/ 18);

    // 16 payload bytes (known "error msg" vector) + big-endian CRC (0xDF58).
    const uint8_t body[] = {0x03, 0x00, 0xFC, 0x01, 0x08, 0x02, 0x32, 0x00, 0xF7, 0x04, 0xE4, 0xAB, 0x00, 0x00, 0x00, 0x00, 0xDF, 0x58};
    TEST_ASSERT_TRUE(msg.addToBody(body, sizeof(body)));

    TEST_ASSERT_TRUE(msg.validateCrc());
    TEST_ASSERT_EQUAL_UINT8(16, msg.messageBodyLength); // CRC bytes stripped
    TEST_ASSERT_EQUAL_HEX16(0xDF58, msg.crc);
    // Payload itself must be untouched by the strip.
    TEST_ASSERT_EQUAL_UINT8_ARRAY(body, msg.messageBody, 16);
}

void test_validateCrc_fails_and_leaves_state_unchanged_on_mismatch(void)
{
    HovalMessageType type(0, 0x00, 0x00, 0x7172, HovalDataType::RAW, 0, DataType::ERRORTYPE);
    HovalMessage msg(1, &type, HovalFunctionCode::UPDATE, 0, 0, /*maxBodyLength*/ 18);

    // Same as the matching vector, but with the CRC's low byte corrupted.
    const uint8_t body[] = {0x03, 0x00, 0xFC, 0x01, 0x08, 0x02, 0x32, 0x00, 0xF7, 0x04, 0xE4, 0xAB, 0x00, 0x00, 0x00, 0x00, 0xDF, 0x59};
    TEST_ASSERT_TRUE(msg.addToBody(body, sizeof(body)));

    TEST_ASSERT_FALSE(msg.validateCrc());
    // Nothing should be mutated on failure.
    TEST_ASSERT_EQUAL_UINT8(18, msg.messageBodyLength);
    TEST_ASSERT_EQUAL_HEX16(0x0000, msg.crc);
}

// ---------------------------------------------------------------------------
// addToBody() - supporting coverage: both CRC methods depend on it correctly
// tracking messageBodyLength / rejecting overflow.
// ---------------------------------------------------------------------------

void test_addToBody_rejects_data_exceeding_max_length(void)
{
    HovalMessageType type(0, 0, 0, 0, HovalDataType::RAW, 0, DataType::RAW);
    HovalMessage msg(1, &type, HovalFunctionCode::READ_REQUEST, 0, 0, /*maxBodyLength*/ 2);

    const uint8_t firstChunk[] = {0x01, 0x02};
    TEST_ASSERT_TRUE(msg.addToBody(firstChunk, sizeof(firstChunk)));
    TEST_ASSERT_EQUAL_UINT8(2, msg.messageBodyLength);

    const uint8_t overflow[] = {0x03};
    TEST_ASSERT_FALSE(msg.addToBody(overflow, sizeof(overflow)));
    TEST_ASSERT_EQUAL_UINT8(2, msg.messageBodyLength); // unchanged on rejection
}

void setUp(void) {}
void tearDown(void) {}

int main(int argc, char** argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_calculateCrc_matches_known_error_message_vector);
    RUN_TEST(test_calculateCrc_matches_known_sequence_vector);
    RUN_TEST(test_calculateCrc_is_self_consistent_with_manually_built_buffer);
    RUN_TEST(test_validateCrc_is_trivially_true_for_bodies_of_two_bytes_or_less);
    RUN_TEST(test_validateCrc_succeeds_and_strips_crc_bytes_on_match);
    RUN_TEST(test_validateCrc_fails_and_leaves_state_unchanged_on_mismatch);
    RUN_TEST(test_addToBody_rejects_data_exceeding_max_length);
    UNITY_END();

    return 0;
}
