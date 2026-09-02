# AuroraAZ bootstrap options for Aurora Rev1655

Date: 2026-09-02

## Bottom line

No safe bootstrap path was found that satisfies all of these constraints at
the same time:

1. the only new installed file is named `AuroraAZ.xex`;
2. Aurora 0.7b.2 Rev1655 loads it automatically;
3. no `launch.ini` or other configuration is changed;
4. no stock XEX is replaced or impersonated;
5. no skin, XZP, XUR, Lua script, QuickView, or database row is required.

This is a deterministic loader limitation, not an XEX packaging limitation.
Rev1655 never asks the kernel to load `game:\Plugins\AuroraAZ.xex`. A valid
XEX at that path therefore remains inert.

`Default.xzp` and the 2015 RealModScene "Aurora plugin patches" do not provide
a hidden bootstrap. They are UI-resource mechanisms, not native-module
discovery mechanisms.

The least invasive viable bootstrap remains a DashLaunch system-plugin slot.
That route can keep the **release payload and new installed payload** to one
file, `AuroraAZ.xex`, and it is skin agnostic, but installation must add one
path to the existing DashLaunch configuration. It therefore does not satisfy
the current "copy one file and change nothing else" rule.

## Evidence baseline

The conclusions apply to the exact tested Rev1655 artifacts:

| Artifact | SHA-256 |
| --- | --- |
| `Aurora.xex` | `583BCD442D8017D6FCB2645B93CDA987F4C0A43A688B652D7364CCAEDAEEFA9F` |
| extracted `Aurora.exe` | `5BB5BAF8DF4CCB197241B34935EB400F36C8C20648CC074E2C30FA80ADD37E3C` |
| stock `Default.xzp` | `3B958674D4331D98C84CAF7836EAC8E72C747B6DE66328D43113337A62187776` |

The native loader proof is recorded in
[`NATIVE_LOADER.md`](NATIVE_LOADER.md). The relevant facts are:

- `PluginManager` constructs exactly seven wrapper objects in code.
- Each wrapper owns one literal path and one module-specific ordinal ABI.
- There is no `Plugins\*.xex` enumeration, external module descriptor, or
  arbitrary-path wrapper.
- The local `XexLoadImage` wrapper at `0x82801C30` has only four direct call
  sites in the Aurora text section. The plugin call site is the common loader
  for those seven preconstructed wrappers; the remaining sites load fixed
  platform components.
- None of the paths is `game:\Plugins\AuroraAZ.xex`.

The seven recognized paths are:

```text
game:\Plugins\CFEditDll.xex
game:\Plugins\connectx.xex
game:\Plugins\launch.xex
game:\Plugins\FtpDll.xex
game:\Plugins\Nova.xex
game:\Plugins\SmbDll.xex
game:\Plugins\NetDbgDll.xex
```

The wrapper does not merely run a generic entry point. After loading, it asks
for a fixed set of exports appropriate to the expected stock module. Renaming
AuroraAZ to one of these paths would therefore impersonate a reserved module
and expose Aurora to invalid function pointers. It is not a safe discovery
mechanism even when an optional stock file happens to be absent.

## Why `Default.xzp` cannot bootstrap the XEX

### What the package actually contains

The locally extracted stock `Default.xzp` contains 418 files:

| Type | Count |
| --- | ---: |
| PNG images | 350 |
| compiled XUI resources (`.xur`) | 39 |
| DDS images | 17 |
| fonts (`.ttf`) | 4 |
| shaders (`.fx`) | 2 |
| XML/meta/version records | 3 |
| JPG, XBG mesh, XPR texture | 3 |
| XEX, DLL, EXE, or Lua files | **0** |

`skin.xml` describes fonts, visual-resource paths, and per-control assets. Its
`<ModuleList>` section can look like a plugin registry at first glance, but it
only maps fixed module names to settings-screen icon paths. For example,
`<CFEDITDLL>`, `<FREESTYLEPLUGIN>`, and `<NOVADLL>` each contain a PNG path;
there is no executable image path, ordinal list, enable policy, or
registration callback there.

The compiled XUI also uses entries such as:

```xml
<ClassOverride>ScnApplication</ClassOverride>
```

That selects a native XUI class which Aurora has already registered. It does
not instruct XUI to locate or load `ScnApplication.xex`, and inventing a new
class name does not supply the missing native code.

An XZP builder may be able to store an arbitrary file, but storage is not
execution. Even if `AuroraAZ.xex` were inserted into an XZP, the verified
Rev1655 image-load call sites contain no path from skin-resource parsing to
`XexLoadImage`. No manifest field in the stock package declares a native
module. Treating an XUI parser bug as an injection mechanism would be an
unproven exploit, not a safe plugin interface.

### It would not be skin agnostic anyway

Aurora loads the selected skin package. Patching `Default.xzp` affects only
that package; it does not alter CleanNXE, Dark, Series, or another selected
skin. Rebuilding every skin would add multiple installed artifacts and make
compatibility depend on each skin's scene structure. Therefore an XZP-based
bootstrap fails the skin-agnostic requirement independently of the native
loader failure.

## What the RealModScene “plugin patches” actually are

Primary source:
<https://www.realmodscene.com/index.php?/topic/5360-aurora-plugin-patches/>

