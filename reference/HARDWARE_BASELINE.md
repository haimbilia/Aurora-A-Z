# Rev1655 hardware baseline

The executable identity and first-party module inventory were recorded on
2026-09-02 before any native Aurora A-Z deployment. Those initial console
operations were read-only. Later probes described below were deployed only to
the separately launchable `Hdd1:\AuroraAZLab` copy. No file in the production
`Hdd1:\Aurora` tree was modified.

## Executable identity

The console copy at `Hdd1:\Aurora\Aurora.xex` is byte-identical to the local
Rev1655 reference:

```text
SHA-256  583BCD442D8017D6FCB2645B93CDA987F4C0A43A688B652D7364CCAEDAEEFA9F
size     12,333,056 bytes
entry    0x828050E0
base     0x82000000
```

This is the only Aurora executable accepted by the first compatibility
allowlist. Hook-site byte signatures remain mandatory in addition to the file
hash.

## First-party modules

`Hdd1:\Aurora\Plugins` currently contains these XEX files:

| File | Size | SHA-256 |
| --- | ---: | --- |
| `FtpDll.xex` | 200,704 bytes | `4546FCD1BD9DAD672142046AD48BEECFDCC475F11715517EBA1A87F2E56BFDA6` |
| `Nova.xex` | 245,760 bytes | `3FDF5175A4CAAA74E075A839776B12D15982AC304807EBC9F8D9FF0B8AE218FA` |

Read-only copies are retained under the ignored `original/console/` tree for
loader-contract analysis. They are proprietary runtime inputs and must not be
committed or redistributed.

## Clean v2 live-image capture

On 2026-09-03, the v2 diagnostic canary from commit `682a155` completed a
clean, CPU-visible capture of Rev1655 after a full console reboot. The ignored
local evidence directory is:

```text
build/lab-harness/image-dump-v2-clean-20260903-062648/
```

The earlier v1 capture read loader backing storage and is superseded for hook
compatibility decisions. The v2 canary deliberately copied the live mappings
through CPU-visible memory before writing them. It made no input, rendering,
filter, skin, database, or production-Aurora change.

### Captured artifacts (proven)

The 28-byte big-endian `AZID` v2 marker reached phase `2` with status
`0x0000000F`: all four v2 header/text mapped-and-written bits were set. It
reported 1,024 header bytes and 9,794,524 `.text` bytes.

| Artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| `AuroraAZ-image-dump.bin` | 28 | `886EA1C212D5C10353B0370BFF724B285B0318D99326709A46F5E83697D17E7C` |
| `AuroraAZ-live-header.bin` | 1,024 | `5F741CADD089B32B2EF5FCDDDFDF668A9E4344AE61DF6F3F6E76FF1198236925` |
| `AuroraAZ-live-text.bin` | 9,794,524 | `19E02188E8E8F940BABD51FC31B73BCAB8155B2440AB2E462AF3AF406442DE80` |
| `screen.bmp` | 3,768,442 | `87E5BB3970DE44705A27BB5926005715374D56604132B73D9E569B3953C20482` |

The live 0x400-byte PE header matches the extracted Rev1655 header exactly.
The diagnostic screenshot is a 1280x720 Default-skin `No Titles Found` screen;
the probe intentionally rendered no overlay.

### Exact `.text` comparison (proven)

The extracted reference `.text` starts at image RVA `0x00210000` and contains
exactly `0x009573DC` bytes. Its complete static SHA-256 is:

```text
EE2FB2EBA844EE1C444AD5D10A52D6474BC4CD9FE1B89B06B65B3E92D2A177EB
```

The live capture and reference are byte-identical through `.text` offset
`[0x00000000, 0x00955DFC)`, corresponding to image RVAs
`[0x00210000, 0x00B65DFC)`. This immutable prefix is `0x00955DFC` bytes and
has SHA-256:

```text
7C7C0B93114B34454D32CD21A1107EF18310E835761633AAD951CD2B4DB86847
```

Every difference is confined to the final import-thunk range:

```text
.text offsets   [0x00955DFC, 0x009573DC)
image RVAs      [0x00B65DFC, 0x00B673DC)
PE file offsets [0x00B58FFC, 0x00B5A5DC)
size            0x000015E0 bytes
layout          350 contiguous slots x 16 bytes
static SHA-256  A7E355CF28050E533D0B61EBB77DF74B741AC99714A7FCB02538CE5ED20E9486
```

In the extracted image, each slot is four big-endian PowerPC words:

```text
0x01000000 | (library_index << 16) | ordinal
0x02000000 | (library_index << 16) | ordinal
0x7D6903A6                                      mtctr r11
0x4E800420                                      bctr
```

The frozen table contains 152 `xam` and 198 `xboxkrnl` function thunks. Its
physical library runs begin at slot indices `0`, `81`, `255`, and `326`.

On this console, the loader changed exactly the first two words of all 350
slots: 700 changed 32-bit words in total. Each live slot uses `lis r11, imm16`
followed by `addi r11, r11, imm16`, matching word masks `0x3D600000` and
`0x396B0000`. The final `mtctr r11` and `bctr` words remained identical in all
350 slots. This hardware result supersedes the earlier emulator-derived
assumption that the second instruction would use `ori`.

Two observed `xam` targets are outside the normal stock-module address ranges:

| Slot | Thunk RVA | Ordinal | Live target |
| ---: | ---: | ---: | ---: |
| 60 | `0x00B661BC` | `0x0217` | `0x91F06F28` |
| 65 | `0x00B6620C` | `0x01FC` | `0x91A4ECF8` |

The bytes and target values above are proven by the clean capture. That a
runtime patch layer such as DashLaunch owns those two redirections is a strong
inference, not yet a proven attribution. A production gate therefore must not
accept arbitrary live target addresses or require every target to equal an
unredirected stock export. It needs an independent, authoritative resolution
policy and must fail closed when that policy cannot establish an exact target.

### v3 IAT capture (pending)

Commit `4a8c6ee` adds a v3 diagnostic capture of CPU-visible image RVA
`[0x00000400, 0x000009B4)` (`0x5B4`, or 1,460 bytes) as
`AuroraAZ-live-iat.bin`. Its 32-byte `AZID` v3 marker adds `iat_bytes` and the
IAT mapped/written status bits. GitHub Actions run `33724035200` completed
successfully, and the downloaded 90,112-byte artifact has SHA-256
`ACB529A3CDC048F19DCA8132D31B7D8637366CF59D894938162C6D0CAD4556E3`.
Its hardware capture is still pending; no live v3 result should be described
as proven.

Static analysis predicts 152 `xam` entries at RVAs `0x400` through `0x65C`, a
separator at `0x660`, and 211 `xboxkrnl` entries at `0x664` through `0x9AC`,
followed by a terminator at `0x9B0`. The latter comprise 198 function imports
and 13 data imports at RVAs `0x670`, `0x6C0`, `0x770`, `0x784`, `0x794`,
`0x7CC`, `0x7E4`, `0x7E8`, `0x8A0`, `0x8AC`, `0x8E8`, `0x918`, and `0x928`.
These are offline structure findings until the v3 hardware bytes confirm their
live semantics.

Production Aurora was restored after the v2 capture and remains untouched. At
the capture checkpoint, the non-hooking v2 probe still occupied the isolated
lab path `Hdd1:\AuroraAZLab\Plugins\NetDbgDll.xex`; a cold reboot followed by
a production-FTP rename is the planned reversible cleanup before deploying v3.

## NOVA visual baseline

The NOVA WebUI on the console returned an existing screenshot as a valid
1280×720 BMP. The captured screen is the CleanNXE coverflow shown in the target
mockup before the alphabet overlay is added:

```text
resolution  1280 × 720
BMP bytes   3,768,442
SHA-256     0ADCC8A3F445F100B178D09E906124F8EA9F9432F83740B268BC8E3D1DE90FF8
```

Use `scripts/capture-nova.ps1` for later before/after captures. Generated BMPs
belong in the ignored `build/nova/` directory.
