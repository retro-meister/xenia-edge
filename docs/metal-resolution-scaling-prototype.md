# Experimental Metal resolution scaling

This branch connects the existing Metal scaled texture, render target, resolve,
and shader paths to `draw_resolution_scale_x` / `draw_resolution_scale_y`.
The default stays 1x. Requested scales above 2x per axis are clamped to 2x.
The Quick Settings dialog on macOS offers Native and 2x; restart the game after
changing the scale.

This is a prototype for user testing, not a compatibility claim. Rendering
correctness, GPU memory use, and performance at 2x still need gameplay testing.
The separate SVOD macOS metadata fix is included in this branch.

## Compare 1x and 2x

Use the same executable, save, camera position, and window size for both runs.
Set both GPU scale settings to 1 for the baseline and 2 for the experiment.
Leave `internal_display_resolution` unchanged: it selects the display mode
reported to the guest, rather than the host rendering multiplier.

The log should contain:

```
Metal prototype: internal rendering resolution scale 2x2
```

The running game's title bar should also show `2x2`. These confirm the scale was
selected; compare fine geometry and edge detail to verify the rendered result.
Check shadows, bloom, menus, cutscenes, and gameplay, as well as frame rate.
A 2x scale in both dimensions produces four times as many pixels for scaled
surfaces. It does not upgrade source texture assets or prerecorded videos.

## Build

Use the upstream macOS build prerequisites and commands in `docs/building.md`.
This branch also has a macOS-only GitHub Actions workflow that produces the
usual `xenia_edge_macos` DMG artifact. Its Release build disables LTO to shorten
prototype build times. The normal multi-platform workflow is skipped only for
this prototype branch.

Gameplay validation is intentionally left to the user. The provided launchers
use an isolated copy of Xenia's user data and pass explicit 1x or 2x overrides.

## Cutscene buffer-growth regression

A user reproduced a 2x-only crash entering an NGII cutscene; the same prototype
at 1x passed. The captured GPU-thread stack stopped in `MTLReportFailure`, then
`blitCommandEncoderCommon:`, `EnsureScaledResolveBufferRange`,
`TextureCache::LoadTexturesData`, and `PrepareDrawTextures`.

Growing an overlapping scaled resolve buffer now closes the submission's active
render encoder before starting the preservation blit. `PrepareDrawTextures`
reopens the render pass afterward; ending the encoder without reopening it would
leave the following draw without an encoder. The close action resets cached
pipeline/binding state, so both the DXIL and MSL draw paths rebind it.

A local native Metal validation harness executing the production buffer-growth
method reproduced `failed assertion: encoding in progress` without the fix.
With the fix it preserved the old buffer's 64 test bytes and resumed a load/store
render pass while preserving its color. This verifies the encoder sequencing;
the actual door transition still requires a gameplay retest.
