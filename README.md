# ParaEQ

A 6-band parametric EQ plugin (VST3, plus AU on Mac) built with JUCE.

**Bands:** Low Cut · Low Shelf · Peak 1 · Peak 2 · High Shelf · High Cut
**Per band:** on/off, frequency (20 Hz–20 kHz), gain (±24 dB, not on cuts), Q (0.1–10)
**Display:** live response curve. Drag a node to move freq/gain, scroll over it to change Q, double-click to toggle the band.

---

## Option A: Build without installing anything (GitHub)

1. Create a free GitHub account and a new repository.
2. Upload this whole folder (including the hidden `.github` folder).
3. Open the **Actions** tab. The build runs automatically (about 5–10 min).
4. When it's green, click the run and download **ParaEQ-Windows** or **ParaEQ-macOS** at the bottom.

## Option B: Build on your computer

**Windows:** install [Visual Studio 2022 Community](https://visualstudio.microsoft.com/) (tick "Desktop development with C++"), [CMake](https://cmake.org/download/) and [Git](https://git-scm.com/).
**Mac:** install Xcode from the App Store, then run `xcode-select --install`, and install CMake (`brew install cmake`).

Then in a terminal inside this folder:

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The first build downloads JUCE automatically. The plugin ends up in `build/ParaEQ_artefacts/Release/VST3/`.

---

## Installing in Ableton

- **Windows:** copy `ParaEQ.vst3` to `C:\Program Files\Common Files\VST3\`
- **Mac:** it's copied automatically when you build locally. From GitHub, copy `ParaEQ.vst3` to `~/Library/Audio/Plug-Ins/VST3/` (and `ParaEQ.component` to `~/Library/Audio/Plug-Ins/Components/`). If macOS blocks it, run:
  `xattr -cr ~/Library/Audio/Plug-Ins/VST3/ParaEQ.vst3`

In Ableton: **Settings → Plug-ins**, turn on **Use VST3 Plug-in System Folders**, click **Rescan**. ParaEQ appears under Plug-ins → ParaEQ.

---

## Where to change things

- `Source/PluginProcessor.h` – the `bandInfos` table: add/remove bands, change types and defaults.
- `Source/PluginProcessor.cpp` – parameter ranges and the filter math (`makeBandCoefficients`).
- `Source/PluginEditor.cpp` – the look: colours, layout, curve drawing, mouse behaviour.
