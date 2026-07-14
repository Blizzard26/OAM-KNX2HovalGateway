#include "HovalGatewayModule.h"
#include "OpenKNX/Log/Logger.h"
#include "knx.h"
#include "knxprod.h"

#include "HovalProtocolDecoder.h"
// Needs to be included after HovalProtocolDecoder.h
#include "HovalMessages.h"

#include "Hoval2KNXMapper.h"
// Needs to be included after Hoval2KNXMapper.h
#include "HovalMessageTransformer.h"

#ifndef CAN_ERROR_LOOP_CNT
#define CAN_ERROR_LOOP_CNT 8
#endif

#ifndef CAN_CONNECT_RETRY_DELAY
// 30 seconds
#define CAN_CONNECT_RETRY_DELAY (30 * 1000)
#endif

// Maximum number of KO slots reserved for this module starting at HOV_Koglobal_online
#ifndef HOV_MAX_KO_NUM
#define HOV_MAX_KO_NUM 100
#endif

#ifndef HOVAL_ACTIVE_PIN_ACTIVE_ON
#define HOVAL_ACTIVE_PIN_ACTIVE_ON HIGH
#endif

void Knx2HovalGatewayModule::setup(bool configured)
{

  if (configured)
  {
    logDebugP("Init Params");
    readParams();
  }

  showInformations();

  // Init Can
  CAN_SPI.begin();
  CAN.setSPI(&CAN_SPI);

  // Init Hoval
  hoval2KNX.setRequestInterval(this->requestInterval);
  hoval2KNX.setGatewayId(this->gatewayUnitId);
  hoval2KNX.setDeviceId(this->deviceUnitId);

#if defined(HOVAL_ACTIVE_PIN)
  pinMode(HOVAL_ACTIVE_PIN, INPUT);
#endif
}

void Knx2HovalGatewayModule::processAfterStartupDelay()
{
  hoval.begin();
}

void Knx2HovalGatewayModule::readParams()
{
  this->vocSensorPresent = ParamHOV_HVVocSensorPresent;

  this->gatewayUnitId = ParamHOV_GatewayDeviceType << 4 | ParamHOV_GatewayDeviceId;

  this->deviceUnitId = ParamHOV_HVDeviceType << 4 | ParamHOV_HVDeviceId;

  this->requestInterval = ParamHOV_HVPollingIntervalDelayTimeMS;
}

// Needs to be called at least every 400us
void Knx2HovalGatewayModule::loop()
{
  // Check if Hoval is Powered / Active
  if (!checkActive())
  {
    return;
  }

  loopCount++;

  // Note only one task is evaluated every call to make sure method returns fast enough. This is achieved using short circuit behavior of || operator.
  bool busy = false;
  // Check can errors
  if (loopCount % CAN_ERROR_LOOP_CNT == 0)
  {
    busy = busy || checkCanError();
  }

  // Send and/or Receive messages from Can Bus
  busy = busy || hoval.task();

  // Check for any necessary update requests
  busy = busy || hoval2KNX.task();

  if (loopCount >= 60000)
  {
    loopCount = 0;
  }
}

inline bool Knx2HovalGatewayModule::checkCanError()
{
  uint8_t err_ptr;
  if (CAN.checkError(&err_ptr) != CAN_OK)
  {
    uint8_t rxStatus = (CAN.readRxTxStatus() & MCP_STAT_RXIF_MASK);
    logErrorP("CAN error: %#02X; RX-Status: %#02X", err_ptr, rxStatus);

    uint16_t errorCode = (err_ptr << 8) | rxStatus;
    if (errorCode != lastCanError)
    {
      lastCanError = errorCode;
      char errorStringBuf[15];

      snprintf(errorStringBuf, 15, "CAN-E %#02X R %#01X", err_ptr, rxStatus);
      KoHOV_general_diagnose.value(errorStringBuf, DPT_String_8859_1);
    }

    canErrorCount++;
    if (canErrorCount > 20)
    {
      logErrorP("Too many CAN errors. Resetting CAN");
      ready = false; // This will trigger a reconnect on the next iteration.
      lastReconnectTry = 0;
      canErrorCount = 0;
      lastCanError = 0;
    }
    return true;
  }
  else
  {
    canErrorCount = 0;
    lastCanError = 0;
  }
  return false;
}

