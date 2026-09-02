# Roboto attribution

Aurora A-Z embeds a raster alpha atlas generated from **Roboto Light 3.016**.
It does not ship the source TTF in the production payload.

- Upstream: <https://github.com/googlefonts/roboto-3-classic>
- Release: `v3.016`
- Release asset: `Roboto_v3.016.zip`
- Release asset SHA-256:
  `1653DBE12F248DA8FB0B9920DB7B9496CD677ED3981154F6F15285C8BD4E334F`
- Input font: `hinted/static/Roboto-Light.ttf`
- Input font SHA-256:
  `59123D9F5A81091626FB1B37C583510A85DB1296AB794B48309AAAD0410232ED`
- Upstream source commit:
  `5166f3d07889bf7d3732fb72e09623d7e52f862b`

The font is licensed under the SIL Open Font License 1.1. See
[`OFL.txt`](OFL.txt).

Regenerate the atlas with:

```powershell
python -m pip install -r scripts\requirements-glyph-atlas.txt
python scripts/generate-glyph-atlas.py `
  --font path\to\Roboto-Light.ttf `
  --header native\include\auroraaz\glyph_atlas.h `
  --source native\src\glyph_atlas.c `
  --preview build\glyph-atlas-preview.png
```
