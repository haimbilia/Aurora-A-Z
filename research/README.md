# Research — not production

Nothing in this folder is a release candidate. `IMPLEMENTATION_PLAN.md` rule 1
forbids shipping another patched skin, and these scripts do exactly that. They
are kept for one reason: they are the only working proof that Aurora's own
QuickView scene can be repositioned and driven, which is useful background for
the M3 overlay spike.

## What is here

| File | What it proves |
|---|---|
| `patch-skin.ps1` | Any skin `.xzp` can be unpacked, its `Aurora_QuickView.xur` rewritten, and repacked, changing exactly two entries |
| `quickview-alphabet-row.ps1` | `ScnQuickViewUI`'s letter strip can be moved to an arbitrary Y and its icon slots suppressed |

```powershell
pwsh -File research\patch-skin.ps1 `
     -SkinPackage "original\skins\Series.xzp" `
     -OutputPackage "out.xzp" [-Offset 70]
```

Requires `tools\XUIHelper\XUIHelper.CLI\bin\Release\net8.0\XUIHelper.CLI.exe`
(local only, gitignored).

## Why this approach was rejected

It works, and it was verified on Default, Dark, Dark Theme Ultimate and Series,
each output changing only `Aurora_QuickView.xur` and `skin.meta`. It still fails
the product contract:

- it requires modifying a skin, which `REQUIREMENTS.md` forbids;
- a new skin needs re-patching on a Windows PC, so it is not skin agnostic;
- `ScnQuickViewUI` shows **one letter at a time**, not the mockup's full row.
  Slots `QVText2..5` are offscreen animation buffers at opacity 0. Whether
  `TabCount` can exceed 5 was never tested;
- it is reached with RB, not R3, and it replaces the QuickView menu rather than
  leaving it alone.

## Findings worth carrying into M3

- `Animator` animates offscreen → rest → offscreen, so the resting position is
  the **minimum** Y in the timeline, not the last keyframe.
- Skins disagree on that value: Default, Dark and Dark Theme Ultimate rest at
  `Y=602`; Series rests at `Y=-115.07`. Any geometry must be computed relative
  to the skin, never hardcoded.
- `Series.xzp` ships a corrupt `skin.meta`: valid JSON followed by leftover
  bytes from a longer previous version. Aurora stops at the closing brace and
  does not care; `ConvertFrom-Json` throws. Parse skin metadata defensively.

Full detail in `HANDOFF.md` §5 and §6.
