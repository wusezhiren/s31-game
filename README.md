# S31 Game Platform

S31 的统一游戏适配层，目标硬件为双核 320 MHz、16 MB Flash、16 MB PSRAM、800x480 RGB 屏。当前已接入 NES、GB/GBC、SMS、Game Gear、SG-1000 和 PCE HuCard 核心。

## 档位

| 档位 | 平台 | 目标 |
| --- | --- | --- |
| 轻松满速 | FC/NES、GB/GBC、SMS/GG、SG-1000、PCE HuCard、Atari 8-bit、NGP/NGPC、WonderSwan、ColecoVision、Vectrex、Game & Watch | 60 FPS 优先，ROM 复制到 PSRAM |
| 有难度 | GBA、SFC/SNES、Mega Drive、早期街机、CPS1、Atari Lynx、Virtual Boy | 核心优化、帧率监测、白名单；不承诺全库满速 |
| 不可行 | CPS2/CPS3、Neo Geo MVS、PS1、N64、Saturn、DC、3DO、32X、NDS/3DS、全量 CD 平台 | 明确拒绝加载，避免运行时 OOM 或伪支持 |

## 落地顺序

FC -> GB/GBC -> SMS/GG/SG-1000 -> PCE -> 早期街机 -> GBA -> Mega Drive -> CPS1 -> SFC 白名单。

`include/s31/config.h` 集中维护平台上限和目标帧率。`rom_loader` 在进入核心前执行大小检查，CPS2 等不支持平台的 ROM 上限为零。视频、音频、输入接口保持核心无关，后续可分别绑定 RGB 屏 DMA、I2S/DAC 和 GPIO/矩阵扫描驱动。

手柄输入支持 Bluetooth HID 和 USB HID 两条后端。Bluetooth Classic HID 会自动扫描并连接标准游戏手柄，适用于支持蓝牙 HID 模式的 Xbox/北通等型号；Type-A 使用官方 USB HID Host，并启用 `CONFIG_USB_HOST_HUBS_SUPPORTED`，允许一个 USB Hub 下枚举多个手柄。Xbox 专用 2.4 GHz Wireless 接收器不是蓝牙 HID，需要后续专用 USB 协议驱动。外接 Hub 建议使用带独立供电型号，避免多个手柄超过板载 USB Host 约 500 mA 供电能力。

NES 显示保持原生 256x240，通过 RGB565 最近邻 2x 放大到 512x480。GB/GBC 和 Game Gear 的 160x144 画面放大到 480x432；SMS/SG-1000 的 256 像素宽画面使用 2x 整数缩放。所有画面居中显示，不拉伸、不插值。

## 游戏文件

`/mnt/publicfile/xbwyxj321to1` 是 NES 合集，包含约 326 个 `.nes` 文件，总量约 100MiB。将其 `rom/` 目录内容复制到 SD 卡的 `/roms/` 后，固件会递归扫描并统计标准 iNES ROM；当前已接入的 mapper 为 `0/1/2/3/7/94/117/180`，扫描日志会显示可识别数量。不要把整个 Windows 模拟器目录直接复制到 `/roms/`。

`/mnt/publicfile/WinKawaks街机游戏合集` 是约 4.7GiB 的 Windows Kawaks/CPS/MVS 资源，包含 `WinKawaks.exe`、INI、EEPROM 和街机 ROM。当前工程没有 Kawaks/MAME/CPS1 核心，不能直接在 S31 上运行；该目录不应复制到 NES ROM 目录，也不能通过当前 NES 核心加载。

不同平台的文件可以混合放置，例如 `mario.nes`、`pokemon.gbc`、`sonic.sms`、`columns.gg`、`monaco.sg1000`、`soundtest.pce` 和 `arcade/sf2.zip` 可以共存。游戏目录层通过扩展名和 NES header 生成统一条目；屏幕类型显示使用 `FC`、`GB`、`GBC`、`SMS`、`GG`、`SG`、`PCE`、`GBA`、`ARCADE`。当前可启动 NES、GB/GBC、SMS/GG/SG-1000 和 PCE HuCard，其他条目显示为 `skip`。

USB 手柄支持运行时热插拔和 Hub 下连接/断开，不要求上电前插入。统一控制组合为 `Start + Select` 暂停/继续，暂停时自动保存 SRAM；`L + R + Start` 保存并返回游戏目录。存档位于 `/sdcard/saves/`。这是游戏电池存档，不是完整即时状态快照。

