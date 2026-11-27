# S31 临时串口传文件工具

`s31_serial_put.py` 用于测试阶段通过 S31 Type-C 口上的 CH340C/UART0，将单个小文件传到 SD 卡的 `/roms/` 目录。它适合临时传输 NES、GB、GBC、SMS、GG 或 SG-1000 测试 ROM，不适合复制大型合集。

## 前提条件

- S31 已烧录包含 `s31_serial_upload` 的当前固件。
- SD 卡已插入并能被固件挂载。
- S31 停留在游戏目录界面，不能正在运行或暂停游戏。
- `idf.py monitor`、串口终端和其他程序均已关闭，串口必须由上传工具独占。
- Linux 用户对 `/dev/ttyUSB0` 有读写权限。

## 安装依赖

工具需要 Python 3 和 `pyserial`：

```sh
python3 -m pip install -i https://pypi.tuna.tsinghua.edu.cn/simple pyserial
```

如果系统 Python 不允许全局安装，可以创建虚拟环境：

```sh
python3 -m venv .venv
. .venv/bin/activate
python -m pip install -i https://pypi.tuna.tsinghua.edu.cn/simple pyserial
```

## 烧录固件

首次使用或固件尚未包含串口接收功能时，在项目根目录执行：

```sh
. /home/yjh/esp/esp-idf-master/export.sh
idf.py --preview -B build-s31 -DIDF_TARGET=esp32s31 build
idf.py --preview -B build-s31 -DIDF_TARGET=esp32s31 \
  -p /dev/ttyUSB0 -b 2000000 flash
```

如果设备没有自动进入下载模式，按住 BOOT，点按 RESET，再释放 BOOT，然后重新执行烧录命令。烧录完成并复位后，等待设备进入游戏目录界面。

## 发送文件

在项目根目录执行：

```sh
python3 tools/s31_serial_put.py \
  --port /dev/ttyUSB0 \
  path/to/test.nes
```

默认使用本地文件名作为 SD 卡中的文件名。例如 `path/to/test.nes` 会写为：

```text
/sdcard/roms/test.nes
```

可以用 `--name` 指定目标文件名：

```sh
python3 tools/s31_serial_put.py \
  --port /dev/ttyUSB0 \
  --name mapper-test.nes \
  path/to/local-file.nes
```

传输成功时会输出字节数、目标文件名、耗时和平均速度，例如：

```text
sent 40976 bytes as mapper-test.nes in 3.8s (10.5 KiB/s)
```

游戏目录会在传输完成后自动刷新。

## 查找串口

Linux 通常将 CH340C 枚举为 `/dev/ttyUSB0`。设备编号不确定时可查看稳定设备名：

```sh
ls -l /dev/serial/by-id/
```

然后将完整路径传给 `--port`：

```sh
python3 tools/s31_serial_put.py \
  --port /dev/serial/by-id/usb-1a86_USB_Serial-if00-port0 \
  path/to/test.nes
```

如果出现权限错误，可将当前用户加入串口设备所属组。常见发行版使用 `dialout`：

```sh
sudo usermod -aG dialout "$USER"
```

重新登录后组权限才会生效。不要长期使用 `sudo` 运行工具来绕过串口权限。

## 参数

```text
usage: s31_serial_put.py [-h] [--port PORT] [--baud BAUD] [--name NAME] file
```

| 参数 | 默认值 | 说明 |
| --- | --- | --- |
| `file` | 无 | 要发送的本地文件 |
| `--port` | `/dev/ttyUSB0` | CH340C 串口设备 |
| `--baud` | `115200` | 串口波特率，必须与固件一致 |
| `--name` | 本地文件名 | `/sdcard/roms/` 下的目标文件名 |

当前固件使用 115200 波特率。除非同时修改并验证板端配置，否则不要改变 `--baud`。

## 安全规则

- 文件大小必须在 1 字节至 8MiB 之间。
- 目标文件名最长 95 个字符。
- 文件名只能包含 ASCII 字母、数字、`.`、`_` 和 `-`。
- 文件名不能以 `.` 开头，不能包含目录、空格、中文或 `..` 路径跳转。
- 工具不会覆盖 SD 卡中已有的同名文件。
- 接收数据先写入 `/sdcard/roms/.s31-upload.part`。
- 只有长度和 CRC32 均正确时，临时文件才会改为最终文件名。
- 超时、断线、写入失败或 CRC 错误时，板端会删除临时文件。

## 常见错误

### `No module named 'serial'`

当前 Python 环境没有安装 `pyserial`。使用运行工具的同一个 Python 安装：

```sh
python3 -m pip install -i https://pypi.tuna.tsinghua.edu.cn/simple pyserial
```

### `Permission denied: /dev/ttyUSB0`

当前用户没有串口权限。检查设备所属组，并按“查找串口”一节加入对应用户组。

### `Device or resource busy`

串口正被 `idf.py monitor`、串口终端或其他程序占用。关闭占用程序后重试。

### `ERR BUSY`

S31 正在运行或暂停游戏。返回游戏目录界面后重新发送。

### `ERR EXISTS`

SD 卡的 `/roms/` 中已经存在同名文件。换一个 `--name`，或者使用读卡器删除旧文件。临时工具不会覆盖文件。

### `ERR OPEN`

板端无法创建临时文件。检查 SD 卡是否插入、是否成功挂载、是否有剩余空间，以及文件系统是否可写。

### `ERR TRANSFER`

串口接收超时或 SD 卡写入失败。确认 USB 连接稳定、SD 卡未被拔出，然后重新执行命令。

### `ERR CRC`

接收到的数据与主机文件不一致。重新发送；如果持续出现，缩短 USB 线、避免使用不稳定 Hub，并确认仍使用 115200 波特率。

### 等待 `READY` 或 `OK` 超时

依次检查：

1. 固件是否为包含串口上传功能的新版本。
2. S31 是否已正常启动并停留在游戏目录。
3. 端口是否确实对应 Type-C 上的 CH340C。
4. `idf.py monitor` 是否已经关闭。
5. 设备打开串口后是否意外复位；等待启动完成后再执行一次。

## 协议说明

主机首先发送一行 ASCII 请求：

```text
S31PUT <十进制字节数> <8位十六进制CRC32> <文件名>\n
```

板端接受请求后返回：

```text
@S31 READY
```

主机随后发送指定长度的原始文件数据。板端完成写入和 CRC32 校验后返回：

```text
@S31 OK
```

失败响应以 `@S31 ERR` 开头。固定前缀用于让主机工具忽略 UART0 上混杂的普通启动和运行日志。

## 当前限制

- 这是测试用临时协议，不提供分块 ACK、断点续传或自动重传。
- 默认 115200 8N1，实际速度通常约 10KiB/s。
- 一次只能发送一个文件。
- 只能写入 `/sdcard/roms/` 根目录，不能创建子目录。
- 不能覆盖、删除、读取或列出 SD 卡文件。
- 传输期间不要拔出 SD 卡、复位设备或启动游戏。
- 大文件和完整 ROM 合集应使用 SD 读卡器复制。
