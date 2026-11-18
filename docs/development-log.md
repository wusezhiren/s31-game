# 开发记录

## 2026-11-17

### 目标

为 S31（双核 320 MHz、16 MB PSRAM、800x480 RGB）建立三档平台适配基础，并按 FC -> GB/GBC -> 早期街机 -> GBA -> MD -> CPS1 -> SFC 白名单推进。

### 现状与问题

仓库初始为空 Git 仓库，没有既有 BSP、启动文件、模拟器核心或远端可用分支。因此不能直接声称已完成某个平台的实机适配，也无法在当前阶段绑定具体 SoC 的 RGB、音频和 SD 驱动。

### 最终方案

- 建立 `s31_platform_info_t` 能力矩阵，区分 `easy`、`optimized`、`unsupported` 三档。
- 用 ROM 上限在核心启动前拦截超预算或明确不支持的平台，避免将 16 MB PSRAM 误当作可全部用于 ROM 的空间。
- 预留 800x480 RGB565 视频帧、48 kHz 双声道音频环形缓冲和统一按键边沿检测接口。
- PCE 仅声明 HuCard 普通 ROM；CD 平台标记为需要流式读取且当前不支持。

### 取舍

- 主机侧先使用 `malloc` 和普通内存帧缓冲验证契约；接入 S31 BSP 时应替换为 PSRAM 分配、屏幕 DMA 双缓冲、I2S/DAC 输出及 SD 流式读取。
- GBA、SFC、MD、CPS1 等只进入优化档，不伪造全库满速承诺；SFC 特殊芯片、CPS2/CPS3、Neo Geo MVS 等直接列为不可支持。
- 未引入大型模拟器框架，先保持核心接口独立，降低后续 mapper/驱动接入风险。
- 手柄输入按统一状态接口接入：蓝牙标准 HID 使用 ESP-IDF `esp_hid` Host；单 USB 口使用官方 USB HID Host，并启用 Hub 支持，Hub 下多个手柄共享同一事件循环。Xbox 专用 2.4 GHz 接收器及厂商私有 HID 报告暂不伪造兼容。

### 验证

- 主机侧待执行：`cmake -S . -B build`、`cmake --build build`、`ctest --test-dir build --output-on-failure`。
- 未完成实板验证：具体 S31 SoC 型号、RGB 屏时序、PSRAM 映射、音频芯片、SD 卡驱动和双核任务分配均待 BSP 确认。
- 未验证模拟器核心性能：当前没有接入 FC、GB/GBC 或其他实际核心与 ROM 测试集。
- 未实板验证 Hub 拓扑、多个手柄并发报告、各型号 Xbox/北通协议差异和 USB 供电裕量；建议使用带独立电源的 USB 2.0 Hub。

### 硬件与手柄增量

- 复用 `/home/yjh/code/home/esp-s3/gateway_s31` 已跑通的 BSP 依赖和板级接口：`viewesmart/bsp_ues31s043h800v480c_u`、ESP-IDF master/preview、800x480 RGB、ES8389、SDMMC 和 ADC 按键。
- USB Type-A 使用 ESP-IDF 6.2 的 `espressif/usb` 与 `espressif/usb_host_hid` 组件，开启 `CONFIG_USB_HOST_HUBS_SUPPORTED=y`。Host Library 事件循环与 HID 驱动分离，可在单个物理 USB 口后挂 Hub 并枚举多个 HID 设备。
- USB HID/Hub 路径可编译并运行；Bluetooth Classic HID 的实际 GAP 适配依赖 ESP-IDF 官方示例私有实现及额外 Bluedroid 链接符号，当前默认固件用明确的 `ESP_ERR_NOT_SUPPORTED` stub 保持可链接。后续需将官方 `esp_hid_gap.c` 及其配套配置完整纳入后再启用蓝牙扫描。
- USB 标准 HID 游戏手柄路径已参与 S31 交叉编译；Xbox 专用 2.4 GHz 无线接收器、厂商私有报告布局和带独立协议的北通接收器仍需型号白名单及协议适配。

### 验证结果

- `cmake -S . -B build-host && cmake --build build-host && ctest --test-dir build-host --output-on-failure`：通过。
- `idf.py --preview -B build-s31 -DIDF_TARGET=esp32s31 build`，使用 ESP-IDF master、`esp-16.1.0_20260609` 工具链：通过，生成 `build-s31/s31_game.bin`，应用占用约 0xca8e0 字节，1 MiB 分区余量约 21%。
- 尚未烧录实板：RGB/音频/SD/触摸初始化、USB Hub 多手柄并发、Bluetooth HID 配对和 ROM 核心帧率仍未做实板验证。

### 2026-11-17 NES 显示缩放

- NES 核心原生输出为 256x240 RGB565；显示层固定使用 2x 最近邻复制为 512x480。
- 512x480 画面在 800x480 RGB 屏水平居中，左右各保留 144 像素黑边；不使用非整数缩放、不使用平滑插值，保持原生像素比例和清晰边缘。
- 新增 `s31_nes_scale_2x_rgb565()` 纯 C 缩放函数，并通过主机边界测试验证四个目标像素的 2x 复制关系。
- LVGL 使用 RGB565 image descriptor，图像对象尺寸为 512x480 并居中；缩放缓冲放在 PSRAM。
- 主机验证：`cmake -S . -B build-host && cmake --build build-host && ctest --test-dir build-host --output-on-failure`，2/2 通过。
- S31 验证：ESP-IDF master/preview `idf.py --preview -B build-s31 -DIDF_TARGET=esp32s31 build`，通过，生成 `build-s31/s31_game.bin`。
- 尚未实板验证屏幕实际颜色顺序、黑边位置和 NES 实时帧率；需要烧录后确认 RGB565 端序与 BSP 屏幕配置一致。

### 2026-11-17 USB0 烧录口确认

