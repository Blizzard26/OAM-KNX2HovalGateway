#include "HovalProtocolDecoder.h"
#include "OpenKNX.h"
#include "math.h"
#include <string.h>

static const uint8_t MAX_SEND_ERROR_CNT = 10;

static const uint8_t MESSAGE_TYPE_OFFSET = 22;
static const uint8_t MESSAGE_TYPE_MASK = 0b11;
static const uint8_t LAST_MESSAGE = 0b10;
static const uint8_t FIRST_MESSAGE = 0b01;

static const uint16_t UNIT_TYPE_MASK = 0x0FF0;

static const uint8_t FUNCTION_GROUP_MASK = 0x7F;

// messageCount, messageId, functionGroup, functionNumber, and datapointId = 6 Bytes
static const uint8_t MAX_MESSAGE_LENGTH = 8;
static const uint8_t SINGLE_MESSAGE_HEADER_LENGTH = 6;

static const uint8_t MULTI_PART_MESSAGE_HEADER_LENGTH = 7;
static const uint8_t FIRST_MESSAGE_MAX_BODY_LENGTH = MAX_MESSAGE_LENGTH - MULTI_PART_MESSAGE_HEADER_LENGTH;
static const uint8_t FOLLOW_MESSAGE_HEADER_LENGTH = 1;
static const uint8_t FOLLOW_MESSAGE_MAX_BODY_LENGTH = MAX_MESSAGE_LENGTH - FOLLOW_MESSAGE_HEADER_LENGTH;
static const uint8_t MAX_MESSAGE_COUNT = 31;

bool HovalProtocolHandler::begin()
{
  for (uint8_t i = 0; i < numberOfMessageFilters; i++)
  {
    HovalMessageTypeId key(*(messageFilter[i]));
    this->messageTypeHashMap.put(key, messageFilter[i]);
  }

  return true;
}

bool HovalProtocolHandler::connect()
{
  logTraceP("connect");
  // Detach any previously-installed ISR before begin() reconfigures the
  // chip (mode/masks/filters aren't written atomically) — a no-op on the
  // very first connect since stop() tolerates not having been started.
  canTransport->stop();
  return canTransport->begin() && canTransport->start();
}

bool HovalProtocolHandler::task()
{
  CanMessage message;
  if (tryReadCANMessage(message))
  {
    onMessageReceived(message.address, message.body, message.bodyLength);
    return true;
  }

  // Only send if there is nothing to receive to avoid doing both in the same loop
  // Check if there is something to send
  if (doSend())
    return true;

  return doPing();
}

bool HovalProtocolHandler::tryReadCANMessage(CanMessage& message)
{
  if (!canTransport->receive(message))
    return false;

  if (isLogRawMessage())
  {
    logTraceP("CAN ID: %#02X", message.address);
    logIndentUp();
    logHexTraceP(message.body, message.bodyLength);
    logIndentDown();
  }
  return true;
}

bool HovalProtocolHandler::trySendCANMessage(uint32_t address, uint8_t* body, uint8_t bodyLength)
{
  if (isLogRawMessage())
  {
    logTraceP("CAN ID: %#02X", address);
    logIndentUp();
    logHexTraceP(body, bodyLength);
    logIndentDown();
  }
  return canTransport->send(address, body, bodyLength);
}

/**
 * Pushes the message to the LRU stack. If there are no free slots available an old
 * message will be removed from the stack to make space for the new one.
 *
 * <param>message</param>
 */
inline void HovalProtocolHandler::pushToReceiveStack(HovalMessage* message)
{
  // consider removing old message (i.e., older than a few seconds) automatically?
  uint32_t maxAge = 0;
  uint32_t oldestIndex = 0;
  uint32_t now = millis();
  for (uint8_t i = 0; i < RECEIVE_STACK_SIZE; i++)
  {
    if (messageStack[i] == nullptr)
    {
      // Free slot found
      messageStackLastUsed[i] = now;
      messageStack[i] = message;
      return;
    }

    uint32_t age = now - messageStackLastUsed[i];
    if (age > 1000) // Message older than 1 sec.
    {
      HovalMessage* oldMessage = messageStack[i];
      delete oldMessage;

      messageStackLastUsed[i] = now;
      messageStack[i] = message;
      return;
    }

    if (age > maxAge)
    {
      // Remember oldest value
      maxAge = age;
      oldestIndex = i;
    }
  }

  // No free slots available. Evict oldest message and replace by new one.
  logErrorP("Evicting oldest message");
  HovalMessage* oldMessage = messageStack[oldestIndex];
  delete oldMessage;

  messageStackLastUsed[oldestIndex] = now;
  messageStack[oldestIndex] = message;
}

