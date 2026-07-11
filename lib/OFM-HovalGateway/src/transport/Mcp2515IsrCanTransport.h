#pragma once

#include "CanTransport.h"
#include <SPI.h>
#include <SimpleRingBuffer.h>
#include <mcp2515_can.h>
#include <stdint.h>
#include <string>

#define MCP2515_ISR_RX_BUFFER_SIZE 10

// RP2040 backend: MCP2515 over SPI, driven by a real attachInterrupt() ISR.
class Mcp2515IsrCanTransport : public CanTransport
{
public:
  Mcp2515IsrCanTransport(uint8_t csPin, uint8_t interruptPin, uint8_t clockSet, SPIClass& spi);
  ~Mcp2515IsrCanTransport() override;

  bool begin() override;
  bool start() override;
  void stop() override;
  bool receive(CanMessage& message) override;
  bool send(uint32_t address, const uint8_t* body, uint8_t bodyLength) override;
  CanErrorInfo checkErrors() override;

private:
  static void isrTrampoline();
  void onInterrupt();

  std::string logPrefix() { return "Mcp2515IsrCanTransport"; }

  mcp2515_can canBus;
  uint8_t interruptPin;
  uint8_t clockSet;
  SPIClass& spi;

  bool started = false;

  /* Can Message Receive Buffer, filled from the ISR, drained by receive() */
  SimpleRingBuffer<CanMessage, MCP2515_ISR_RX_BUFFER_SIZE> rxBuffer;
  // Only ever written inside the ISR's noInterrupts()/interrupts() section
  // (see onInterrupt()); checkErrors() reads it from main task context.
  volatile uint32_t droppedMessageCount = 0;

  // Single static instance pointer: exactly one HovalProtocolHandler/transport
  // per device, same pattern already used for the RP2040 timer callback
  // (openknx.timerInterrupt.interrupt()).
  static Mcp2515IsrCanTransport* s_instance;
};
