#pragma once

#include <Arduino.h>
#include <stdint.h>
#include <stdlib.h>
#include <string>

#include "HovalProtocolDecoder.h"
#include <knx/dpt.h>
#include <knx/group_object.h>
#include <knx/knx_value.h>

struct HovalMessageTransformer
{

  const HovalMessageType* type;
  const uint16_t comObject = 0;
  const int16_t prerequisiteComObject = -1; // ComObject that must be sent before this one (-1 = none)
  const bool ignoreIncoming = false;         // If true, incoming Hoval messages of this type are never applied to this comObject
  const bool sendOnlyAsPrerequisite = false; // If true, a direct write to this comObject is not forwarded to Hoval;
                                              // it is only sent when pulled in as another entry's prerequisite
  // const float factor = 1.f;
  const Dpt dpt;
  KNXValue (*const transformer)(HovalMessage*);
  HovalValue (*const inverseTranformer)(KNXValue& value);
  uint32_t lastSeen = 0;
  uint32_t lastSend = 0;

private:
  uint8_t* lastValue = nullptr; // Last encoded KNX value received from Hoval (max 16 bytes)
  uint8_t sizeInMemory;
  bool lastValueInitialized = false; // Flag to indicate if lastValue has been initialized
  bool (*const sendOnChangeFunc)();
  uint32_t (*const sendIntervalMsFunc)(HovalMessage*);
  bool (*const activeFunc)();
  std::string logPrefix();

  HovalMessageTransformer(const HovalMessageTransformer&) = delete;
  HovalMessageTransformer& operator=(const HovalMessageTransformer&) = delete;

public:
  /// @brief Hoval message transfomer used for describing transformation from Hoval Message to KNX Message and
  //         vice versa
  /// @param _type Hoval Message Type this transformer applies to
  /// @param _comObject KNX Comm Object index this transformer applies to
  /// @param _dpt Knx Data Point Type resulting from the transformation
  /// @param _transformer Function to transform HovalMessage to KNXValue
  HovalMessageTransformer(const HovalMessageType* _type, uint8_t _comObject, Dpt _dpt, KNXValue (*_transformer)(HovalMessage*),
                          HovalValue (*_inverseTranformer)(KNXValue&), bool (*_sendOnChange)(), uint32_t (*_sendIntervalMs)(HovalMessage*),
                          bool (*_activeFunc)() = nullptr, int16_t _prerequisiteComObject = -1, bool _ignoreIncoming = false,
                          bool _sendOnlyAsPrerequisite = false)
      : type(_type), comObject(_comObject), prerequisiteComObject(_prerequisiteComObject), ignoreIncoming(_ignoreIncoming),
        sendOnlyAsPrerequisite(_sendOnlyAsPrerequisite), dpt(_dpt), transformer(_transformer), sendOnChangeFunc(_sendOnChange),
        sendIntervalMsFunc(_sendIntervalMs), activeFunc(_activeFunc), inverseTranformer(_inverseTranformer)
  {
    uint8_t dataLength = dpt.dataLength();
    sizeInMemory = (dpt.mainGroup == 16) ? dataLength + 1 : dataLength; // Initialize sizeInMemory
    lastValue = new uint8_t[sizeInMemory]();                            // Initialize lastValue with the correct size
  }

  HovalMessageTransformer(const HovalMessageType* _type, uint8_t _comObject, Dpt _dpt, KNXValue (*_transformer)(HovalMessage*), bool (*_sendOnChange)(),
                          uint32_t (*_sendIntervalMs)(HovalMessage*), bool (*_activeFunc)() = nullptr, int16_t _prerequisiteComObject = -1,
                          bool _ignoreIncoming = false, bool _sendOnlyAsPrerequisite = false)
      : HovalMessageTransformer(_type, _comObject, _dpt, _transformer, nullptr, _sendOnChange, _sendIntervalMs, _activeFunc, _prerequisiteComObject,
                                _ignoreIncoming, _sendOnlyAsPrerequisite)
  {}

  ~HovalMessageTransformer()
  {
    delete[] lastValue;
  }

  KNXValue transform(HovalMessage* message);
  bool active();
  bool sendOnChange();
  uint32_t sendIntervalMs(HovalMessage* message);
  void setLastValue(const KNXValue& value);
  bool valueChanged(const KNXValue& value) const;
};

class Hoval2KNXMapper : public IHovalEventHandler
{
private:
  static HovalMessageTransformer messageTransformers[];
  const static uint8_t numberOfMessageTransformers;
  /// @brief Request interval in ms
  uint32_t requestInterval = 60000;
  /// @brief Id to use for sending requests (i.e., id of the display we're mimicking).
  uint16_t gatewayId;
  /// @brief Id of the device to communicate with (i.e., Hoval HomeVent)
  uint16_t deviceId;

  HovalProtocolHandler* protocolHandler;

  uint32_t lastUpdateRequest = 0;
  uint8_t nextSendIndex = 0;

  void internalSendToKnx(HovalMessage* message, HovalMessageTransformer* messageProcessing);

  void internalSendToHoval(KNXValue& value, HovalMessageTransformer* messageProcessing, bool sendAlways);
  void requestUpdate(const HovalMessageType* type);

  /// @brief Internal implementation of sendToHovalBus.
  /// @param ko KNX Comm-Object to send to Hoval
  /// @param isPrerequisite Set to true when this call is made to send another comObject's prerequisite value.
  ///                        Allows sending comObjects marked as sendOnlyAsPrerequisite.
  void internalSendToHovalBus(GroupObject& ko, bool isPrerequisite);

  std::string logPrefix() { return "Hoval2KNXMapper"; }

public:
  Hoval2KNXMapper(HovalProtocolHandler* protocolHandler) : protocolHandler(protocolHandler) {}

  /// @brief
  /// @param requestInterval Request interval
  void setRequestInterval(uint32_t requestInterval) { this->requestInterval = requestInterval; }

  /// @brief
  /// @param gatewayId Id to use for sending requests (i.e., id of the display we're mimicking).
  void setGatewayId(uint16_t gatewayId)
  {
    this->gatewayId = gatewayId;
    protocolHandler->setGatewayId(this->gatewayId);
  }
  /// @brief
  /// @param deviceId Id of the device to communicate with (i.e., Hoval HomeVent)
  void setDeviceId(uint16_t deviceId) { this->deviceId = deviceId; }

  /// @brief Execute regular tasks (e.g., receiving and sending messages). This needs to be called regularly,
  ///        otherwise CAN message buffer will run full.
  bool task();

  /// @brief Forward HovalMessage to KNX Bus
  /// @param message Message to be send
  void sendToKNXBus(HovalMessage* message);

  /// @brief Forward KNX Message to Hoval CAN Bus
  /// @param ko KNX Comm-Object to send to Hoval
  void sendToHovalBus(GroupObject& ko);

  void hovalEvent(HovalMessage* message);
};