SMS/GG/SG-1000 使用 GPL-2.0 的 SMS Plus 核心，源码许可证位于 `third_party/smsplus/COPYING`。工程不包含 ColecoVision BIOS，也不启用 ColecoVision；不要向仓库加入受版权保护的 BIOS。

PCE 使用上游随附 GPLv2 `COPYING` 和 `CREDITS` 的 PCE-GO 核心，仅支持普通 HuCard `.pce`。当前不支持 PC Engine CD、Super CD、Arcade Card 或 BIOS 镜像；普通 HuCard 没有通用 SRAM，只有核心识别到 Populous 板载 32KiB RAM 时才启用存档。

## 主机侧验证

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

这些测试验证接口、目录识别、缩放和 SMS Plus 的短 ROM/单帧执行；真实 ROM 兼容性和帧率仍以 S31 实板为准。

## S31 USB0 烧录

本板烧录口是 Type-C 上的 CH340C，对应 ESP32-S31 UART0。Linux 通常枚举为 `/dev/ttyUSB0`；它不是 USB Type-A Host 口。烧录前按住 BOOT（GPIO61），点按 RESET，再释放 BOOT。

```sh
export IDF_PATH=/home/yjh/esp/esp-idf-master
export PATH=/home/yjh/.espressif/tools/riscv32-esp-elf/esp-16.1.0_20260609/riscv32-esp-elf/bin:$PATH
idf.py --preview -B build-s31 -DIDF_TARGET=esp32s31 -p /dev/ttyUSB0 -b 2000000 flash
idf.py --preview -B build-s31 -DIDF_TARGET=esp32s31 -p /dev/ttyUSB0 monitor
```

Flash 为 16MiB，factory 应用分区为 2MiB，剩余约 13.94MiB 为 FAT 数据区。构建后也可生成一个从地址 `0x0` 一次写入的合并镜像：

```sh
python -m esptool --chip esp32s31 merge-bin --flash-mode dio --flash-freq 80m \
  --flash-size 16MB -o build-s31/s31_game_merged.bin \
  0x2000 build-s31/bootloader/bootloader.bin \
  0x8000 build-s31/partition_table/partition-table.bin \
  0x10000 build-s31/s31_game.bin
python -m esptool --chip esp32s31 -p /dev/ttyUSB0 -b 2000000 \
  --before default-reset --after hard-reset --no-stub write-flash 0x0 \
  build-s31/s31_game_merged.bin
```

ESP32-S31 不能省略 bootloader 和分区表直接把 ESP-IDF 应用链接到 `0x0`；合并镜像只是把三者封装成一个从 `0x0` 写入的文件。

如果设备枚举名称不同，使用 `ls /dev/serial/by-id/` 找到 CH340 对应的稳定路径后替换 `/dev/ttyUSB0`。关闭 monitor 后再执行 flash，避免串口被占用。Type-A 口用于 USB Host 手柄/Hub，不要把它当作下载口。

## 临时串口传文件工具

固件在游戏目录界面监听 UART0，可将单个不超过 8MiB 的测试文件写到 SD 卡 `/roms/`。先关闭 `idf.py monitor`，安装 `pyserial` 后执行：

```sh
python -m pip install -i https://pypi.tuna.tsinghua.edu.cn/simple pyserial
python tools/s31_serial_put.py --port /dev/ttyUSB0 path/to/test.nes
```

文件名仅支持 ASCII 字母、数字、点、下划线和连字符。工具不会覆盖 SD 卡上的同名文件；传输时必须停留在游戏目录，成功后目录会自动刷新。协议先写 `/sdcard/roms/.s31-upload.part`，长度和 CRC32 校验通过后才改为最终文件名。该工具使用 115200 波特率且没有分块重传，只用于临时传输小文件；失败时可直接重新执行。

CH340 在部分主机上打开串口会复位板卡，工具默认等待 5 秒再发送，可用 `--settle` 调整。测试固件还支持串口运行和停止：

```sh
python tools/s31_serial_cmd.py --port /dev/ttyUSB0 run test_gb.gb
python tools/s31_serial_cmd.py --port /dev/ttyUSB0 stop
```

对应协议为 `S31RUN <filename>` 和 `S31STOP`；只接受 `/sdcard/roms` 根目录下已被目录服务识别的文件。

完整安装、烧录、参数、错误处理和协议说明见 [`tools/README.md`](tools/README.md)。
