# Real3D-Surround-Upmixer

[中文](#中文) | [English](#english)

---

## 中文

Real3D-VST 是一个专为多通道环绕声上混（Upmixer）设计的音频插件。它基于经典的 FreeSurround 解码算法（foo_dsp_fsurround_source-0.9.0），旨在将传统的立体声（2.0）音频实时扩展为更加沉浸的多声道（如 5.1 或 7.1）环境。本项目包含跨平台的 VST 插件版本（兼容 Equalizer APO，可与 HeSuVi 搭配使用）以及专为 foobar2000 设计的组件。

[![Real3D-VST Screenshot](screenshot.png)](screenshot.png)
**你可能需要对 HeSuVi 的配置进行额外处理，内置的声道识别与上混可能会冲突**
参考详细教程：[结合 Real3D-VST 与 HeSuVi 的配置教程](Real3D-VST_HeSuVi_Tutorial.md)

### 编译指南

#### VST 插件 (CMake)

1. **环境要求**:
    - **JUCE 框架**: 默认路径需位于 `C:/JUCE`（若不同请在 [CMakeLists.txt](CMakeLists.txt) 中配置）。
    - **编译工具**: Ninja llvm Clang

2. **编译步骤**:
    ```powershell
    cmake -B build -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Release
    cmake --build build
    ```
3. **输出路径**: 编译成功的二进制文件将位于 `build/Real3D-VST-*_artefacts/Release/VST` 目录下。

### 重要说明 (Caveats)

- **HRTF 角度匹配**: FreeSurround 底层算法生成的虚拟 7.1 声场扬声器默认角度（前置 ±27°、侧置 ±95°、后置 ±142°）与 HeSuVi 等常见虚拟 7.1 HRTF 标准配置（前置 ±30°、侧置 ±90°、后置 ±135°）存在微小差异。
- **兼容性测试**: 目前插件仅在 **8声道（7.1环绕声）** Windows 环境下进行了完整测试，其他声道布局（如 5.1）的性能表现可能未经充分验证。
- **16.1 输出顺序（固定）**: 当 `Output Configuration` 选择 `16.1 Surround` 时，插件按 FreeSurround 原生顺序固定输出，不做 Windows rank 重排，且 LFE 固定最后。顺序如下：`FL, FCL, FC, FCR, FR, SFL, SFR, SCL, SCR, SBL, SBR, BL, BCL, BC, BCR, BR, LFE`（对应 `out0..out16`）。
- **第三方库说明**: 本项目引用的 `VST_SDK_2.4` 源自第三方存储库。**如有侵权行为，请告知**
- **代码状态**: 本项目目前的实现包含较多原始或未经深度重构的代码（Experimental/Vibe-driven code），可能存在待优化的部分，欢迎贡献改进建议。
