# Research notes

## RealModScene: “Aurora plugin patches” (2015)

Source: <https://www.realmodscene.com/index.php?/topic/5360-aurora-plugin-patches/>

Despite its title, this is **not** an SDK or an in-process plugin mechanism for
the Aurora dashboard. The original post describes a proof-of-concept Aurora
theme for the *Freestyle Plugin* HUD. It was made with XuiTool, XUI Workshop, a
hex editor, and a binary-diff utility, and it replaces three files under:

```text
Game:\Plugins\Hudscene
```

The patch only changes the HUD label and icon from Freestyle to Aurora.

### What it means for this project

- It confirms that compiled Xbox XUI assets can be modified and deployed as
  patches.
- It may be useful later if we choose to theme the Guide/HUD.
- It does **not** expose the Aurora dashboard’s game list or coverflow, so it
  cannot implement an A–Z selector by itself.

The correct initial target remains the Aurora skin's `Aurora_Main` scene plus
Aurora Lua filters. We will only investigate a binary patch after a skin/Lua
implementation proves insufficient for jump-to-letter behavior.

