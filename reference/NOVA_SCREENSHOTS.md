# NOVA screenshot capture

AuroraAZ uses NOVA's HTTP API for repeatable hardware screenshots during
visual testing. The helper is [`scripts/capture-nova.ps1`](../scripts/capture-nova.ps1).

## Configure

Set the console URL in the current PowerShell process:

```powershell
$env:AURORAAZ_NOVA_URL = 'http://CONSOLE_IP:9999'
```

This console currently permits anonymous read access. If WebUI authentication
is enabled later, also set the credentials in environment variables:

```powershell
$env:AURORAAZ_NOVA_USERNAME = 'your-webui-user'
$env:AURORAAZ_NOVA_PASSWORD = 'your-webui-password'
```

The helper exchanges those credentials for a JWT using `POST /authenticate`.
The JWT is kept in memory and is not printed or saved. NOVA 0.7b.2 serves HTTP
without transport encryption by default, so keep port 9999 on the trusted LAN.

## Capture and download

```powershell
.\scripts\capture-nova.ps1
```

The default output directory is `build/nova/`, which is ignored by Git. NOVA's
`GET /screencapture/meta` endpoint creates a screenshot entry on the console;
the helper deliberately does not delete it.

To choose a fixed output path:

```powershell
.\scripts\capture-nova.ps1 -OutputPath .\build\nova\milestone-m3.bmp
```

The helper refuses to overwrite an existing file.

## Read-only connection check

Download the newest existing capture without creating or deleting anything:

```powershell
.\scripts\capture-nova.ps1 -LatestExisting
```

This mode calls `GET /screencapture/meta/list` followed by
`GET /image/screencapture?uuid=...`. A successful download is validated for a
BMP signature before it is written.

## NOVA 0.7b.2 endpoint sequence

| Purpose | Method and endpoint | Console state |
| --- | --- | --- |
| Optional authentication | `POST /authenticate` | Issues an in-memory JWT |
| Take capture | `GET /screencapture/meta` | Adds a screenshot entry |
| List existing captures | `GET /screencapture/meta/list` | Read-only |
| Download BMP | `GET /image/screencapture?uuid=...` | Read-only |

Do not automate `DELETE /screencapture` as part of test capture. Retention can
be managed deliberately after the captured evidence has been reviewed.
