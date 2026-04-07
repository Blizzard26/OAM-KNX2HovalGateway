#ifndef _HOVAL2KNXMAPPER_H
#define _HOVAL2KNXMAPPER_H

#include <Arduino.h>
#include <stdint.h>
#include <stdlib.h>
#include <string>

#include "HovalProtocolDecoder.h"
#include <knx/dpt.h>
#include <knx/knx_value.h>

struct HovalMessageTransformer
{

  const HovalMessageType* type;
  const uint16_t comObject = 0;
  // const float factor = 1.f;
  const Dpt dpt;
  KNXValue (*transformer)(HovalMessage*);
  uint32_t lastSeen = 0;
  uint32_t lastSend = 0;

private:
  bool (*sendOnChangeFunc)();
  uint32_t (*sendIntervalMsFunc)(HovalMessage*);
  bool (*activeFunc)();
  std::string logPrefix();

public:
  /// @brief Hoval message transfomer used for describing transformation from Hoval Message to KNX Message and
  //         vice versa
  /// @param _type Hoval Message Type this transformer applies to
  /// @param _comObject KNX Comm Object index this transformer applies to
  /// @param _dpt Knx Data Point Type resulting from the transformation
  /// @param _transformer Function to transform HovalMessage to KNXValue
  HovalMessageTransformer(const HovalMessageType* _type, uint8_t _comObject, Dpt _dpt, KNXValue (*_transformer)(HovalMessage*), bool (*_sendOnChange)(),
                          uint32_t (*_sendIntervalMs)(HovalMessage*), bool (*_activeFunc)() = nullptr)
      : type(_type), comObject(_comObject), dpt(_dpt), transformer(_transformer), sendOnChangeFunc(_sendOnChange), sendIntervalMsFunc(_sendIntervalMs),
        activeFunc(_activeFunc)
  {}

  KNXValue transform(HovalMessage* message);
  bool active();
  bool sendOnChange();
  uint32_t sendIntervalMs(HovalMessage* message);
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

  void internalSendToHoval(HovalMessageTransformer* messageProcessing, uint8_t value);
  void internalSendToHoval(HovalMessageTransformer* messageProcessing, uint16_t value);
  void requestUpdate(const HovalMessageType* type);

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
  /// @tparam T Type of the value to be send. Supported types: uint8_t, uint16_t
  /// @param comObjIndex Index of the KNX Comm-Object to send the message to
  /// @param value value to be send.
  template <typename T> void sendToHovalBus(uint8_t comObjIndex, T value);

  void hovalEvent(HovalMessage* message);
};
#endif