- 根据板卡资料和已跑通的网关工程确认：Type-C 上的 CH340C 连接 UART0，是固件下载和日志口；Linux 通常为 `/dev/ttyUSB0`。
- Type-A 连接独立 USB 2.0 High-Speed OTG PHY，作为 Host 给手柄/Hub 使用，不是下载口。
- 烧录命令固定使用 `idf.py --preview -B build-s31 -DIDF_TARGET=esp32s31 -p /dev/ttyUSB0 flash`；进入下载模式按住 BOOT（GPIO61）并点按 RESET。
- 当前只完成文档和命令路径确认，尚未在本次操作中连接实板执行 flash；实板启动、NES 画面和 `512x480` 黑边位置仍待验证。

### 2026-11-17 USB0 首次实板烧录与配置修复

- 首次通过 USB0 烧录旧构建成功并校验，但启动日志显示 `SPI Flash Size : 2MB`、未加入 PSRAM，BSP 在申请 RGB 帧缓冲时 `ESP_ERR_NO_MEM` 并 abort。
- 根因是工程只有 `sdkconfig.defaults.s31`，ESP-IDF 不会自动加载该文件；旧 `sdkconfig` 仍锁定 2MB/无 PSRAM。
- 新增标准 `sdkconfig.defaults`，删除旧生成配置和构建缓存后重新生成；同时修正 16MB 分区表，使 factory 1MB 与 storage 分区不越界。
- 第二次 USB0 烧录成功，Hash 校验通过。最终实板启动确认：16MB Flash QIO、16MB Octal PSRAM @200MHz、PSRAM memory test OK、RGB888 800x480、GT911、ES8389、SDHC 4-bit 均正常。
- USB Host 初始化从固定延时改为任务通知同步；最终日志出现 `USB host ready; hubs and multiple HID devices enabled`，不再出现先安装 HID 导致的 `ESP_ERR_INVALID_STATE`。
- 当前实板未加载 ROM：SD 已挂载，日志提示 `/sdcard/roms` 不存在。NES 画面、512x480 黑边位置、USB Hub 实际手柄报告和 NES 游戏帧率仍待将 ROM 放入 SD 后验证。

### 2026-11-17 游戏目录识别

- `/mnt/publicfile/xbwyxj321to1` 确认为 NES 合集：326 个 `.nes` 文件，约 104,085,977 字节；内容含标准 iNES 头，但 mapper 分布较广，当前核心只启用 mapper 0/1/2/3/7/94/117/180。
- `/mnt/publicfile/WinKawaks街机游戏合集` 确认为 Windows Kawaks/CPS/MVS 资源，约 4.7GiB，包含 Windows EXE、INI、EEPROM 和街机 ROM；当前没有对应街机核心，不接入 NES 路径。
- 新增统一游戏目录接口 `s31_game_catalog`，支持混合目录中的 NES、GB、GBC、SMS、GG、GBA 和 Arcade 扩展名识别，并为屏幕提供 `FC/GB/GBC/SMS/GG/GBA/ARCADE` 图标 ID；当前 UI 仍待接入实际游戏列表页面。
- 固件 ROM 扫描改为递归扫描 `/sdcard/roms` 下 `.nes/.NES` 文件，读取 iNES header 并统计文件数、容量和当前支持 mapper 数量；不自动复制或修改 `/mnt/publicfile` 原始目录。
- 尚未执行 SD 卡文件复制，因为当前只能通过 USB0 连接固件，主机目录不是设备上的 SD 挂载点；需要将 `xbwyxj321to1/rom` 内容复制到实体 SD 卡 `/roms/` 后再实板验证。

### 2026-11-17 NES 运行闭环、热插拔与存档

- 统一目录服务递归收集最多 512 个混合平台条目，按平台和标题排序；屏幕显示前 12 个条目的平台 icon ID、支持状态和标题。
- 当前启动策略选择第一个受支持 NES ROM，读取到内存后调用第三方核心 `nes_run()`，使用完整 CPU/PPU 帧循环；首帧到达后隐藏目录文字并显示 512x480 画面。
- USB Host/HID 驱动保持常驻事件循环，支持上电后插入、拔出和 Hub 枚举；断开时发布清空输入状态。
- `Start+Select` 切换暂停，暂停时写 8KB battery SRAM 到 `/sdcard/saves`；加载 ROM 时自动恢复。`L+R+Start` 保存并停止核心、返回目录。
- 电池存档与即时存档严格区分：当前没有完整 CPU/PPU/mapper 状态快照，不宣称支持随时恢复任意画面。
- 主机测试 3/3 通过；ESP32-S31 固件构建通过，镜像约 `0xdb410`，1MiB factory 分区剩余约 14%。尚未将本版本烧录并用真实 ROM、手柄热插拔、暂停及存档做实板验证。

### 2026-11-17 游戏目录状态机

- 将自动启动第一个 ROM 改为目录/运行/暂停三态状态机。目录支持上下选择、A 启动、B 重新扫描；每页显示 10 个条目并高亮当前项，未支持平台显示 `skip`。
- ROM 文件读取、核心创建/销毁和停止等待统一在主循环处理，USB HID 回调只更新按键位图，避免回调线程访问文件系统或释放核心。
- NES 使用独立 core1 PSRAM 栈任务执行 `nes_run()`；平台帧回调负责约 60Hz 节拍、逐帧输入同步和暂停门控。
- 暂停退出时先解除暂停再置退出标志，等待核心任务退出后再销毁，避免暂停状态死锁或 use-after-free。
- 空目录显示提示，B 可刷新；不支持平台或加载失败会在目录标题显示错误状态，不自动崩溃或切换核心。
- 主机测试 3/3、`git diff --check` 和 S31 构建通过；最终镜像约 `0xdb2c0`，1MiB factory 分区剩余约 14%。按用户要求先开发功能，当前版本尚未重新烧录，待后续拷贝 ROM 后一起做实板问题修复。