inline bool Knx2HovalGatewayModule::checkActive()
{
  bool active;
  // Check if Hoval is connected and powered on if pin is defined. Otherwise we assume yes.
#if defined(HOVAL_ACTIVE_PIN)
  active = digitalRead(HOVAL_ACTIVE_PIN) == HOVAL_ACTIVE_PIN_ACTIVE_ON;
#else
  active = true;
#endif

  bool changed = false;

  if (!active)
  {
    // Reset ready flag if not active
    ready = false;

    // If not active log every N seconds
    if (lastReconnectTry == 0 || delayCheck(lastReconnectTry, CAN_CONNECT_RETRY_DELAY))
    {
      logErrorP("Hoval not connected!");
      lastReconnectTry = millis();
      // Mark as changed
      changed = true;
    }
  }
  // If not yet / no longer connected try to reconnect
  else if (!ready)
  {
    // Retry connection every N seconds
    if (lastReconnectTry == 0 || delayCheck(lastReconnectTry, CAN_CONNECT_RETRY_DELAY))
    {
      lastReconnectTry = millis();
      ready = connect();
      // If ready then something changed
      changed |= ready;
    }
  }
  // active && ready
  else
  {
    // If active && ready this has already been the previous state
    changed = false;
    lastReconnectTry = 0;
  }

#ifdef HOV_Koglobal_online
  if (changed)
  {
    changed = KoHOV_global_online.valueCompare(active && ready, DPT_Switch);
  }
  if (!changed && delayCheck(lastHeartbeat, ParamHOV_HVSendActiveDelayTimeMS))
  {
    lastHeartbeat = millis();
    KoHOV_global_online.objectWritten();
  }
#endif

  return active && ready;
}

inline bool Knx2HovalGatewayModule::connect()
{
  logDebugP("Reconnecting Hoval CAN Bus");

  if (hoval.connect())
  {
    logDebugP("Hoval connection ok!");
    return true;
  }
  else
  {
    logErrorP("Hoval connection failed!");
    return false;
  }
}

void Knx2HovalGatewayModule::processInputKo(GroupObject& ko)
{
  if (ko.asap() >= HOV_Koglobal_online && ko.asap() < HOV_Koglobal_online + HOV_MAX_KO_NUM)
  {
    logTraceP("Received update for KO %u, %u", ko.asap(), ko.commFlag());
    hoval2KNX.sendToHovalBus(ko);
  }
}

// Version + 1 byte of initialized flags (1 bit per KO) + 2 KOs * DPT_Scaling data byte
#define HOV_FLASH_VERSION 1
#define HOV_FLASH_FLAG_PARTY_VALUE_INITIALIZED (1 << 0)
#define HOV_FLASH_FLAG_PAUSE_VALUE_INITIALIZED (1 << 1)

uint16_t Knx2HovalGatewayModule::flashSize()
{
  return 1 + 1 + 2;
}

void Knx2HovalGatewayModule::writeFlash()
{
  GroupObject& partyValueKo = KoHOV_vent_party_value;
  GroupObject& pauseValueKo = KoHOV_vent_pause_value;

  uint8_t flags = 0;
  if (partyValueKo.initialized())
    flags |= HOV_FLASH_FLAG_PARTY_VALUE_INITIALIZED;
  if (pauseValueKo.initialized())
    flags |= HOV_FLASH_FLAG_PAUSE_VALUE_INITIALIZED;

  openknx.flash.writeByte(HOV_FLASH_VERSION);
  openknx.flash.writeByte(flags);
  openknx.flash.writeByte(partyValueKo.initialized() ? (uint8_t)partyValueKo.value(DPT_Scaling) : 0);
  openknx.flash.writeByte(pauseValueKo.initialized() ? (uint8_t)pauseValueKo.value(DPT_Scaling) : 0);
}

