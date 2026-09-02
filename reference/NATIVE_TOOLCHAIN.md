# Native Xbox 360 toolchain audit

Date: 2026-09-02

## Result

A complete pinned OpenXeChain toolchain now builds in GitHub Actions and
produces a real one-file `AuroraAZ.xex`. Workflow run `33583339065` completed
the compiler, linker, SynthXEX packaging, strict export/header validation, and
artifact upload. Toolchain compilation is no longer the M1 blocker.

The first CI XEX was nevertheless rejected by the retail console's
`XexLoadImage` before its entry point. Successful compilation and internal XEX
hash validation therefore remain offline gates, not hardware acceptance. The
current retry patches pinned SynthXEX to emit a title-DLL-shaped image with
module flags `0x9`, explicit Image Base Address header `0x10201`, and no empty
TLS stub. The resulting image now passes Aurora's hardware load and ordinal
resolution path. Toolchain/container acceptance is proven; the canary's own
code-execution signal remains pending.

Do not treat a file renamed to `.xex`, a libXenon ELF, or a repacked existing
binary as a plugin build.

## Installed Microsoft/XDK route

The workstation was checked for the normal Xbox 360 XDK integration:

- `%XEDK%` is not set.
- `C:\Program Files (x86)\Microsoft Xbox 360 SDK` is absent.
- `C:\Program Files (x86)\MSBuild\Microsoft.Cpp\v4.0\Platforms\Xbox 360`
  is absent.
- No `xexbuild.exe`, `imagexex.exe`, `xextool.exe`, `xenon-gcc`, or PowerPC
  cross-compiler is on `PATH`.
- Visual Studio Community 2026 is installed, but its C++ toolset directory is
  absent and `vswhere` did not report the native C++ workload.
- The 32-bit .NET Framework MSBuild 4 executable is present at
  `C:\Windows\Microsoft.NET\Framework\v4.0.30319\MSBuild.exe`, but it is not
  useful without the XDK's Xbox 360 platform files and libraries.

Therefore an XDK project cannot currently be compiled on this workstation. CI
uses the clean-room OpenXeChain route below instead. If a legitimately licensed
XDK is supplied later, it remains a useful comparison route. Primary project
references confirm that a DashLaunch-style module is a dynamic/system XEX with
a DLL entry point and XEX image metadata:

- <https://github.com/ClementDreptin/ModdingResources/blob/main/GettingStarted/getting-started.md>
- <https://github.com/xeghosted/xbox360-xex-vs2026-tutorial/blob/main/README.md>

## Clean-room route: OpenXeChain

