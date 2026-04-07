#include "hardware.h"
#include <Arduino.h>

#include "FileTransferModule.h"
#include "Logic.h"
#include <knx.h>

#include "knxprod.h" // Needs to be included after knx.h

#include "HovalGatewayModule.h"

////////////////////////
// Global Variables
////////////////////////

Knx2HovalGatewayModule hovalGatewayModule;

const uint8_t firmwareRevision = 1;

void setup()
{
  // Start debug serial with 115200 bauds
  Serial.begin(115200);
#if defined(OPENKNX_DEBUG)
  // Wait up to 5 seconds for serial to be ready
  uint8_t i = 0;
  while (!Serial && i++ < 5)
  {
    delay(1000);
  }

  logInfo("KNX2HovalGateway", "r%u %s", firmwareRevision, __TIMESTAMP__);
  Serial.flush();
#endif

  openknx.init(firmwareRevision);

  openknx.addModule(1, hovalGatewayModule);
  openknx.addModule(3, openknxLogic);
  openknx.addModule(9, openknxFileTransferModule);

  openknx.setup();
}

void loop()
{
  openknx.loop();
}

#if defined(OPENKNX_DUALCORE)

void setup1()
{
  openknx.setup1();
}

void loop1()
{
  openknx.loop1();
}

#endif