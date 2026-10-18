// hooks.cpp - Camera hook for native 4:3 (and other narrow) displays.
//
// Originally part of Townfall Super Ultrawide by Infinest. Rewritten
// 2026-10-17 by it-was-katsumata for the Townfall 4:3 fork. GPL-3.0, see
// LICENSE.
//
// FMinimalViewInfo::CalculateProjectionMatrixGivenView (RVA 0x13EEC44 in the
// 2026-10-08 Steam build, located by pattern scan):
//     rcx = const FMinimalViewInfo&, dl = EAspectRatioAxisConstraint,
//     r8  = FViewport*, r9 = FSceneViewProjectionData&
// It computes ViewExtents = Viewport->CalculateViewExtents(AspectRatio,
// ViewRect) and calls CalculateProjectionMatrixGivenViewRectangle, which
// writes ProjectionMatrix and (for bConstrainAspectRatio cameras) the
// ConstrainedViewRect - the letterboxed area the scene is rendered into.
//
// Layout (from the shipped PDB):
//   FMinimalViewInfo (0x8C0): +0x30 FOV, +0x5C AspectRatio,
//                             +0x68 bit0 bConstrainAspectRatio
//   FSceneViewProjectionData: +0xA0 ProjectionMatrix (double[4][4]),
//                             +0x120 ViewRect, +0x148 ConstrainedViewRect
//
// Cameras are classified like the original ultrawide mod: constraint 1
// (MaintainXFOV) with an authored AspectRatio is a cutscene camera, anything
// else is a gameplay camera.
//
//  - Gameplay cameras with bConstrainAspectRatio set (letterboxed) are run
//    on a private copy with the flag cleared, so gameplay fills the screen.
//  - On displays narrower than 16:9 (KeepWidescreenWidth=1), the gameplay
//    projection is zoomed out uniformly so the horizontal FOV equals what the
//    engine builds for a 16:9 rect of the same height: the full 16:9 width,
//    plus extra view above and below (Vert+). First-person primitives
//    (FFirstPersonParameters::FOVCorrectionFactor is relative to the main
//    projection) zoom out with the world.
//  - Cutscenes are left alone by default (the game letterboxes them to the
//    authored ~2.35:1). With [Camera] Cutscenes=1 the letterboxed frame is
//    expanded to the full screen height, keeping the authored vertical FOV
//    and cropping the sides.

#include <windows.h>

#include <cstdint>
#include <cstring>

#include "config.h"
#include "detour.h"
#include "log.h"

