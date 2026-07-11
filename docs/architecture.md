# Architecture

## Module registration (`src/main.cpp`)

The firmware follows the OpenKNX module pattern: `openknx.addModule(id, moduleInstance)` registers
each `OpenKNX::Module` subclass, then `openknx.setup()` / `openknx.loop()` drive all of them.
This project registers `Knx2HovalGatewayModule` (id 1, in `lib/OFM-HovalGateway`) alongside the
stock `openknxLogic` (id 3) and `openknxFileTransferModule` (id 9) modules. Board/pin definitions
live in `include/hardware.h`, gated on `ARDUINO_SEEED_XIAO_RP2040`.

## Gateway module (`Knx2HovalGatewayModule`, `HovalGatewayModule.h/.cpp`)

Top-level `OpenKNX::Module`. Owns the CAN transceiver (`mcp2515_can`), a `HovalProtocolHandler`,
and a `Hoval2KNXMapper`, and wires them together. `loop()` evaluates at most one of
{CAN-error check, `hoval.task()`, `hoval2KNX.task()`} per call (short-circuited with `||`) to keep
each iteration fast, since the OpenKNX main loop expects it to return quickly. Also implements the
`hov ...` serial console commands (see `showHelp()`/`processCommand()`):
`hov log msg|msgData|filt|filtData` toggle logging of received/filtered Hoval messages,
`hov can read <register>` reads a raw MCP2515 register.

## Hoval protocol layer (`lib/OFM-HovalGateway/src`)

Three-stage pipeline from raw CAN frames to typed values and back:

1. **`HovalProtocolHandler` (`HovalProtocolDecoder.h/.cpp`)** — talks to the MCP2515 over SPI,
   reassembles single- and multi-part Hoval CAN messages into a `HovalMessage`, validates CRC
   (`HovalCrc.h`), and dispatches known types to an `IHovalEventHandler`. Message types are
   matched against a static filter table via a fixed-capacity `HashMap` (`HashMap.h`); raw CAN
   frames flow through a fixed-capacity `SimpleRingBuffer` (`SimpleRingBuffer.h`). Outgoing
   messages queue in a small `SimpleRingBuffer<std::unique_ptr<HovalMessage>, SEND_BUFFER_SIZE>`.
   These custom containers exist because the target MCU can't afford STL container overhead/heap
   churn.
2. **`HovalMessage` (`HovalMessage.h/.cpp`)** — a decoded protocol message with typed accessors
   (`u8Value()`, `s16Value()`, `floatValue()`, `errorMessage()`, ...) driven by the matched
   `HovalMessageType`'s `HovalDataType`/`decimals`.
3. **`Hoval2KNXMapper` (`Hoval2KNXMapper.h`, implemented in `HovalGatewayModule`'s
   `HovalMessageTransformer` table)** — implements `IHovalEventHandler` and translates between
   `HovalMessage`s and KNX `GroupObject`s. Each KNX comm-object is described by a
   `HovalMessageTransformer` entry: a `HovalMessageType*`, a KNX DPT, a `transform`/
   `inverseTransformer` function pair, and policy callbacks (`sendOnChange`, `sendIntervalMs`,
   `active`) that decide when a value is pushed to the KNX bus. The full transformer table (the
   actual Hoval<->KNX datapoint mapping) is defined in `HovalMessageTransformer.h`.

`HovalMessages.h` is the catalogue of known Hoval datapoints — each `const HovalMessageType`
identifies a value by `(unitType, functionGroup, functionNumber, dataPointId)` plus its raw wire
type, decimal scaling, and KNX-facing `DataType`. When adding support for a new datapoint, define
it here first, then add a corresponding `HovalMessageTransformer` entry.

`docs/Hoval.adoc` documents the underlying Hoval CAN protocol (framing, addressing, function
codes) independent of this codebase's implementation — consult it before changing message parsing
or CRC logic.
