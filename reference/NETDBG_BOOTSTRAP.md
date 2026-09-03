# Rev1655 Network Debugger bootstrap

Date: 2026-09-02

## Proven bootstrap decision

Aurora A-Z uses Aurora 0.7b.2 Rev1655's optional Network Debugger wrapper as
its one-file bootstrap. M1 proved this path on hardware. The release artifact
is `AuroraAZ.xex`; installation copies the same binary bytes to:

```text
Hdd1:\Aurora\Plugins\NetDbgDll.xex
```

This is skin agnostic and requires no Lua, XZP, database row, stock executable
patch, DashLaunch plugin slot, or `launch.ini` change. It is valid only when
the target file is absent. An installer must never overwrite an existing
Network Debugger module.

This design intentionally supplies Aurora's expected NetDbg ABI. It does not
claim that Rev1655 has a generic third-party plugin interface. Hardware evidence
now proves the corrected container, ordinal resolution, automatic ordinal-4
dispatch, Aurora's thread wrapper, and entry into AuroraAZ-owned worker code.
That closes M1; it does not prove that controller observation, input
consumption, overlay rendering, or coverflow filtering works.

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

| Ordinal | Call site | Aurora arguments | Current M2a compatibility behavior |
| ---: | --- | --- | --- |
| 2 | `0x8232A740` | `r3=9994`, `r4=9993`, `r5=3` | Compatibility no-op; return zero |
| 3 | `0x8232A7BC` | No deliberate argument setup | Non-blocking fallback shutdown request; return zero immediately |
| 4 | `0x8232A9D8` | `r3` = NUL-terminated formatted log line | Atomically claim the one-shot bootstrap, start a worker, and return without recursive logging |
| 5 | No key-7 call site | Resolved only | Export a valid immediate-return stub |

Aurora ignores the return value at every observed call. The key-7 logger has
one direct construction site at `0x8232A550` and no hidden ordinal-5 call in
its primary or secondary vtable paths. The final M1 marker identifies ordinal
4 as the automatic startup source; this is observed hardware behavior rather
than an inference from the static call graph.

Ordinal 3 is not a usable pre-launch notification. A shutdown-capable build
still black-screened during title handoff, and the persisted v5 runtime marker
after reboot recorded `shutdown_requests=0` and state `RUNNING`. The active
lifecycle experiment instead hooks the exact Rev1655 `ContentLauncher` entry
at `0x82294DD0`, synchronously closes the runtime, restores that entry and the
three feature hooks, and resumes original execution at `0x82294DD4`.

The build requires an ordinal-only PE export table (`NONAME`) containing
exactly ordinals 2, 3, 4, and 5. That check alone is not sufficient: pinned
SynthXEX v0.0.6 leaves the XEX security-info export pointer at zero even when
the PE has `.edata`. `scripts/xex_exports.py` therefore fills a dedicated
mapped code section with the big-endian Xbox export-address table before
SynthXEX hashes the image, sets the absolute pointer at security-info offset
`+0x160` after packaging, and validates the final table, code-page descriptor,
page hash chain, and header hash. A PE-only export success cannot reach the
hardware gate.

## Hardware results

### Earlier loader attempts (superseded)

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
XEX fields together, so the hardware result does not isolate a single cause.

The corrected 24,576-byte image had SHA-256:

```text
C51E3A322B07D1DE094C644E33D005D87305FFB24B587548953F1E88678C63E5
```

Its FTP round-trip hash matched. AuroraAZLab remained usable at 1280x720 and
logged:

```text
IDllBase::Load: Completing DLLModule loading:  dll.aurora.netdbg
PluginManager: Module Loaded:  dll.aurora.netdbg
```

This proved that the corrected XEX reached the wrapper's post-resolution loaded
notification. It did not produce the expected signal because the original
`DllMain`/thread-start observation contract was wrong; it is not the current M1
status. The file was renamed
`NetDbgDll.xex.disabled-c51e3a322b07`, the lab restarted with the active path
absent, and production Aurora was restored without modifying its files or
`launch.ini`.

### Final M1 acceptance (complete)

The passing canary was built from commit `39b551c` by GitHub Actions run
`33604028771`. Both the CI artifact and the file downloaded after staging in
`Hdd1:\AuroraAZLab\Plugins\NetDbgDll.xex` had SHA-256:

```text
87894F41A89F4F3CAAFA8A1864AB8F8A91A2ED011882EEEF36E4D3FAEF58596C
```

Raw `ExCreateThread` variants did not enter the worker. The passing contract
validates and calls Aurora Rev1655's complete 25-word/100-byte thread wrapper at
`0x82361AA8`. Before that call it also validates the first 8 words/32 bytes of
`XapiThreadStartup` at `0x82804650`:

```text
7D8802A6 48163679 3BE1FF80 9421FF80
7C7E1B78 7C9D2378 39600000 917F0050
```

The validated wrapper supplies `XapiThreadStartup`, passes create flags `2`,
selects processor `3`, sets priority `15`, and resumes the returned handle once.
AuroraAZ does not duplicate those operations around the wrapper.

The two durable, 36-byte, big-endian `AZM1` records decoded as follows:

| Record | Magic/version/size | Calls | Source | Phase | State | Create | Resume |
| --- | --- | ---: | ---: | --- | --- | ---: | ---: |
| Primary `AuroraAZ-M1.bin` | `AZM1` / `4` / `36` | 1 | 4 | 5 (`COMPLETE`) | 2 (`RUNNING`) | 0 | 0 |
| Worker `AuroraAZ-M1-worker.bin` | `AZM1` / `4` / `36` | 1 | 4 | 7 (`WORKER_ENTERED`) | 2 (`RUNNING`) | 0 | 0 |

The primary source field proves that normal Aurora logger traffic called
ordinal 4 automatically. The independent worker-phase record can only be
published after AuroraAZ-owned worker code begins executing. Along with the
matching artifact/round-trip hash and Aurora's module-loaded events, these
records satisfy the M1 one-file bootstrap and code-execution gates.

### M2a boundary

M2a is now underway. Its ordinal-4 path is once-gated and starts a worker
through the same validated wrapper. That worker requests only
`AZ_REV1655_RUNTIME_STAGE_INPUT_OBSERVE`. M2a may observe controller state but
must not consume it; overlay and filter-consumer sources are outside this
milestone. No input, overlay, or filter functionality should be inferred from
the completed M1 result.

## Compatibility boundary

- Refuse unsupported Aurora hashes or mismatched hook-site bytes.
- Refuse installation when `Plugins\NetDbgDll.xex` already exists.
- Installing Aurora A-Z uses the Network Debugger slot; the two cannot coexist.
- Keep the release to one binary. Fonts, glyphs, shaders, and runtime state are
  embedded in that image.
