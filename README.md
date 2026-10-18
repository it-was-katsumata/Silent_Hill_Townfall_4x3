# Townfall 4:3 Mod

Fixes for playing *Silent Hill: Townfall* (Steam, UE 5.6.1) on a display that
is already 4:3, such as 2048x1536 or 1920x1440. It does not force 4:3 onto a
wide screen.

Fork of [Infinest/Silent_Hill_Townfall_Ultrawide](https://github.com/Infinest/Silent_Hill_Townfall_Ultrawide)
(super-ultrawide support). The proxy loader, logger, detour helper and
reverse-engineering notes come from that project; the camera hook was
rewritten for narrow displays, and the HUD constraint was removed (it has
nothing to do on a native 4:3 screen).

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

## License

GPL-3.0, as upstream (see `LICENSE`).
