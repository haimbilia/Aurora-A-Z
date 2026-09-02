# AuroraAZ lab deployment and test harness

`scripts/invoke-auroraaz-lab.ps1` provides the guarded hardware-test workflow
for the isolated `Hdd1:\AuroraAZLab` installation. It is deliberately unable
to transfer, rename, or remove any file under the production
`Hdd1:\Aurora` directory.

## One-time local setup

Use PowerShell 7 and install WinSCP. Create two saved sessions on the
development PC, both pointing to the intended test console:

- `Xbox 360`: credentials used by the production Aurora FTP server.
- `AuroraAZ Lab`: credentials used only by the AuroraAZLab FTP server.

Those are the harness defaults. Change them with `-ProductionFtpSession` and
`-LabFtpSession` if the saved names differ. A single saved session may be
passed for both only when both Aurora instances genuinely use the same FTP
credentials. The harness never accepts an FTP URL or raw FTP password;
credentials remain in WinSCP's local credential store.

`Deploy` uses the production session because production Aurora is still
running while the file is staged under the separate lab directory. `Collect`
uses the lab session. `Disable` tries the lab session first and then the
production session, which lets cleanup work on either side of a title change.

NOVA authentication is optional. When it is enabled, set credentials only in
the current process environment:

```powershell
$env:AURORAAZ_NOVA_URL = 'http://CONSOLE_IP:9999'
$env:AURORAAZ_NOVA_USERNAME = 'your-nova-user'
$env:AURORAAZ_NOVA_PASSWORD = 'your-nova-password'
```

Do not place those assignments in a committed script. NOVA's JWT and the
password stay in memory and are not included in result manifests.

## Inspect the plan without contacting the console

`Plan` is the default action. It validates the local artifact, prints the
fixed targets, and performs no network operation:

```powershell
.\scripts\invoke-auroraaz-lab.ps1 -Action Plan `
  -Artifact .\build\ci\artifact\AuroraAZ.xex
```

`-DryRun` gives the same no-network behavior for any other action.

## Fully automatic smoke test

Start with production Aurora running and NOVA loaded, then run:

```powershell
.\scripts\invoke-auroraaz-lab.ps1 -Action Test `
  -Artifact .\build\ci\artifact\AuroraAZ.xex
```

The harness performs this sequence:

1. Verify through NOVA that production Aurora is running.
2. Refuse to continue if
   `Hdd1:\AuroraAZLab\Plugins\NetDbgDll.xex` already exists.
3. Refuse a colliding stage name, then upload the candidate under the unique
   `.staged-<12-hex-hash>-<8-hex-nonce>` name.
4. Download it and require an exact SHA-256 match.
5. Rename the verified stage to the one active lab filename once, then confirm
   through a fresh listing that the source disappeared and destination exists.
6. Launch `AuroraAZLab\Aurora.xex` through NOVA and confirm its title path.
7. Download available named markers and `debug.log`, save NOVA title metadata,
   and capture a BMP through `capture-nova.ps1`.
8. Rename the active lab plugin to a unique
   `.disabled-<12-hex-hash>-<6-hex-nonce>` filename and verify the resulting
   remote state.
9. Launch production `Aurora\Aurora.xex` directly through NOVA and confirm that
   production Aurora is again running. If the transition only dropped NOVA,
   the harness can load NOVA through production FTP and recheck the title.

Steps 8 and 9 run from a `finally` block if the test itself fails. If the lab
FTP server is unavailable, disable is retried through production FTP after the
title restore. A failed restore or failed post-restore disable is a hard error
and must be resolved before another lab launch.

`SITE REBOOT` is not used by default. On the current test console that command
has powered the machine off instead of returning it to production. Only add
`-AllowSiteRebootFallback` when that behavior is understood and a reboot is
explicitly wanted:

```powershell
.\scripts\invoke-auroraaz-lab.ps1 -Action Test `
  -Artifact .\build\ci\artifact\AuroraAZ.xex `
  -AllowSiteRebootFallback
```

Evidence is stored beneath `build\lab-harness\<timestamp>-<id>`. That directory
is git-ignored. The JSON manifest contains paths, hashes, and outcomes, but no
credentials or JWT.