/**
  Remove message with given message ID from the LRU stack and return it.
*/
inline HovalMessage* HovalProtocolHandler::popFromReceiveStack(uint8_t messageId)
{
  for (uint8_t i = 0; i < RECEIVE_STACK_SIZE; i++)
  {
    if (messageStack[i] != nullptr && messageStack[i]->messageId == messageId)
    {
      HovalMessage* message = messageStack[i];
      messageStackLastUsed[i] = 0;
      messageStack[i] = nullptr;
      return message;
    }
  }
  return nullptr;
}

uint8_t HovalProtocolHandler::getStackCount()
{
  uint8_t count = 0;
  for (uint8_t i = 0; i < RECEIVE_STACK_SIZE; i++)
  {
    if (messageStack[i] != nullptr)
    {
      count++;
    }
  }
  return count;
}

uint8_t HovalProtocolHandler::getNumberOfMessageFilters()
{
  return numberOfMessageFilters;
}

#pragma region Received Message Processing

inline const HovalMessageType* HovalProtocolHandler::findMessageType(uint8_t unitType, uint8_t functionGroup, uint8_t functionNumber, uint16_t dataPointId)
{
  HovalMessageTypeId key(unitType, functionGroup, functionNumber, dataPointId);
  return this->messageTypeHashMap.get(key);
}

void HovalProtocolHandler::onMessageReceived(uint32_t address, uint8_t* body, size_t bodyLength)
{
  uint16_t receiver = (uint16_t)(address & 0x7FF);
  address >>= 11;
  uint16_t sender = (uint16_t)(address & 0x7FF);
  address >>= 11;
  uint8_t messageKind = (uint8_t)(address & 0b11);
  address >>= 2;
  uint8_t messageIndex = (uint8_t)(address & 0x1F);

  // Extract message Type bits
  bool firstMessage = messageKind & FIRST_MESSAGE;
  bool lastMessage = messageKind & LAST_MESSAGE;

  if (firstMessage && lastMessage)
  {
    // for SingleMessage messageIndex must be 0x1F
    if (messageIndex == 0x0)
    {
      // Body length should be either
      // * 2 and body should be 01 02
      // * 7 and body should be 01 0A 02 02 08 04 08
      if (bodyLength != 2 || receiver == gatewayId)
      {
        logDebugP("SpecialMessage for %#X->%#X", sender, receiver);
        logHexDebugP(body, bodyLength);
      }
      else
      {
        logTraceP("SpecialMessage");
      }
      return;
    }

    onSingleMessage(sender, receiver, body, bodyLength);
  }
  else
  {
    onMultiPartMessage(sender, receiver, messageIndex, firstMessage, lastMessage, body, bodyLength);
  }
}

