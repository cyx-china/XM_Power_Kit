# XM Power Kit V7 上位机

配套固件：`UserCode/Task/APP/SerialCoreTask/`（协议 v2）。
USB CDC 与蓝牙（USART3）两条链路共用同一套协议，上位机在 Windows 上看到的都是 COM 口。

## 运行

```bash
pip install -r requirements.txt
python main.py
```

目录里已带打包好的单文件 `上位机.exe`（约 37 MB，双击即用，无需装 Python）。
改了代码后重新打包：

```bash
pip install pyinstaller
pyinstaller main.spec
# 产物：dist/上位机.exe
```

无硬件自检（验证协议编解码、参数、文件读写逻辑）：

```bash
python selftest.py
```

## 界面

| 页签 | 内容 |
| --- | --- |
| 参数配置 | 5 个大类、18 个分组，共 56 个参数；可读当前值/默认值，修改后统一写入；表头「新值」留空表示不修改 |
| 文件管理 | 浏览设备外部 Flash 上的 FATFS（类似资源管理器）；上传/下载/删除/重命名/新建文件夹；显示容量与传输进度，可取消 |
| 固件升级 | 选择 `XM_Power_Kit.bin`（自动识别版本/CRC）→ 写入设备外部 Flash → 设备重启由 bootloader 安装；可一键回滚到上一版本 |
| 设备信息 | 固件版本、协议版本、参数数量、Flash 容量、蓝牙波特率设置、刷新字库缓存、重启设备 |

连接后会自动读取一次当前参数与根目录。

界面随窗口大小整体缩放：字号、内边距、表格行高、列宽、导航宽度同步变化（0.80×～2.00×，
基准 1180×760），高分屏（4K / 系统缩放 150% 以上）自动跟随系统缩放，不会出现界面过小。

### 更新字库的流程

1. 文件管理页进入 `/font`（没有就新建）；
2. 「上传文件」选择 `.bin` 字模（如 `NotoMono10x14.bin`、`Mono5x7.bin`）；
3. 回到设备信息页点「刷新字库缓存」——手动绘制的界面立即使用新字模；
4. LVGL 界面的字体是启动时 `lv_font_load` 加载的，需点「重启设备」才生效。

### 固件升级（OTA）

升级包 = 80 字节包头 + 裸镜像（`.bin`）。包头字段：magic `"XMK7OTA1"`、版本串（32B）、镜像大小、镜像 CRC32、
包头 CRC32、目标地址（0x08008000）、构建日期（8B）、机型（16B）。

流程：

1. 编译固件，得到 `build/Release/XM_Power_Kit.bin`（CMake 的 POST_BUILD 会自动生成 `.bin` 和 `.hex`）；
2. 「固件升级」页 →「选择文件」→ 自动读出镜像里的版本串（就是 `SYS_VERSION_CODE`）并算出 CRC32；
3. 点「开始升级」：`OTA_BEGIN` 写包头 → `OTA_DATA` 分块传镜像（432 字节/块）→ `OTA_COMMIT` 校验并让设备重启；
4. 设备重启后进入 bootloader：校验包 CRC → 擦除应用分区 → 写入 → 回读校验 → 跳转新固件，屏幕上显示进度；
5. 新固件第一次启动时写 `BOOTOK.BIN` 确认，bootloader 从此认为本次升级成功。

回滚：

- 提交升级前，应用会把**当前正在运行的镜像**快照成 `0:/ota/GOOD.BIN`，这就是回滚源；
- 新固件连续 3 次启动失败（没写 BOOTOK）→ bootloader 自动用 `GOOD.BIN` 刷回；
- 也可以手动点「回滚到上一版本」，设备重启后 bootloader 直接用 `GOOD.BIN` 覆盖。

外部 Flash 上的 OTA 文件：`0:/ota/FW.BIN`（待安装包）、`GOOD.BIN`（回滚快照）、`BOOTOK.BIN`（启动成功标记）、
`REQ.BIN`（应用写给 bootloader 的请求，含令牌防止重复执行）。

注意事项：

