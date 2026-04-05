# Configuration Tutorial: Combining Real3D-VST with HeSuVi in Equalizer APO

This tutorial explains how to use a custom VST plugin (like Real3D-VST) for channel upmixing in Equalizer APO while preventing HeSuVi's internal matrix upmixing from overwriting the VST's output.

## Background Principle

By default, HeSuVi automatically applies corresponding upmixing matrices (such as `matrix.txt` or `matrix5.1.txt`) based on the input channel count (`fakeChannelCount`). If you load `Real3D-VST` before HeSuVi to process the channel upmix, HeSuVi's default behavior will overwrite your VST processing results.
By introducing a custom variable `use_real3d`, we can tell HeSuVi to skip its default upmixing steps when the VST is enabled.

---

## Step 1: Modify HeSuVi Core Script

We need to modify HeSuVi's main control script to support conditional logic.

1. Open the `HeSuVi\hesuvi.txt` file in the Equalizer APO configuration directory. The path is typically:
   `C:\Program Files\EqualizerAPO\config\HeSuVi\hesuvi.txt`
2. Find the following code snippet (around line 20):
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
3. Change the above code to include an `If: use_real3d != 1` check:
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
4. Save `hesuvi.txt`.

---

## Step 2: Modify Main Configuration File

Next, we declare this variable in the main configuration file and load the VST plugin.

1. Open the main configuration file `config.txt`. The path is typically:
   `C:\Program Files\EqualizerAPO\config\config.txt`
2. Arrange your configuration in the following order:

    ```text
    # 1. Declare to enable Real3D-VST mode
    Eval: use_real3d=1

    # 2. Load your VST plugin (Path and parameters should be based on your actual setup)
    VSTPlugin: Library C:\9919\code\Real3D-VST\build\Real3D-VST_artefacts\Release\VST\Real3D-VST.dll ChunkData "..."

    # 3. Load HeSuVi
    Include: HeSuVi\hesuvi.txt

    # 4. (Optional) Reset the variable to prevent affecting other subsequent configurations
    Eval: use_real3d=0
    ```

3. Save `config.txt`.

---

## How to Use and Switch

When using the **Configuration Editor (Equalizer APO's configuration editor)**, you can easily switch between "Real3D-VST Mode" and "Native HeSuVi Mode":

### Enable Real3D-VST (Custom Upmix)

Ensure that the **power buttons (switches) next to `Eval: use_real3d=1` and `VSTPlugin: Library ...` are lit (enabled state)**.
Now, the audio will first be upmixed to surround sound by Real3D-VST, and then losslessly enter HeSuVi's virtual surround (HRIR) convolution module.

### Revert to Native HeSuVi Upmix

If you want to listen to the stereo upmix effect built into HeSuVi, simply do the following in the Configuration Editor:

1. Turn off the `Eval: use_real3d=1` item (the power button turns gray, or prefix it with `#` to comment it out).
2. Turn off the `VSTPlugin: Library ...` item as well.
   At this point, the `use_real3d` variable is inactive, and the HeSuVi script will automatically revert to calling its built-in `matrix.txt` or `matrix5.1.txt`.
