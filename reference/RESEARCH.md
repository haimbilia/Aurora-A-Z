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

Hardware testing established that the Aurora skin plus content-filter Lua is
not sufficient for the complete interaction. The skin can draw the row and Lua
can define predicates, but the stock `ScnApplication` native class owns
coverflow input. Changing the hidden `QuickViewRB` control's `PressKey` did not
intercept D-pad Down, and the native RB path opens the separate QuickView menu.

The production implementation is required to be skin agnostic. It must provide
an input/filter bridge that owns selector state without replacing the normal RB
QuickView behavior or modifying any `.xzp` package. The selected direction is a
version-gated runtime module that injects its own top-level XUI scene or renders
an overlay, plus narrowly scoped in-memory hooks for controller input and
filter application. A static skin row, a patched skin, or a restyled QuickView
menu is not sufficient.