### 2026-11-17 GB/GBC 与 SMS/GG/SG-1000 接入

- 目标：在统一目录、输入、暂停、存档和显示框架中接入 Gnuboy 与 SMS Plus，应用容量按用户要求固定为 2MiB，并提供从 `0x0` 一次烧录的单文件镜像。
- 问题与原因：Gnuboy 旧代码触发 GCC 16 字符数组和未初始化变量警告；SMS Plus 原代码依赖未随源码提供的 ColecoVision BIOS、使用宿主机宽度不固定的 `long` 类型、RISC-V 上存在未对齐 32 位访问风险，且销毁时不释放渲染/Z80 表。SMS Plus 非 retro-go 构建还会把 CRC 固定为零，导致特殊 mapper 数据库失效。
- 最终方案：Gnuboy 通过局部编译选项兼容旧警告；SMS Plus 移除 Coleco BIOS 路径，改用固定宽度整数、本地 CRC32、安全未对齐访问，并在销毁时释放渲染和 Z80 资源。短于 32KiB 的 ROM 以 16KiB 页镜像，满足 SG-1000 初始 32KiB 映射。
- 集成：新增 `s31_sms` 生命周期，SMS/GG/SG-1000 共用核心；目录支持 `.sms/.gg/.sg/.sg1000`。核心输出的 8 位调色板索引转换为本机 RGB565；GG 使用 3x 480x432，SMS/SG-1000 使用 2x 整数缩放。统一 SRAM 保存为 32KiB，暂停和退出沿用现有组合键。
- 许可证：SMS Plus 的 GPL-2.0 文本保存在 `third_party/smsplus/COPYING`；没有复制或分发 ColecoVision BIOS，ColecoVision 路径明确禁用。
- Flash 布局：`factory` 位于 `0x10000`、大小 `0x200000`；FAT `storage` 位于 `0x210000`、大小 `0xDF0000`。ESP-IDF 仍需 bootloader 和分区表，另生成 `build-s31/s31_game_merged.bin`，可由 esptool 从 `0x0` 一次写入。
- 主机验证：`cmake -S . -B build-host -DCMAKE_BUILD_TYPE=Debug && cmake --build build-host -j4 && ctest --test-dir build-host --output-on-failure`，4/4 通过；SMS 测试执行了 16KiB SG-1000 ROM 镜像、Z80/VDP 单帧和 256x192 输出回调。
- 交叉构建：ESP-IDF 6.2 preview、ESP32-S31、GCC 16.1 构建通过；`s31_game.bin` 为 `0xF6A60`，2MiB 应用分区剩余 `0x1095A0`（52%）。合并镜像为 `0x106A60`。
- 尚未验证：真实 SMS/GG/SG-1000 ROM 的画面、颜色、音频、mapper 兼容性和持续帧率需本轮烧录后及后续放入 ROM 后实板验证；当前 SMS Plus 音频尚未送入 ES8389。
- 首次烧录合并镜像后，bootloader、16MiB Flash、2MiB factory 和 16MiB PSRAM 均启动正常，但 RGB 驱动因内部 DMA 内存不足无法分配默认 20 行双 bounce buffer。原因是新增核心代码压缩了内部 RAM 余量，而 BSP 的 `bsp_display_start_with_config()` 不向底层传递 bounce 高度。修复为在 LVGL 启动前显式调用 `bsp_display_new()`，保留双 PSRAM framebuffer，将 bounce 从 20 行降为 10 行，内部 DMA 预算约从 96KiB 降至 48KiB。
- 第二次启动显示已稳定，但 BSP 默认同时创建 I2S TX/RX，每方向 8x1024 帧 DMA，RX 初始化耗尽内部 DMA 堆。游戏固件不采集麦克风，最终将板级音频限定为 TX-only，并将 DMA 改为 4x512 帧；保留 ES8389 DAC 和功放输出，不以禁用音频规避问题。
- 最终实板验证：使用 `esptool write-flash 0x0 build-s31/s31_game_merged.bin` 写入 1,075,760 字节并通过 Hash 校验。冷启动确认 16MiB Flash、2MiB factory、16MiB Octal PSRAM @200MHz、双 PSRAM RGB888 framebuffer、10 行 bounce、GT911、ES8389 TX-only、32GB SDHC 4-bit 和 USB Host/Hub 均初始化成功；系统稳定进入游戏目录，SD 卡当前扫描到 0 个 ROM。真实 SMS/GG/SG-1000 ROM 的运行、颜色、声音、手柄与存档仍需放入对应 ROM 后验证。

### 2026-11-17 串口向 SD 卡传文件可行性

- 目标：确认当前 S31 是否能通过 Type-C 串口直接把主机文件复制到实体 SD 卡，并评估最小实现路径。
- 现状：CH340C 连接 UART0，当前仅用于 ESP ROM/`idf.py` 烧录及 115200 波特率日志；应用没有 UART RX 任务、文件上传命令、Console REPL 或 X/Y/ZMODEM。烧录协议写入板载 SPI Flash，不能写入 SDMMC 上挂载的 `/sdcard`。
- 硬件条件：SD 卡已通过 GPIO20-25 的 SDMMC 4-bit 接口挂载为可写 FAT VFS，现有存档路径已使用 `fopen("wb")`/`fwrite()`；UART 与 SDMMC 引脚不冲突，因此无需修改硬件即可增加上传功能。UART1 GPIO33/34 可作为独立数据口，但需确认接口引出并外接 3.3V USB-TTL。
- 推荐方案：复用 Type-C/UART0，增加带文件名、64 位长度、分块序号、CRC32 和 ACK/重传的接收协议及主机 `pyserial` 工具；固定写入 `/sdcard/roms`，过滤路径，先写 `.part`，总校验成功后再改名。上传期间禁止启动游戏、协调 FAT 访问并抑制 UART0 日志，避免日志污染二进制数据及目录扫描/存档并发写卡。
- 取舍：115200 8N1 理论载荷约 11.5KiB/s，实际约 10KiB/s，适合传少量 ROM；约 100MiB 的完整合集预计需要数小时，仍应使用读卡器。后续可实板验证 921600 波特率稳定性。
- 验证：本次仅核对仓库源码、BSP 引脚定义、构建配置及既有实板记录，没有实现协议、重新构建或进行实板串口传输。