void HovalProtocolHandler::onSingleMessage(uint16_t sender, uint16_t target, uint8_t* body, size_t bodyLength)
{
  logTraceP("SingleMessage");

  uint8_t messageCount = body[0] >> 3;
  if (messageCount != 0)
  {
    logErrorP("MessageCount!=0");
    return;
  }

  HovalFunctionCode functionCode = (HovalFunctionCode)body[1];
  if (functionCode == HovalFunctionCode::READ_REQUEST || functionCode == HovalFunctionCode::UPDATE       //
      || functionCode == HovalFunctionCode::ACTIVATE || functionCode == HovalFunctionCode::WRITE_REQUEST //
      || functionCode == HovalFunctionCode::READ_ERROR)
  {
    // Body must contain messageCount, messageId, functionGroup, functionNumber, and datapointId = 6+ Bytes
    if (bodyLength < SINGLE_MESSAGE_HEADER_LENGTH)
    {
      logErrorP("MessageBodyTooShort %u", bodyLength);
      return;
    }

    uint8_t functionGroup = body[2] & FUNCTION_GROUP_MASK;
    uint8_t functionNumber = body[3];
    uint16_t dataPointId = ((uint16_t)body[4]) << 8 | body[5];

    bool response = (target == 0x7FF);
    uint16_t unitId = response ? sender : target;
    uint8_t unitType = (uint8_t)((unitId & UNIT_TYPE_MASK) >> 4);

    // Check if message should be filtered
    const HovalMessageType* type = findMessageType(unitType, functionGroup, functionNumber, dataPointId);
    if (type == nullptr)
    {
      if (isLogFilteredMessage())
      {
        // Message should be filtered so return
        logInfoP("Filtering: fCode:  %#02X | uType: %u, uId: %u | fGrp: %u, fNo: %u, dPId: %u", functionCode, unitType, (unitId & 0xF), functionGroup,
                 functionNumber, dataPointId);
        if (isLogFilteredMessageData() && bodyLength > SINGLE_MESSAGE_HEADER_LENGTH)
        {
          logIndentUp();
          logHexInfoP(body + SINGLE_MESSAGE_HEADER_LENGTH, bodyLength - SINGLE_MESSAGE_HEADER_LENGTH);
          logIndentDown();
        }
      }
      return;
    }

    uint8_t payloadLength = (uint8_t)bodyLength - SINGLE_MESSAGE_HEADER_LENGTH;
    HovalMessage* message = new HovalMessage(0, type, functionCode, sender, target, payloadLength);
    message->addToBody(body + SINGLE_MESSAGE_HEADER_LENGTH, payloadLength);
    processMessage(message);
    delete message;
  }
  else if (functionCode == HovalFunctionCode::PING    //
           || functionCode == HovalFunctionCode::TIME //
  )
  {
    return;
  }
  else if (functionCode == HovalFunctionCode::ANNOUNCEMENT //
           || functionCode == HovalFunctionCode::UNKNOWN1  //
           || functionCode == HovalFunctionCode::UNKNOWN2  //
           || functionCode == HovalFunctionCode::UNKNOWN4  //
           || functionCode == HovalFunctionCode::UNKNOWN5  //
           || functionCode == HovalFunctionCode::UNKNOWN6  //- no single message
  )
  {
    if (isLogFilteredMessage())
    {
      logInfoP("Ignoring fCode: %#02X | %#04X -> %#04X", functionCode, sender, target);
      if (isLogFilteredMessageData() && bodyLength > 0)
      {
        logIndentUp();
        logHexInfoP(body, bodyLength);
        logIndentDown();
      }
    }
    return;
  }
  else if (functionCode == HovalFunctionCode::DISPLAY_TEXT)
  {
    // Ignore
    if (isLogFilteredMessage())
    {
      logDebugP("Ignoring fCode: %#02X", functionCode);
    }
  }
  else
  {
    logErrorP("Unknown Function Code %#02X", functionCode);
    return;
  }
}

void HovalProtocolHandler::onMultiPartMessage(uint16_t sender, uint16_t target, uint8_t messageIndex, bool firstMessage, bool lastMessage, uint8_t* body,
                                              size_t bodyLength)
{

  if (messageIndex == 0)
  {
    // Unknown message type
    logErrorP("Unknown Message Type %x->%x: %c%c", sender, target, (firstMessage ? 'f' : '-'), (lastMessage ? 'l' : '-'));
    logIndentUp();
    logHexErrorP(body, bodyLength);
    logIndentDown();
    return;
  }

  if (firstMessage)
  {
    onMultiPartMessageStart(sender, target, body, bodyLength);
  }
  else
  {
    onMultiPartMessageCont(messageIndex, lastMessage, body, bodyLength);
  }
}

