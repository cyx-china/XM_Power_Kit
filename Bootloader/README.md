# OTA Bootloader

独立编译的 16KB 引导程序，负责把外部 Flash 里的固件包搬进内部 Flash，并在新固件起不来时回滚。

## 内部 Flash 分区

| 地址 | 大小 | 用途 |
| --- | --- | --- |
| `0x08000000` | 16 KB | **Bootloader**（sector 0，本目录） |
| `0x08004000` | 16 KB | OTA 状态记录（sector 1，只有 bootloader 会写） |
| `0x08008000` | 480 KB | **应用**（sector 2~7，主工程的链接脚本） |

应用镜像不能超过 480 KB；目前约 296 KB。

## 外部 Flash 上的文件（FATFS 卷 `0:`）

| 路径 | 说明 |
| --- | --- |
| `0:/ota/FW.BIN` | 上位机上传的新固件包（80 字节头 + 镜像） |
| `0:/ota/PREV.BIN` | 升级前那版固件的快照，回滚源 |
| `0:/ota/BOOTOK.BIN` | 应用启动成功后写入，bootloader 用它确认“新固件能起来” |
| `0:/ota/REQ.BIN` | 应用写给 bootloader 的升级/回滚请求（带令牌，处理一次即失效） |
| `0:/font/boot.bin` | bootloader 屏幕用的点阵字库（BFNT 格式） |

## 决策流程

```
上电
 ├─ 读 sector1 状态记录（CRC 不符就回落默认值）
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

320×240 的界面上显示：标题、升级包版本 / 大小 / CRC、进度条与百分比、当前动作
（正在校验升级包 / 正在擦除内部 Flash / 正在写入固件 / 正在校验写入结果 / 正在回滚到上一版本），
以及底部的“请勿断电，完成后自动重启”。

字库缺失时界面降级为进度条 + 红色提示块，不影响升级功能。

重新生成字库（扫描 `boot_ui.c` 里的所有字符串 + 可打印 ASCII，用系统黑体渲染）：

```bash
python tools/gen_boot_font.py
# 输出 Bootloader/boot_font.bin，用上位机「文件管理」上传到设备 /font/boot.bin
```

## 编译

```bash
cmake -S . -B build/Release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/Release
# 产物：build/Release/Bootloader/XM_Power_Kit_Boot.hex（约 15.7 KB / 16 KB）
```

## 首次安装（只需一次）

旧固件没有 OTA 能力，所以第一次得先把“包 + 字库”放进去，再用 ST-Link 烧 bootloader：

1. **打包**：新上位机「固件升级」页选 `build/Release/XM_Power_Kit.bin` → 「导出升级包…」
   → 存成 `FW.BIN`（80 字节头 + 镜像）；
2. **上传**：连上旧固件，用「文件管理」在根目录新建 `/ota`，把 `FW.BIN` 传进去
   （路径必须是 `0:/ota/FW.BIN`）；顺便把 `Bootloader/boot_font.bin` 传成 `/font/boot.bin`；
3. **烧 bootloader**：ST-Link 把 `build/Release/Bootloader/XM_Power_Kit_Boot.hex` 烧到 `0x08000000`，
   只烧这 16KB，不要全片擦除（会连同刚刚上传到外部 Flash 的内容一起没事，但内部 Flash 的应用区会被擦掉，
   这正是我们要的）；
4. **上电**：bootloader 发现应用区无效、`FW.BIN` 存在 → 自动校验、擦除、写入、回读校验 → 跳转；
   屏幕上会显示版本、大小、CRC 和进度；
5. 连上新固件，以后升级直接用「固件升级」页，不再需要 ST-Link。

## 恢复手段

| 现象 | 处理 |
| --- | --- |
| 升级后新固件起不来 | 自动：连续 3 次未确认 → 用 `PREV.BIN` 回滚 |
| 想主动退回上一版 | 上位机「固件升级」→「回滚到上一版本」 |
| `PREV.BIN` 也没有、应用区损坏 | ST-Link 烧 bootloader + 用文件管理上传 FW.BIN（需先能进系统）或直接 ST-Link 烧应用 |
| bootloader 自身损坏 | ST-Link 重烧 `XM_Power_Kit_Boot.hex` |
