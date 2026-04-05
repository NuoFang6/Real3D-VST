# 结合 Real3D-VST 与 HeSuVi 的 Equalizer APO 配置教程

本教程介绍如何在 Equalizer APO 中使用自定义的 VST 插件（如 Real3D-VST）进行声道上混，同时避免 HeSuVi 的内部矩阵上混处理覆盖 VST 的输出结果。

## 背景原理

默认情况下，HeSuVi 会根据输入的声道数（`fakeChannelCount`）自动应用对应的上混矩阵（如 `matrix.txt` 或 `matrix5.1.txt`）。如果你在 HeSuVi 之前加载了 `Real3D-VST` 来处理声道上混，HeSuVi 的默认行为会把你的 VST 处理结果给覆盖掉。
通过引入一个自定义变量 `use_real3d`，我们可以告诉 HeSuVi 在 VST 开启时跳过它的默认上混步骤。

---

## 第一步：修改 HeSuVi 核心脚本

我们需要修改 HeSuVi 的主控脚本，使其支持条件判断。

1. 打开 Equalizer APO 配置目录下的 `HeSuVi\hesuvi.txt` 文件。路径通常为：
   `C:\Program Files\EqualizerAPO\config\HeSuVi\hesuvi.txt`
2. 找到以下这段代码（约在第 20 行附近）：
    ```text
    Stage: pre-mix
    Include: incc.txt
    If: fakeChannelCount == 2
      Include: matrix.txt
    ElseIf: fakeChannelCount == 6
      Include: matrix5.1.txt
    EndIf:
    Stage: post-mix
    ```
3. 将上方代码修改为（加入 `If: use_real3d != 1` 的判断）：
    ```text
    Stage: pre-mix
    Include: incc.txt
    If: use_real3d != 1
      If: fakeChannelCount == 2
        Include: matrix.txt
      ElseIf: fakeChannelCount == 6
        Include: matrix5.1.txt
      EndIf:
    EndIf:
    Stage: post-mix
    ```
4. 保存 `hesuvi.txt`。

---

## 第二步：修改主配置文件

接下来，我们在主配置文件中声明这个变量，并加载 VST 插件。

1. 打开主配置文件 `config.txt`。路径通常为：
   `C:\Program Files\EqualizerAPO\config\config.txt`
2. 按照以下顺序结构排列你的配置：

    ```text
    # 1. 声明开启 Real3D-VST 模式
    Eval: use_real3d=1

    # 2. 加载你的 VST 插件 (路径和参数根据实际情况为准)
    VSTPlugin: Library C:\9919\code\Real3D-VST\build\Real3D-VST_artefacts\Release\VST\Real3D-VST.dll ChunkData "..."

    # 3. 加载 HeSuVi
    Include: HeSuVi\hesuvi.txt

    # 4. (可选) 重置变量，防止影响后续的其他配置
    Eval: use_real3d=0
    ```

3. 保存 `config.txt`。

---

## 如何使用与切换

在使用 **Configuration Editor (Equalizer APO 的配置编辑器)** 时，你可以非常方便地在 "Real3D-VST 模式" 和 "HeSuVi 原生模式" 之间切换：

### 开启 Real3D-VST (自定义上混)

确保前面添加的 `Eval: use_real3d=1` 和 `VSTPlugin: Library ...` 前面的**电源按钮（开关）是点亮的（开启状态）**。
此时，声音会先由 Real3D-VST 上混出环绕声，然后无损进入 HeSuVi 的虚拟环绕（HRIR）卷积模块。

### 恢复使用 HeSuVi 原生上混

如果你想听 HeSuVi 自带的立体声上混效果，只需在 Configuration Editor 中：

1. 将 `Eval: use_real3d=1` 这一项关闭（电源按钮变灰，或是前面加 `#` 注释掉）。
2. 将 `VSTPlugin: Library ...` 这一项也关闭。
   此时 `use_real3d` 变量未生效，HeSuVi 脚本会自动恢复调用它自带的 `matrix.txt` 或 `matrix5.1.txt`。
