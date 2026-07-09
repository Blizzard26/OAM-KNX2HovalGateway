#pragma once

#include <mcp_can.h>
#include <memory>
#include <stdint.h>
#include <stdlib.h>
// #include <memory.h>
#include "HovalMessage.h"
#include <HashMap.h>
#include <SimpleRingBuffer.h>
#include <string>

#define FILTER_CAN_MESSAGES

#if defined(OPENKNX_DEBUG)
#define LOG_MESSAGES true
#else
#define LOG_MESSAGES false
#endif

struct CanMessage
{
  uint32_t address;
  uint8_t body[8];
  uint8_t bodyLength = 0;
};

#define RECEIVE_STACK_SIZE 8
#define SEND_BUFFER_SIZE 3

class IHovalEventHandler
{
public:
  inline virtual ~IHovalEventHandler() {};
  virtual void hovalEvent(HovalMessage* message) = 0;
};

static uint16_t messageFilterHash(const HovalMessageTypeId& messageTypeId)
{
  uint16_t hashCode = messageTypeId.hashCode();
  // logTrace("HovalProtocolDecoder", "HashCode: %u, dp: ", hashCode, messageTypeId.dataPointId);
  return hashCode;
}

class HovalProtocolHandler
{
  MCP_CAN* canBus;
  uint32_t interruptPin;
  IHovalEventHandler* hovalEventHandler = nullptr;
  uint16_t gatewayId = 0;
  uint32_t lastPingTimestamp = 0;

  uint8_t messageId = 0;

  static const HovalMessageType* messageFilter[];
  static const uint8_t numberOfMessageFilters;

  HashMap<HovalMessageTypeId, const HovalMessageType*, 53> messageTypeHashMap;
  /* Can Message Receive Buffer*/
  SimpleRingBuffer<CanMessage, 10> canReceiveBuffer;

  /* BEGIN Receive LRU Buffer */
  HovalMessage* messageStack[RECEIVE_STACK_SIZE]{};
  uint32_t messageStackLastUsed[RECEIVE_STACK_SIZE]{};

  inline void pushToReceiveStack(HovalMessage* message);
  inline HovalMessage* popFromReceiveStack(uint8_t messageId);
  /* END Receive Buffer*/

  /* BEGIN Send Buffer*/
  SimpleRingBuffer<std::unique_ptr<HovalMessage>, SEND_BUFFER_SIZE> sendBuffer;
  uint8_t sendOffset = 0;
  uint8_t sendErrorCnt = 0;
  /* END Send Buffer*/

  bool logRawMessage = false;
  bool logMessage = false;
  bool logMessageData = false;
  bool logFilteredMessage = false;
  bool logFilteredMessageData = false;

  inline bool tryReadCANMessage(uint32_t& address, uint8_t* body, uint8_t& bodyLength);
  inline bool trySendCANMessage(uint32_t address, uint8_t* body, uint8_t bodyLength);
  inline bool isLogRawMessage() { return LOG_MESSAGES && logRawMessage; }

  inline const HovalMessageType* findMessageType(uint8_t unitType, uint8_t functionGroup, uint8_t functionNumber, uint16_t dataPointId);

  inline void onMessageReceived(uint32_t address, uint8_t* body, size_t bodyLength);
  inline void onSingleMessage(uint16_t sender, uint16_t target, uint8_t* body, size_t bodyLength);
  inline void onMultiPartMessage(uint16_t sender, uint16_t target, uint8_t messageIndex, bool firstMessage, bool lastMessage, uint8_t* body, size_t bodyLength);
  void onMultiPartMessageStart(uint16_t sender, uint16_t target, uint8_t* body, size_t bodyLength);
  void onMultiPartMessageCont(uint8_t messageIndex, bool lastMessage, uint8_t* body, size_t bodyLength);

  inline void processMessage(HovalMessage* message);
  void onHovalEvent(HovalMessage* message);

  inline uint32_t buildAddress(uint8_t messageIndex, bool firstMessage, bool lastMessage, uint16_t sender, uint16_t target);
  inline void sendSingleMessage(HovalMessage* message);

  inline bool doSend();

  inline bool doPing();

  std::string logPrefix() { return "HovalProtocolDecoder"; }

public:
  HovalProtocolHandler(MCP_CAN* canBus, uint32_t interruptPin) : canBus(canBus), interruptPin(interruptPin), messageTypeHashMap(messageFilterHash)
  {
    memset(messageStack, 0, RECEIVE_STACK_SIZE * sizeof(HovalMessage*));
    memset(messageStackLastUsed, 0, RECEIVE_STACK_SIZE * sizeof(uint32_t));

#if defined(USE_CAN_ISR)
    pinMode(interruptPin, INPUT | INPUT_PULLUP);
#endif
  };

  ~HovalProtocolHandler()
  {
    for (uint8_t i = 0; i < RECEIVE_STACK_SIZE; i++)
    {
      delete messageStack[i];
      messageStack[i] = nullptr;
    }
  }

  bool begin();

  bool connect();

  void setHovalEventHandler(IHovalEventHandler* hovalEventHandler) { this->hovalEventHandler = hovalEventHandler; }
  void setGatewayId(uint16_t gatewayId) { this->gatewayId = gatewayId; }

  bool task();

  bool sendMessage(uint16_t sender, uint16_t target, HovalFunctionCode functionCode, const HovalMessageType* type, uint8_t* body, uint8_t bodyLength);

  void requestUpdate(uint16_t sender, uint16_t target, const HovalMessageType* type);
  void write(uint16_t sender, uint16_t target, const HovalMessageType* type, HovalValue& value);

  uint8_t getStackCount();
  uint8_t getNumberOfMessageFilters();

  inline bool isLogFilteredMessage() { return logFilteredMessage; }
  inline bool isLogFilteredMessageData() { return logFilteredMessageData; }
  inline bool isLogMessage() { return logMessage; }
  inline bool isLogMessageData() { return logMessageData; }

  void setLogFilteredMessage(bool enabled) { logFilteredMessage = enabled; }
  void setLogFilteredMessageData(bool enabled) { logFilteredMessageData = enabled; }
  void setLogMessage(bool enabled) { logMessage = enabled; }
  void setLogMessageData(bool enabled) { logMessageData = enabled; }
};