### 2026-11-17 临时串口小文件上传工具

- 目标：为测试阶段提供无需拔 SD 卡的单文件上传路径，通过 Type-C/CH340C UART0 将小 ROM 写入 `/sdcard/roms`。
- 板端实现：新增低优先级 UART0 接收任务，协议头为 `S31PUT <size> <crc32> <filename>`；限制文件为 1 字节至 8MiB，文件名只允许 ASCII 字母、数字、点、下划线和连字符，并拒绝覆盖同名文件。数据先写固定 `.s31-upload.part`，完整长度和 CRC32 校验通过后再原子重命名。
- 并发处理：仅游戏目录状态接受上传；开始处理协议头时先设置上传活动标志，主循环在上传结束前不响应目录按键、启动游戏或重新扫描。上传任务完成落盘并清除活动标志后只设置通知，由主循环刷新目录和 UI。
- 主机工具：新增 `tools/s31_serial_put.py`，使用 `pyserial` 在 115200 波特率发送文件，并只解析带 `@S31` 前缀的板端响应以容忍普通日志混流。工具显式关闭 DTR/RTS，避免打开 CH340 时主动触发自动下载控制线；支持 `--port`、`--baud` 和 `--name`。
- 取舍：这是临时测试工具，整文件只有最终 CRC32，没有分块 ACK/重传；传输中断会删除临时文件，重新执行即可。保留 8MiB 上限和 115200 默认速率，不用于大合集。
- 验证：`cmake --build build-host -j4 && ctest --test-dir build-host --output-on-failure` 5/5 通过，覆盖协议头、日志混流响应、文件数据、非法路径和板端错误；ESP-IDF 6.2 preview ESP32-S31 构建通过，`s31_game.bin` 为 `0xfbdc0`，2MiB 应用分区剩余 51%；`git diff --check` 通过。
- 尚未验证：当前未连接实板烧录并执行 UART0 到实体 SD 卡的端到端传输，CH340 打开端口行为、持续 115200 数据接收和 FAT 落盘需实板确认。
- 文档：新增 `tools/README.md`，集中说明依赖安装、固件烧录、端口查找、上传命令、参数、安全规则、错误排查、线协议和临时工具限制；项目主 README 保留快速入口并链接到完整文档。

### 2026-11-17 六平台 UART 上传与 Shell 实板回归

- 目标：利用 UART0 上传功能，为 NES、GB、GBC、SMS、GG、SG-1000 各放入一个测试 ROM，并通过串口命令自动启动和停止，验证核心真实运行路径。
- 自动化接口：新增 `S31RUN <filename>` 和 `S31STOP`，UART 任务只排队命令，主应用任务负责目录扫描和核心生命周期；新增 `tools/s31_serial_cmd.py`。响应使用 `@S31 RUN OK/ERR` 和 `@S31 STOP OK`。
- 上传工具问题：CH340 打开串口会触发板卡复位，原工具立即发送导致命令在约 4 秒启动阶段被 bootloader 丢弃。`s31_serial_put.py` 和 `s31_serial_cmd.py` 现默认等待 5 秒，可通过 `--settle` 调整。六个文件均通过板端长度和 CRC32 校验，实测约 11.2KiB/s。
- 测试文件：NES 使用本地 mapper 0 的 `F1赛车.nes`，上传名 `test_nes.nes`；GB/GBC 使用 MIT 的 `dmg-acid2` v1.0 和 `cgb-acid2` v1.1；SMS/GG 使用作者发布的 `SMSTestSuite` v0.38、`GGTestSuite` v0.14，仅作内部兼容性测试；SG-1000 使用 GPL-2.0-or-later 的 240p Test Suite 固定提交 `d5c16c94`。海外官方 GitHub 源未找到可信国内镜像，下载后逐项核对 SHA256。
- Gnuboy 根因修复：`gnuboy_load_rom()` 的 bank 表直接引用传入 ROM，应用原先加载后立即释放缓冲，导致非法 opcode 和 CPU1 watchdog。适配层改为持有 ROM 副本直到核心销毁；同时将 `readw/writew` 改为字节组合以规避 RISC-V 未对齐访问，并为 `gnuboy_run()` 的 LY 循环加入每帧 154 行上限，确保异常 LCD 状态也能返回主循环。
- UART 响应修复：核心启动日志和直接 UART ACK 可能交错拆分，核心任务现延迟 50ms 启动，ACK 在日志排空后发送；主机工具按去空白连续流识别响应。
- 最终实板结果：`test_nes.nes`、`test_gb.gb`、`test_gbc.gbc`、`test_sms.sms`、`test_gg.gg`、`test_sg.sg1000` 均通过 `S31RUN` 启动，GB/GBC/SMS/GG/SG-1000 均收到首帧回调，持续运行 2 秒后通过 `S31STOP` 安全返回目录；最终日志无非法 opcode、task watchdog 或 abort。
- 验证：主机测试 5/5；ESP32-S31 交叉构建通过；最终合并镜像从 `0x0` 烧录并通过写后 Hash 校验。SD 卡保留六个测试 ROM，便于后续回归。

### 2026-11-17 PCE HuCard 接入

