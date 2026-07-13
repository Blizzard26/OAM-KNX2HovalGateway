#include "HovalMessage.h"
#include <Log.h>
#include "crc/HovalCrc.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef MIN
// clang-format off
#define MIN(a, b) ((a < b) ? (a) : (b))
// clang-format on
#endif

#ifndef MAX
// clang-format off
#define MAX(a, b) ((a > b) ? (a) : (b))
// clang-format on
#endif

static constexpr uint32_t pow10[] = {1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000};

#pragma region HovalMessage

HovalMessage::HovalMessage(uint8_t _messageId, const HovalMessageType* _messageType, HovalFunctionCode _functionCode, uint16_t _senderId, uint16_t _targetId,
                           uint8_t _maxBodyLength)
    : messageId(_messageId), messageType(_messageType), functionCode(_functionCode), senderId(_senderId), targetId(_targetId), maxBodyLength(_maxBodyLength)
{
  if (maxBodyLength > 0)
  {
    messageBody = new uint8_t[maxBodyLength];
    memset(messageBody, 0, maxBodyLength);
  }
}

HovalMessage::~HovalMessage()
{
  if (messageBody != nullptr)
  {
    delete[] messageBody;
  }

  if (messageType->decimals == DYNAMIC_MESSAGE_TYPE)
  {
    delete (messageType);
  }
}

bool HovalMessage::addToBody(const uint8_t* data, uint8_t length)
{
  if (messageBodyLength + length > maxBodyLength)
  {
    logError("HovalMessage", "Cannot add data to message body. Max length exceeded. Current length: %u, Adding: %u, Max: %u", messageBodyLength, length,
             maxBodyLength);
    return false;
  }

  memcpy(messageBody + messageBodyLength, data, length);
  messageBodyLength += length;
  return true;
}

uint16_t HovalMessage::computeCrc(uint8_t payloadLength) const
{
  uint8_t bufferLength = payloadLength + 5;
  uint8_t buf[bufferLength];
  buf[0] = functionCode;
  buf[1] = messageType->functionGroup;
  buf[2] = messageType->functionNumber;
  buf[3] = messageType->dataPointId >> 8;
  buf[4] = messageType->dataPointId & 0xFF;
  memcpy(buf + 5, messageBody, payloadLength);

  return HovalCrc::calcCrc(buf, bufferLength);
}

bool HovalMessage::validateCrc()
{
  // Single message don't have a crc
  if (messageBodyLength <= 2)
  {
    return true;
  }

  // Last two bytes are the CRC; compute over payload only
  uint16_t receivedCrc = (uint16_t)messageBody[messageBodyLength - 2] << 8 | messageBody[messageBodyLength - 1];

  uint8_t payloadLength = messageBodyLength - 2;
  uint16_t calculatedCrc = computeCrc(payloadLength);

  if (calculatedCrc != receivedCrc)
  {
    logError("HovalMessage", "CRC validation failed. Expected: 0x%04X, Calculated: 0x%04X", receivedCrc, calculatedCrc);
    logHexDebug("HovalMessage", messageBody, payloadLength);
    return false;
  }

  // Strip CRC bytes from body only after successful validation
  crc = receivedCrc;
  messageBodyLength = payloadLength;
  return true;
}

uint16_t HovalMessage::calculateCrc() const
{
  // Same buffer layout as validateCrc(), computed over the outgoing body
  return computeCrc(messageBodyLength);
}

