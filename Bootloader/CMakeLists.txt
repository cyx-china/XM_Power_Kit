# ============================================================================
# OTA Bootloader：独立可执行文件，链接到内部 Flash 0x08000000（16KB 分区）
#
# 说明：
#   * 本目标不改动应用目标的任何设置，两者各自编译、各自链接。
#   * 工具链文件里硬编码了应用的 -T（STM32F407xx_FLASH.ld）和 -Wl,-Map，
#     这里用 target_link_options 在后面追加自己的，ld 取最后一个 -T / -Map 生效。
# ============================================================================
set(BOOT_NAME "XM_Power_Kit_Boot")
set(BOOT_DIR  ${CMAKE_CURRENT_SOURCE_DIR})
set(PROJ_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(HAL_DIR   ${PROJ_DIR}/Drivers/STM32F4xx_HAL_Driver/Src)

file(GLOB BOOT_SOURCES ${BOOT_DIR}/*.c)

add_executable(${BOOT_NAME}
        ${BOOT_SOURCES}

        ${PROJ_DIR}/startup_stm32f407xx.s
        ${PROJ_DIR}/Core/Src/system_stm32f4xx.c

        ${HAL_DIR}/stm32f4xx_hal.c
        ${HAL_DIR}/stm32f4xx_hal_cortex.c
        ${HAL_DIR}/stm32f4xx_hal_rcc.c
        ${HAL_DIR}/stm32f4xx_hal_rcc_ex.c
        ${HAL_DIR}/stm32f4xx_hal_gpio.c
        ${HAL_DIR}/stm32f4xx_hal_flash.c
        ${HAL_DIR}/stm32f4xx_hal_flash_ex.c
        ${HAL_DIR}/stm32f4xx_hal_sram.c
        ${HAL_DIR}/stm32f4xx_ll_fsmc.c

        ${PROJ_DIR}/FATFS/ff.c
        ${PROJ_DIR}/FATFS/ffunicode.c
)

target_include_directories(${BOOT_NAME} PRIVATE
        ${BOOT_DIR}/Inc
        ${PROJ_DIR}/Core/Inc
        ${PROJ_DIR}/Drivers/STM32F4xx_HAL_Driver/Inc
        ${PROJ_DIR}/Drivers/CMSIS/Device/ST/STM32F4xx/Include
        ${PROJ_DIR}/Drivers/CMSIS/Include
        ${PROJ_DIR}/FATFS
)

target_compile_definitions(${BOOT_NAME} PRIVATE
        USE_HAL_DRIVER
        STM32F407xx
        BOOTLOADER
)

target_compile_options(${BOOT_NAME} PRIVATE
        -Os -g0
        -Wall -Wextra
        -ffunction-sections -fdata-sections
)

target_link_options(${BOOT_NAME} PRIVATE
        -T${BOOT_DIR}/Boot.ld
        -Wl,-Map=${BOOT_NAME}.map
        -specs=nosys.specs
        -Wl,--gc-sections
        -Wl,--print-memory-usage
)

add_custom_command(
        TARGET ${BOOT_NAME} POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${BOOT_NAME}> ${BOOT_NAME}.hex
        COMMENT "Generating ${BOOT_NAME}.hex file..."
)