- 目标：在现有统一目录、输入、暂停、串口运行和核心生命周期框架中接入 PC Engine/TurboGrafx-16 HuCard，不扩展到 CD 系统。
- 核心与许可证：从固定 retro-go `d9b0c17a9ff37873e025606e160df50d291ae51b` 引入 `pce-go` 的五个核心源文件，保留上游 `COPYING`（GPLv2 文本）和 `CREDITS`。源码没有逐文件 SPDX，文档不额外断言 `or later`。
- 适配：新增 `s31_pce` 单实例包装层，使用可停止的单帧 `pce_run()`，不调用上游无限循环 `RunPCE()`；目录识别 `.pce`，图标 ID 为 `PCE`。输入映射 A/B、Select、Run 和方向键；PCE 任务使用 12KiB PSRAM 栈。
- 视频与音频：核心输出 368-byte stride 的 8-bit 索引帧，转换为最大 352x242 RGB565；能放入屏幕时使用 2x 整数缩放，否则 1x 居中。每帧生成 800 个 48kHz 立体声样本并送 ES8389。
- ROM 与存档：保留标准无头和 512-byte header HuCard 原始字节及 CRC；仅小于 8KiB 的合法 homebrew 镜像到最小 8KiB bank。普通 HuCard 无通用 SRAM；核心 CRC 表识别到 Populous 时保存/恢复 32KiB ExRAM。CD、Super CD、Arcade Card 和 BIOS 均不支持。
- 核心修复：非 retro-go 构建恢复本地 CRC32，确保特殊 mapper/Populous 表可用；HuC6280 16-bit 访问改为跨页安全字节读写，避免 RISC-V 未对齐访问；Populous ExRAM 增加分配检查和清零；固定 8-byte save-state magic 改为显式数组以兼容 GCC 16。
- 测试 ROM：使用 MIT 的官方 `pce-devel/PCE_Soundtest` 当前 `beep.pce`，实际下载为 65,536 字节，SHA256 `f0e993c987eaf13d6511c8b1273c6a11cfbddb7d04d54a0561021de81d7b33cb`。未找到可信国内官方镜像，因此使用作者 GitHub 仓库并校验哈希；SD 文件名为 `test_pce.pce`。
- 验证：主机测试扩展为 6/6；ESP32-S31 构建通过，`s31_game.bin` 为 `0x1087c0`，2MiB factory 剩余 48%；合并镜像从 `0x0` 烧录并通过 Hash 校验。实板日志显示 PCE ROM 8 banks、CRC `09705F67`、256x224 首帧；持续运行 5 秒后 `S31STOP` 正常返回。随后 NES 运行 2 秒回归通过，均无 watchdog、abort、OOM 或 Guru Meditation。

### 2026-11-17 全白屏与手柄枚举误启动修复

- 现象：固件正常启动并扫描目录，但屏幕看起来全白。串口无 panic，显示、触摸、音频和 USB Host 均初始化成功。
- 根因：应用未显式设置 LVGL screen 背景，默认浅色主题接近白色，同时目录文字固定为白色；NES 零帧图像创建后默认可见。另有 direct-mode 契约问题：双 RGB framebuffer 启用 direct mode 时，LVGL buffer 应为完整 800x480，而应用仍传 20 行大小。
- 修复：LVGL direct-mode buffer 改为 `BSP_LCD_H_RES * BSP_LCD_V_RES`；活动 screen 设置为不透明纯黑；NES/GB/SMS/PCE 四个游戏 image 初始全部隐藏，目录白字保持可见。启动日志新增 `LVGL catalog ready: black background, game images hidden`。
- SD 插曲：测试期间用户正在烧录/操作 SD，卡一度在 OCR `send_op_cond` 阶段超时。最终保留更稳健的 SD 上电流程：GPIO60 拉高断电 1 秒、GPIO20-25 高阻复位、拉低上电 1 秒后单次挂载。用户操作完成后 SDHC 恢复，扫描达到目录上限 512 条。
- 手柄问题：USB Hub 枚举出两个 HID interface，首个非零报告被目录主循环当作 A 键边沿，曾自动启动 `1942.nes`。新增目录输入门控：必须先收到一次全按键释放才启用目录按键；游戏运行输入不受影响。冷启动 11 秒验证两个 HID 连接后仍停留目录，无自动启动。
- 实板验证：黑色目录初始化日志正常；SDHC `USD00` 恢复；通过 `S31RUN 1942.nes` 启动 NES，运行 2 秒后 `S31STOP` 返回目录，无 watchdog、abort 或 Guru Meditation。`test_pce.pce` 因当前 512 条目录上限未进入索引，本轮未重复运行，但此前 PCE 256x224 首帧和停止路径已通过实板验证。
- 工具迁移：SD 完整上电周期延长了应用启动时间，`s31_serial_put.py` 与 `s31_serial_cmd.py` 默认 `--settle` 从 5 秒调整为 8 秒。

### 2026-11-17 北通 USB 手柄输入诊断