The locally captured page is `tools/aurora-plugin-patches.html` (SHA-256
`A42DFAD103FBD9835CA9C0F79393A5A7BFE4E8C3421DE0322E4293009FB9B254`).
The author describes the work as an Aurora **theme for the Freestyle Plugin**
and explicitly says it only changes the displayed "Freestyle Home" text and
icon. Its installation instruction is to replace three files under:

```text
Game:\Plugins\Hudscene
```

Later in the thread, the author describes editing XUI/XUR resources and notes
that the images are not packaged in an XZP like normal Aurora/FSD themes.
Nothing in the post describes an XEX loader, native callback ABI, or arbitrary
module registration. The word "plugin" in the title refers to the existing
Freestyle Plugin whose Guide/HUD resources are being themed; the patch is not
an Aurora plugin SDK.

The file currently named
`reference/downloads/Hud Scene_aurora.rar` must not be used as evidence or
installed: it is a 136,677-byte HTML forum response beginning with
`<!DOCTYPE html>`, not a RAR archive. The forum attachment currently requires
permission for anonymous download. The conclusion above relies on the
author's accessible post and the verified native loader, not on an assumed
attachment payload.

The Aurora release package reinforces the distinction: `Plugins\HudScene`
contains `.xur` and image resources, while executable modules such as
`Plugins\Nova.xex` and `Plugins\FtpDll.xex` are separate files and are loaded
through their hardcoded native wrappers.

## Candidate matrix

| Candidate | Loads independent `AuroraAZ.xex`? | Skin agnostic? | One new payload file? | Changes existing state? | Result |
| --- | --- | --- | --- | --- | --- |
| Copy `AuroraAZ.xex` into `Plugins` | No; no enumeration or literal path | Yes | Yes | No | **Impossible on Rev1655** |
| Put the XEX inside `Default.xzp` | No executable-load path | No | No in the requested form | Replaces a skin | **Reject** |
| RealModScene HudScene patch | No; replaces XUR/image resources | Not applicable to coverflow | No | Replaces HUD assets | **Reject** |
| Add a custom XUI `ClassOverride` | No; native class must already exist | No | No | Replaces a skin | **Reject** |
| Add a User Lua utility/content script | No documented arbitrary XEX load API; also requires a companion script and user/script lifecycle | UI portion can vary | No | Adds script state | **Reject** |
| Launch the XEX as a title from File Manager/Nova | Runs as a different title, not as an in-process Aurora module | Yes | Yes | Manual launch; leaves Aurora | **Not a bootstrap** |
| Use a reserved wrapper filename such as `CFEditDll.xex` or `NetDbgDll.xex` | Potentially, only by impersonating the expected module and ABI | Yes | One renamed file | Occupies/replaces a stock-reserved module | **Unsafe; reject** |
| Replace or proxy `Nova.xex`/`FtpDll.xex` | Yes in principle | Yes | Not an independent named file | Replaces stock functionality | **Unsafe; reject** |
| Patch `Aurora.xex` on disk | Yes | Yes | No | Replaces dashboard executable | **Reject** |
| Patch the manager in memory | Only after some other code is already running | Yes | Depends on bootstrap | Requires an external loader | **Circular** |
| Remote debugger/JRPC-style injection | Possible as a development technique | Yes | No deployable standalone contract | Requires an external resident service/PC | **Lab-only, not release** |
| DashLaunch system-plugin slot | Yes; the boot loader supplies the missing load request | Yes | **Yes** | **Edits DashLaunch configuration and reboots** | **Viable if constraint is relaxed** |

## Artifact-count distinction for the DashLaunch option

If the user approves a DashLaunch slot, the project can still legitimately
ship one binary:

```text
AuroraAZ.xex
```

No companion font, shader, Lua file, skin, database record, or helper XEX is
required. The necessary distinction is that one existing configuration file
must reference that binary. In other words:

- release payload count: **one file**;
- newly installed payload count: **one file**;
- installation mutations: **copy one file plus change one plugin-slot value**.

That is materially different from the current documented promise of copying
one file with no `launch.ini` change. It requires explicit approval before any
console deployment because an incorrect boot-plugin configuration can affect
startup and recovery.

## Confidence and remaining uncertainty

- **High confidence:** Rev1655 has no arbitrary native-plugin scan; the seven
  hardcoded paths and their ordinal contracts are recovered from the exact
  console-matching executable.
- **High confidence:** stock `Default.xzp` is a skin-resource package and has
  no native-module declaration; the stock contents and `skin.xml` were
  inspected directly.
- **High confidence:** the RealModScene item is a Freestyle Guide/HUD resource
  theme, not a native Aurora plugin loader; this is stated by its author.
- **High confidence:** patching one XZP is not skin agnostic.
- **Not claimed:** that no memory-corruption exploit could ever be built from
  an XUI/XZP parser. Such an exploit was neither found nor sought; it would not
  meet the project's safe/supportable requirement.
- **Not hardware-tested here:** any experimental bootstrap. This investigation
  was read-only and made no console changes.

## Decision gate

Keep native canary, input, rendering, and filter work buildable offline, but do
not call M1 complete and do not deploy a boot plugin until the user explicitly
chooses one of these product contracts:

1. **Strict no-configuration contract:** stop; Rev1655 cannot load the file.
2. **One-binary contract:** permit one DashLaunch plugin-slot entry while the
   shipped and newly installed payload remains only `AuroraAZ.xex`.
