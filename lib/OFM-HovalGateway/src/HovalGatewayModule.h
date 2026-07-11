#pragma once

#include "Arduino.h"
#include "OpenKNX.h"
#include "hardware.h"

#include "CanTransportFactory.h"
#include "HovalProtocolDecoder.h"

#include "Hoval2KNXMapper.h"

#include <memory>

class Knx2HovalGatewayModule : public OpenKNX::Module
{
private:
  // Begin Settings
  uint16_t gatewayUnitId = 1024 + 9;
  uint16_t deviceUnitId = 512 + 8;

  bool vocSensorPresent = false;
  // Request interval in ms
  uint32_t requestInterval = 30000;
  // End Settings

  uint32_t lastHeartbeat = 0;
  uint32_t lastReconnectTry = 0;
  bool ready = false;

  uint16_t loopCount = 0;
  uint8_t canErrorCount = 0;
  uint32_t lastCanError = 0;

  /* Begin Can / Hoval Interface */
  std::unique_ptr<CanTransport> canTransport;
  HovalProtocolHandler hoval;
  Hoval2KNXMapper hoval2KNX;
  /* End */

  void readParams();
  inline bool checkCanError();

  inline bool checkActive();
  inline bool connect();

public:
  Knx2HovalGatewayModule() : canTransport(createCanTransport()), hoval(canTransport.get()), hoval2KNX(&hoval) { hoval.setHovalEventHandler(&hoval2KNX); }

  void setup(bool configured) override;
  void loop() override;

  void processAfterStartupDelay() override;

  void processInputKo(GroupObject& ko) override;

  uint16_t flashSize() override;
  void writeFlash() override;
  void readFlash(const uint8_t* data, const uint16_t size) override;

  const std::string name() override;
  const std::string version() override;

  bool processCommand(const std::string cmd, bool diagnoseKo) override;

  void showHelp() override;

  void showInformations() override;
};
