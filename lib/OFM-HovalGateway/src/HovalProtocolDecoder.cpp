#include "HovalProtocolDecoder.h"
#include "OpenKNX.h"
#include "hardware.h"
#include "math.h"
#include <mcp2515_can_dfs.h>
#include <string.h>

#define MAX_SEND_ERROR_CNT 10

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

#if !defined(CAN_CLOCK)
#error "CAN_CLOCK not defined. "
#endif

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
  while (CAN_OK != canBus->begin(CAN_50KBPS, CAN_CLOCK))
  { // init can bus : baudrate = 50k
    logErrorP("CAN init fail, retry...");
    delay(100);
  }

  // canBus->setMode(MODE_LISTENONLY);
  bool success = canBus->setMode(MODE_NORMAL) == MCP2515_OK;
  if (!success)
  {
    logTraceP("setMode failed");
  }
  // Allow EXT-Messages to be received
#if defined(FILTER_CAN_MESSAGES)
  success &= canBus->init_Mask(0, true, 0x000007FF) == MCP2515_OK; // Filter enabled
#else
  success &= canBus->init_Mask(0, true, 0x00000000) == MCP2515_OK; // Filter disabled
#endif
  if (!success)
  {
    logTraceP("init_Mask failed");
  }
  success &= canBus->init_Filt(0, true, 0x000007FF) == MCP2515_OK;
  if (!success)
  {
    logTraceP("init_Filt failed");
  }
  canBus->enableTxInterrupt(false);
  return success;
}

bool HovalProtocolHandler::task()
{

  if (canReceiveBuffer.available()
#if defined(USE_CAN_ISR)
      // Interrupt pin is pulled low when there are messages to read
      && digitalRead(interruptPin) == LOW
#endif
  )
  {
    CanMessage* canMessage = canReceiveBuffer.beginPush();
    // ASSERT(canMessage != nullptr, "Can Receive Buffer Full"); // Should never happen because we check available first
    //  Check if there is something to receive
    if (tryReadCANMessage(canMessage->address, canMessage->body, canMessage->bodyLength)) // read data,  len: data length, buf: data buf
    {
      canReceiveBuffer.endPush();
      return true;
    }
  }

  if (!canReceiveBuffer.isEmpty())
  {
    CanMessage* message = canReceiveBuffer.pop();
    onMessageReceived(message->address, message->body, message->bodyLength);
    return true;
  }

  // Only send if there is nothing to receive to avoid doing both in the same loop
  // Check if there is something to send
  if (doSend())
    return true;

  return doPing();
}

