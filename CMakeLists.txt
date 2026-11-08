cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary

# 得到所有子文件夹目录的函数：
add_definitions(-DLV_LVGL_H_INCLUDE_SIMPLE)

function(collect_subdirectories OUTPUT_VAR ROOT_DIR)
    set(SUBDIRS "${ROOT_DIR}")  # 优先把根目录放进去
    function(_recurse dir)
        file(GLOB CHILDREN RELATIVE ${dir} ${dir}/*)
        foreach(CHILD ${CHILDREN})
            set(CHILD_PATH "${dir}/${CHILD}")
            if(IS_DIRECTORY ${CHILD_PATH})
                list(APPEND SUBDIRS ${CHILD_PATH})
                _recurse(${CHILD_PATH})
            endif()
        endforeach()
        set(SUBDIRS ${SUBDIRS} PARENT_SCOPE)
    endfunction()
    _recurse(${ROOT_DIR})
    list(REMOVE_DUPLICATES SUBDIRS)
    set(${OUTPUT_VAR} ${SUBDIRS} PARENT_SCOPE)
endfunction()

# 得到子文件夹所有.c文件的函数：
function(collect_all_c_files OUTPUT_VAR ROOT_DIR)
    file(GLOB_RECURSE C_FILES "${ROOT_DIR}/*.c")
    list(REMOVE_DUPLICATES C_FILES)
    set(${OUTPUT_VAR} ${C_FILES} PARENT_SCOPE)
endfunction()

#############  路径整备  ################
# 整备UserCode文件夹
set(USER_CODE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/UserCode)         # 设定根目录地址
collect_subdirectories(USER_CODE_SUBDIRS ${USER_CODE_DIR})      # 得到所有子文件夹
collect_all_c_files(USERCODE_SOURCE_FILES ${USER_CODE_DIR})     # 得到所有文件名

# 整备lvgl文件夹
set(LVGL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lvgl)         # 设定根目录地址
collect_subdirectories(LVGL_SUBDIRS ${LVGL_DIR})      # 得到所有子文件夹
collect_all_c_files(LVGL_SOURCE_FILES ${LVGL_DIR})     # 得到所有文件名

# 整备lvgl_app文件夹
set(LVGL_APP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lvgl_app)         # 设定根目录地址
collect_subdirectories(LVGL_APP_SUBDIRS ${LVGL_APP_DIR})      # 得到所有子文件夹
collect_all_c_files(LVGL_APP_SOURCE_FILES ${LVGL_APP_DIR})     # 得到所有文件名
# 整备FATFS文件夹
set(FATFS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/FATFS)         # 设定根目录地址
collect_subdirectories(FATFS_SUBDIRS ${FATFS_DIR})      # 得到所有子文件夹
collect_all_c_files(FATFS_SOURCE_FILES ${FATFS_DIR})     # 得到所有文件名

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME XM_Power_Kit)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME}
        ${USERCODE_SOURCE_FILES}    # UserCode 源文件
        ${LVGL_SOURCE_FILES}
        ${LVGL_APP_SOURCE_FILES}
        ${FATFS_SOURCE_FILES}
)

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
        # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
        # Add user sources here
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
        # Add user defined include paths
        ${USER_CODE_SUBDIRS}        # Usercode 头文件
        ${LVGL_SUBDIRS}
        ${LVGL_APP_SUBDIRS}
        ${FATFS_SUBDIRS}
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
        # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
        stm32cubemx

        # Add user defined libraries
)

# 链接脚本与 Map 文件按目标指定（工具链文件里已移除写死的 -T，避免和 Bootloader 的脚本叠加）
target_link_options(${CMAKE_PROJECT_NAME} PRIVATE
        -T${CMAKE_SOURCE_DIR}/STM32F407xx_FLASH.ld
        -Wl,-Map=${CMAKE_PROJECT_NAME}.map
)


# 浮点打印：全工程禁用 printf 系函数的 %f（统一走 UserCode/Utils/format.h 的定点格式化），
# 因此不再需要 -u _printf_float（该选项会把 _printf_float/_dtoa_r 整套代码链进镜像，约 4KB）。
set(CMAKE_SYSTEM_PROCESSOR cortex-m4)
set(MCU_FLAGS "-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard")

# 注意：add_compile_options() 只对"调用之后创建的 target"生效（COMPILE_OPTIONS 是目录属性，
# 在 target 创建时拷贝进 target），而本工程的 add_executable() 在文件开头，
# 因此编译/链接选项必须写进 CMAKE_C_FLAGS / CMAKE_EXE_LINKER_FLAGS 才能全局生效。
# -flto：链接期优化（跨文件内联、去重、无用段消除），实测可省约 6% FLASH。
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mfloat-abi=hard -flto=auto")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -flto=auto")
# 说明：Release 的优化等级由 cmake/gcc-arm-none-eabi.cmake 中的 CMAKE_C_FLAGS_RELEASE("-Os -g0") 决定。

# 启用 生成HEX文件 / OTA 用的裸二进制
add_custom_command(
        TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${CMAKE_PROJECT_NAME}> ${CMAKE_PROJECT_NAME}.hex
        COMMAND ${CMAKE_OBJCOPY} -O binary -j .isr_vector -j .text -j .rodata -j .ARM -j .init_array -j .fini_array -j .data
                $<TARGET_FILE:${CMAKE_PROJECT_NAME}> ${CMAKE_PROJECT_NAME}.bin
        COMMENT "Generating ${CMAKE_PROJECT_NAME}.hex / .bin file..."
)

# OTA Bootloader：独立目标，链接到内部 Flash 0x08000000（16KB），生成 XM_Power_Kit_Boot.elf/.hex
add_subdirectory(Bootloader)