void HovalProtocolHandler::onMultiPartMessageStart(uint16_t sender, uint16_t target, uint8_t* body, size_t bodyLength)
{
  logTraceP("MultiPart-Message");
  // If first message
  // Body must contain messageCount, messageId, messageCode,
  // functionGroup, functionNumber, and datapointId and value = 8 Bytes
  if (bodyLength != MAX_MESSAGE_LENGTH)
  {
    logErrorP("MessageBodyTooShort %u", bodyLength);
    return;
  }

  uint8_t messageCount = body[0] >> 3;
  if (messageCount <= 1)
  {
    logErrorP("MessageCount<=1");
    return;
  }

  HovalFunctionCode functionCode = (HovalFunctionCode)body[2];
  if (functionCode == HovalFunctionCode::READ_REQUEST || functionCode == HovalFunctionCode::UPDATE       //
      || functionCode == HovalFunctionCode::ACTIVATE || functionCode == HovalFunctionCode::WRITE_REQUEST //
      || functionCode == HovalFunctionCode::READ_ERROR)
  {

    //(body[3] & 0xC0 >> 7)
    uint8_t functionGroup = body[3] & FUNCTION_GROUP_MASK;
    uint8_t functionNumber = body[4];
    uint16_t dataPointId = ((uint16_t)body[5]) << 8 | body[6];

    // Get Message Type byte from address
    bool response = (target == 0x7FF);
    uint16_t unitId = response ? sender : target;
    uint8_t unitType = (uint8_t)((unitId & UNIT_TYPE_MASK) >> 4);

    // Check if message should be filtered
    const HovalMessageType* type = findMessageType(unitType, functionGroup, functionNumber, dataPointId);
    if (type == nullptr)
    {
      if (isLogFilteredMessage() && isLogFilteredMessageData())
      {
        // Keep on processing message to get the full body
        type =
            new HovalMessageType(unitType, functionGroup, functionNumber, dataPointId, HovalDataType::RAW, HovalMessage::DYNAMIC_MESSAGE_TYPE, DataType::RAW);
      }
      // Message should be filtered so return
      else if (isLogFilteredMessage())
      {
        // Log message & return
        logDebugP("Filtering: fCode:  %#02X | uType: %u, uId: %u | fGrp: %u, fNo: %u, dPId: %u | cnt: %u", functionCode, unitType, (unitId & 0xF),
                  functionGroup, functionNumber, dataPointId, messageCount);
        return;
      }
      else
      {
        // Skip processing message
        return;
      }
    }

    uint8_t multiPartMessageId = body[1];
    // Initialize message body with maximum size (1 byte for first message + 8-1 bytes for remaining messages)
    uint8_t maxPayloadLength = FIRST_MESSAGE_MAX_BODY_LENGTH + (messageCount - 1) * FOLLOW_MESSAGE_MAX_BODY_LENGTH;
    HovalMessage* message = new HovalMessage(multiPartMessageId, type, functionCode, sender, target, maxPayloadLength);
    message->addToBody(body + MULTI_PART_MESSAGE_HEADER_LENGTH, FIRST_MESSAGE_MAX_BODY_LENGTH);

    // Push message to LRU Stack
    pushToReceiveStack(message);
  }
  else if (functionCode == HovalFunctionCode::UNKNOWN1    //
           || functionCode == HovalFunctionCode::UNKNOWN2 //
           || functionCode == HovalFunctionCode::UNKNOWN4 //
           || functionCode == HovalFunctionCode::UNKNOWN5 //
           || functionCode == HovalFunctionCode::UNKNOWN6)
  {
    if (isLogFilteredMessage())
    {
      logInfoP("Ignoring fCode: %#02X | %#04X -> %#04X", functionCode, sender, target);
      if (isLogFilteredMessageData() && bodyLength > 0)
      {
        logIndentUp();
        logHexInfoP(body, bodyLength);
        logIndentDown();
      }
    }
    return;
  }
  else if (functionCode == HovalFunctionCode::DISPLAY_TEXT)
  {
    // Ignore
    if (isLogFilteredMessage())
    {
      logInfoP("Ignoring fCode: %#02X", functionCode);
    }
  }
  else
  {
    logErrorP("Unknown Function Code %#02X", functionCode);
    return;
  }
}