By default the collector looks for:

- `AuroraAZ-M2a-input.bin`
- `AuroraAZ-M2a.bin`
- `AuroraAZ-M1.bin`
- `AuroraAZ-M1-worker.bin`
- `debug.log`

Override the list with one or more `-EvidenceName` arguments. Only simple
filenames in `Hdd1:\AuroraAZLab\Data\Logs` are accepted.

## Interactive controller test

Use separate actions when the build must remain active while a person tests
R3, left/right, A, and RB:

```powershell
.\scripts\invoke-auroraaz-lab.ps1 -Action Deploy `
  -Artifact .\build\ci\artifact\AuroraAZ.xex
.\scripts\invoke-auroraaz-lab.ps1 -Action Launch
```

After the controller test, collect evidence and restore immediately:

```powershell
.\scripts\invoke-auroraaz-lab.ps1 -Action Collect
.\scripts\invoke-auroraaz-lab.ps1 -Action Restore
```

`Restore` first attempts the recoverable disable rename, launches production
through NOVA, verifies production, and retries the disable from production if
the lab FTP service was unavailable. It does not reboot unless the explicit
fallback switch is present.

## Safety invariants

- The only active plugin path accepted by the script is
  `/Hdd1/AuroraAZLab/Plugins/NetDbgDll.xex`.
- Staged and disabled names must share that exact basename and remain in the
  lab `Plugins` directory. Their exact grammars are allowlisted; each generated
  inactive name is exactly 42 characters, the FATX filename limit.
- Evidence downloads are limited to simple filenames directly inside
  `/Hdd1/AuroraAZLab/Data/Logs`; evidence names also honor the 42-character
  FATX limit.
- There is no FTP `rm` operation. Activation and disable are recoverable
  renames; failed hash verification leaves only an inactive staged file.
- Rename destinations are required to be absent before the one-shot command.
  A new listing must prove the expected source/destination state afterward;
  WinSCP's exit code alone is not treated as proof because FTP can disconnect
  after applying a rename.
- Read-only listings/downloads and the inactive staged upload retry transient
  Aurora FtpDll connection drops. Activation/disable renames are issued once;
  the cleanup path detects and handles their resulting active state.
- An existing active lab plugin is never overwritten or displaced. Use
  `-Action Disable` after identifying it, then deploy again.
- Production restoration normally uses NOVA's fixed title-launch target. FTP
  `SITE NOVA LOAD` is only a transition recovery step, and `SITE REBOOT` is an
  opt-in fallback. None performs a production file operation or edits
  DashLaunch or `launch.ini`.

## NOVA multipart contract

The `/title/launch` endpoint accepts the same three fields as the known-good
`curl -F` request: `exec=Aurora.xex`, the fixed device `path`, and `type=0`.
The harness sends them as `ByteArrayContent` parts with no per-part
`Content-Type`. This matters because the console rejected .NET
`StringContent` parts carrying `Content-Type: text/plain` with HTTP 400.

The parser, no-network action matrix, allowlist checks, FATX name lengths,
rename-state logic, and multipart wire shape can all be tested locally. Before
calling the harness fully live for the first time, the remaining console checks
are:

1. Confirm both saved WinSCP sessions connect to the expected title phases.
2. Run `-Action Launch` against production and require HTTP success plus the
   exact `AuroraAZLab\Aurora.xex` title report.
3. Run `-Action Restore` with no reboot fallback and require the exact
   production `Aurora\Aurora.xex` title report.
4. Perform one lab deploy/round-trip/disable cycle and inspect the resulting
   inactive 42-character filename. Do not use the reboot fallback on the
   current console.

## Recovery notes

If the console or network becomes unreachable during a test, boot production
Aurora normally. Then run `-Action Restore`. If that action reports that the
active lab plugin could not be disabled, use WinSCP only to rename the exact
file:

`Hdd1:\AuroraAZLab\Plugins\NetDbgDll.xex`

to a unique name no longer than 42 characters, for example
`NetDbgDll.xex.disabled-manual-000001` (36 characters). Do not relaunch the lab
until the active filename is absent.