- 现象：用户只连接一个手柄，但日志出现两个 HID 连接且目录/游戏均不响应。实物 USB ID 为 `VID 20bc / PID 507f`，同一 USB 地址 `addr=1` 下有 interface 0 和 1，均为 HID class 03、subclass/protocol 0。
- 软件根因修复：原实现将每个 interface 的报告独立发布，后到的空/短报告会覆盖有效状态，任一 interface 断开也会清空整个物理设备。现改为以 `(addr, iface)` 保存状态、按物理 USB 地址聚合按钮；短报告或解码失败不发布零状态；只有全部 interface 离线才发布断开。通用解码器改为返回成功/失败，并增加常见 Report ID 偏移兼容。
- 设备诊断：额外 report descriptor 和 SET_IDLE 控制请求在该设备上超时，稳定版不再发送这些请求。配置描述符确认设备只有两个 HID interface，没有 Linux xpad 常见的 vendor-specific `ff/5d/01` 或 `ff/5d/81` interface，因此没有保留错误的 XInput低层驱动尝试。
- 当前硬件阻塞：两个 interface 均成功 open/start，但多轮 14-20 秒采集中没有任何 `HID_HOST_INTERFACE_EVENT_INPUT_REPORT`，按键端点没有收到数据。该状态通常表示无线手柄本体未开机、未与接收器配对，或接收器模式与手柄不匹配；仅凭接收器枚举无法完成实际按键映射。
- 验证：固件构建和从 `0x0` 合并镜像烧录均通过，写后 Hash 校验通过；主机测试 6/6。稳定版保留每个 interface 前 16 个 raw report 限流日志，待手柄配对成功后可直接继续确认按键布局。
- 用户确认手柄与接收器为原配 1 对 1，排除未配对假设。进一步确认设备配置只有两个 HID interface（class 03/subclass 00/protocol 00），每个各有端点，没有 xpad 常见 vendor interface。两种 descriptor 请求时序均在控制传输超时；SET_IDLE 不可提交；GET_REPORT 曾返回 `ESP_OK` 但长度下溢为 `0xfffffff8`，属于组件/设备组合的不安全结果，诊断代码已移除。
- 最终稳定版：保留多 interface 状态聚合、解码成功门控、Report ID兼容、断开聚合和 raw report限流日志；不发送失败的 descriptor/SET_IDLE/GET_REPORT请求，也不保留错误的低层 XInput client。当前硬件仍不产生 interrupt IN report；若需继续实现厂商 OUT 初始化，必须确认北通手柄准确型号和当前 PC/XInput/Android/Switch 模式，避免盲发厂商命令。
- 型号最终确认为北通阿修罗 2 Pro 星云、原配 1 对 1 接收器。公开检索未找到 `20bc:507f` 精确驱动、report layout 或初始化包。设备配置确认只有两个 HID interface，不是 xpad vendor interface。安全探测结果：同厂 Microsoft Compatibility Descriptor只读请求 `C0 EE 0000 0004` 成功但返回全零；两个 interface 的零震动 Output Report `00 00 00 00` 均成功，但仍无输入报告；因此这些尝试未保留。继续适配需要在 Windows 正常工作的主机上用 USBPcap/Wireshark 抓取插入接收器到首次按键之间的 control/interrupt OUT事务，或在 Linux 上提供 `lsusb -v` 与 usbmon 抓包。

### 2026-11-17 北通接收器最终输入修复与目录回归

- 前述“无 interrupt IN report、可能需要厂商初始化”是阶段性结论，最终实板结果已推翻。Linux 缓存的 interface 0 HID descriptor 长 202 字节；S31 专用驱动独占 interface 0 后，`GET_DESCRIPTOR` 成功返回 210 字节（含 8 字节 setup），标准 `SET_IDLE` 也成功。descriptor 仅定义 Input Report ID 3/4，不存在 Output 或 Feature report，不需要猜测厂商初始化命令。
- interface 0 配置描述符有 `0x81`、`0x83` 两个 64-byte interrupt IN endpoint。多轮实板确认 `0x83` 返回 `USB_TRANSFER_STATUS_ERROR`，`0x81` 才产生 11-byte Report ID 3；稳定驱动只监听 `0x81`。通用 HID Host 仍负责 interface 1，interface 0 由 `main/s31_betop_dual.c` 专用驱动负责。
- Report ID 3 的关键布局为 `[03][8-bit Constant][Hat][15 buttons + 1 consumer bit][X][Y][Rx][Ry][Z][Rz]`。早期代码漏掉 ID 后的 Constant，错误地把 Hat 当按钮低字节，导致按“下”直接触发 A 并进入游戏。专用和通用解码器均整体后移一字节，并要求报告至少 11 字节；新增 `s31_gamepad_test` 覆盖 neutral、Down 和 A，确保 Down 不包含 A。
- 实板原始报告确认空闲 Hat 为 `0x0f`，摇杆和按钮报告均来自 `0x81`。修复后十字键“下”不再启动游戏，随后 A 正常启动 `/sdcard/roms/飞机/1942.nes`；`S31STOP` 返回 `STOP OK`。诊断完成后移除 raw hexdump，保留 descriptor、SET_IDLE 和异常日志。
- 开机只显示 `text` 的原因是 LVGL label 创建后的默认占位文本可见，而首次目录更新可能因 100 ms 显示锁等待失败而静默返回。创建时立即设为空文本，目录渲染锁等待改为 1000 ms并记录超时。启动日志确认扫描并渲染 512 项，未出现锁超时。
- 验证：host CMake 构建通过；CTest 7/7（含新 gamepad test）通过；`git diff --check` 通过；ESP32-S31 交叉构建通过；合并镜像从 `0x0` 烧录并通过写后 Hash 校验。实体 `L+R+Start` 保存退出组合本轮未获得串口日志，退出链路以 `S31STOP` 验证。

### 2026-11-17 全屏中文游戏目录与翻页

- 现象：原目录仅为左上角一个默认 14 px 多行 label，在 800x480 屏幕上只占很小区域；所谓平台“图标”实际只是文本 `[FC]`、`[GB]`，中文 ROM 名还会因 LVGL 内置精简 CJK 子集缺字。
- UI 重构：目录改为 800x480 全屏深蓝界面，包含顶部标题/位置状态、10 行固定列表、彩色平台徽标、游戏名省略显示、可运行/不支持状态、选中行高亮和底部中文按键提示。平台徽标按 NES/GB/GBC/SMS/GG/SG/PCE 等使用不同颜色，不依赖外部图片资源。目录根容器通过统一 `hide_catalog()` 在 NES、GB/GBC、SMS/GG/SG-1000、PCE 显示及暂停路径隐藏，避免遮挡游戏画面。
- 中文字体：LVGL 内置思源字体只包含精简字符集，改用本机 `DroidSansFallbackFull.ttf` 通过 `lv_font_conv 1.5.3` 生成 `main/s31_font_cjk_16.c`。字体为 16 px、2 bpp，覆盖 ASCII `U+0020-U+007E` 和基本中日韩统一表意文字 `U+4E00-U+9FFF` 共 20,992 个汉字；启用 `CONFIG_LV_FONT_FMT_TXT_LARGE` 支持超过 20-bit bitmap index 的大字库。未覆盖 CJK Extension A/B 等生僻扩展区。
- 翻页：目录中 Up/Down 仍逐项移动，Left 跳到上一页首项，Right 跳到下一页首项；首页和最后一页不会越界。新增 `s31_game_catalog_page_move()` 及首页、中间页、末页 host 回归断言。
- Flash 布局迁移：完整字库使 app 实测增至 `0x26ef70`，factory 分区由 2 MiB扩至 3 MiB（`0x10000-0x310000`）；Flash FAT storage 改为 `0x310000-0x1000000`，大小 `0xCF0000`，约 12.94 MiB。SD 卡 ROM 不受该迁移影响；依赖旧 Flash FAT 偏移的镜像/数据需按新布局重建。
- 验证：host CTest 7/7 通过；`git diff --check` 通过；S31 交叉构建通过；`0x0` 合并镜像烧录并通过 Hash 校验。启动日志确认新 factory/storage 分区、PSRAM XIP、LVGL 全屏目录初始化和 512 项扫描，无 panic/assert/Guru Meditation。

