#pragma once
#include <stdint.h>
#include <string.h>

struct CanMessage
{
  uint32_t address;
  uint8_t body[8];
  uint8_t bodyLength = 0;

  void set(uint32_t newAddress, const uint8_t* newBody, uint8_t newBodyLength)
  {
    address = newAddress;
    bodyLength = newBodyLength;
    memcpy(body, newBody, newBodyLength);
  }
};

struct CanErrorInfo
{
  bool hasError = false;         // any error worth logging/counting
  bool overflowDetected = false; // RX buffer/queue overflow, HW or SW-side (e.g. rxBuffer full)
  uint32_t rawErrorFlags = 0;    // backend-specific bits, logged as hex only
};

// Threading contract: every method is called from the main Arduino task only.
// Implementations are free to use ISRs, FreeRTOS tasks, or driver-internal
// queues to fill/drain data, but must guarantee receive()/send()/checkErrors()
// are safe to call from plain task context without the caller doing any locking.
class CanTransport
{
public:
  virtual ~CanTransport() = default;

  // One-time configuration (bit timing, filters, mode). Main task only.
  virtual bool begin() = 0;

  // Bring the bus online: installs the ISR / starts the driver. Idempotent
  // enough to be called again after stop() for reconnect. Main task only.
  virtual bool start() = 0;

  // Detach ISR / stop the driver, e.g. before a reconnect attempt.
  virtual void stop() = 0;

  // Non-blocking. Returns true and fills `message` if a frame was queued.
  // Call in a loop to drain everything currently buffered. Main task only —
  // any ISR-vs-main synchronization needed is entirely the backend's problem.
  virtual bool receive(CanMessage& message) = 0;

  // Best-effort non-blocking send. False covers both "no free TX slot" and
  // any other backend-specific send failure (which the implementation logs
  // itself); the caller cannot tell the two apart and should just retry
  // later (mirrors current trySendCANMessage/CAN_FAILTX contract).
  virtual bool send(uint32_t address, const uint8_t* body, uint8_t bodyLength) = 0;

  // Polls backend error/overflow state, including any software-side buffer
  // drops (see CanErrorInfo). Called periodically from the main loop (never
  // from an ISR).
  virtual CanErrorInfo checkErrors() = 0;
};
