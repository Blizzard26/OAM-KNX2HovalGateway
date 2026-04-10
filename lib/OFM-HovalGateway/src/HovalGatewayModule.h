#ifndef _KNX2HOVAL_GATEWAY_MODULE
#define _KNX2HOVAL_GATEWAY_MODULE

#include "Arduino.h"
#include "OpenKNX.h"
#include "hardware.h"

#include <mcp2515_can.h>

#include "HovalProtocolDecoder.h"

#include "Hoval2KNXMapper.h"

#ifndef CAN_INT_PIN
#error "CAN_INT_PIN not defined"
#endif

#ifndef SPI_CS_PIN
#error "SPI_CS_PIN not defined"
#endif

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

  /* Begin Can / Hoval Interface */
  mcp2515_can CAN;
  HovalProtocolHandler hoval;
  Hoval2KNXMapper hoval2KNX;
  /* End */

  void readParams();
  inline bool checkCanError();

  inline bool checkActive();
  inline bool connect();

public:
  Knx2HovalGatewayModule() : CAN(SPI_CS_PIN), hoval(&CAN, CAN_INT_PIN), hoval2KNX(&hoval) { hoval.setHovalEventHandler(&hoval2KNX); }

  void setup(bool configured) override;
  void loop() override;

  void processAfterStartupDelay() override;

  void processInputKo(GroupObject& ko) override;

  const std::string name() override;
  const std::string version() override;

  bool processCommand(const std::string cmd, bool diagnoseKo) override;

  void showHelp() override;

  void showInformations() override;
};

#endif