### 2026-11-17 完整 CJK 字库方框修复

- 现象：完整 `U+4E00-U+9FFF` 字库首次烧录后，目录中文和英文均显示为方框。
- 根因：`lv_font_conv 1.5.3` 使用旧版 LVGL 输出模板，生成的 `lv_font_t` 在当前 LVGL 9.6 下缺少 `.static_bitmap = 1`，导致常量位图未按静态 bitmap 路径处理；所用 `DroidSansFallbackFull.ttf` 在转换结果中仅导出 ASCII 空格，英文字母也不能由主字体提供。另外标题曾显式覆盖为 Montserrat，绕过了目录根容器的 CJK 字体继承。
- 修复：为生成字体补充 LVGL 9.3+ 的 `static_bitmap` 标志；将 `lv_font_montserrat_14` 配置为 ASCII fallback；移除标题的局部 Montserrat 覆盖，使标题、列表、状态和底部提示统一继承完整 CJK 字体。
- 防回归：板级初始化时通过 `lv_font_get_glyph_dsc()` 检查“中”（U+4E2D）、“游”（U+6E38）和 `A`，启动日志输出 `CJK font self-test: Chinese=OK ASCII=OK`。该检查验证字形映射和 fallback，不能替代屏幕像素级截图验证。
- 验证：host CTest 7/7、`git diff --check`、S31 构建和 Flash Hash 均通过；最终实板启动日志为 `Chinese=OK ASCII=OK`，目录扫描 512 项，无 panic/assert/Guru Meditation。

### 2026-11-17 CJK 压缩位图解码最终修复

- 照片复核显示全屏布局、ASCII 文件名和 FC 徽标均正常，仅中文位置显示缺字框，证明前一轮 descriptor 自检不足以验证实际渲染。
- 最终根因：自定义字体由 `lv_font_conv` 默认生成压缩位图（`bitmap_format=1`），但项目的 `CONFIG_LV_USE_FONT_COMPRESSED` 未启用；LVGL 9.6 在 `lv_font_get_bitmap_fmt_txt()` 中因此直接返回 NULL。压缩字体还必须保持 `lv_font_t.static_bitmap=0`，不能沿用未压缩内置字体的 `static_bitmap=1`。
- 最终修复：启用 `CONFIG_LV_USE_FONT_COMPRESSED=y`；将自定义字体的 `static_bitmap` 设为 0；保留 Montserrat 14 ASCII fallback。
- 自检增强：除“中”“游”和 `A` 的 glyph descriptor 外，现为“中”创建 A8 draw buffer，调用实际 `get_glyph_bitmap()` 解压，并扫描非零像素。只有 descriptor、fallback 和解压位图都成功时才输出 `CJK font self-test: Chinese=OK ASCII=OK Bitmap=OK`。
- 验证：host CTest 7/7、`git diff --check`、S31 交叉构建、合并镜像烧录及 Hash 校验均通过；实板启动输出 `Chinese=OK ASCII=OK Bitmap=OK`，512 项目录扫描正常，无 compressed-font warning、panic/assert/Guru Meditation。

### 2026-11-17 NES Mapper 扩展与目录长按连发

- 目录“不支持”统计显示原工程只编译 8 个原生 NES Mapper，虽然代码树中已包含完整 InfoNES 兼容层。启用 `CONFIG_NES_MAPPER_INFONES_ENABLE` 和完整 Mapper 表，将约 130 个导入 Mapper 纳入 host/S31 构建；原生 Mapper 0/1/2/3/7/94/117/180 保持优先，其余才回退 InfoNES，避免改变已验证游戏路径。
- 目录支持判定改为与编译 Mapper 表一致。实板 512 项统计为 242 项支持、270 项不支持；进一步分类确认 270 项中有 242 个 Arcade `.zip`（当前没有街机核心）和 28 个无有效 iNES header 的 `.nes`，不是仍缺少常见 NES Mapper。界面分别显示“缺少核心”和“无效ROM”，不虚假标为可运行。
- 上下左右新增长按重复：首按立即触发，持续 400 ms 后进入连发，每 100 ms重复一次；Up/Down逐项移动，Left/Right整页移动。松开或切换方向会重置重复状态。
- 测试：新增首按、399 ms不重复、400 ms首次重复、100 ms周期重复和松开复位断言，以及 Mapper 4支持/Mapper 31不支持判定。host CTest 7/7通过；S31完整 Mapper集构建后 app为`0x28a9c0`，3 MiB分区余量15%；合并镜像烧录与Hash校验通过。

### 2026-11-18 中文目录字体样式优先级修复

