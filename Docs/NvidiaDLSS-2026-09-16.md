# NVIDIA DLSS 4.5 (Super Resolution + DLAA)

Integrated 2026-09-16. Only Super Resolution and DLAA are used. Frame Generation, Reflex and Ray Reconstruction are not installed.

## Installing on a machine

The plugin is **not in git**. It is about 1.2 GB of NVIDIA binaries under NVIDIA's license, and `/Plugins/NVIDIA/` is in `.gitignore`.

1. Download the NVIDIA DLSS 4.5 plugin for UE 5.7 (v8.8.0-NGX310.9.1) from NVIDIA's developer site. You accept NVIDIA's license yourself when you download it.
2. From the package, copy only these two plugins into the project:
   - `Plugins/DLSS` → `Plugins/NVIDIA/DLSS`
   - `Plugins/StreamlineNGXCommon` → `Plugins/NVIDIA/StreamlineNGXCommon`
3. Put the rest of the package outside the project. On the main workstation it lives at `F:\Vendor\NVIDIA\2026.09.15_UE5.7_DLSS4.5Plugin_v8.8.0\`. Do not unzip the whole package into `Plugins/`: Unreal enables every project plugin it finds, including DLSSG, Reflex and the Streamline plugins.
4. Close the editor, then build. The NVIDIA modules compile from source.

A machine without the plugin builds and runs normally and uses TSR.

## How it turns on

`Config/DefaultEngine.ini` sets `r.NGX.DLSS.Enable=1` in `[ConsoleVariables]`. The plugin's default is 0. DLSS runs only when all three of these are true (`FDLSSUpscaler::IsDLSSActive`):

- NGX reports DLSS-SR is supported (an RTX GPU on a recent driver, with DX12 or Vulkan),
- `r.TemporalAA.Upscaler` is on,
- `r.NGX.DLSS.Enable` is non-zero.

In every other case the renderer uses `r.AntiAliasingMethod=4` (TSR). There's no settings menu yet.

- **Below native resolution** (screen percentage under 100), DLSS upscales.
- **At native resolution**, it runs as DLAA. This is the current default.

## Verification (2026-09-16)

- Headless run (`Validate-Unreal.ps1`, null RHI): the build succeeded and 683/683 tests passed. Only DLSS and StreamlineNGXCommon mounted. NGX correctly reported `DLSS-SR=0` and stayed inactive.
- DX12 editor session running PIE on `L_Aurelion_M12` (`Saved/Validation/Aurelion/DLSSVerify-20260916-161345-c12ec8c0`):
  - The log reported `NVIDIA NGX DLSS supported DLSS-SR=1`.
  - During play it logged `Creating NGX DLSS Feature SrcRect=1696x862 DestRect=1696x862 ScaleX=1.0`, which is DLAA.
  - The feature was released cleanly when PIE ended, and there were no errors.
  - On the first run, NGX reported "failed to load from cache". That's expected: the driver's model cache was empty, so it loaded the DLSS DLLs bundled with the plugin.

## Before shipping

- `NVIDIANGXApplicationId` under `[/Script/DLSS.DLSSSettings]` is 0, so NGX uses the Project ID. A shipped build should use the App ID NVIDIA assigns.
- The plugin shows on-screen DLSS debug messages and allows OTA model updates by default (`bShowDLSSSDebugOnScreenMessages`, `bAllowOTAUpdate`). Review both before release.
- A packaged build needs the plugin present on the build machine, because it isn't in the repository.
