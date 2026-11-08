# OTA Bootloader

独立编译的引导程序（48KB 分区），负责三件事：
1. **出厂初始化**：外部 Flash 没文件系统就现场格式化，建好目录、字库、参数文件，新机上电即可用；
2. **自检**：时钟 / 外部 Flash / 文件系统 / 配置文件 / 传感器 / 应用固件，结果直接显示在屏幕上；
3. **OTA**：把外部 Flash 里的固件包搬进内部 Flash，并在新固件起不来时回滚。

## 内部 Flash 分区

| 地址 | 大小 | 用途 |
| --- | --- | --- |
| `0x08000000` | 48 KB | **Bootloader**（sector 0~2，本目录） |
| `0x0800C000` | 16 KB | OTA 状态记录（sector 3，只有 bootloader 会写） |
| `0x08010000` | 448 KB | **应用**（sector 4~7，主工程的链接脚本） |

应用镜像不能超过 448 KB；目前约 296 KB。

## 外部 Flash 上的文件（FATFS 卷 `0:`）

| 路径 | 说明 |
| --- | --- |
| `0:/ota/FW.BIN` | 上位机上传的新固件包（80 字节头 + 镜像） |
| `0:/ota/PREV.BIN` | 升级前那版固件的快照，回滚源 |
| `0:/ota/BOOTOK.BIN` | 应用启动成功后写入，bootloader 用它确认“新固件能起来” |
| `0:/ota/REQ.BIN` | 应用写给 bootloader 的升级/回滚请求（带令牌，处理一次即失效） |
| `0:/font/bootloader/boot.bin` | bootloader 屏幕用的点阵字库（BFNT 格式；固件里也内嵌一份，缺失/损坏时自动用内嵌的）。bootloader 每次启动检查长度，和固件内置版本不一致就自动重写 |
| `0:/uparam.bin` | 参数文件（56 个参数 × 4 字节 = 224 字节）。**不存在时由 bootloader 用内置默认值创建**，已存在则绝不覆盖（保护用户校准值） |
| `0:/font`、`0:/images` | 给主程序准备的目录，初始化时自动创建 |

## 开机流程

```
上电
 ├─ 背光立即点亮（不依赖时钟/OS，任何一步出错屏幕都不是黑的）
 ├─ 时钟 168MHz → GPIO/按键 → 屏幕（只清成黑屏，不画 logo）→ 外部 Flash
 ├─ 挂载文件系统 ──失败──> 亮出自检界面 + “正在格式化 FAT16”（32MB 卷，4096 字节扇区）
 ├─ 出厂文件：建 /font、/font/bootloader、/images、/ota；字库长度不符就重写；缺 uparam.bin 就创建
 ├─ 自检 6 项静默执行（时钟/外部Flash/文件系统/配置文件/传感器/应用固件），正常时不出图
 ├─ OTA 决策（见下；需要搬运时自己切到升级界面）
 └─ 应用固件
      ├─ 存在 + 自检全通过   → 直接跳转（无 logo、无等待、黑屏一闪）
      ├─ 存在 + 有项目未通过 → 画出自检表 + 8 秒倒计时，按键可停留，超时跳转
      └─ 不存在 / 文件系统不可用 → 画出自检表并常驻（维护模式，慢闪 LED）
```

## OTA 决策流程

```
读 sector1 状态记录（CRC 不符就回落默认值）
 ├─ REQ.BIN 存在且令牌未被消费？
 │    ├─ op = 升级   → 校验 FW.BIN → 擦除 → 写入 → 回读 CRC → 记录 WAIT_BOOTOK
 │    └─ op = 回滚   → 校验 PREV.BIN → 擦除 → 写入 → 回读 CRC → 记录 IDLE
 ├─ 状态 = WAIT_BOOTOK？
 │    ├─ BOOTOK.BIN 的版本 == 记录的版本 → 标记 IDLE，删 BOOTOK（升级成功）
 │    └─ 否则 fail_count++，达到 3 次 → 用 PREV.BIN 回滚
 ├─ 应用区无效（首次安装 / 搬运中途掉电）→ 先试 FW.BIN，再试 PREV.BIN
 └─ 应用向量表有效 → 关中断、设 VTOR、跳转
```

任何一步失败都不会把设备变砖：应用区损坏时只要外部 Flash 上还有 `PREV.BIN` 就能恢复；
两块都没有时才需要用 ST-Link 重新烧录。

## 屏幕提示

**自检界面**（只在要格式化 / 有项目不通过 / 没有应用固件时出现，正常启动完全不出图）：

