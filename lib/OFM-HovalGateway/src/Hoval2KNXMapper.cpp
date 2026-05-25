#include "Hoval2KNXMapper.h"

#include "OpenKNX.h"
#include "OpenKNX/Log/Logger.h"
#include <knx.h>

#include "HovalProtocolDecoder.h"

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
  uint32_t now = millis();
  for (int i = 0; i < numberOfMessageTransformers; i++)
  {
    HovalMessageTransformer* processing = &(messageTransformers[i]);
    if (processing->type == message->messageType)
    {
      found = true;
      if (!processing->ignoreIncoming)
      {
        internalSendToKnx(message, processing);
      }
      processing->lastSeen = now;
    }
  }
  if (!found)
  {
    logDebugP("Unmapped message type: fGrp: %u, fNo: %u, dPId: %u", message->messageType->functionGroup, message->messageType->functionNumber,
              message->messageType->dataPointId);
  }
}

void Hoval2KNXMapper::sendToHovalBus(GroupObject& ko)
{
  internalSendToHovalBus(ko, false);
}

void Hoval2KNXMapper::internalSendToHovalBus(GroupObject& ko, bool isPrerequisite)
{
  bool found = false;
  for (int i = 0; i < numberOfMessageTransformers; i++)
  {
    HovalMessageTransformer* processing = &(messageTransformers[i]);
    if (processing->comObject == ko.asap())
    {
      found = true;

      // KOs marked sendOnlyAsPrerequisite are never sent on their own; they are only sent
      // together with the comObject that lists them as a prerequisite.
      if (!isPrerequisite && processing->sendOnlyAsPrerequisite)
      {
        continue;
      }

      KNXValue value = ko.value(processing->dpt);

      // Send prerequisite first if one is defined - but only when this KO's own value actually needs
      // sending. Without this guard, ANY call to sendToHovalBus for this KO (e.g. a periodic keep-alive
      // resend, or this device re-broadcasting a Hoval-reported status value it just echoed back onto
      // the KNX bus) would unconditionally re-send the prerequisite - which for party_mode/pause_mode
      // overwrites the physical register they share, making the Hoval output oscillate between the two.
      if (processing->prerequisiteComObject >= 0 && (isPrerequisite || processing->valueChanged(value)))
      {
        GroupObject& prerequisiteKo = knx.getGroupObject(processing->prerequisiteComObject);
        if (prerequisiteKo.initialized())
        {
          internalSendToHovalBus(prerequisiteKo, true);
        }
        else
        {
          logErrorP("Prerequisite KO %u for KO %u not initialized, skipping send", processing->prerequisiteComObject, processing->comObject);
          continue; // Skip sending this KO if its prerequisite is not initialized
        }
      }

      internalSendToHoval(value, processing, isPrerequisite);
    }
  }
  if (!found)
  {
    logDebugP("Sending not supported for KO %u", ko.asap());
  }
}

void Hoval2KNXMapper::internalSendToKnx(HovalMessage* message, HovalMessageTransformer* messageProcessing)
{
  GroupObject& groupObject = knx.getGroupObject(messageProcessing->comObject);

  KNXValue value = messageProcessing->transform(message);

  bool changed = groupObject.valueNoSendCompare(value, messageProcessing->dpt);
  if (changed)
  {
    messageProcessing->setLastValue(value); // Store the last KNX value from Hoval for future change detection
  }

  bool requestSend = changed && messageProcessing->sendOnChange();
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

void Hoval2KNXMapper::internalSendToHoval(KNXValue& value, HovalMessageTransformer* messageProcessing, bool sendAlways)
{

  if (messageProcessing->inverseTranformer == nullptr)
  {
    logDebugP("No message transformer for KO %u", messageProcessing->comObject);
    return;
  }

  // Check if the value to send is different from the last value received from Hoval
  if (!sendAlways && !messageProcessing->valueChanged(value))
  {
    logDebugP("Skipping send for KO %u: value unchanged", messageProcessing->comObject);
    return;
  }

  logInfoP("Sending update to Hoval: KO %u", messageProcessing->comObject);

  HovalValue hovalValue = messageProcessing->inverseTranformer(value);

  protocolHandler->write(gatewayId, deviceId, messageProcessing->type, hovalValue);

  // Entries ignoring incoming Hoval messages never learn their own echo via sendToKNXBus,
  // so track the sent value here to keep the change-detection dedup above working.
  if (messageProcessing->ignoreIncoming)
  {
    messageProcessing->setLastValue(value);
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

void HovalMessageTransformer::setLastValue(const KNXValue& value)
{
  const size_t dataLength = std::min(dpt.dataLength(), sizeInMemory);
  memset(lastValue, 0, sizeInMemory);
  const bool encoded = KNX_Encode_Value(value, lastValue, dataLength, dpt);
  lastValueInitialized = encoded;
}

bool HovalMessageTransformer::valueChanged(const KNXValue& value) const
{
  // If lastValue hasn't been initialized, consider the value as changed
  if (!lastValueInitialized)
  {
    return true;
  }

  // Compare values by converting to the same DPT format and comparing the binary representation
  // Same as in GroupObject::valueNoSendCompare
  const size_t dataLength = std::min(dpt.dataLength(), sizeInMemory);
  uint8_t currentData[dataLength];
  memset(currentData, 0, dataLength);

  const bool currentEncoded = KNX_Encode_Value(value, currentData, dataLength, dpt);

  if (!currentEncoded)
  {
    return true; // Encoding failed, consider them different
  }

  return memcmp(currentData, lastValue, dataLength) != 0;
}
