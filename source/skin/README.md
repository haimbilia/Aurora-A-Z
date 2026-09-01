# Skin source

This folder will hold the editable sources extracted from the user's exact
Aurora Default skin, beginning with `Aurora_Main.xui`.

The skin has not been added yet because Aurora `.xur` files are binary and the
correct scene/control names vary by Aurora skin version. Adding guessed XUI
would make a package that is likely to fail to load.

After `original/Default.xzp` is available:

1. Extract it without overwriting `original/Default.xzp`.
2. Locate `Aurora_Main.xur` and export it to `Aurora_Main.xui` in XuiTool.
3. Place the exported `.xui` here.
4. We will add an inactive visual alphabet row beneath the coverflow, then
   compile and verify it on the Xbox.

