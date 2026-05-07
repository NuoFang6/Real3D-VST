cmake_minimum_required(VERSION 3.22)
project(Real3D-VST VERSION 1.0.0)

# 告诉 CMake 你的 JUCE 文件夹在哪里
add_subdirectory("C:/JUCE" juce_build)

# 显式使用 JUCE 推荐的函数设置 VST2 SDK 路径 (指向新拉取的符合标准的 SDK)
juce_set_vst2_sdk_path("${CMAKE_CURRENT_SOURCE_DIR}/VST_SDK_2.4")

# 定义 VST 插件项目
juce_add_plugin(Real3D-VST
    COMPANY_NAME "Real3D"
    PLUGIN_MANUFACTURER_CODE "R3DV"
    PLUGIN_CODE "R3DV"
    FORMATS VST # JUCE 使用 VST 而不是 VST2 来标识 VST2 格式
    COPY_PLUGIN_AFTER_BUILD FALSE
    PRODUCT_NAME "Real3D-VST"
)

# 添加你的源代码文件 (等下我们会创建这些文件)
target_sources(Real3D-VST PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/dsp/channelmaps.cpp
    Source/dsp/freesurround_decoder.cpp
    kissfft/kiss_fft.c
    kissfft/kiss_fftr.c
)

# 链接 JUCE 模块
target_link_libraries(Real3D-VST PRIVATE
    juce::juce_audio_utils
    PUBLIC
    juce::juce_dsp
)

target_include_directories(Real3D-VST PRIVATE
    kissfft
)

# 设置 C++ 标准和编码以解决 C4819 警告
target_compile_features(Real3D-VST PRIVATE cxx_std_17)
if(MSVC)
    target_compile_options(Real3D-VST PUBLIC /utf-8)
endif()

# 解决 VST3 自动化冲突错误 (通过禁用 VST2 处理逻辑)
target_compile_definitions(Real3D-VST PRIVATE
    JUCE_VST3_CAN_REPLACE_VST2=0
    kiss_fft_scalar=double
)
