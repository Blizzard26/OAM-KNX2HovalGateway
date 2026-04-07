# Hoval2KNXGateway

A KNX gateway for Hoval TopTronic systems, enabling integration between KNX home automation networks and Hoval CAN-based controllers.

It is a PlatformIO project and needs a working ETS 5.7 (or higher) installed on the same PC (at least for generating the required ETS files).

It uses some Modules from the OpenKNX project: OGM-Common, OFM-cnofigTransfer, OFM-FileTransferModule, OFM-LogicModule

## Description

This project implements a bridge between KNX protocol and Hoval's proprietary CAN-based communication protocol. It allows KNX devices to read and write parameters from Hoval TopTronic systems.

The gateway uses the Hoval Message Protocol over CAN bus at 50kbps, supporting both single and multi-part messages for comprehensive datapoint access.


## Supported Hardware

Currently supports

- Seeeduino XIAO RP2040 with a MCP2515 (and a support CAN Transceive, e.g. MCP2562). Make sure to electrically separate the KNX bus from the Hoval bus as the operate on different potentials.

## Software Requirements

- **PlatformIO**: For building and flashing the firmware
- **Git**: For cloning this repository
- **Arduino Framework**: Used via PlatformIO
- **Dependencies**:
  - KNX library (version 2.2.2+)
  - Seeed Arduino CAN library
  - Various OpenKNX modules

## Installation

1. Clone this repository and initialize submodules:
   ```bash
   git clone <repository-url>
   cd Hoval2KNXGateway
   git submodule update --init --recursive
   ```
   NOTE: In contrast to other OpenKNX Projects this project uses git submodule to pull in libraries instead of restore scripts and `dependencies.txt`.

2. Install PlatformIO if not already installed.

3. Install dependencies:
   ```bash
   pio lib install
   ```

## Building

Use the provided build scripts or PlatformIO directly:

### Development Build
```bash
scripts/Build-Release.ps1
```
Or use the VS Code task: `Build-Dev`

### Release Build
```bash
scripts/Build-Release.ps1 Release
```
Or use the VS Code task: `Build-Release`

### Configuration

- Configure KNX parameters using ETS (Engineering Tool Software)

### Monitoring

Connect to the serial port at 115200 baud for debug output and monitoring.

There are four new commands added:

- `hoval log message`: Log received and processed messages - header only.
- `hoval log messageData`: Log received and processed message - including (parsed) data.
- `hoval log filtered`: Log filtered message, i.e., messages not known to the decoder - header only.
- `hoval log filteredData`: Log filtered message - including raw data.

## Project Structure

- `src/`: Main source code
- `include/`: Header files and configuration
- `lib/`: Library dependencies and modules
- `docs/`: Protocol documentation (Hoval.adoc)
- `schematics/`: Hardware schematics in KiCad format
- `scripts/`: Build and utility scripts
- `test/`: Test files

## Documentation

Detailed protocol documentation is available in `docs/Hoval.adoc`, including:
- Message structure and addressing
- Function codes and datapoints
- Communication flow examples


## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Test thoroughly
5. Submit a pull request

## License

GNU General Public License v3.0

## Support

For issues and questions:
- Check the debug logs via serial output
- Refer to the protocol documentation
- Ensure proper hardware connections

## Disclaimer

This software is provided as-is. Ensure compatibility with your specific Hoval system and KNX installation. Always backup configurations before making changes.