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

#ifndef delayCheck
// clang-format off
#define delayCheck(last, duration) (millis() - last >= duration)
// clang-format on
#endif

#ifndef CAN_ERROR_LOOP_CNT
#define CAN_ERROR_LOOP_CNT 128
#endif

#ifndef CAN_CONNECT_RETRY_DELAY
// 30 seconds
#define CAN_CONNECT_RETRY_DELAY 30 * 1000
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
  this->vocSensorPresent = ParamHOV_homevent_voc_sensor_present;

  this->gatewayUnitId = ParamHOV_gateway_device_type << 4 | ParamHOV_gateway_device_id;

  this->deviceUnitId = ParamHOV_homevent_device_type << 4 | ParamHOV_homevent_device_id;

  this->requestInterval = ParamHOV_homevent_pollingInterval_DelayTimeMS;
}

inline static constexpr const uint32_t calculateTimeDifference(const uint32_t lastTime, const uint32_t newTime)
{
  return newTime - lastTime;
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
    logErrorP("CAN error: %#02X", err_ptr);
    uint8_t rxStatus = (CAN.readRxTxStatus() & MCP_STAT_RXIF_MASK);
    logDebugP("RX-Status: %#02X", rxStatus);
#if defined(BASE_KoDiagnose)
    openknx.console.writeDiagnoseKo("CAN-E %#02X R %#01X", err_ptr, rxStatus);
#endif

    canErrorCount++;
    if (canErrorCount > 20)
    {
      logErrorP("Too many CAN errors. Resetting CAN");
      ready = false; // This will trigger a reconnect on the next iteration.
      lastReconnectTry = 0;
      canErrorCount = 0;
    }
  }
  else
  {
    canErrorCount = 0;
  }
  return true;
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
  else if (active && !ready)
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
  if (!changed && delayCheck(lastHeartbeat, ParamHOV_homevent_sendActive_DelayTimeMS))
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
{}

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
  if (cmd.substr(0, 6) != "hoval " || cmd.length() < 7)
    return false;

  if (!ready)
  {
    logInfoP("Hoval not connected. Command cannot be processed.");
    return true;
  }

  std::string command = cmd.substr(6);

  if (command.substr(0, 4) == "log ")
  {
    command = command.substr(4);

    if (command == "message")
    {
      hoval.setLogMessage(!hoval.isLogMessage());
      logInfoP("Logging message headers: %s", hoval.isLogMessage() ? "enabled" : "disabled");
      return true;
    }
    if (command == "messageData")
    {
      if (!hoval.isLogMessageData())
      {
        hoval.setLogMessage(true);
      }

      hoval.setLogMessageData(!hoval.isLogMessageData());
      logInfoP("Logging message data: %s", hoval.isLogMessageData() ? "enabled" : "disabled");
      return true;
    }
    if (command == "filtered")
    {
      hoval.setLogFilteredMessage(!hoval.isLogFilteredMessage());
      logInfoP("Logging filtered message headers: %s", hoval.isLogFilteredMessage() ? "enabled" : "disabled");
      return true;
    }
    if (command == "filteredData")
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

  return false;
}

void Knx2HovalGatewayModule::showHelp()
{
  openknx.console.printHelpLine("hoval log message", "Log header of received (known) messages");
  openknx.console.printHelpLine("hoval log messageData", "Log payload of received (known) messages");
  openknx.console.printHelpLine("hoval log filtered", "Log header of filtered messages");
  openknx.console.printHelpLine("hoval log filteredData", "Log payload of filtered messages");
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
