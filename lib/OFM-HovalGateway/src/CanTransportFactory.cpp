#include "CanTransportFactory.h"
#include "hardware.h"

#if defined(ARDUINO_ARCH_RP2040)
#include "transport/Mcp2515IsrCanTransport.h"

#ifndef SPI_CS_PIN
#error "SPI_CS_PIN not defined"
#endif
#ifndef CAN_INT_PIN
#error "CAN_INT_PIN not defined"
#endif
#ifndef CAN_CLOCK
#error "CAN_CLOCK not defined"
#endif
#ifndef CAN_SPI
#error "CAN_SPI not defined"
#endif
#endif

std::unique_ptr<CanTransport> createCanTransport()
{
#if defined(ARDUINO_ARCH_RP2040)
  return std::make_unique<Mcp2515IsrCanTransport>(SPI_CS_PIN, CAN_INT_PIN, CAN_CLOCK, CAN_SPI);
#else
#error Unsupported board
#endif
}
