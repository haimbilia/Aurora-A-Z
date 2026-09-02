# Rev1655 hardware baseline

Recorded on 2026-09-02 before any native Aurora A-Z deployment. All console
operations used to establish this baseline were read-only.

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