void HovalProtocolHandler::onMultiPartMessageCont(uint8_t messageIndex, bool lastMessage, uint8_t* body, size_t bodyLength)
{
  logTraceP("MultiPart-Cont");
  // Follow Up message of a multi part message
  // Byte 0 of body is the messageId
  uint8_t multiPartMessageId = body[0];

  // Get message by id
  HovalMessage* message = popFromReceiveStack(multiPartMessageId);
  if (message == nullptr)
  {
    // Seems we missed the first message or it got filtered, return
    logTraceP("Unknown Message Id %#02X", multiPartMessageId);
    if (isLogFilteredMessage() && isLogFilteredMessageData() && bodyLength > 0)
    {
      logIndentUp();
      logHexTraceP(body, bodyLength);
      logIndentDown();
    }
    return;
  }

  // Message index counts from 31 down to 0
  uint8_t messageNo = 31 - messageIndex;

  // first message has two bytes, subsequent message have 8-1 bytes (for messageId)
  uint8_t offset = (messageNo - 1) * FOLLOW_MESSAGE_MAX_BODY_LENGTH + FIRST_MESSAGE_MAX_BODY_LENGTH;

  uint8_t payloadLength = bodyLength - FOLLOW_MESSAGE_HEADER_LENGTH;
  // Sanity check to not overflow
  if (offset + payloadLength > message->maxBodyLength)
  {
    logErrorP("Illegal Message Length");
    delete message;
    return;
  }
  if (offset != message->messageBodyLength)
  {
    logErrorP("Missed Message: %u vs %u", message->messageBodyLength, offset);
    logErrorP("Missed: fCode:  %#02X | uType: %u, uId: %u | fGrp: %u, fNo: %u, dPId: %u", message->functionCode, message->messageType->unitType,
              (message->senderId & 0xF), message->messageType->functionGroup, message->messageType->functionNumber, message->messageType->dataPointId);
    delete message;
    return;
  }
  if (!lastMessage && bodyLength != MAX_MESSAGE_LENGTH)
  {
    logErrorP("Intermediate message has less than 8 bytes");
    delete message;
    return;
  }

  // Copy message body to message
  message->addToBody(body + 1, payloadLength);

  if (lastMessage)
  {
    // Check if expected message size matches actual
    if (message->maxBodyLength - message->messageBodyLength > FOLLOW_MESSAGE_MAX_BODY_LENGTH)
    {
      logErrorP("Multi-Part Count missmatch: %u vs. %u", message->messageBodyLength, message->maxBodyLength);
      delete message;
      return;
    }

    if (!message->validateCrc())
    {
      logErrorP("Crc Validation failed: fCode:  %#02X | uType: %u, uId: %u | fGrp: %u, fNo: %u, dPId: %u", message->functionCode,
                message->messageType->unitType, (message->senderId & 0xF), message->messageType->functionGroup, message->messageType->functionNumber,
                message->messageType->dataPointId);
      if (isLogFilteredMessage() && isLogFilteredMessageData())
      {
        logIndentUp();
        logHexErrorP(message->messageBody, message->messageBodyLength + 2);
        logIndentDown();
      }
    }
    else if (message->messageType->decimals == HovalMessage::DYNAMIC_MESSAGE_TYPE)
    {
      if (isLogFilteredMessage())
      {
        logInfoP("Filtering: fCode:  %#02X | uType: %u, uId: %u | fGrp: %u, fNo: %u, dPId: %u", message->functionCode, message->messageType->unitType,
                 (message->senderId & 0xF), message->messageType->functionGroup, message->messageType->functionNumber, message->messageType->dataPointId);
        if (isLogFilteredMessageData())
        {
          logIndentUp();
          logHexInfoP(message->messageBody, message->messageBodyLength);
          logIndentDown();
        }
      }
    }
    else
    {
      processMessage(message);
    }
    delete message;
  }
  else
  {
    pushToReceiveStack(message);
  }
}

void HovalProtocolHandler::processMessage(HovalMessage* message)
{

  if (isLogMessage())
  {
    logInfoP("Rec: fCode: %#02X | uType: %u, uId: %u | fGrp: %u, fNo: %u, dPId: %u", message->functionCode, message->messageType->unitType, message->senderId,
             message->messageType->functionGroup, message->messageType->functionNumber, message->messageType->dataPointId);
    if (isLogMessageData() && message->messageBodyLength > 0)
    {
      logIndentUp();
      // READ_ERROR has different content structure
      if (message->functionCode != HovalFunctionCode::READ_ERROR)
      {
        char buffer[60];
        message->printBody(buffer, sizeof(buffer));
        logInfoP("body: %s", buffer);
      }
      else
      {
        logHexInfoP(message->messageBody, message->messageBodyLength);
      }

      logIndentDown();
    }
  }
  onHovalEvent(message);
}