void Knx2HovalGatewayModule::readFlash(const uint8_t* data, const uint16_t size)
{
  if (size == 0) // first call - without data
    return;

  uint8_t version = openknx.flash.readByte();
  if (version != HOV_FLASH_VERSION)
  {
    logDebugP("Wrong version of flash data (%i)", version);
    return;
  }

  uint8_t flags = openknx.flash.readByte();
  uint8_t partyValue = openknx.flash.readByte();
  uint8_t pauseValue = openknx.flash.readByte();

  if (flags & HOV_FLASH_FLAG_PARTY_VALUE_INITIALIZED)
  {
    KoHOV_vent_party_value.valueNoSend(partyValue, DPT_Scaling);
  }
  if (flags & HOV_FLASH_FLAG_PAUSE_VALUE_INITIALIZED)
  {
    KoHOV_vent_pause_value.valueNoSend(pauseValue, DPT_Scaling);
  }
}

const std::string Knx2HovalGatewayModule::name()
{
  return std::string("KNX2HovalGateway");
}

const std::string Knx2HovalGatewayModule::version()
{
  return std::string("0.1");
}

bool Knx2HovalGatewayModule::processCommand(const std::string cmd, bool diagnoseKo)
{
  if (cmd.substr(0, 4) != "hov " || cmd.length() < 5)
    return false;

  if (!ready)
  {
    logInfoP("Hoval not connected. Command cannot be processed.");
    return true;
  }

  std::string command = cmd.substr(4);

  if (command.substr(0, 4) == "log ")
  {
    command = command.substr(4);

    if (command == "msg")
    {
      hoval.setLogMessage(!hoval.isLogMessage());
      logInfoP("Logging message headers: %s", hoval.isLogMessage() ? "enabled" : "disabled");
      return true;
    }
    if (command == "msgData")
    {
      if (!hoval.isLogMessageData())
      {
        hoval.setLogMessage(true);
      }

      hoval.setLogMessageData(!hoval.isLogMessageData());
      logInfoP("Logging message data: %s", hoval.isLogMessageData() ? "enabled" : "disabled");
      return true;
    }
    if (command == "filt")
    {
      hoval.setLogFilteredMessage(!hoval.isLogFilteredMessage());
      logInfoP("Logging filtered message headers: %s", hoval.isLogFilteredMessage() ? "enabled" : "disabled");
      return true;
    }
    if (command == "filtData")
    {
      if (!hoval.isLogFilteredMessageData())
      {
        hoval.setLogFilteredMessage(true);
      }

      hoval.setLogFilteredMessageData(!hoval.isLogFilteredMessageData());
      logInfoP("Logging filtered message data: %s", hoval.isLogFilteredMessageData() ? "enabled" : "disabled");
      return true;
    }
  }

  if (command.substr(0, 4) == "can ")
  {
    command = command.substr(4);

    if (command.substr(0, 5) == "read ")
    {
      std::string commandValue = command.substr(5);
      uint8_t reg = strtol(commandValue.c_str(), nullptr, 0);
      logInfoP("Read CAN register %#02X: %#02X", reg, CAN.readRegister(reg));
      return true;
    }
  }

  return false;
}

void Knx2HovalGatewayModule::showHelp()
{
  openknx.console.printHelpLine("hov log msg", "Log header of received (known) messages");
  openknx.console.printHelpLine("hov log msgData", "Log payload of received (known) messages");
  openknx.console.printHelpLine("hov log filt", "Log header of filtered messages");
  openknx.console.printHelpLine("hov log filtaiData", "Log payload of filtered messages");
  openknx.console.printHelpLine("hov can read <register>", "Read a CAN register");
}

void Knx2HovalGatewayModule::showInformations()
{
  logInfoP("Settings");
  logInfoP("- deviceId: %u", this->deviceUnitId);
  logInfoP("- gatewayUnitId: %u", this->gatewayUnitId);
  logInfoP("- requestInterval: %u", this->requestInterval);
  logInfoP("- vocSensor: %u", this->vocSensorPresent);

  logTraceP("HovFilt: %u", hoval.getNumberOfMessageFilters());
}
