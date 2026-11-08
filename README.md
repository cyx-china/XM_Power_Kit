XM_POWER KIT
======================================================================

此仓库是本人立创开源项目：XM POWER KIT的工程代码

项目地址如下：https://oshwhub.com/cyxsnbb666/xm_power_kit_v7

本人博客如下：https://xuemeng.work

----------------------------------------------------------------------
开发/升级相关文档
----------------------------------------------------------------------

- 上位机（含打包好的 exe）：V7上位机/README.md
  · 参数配置、文件管理（直接读写外部 Flash）、固件升级（OTA）
  · 设备停在 bootloader 维护界面时可直接「写入并安装」，一键刷机
- Bootloader（出厂初始化 / 开机自检 / OTA / 回滚）：Bootloader/README.md
  · 内部 Flash 分区：0x08000000+48KB bootloader，0x0800C000+16KB 状态记录，0x08010000+448KB 应用
  · 新板子上电自动格式化外部 Flash、建目录、写字库与参数文件，然后停在维护界面等上位机装固件
- 编译：cmake -S . -B build/Release -G Ninja -DCMAKE_BUILD_TYPE=Release
        cmake --build build/Release
  · 产物：build/Release/XM_Power_Kit.hex/.bin（应用）、
          build/Release/Bootloader/XM_Power_Kit_Boot.hex（bootloader，烧 0x08000000）
