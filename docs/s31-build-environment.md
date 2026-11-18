# S31 Build Environment

This file records the exact ESP32-S31 environment used to build and flash this project. It intentionally does not vendor ESP-IDF source code or compiler binaries.

## ESP-IDF

- Version: `ESP-IDF v6.2.0`
- Repository: `https://gitee.com/EspressifSystems/esp-idf.git`
- Branch: `master`
- Upstream: `origin/master`
- Commit: `827cc6229fb01c6ecf6362b4840ef27d62d2138e`
- Local patch: none; the IDF worktree was clean when this record was generated
- Original path: `/home/yjh/esp/esp-idf-master`
- Python: `3.12.7`
- CMake: `3.30.3`
- Ninja: `1.12.1`

The S31 Bluetooth controller submodule was:

```text
ef587f4a9956e96bdff3e93764059789133db680 components/bt/controller/lib_esp32s31/esp32s31-bt-lib
```

Clone and initialize the exact source revision:

```bash
git clone --recursive https://gitee.com/EspressifSystems/esp-idf.git esp-idf-s31
git -C esp-idf-s31 checkout 827cc6229fb01c6ecf6362b4840ef27d62d2138e
git -C esp-idf-s31 submodule update --init --recursive
```

The selected commit already contains ESP32-S31 support. No out-of-tree IDF patch is required.

## S31 Support Files

Critical files that must exist after checkout include:

```text
tools/cmake/toolchain-esp32s31.cmake
components/bt/controller/lib_esp32s31/esp32s31-bt-lib
components/bt/include/esp32s31
components/bt/porting_btdm/controller/esp32s31
components/esp_hw_support/port/esp32s31
components/esp_rom/esp32s31
components/hal/esp32s31
components/soc/esp32s31
```

`tools/cmake/toolchain-esp32s31.cmake` contains:

```cmake
set(_CMAKE_TOOLCHAIN_PREFIX riscv32-esp-elf-)
include($ENV{IDF_PATH}/tools/cmake/toolchain.cmake)
```

Reference SHA-256 values from the working environment:

```text
4663bf2f8ef1f650d3d1e641d029e0336753d9aea8e6ef5643048b8095d2fb49  tools/cmake/toolchain-esp32s31.cmake
e2fcecc482ec32b81e578b319e0fd633a1801adaccd6b3fb3350ea89c4eebb4f  tools/cmake/targets.cmake
```

## Compiler Toolchain

- Tool: `riscv32-esp-elf-gcc`
- Version: `riscv32-esp-elf-gcc (crosstool-NG esp-16.1.0_20260609) 16.1.0`
- IDF tools directory: `~/.espressif/tools/riscv32-esp-elf/esp-16.1.0_20260609/riscv32-esp-elf`
- Compiler SHA-256: `d50a9ce3339d60d7124800d0ee6192b7dbf04487c1cefe522b5058e867eeea25`

Install the tools declared by the checked-out IDF rather than copying the original absolute path:

```bash
cd esp-idf-s31
./install.sh esp32s3,esp32c3,esp32s31
. ./export.sh
riscv32-esp-elf-gcc --version
```

If `install.sh` cannot select `esp32s31` by name, run `./install.sh all`; the exact IDF commit's `tools/tools.json` is authoritative for tool versions.

## Project Dependencies

Exact managed-component versions and hashes are committed in `dependencies.lock`. Important versions are:

```text
ESP-IDF                                      6.2.0
viewesmart/bsp_ues31s043h800v480c_u          1.0.2
espressif/esp_lvgl_port                      2.9.0
lvgl/lvgl                                    9.6.0~1
espressif/usb                                1.6.1
espressif/usb_host_hid                       1.2.1
espressif/esp_codec_dev                      1.5.11
```

The component registry is `https://components.espressif.com`. When restoring in a network-restricted environment, configure the required mirror or proxy before the first build so the component manager can materialize `managed_components/` from `dependencies.lock`.

## Build Configuration

The repository tracks the relevant project configuration:

```text
CMakeLists.txt
dependencies.lock
partitions.csv
sdkconfig.defaults
sdkconfig.defaults.s31
main/CMakeLists.txt
main/idf_component.yml
```

`sdkconfig` is machine-generated and intentionally ignored. Regenerate it from the tracked defaults on the destination machine.

Known successful S31 build command:

```bash
. /path/to/esp-idf-s31/export.sh
idf.py --preview -B build-s31 -DIDF_TARGET=esp32s31 build
```

The build used 16 MiB QIO flash, 16 MiB Octal PSRAM at 200 MHz, a 3 MiB factory partition, and the custom partition table in `partitions.csv`.

## Verification

After restoring, verify the environment before building:

```bash
test -f "$IDF_PATH/tools/cmake/toolchain-esp32s31.cmake"
test "$(git -C "$IDF_PATH" rev-parse HEAD)" = "827cc6229fb01c6ecf6362b4840ef27d62d2138e"
git -C "$IDF_PATH" submodule status components/bt/controller/lib_esp32s31/esp32s31-bt-lib
riscv32-esp-elf-gcc --version
idf.py --version
```

For cross-target smoke testing, use separate build directories and target-specific example projects or defaults. This game project itself selects ESP32-S31 hardware and is not expected to build unchanged for ESP32-S3 or ESP32-C3.
