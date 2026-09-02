# Rev1655 native module loader analysis

Date: 2026-09-02

## Result

Aurora 0.7b.2 Rev1655 does **not** discover arbitrary XEX files in its
`Plugins` directory. Its `PluginManager` constructor allocates seven specific
wrapper classes and gives each wrapper one hardcoded path. There is no
`Plugins\*.xex` enumeration or generic manifest path in this flow.

Consequently, copying a new file to
`Hdd1:\Aurora\Plugins\AuroraAZ.xex` cannot satisfy the M1 loading gate. The
file will remain unopened because no Rev1655 object refers to that path.

This is a loader blocker, not a compiler blocker. A correctly built system XEX
still needs an already-running component to call `XexLoadImage` for it. The
current one-file/no-configuration requirements provide no such bootstrap.

## Analysis baseline

All addresses below are virtual addresses for this exact Aurora image only:

| Artifact | SHA-256 |
| --- | --- |
| `Aurora.xex` | `583BCD442D8017D6FCB2645B93CDA987F4C0A43A688B652D7364CCAEDAEEFA9F` |
| Extracted `Aurora.exe` | `5BB5BAF8DF4CCB197241B34935EB400F36C8C20648CC074E2C30FA80ADD37E3C` |
| Console `FtpDll.xex` | `4546FCD1BD9DAD672142046AD48BEECFDCC475F11715517EBA1A87F2E56BFDA6` |
| Console `Nova.xex` | `3FDF5175A4CAAA74E075A839776B12D15982AC304807EBC9F8D9FF0B8AE218FA` |

`Aurora.xex` is a fixed-base development XEX loaded at `0x82000000`; its entry
point is `0x828050E0`. The file timestamp recorded in the XEX is 2021-11-10
13:53:46. Any native integration must reject a different executable hash and
verify the expected instructions before using these addresses.

The analysis used `tools/jeff.exe` to inspect/extract the XEX and Capstone in
PowerPC 32-bit big-endian mode to trace string references, constructors,
vtables, direct branches, and ordinal lookups. The binary has no usable C++
symbols, so class and method names in this document are descriptive names
derived from behavior.

## Hardcoded registry construction

The `PluginManager` constructor begins at `0x8238CBF8`. It constructs its
registry at `0x82BC3860`, allocates the following objects, and inserts them by
integer key. These are constructor calls in code, not rows parsed from a file.

| Key | Display name | Module identity | Exact image path | Object constructor | Size | Raw mode/policy | Vtable |
| ---: | --- | --- | --- | --- | ---: | --- | --- |
| 1 | Coverflow Edit | `dll.aurora.cfedit` | `game:\Plugins\CFEditDll.xex` | `0x823894F8` | `0xA8` | `9, 1` | `0x821425C0` |
| 2 | ConnectX | `connectx.xex` | `game:\Plugins\connectx.xex` | `0x82389750` | `0x9C` | `10, 0` | `0x821425E8` |
| 3 | Dashlaunch | `launch.xex` | `game:\Plugins\launch.xex` | `0x82389D48` | `0xA8` | `10, 2` | `0x82142610` |
| 4 | FTP Server | `dll.aurora.ftp` | `game:\Plugins\FtpDll.xex` | `0x82389FB0` | `0xCC` | `9, 1` | `0x82142638` |
| 5 | Nova | `Nova.xex` | `game:\Plugins\Nova.xex` | `0x8238A590` | `0x110` | `10, 0` | `0x82142688` |
| 6 | SMB Server | `dll.aurora.smb` | `game:\Plugins\SmbDll.xex` | `0x8238A2E0` | `0xB8` | `9, 1` | `0x82142660` |
| 7 | Network Debugger | `dll.aurora.netdbg` | `game:\Plugins\NetDbgDll.xex` | `0x8238E848` | `0xA8` | `9, 1` | `0x821425C0` |

The raw mode and policy values are included as evidence but are intentionally
not assigned speculative enum names. The load routine uses them to select
different image-loading paths and policies.

The common wrapper constructor is at `0x8238EBB8`. Its observed register
signature is:

```cpp
ModuleWrapper(
    void *self,                  // r3
    const std::string &path,     // r4; stored at self + 0x3C
    const std::string &identity, // r5; stored at self + 0x20
    uint32_t mode,               // r6; stored at self + 0x58
    uint32_t policy,             // r7; stored at self + 0x5C
    const std::wstring &label);  // r8; stored at self + 0x04
```

The loaded module handle is stored at `self + 0x60`. No constructor accepts a
directory, wildcard, external descriptor, or arbitrary additional module.

As a cross-check, the local `XexLoadImage` wrapper at `0x82801C30` has only
four direct call sites in the entire Aurora text section. One is the common
module load path at `0x82388DC0`; the others load fixed platform components
such as `xboxkrnl.exe` and `PlayReady.xex`. The lower-level direct image-load
sites likewise contain no Plugins-directory scan. This supports the registry
constructor evidence rather than relying on strings alone.

## Generic load lifecycle

The common load method is `0x82388B50`; the common unload method is
`0x82388FC0`.

The load method:

1. Checks the hardcoded module identity with the local
   `XexGetModuleHandle` wrapper.
2. Selects an image-loading path from the wrapper's raw mode/policy fields.
3. Loads the wrapper's hardcoded path.
4. Retrieves the module handle again and stores it at offset `0x60`.
5. Calls virtual slot `+0x04`, which resolves that wrapper's fixed export
   ordinals.