bool HovalProtocolHandler::tryReadCANMessage(uint32_t& address, uint8_t* body, uint8_t& bodyLength)
{
  // Try reading data
  if (CAN_OK == this->canBus->readMsgBufID(&address, &bodyLength, body))
  {
    if (isLogRawMessage())
    {
      logTraceP("CAN ID: %#02X", address);
      logIndentUp();
      logHexTraceP(body, bodyLength);
      logIndentDown();
    }
    return true;
  }
  return false;
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
  // Don't wait for sent. Timeout in mcp2515_can.cpp is too short and will result in CAN_SENDMSGTIMEOUT
  uint8_t result = this->canBus->sendMsgBuf(address, true, false, bodyLength, body, false);
  if (result != CAN_OK)
    logErrorP("Send Failed: %u", result);

  return result == CAN_OK;
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
    // TODO this could be improved and moved to the constructor - or at least tho the message class
    memcpy(message->messageBody, body + SINGLE_MESSAGE_HEADER_LENGTH, payloadLength);
    message->messageBodyLength = payloadLength;
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

    memcpy(message->messageBody, body + MULTI_PART_MESSAGE_HEADER_LENGTH, FIRST_MESSAGE_MAX_BODY_LENGTH);
    message->messageBodyLength = FIRST_MESSAGE_MAX_BODY_LENGTH;

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

  logTraceP("Offset: %u", offset);
  // Copy message body to message
  memcpy(message->messageBody + offset, body + 1, payloadLength);
  message->messageBodyLength += payloadLength;

  if (lastMessage)
  {
    // Check if expected message size matches actual
    if (message->maxBodyLength - message->messageBodyLength > FOLLOW_MESSAGE_MAX_BODY_LENGTH)
    {
      logErrorP("Multi-Part Count missmatch: %u vs. %u", message->messageBodyLength, message->maxBodyLength);
      delete message;
      return;
    }

    // Last two bytes are some form of CRC
    message->crc = (uint16_t)message->messageBody[message->messageBodyLength - 2] << 8 | message->messageBody[message->messageBodyLength - 1];
    message->messageBodyLength -= 2;

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
    // No more space in Ringbuffer
    return false;
  }

  HovalMessage* message = new HovalMessage(messageId++, type, functionCode, sender, target, bodyLength);
  memcpy(message->messageBody, messageBody, bodyLength);
  message->messageBodyLength = bodyLength;

  sendBuffer.push(message);

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

  HovalMessage* message = *(sendBuffer.peek());
  uint8_t bodyLength = message->messageBodyLength;

  if (bodyLength <= (MAX_MESSAGE_LENGTH - SINGLE_MESSAGE_HEADER_LENGTH))
  {
    logTraceP("Sending Single");
    // Single Message
    uint32_t address = buildAddress(0x1F, true, true, message->senderId, message->targetId);

    uint8_t body[MAX_MESSAGE_LENGTH];
    memset(body, 0, MAX_MESSAGE_LENGTH);
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
    uint8_t messageLength = SINGLE_MESSAGE_HEADER_LENGTH + bodyLength;
    if (trySendCANMessage(address, body, messageLength))
    {
      delete *(sendBuffer.pop());
      sendErrorCnt = 0;
    }
    else
    {
      sendErrorCnt++;
      if (sendErrorCnt > MAX_SEND_ERROR_CNT)
      {
        delete *(sendBuffer.pop());
        sendErrorCnt = 0;
      }
    }
  }
  else
  {
    // First message takes 1 byte, subsequent messages take 7 bytes
    uint8_t followMessageBodyLength = bodyLength - MULTI_PART_MESSAGE_HEADER_LENGTH;
    uint8_t messageCount =
        followMessageBodyLength / FOLLOW_MESSAGE_MAX_BODY_LENGTH + ((followMessageBodyLength % FOLLOW_MESSAGE_MAX_BODY_LENGTH != 0) ? 1 : 0) + 1;
    if (sendOffset == 0)
    {
      uint32_t address = buildAddress(0x1F, true, false, message->senderId, message->targetId);

      uint8_t body[MAX_MESSAGE_LENGTH];
      memset(body, 0, MAX_MESSAGE_LENGTH);
      // Bits 7-3: Number of message of a multi-part message (including start and end message).
      // In case of single message message count is 0 (instead of 1).
      // Bits 3 -1: Usage unknown. Currently always 0b001
      body[0] = messageCount << 3 | 0b001;
      body[1] = message->messageId;
      body[2] = (uint8_t)message->functionCode;
      body[3] = message->messageType->functionGroup;
      body[4] = message->messageType->functionNumber;
      body[5] = (message->messageType->dataPointId >> 8) & 0xFF;
      body[6] = message->messageType->dataPointId & 0xFF;
      body[7] = message->messageBody[0];

      if (trySendCANMessage(address, body, MAX_MESSAGE_LENGTH))
      {
        sendErrorCnt = 0;
        sendOffset = 1;
      }
      else
      {
        sendErrorCnt++;
        if (sendErrorCnt > MAX_SEND_ERROR_CNT)
        {
          delete *(sendBuffer.pop());
          sendErrorCnt = 0;
        }
      }
    }
    else
    {
      uint8_t messageIndex = (sendOffset - 1) / FOLLOW_MESSAGE_MAX_BODY_LENGTH;

      uint32_t address = buildAddress(messageIndex, false, messageIndex == messageCount, message->senderId, message->targetId);

      uint8_t body[MAX_MESSAGE_LENGTH];
      memset(body, 0, MAX_MESSAGE_LENGTH);
      body[0] = messageId;
      // 7 bytes message body left
      uint8_t payloadLength = min(FOLLOW_MESSAGE_MAX_BODY_LENGTH, bodyLength - sendOffset);
      memcpy(body + FOLLOW_MESSAGE_HEADER_LENGTH, message->messageBody + sendOffset, payloadLength);
      uint8_t messageLength = FOLLOW_MESSAGE_HEADER_LENGTH + payloadLength;

      if (trySendCANMessage(address, body, messageLength))
      {
        sendErrorCnt = 0;
        sendOffset += payloadLength;

        if (sendOffset >= message->messageBodyLength)
        {
          delete *(sendBuffer.pop());
        }
      }
      else
      {
        sendErrorCnt++;
        if (sendErrorCnt > MAX_SEND_ERROR_CNT)
        {
          delete *(sendBuffer.pop());
          sendErrorCnt = 0;
        }
      }
    }
  }
  return true;
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

void HovalProtocolHandler::write(uint16_t sender, uint16_t target, const HovalMessageType* type, uint8_t value)
{
  sendMessage(sender, target, HovalFunctionCode::WRITE_REQUEST, type, &value, 1);
}

void HovalProtocolHandler::write(uint16_t sender, uint16_t target, const HovalMessageType* type, uint16_t value)
{
  // TODO: Might need to invert bytes?
  sendMessage(sender, target, HovalFunctionCode::WRITE_REQUEST, type, (uint8_t*)&value, 2);
}