```
XM Power Kit 设备初始化
  时钟        168 MHz
  外部Flash   32 MB
  文件系统    已格式化        ← 首次上电会看到"已格式化"，之后是"已挂载"
  配置文件    已创建          ← 首次是"已创建"，之后是"就绪"
  传感器      3/3             ← INA226 + TMP102（I2C1）与 MCP4725（I2C3）应答个数
  应用固件    有效 / 无
  自检未通过 / 已停留维护界面
  8 秒后进入系统，按键停留
  初始化完成后自动进入系统
```

绿色=通过，红色=失败。格式化失败或应用固件无效时界面常驻，慢闪 LED 等待上位机/ST-Link。

**升级界面**：标题、升级包版本 / 大小 / CRC、进度条与百分比、当前动作
（正在校验升级包 / 正在擦除内部 Flash / 正在写入固件 / 正在校验写入结果 / 正在回滚到上一版本），
以及底部的“请勿断电，完成后自动重启”。

## 字库与脚本

字库彻底不可用时界面降级为进度条 + 红色提示块，不影响升级与初始化功能。

两套字库/数据脚本（都不需要手动上传产物，编译时会打进固件）：

```bash
python Bootloader/tools/gen_boot_font.py      # 扫描 Bootloader/*.c 里所有字符串 + 可打印 ASCII
                                              # → Bootloader/boot_font.bin（改文案后必须重跑）
python Bootloader/tools/gen_param_default.py  # 从 UserDefineManage.c 的默认值表生成
                                              # → Bootloader/Inc/boot_param_default.h
                                              # （改参数默认值后必须重跑）
```

## 编译

```bash
cmake -S . -B build/Release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/Release
# 产物：build/Release/Bootloader/XM_Power_Kit_Boot.hex（约 42 KB / 48 KB）
```

## 维护模式（USB 常驻）

停在维护界面（格式化失败 / 应用无效）时，bootloader 会初始化 USB CDC 并进入维护循环：

- 只有 USB 一条链路（bootloader 不初始化蓝牙），协议与主程序完全相同，上位机代码不用区分；
- 支持的命令：`SYS_INFO`（特性位 bit4 标记维护模式）、`SYS_REBOOT`、`FS_*` 全部文件操作、
  `OTA_APPLY`（把 `0:/ota/FW.BIN` 立刻写进内部 Flash 并重启）；
- 上位机连上后自动识别维护模式：参数配置页置灰，「固件升级」页的按钮变成「写入并安装」——
  选择 `XM_Power_Kit.bin` 点一下，就自动打包 → 上传到 `0:/ota/FW.BIN` → `OTA_APPLY` → 搬运安装；
- 「文件管理」页照常可用，可以往设备传字库、图片等文件；
- 屏幕提示行会随连接状态显示“维护模式：请连接上位机 USB”/“上位机已连接，可升级或传文件”。

## 首次安装（只需一次）

1. **烧 bootloader**：ST-Link 把 `build/Release/Bootloader/XM_Power_Kit_Boot.hex` 烧到 `0x08000000`
   （48KB）。这会顺带擦掉旧应用，bootloader 随即进入维护界面；
2. **上电并连上位机**：等屏幕出现自检表（应用固件：无），用 USB 连上位机；
3. **写入并安装**：上位机「固件升级」页选 `build/Release/XM_Power_Kit.bin` →
   点「写入并安装」→ bootloader 校验、擦除、写入、回读、跳转，屏幕显示版本/大小/CRC 与进度；
4. 以后升级直接用「固件升级」页，不再需要 ST-Link。

如果手边没有上位机，也可以把打包好的 `FW.BIN`（80 字节头 + 镜像）先放进 `0:/ota/`，
bootloader 上电时看到应用区无效会自己安装。

**全新板子（外部 Flash 是空的）**：不需要任何准备。上电 → bootloader 自动格式化、建目录、
写字库与参数文件 → 自检表显示“文件系统 已格式化 / 配置文件 已创建” → 停在维护界面等固件，
接上位机「写入并安装」即可。

## 恢复手段

| 现象 | 处理 |
| --- | --- |
| 升级后新固件起不来 | 自动：连续 3 次未确认 → 用 `PREV.BIN` 回滚 |
| 想主动退回上一版 | 上位机「固件升级」→「回滚到上一版本」 |
| `PREV.BIN` 也没有、应用区损坏 | ST-Link 烧 bootloader → 维护模式下「写入并安装」 |
| bootloader 自身损坏 | ST-Link 重烧 `XM_Power_Kit_Boot.hex` |