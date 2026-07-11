# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project overview

Firmware for a KNX<->Hoval gateway: bridges a KNX home automation bus to a Hoval TopTronic
CAN-based controller (currently a Hoval HomeVent ventilation unit). Runs on a Seeeduino XIAO
RP2040 with an MCP2515 CAN controller, built on the OpenKNX Arduino framework via PlatformIO.

## Build commands

Build via the PlatformIO environments defined in `platformio.custom.ini` (merged into
`platformio.ini` together with `lib/OGM-Common/platformio.base.ini` and
`platformio.rp2040.ini`):

- `scripts/Build-Release.ps1` — development build (`env:dev_KNX2HOVALGATEWAY`)
- `scripts/Build-Release.ps1 Release` — release build (`env:release_KNX2HOVALGATEWAY`)

Both wrap the reusable build scripts from the `OGM-Common` submodule
(`lib/OGM-Common/scripts/setup/reusable/`), which generate `include/versions.h` and the ETS
`knxprod` files before invoking PlatformIO. These are also exposed as VS Code tasks
`Build-Dev` and `Build-Release` (`.vscode/tasks.json`).

Both `include/versions.h` and `*.knxprod` are generated artifacts (gitignored) — do not hand-edit
them; they come from the ETS/OpenKNX build pipeline. Generating the ETS files additionally
requires ETS 5.7+ installed on the build machine.

## Native unit tests (`pio test -c test_platformio.ini`)

Test suites live under `test/<suite_name>/`, each an isolated PlatformIO test
suite for the `native_test` environment (`test_platformio.ini`). Files placed
directly in `test/` (not in a subdirectory) get linked into *every* suite's
build, so always put suite sources in their own `test/<suite_name>/` folder.

Run them individually with 

```
pio test -c test_platformio.ini -f test_hovalcrc
pio test -c test_platformio.ini -f test_log_seam
```

### Known PlatformIO Core bug: don't run multiple suites in one `pio test` invocation

Running more than one suite in a single invocation, e.g.:

```
pio test -c test_platformio.ini -e native_test
```

fails on the second (and any subsequent) suite with:

```
UndefinedEnvPlatformError: Please specify platform for 'native_test' environment
```

even though every suite passes fine when run individually. 

**Workaround: run each suite separately.**

```
pio test -c test_platformio.ini -f test_hovalcrc
pio test -c test_platformio.ini -f test_log_seam
```


Do **not** try merging `test_platformio.ini`'s `[env:native_test]` into the
main `platformio.ini` (e.g. via `extra_configs`) as a fix — that was already
tried on this project and did not resolve the issue.

## Dependency layout

Unlike most OpenKNX projects, this repo pulls in OpenKNX libraries as **git submodules** under
`lib/` (`knx`, `OGM-Common`, `OFM-LogicModule`, `OFM-ConfigTransfer`, `OFM-FileTransferModule`)
rather than via `restore` scripts and `dependencies.txt`. Run
`git submodule update --init --recursive` after cloning. `dependencies.txt` only records the
submodule commits the project was last built against — it is not used to fetch them.

The actual gateway logic lives in `lib/OFM-HovalGateway`, a project-local (non-submodule) OpenKNX
module.

## Architecture

See [docs/architecture.md](docs/architecture.md) for module registration, the gateway module's
responsibilities, and the Hoval protocol pipeline (`HovalProtocolHandler` -> `HovalMessage` ->
`Hoval2KNXMapper`).

## Code style

`.clang-format` is LLVM-based with 2-space indentation, `PointerAlignment: Left`,
`ColumnLimit: 160`, and brace-wrapping (Allman-ish) after classes/functions/control statements.
Run clang-format before committing C++ changes.