void HovalMessage::printBody(char* buf, uint8_t bufLen)
{
  if (bufLen < 2)
    return;
  if (messageBodyLength == 0)
  {
    buf[0] = '-';
    buf[1] = '\0';
    return;
  }

  // printer->print(message->unitId);
  switch (messageType->dataType)
  {
  case DataType::FLOAT:
    snprintf(buf, bufLen, "%1.2f", floatValue());
    break;
  case DataType::UINT8:
    snprintf(buf, bufLen, "%u", u8Value());
    break;
  case DataType::LIST:
    snprintf(buf, bufLen, "0x%02X", list());
    break;
  case DataType::RAW:
  case DataType::ERRORTYPE: {
    uint8_t index = 0, len = 0;
    while (index < messageBodyLength && len + 3 < bufLen - 1)
    {
      len += snprintf(buf + len, bufLen - len, "%02X ", messageBody[index++]);
    }

    if (index < messageBodyLength)
    {
      buf[MIN(bufLen - 2, len + 1)] = '-';
      buf[MIN(bufLen - 1, len + 2)] = '\0';
    }
  }
  break;
  case DataType::STRING:
    memset(buf, 0, bufLen);
    memcpy(buf, messageBody, MIN(bufLen - 1, messageBodyLength));
    buf[MIN(bufLen - 1, messageBodyLength)] = '\0';
    break;
    /*  case DataType::ERRORTYPE:
      {
        ErrorMessage e = errorMessage();
        uint8_t pos = snprintf(buf, bufLen, "%c:%u", e.error_type, e.error_code);
        struct tm *timeinfo = localtime(&e.appearance_time);
        pos += strftime(buf + pos, bufLen - pos, " %d.%m.%y %T", timeinfo);
      }
      break;*/
  default:
    buf[0] = '?';
    buf[1] = '\0';
    break;
  }
}

float toFloat(uint16_t rawValue, uint8_t decimals)
{
  float value = rawValue;
  value /= pow10[decimals];
  return value;
}

float toFloat(int16_t rawValue, uint8_t decimals)
{
  float value = rawValue;
  value /= pow10[decimals];
  return value;
}

double toDouble(int64_t rawValue, uint8_t decimals)
{
  double value = rawValue;
  value /= pow10[decimals];
  return value;
}

float HovalMessage::floatValue() const
{
  switch (messageType->rawType)
  {
  case HovalDataType::U8:
    return toFloat((uint16_t)u8Value(), messageType->decimals);
  case HovalDataType::U16:
    return toFloat(u16Value(), messageType->decimals);
  case HovalDataType::S8:
    return toFloat((int16_t)s8Value(), messageType->decimals);
  case HovalDataType::S16:
    return toFloat(s16Value(), messageType->decimals);
  default:
    logError("HovalMessage", "Unsupported float type: %d", (uint8_t)messageType->rawType);
    return NAN; // NAN
  }
}

double HovalMessage::doubleValue() const
{
  switch (messageType->rawType)
  {
  case HovalDataType::U8:
  case HovalDataType::U16:
  case HovalDataType::S8:
  case HovalDataType::S16:
    return floatValue();

  case HovalDataType::U32:
    return toDouble(u32Value(), messageType->decimals);
  case HovalDataType::S32:
    return toDouble(s32Value(), messageType->decimals);
  case HovalDataType::S64:
    return toDouble(s64Value(), messageType->decimals);
  default:
    logError("HovalMessage", "Unsupported double type: %d", (uint8_t)messageType->rawType);
    return NAN; // NAN
  }
}

uint8_t HovalMessage::u8Value() const
{
  if (messageBodyLength != 1)
  {
    logError("HovalMessage", "DataPointType missmatch");
    return UINT8_MAX;
  }
  return messageBody[0];
}

uint16_t HovalMessage::u16Value() const
{
  if (messageBodyLength != 2)
  {
    logError("HovalMessage", "DataPointType missmatch");
    return UINT16_MAX;
  }
  return bigEndianToUint16(messageBody);
}

uint32_t HovalMessage::u32Value() const
{
  if (messageBodyLength != 4)
  {
    logError("HovalMessage", "DataPointType missmatch");
    return UINT32_MAX;
  }
  return bigEndianToUint32(messageBody);
}

int8_t HovalMessage::s8Value() const
{
  return (int8_t)u8Value();
}

int16_t HovalMessage::s16Value() const
{
  return (int16_t)u16Value();
}

int32_t HovalMessage::s32Value() const
{
  return (int32_t)u32Value();
}

int64_t HovalMessage::s64Value() const
{
  if (messageBodyLength != 8)
  {
    logError("HovalMessage", "DataPointType missmatch");
    return INT64_MAX;
  }
  return bigEndianToInt64(messageBody);
}

uint8_t HovalMessage::list() const
{
  return u8Value();
}

time_t convertToUnixTime(uint16_t date, uint16_t time)
{
  // (date - (1.1.1900 - 1.1.1970)) convert to seconds + time in seconds
  return ((uint32_t)date - 25568) * 86400u + time * 60;
}

