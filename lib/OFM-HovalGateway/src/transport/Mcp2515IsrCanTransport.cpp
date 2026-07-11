#include "transport/Mcp2515IsrCanTransport.h"
#include "OpenKNX.h"
#include <mcp2515_can_dfs.h>
#include <string.h>

#define FILTER_CAN_MESSAGES

static const uint8_t CAN_INIT_MAX_RETRIES = 5;

Mcp2515IsrCanTransport* Mcp2515IsrCanTransport::s_instance = nullptr;

Mcp2515IsrCanTransport::Mcp2515IsrCanTransport(uint8_t csPin, uint8_t interruptPin, uint8_t clockSet, SPIClass& spi)
    : canBus(csPin), interruptPin(interruptPin), clockSet(clockSet), spi(spi)
{
}

Mcp2515IsrCanTransport::~Mcp2515IsrCanTransport()
{
  stop();
}

bool Mcp2515IsrCanTransport::begin()
{
  spi.begin();
  canBus.setSPI(&spi);

  for (uint8_t i = 0; i < CAN_INIT_MAX_RETRIES; i++)
  {
    if (CAN_OK == canBus.begin(CAN_50KBPS, clockSet))
      break;
    logErrorP("CAN init fail (%u/%u)", i + 1, CAN_INIT_MAX_RETRIES);
    if (i + 1 == CAN_INIT_MAX_RETRIES)
    {
      logErrorP("CAN init failed after %u retries", CAN_INIT_MAX_RETRIES);
      return false;
    }
    delay(100);
  }

  bool success = canBus.setMode(MODE_NORMAL) == MCP2515_OK;
  if (!success)
  {
    logErrorP("setMode failed");
  }
  // Allow EXT-Messages to be received
#if defined(FILTER_CAN_MESSAGES)
  success &= canBus.init_Mask(0, true, 0x000007FF) == MCP2515_OK; // Filter enabled
  success &= canBus.init_Mask(1, true, 0x000007FF) == MCP2515_OK; // Filter enabled
#else
  success &= canBus.init_Mask(0, true, 0x00000000) == MCP2515_OK; // Filter disabled
#endif
  if (!success)
  {
    logErrorP("init_Mask failed");
  }
  success &= canBus.init_Filt(0, true, 0x000007FF) == MCP2515_OK;
  success &= canBus.init_Filt(2, true, 0x000007FF) == MCP2515_OK;
  if (!success)
  {
    logErrorP("init_Filt failed");
  }
  canBus.enableTxInterrupt(false);
  return success;
}

bool Mcp2515IsrCanTransport::start()
{
  if (started)
    return true;

  s_instance = this;

  pinMode(interruptPin, INPUT_PULLUP);
  spi.usingInterrupt(digitalPinToInterrupt(interruptPin));
  attachInterrupt(digitalPinToInterrupt(interruptPin), isrTrampoline, LOW);

  started = true;
  return true;
}

void Mcp2515IsrCanTransport::stop()
{
  if (!started)
    return;

  detachInterrupt(digitalPinToInterrupt(interruptPin));

  if (s_instance == this)
    s_instance = nullptr;

  started = false;
}

void __time_critical_func(Mcp2515IsrCanTransport::isrTrampoline)()
{
  s_instance->onInterrupt();
}

void __time_critical_func(Mcp2515IsrCanTransport::onInterrupt)()
{
  while (digitalRead(interruptPin) == LOW)
  {
    uint32_t address;
    uint8_t body[8];
    uint8_t len;
    if (canBus.readMsgBufID(&address, &len, body) != CAN_OK)
      break;

    // Defensive: the DLC field is 4 bits (0-15) but body[]/CanMessage::body
    // are both fixed at 8 bytes; clamp so a noisy/corrupted DLC read can't
    // overflow either buffer.
    if (len > sizeof(body))
      len = sizeof(body);

    noInterrupts();
    CanMessage* slot = rxBuffer.beginPush();
    if (slot != nullptr)
    {
      slot->set(address, body, len);
      rxBuffer.endPush();
    }
    else
    {
      droppedMessageCount++; // SW buffer full, message dropped — surfaced via checkErrors()
    }
    interrupts();
  }
}

bool Mcp2515IsrCanTransport::receive(CanMessage& message)
{
  noInterrupts();
  CanMessage* popped = rxBuffer.pop();
  if (popped != nullptr)
    message = *popped;
  interrupts();

  return popped != nullptr;
}

bool Mcp2515IsrCanTransport::send(uint32_t address, const uint8_t* body, uint8_t bodyLength)
{
  // Don't wait for sent. Timeout in mcp2515_can.cpp is too short and will result in CAN_SENDMSGTIMEOUT
  uint8_t result = canBus.trySendMsgBuf(address, true, false, bodyLength, body, MCP_N_TXBUFFERS);
  if (result == CAN_FAILTX)
  {
    // No available buffer for send. Try again later.
    return false;
  }
  if (result != CAN_OK)
    logErrorP("Send Failed: %u", result);

  return result == CAN_OK;
}

CanErrorInfo Mcp2515IsrCanTransport::checkErrors()
{
  CanErrorInfo info;

  uint8_t errPtr = 0;
  if (canBus.checkError(&errPtr) != CAN_OK)
  {
    uint8_t rxStatus = canBus.readRxTxStatus() & MCP_STAT_RXIF_MASK;
    info.hasError = true;
    info.rawErrorFlags = ((uint32_t)errPtr << 8) | rxStatus;
  }

  if (droppedMessageCount > 0)
  {
    info.overflowDetected = true;
    droppedMessageCount = 0;
  }

  return info;
}
