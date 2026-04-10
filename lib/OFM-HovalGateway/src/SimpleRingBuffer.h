#pragma once

#include <stdint.h>

template <typename T, uint8_t BUFFER_SIZE> class SimpleRingBuffer
{
private:
  T buffer[BUFFER_SIZE];
  uint8_t insertPosition = 0;
  uint8_t readPosition = UINT8_MAX;

  inline void incReadPos()
  {
    readPosition = (readPosition + 1) % BUFFER_SIZE;
    if (readPosition == insertPosition)
    {
      readPosition = UINT8_MAX;
    }
  }

  inline void incInsertPos()
  {
    if (isEmpty())
    {
      readPosition = insertPosition;
    }
    insertPosition = (insertPosition + 1) % BUFFER_SIZE;
  }

public:
  SimpleRingBuffer() { static_assert(BUFFER_SIZE > 0 && BUFFER_SIZE != UINT8_MAX, "Invalid buffer size"); }

  inline bool offer(const T& value)
  {
    T* insertPosition = beginPush();

    if (insertPosition == nullptr)
    {
      return false;
    }

    *insertPosition = value;
    endPush();
    return true;
  }

  void push(const T& value)
  {
    if (isFull())
    {
      // LOG_ERROR(F("Buffer Overflow"));
      incReadPos();
    }

    T* insertPosition = beginPush();

    *insertPosition = value;
    endPush();
  }

  T* beginPush()
  {
    if (isFull())
    {
      // LOG_ERROR(F("Buffer Full"));
      return nullptr;
    }
    return &(buffer[insertPosition]);
  }

  void endPush() { incInsertPos(); }

  T* pop()
  {
    if (isEmpty())
    {
      return nullptr;
    }

    T* value = &(buffer[readPosition]);

    incReadPos();
    return value;
  }

  T* peek()
  {
    if (isEmpty())
    {
      return nullptr;
    }

    return &(buffer[readPosition]);
  }

  inline bool isEmpty() { return readPosition == UINT8_MAX; }

  inline bool isFull() { return readPosition == insertPosition; }

  inline bool available() { return readPosition != insertPosition; }
};