void HovalProtocolHandler::onHovalEvent(HovalMessage* message)
{
  if (hovalEventHandler != nullptr)
  {
    hovalEventHandler->hovalEvent(message);
  }
}

#pragma endregion

bool HovalProtocolHandler::sendMessage(uint16_t sender, uint16_t target, HovalFunctionCode functionCode, const HovalMessageType* type, uint8_t* messageBody,
                                       uint8_t bodyLength)
{
  if (sendBuffer.isFull())
  {
    logErrorP("SendBuffer Full");
    // No more space in Ringbuffer
    return false;
  }

  std::unique_ptr<HovalMessage> message(new HovalMessage(messageId++, type, functionCode, sender, target, bodyLength));
  message->addToBody(messageBody, bodyLength);

  logDebugP("Queueing: %u | fCode: %#02X | uType: %u, uId: %u | fGrp: %u, fNo: %u, dPId: %u", message->messageId, message->functionCode,
            message->messageType->unitType, message->senderId, message->messageType->functionGroup, message->messageType->functionNumber,
            message->messageType->dataPointId);

  sendBuffer.push(std::move(message));

  return true;
}

bool HovalProtocolHandler::doPing()
{
  if (lastPingTimestamp != 0 && !delayCheck(lastPingTimestamp, 5000))
  {
    return false;
  }

  uint32_t address = buildAddress(0, true, true, this->gatewayId, 0x7FF);

  uint8_t body[2] = {0x01, 0x02};

  trySendCANMessage(address, body, 2);

  lastPingTimestamp = millis();
  return true;
}

#pragma region Message Send

bool HovalProtocolHandler::doSend()
{
  if (sendBuffer.isEmpty())
  {
    // No message on stack
    return false;
  }

  HovalMessage* message = sendBuffer.peek()->get();

  if (message->messageBodyLength <= (MAX_MESSAGE_LENGTH - SINGLE_MESSAGE_HEADER_LENGTH))
  {
    logTraceP("Sending Single");
    // Single Message
    sendSingleMessage(message);
  }
  else
  {
    // Calculating of CRC is currently unknown.
    logErrorP("Sending of Multi-Part Messages is currently not supported.");
    sendBuffer.pop()->reset();
  }
  return true;
}

void HovalProtocolHandler::sendSingleMessage(HovalMessage* message)
{
  uint32_t address = buildAddress(0x1F, true, true, message->senderId, message->targetId);

  uint8_t bodyLength = message->messageBodyLength;
  uint8_t messageLength = SINGLE_MESSAGE_HEADER_LENGTH + bodyLength;

  uint8_t body[MAX_MESSAGE_LENGTH] = {0};

  // Bits 7-3: Number of message of a multi-part message (including start and end message).
  // In case of single message message count is 0 (instead of 1).
  // Bits 3 -1: Usage unknown. Currently always 0b001
  body[0] = 0b001;
  body[1] = (uint8_t)message->functionCode;
  body[2] = message->messageType->functionGroup;
  body[3] = message->messageType->functionNumber;
  body[4] = (message->messageType->dataPointId >> 8) & 0xFF;
  body[5] = message->messageType->dataPointId & 0xFF;

  // two bytes message body left
  if (bodyLength > 0)
  {
    body[6] = message->messageBody[0];
  }
  if (bodyLength > 1)
  {
    body[7] = message->messageBody[1];
  }

  if (message->functionCode == HovalFunctionCode::WRITE_REQUEST)
  {
    logInfoP("Sending: address: %#02X", address);
    logIndentUp();
    logHexInfoP(body, messageLength);
    logIndentDown();
  }

  boolean success = trySendCANMessage(address, body, messageLength);

  if (success)
  {
    sendBuffer.pop()->reset();
    sendErrorCnt = 0;
  }
  else
  {
    logErrorP("Error sending message");
    sendErrorCnt++;
    if (sendErrorCnt > MAX_SEND_ERROR_CNT)
    {
      sendBuffer.pop()->reset();
      sendErrorCnt = 0;
    }
  }
}