- 升级期间不要断电；上传中断只会留下不完整的 `FW.BIN`，没有 `REQ.BIN` 的设备照常启动旧固件；
- 内部 Flash 分区：`0x08000000`+16KB = bootloader，`0x08004000`+16KB = OTA 状态记录，`0x08008000`+480KB = 应用；
- 首次使用需要先用 ST-Link 把 `build/Release/Bootloader/XM_Power_Kit_Boot.hex` 烧到 `0x08000000`（只需一次）；
- bootloader 屏幕界面用的字库是 `0:/font/boot.bin`，由 `python tools/gen_boot_font.py` 生成，
  首次也要通过文件管理页上传到设备的 `/font` 目录（缺字库时界面降级成进度条+色块，但不影响升级）。

### 蓝牙

- 固件默认 115200 8N1（`USART3`，PB10/PB11）；
- 蓝牙模块的波特率必须与设备一致，不一致时先在「设备信息」页改设备端波特率（建议用 USB 连接改，改完重新连接蓝牙）；
- 设备端会把波特率保存在外部 Flash 的 `0:/btcfg.bin`，断电不丢。

## 协议

```
A5 5A | LEN_L LEN_H | CMD | SEQ | DATA[LEN] | CRC_L CRC_H
```

- `LEN`：负载长度（0~512，小端）
- `CRC16`：CCITT-FALSE（poly 0x1021，初值 0xFFFF），覆盖 `LEN` 到 `DATA` 末尾
- 应答：`CMD`/`SEQ` 原样返回，`DATA[0]` 为状态码（0 = 成功），`DATA[1..]` 为数据

| CMD | 名称 | 请求 | 应答 |
| --- | --- | --- | --- |
| 0x01 | SYS_INFO | — | proto, 固件版本串, 参数个数, 参数字节数, 特性位, 蓝牙波特率, 总容量, 剩余容量 |
| 0x02 | SYS_REBOOT | — | — |
| 0x03 | SYS_BT_BAUD | u32 波特率 | — |
| 0x10 | PARAM_GET | u8 kind（0 当前 / 1 默认） | 224 字节参数 |
| 0x11 | PARAM_SET | 224 字节参数 | — |
| 0x20 | FS_LIST | len+path, u16 start, u8 max | count, u16 next（0xFFFF 结束）, 条目 |
| 0x21 | FS_READ | len+path, u32 offset, u16 len | u16 长度 + 数据 |
| 0x22 | FS_WRITE | len+path, u32 offset, u16 len + 数据 | u16 已写字节数 |
| 0x23 | FS_DELETE | len+path | — |
| 0x24 | FS_RENAME | len+old, len+new | — |
| 0x25 | FS_INFO | — | u32 总 KB, u32 剩余 KB |
| 0x26 | FS_MKDIR | len+path | — |
| 0x27 | FS_STAT | len+path | u8 属性, u32 大小 |
| 0x28 | FS_SYNC | — | — |
| 0x30 | OTA_INFO | — | flags, 上传中, 应用占用, GOOD 版本/大小, FW 版本/大小/CRC, 失败次数, 状态 |
| 0x31 | OTA_BEGIN | 80 字节包头 | u32 已写（0） |
| 0x32 | OTA_DATA | u32 包内偏移 + 数据 | u32 累计已写镜像字节数 |
| 0x33 | OTA_COMMIT | — | —（设备随后重启） |
| 0x34 | OTA_ABORT | — | — |
| 0x35 | OTA_ROLLBACK | — | —（设备随后重启） |

状态码：0 成功、1 参数错误、2 文件系统错误（附带 FRESULT）、3 不存在、4 数据过长、5 忙、6 不支持、7 校验失败。

单次文件读写数据量上限 432 字节（受 512 字节负载限制），大文件由上位机按偏移分块传输。

## 文件

| 文件 | 作用 |
| --- | --- |
| `protocol.py` | 帧编解码、CRC16/CRC32、命令/状态码常量、升级包格式常量 |
| `params.py` | 56 个参数的类型/名称/单位/界面分组，编解码 |
| `device.py` | 串口链路、请求重试、参数与文件系统高层接口 |
| `worker.py` | 后台线程，所有串口操作与文件传输在这里执行 |
| `ui_params.py` | 参数配置页 |
| `ui_files.py` | 文件管理页 |
| `ota.py` | 升级包组装/解析、镜像版本识别、CRC32 |
| `ui_ota.py` | 固件升级页 |
| `main.py` | 主窗口 + 连接栏 + 设备信息页 |
| `selftest.py` | 无硬件自检 |