6. Calls virtual slot `+0x20` for the common post-load work.
7. Logs `Completing DLLModule loading: %s` and notifies `PluginManager`.

Relevant fixed addresses are:

| Behavior | Address |
| --- | --- |
| Common load | `0x82388B50` |
| Common unload | `0x82388FC0` |
| Common wrapper constructor | `0x8238EBB8` |
| `XexUnloadImage` wrapper | `0x82801B88` |
| `XexGetProcedureAddress` wrapper | `0x82801BC0` |
| `XexLoadImage` wrapper | `0x82801C30` |
| `XexGetModuleHandle` wrapper | `0x82801CB0` |
| PluginManager loaded notification | `0x8238D2B0` |
| PluginManager unloaded notification | `0x8238D520` |

The API names in this table are inferred with high confidence from their
arguments, return handling, and the underlying Xbox kernel call pattern.

An XEX's standard image entry point is run by the kernel image loader. Aurora
does not look for a universal named `Initialize`, `PluginMain`, or equivalent
export. After the image is loaded, Aurora's precompiled wrapper asks for the
module-specific ordinal set below. This means an image entry point alone is
not the Aurora plugin ABI.

For comparison, the inspected FTP image has base `0x85000000`, entry point
`0x850173D0`, and original PE name `FtpDll.dll`. The Nova XEX header records
base `0x91A00000`, entry point `0x91A70170`, and original PE name `Nova.dll`.

## Module-specific ordinal contracts

Virtual slot `+0x04` is not a generic registration callback. Every wrapper
contains compiled calls to `XexGetProcedureAddress(module, ordinal)` and
stores the returned pointers in fixed object fields. The directly observed
resolution requests are:

| Wrapper | Resolver | Ordinals requested |
| --- | --- | --- |
| Coverflow Edit / Network Debugger | `0x82389650` | `2` through `5` |
| ConnectX | `0x823898A0` | `23` |
| Dashlaunch | `0x82389EC8` | `9` through `11` |
| FTP Server | `0x8238A130` | `2` through `14` |
| SMB Server | `0x8238A440` | `2` through `9` |
| Nova | `0x8238A7A0` | `2`, `3`, `7` through `19`, and `22` through `35` |

These are ordinal requests, not recovered source-level function signatures.
Most resolvers mark the wrapper ready when a module handle exists without
checking every returned pointer at resolution time; later module-specific code
can still assume those pointers are valid. Mimicking a known filename without
matching its ABI is therefore crash-prone.

## Why `AuroraAZ.xex` discovery fails

The failure is deterministic:

```text
Aurora starts
  -> PluginManager constructor allocates seven known wrapper types
  -> each wrapper retains one literal image path
  -> load requests operate on a wrapper selected from keys 1..7
  -> no wrapper contains game:\Plugins\AuroraAZ.xex
  -> Aurora never calls XexLoadImage for AuroraAZ.xex
```

The word `Plugins` in the directory name should not be interpreted as a public
plugin SDK or filesystem discovery contract. In Rev1655 it is merely the
location of Aurora's own known native modules.

Replacing or renaming one of the known modules is not an acceptable shortcut:

- it removes or corrupts a stock feature;
- the known wrapper resolves a module-specific ordinal ABI;
- the manager may invoke those pointers later;
- update/recovery behavior becomes unsafe;
- it does not provide an independent Aurora A-Z lifecycle.

## Implication for the implementation plan

M1 must remain failed until a separate bootstrap is proven. Do not spend time
on input, rendering, or filtering hooks in an XEX that Aurora cannot load.

The available directions change at least one current constraint:

| Direction | Technically plausible | Requirement impact |
| --- | --- | --- |
| Configure `AuroraAZ.xex` as a DashLaunch system plugin | Yes; requires a hardware canary and title-lifecycle handling | Adds or changes boot-loader configuration, so installation is not literally one untouched-file drop |
| Patch Aurora's manager to add an eighth wrapper/path | Yes for this exact hash, with substantial reverse engineering | Modifies Aurora code in memory or on disk; an external bootstrap is still needed for an in-memory patch |
| Ship a patched `Aurora.xex` | Technically direct | Violates the no-on-disk-patch constraint and is unsuitable for distribution |
| Replace `FtpDll.xex`, `NetDbgDll.xex`, or another known file | Unsafe | Conflicts with stock functionality and requires the victim module's ordinal ABI; reject |
| Modify Nova or another stock module to load Aurora A-Z | Possible in principle | Modifies a second artifact and violates the one-file/no-stock-modification requirements |

The cleanest next decision is whether “one release payload” may include a
one-time installer change to a DashLaunch plugin slot. If the strict rule is
instead “copy only `AuroraAZ.xex` into Aurora's Plugins folder and change
nothing else,” Rev1655 cannot meet it through its native manager.

## Confidence and limits

- **High confidence:** seven hardcoded registry entries, literal paths,
  constructor and lifecycle addresses, lack of arbitrary path discovery in
  this manager flow, vtable assignments, and ordinal numbers.
- **High confidence inference:** the four kernel-wrapper roles, based on exact
  Xbox API calling patterns.
- **Not yet recovered:** source-level names and signatures for the
  module-specific ordinal exports, a safe externally supplied bootstrap, and
  the coverflow input/render/filter hook sites.
- **Not tested here:** loading any experimental image on the console. This was
  offline analysis only and made no console changes.
