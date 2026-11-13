XM_POWER KIT
======================================================================

此仓库是本人立创开源项目 **XM POWER KIT** 的工程代码。

- 项目地址：https://oshwhub.com/cyxsnbb666/xm_power_kit_v7
- 本人博客：https://xuemeng.work
- 当前版本：**Rel.0014**（`STM32F407VE.261114.Rel.0014`）

硬件平台：STM32F407VE + W25Q256/W25Q128 外部 Flash（FATFS 挂载为 `0:`）+ ST7789 320×240 屏，
FreeRTOS，界面为自研图形库（已移除 LVGL），带 bootloader 与 OTA。

---

## 一、固件刷写

### 1.1 内部 Flash 分区（固定，不要烧错）

| 区域 | 起始地址 | 大小 | 内容 |
| --- | --- | --- | --- |
| bootloader | `0x08000000` | 48 KB | 出厂初始化、开机自检、OTA 搬运、维护模式 |
| OTA 状态记录 | `0x0800C000` | 16 KB | 升级记录、失败计数、令牌（只由 bootloader 写） |
| 应用 | `0x08010000` | 448 KB | 主固件（USB、蓝牙、各 APP） |

> 主程序**不能**烧到 `0x08000000`，否则 bootloader 被覆盖，OTA 与恢复能力一起失效。

### 1.2 首次刷写（新板子）

1. **ST-Link 只烧 bootloader**：目标地址 `0x08000000`，文件 `XM_Power_Kit_Boot.hex`。
2. **上电自动初始化**：bootloader 会格式化外部 Flash、建好 `/font` `/font/bootloader` `/images` `/ota`，
   写入内嵌字库与默认 `uparam.bin`，并跑一遍开机自检（时钟 / 外部 Flash / 文件系统 / 配置文件 / 传感器 / 应用固件）。
   全部通过就直接进系统；有项目失败才停在自检表上（按任意键可停留维护，8 秒倒计时后仍会尝试进入）。
3. **装主固件**：设备连电脑（USB CDC）→ 打开 `上位机.exe` → 连上设备 →「固件升级」页 →「写入并安装」→
   选择 `XM_Power_Kit.bin`（或自己编译出的 `build/Release/XM_Power_Kit.bin`）。
   设备自动重启进 bootloader 擦写、回读校验、跳转，全程约 10~15 秒。
4. **上传外部资源**：用上位机「文件管理」把 `font/` 里的 `.bin` 传到设备 `/font`，
   `images/` 里的 `.bin` 传到 `/images`（支持拖拽、文件夹递归上传）。
   漏传字库会只剩色块没有文字，漏传图标主界面图标是空白。

### 1.3 用 ST-Link 直接刷（跳过 OTA）

| 文件 | 烧写地址 |
| --- | --- |
| `XM_Power_Kit_Boot.hex` | `0x08000000` |
| `XM_Power_Kit.hex` | `0x08010000` |

两个 `.hex` 各自带地址信息，分别烧即可；`.bin` 是没有地址信息的原始镜像，只给 OTA / 上位机使用。

### 1.4 以后的升级（OTA）

- **正常升级**：上位机「固件升级」→ 选 `.bin` →「开始升级」。
  提交前会把当前运行镜像快照成 `0:/ota/PREV.BIN`，之后随时可以「回滚到上一版本」；
  新固件连续 3 次没有确认启动，bootloader 会自动回滚到 `PREV.BIN`。
- **刷坏了起不来**：上电时**按住任意键**，停在 bootloader 的**维护模式**（屏幕有提示），
  再用上位机「写入并安装」即可恢复。维护模式只走 USB CDC，蓝牙不支持升级。
- **离线方式**：把固件包直接拷到设备 `0:/ota/FW.BIN`（或 `/ota` 下任意 `.bin`）后重启，
  bootloader 会自动找到并安装。

### 1.5 刷写注意事项

- 升级过程中**不要断电**（约 10~15 秒）；写入前会先校验包头与镜像 CRC。
- 外部字库与图标按版本配套，升级固件后请一并重传资源（Rel.0014 的资源与 Rel.0013 及更早不通用）。
- 从没有 bootloader 的旧版本升上来，必须先用 ST-Link 烧一次 bootloader（只需一次）。
- 16 MB 及以上的外部 Flash 都支持（W25Q128 会格成 FAT12，W25Q256 是 FAT16），
  bootloader 按 JEDEC 容量自动切换 3/4 字节地址命令。

---

## 二、上位机

`V7上位机/` 是 PyQt5 编写的 PC 端工具，仓库里已带打包好的 `上位机.exe`。

- **连接**：USB CDC 或蓝牙串口，可修改蓝牙波特率（断电保存）。
- **参数配置**：59 项 / 18 分组，支持读取当前值、读取默认值、批量写入。
- **文件管理**：直接读写设备外部 Flash，支持拖拽上传、文件夹递归上传、批量下载、递归删除、容量显示。
- **固件升级**：选择 `.bin` 一键「写入并安装」；可查看可回滚版本、待安装包、失败计数，支持手动回滚。

细节见 [`V7上位机/README.md`](V7上位机/README.md)。

---

## 三、外部 Flash 资源

设备的 `0:` 就是外部 Flash，刷完固件后需要这些文件：

| 路径 | 内容 |
| --- | --- |
| `/font` | 界面与各 APP 的字库（`ui_text.bin` `ui_title.bin` `ui_mono.bin` `pd_text.bin`，绘图用的 `yahei*`、`JetBrainsMono*`、`NotoMono10x14.bin`、`Mono5x7.bin` 等） |
| `/font/bootloader` | bootloader 字库（可选，缺失时自动使用固件内嵌字库） |
| `/images` | 主界面图标、波形预览图等 |
| `/ota` | `FW.BIN` 待安装包、`PREV.BIN` 上一版本快照、`REQ.BIN` 升级请求、`BOOTOK.BIN` 启动确认 |
| `/uparam.bin` | 59 项参数（236 字节），缺失时 bootloader 会用默认值生成 |

发布包里已经整理好 `font/` 与 `images/`，整目录上传即可。

---

## 四、编译

```
cmake -S . -B build/Release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/Release
```

产物：

- `build/Release/XM_Power_Kit.hex` / `.bin`：应用（烧 `0x08010000`）
- `build/Release/Bootloader/XM_Power_Kit_Boot.hex`：bootloader（烧 `0x08000000`）

新增源文件后需要重新执行一次 `cmake -S . -B ...`（脚本用 `file(GLOB)` 收集源文件）。

---

## 五、相关文档

- 上位机（参数 / 文件管理 / OTA / 维护模式）：[`V7上位机/README.md`](V7上位机/README.md)
- Bootloader（出厂初始化、自检、OTA 搬运、回滚、恢复手段）：[`Bootloader/README.md`](Bootloader/README.md)