- 现象：字体自检显示中文 glyph、ASCII fallback 和解压位图全部正常，但实屏所有中文仍不显示。
- 根因：自定义 CJK 字体只设置在目录根容器。LVGL 主题在每个 `lv_label` 上设置了局部默认 Montserrat 字体；局部样式优先于父对象继承，因此 label 实际没有使用 CJK 字体。此前字形自检只验证字体数据和解码路径，没有覆盖对象最终计算样式。
- 修复：为目录标题、状态、平台徽标、游戏名、运行状态、底部提示和暂停提示等全部 label 显式设置 `s31_font_cjk_16`，不再依赖父容器字体继承。保留字体内 Montserrat ASCII fallback。
- 验证：host CTest 7/7通过；ESP32-S31交叉构建通过，app大小`0x28aa60`，3 MiB factory余量15%；通过`/dev/ttyUSB0`烧录并完成Flash Hash校验。实板重启日志确认`CJK font self-test: Chinese=OK ASCII=OK Bitmap=OK`、目录扫描正常且无panic/assert。中文字形的最终屏幕观感仍需人工目视确认。

### 2026-11-18 北通 USB 接收器端点资源回归修复

- 现象：`20bc:507f` 接收器可以枚举出 interface 0/1，但日志出现`EP Alloc error: ESP_ERR_NO_MEM`和`failed to claim interface 0`，手柄没有输入。
- 根因：通用 HID Host 与北通专用客户端异步注册。通用 HID 先打开 interface 1 并分配硬件端点，北通专用客户端随后认领 interface 0 时已经没有可用端点；原代码只让 interface 0 绕过通用 HID，且没有等待专用客户端完成注册，形成资源所有权竞争。
- 修复：USB Host 安装后同步创建并注册北通专用客户端，再安装通用 HID Host；`20bc:507f`的interface 0和1均明确委派给专用驱动。专用驱动独占interface 0的有效`0x81` interrupt IN端点，其他型号和接口仍由通用HID处理。专用驱动注册失败只降低北通支持，不阻断通用USB手柄初始化。
- 验证：host CTest 7/7通过；ESP32-S31交叉构建通过，app大小`0x28aa60`；实板烧录并通过Flash Hash校验。重启日志确认两个接口均委派成功、interface 0认领成功、报告描述符读取210字节、`SET_IDLE`完成、`submit ep=0x81 mps=64: ESP_OK`，原端点分配和接口认领错误不再出现。实体按键动作仍需人工操作确认。

### 2026-11-18 CJK 字体切换为未压缩静态位图

- 现象：压缩字体的字符映射、运行时解压和 label 字体样式自检均通过，但用户确认屏幕中文仍不显示，说明已有自检没有覆盖实际绘制管线的最终效果。
- 处理：使用同一`DroidSansFallbackFull.ttf`重新生成16 px、1 bpp、未压缩字体，覆盖空格及`U+4E00-U+9FFF`；设置`bitmap_format=0`和LVGL 9.3+的`static_bitmap=1`，彻底移除运行时RLE解压依赖。ASCII继续通过`lv_font_montserrat_14` fallback提供。
- 取舍：1 bpp没有2 bpp抗锯齿灰阶，但字形路径更直接、运行时开销更低，生成源码从约9.5 MB降至5.9 MB。固件从`0x28aa60`缩小到`0x1fed10`，3 MiB factory余量由15%提高到33%，因此保持现有分区表和Flash storage容量不变。
- 验证：host CTest 7/7通过；ESP32-S31交叉构建通过；`/dev/ttyUSB0`烧录并通过Flash Hash校验。实板启动确认`Chinese=OK ASCII=OK Bitmap=OK`、364项目录正常、北通`0x81`端点仍为`ESP_OK`，无panic/assert。串口无法验证屏幕最终观感，未压缩字体版的中文显示仍需用户目视确认。

### 2026-11-18 S31 构建环境可复现信息入库

- 目标：不提交约727 MB的ESP-IDF源码或约979 MB的RISC-V工具链二进制，只在仓库保存另一台电脑恢复构建环境所需的精确信息。
- 记录：新增`docs/s31-build-environment.md`，固定ESP-IDF仓库、`827cc622` commit、S31蓝牙子模块commit、Python/CMake/Ninja版本、`riscv32-esp-elf` 16.1.0版本、关键S31文件路径和SHA-256，以及已成功的S31构建命令。
- 依赖：将`dependencies.lock`纳入版本控制，以固定BSP 1.0.2、LVGL 9.6.0~1、USB/HID和音频组件版本及组件哈希；`sdkconfig`继续忽略，由`defaults`在目标机器重新生成。
- 取舍：文档明确该游戏工程绑定S31硬件，ESP32-S3/C3应使用独立示例工程做IDF target烟雾测试，不能把本工程跨target构建失败误判为IDF环境失败。

### 2026-11-18 SD 卡中文游戏文件名编码修复

- 现象：界面标题、状态和操作提示等系统中文可以正常显示，但从SD卡扫描得到的中文游戏名称不能显示，因此问题不在LVGL字体或中文绘制链路。
- 根因：ESP-IDF FatFs默认配置为CP437代码页和ANSI/OEM API。FAT长文件名在磁盘上使用Unicode，但`readdir()`返回给应用的名称不是UTF-8；目录代码直接将`d_name`传给UTF-8模式的LVGL，导致中文文件名无法解码。
- 修复：在通用和S31默认配置中启用FatFs heap长文件名、CP936代码页和UTF-8 API：`CONFIG_FATFS_LFN_HEAP=y`、`CONFIG_FATFS_CODEPAGE_936=y`、`CONFIG_FATFS_API_ENCODING_UTF_8=y`。目录扫描在实板上首次遇到非ASCII名称时输出`UTF-8 filename from FAT`诊断，便于确认转换结果。
- 验证：host CTest 7/7通过；ESP32-S31交叉构建通过，app大小`0x229750`，3 MiB factory余量28%。由于`/dev/ttyUSB0`当时持续返回`EIO`，本轮尚未烧录，SD中文文件名显示和诊断日志仍需实板确认。
