# Skin source

The stock Aurora 0.7b.2 skin is kept under ignored `original/` and `stock/`
folders. Source-controlled PowerShell patches transform the recovered XUI into
the Aurora A-Z skin without redistributing unmodified Aurora source assets.

`patches/add-alphabet-row.ps1` currently inserts the visual selector immediately
after `CoverflowWrapper` in `Aurora_Main`, at Y=532 above the stock title panel.
