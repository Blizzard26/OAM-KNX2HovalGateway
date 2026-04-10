// Hier stehen alle Hardwarespezifischen defines
#pragma once

#if defined(ARDUINO_SEEED_XIAO_RP2040)
// Arduino XIAO RP20240
// OpenKNX defines
#define KNX_DEBUG_SERIAL Serial
#define KNX_SERIAL Serial1 // D7=RX/D6=TX

#define PROG_LED_PIN PIN_LED_B
#define PROG_LED_PIN_ACTIVE_ON LOW
#define INFO1_LED_PIN PIN_LED_G
#define INFO1_LED_PIN_ACTIVE_ON LOW
#define INFO2_LED_PIN PIN_LED_R
#define INFO2_LED_PIN_ACTIVE_ON LOW

#define PROG_BUTTON_PIN D3
#define PROG_BUTTON_PIN_INTERRUPT_ON FALLING
//
// #define SAVE_INTERRUPT_PIN 5
#define KNX_UART_RX_PIN D7
#define KNX_UART_TX_PIN D6
#define KNX_UART_NUM 0

// Application Custom defines
#define DEBUGSERIAL Serial // USB port

#define SPI_CS_PIN D1

#define CAN_SPI SPI
#define CAN_INT_PIN D2
#define CAN_CLOCK MCP_16MHz
#define USE_CAN_ISR

#define HOVAL_ACTIVE_PIN D0
#define HOVAL_ACTIVE_PIN_ACTIVE_ON LOW

#else
#error Unsupported board
#endif