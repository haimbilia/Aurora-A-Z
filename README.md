# Aurora Alphabet Selector

An Aurora skin and Lua extension that adds a controller-friendly `# A ... Z`
selector to the main coverflow screen.

## Current milestone: visual skin proof

The first build only adds the selector to a copy of the Aurora Default skin.
It must load successfully before we connect input or filtering.

## Repository layout

```text
source/
  lua/                  Lua behavior (safe to edit in VS Code)
  skin/                 Editable XUI source and notes
reference/              Aurora-specific XUI definitions (local-only)
original/               Your unmodified Default.xzp (local-only)
build/                  Generated XZP packages (local-only)
tools/                  XuiTool/XZP utilities (local-only)
```

## Required local inputs

1. Copy `Aurora/Skins/Default.xzp` from the Xbox to `original/Default.xzp`.
2. Provide the exact Aurora version and screen resolution in use.
3. Install the Xbox 360 XDK UI Authoring Tool (`XuiTool.exe`), AuroraElements,
   and an XZP extract/repack utility.

## Workflow

1. Extract `original/Default.xzp` to a temporary working skin directory.
2. Convert `Aurora_Main.xur` to editable `.xui` with XuiTool.
3. Copy the resulting source into `source/skin/` and edit it in VS Code.
4. Export it back to `.xur`, package the skin as `build/Alphabet.xzp`, and
   install it on the Xbox via Aurora FTP.

Do not edit the only copy of `Default.xzp`; the build should always start from
an untouched copy.

## Behavior plan

`source/lua/Alphabet.lua` currently provides the alphabet normalization and
matching logic. Once the actual skin is available, it will be connected to the
Aurora scene and its documented filter API. The desired jump-to-first-title
behavior depends on whether the installed Aurora coverflow exposes a selectable
index/message; filtering remains the reliable fallback.

## Research boundary

The RealModScene “Aurora plugin patches” example is a patch for the separate
Freestyle HUD plugin, not an Aurora dashboard extension API. Its relevance and
limits are documented in `reference/RESEARCH.md`; do not use it as the basis
for modifying the main game list.
