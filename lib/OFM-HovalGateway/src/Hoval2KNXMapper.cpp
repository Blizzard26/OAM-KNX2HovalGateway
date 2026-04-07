#include "Hoval2KNXMapper.h"

#include "OpenKNX.h"
#include "OpenKNX/Log/Logger.h"
#include <knx.h>

#include "HovalProtocolDecoder.h"

#ifndef delayCheck
// clang-format off
#define delayCheck(last, duration) (millis() - last >= duration)
// clang-format on
#endif

#define UPDATE_REQUEST_RATE_MS 250

bool Hoval2KNXMapper::task()
{
  if (requestInterval == 0)
  {
    return false;
  }

  if (delayCheck(lastUpdateRequest, UPDATE_REQUEST_RATE_MS))
  {
    uint32_t now = millis();
    lastUpdateRequest = now;

    if (knx.getGroupObject(messageTransformers[nextSendIndex].comObject).transmitEnable() && messageTransformers[nextSendIndex].active() &&
        delayCheck(messageTransformers[nextSendIndex].lastSeen, requestInterval))
    {
      messageTransformers[nextSendIndex].lastSeen = now; // Set lastSeen to now so we don't request too often
      requestUpdate(messageTransformers[nextSendIndex].type);
      return true;
    }
    nextSendIndex = (nextSendIndex + 1) % numberOfMessageTransformers;
  }
  return false;
}

void Hoval2KNXMapper::requestUpdate(const HovalMessageType* type)
{
  logDebugP("Requesting Update: %u:%u:%u", type->functionGroup, type->functionNumber, type->dataPointId);
  protocolHandler->requestUpdate(this->gatewayId, this->deviceId, type);
}

void Hoval2KNXMapper::hovalEvent(HovalMessage* message)
{

  if (message->functionCode == HovalFunctionCode::UPDATE)
  {
    if (message->senderId == this->deviceId)
    {
      sendToKNXBus(message);
    }
    else
    {
      logDebugP("Unknown device: %u", message->senderId);
    }
  }
  else if (message->functionCode != HovalFunctionCode::READ_REQUEST)
  {
    if (message->targetId == this->gatewayId)
    {
      logInfoP("Read request received: %u:%u:%u", message->messageType->functionGroup, message->messageType->functionNumber, message->messageType->dataPointId);
    }
  }
}

void Hoval2KNXMapper::sendToKNXBus(HovalMessage* message)
{
  bool found = false;
  for (int i = 0; i < numberOfMessageTransformers; i++)
  {
    HovalMessageTransformer* processing = &(messageTransformers[i]);
    if (processing->type == message->messageType)
    {
      internalSendToKnx(message, processing);
      processing->lastSeen = millis();
      found = true;
    }
  }
  if (!found)
  {
    logDebugP("Unmapped message type: fGrp: %u, fNo: %u, dPId: %u", message->messageType->functionGroup, message->messageType->functionNumber,
              message->messageType->dataPointId);
  }
}

void Hoval2KNXMapper::internalSendToHoval(HovalMessageTransformer* messageProcessing, uint8_t value)
{
  // TODO Implement send to Hoval
}

void Hoval2KNXMapper::internalSendToHoval(HovalMessageTransformer* messageProcessing, uint16_t value)
{
  // TODO Implement send to Hoval
}

template <typename T> void Hoval2KNXMapper::sendToHovalBus(uint8_t comObjIndex, T value)
{
  for (int i = 0; i < numberOfMessageTransformers; i++)
  {
    HovalMessageTransformer* processing = &(messageTransformers[i]);
    if (processing->comObject == comObjIndex)
    {
      internalSendToHoval(processing, value);
    }
  }
}

template void Hoval2KNXMapper::sendToHovalBus<uint8_t>(byte objectIndex, uint8_t value);
template void Hoval2KNXMapper::sendToHovalBus<uint16_t>(byte objectIndex, uint16_t value);

void Hoval2KNXMapper::internalSendToKnx(HovalMessage* message, HovalMessageTransformer* messageProcessing)
{
  GroupObject& groupObject = knx.getGroupObject(messageProcessing->comObject);

  KNXValue value = messageProcessing->transform(message);
  bool changed = groupObject.valueNoSendCompare(value, messageProcessing->dpt);

  bool requestSend = (changed && messageProcessing->sendOnChange());
  if (!requestSend)
  {
    uint32_t sendInterval = messageProcessing->sendIntervalMs(message);
    requestSend = (sendInterval > 0 && delayCheck(messageProcessing->lastSend, sendInterval));
  }

  if (requestSend)
  {
    messageProcessing->lastSend = millis();
    groupObject.objectWritten();
  }
}

std::string HovalMessageTransformer::logPrefix()
{
  return "HovalMessageTransformer";
}

KNXValue HovalMessageTransformer::transform(HovalMessage* message)
{
  return transformer(message);
}

bool HovalMessageTransformer::active()
{
  if (activeFunc == nullptr)
    return true;
  return activeFunc();
}

bool HovalMessageTransformer::sendOnChange()
{
  if (sendOnChangeFunc == nullptr)
    return false;
  return sendOnChangeFunc();
}

uint32_t HovalMessageTransformer::sendIntervalMs(HovalMessage* message)
{
  if (sendIntervalMsFunc == nullptr)
    return 0;
  return sendIntervalMsFunc(message);
}