ErrorMessage HovalMessage::errorMessage() const
{
  ErrorMessage errorMessage;
  /*
  Byte
  0	  error_type - log level, displayed as char where 3 = W, 4 = E ...
  1	  ?? - maybe high byte of error_type, but error type is defined as uint8
  2	  error_code - low byte
  3	  error_code - high byte
  4	  source - low byte
  5	  source - high byte
  6	  function_group
  7	  function_number
  8	  start_time - low byte time since midnight
  9	  start_time - high byte
  10  start_date - low byte date since 01.01.1900
  11  start_date - high byte
  12  end_time - low byte time since midnight
  13  end_time - high byte
  14  end_date - low byte date since 01.01.1900
  15  end_date - high byte
  16  crc - not included in messageBody
  17  crc - not included in messageBody
  */

  if (messageBodyLength != 16)
  {
    logError("HovalMessage", "ErrorMessage - DataPointType missmatch");
    errorMessage.error_type = '\0';
    errorMessage.error_code = 0xFFFF;
    return errorMessage;
  }

  if (messageBody[0] == 0)
  {
    errorMessage.error_type = '\0';
    errorMessage.error_code = 0xFFFF;
    return errorMessage;
  }

  if (messageBody[0] == 0xB4)
  {
    // Not sure about the meaning of these messages. They appear after confirming a warning and always have the following content:
    // B4 34 00 20 40 00 00 00 72 71 01 00 00 00 00 00
    errorMessage.error_type = '\0';
    errorMessage.error_code = 0xFFFF;
    return errorMessage;
  }

  errorMessage.error_type = mapErrorType(messageBody[0]);
  errorMessage.error_code = littleEndianToUint16(messageBody + 2);
  errorMessage.source = littleEndianToUint16(messageBody + 4);
  errorMessage.function_group = messageBody[6];
  errorMessage.function_number = messageBody[7];
  errorMessage.appearance_time = convertToUnixTime(littleEndianToUint16(messageBody + 10), littleEndianToUint16(messageBody + 8));
  errorMessage.disappear_time = convertToUnixTime(littleEndianToUint16(messageBody + 14), littleEndianToUint16(messageBody + 12));

  if (errorMessage.error_type == '?')
  {
    logInfo("HovalMessage", "Unkown Error Type %#x", messageBody[0]);
    logIndentUp();
    logHexInfo("HovalMessage", messageBody, messageBodyLength);
    logIndentDown();
  }
  return errorMessage;
}

char HovalMessage::mapErrorType(uint8_t errorType) const
{
  // Known error codes
  // 03 00 FC 01 = 3:508 = B:508
  // 08 00 FF 01 = 8:511 = W:511
  // TODO: This is currently just a wild guess. Needs to be confirmed.
  switch (errorType)
  {
  case 3:
    return 'B';
  case 8:
    return 'W';
  default:
    return '?';
  }
}

HovalMessage::operator uint8_t() const
{
  return u8Value();
}

HovalMessage::operator uint16_t() const
{
  return u16Value();
}

HovalMessage::operator uint32_t() const
{
  return u32Value();
}

HovalMessage::operator int8_t() const
{
  return s8Value();
}

HovalMessage::operator int16_t() const
{
  return s16Value();
}

HovalMessage::operator int32_t() const
{
  return s32Value();
}

HovalMessage::operator int64_t() const
{
  return s64Value();
}

HovalMessage::operator ErrorMessage() const
{
  return errorMessage();
}

constexpr uint16_t HovalMessage::littleEndianToUint16(const uint8_t* body)
{
  return body[0] | (uint16_t)body[1] << 8;
}

constexpr uint16_t HovalMessage::bigEndianToUint16(const uint8_t* body)
{
  return (uint16_t)body[0] << 8 | body[1];
}

constexpr uint32_t HovalMessage::bigEndianToUint32(const uint8_t* body)
{
  return (uint32_t)body[0] << 24 | (uint32_t)body[1] << 16 | (uint32_t)body[2] << 8 | body[3];
}