uint32_t HovalProtocolHandler::buildAddress(uint8_t messageIndex, bool firstMessage, bool lastMessage, uint16_t sender, uint16_t target)
{
  return (messageIndex & 0x1F) << 24 | ((lastMessage ? LAST_MESSAGE : 0) | (firstMessage ? FIRST_MESSAGE : 0)) << 22 | (sender & 0x7FF) << 11 |
         (target & 0x7FF) << 0;
}

#pragma endregion

void HovalProtocolHandler::requestUpdate(uint16_t sender, uint16_t target, const HovalMessageType* type)
{
  sendMessage(sender, target, HovalFunctionCode::READ_REQUEST, type, nullptr, 0);
}

static inline uint8_t checkBounds(uint8_t value, int32_t minValue, int32_t maxValue)
{
  if (minValue != INT32_MIN && value < minValue)
    return minValue < 0 ? 0 : (uint8_t)minValue;
  if (maxValue != INT32_MAX && value > maxValue)
    return maxValue > UINT8_MAX ? UINT8_MAX : (uint8_t)maxValue;
  return value;
}

static inline int8_t checkBounds(int8_t value, int32_t minValue, int32_t maxValue)
{
  if (minValue != INT32_MIN && value < minValue)
    return minValue < INT8_MIN ? INT8_MIN : (int8_t)minValue;
  if (maxValue != INT32_MAX && value > maxValue)
    return maxValue > INT8_MAX ? INT8_MAX : (int8_t)maxValue;
  return value;
}

static inline uint16_t checkBounds(uint16_t value, int32_t minValue, int32_t maxValue)
{
  if (minValue != INT32_MIN && value < minValue)
    return minValue < 0 ? 0 : (uint16_t)minValue;
  if (maxValue != INT32_MAX && value > maxValue)
    return maxValue > UINT16_MAX ? UINT16_MAX : (uint16_t)maxValue;
  return value;
}

static inline int16_t checkBounds(int16_t value, int32_t minValue, int32_t maxValue)
{
  if (minValue != INT32_MIN && value < minValue)
    return minValue < INT16_MIN ? INT16_MIN : (int16_t)minValue;
  if (maxValue != INT32_MAX && value > maxValue)
    return maxValue > INT16_MAX ? INT16_MAX : (int16_t)maxValue;
  return value;
}

void HovalProtocolHandler::write(uint16_t sender, uint16_t target, const HovalMessageType* type, HovalValue& value)
{

  switch (type->rawType)
  {
  case HovalDataType::U8: {
    uint8_t v = checkBounds(value.u8Value(type->decimals), type->minValue, type->maxValue);
    sendMessage(sender, target, HovalFunctionCode::WRITE_REQUEST, type, &v, sizeof(v));
    break;
  }
  case HovalDataType::U16: {
    uint16_t v16 = checkBounds(value.u16Value(type->decimals), type->minValue, type->maxValue);
    uint8_t buffer[2] = {(uint8_t)(v16 >> 8), (uint8_t)(v16 & 0xFF)};
    sendMessage(sender, target, HovalFunctionCode::WRITE_REQUEST, type, buffer, sizeof(buffer));
    break;
  }
  case HovalDataType::S8: {
    int8_t sv = checkBounds(value.s8Value(type->decimals), type->minValue, type->maxValue);
    uint8_t buffer = *(uint8_t*)&sv;  // or memcpy
    sendMessage(sender, target, HovalFunctionCode::WRITE_REQUEST, type, &buffer, sizeof(buffer));
    break;
  }
  case HovalDataType::S16: {
    int16_t sv16 = checkBounds(value.s16Value(type->decimals), type->minValue, type->maxValue);
    uint8_t buffer[2] = {(uint8_t)(sv16 >> 8), (uint8_t)(sv16 & 0xFF)};
    sendMessage(sender, target, HovalFunctionCode::WRITE_REQUEST, type, buffer, sizeof(buffer));
    break;
  }

  case HovalDataType::U32:
  case HovalDataType::S32:
  case HovalDataType::S64:
  case HovalDataType::RAW:
    // Currently not supported as it requires a multi-part message
    logErrorP("DataType not supported yet");
    break;
  default:
    logErrorP("Unknown DataType");
    break;
  }
}
