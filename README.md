# Townfall 4:3 Mod

Fixes for playing *Silent Hill: Townfall* (Steam, UE 5.6.1) on a display that
is already 4:3, such as 2048x1536 or 1920x1440. It does not force 4:3 onto a
wide screen.

This is a modified fork of **[Townfall Super Ultrawide](https://github.com/Infinest/Silent_Hill_Townfall_Ultrawide)
by [Infinest](https://github.com/Infinest)** ([Nexus Mods](https://www.nexusmods.com/silenthilltownfall/mods/29)).
The proxy DLL design, export-stub generator, logger, detour and pattern-scan
helpers and the original reverse engineering of the game's camera code are
Infinest's work. See [Credits](#credits) and [License](#license) below.

## What it does

- **Full 16:9 width, extra view above and below.** On its own the game keeps
  the 16:9 vertical view at 4:3 and cuts off the sides. The mod runs the
  engine's projection a second time for a 16:9 rect of the same height and
  zooms the real projection out uniformly until the horizontal FOV matches,
  so a 4:3 player sees everything a 16:9 player sees, plus more top and
  bottom (Vert+). First-person hands and weapons zoom out with the world.
- **Gameplay is never letterboxed.** If the gameplay camera has an
  aspect-ratio constraint (for example from the in-game Letterbox option),
  the projection is built on a private copy of the view info with
  `bConstrainAspectRatio` cleared, so gameplay fills the 4:3 screen with the
  authored vertical FOV.
- **Cutscenes are left as the game renders them** (letterboxed to the
  authored ~2.35:1). Optionally they can fill the screen instead, keeping the
  cinematic vertical framing and cropping the sides.
- With `[Log] Enabled=1` the log records, for the first gameplay and the
  first cutscene camera, the vanilla constraint, `bConstrainAspectRatio`,
  aspect ratio, FOV and the final view rect, so you can see what the game
  does on your display.

## How it works

One hook on `FMinimalViewInfo::CalculateProjectionMatrixGivenView`
(RVA `0x13EEC44` in the 2026-10-08 build), located at runtime by pattern
scan. The game exe on disk is never modified; if an update moves the code the
hook refuses to install and the game runs unmodified.

Offsets come from the shipped `Townfall-Win64-Shipping.pdb`:

| Struct | Offset | Field |
|---|---|---|
| `FMinimalViewInfo` (0x8C0) | `+0x30` | `FOV` |
| | `+0x5C` | `AspectRatio` |
| | `+0x68` bit 0 | `bConstrainAspectRatio` |
| `FSceneViewProjectionData` | `+0xA0` | `ProjectionMatrix` (double 4x4) |
| | `+0x120` | `ViewRect` |
| | `+0x148` | `ConstrainedViewRect` |

## Install

1. Copy `dist\winmm.dll` into `Townfall\Townfall\Binaries\Win64\`.
2. Start the game normally.

The DLL is a proxy for the system `winmm.dll` (all calls are forwarded to the
real library, loaded by full system path), so it coexists with OptiScaler or
ReShade installed as `dxgi.dll`. `build.bat <target>` builds it under another
system DLL name if needed (see the target table in the upstream README).

Uninstall: delete `winmm.dll`, `Townfall43.ini` and `Townfall43.log`.
To disable temporarily, rename `winmm.dll` to `winmm.dll.off`, or set
`[Camera] Enabled=0`.

## Config

`Townfall43.ini` next to the DLL (generated on first launch; delete it to
reset to defaults). Changes take effect on the next game start.

```ini
[Camera]
; 0 - the mod does nothing (no hooks installed), 1 - on
Enabled=1
; Full 16:9 width plus extra view above/below on narrower displays.
KeepWidescreenWidth=1
; Removes letterboxing from normal gameplay.
NoGameplayLetterbox=1
; Cutscenes fill the screen instead of the ~2.35:1 letterbox (sides cropped).
FillCutscenes=0

[Log]
; Writes Townfall43.log next to the DLL.
Enabled=0
```

## Build

Needs MSVC (VS 2022 Build Tools by default; set `VS_ROOT` for another
install) and Python 3.

```
build.bat            -> dist\winmm.dll
build.bat dsound     -> dist\dsound.dll
```

## Tools

- `tools\pdbtool.exe <pdb> layout|func|public|dpublic|vfuncs <name>` - struct
  layouts, function RVAs and vtable slots from the shipped PDB (DIA SDK; set
  `DIA_DLL` to `msdia140.dll`). Build with `tools\build_pdbtool.bat`.
- `tools\pescan.py <exe> info|bytes|qword|find ...` - check hook signatures
  against a game build.
- The remaining tools are from upstream and have the author's paths
  hard-coded.

## Credits

- **[Infinest](https://github.com/Infinest)**, author of the original
  [Townfall Super Ultrawide](https://github.com/Infinest/Silent_Hill_Townfall_Ultrawide)
  mod this fork is based on: the system-DLL proxy (`src/proxy.cpp`,
  `src/proxy.h`, `tools/gen_proxy.py`), the logger (`src/log.cpp`), the detour
  and pattern-scan helpers (`src/detour.cpp`, `src/detour.h`), the identification
  of the camera projection function this mod hooks, and the reversing and test
  tools in `tools/` that are not listed below. The full upstream commit history
  is preserved in this repository.
- **[it-was-katsumata](https://github.com/it-was-katsumata)**, this fork: the
  native 4:3 camera hook (`src/hooks.cpp`), config handling (`src/config.*`),
  `tools/pdbtool.cpp` (based on upstream's `tools/dia_layout.cpp`) and
  `tools/pescan.py`.

## Modifications

As required by section 5 of the GPL-3.0, this is a modified version of
Infinest's work. Changes made on 2026-10-17:

- Rewrote the camera hook for displays that are already 4:3: keeps the 16:9
  horizontal FOV with extra view above and below, removes gameplay
  letterboxing, and adds optional full-screen cutscenes. The upstream 32:9
  unlock and its second view-extents hook were removed.
- Removed the HUD constraint (`src/uiconstraint.cpp`).
- Builds as a `winmm.dll` proxy by default instead of `dxgi.dll`.
- Renamed the config and log files to `Townfall43.ini` / `Townfall43.log`
  and moved config reading into `src/config.*`.
- Made `build.bat` use the VS 2022 Build Tools by default.
- Added `tools/pdbtool.cpp`, `tools/build_pdbtool.bat` and `tools/pescan.py`.
- Rewrote this README.

## License

Licensed under the GNU General Public License v3.0, the same license as the
upstream project. See [`LICENSE`](LICENSE) for the full text.

Original work by Infinest. Modifications copyright (C) 2026 it-was-katsumata.
This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. If you distribute a built `winmm.dll`, the GPL
requires you to make the corresponding source available as well.

SILENT HILL is a trademark of Konami Digital Entertainment. This project is
not affiliated with or endorsed by Konami or the game's developers.
