# Rev1655 Network Debugger bootstrap

Date: 2026-09-02

## Candidate decision

Aurora A-Z is testing Aurora 0.7b.2 Rev1655's optional Network Debugger wrapper
as its one-file bootstrap. The proposed release artifact is `AuroraAZ.xex`;
installation would copy the same binary bytes to:

```text
Hdd1:\Aurora\Plugins\NetDbgDll.xex
```

This is skin agnostic and requires no Lua, XZP, database row, stock executable
patch, DashLaunch plugin slot, or `launch.ini` change. It is valid only when
the target file is absent. An installer must never overwrite an existing
Network Debugger module.

This design intentionally supplies Aurora's expected NetDbg ABI. It does not
claim that Rev1655 has a generic third-party plugin interface, nor that the
candidate bootstrap is hardware-proven before M1 passes.

## Exact tested image

All addresses and ABI conclusions apply only to the extracted Aurora image
with SHA-256:

```text
5BB5BAF8DF4CCB197241B34935EB400F36C8C20648CC074E2C30FA80ADD37E3C
```

The live console and the `Aurora 0.7b.2 - Release Package.rar` archive contain
no `NetDbgDll.xex`. The archive SHA-256 is:

```text
0C7C765C3A5B938CFC64C71BEDA276A17D109B88DF2A88E3F91EB1E74B7E46A0
```

## Loader and export contract

PluginManager key 7 constructs the wrapper at `0x8238E848` with literal path
`game:\Plugins\NetDbgDll.xex`, mode 9, and policy 1. Its resolver at
`0x82389650` requests ordinals 2 through 5. It marks the wrapper ready from the
module handle alone, so every export must be present and non-null.

| Ordinal | Call site | Aurora arguments | Required canary behavior |
| ---: | --- | --- | --- |
| 2 | `0x8232A740` | `r3=9994`, `r4=9993`, `r5=3` | Return immediately |
| 3 | `0x8232A7BC` | No deliberate argument setup | Return immediately |
| 4 | `0x8232A9D8` | `r3` = NUL-terminated formatted log line | Ignore it and return; never log recursively |
| 5 | No key-7 call site | Resolved only | Export a valid immediate-return stub |

Aurora ignores the return value at every observed call. The key-7 logger has
one direct construction site at `0x8232A550` and no hidden ordinal-5 call in
its primary or secondary vtable paths.

The build requires an ordinal-only PE export table (`NONAME`) containing
exactly ordinals 2, 3, 4, and 5. That check alone is not sufficient: pinned
SynthXEX v0.0.6 leaves the XEX security-info export pointer at zero even when
the PE has `.edata`. `scripts/xex_exports.py` therefore fills a dedicated
mapped code section with the big-endian Xbox export-address table before
SynthXEX hashes the image, sets the absolute pointer at security-info offset
`+0x160` after packaging, and validates the final table, code-page descriptor,
page hash chain, and header hash. A PE-only export success cannot reach the
hardware gate.

## First hardware result

The first canary did not pass M1. It was uploaded only to
`Hdd1:\AuroraAZLab\Plugins\NetDbgDll.xex`; a fresh download matched the local
SHA-256
`B20E2F54608FE071BACBFE2FF8221158A72D7577D51D5B82E297CE35E59699BA`.
The lab Aurora survived, but NOVA found no live thread in the reserved module
window and `debug.log` reported exactly:

```text
Failed to load game:\Plugins\NetDbgDll.xex
Failed to load NetDbgDll
```

The Xbox loader rejected the image before its entry point or export-resolution
contract could run. The production `Hdd1:\Aurora\` copy and its plugin
directory were untouched; `launch.ini` remained on the production title. The
lab file was recoverably renamed to
`NetDbgDll.xex.disabled-b20e2f54608f`, leaving the active target absent.

Comparison with working Rev1655 XEX images identified a corrected retry shape:

| Field | Failed canary | Corrected retry | Evidence boundary |
| --- | --- | --- | --- |
| Module flags | `0xA` (`sysdll`) | `0x9` (`titledll`) | Matches working `FtpDll.xex`; working Nova also uses `0xA`, so this is not a proven cause |
| Image-base optional header | absent | `0x10201 = 0x91D00000` | Present in every inspected working Rev1655 XEX; strongest isolated difference, still not hardware proof |
| TLS optional header | `0x20104`, empty SynthXEX stub | omitted | Working `FtpDll.xex` and Nova DLLs omit it; title `Aurora.xex` carries the same empty tuple, so scope rather than tuple contents is the candidate distinction |

Aurora wrapper mode `9` and XEX module flags `0x9` are unrelated fields; no
source establishes a numeric mapping between them. The retry changes all three
XEX fields together; the correction pipeline is validator-tested, but M1
remains pending until the resulting image loads on hardware.

## Corrected hardware gate

The retry remains an inert canary:

1. Verify the console still runs the exact Aurora build, normal boot points to
   the known-good `Hdd1:\Aurora\Aurora.xex`, and the target path is absent in
   both the production and laboratory plugin directories.
2. Inspect the built XEX2 header, hash, load base, imports, entry point, and
   exported ordinals offline.
3. Create and boot-test a clean `Hdd1:\AuroraAZLab\` copy before adding the
   canary. Its `Aurora.xex` must hash to
   `583BCD442D8017D6FCB2645B93CDA987F4C0A43A688B652D7364CCAEDAEEFA9F`.
4. Copy the single verified binary only to
   `Hdd1:\AuroraAZLab\Plugins\NetDbgDll.xex`, then download it to a new local
   filename and require the round-trip SHA-256 to match.
5. Launch the lab `Aurora.xex` through NOVA and confirm it returns to the
   coverflow. Do not change `launch.ini`.
6. Use NOVA `GET /thread` to prove a live canary monitor starts inside the
   reserved `0x91D00000-0x91DFFFFF` module window.
7. Confirm Aurora, FTP, NOVA, RB QuickView, and ordinary coverflow navigation
   still behave normally.
8. Return to the production Aurora copy, rename the lab canary to a
   SHA-derived `.disabled-<sha12>` filename, restart the lab once more, and
   prove the module-window thread is absent. A power cycle must always return
   to production Aurora if the lab crashes.

For a NOVA instance with security enabled, set `AURORAAZ_NOVA_USERNAME` and
`AURORAAZ_NOVA_PASSWORD` in the current process. The lab verification command
is:

```powershell
pwsh -File scripts/verify-nova-canary.ps1 `
  -ExpectedTitlePathSuffix '\AuroraAZLab\Aurora.xex'
```

No input hook, renderer, or filter mutation is linked into this retry image.
Failure at any gate stops the native rollout.

## Compatibility boundary

- Refuse unsupported Aurora hashes or mismatched hook-site bytes.
- Refuse installation when `Plugins\NetDbgDll.xex` already exists.
- Installing Aurora A-Z uses the Network Debugger slot; the two cannot coexist.
- Keep the release to one binary. Fonts, glyphs, shaders, and runtime state are
  embedded in that image.
