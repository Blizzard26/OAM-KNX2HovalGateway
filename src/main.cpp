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

void setup()
{
  // Start debug serial with 115200 bauds
  Serial.begin(115200);

  openknx.init();

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