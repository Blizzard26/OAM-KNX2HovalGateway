#pragma once

#include <stddef.h>
#include <stdint.h>
#include <time.h>

enum class HovalDataType
{
  U8,
  U16,
  U32,
  S8,
  S16,
  S32,
  S64,
  RAW
};

enum class DataType
{
  BOOL,
  FLOAT,
  DOUBLE,
  INT8,
  UINT8,
  INT16,
  UINT16,
  INT32,
  UINT32,
  INT64,
  LIST,
  STRING,
  ERRORTYPE,
  RAW

};

struct ErrorMessage
{
  time_t appearance_time;
  time_t disappear_time;
  uint16_t source;
  uint8_t function_group;
  uint8_t function_number;
  char error_type;
  uint16_t error_code;
};

struct HovalMessageType
{
  const uint8_t unitType = 0;
  const uint8_t functionGroup = 0;
  const uint8_t functionNumber = 0;
  const uint16_t dataPointId = 0;

  const HovalDataType rawType;
  const uint8_t decimals = 0;

  const DataType dataType;

  const int32_t minValue;
  const int32_t maxValue;

  HovalMessageType(uint16_t _unitType, uint8_t _functionGroup, uint8_t _functionNumber, uint16_t _dataPointId, HovalDataType _rawType, uint8_t _decimals,
                   DataType _dataType, int32_t _minValue = INT32_MIN, int32_t _maxValue = INT32_MAX)
      : unitType(_unitType), functionGroup(_functionGroup), functionNumber(_functionNumber), dataPointId(_dataPointId), rawType(_rawType), decimals(_decimals),
        dataType(_dataType), minValue(_minValue), maxValue(_maxValue)
  {}

private:
  // Disable copy constructor
  HovalMessageType(const HovalMessageType&);
};

struct HovalMessageTypeId
{
  const uint8_t unitType = 0;
  const uint8_t functionGroup = 0;
  const uint8_t functionNumber = 0;
  const uint16_t dataPointId = 0;

  HovalMessageTypeId(const HovalMessageType& type)
      : unitType(type.unitType), functionGroup(type.functionGroup), functionNumber(type.functionNumber), dataPointId(type.dataPointId)
  {}

  HovalMessageTypeId(uint16_t _unitType, uint8_t _functionGroup, uint8_t _functionNumber, uint16_t _dataPointId)
      : unitType(_unitType), functionGroup(_functionGroup), functionNumber(_functionNumber), dataPointId(_dataPointId)
  {}

  bool operator==(const HovalMessageTypeId& other) const
  {
    return dataPointId == other.dataPointId && functionNumber == other.functionNumber && functionGroup == other.functionGroup && unitType == other.unitType;
  }

  bool operator!=(const HovalMessageTypeId& other) const { return !(*this == other); }

  uint16_t hashCode() const { return dataPointId + 31 * functionNumber + 31 * 31 * functionGroup + 31 * 31 * 31 * unitType; }
};

enum HovalFunctionCode
{
  PING = 0x02,          // Ping/Announcement (alle 5s)
  TIME = 0x08,          // Unix-Zeit
  UNKNOWN1 = 0x0A,      // Alle 10s dieselbe Nachricht (0x0202080408)
  READ_REQUEST = 0x40,  // Adressen Lesen
  UPDATE = 0x42,        // Aktualisierten Wert senden (Antwort auf 0x40)
  ACTIVATE = 0x44,      // Funktion aktivieren (z.B. Pause Modus)
  WRITE_REQUEST = 0x46, // Neuen Wert in Adresse schreiben
  UNKNOWN2 = 0x4C,      // Alle 10s (Nachrichten haben Muster)
  ANNOUNCEMENT = 0x50,  // Alle 10s dieselbe Nachricht (0x02080408)
  UNKNOWN4 = 0x52,      // Alle 10s (Nachrichten haben teilweise Muster)
  READ_ERROR = 0x56,    // Fehler beim Lesen (z.B. Falls Sensor für Lesen nicht vorhanden)
  UNKNOWN5 = 0x61,      // Alle 7s
  DISPLAY_TEXT = 0x62,  // Multi-Multi-Part Nachrichten?
  UNKNOWN6 = 0x74,      // Alle 5s (Funktionsgruppe zählt von 0-4, Muster wiederholt sich alle 25s)
};

struct HovalValue
{
public:
  HovalValue(uint8_t value, DataType type = DataType::UINT8);
  HovalValue(uint16_t _value) : type(DataType::UINT16) { value.ushortValue = _value; }

  HovalValue(float _value) : type(DataType::FLOAT) { value.floatValue = _value; }

  uint8_t u8Value(uint8_t decimals) const;
  uint16_t u16Value(uint8_t decimals) const;
  int8_t s8Value(uint8_t decimals) const;
  int16_t s16Value(uint8_t decimals) const;

private:
  union Value
  {
    bool boolValue;
    uint8_t ucharValue;
    uint16_t ushortValue;
    //uint32_t uintValue;
    //uint64_t ulongValue;
    int8_t charValue;
    int16_t shortValue;
    //int32_t intValue;
    //int64_t longValue;
    float floatValue;
    double doubleValue;
    //const char* stringValue;
    //struct tm timeValue;
  };

  DataType type;
  Value value;
};

struct HovalMessage
{
  static const uint8_t DYNAMIC_MESSAGE_TYPE = 0xFF;

  const uint8_t messageId;

  const HovalMessageType* messageType;
  const HovalFunctionCode functionCode;
  const uint16_t senderId;
  const uint16_t targetId;
  const uint8_t maxBodyLength;

  uint8_t* messageBody = nullptr;
  uint8_t messageBodyLength = 0;
  uint16_t crc = 0x0000;

  HovalMessage(uint8_t _messageId, const HovalMessageType* _messageType, HovalFunctionCode _functionCode, uint16_t _senderId, uint16_t _targetId,
               uint8_t _maxBodyLength);

  ~HovalMessage();

  bool validateCrc();

  void printBody(char* buf, uint8_t bufLen);

  float floatValue() const;
  double doubleValue() const;

  /* Unsigned */
  uint8_t u8Value() const;
  uint16_t u16Value() const;
  uint32_t u32Value() const;
  /* Signed */
  int8_t s8Value() const;
  int16_t s16Value() const;
  int32_t s32Value() const;
  int64_t s64Value() const;
  /* Bit List*/
  uint8_t list() const;
  /* Raw String */
  // byte* raw();
  ErrorMessage errorMessage() const;

  operator uint8_t() const;
  operator uint16_t() const;
  operator uint32_t() const;
  operator int8_t() const;
  operator int16_t() const;
  operator int32_t() const;
  operator int64_t() const;
  operator ErrorMessage() const;

private:
  // Disable copy constructor
  HovalMessage(const HovalMessage&);

  inline static constexpr uint16_t littleEndianToUint16(const uint8_t* body);
  inline static constexpr uint16_t bigEndianToUint16(const uint8_t* body);
  inline static constexpr uint32_t bigEndianToUint32(const uint8_t* body);
  inline static constexpr int64_t bigEndianToInt64(const uint8_t* body);

  char mapErrorType(uint8_t errorType) const;
};
