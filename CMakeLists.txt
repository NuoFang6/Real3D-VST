cmake_minimum_required(VERSION 3.22)
project(Real3D-VST VERSION 1.0.0)

# ================= 1. 全局依赖与配置 =================

# JUCE SDK
add_subdirectory("C:/JUCE" juce_build)

# JUCE 设置 VST2 SDK 路径
juce_set_vst2_sdk_path("${CMAKE_CURRENT_SOURCE_DIR}/VST_SDK_2.4")

# 检查 LTO 支持 (全局检查一次即可)
include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported)
if(ipo_supported)
    message(STATUS "LLVM LTO is supported. Enabling for maximum cross-file optimization.")
endif()


# ================= 2. 定义构建工厂函数 =================

# 定义一个函数，用于利用不同的架构参数生成不同的 VST 插件
# TARGET_NAME: 生成的 CMake 目标名称和最终的 DLL 名称
# ARCH_FLAG: 架构优化指令 (例如 -march=native 或 -march=x86-64-v3)
# PLUG_CODE: 4字符的 VST 唯一标识符
function(create_real3d_variant TARGET_NAME ARCH_FLAG PLUG_CODE)

    # 定义 VST 插件项目
    juce_add_plugin(${TARGET_NAME}
        COMPANY_NAME "Real3D"
        PLUGIN_MANUFACTURER_CODE "R3DV"
        PLUGIN_CODE "${PLUG_CODE}"
        FORMATS VST
        COPY_PLUGIN_AFTER_BUILD FALSE
        PRODUCT_NAME "${TARGET_NAME}" # 让生成的 DLL 带有后缀区分
    )

    # 源代码文件
    target_sources(${TARGET_NAME} PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/dsp/channelmaps.cpp
        Source/dsp/freesurround_decoder.cpp
        kissfft/kiss_fft.c
        kissfft/kiss_fftr.c
    )

    # 链接 JUCE 模块和 OpenMP 运行时
    target_link_libraries(${TARGET_NAME} PRIVATE
        juce::juce_audio_utils
        PUBLIC
        juce::juce_dsp
    )

    target_include_directories(${TARGET_NAME} PRIVATE
        kissfft
    )

    # 开启 LTO
    if(ipo_supported)
        set_property(TARGET ${TARGET_NAME} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif()

    # 设置 C++ 标准
    target_compile_features(${TARGET_NAME} PRIVATE cxx_std_17)

    # Clang 独有的激进优化标志
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(${TARGET_NAME} PRIVATE
            -O3
            ${ARCH_FLAG}                # 动态传入架构指令 (核心修改点)

            -funroll-loops
            -fslp-vectorize
            -fvectorize

            -fomit-frame-pointer

        )

        target_link_options(${TARGET_NAME} PRIVATE
            -Wl,--gc-sections
            -Wl,-O3
        )

        set_target_properties(${TARGET_NAME} PROPERTIES
            CXX_VISIBILITY_PRESET hidden
            VISIBILITY_INLINES_HIDDEN ON
        )
    endif()

    # VST 配置与 KissFFT 宏定义
    target_compile_definitions(${TARGET_NAME} PRIVATE
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISABLE_ASSERTIONS=1
        JUCE_LOG_ASSERTIONS=0
        kiss_fft_scalar=double
    )
endfunction()


# ================= 3. 实例化两个编译目标 =================

# 目标 1：针对 x86_64-v3 优化 (兼容支持 AVX2/FMA 的 CPU，约2014年以后的机器)
# 使用插件代码 R3D3 以防在宿主中冲突
create_real3d_variant(Real3D-VST-x64v3 "-march=x86-64-v3" "R3D3")

# 目标 2：针对 Native (本机) 极致优化 (可能会包含 AVX-512 等本机特有指令)
# 使用插件代码 R3DN
create_real3d_variant(Real3D-VST-Native "-march=native" "R3DN")