namespace {

constexpr size_t kViewInfoSize = 0x8C0;
constexpr size_t kFovOff = 0x30;
constexpr size_t kAspectRatioOff = 0x5C;
constexpr size_t kFlagsOff = 0x68;
constexpr size_t kProjMatrixOff = 0xA0;
constexpr size_t kViewRectOff = 0x120;
constexpr size_t kConstrainedRectOff = 0x148;
constexpr size_t kProjDataSize = 0x160;  // sizeof(FSceneViewProjectionData)

struct IntRect {
    int minX, minY, maxX, maxY;
};

using CalcProj_t = void (*)(const void* fmi, unsigned char constraint, void* viewport,
                            unsigned char* projData);
CalcProj_t g_origCalcProj = nullptr;

bool gUnletterboxGameplay = true;
bool gFillCutscenes = false;
bool gKeepWidth = true;
const double gKeepWidthAspect = 16.0 / 9.0;

bool gLoggedKeepWidth = false;
bool gLoggedGameplay = false;
bool gLoggedCutscene = false;

// One line per camera kind with the vanilla parameters and the final rect,
// so the log shows what the game does on this display.
void LogFirst(bool& flag, const char* what, const void* fmi, unsigned char constraint,
              unsigned char* projData, const char* action) {
    if (flag) return;
    flag = true;
    const char* v = static_cast<const char*>(fmi);
    const IntRect full = *reinterpret_cast<const IntRect*>(projData + kViewRectOff);
    const IntRect out = *reinterpret_cast<const IntRect*>(projData + kConstrainedRectOff);
    LogLine("camera: first %s view %dx%d constraint=%d bConstrain=%d aspect=%.4f fov=%.2f"
            " -> rect (%d,%d)-(%d,%d) [%s]",
            what, full.maxX - full.minX, full.maxY - full.minY, constraint,
            (*reinterpret_cast<const unsigned*>(v + kFlagsOff) & 1u) ? 1 : 0,
            static_cast<double>(*reinterpret_cast<const float*>(v + kAspectRatioOff)),
            static_cast<double>(*reinterpret_cast<const float*>(v + kFovOff)), out.minX, out.minY,
            out.maxX, out.maxY, action);
}

void HookCalcProj(const void* fmi, unsigned char constraint, void* viewport,
                  unsigned char* projData) {
    const char* v = static_cast<const char*>(fmi);
    const float authoredAspect = *reinterpret_cast<const float*>(v + kAspectRatioOff);
    const bool cutscene = constraint == 1 && authoredAspect > 0.f;
    const bool constrained = (*reinterpret_cast<const unsigned*>(v + kFlagsOff) & 1u) != 0;

    if (!cutscene) {
        alignas(64) unsigned char copy[kViewInfoSize];
        const void* info = fmi;
        if (gUnletterboxGameplay && constrained) {
            memcpy(copy, fmi, sizeof(copy));
            *reinterpret_cast<unsigned*>(copy + kFlagsOff) &= ~1u;
            info = copy;
        }
        // Untouched input, for the 16:9 reference pass below.
        alignas(16) unsigned char ref[kProjDataSize];
        memcpy(ref, projData, sizeof(ref));

        g_origCalcProj(info, constraint, viewport, projData);

        const IntRect full = *reinterpret_cast<const IntRect*>(projData + kViewRectOff);
        const int fw = full.maxX - full.minX;
        const int fh = full.maxY - full.minY;
        double* m = reinterpret_cast<double*>(projData + kProjMatrixOff);  // row-major M[r][c]
        if (gKeepWidth && fw > 0 && fh > 0 && m[0] != 0.0 &&
            static_cast<double>(fw) / fh < gKeepWidthAspect - 1e-3) {
            // Let the engine build the projection for a 16:9 rect of the
            // same height; its M00 is the 16:9 horizontal FOV. Zoom the real
            // projection out uniformly (X and Y columns) until M00 matches:
            // same width as 16:9, extra view above and below.
            IntRect& refRect = *reinterpret_cast<IntRect*>(ref + kViewRectOff);
            refRect.maxX = refRect.minX + static_cast<int>(fh * gKeepWidthAspect + 0.5);
            *reinterpret_cast<IntRect*>(ref + kConstrainedRectOff) = refRect;
            g_origCalcProj(info, constraint, viewport, ref);
            const double ref00 = reinterpret_cast<const double*>(ref + kProjMatrixOff)[0];
            const double k = ref00 / m[0];
            if (k > 0.0 && k < 1.0) {
                for (int r = 0; r < 4; ++r) {
                    m[r * 4 + 0] *= k;
                    m[r * 4 + 1] *= k;
                }
            }
            if (!gLoggedKeepWidth) {
                gLoggedKeepWidth = true;
                LogLine("camera: keep 16:9 width at %dx%d, zoom factor %.4f%s", fw, fh, k,
                        (k > 0.0 && k < 1.0) ? "" : " (not applied)");
            }
        }
        LogFirst(gLoggedGameplay, "gameplay", fmi, constraint, projData,
                 info != fmi ? "letterbox removed" : "vanilla");
        return;
    }

    g_origCalcProj(fmi, constraint, viewport, projData);
    if (!gFillCutscenes) {
        LogFirst(gLoggedCutscene, "cutscene", fmi, constraint, projData, "vanilla");
        return;
    }

    // Fill: render into the full view rect, keep M11 (the authored vertical
    // FOV of the letterboxed frame) and set M00 = M11 / screenAspect by
    // scaling the whole X column, which keeps any off-center terms consistent.
    const IntRect full = *reinterpret_cast<const IntRect*>(projData + kViewRectOff);
    IntRect& out = *reinterpret_cast<IntRect*>(projData + kConstrainedRectOff);
    double* m = reinterpret_cast<double*>(projData + kProjMatrixOff);  // row-major M[r][c]
    const int fw = full.maxX - full.minX;
    const int fh = full.maxY - full.minY;
    if (fw <= 0 || fh <= 0 || m[0] == 0.0) return;
    const double k = (m[5] * fh / fw) / m[0];
    m[0] *= k;
    m[4] *= k;
    m[8] *= k;
    m[12] *= k;
    out = full;
    LogFirst(gLoggedCutscene, "cutscene", fmi, constraint, projData, "filled");
}

}  // namespace

bool InstallCameraHooks(HMODULE game) {
    gUnletterboxGameplay = ConfigInt(L"Camera", L"NoGameplayLetterbox", 1) != 0;
    gFillCutscenes = ConfigInt(L"Camera", L"FillCutscenes", 0) != 0;
    gKeepWidth = ConfigInt(L"Camera", L"KeepWidescreenWidth", 1) != 0;
    LogLine("camera: KeepWidescreenWidth=%d NoGameplayLetterbox=%d FillCutscenes=%d",
            gKeepWidth ? 1 : 0, gUnletterboxGameplay ? 1 : 0, gFillCutscenes ? 1 : 0);

    // mov [rsp+8],rbx; mov [rsp+10h],rsi; push rdi; sub rsp,30h (15 bytes,
    // rsp-relative, trampoline-safe), then the AsymmetricCropFraction reads.
    static const unsigned char kSigCalcProj[] = {
        0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x74, 0x24, 0x10, 0x57, 0x48, 0x83, 0xEC, 0x30,
        0xF3, 0x0F, 0x10, 0x91, 0x74, 0x08, 0x00, 0x00, 0x49, 0x8B, 0xF9, 0xF3, 0x0F, 0x58, 0x91,
        0x70, 0x08, 0x00, 0x00};
    return InstallHook(game, "CalcProj", kSigCalcProj, sizeof(kSigCalcProj), 15,
                       reinterpret_cast<void*>(&HookCalcProj),
                       reinterpret_cast<void**>(&g_origCalcProj));
}