[OpenXeChain](https://github.com/OpenXeChain) is a current clean-room LLVM
toolchain targeting programs that run under the Xbox 360 operating system. It
is distinct from libXenon, which targets bare-metal XeLL payloads and is not a
drop-in dashboard plugin toolchain.

The audited buildscript commit is
`eed1fa65bf9577fd31625764b320a90182ea9ade`. It pins:

| Component | Commit |
| --- | --- |
| LLVM/Clang/LLD | `890b83f6c8259a8899e182a5f7d9cf39c64131cc` |
| Newlib | `f929633c27099d404e9cb5b2739a9c7f9b6afccc` |
| SynthXEX | `48d1453a55468aa2f8a211db0b20edd594ef5be3` (`v0.0.6`) |
| xecorelib | `c65d67e071357acade681f04c46ae9719797f239` |

Evidence relevant to AuroraAZ:

- SynthXEX `v0.0.6` accepts both `-t sysdll` (module flags `0xA`) and
  `-t titledll` (module flags `0x9`). The first AuroraAZ attempt used
  `sysdll`; the corrected loader retry uses `titledll`. This is a compatibility
  candidate, not a claim that Aurora wrapper mode `9` maps to XEX flags `0x9`.
- xecorelib supplies ordinal import libraries for `xboxkrnl.exe` and
  `xam.xex`; its definitions include `DbgPrint`, module-loading APIs,
  `XamInputGetState`, and `XamInputGetKeystrokeEx`.
- The project describes itself as early-development software. Hardware
  acceptance is mandatory; successful compilation alone is not a release
  gate.

Primary sources:

- <https://github.com/OpenXeChain/buildscript>
- <https://github.com/OpenXeChain/SynthXEX>
- <https://github.com/OpenXeChain/xecorelib>

## Reproducible CI build

The workflow pins the buildscript and component commits above, applies the
repository's Newlib preprocessor correction, builds the full cross-toolchain,
and caches it for later canary retries. The successful first run used source
commit `f9258376a5c5ad8c8ee3182b1e982b4e34778c97` and produced:

| Item | Value |
| --- | --- |
| Workflow run | `33583339065` |
| Artifact ID | `9830578978` |
| Artifact ZIP SHA-256 | `15B580178F0857571B23035E0852C2660D83FFEBB81DEA2273ED8C0542890BCC` |
| First XEX SHA-256 | `B20E2F54608FE071BACBFE2FF8221158A72D7577D51D5B82E297CE35E59699BA` |
| Image base / entry point | `0x91D00000` / `0x91D01000` |
| Exports | ordinal-only 2, 3, 4, 5 |

That XEX passed the repository's export table, code-page, page-hash-chain, and
header-hash validators, then failed the hardware load described in
`NETDBG_BOOTSTRAP.md`. The corrected pipeline additionally requires XEX module
flags `0x9`, optional header `0x10201` equal to the PE/security image base, and
absence of the synthetic empty TLS header. These checks prevent a known-bad
header shape from reaching hardware; they cannot replace hardware observation.

Corrected workflow run `33588884258` built source commit
`43cef3a3f40fed2787c3a0246a6647a9511d4272`. Its 24,576-byte XEX has SHA-256
`C51E3A322B07D1DE094C644E33D005D87305FFB24B587548953F1E88678C63E5`.
The console accepted it through Aurora's wrapper and emitted both module-loaded
events, proving the corrected container shape and ordinal-resolution path.

## Local packager proof

WSL2 Ubuntu 24.04 is installed. The following packages were installed:

```text
build-essential 12.10ubuntu1
bzip2           1.0.8-5.1ubuntu0.1
clang           1:18.0-59~exp2 (compiler reports 18.1.3)
cmake           3.28.3-1build7
gawk            1:5.2.1-2ubuntu0.1
git             1:2.43.0-1ubuntu7.3
gzip            1.12-1ubuntu3.2
lld             1:18.0-59~exp2
make            4.3-4.1build2
ninja-build     1.11.1-2
python3-yaml    6.0.1-2build2
unzip           6.0-28ubuntu4.1
zip             3.0-13ubuntu0.2
```

Exact setup command:

```powershell
wsl.exe -d Ubuntu-24.04 -- bash -lc `
  "export DEBIAN_FRONTEND=noninteractive; apt-get update -qq && apt-get install -y -qq clang cmake ninja-build make build-essential git python3-yaml bzip2 gzip unzip zip gawk lld"
```

SynthXEX was built at its buildscript-pinned version and installed at
`/opt/openxechain-packer-v0.0.6/bin/synthxex`:

```bash
git clone --branch v0.0.6 --depth 1 \
  https://github.com/OpenXeChain/SynthXEX.git \
  /opt/auroraaz-synthxex-v0.0.6

cmake -S /opt/auroraaz-synthxex-v0.0.6 \
  -B /opt/auroraaz-synthxex-v0.0.6/build \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/opt/openxechain-packer-v0.0.6

cmake --build /opt/auroraaz-synthxex-v0.0.6/build --parallel 2
cmake --install /opt/auroraaz-synthxex-v0.0.6/build
```

`synthxex --version` reports `v0.0.6`; `synthxex --help` lists `sysdll`.
The source plus build tree occupies about 1.2 MiB and the installed packager
about 60 KiB.

As a compatibility probe, SynthXEX validated the extracted stock
`original/Aurora.exe` as a PE and read its basic headers, but exited `255` while
reading its XDK-generated import table:

```text
SynthXEX> PE valid!
SynthXEX> Got header data from PE!
SynthXEX> Retrieving import data from PE...
SynthXEX> ERROR: Invalid RVA or offset found. Aborting.
```

The latest SynthXEX main commit
`4bda05e21e3f6384ac447f8db3103a0a15b96887` was also built and produced the
same result. This does not show that OpenXeChain-generated PE files fail; it
shows that repacking Aurora's XDK PE is not a substitute for building a plugin.

Ubuntu's stock Clang is also not a substitute. This command emits an
`elf32-powerpc` object even with a Windows-looking triple:

```bash
clang --target=powerpc-pc-windows-msvc \
  -ffreestanding -nostdlib -c test.c -o test.obj
```

Stock `lld-link-18 /machine:ppc` then fails with `unknown /machine argument:
ppc`. OpenXeChain's patched LLVM/LLD is required.

## Disk constraint and prebuilt check

The full OpenXeChain LLVM repository reports a GitHub repository size of
1,680,790 KiB before a build tree. The host currently has less than 9 GiB free
on `C:` and less than 1 GiB free on `D:`. A local LLVM source build was not
started because its transient source, object, and install footprint could fill
the host volume.

The primary OpenXeChain repositories currently publish no GitHub Releases and
no GitHub Actions artifacts for the buildscript or LLVM repository. No
verified prebuilt compiler was found. Third-party binaries were deliberately
not substituted.

Installing the WSL prerequisites changed the WSL root filesystem from roughly
1.5 GiB used to 3.23 GB used. `/usr/lib/llvm-18` occupies about 657 MiB and the
APT archive cache about 241 MiB at the time of measurement.

## Current gate

The compiler, packager, corrected XEX shape, and Aurora wrapper resolution are
operational on retail hardware. The remaining M1 question is why no
AuroraAZ-owned log or resident thread was observed after Aurora reported the
module loaded. The next build must remain a no-hook observation canary and must
be tested only through `Hdd1:\AuroraAZLab\`.

Before calling the runtime usable:

1. preserve the now-passing module flags, image-base header, TLS omission, and
   exports 2-5;
2. add one minimal, non-recursive signal that distinguishes AuroraAZ code
   execution from Aurora's wrapper notification;
3. repeat the isolated upload, round-trip hash, NOVA/log observation, and
   recoverable rollback procedure;
4. leave production Aurora and `launch.ini` untouched throughout.

Until that signal is observed, the correct statement is: the one-file XEX loads
and resolves on hardware, but AuroraAZ initialization is not yet proven.