constexpr int64_t HovalMessage::bigEndianToInt64(const uint8_t* body)
{
  return (int64_t)body[0] << 56 | (int64_t)body[1] << 48 | (int64_t)body[2] << 40 | (int64_t)body[3] << 32 | (int64_t)body[4] << 24 | (int64_t)body[5] << 16 |
         (int64_t)body[6] << 8 | body[7];
}

#pragma endregion

#pragma region HovalValue

HovalValue::HovalValue(uint8_t _value, DataType _type) : type(_type)
{
  value.ucharValue = _value;
}

uint32_t scaleUnsigned(uint32_t value, uint8_t decimals)
{
  uint64_t scaledValue = value;
  scaledValue *= pow10[decimals];
  return (uint32_t)MIN(scaledValue, UINT32_MAX);
}

int32_t scaleSigned(int32_t value, uint8_t decimals)
{
  int64_t scaledValue = value;
  scaledValue *= pow10[decimals];
  return (int32_t)MAX(MIN(scaledValue, INT32_MAX), INT32_MIN);
}

int32_t scaleFloat(double value, uint8_t decimals)
{
  double scaledValue = value;
  scaledValue *= pow10[decimals];
  return (int32_t)round(scaledValue);
}

uint8_t HovalValue::u8Value(uint8_t decimals) const
{
  switch (type)
  {
  case DataType::BOOL:
    return scaleUnsigned(value.boolValue ? 1 : 0, decimals);
  case DataType::INT8:
    return scaleUnsigned(MAX(0, value.charValue), decimals);
  case DataType::UINT8:
  case DataType::LIST:
    return scaleUnsigned(value.ucharValue, decimals);
  case DataType::FLOAT:
  case DataType::DOUBLE:
    return MIN(MAX(scaleFloat(value.floatValue, decimals), 0), UINT8_MAX);
  default:
    logError("HovalValue", "DataType missmatch");
    return 0;
  }
}

int8_t HovalValue::s8Value(uint8_t decimals) const
{
  switch (type)
  {
  case DataType::BOOL:
    return scaleUnsigned(value.boolValue ? 1 : 0, decimals);
  case DataType::INT8:
    return scaleSigned(value.charValue, decimals);
  case DataType::UINT8:
  case DataType::LIST:
    return scaleUnsigned(value.ucharValue, decimals);
  case DataType::FLOAT:
  case DataType::DOUBLE:
    return MIN(MAX(scaleFloat(value.floatValue, decimals), INT8_MIN), INT8_MAX);
  default:
    logError("HovalValue", "DataType missmatch");
    return 0;
  }
}

uint16_t HovalValue::u16Value(uint8_t decimals) const
{
  switch (type)
  {
  case DataType::BOOL:
    return scaleUnsigned(value.boolValue ? 1 : 0, decimals);
  case DataType::INT8:
    return scaleUnsigned(MAX(0, value.charValue), decimals);
  case DataType::UINT8:
  case DataType::LIST:
    return scaleUnsigned(value.ucharValue, decimals);
  case DataType::UINT16:
    return scaleUnsigned(value.ushortValue, decimals);
  case DataType::INT16:
    return scaleUnsigned(MAX(0, value.shortValue), decimals);
  case DataType::FLOAT:
  case DataType::DOUBLE:
    return MIN(MAX(scaleFloat(value.floatValue, decimals), 0), UINT16_MAX);
  default:
    logError("HovalValue", "DataType missmatch");
    return 0;
  }
}

int16_t HovalValue::s16Value(uint8_t decimals) const
{
  switch (type)
  {
  case DataType::BOOL:
    return scaleUnsigned(value.boolValue ? 1 : 0, decimals);
  case DataType::INT8:
    return scaleSigned(value.charValue, decimals);
  case DataType::UINT8:
  case DataType::LIST:
    return scaleUnsigned(value.ucharValue, decimals);
  case DataType::INT16:
    return scaleSigned(value.shortValue, decimals);
  case DataType::UINT16:
    return scaleUnsigned(value.ushortValue, decimals);
  case DataType::FLOAT:
  case DataType::DOUBLE:
    return MIN(MAX(scaleFloat(value.floatValue, decimals), INT16_MIN), INT16_MAX);
  default:
    logError("HovalValue", "DataType missmatch");
    return 0;
  }
}

#pragma endregion