# Rev1655 overlay implementation

Date: 2026-09-02

## Decision

For Rev1655, draw Aurora A-Z immediately before Aurora's resident ATG font
`End()` call, not on return from `GameContentManager::RenderMenu`.

The final application render pass has already rendered XUI and the selected
skin at that point. Aurora has also opened its own font graphics-state bracket,
and the frame has not yet been resolved. The exact sequence is:

```text
8221180C  bctrl                    ; render the XUI/root scene
...
82211830  mr      r3,r30           ; r30 = application + 0xE0 (ATG Font)
82211838  bl      0x8247E0C0       ; ATG Font::Begin
8221183C  mr      r3,r30
82211840  bl      0x8247E390       ; ATG Font::End
...
82211870  bl      0x827787D8       ; resolve the completed 1280x720 pass
```

Detour `0x8247E390`, render before invoking its original trampoline, and then
let the original `End()` perform Aurora's normal cleanup. There is exactly one
direct caller of `0x8247E0C0` and exactly one direct caller of `0x8247E390` in
this image, at `0x82211838` and `0x82211840` respectively.

Keep the `GameContentManager::RenderMenu` detour, but use it only to produce a
per-frame coverflow visibility token. Rendering directly from its return hook
is no longer the preferred path: `RenderMenu` produces and resolves an
off-screen coverflow resource, while later XUI/skin work can still cover a
primitive drawn at that return site.

This design remains one installed file, `AuroraAZ.xex`. The Roboto Light atlas,
glyph rectangles, hook signatures, and all selector state live in that XEX.
It reads no selected-skin font, XUR, XZP, element ID, or external plugin asset.
It reuses only the resident Rev1655 font shaders/state bracket that Aurora has
already initialized as part of its stock core render path.

## Evidence labels

- **Proven**: observed in the exact Rev1655 executable through code, relocation,
  data, or a unique instruction window.
- **Strong inference**: follows from the decoded implementation and normal
  Xbox 360 D3D semantics, but still needs the first hardware pixel canary.
- **Hardware gate**: must be confirmed through NOVA before it is a release
  dependency.

All addresses below apply only to the binaries already allowlisted in
`NATIVE_HOOKS.md`:

| Artifact | SHA-256 |
| --- | --- |
| `original/Aurora.xex` | `583BCD442D8017D6FCB2645B93CDA987F4C0A43A688B652D7364CCAEDAEEFA9F` |
| `original/Aurora.exe` | `5BB5BAF8DF4CCB197241B34935EB400F36C8C20648CC074E2C30FA80ADD37E3C` |

## Hook topology

Use two hooks with a small shared state object:

```c
struct AzOverlayRuntime {
    volatile uint32_t render_menu_epoch;
    uint32_t drawn_epoch;
    void *last_game_content_manager;
    void *atlas_texture;
    void *atlas_device;
    uint8_t texture_ready;
    uint8_t unloading;
};
```

### Coverflow marker

Detour `0x82358A08` with the recovered prototype:

```c
typedef int32_t (*AzRenderMenuFn)(void *game_content_manager);
```

Call the trampoline first. Only when it returns `0`, the manager's render byte
at `+0x225D` is nonzero, and the three resources at `+0x2224`, `+0x222C`, and
`+0x2234` are non-null, cache the manager and increment
`render_menu_epoch`. Do not draw or allocate in this hook.

### Final-composite draw hook

Detour the entry of `0x8247E390`:

```c
typedef void (*AzAtgFontEndFn)(void *font);
```

The hook must always invoke the original trampoline exactly once. Before doing
so, draw only when all of these checks pass:

1. the saved caller LR is `0x82211844`;
2. `font != NULL` and `*(uint32_t *)(font + 0xB4) == 1`;
3. `*(uint32_t *)(font + 0xB8) != 0`;
4. `*(void **)0x82BC6BD8` is non-null;
5. `render_menu_epoch != drawn_epoch`;
6. no system UI or proven Aurora modal-scene gate is active;
7. the plugin is not unloading.

Set `drawn_epoch = render_menu_epoch` even if lazy texture creation fails. That
prevents a failed resource allocation from being retried every render frame;
retry only after a bounded cooldown or device-pointer change.

The caller-LR test matters even though there is only one recovered direct
caller. It makes an unexpected indirect use of `Font::End()` fail closed.

The entry detour must preserve the original LR before calling C code. A normal
C wrapper alone cannot inspect the original caller after its own prologue has
changed LR. Use the existing naked PPC entry shim to pass both `r3` and the
captured LR to the C renderer.

### Why an entry detour instead of rewriting the call

The plugin's planned `0x91D00000` image base is outside a PowerPC relative
branch's +/-32 MiB range from `Font::End()`. The runtime therefore reserves a
small executable title-memory arena between `0x82D50000` and `0x84340000`.
That range is reachable from the input, RenderMenu, and Font::End targets.

Install each hook with one aligned relative `b` instruction to a prepared near
relay. The relay contains the established absolute jump sequence:

```text
lis    r11, target_hi
ori    r11, r11, target_lo
mtctr  r11
bctr
```

`r11` and CTR are volatile under the Xbox PowerPC ABI. Because the live target
patch replaces only the first four-byte `mflr r12`, its nearby trampoline needs
only that displaced instruction followed by a relative branch to
`0x8247E394`. The relative call at `0x8247E394` remains in place. This makes the
live mutation a single compare-and-swap instead of a tear-prone four-word
rewrite. Build and flush the relay/trampoline before atomically publishing the
target branch; flush the target again after install or restore.

## Resident font pipeline available at the hook

Aurora constructs an ATG font object at application `+0xE0` from the stock
core resource `game:\\Media\\Fonts\\Arial_16.xpr`. The application render loop
passes that object to `Begin()` and `End()` as shown above.

`Begin()` at `0x8247E0C0` does all of the following before the overlay hook is
reached:

- saves the render, blend, depth, raster, and sampler states used by the ATG
  font path;
- binds the ATG vertex declaration at `*(void **)0x82BC455C`;
- binds the vertex shader at `*(void **)0x82BC4560`;
- binds the pixel shader at `*(void **)0x82BC4564`;
- binds the stock font texture at stage zero;
- increments the nested-begin count at font `+0xB4`.

The linked font shader source and vertex declaration are present in the exact
image. The recovered shader inputs are:

```hlsl
float2 Pos             : POSITION;
float2 Tex             : TEXCOORD0;
float4 ChannelSelector : TEXCOORD1;

uniform float4 Color   : register(c1);
uniform float2 TexScale: register(c2);
```

The pixel shader has two paths. A nonzero `ChannelSelector` selects one channel
from the stock packed font texture. An all-zero selector returns the complete
sampled texel multiplied by `Color`. The embedded Aurora A-Z atlas uses the
all-zero path, so it is independent of the stock font's glyph data.

The declaration definition contains elements at byte offsets `0`, `8`, and
`12`, followed by its end marker. Together with the shader inputs, this gives
the practical 16-byte vertex used by the overlay:

```c
typedef struct AzFontVertex {
    float x;
    float y;
    int16_t u;                 /* texel coordinate */
    int16_t v;                 /* texel coordinate */
    uint32_t channel_selector; /* zero for RGBA atlas */
} AzFontVertex;
```

The exact declaration bytes are:

```text
00 00 00 00 00 2C 23 A5 00 00 00 00
00 00 00 08 00 2C 22 59 00 05 00 00
00 00 00 0C 00 18 28 86 00 05 01 00
00 FF 00 00 FF FF FF FF 00 00 00 00
```

Calling Aurora's `DrawPrimitiveUP` wrapper with primitive value `5`, four
vertices, and a 16-byte stride is the preferred quad path. Existing Aurora
callers use that exact `(5, 4, data, 0x10)` pattern. The wrapper copies exactly
`vertex_count * stride` bytes into its transient vertex allocation, so the
four-vertex ABI is proven even though the friendly enum name
`D3DPT_TRIANGLESTRIP` is a strong inference.

## Embedded atlas upload

The current embedded artifact is a deterministic 1024x64 A8 Roboto Light
atlas. Its complete row advance is 922 pixels, its ink occupies atlas Y 20
through 53, and its baseline is atlas Y 48. Keep the A8 source plus per-glyph
crop table in `.rdata`; do not add a companion DDS/XPR file.

Create and upload the GPU texture lazily in the final-composite hook, on the
render thread, after Aurora's device exists. Do not create it from `DllMain` or
the system-monitor thread.

Aurora itself uses this exact working sequence for CPU-written 32-bit textures:

```c
texture = create_texture(width, height, 1, 1, 0,
                         0x18280086, 1, 3);
lock_texture(texture, 0, &locked, NULL, 0);
/* write rows using locked.Pitch */
unlock_texture(texture, 0);
```

`0x18280086` is strongly identified as Aurora's 32-bit A8R8G8B8 texture format:
the observed caller writes `width * 4` bytes per row. Use that known format
instead of guessing the Xbox value for a native A8 texture. Expand each source
alpha byte once during upload:

```c
uint32_t pixel = ((uint32_t)alpha << 24) | 0x00FFFFFFu;
```

The CPU is big-endian; write aligned 32-bit words, not manually reordered byte
lanes. Respect `locked.Pitch` for every row. The resulting white RGB plus atlas
alpha works with the resident font pixel shader and its normal source-alpha
blend setup.

Use this minimal local lock structure:

```c
typedef struct AzLockedRect {
    uint32_t pitch;
    void *bits;
} AzLockedRect;
```

If the device pointer at `0x82BC6BD8` changes, stop drawing, retire the old
texture on the render thread, and recreate it against the new device context.

## Exact callable routines

These are direct, statically linked routines in the exact Aurora image. They
are not import-table APIs and must never be called before the Rev1655 identity
and instruction windows pass.

```c
typedef void *(*AzCreateTextureFn)(
    uint32_t width, uint32_t height,
    uint32_t arg5, uint32_t arg6, uint32_t arg7, uint32_t format,
    uint32_t arg9, uint32_t arg10);

typedef int32_t (*AzLockTextureFn)(
    void *texture, uint32_t level, AzLockedRect *locked,
    const void *rect, uint32_t flags);

typedef int32_t (*AzUnlockTextureFn)(void *texture, uint32_t level);

typedef void (*AzSetTextureFn)(
    void *device, uint32_t stage, void *texture, uint32_t fetch_flags);

typedef void (*AzSetVertexShaderConstantFFn)(
    void *device, uint32_t start_register,
    const float *vectors, uint32_t vector4_count,
    uint64_t dirty_block_mask);

typedef void (*AzDrawPrimitiveUpFn)(
    void *device, uint32_t primitive_type, uint32_t vertex_count,
    const void *vertices, uint32_t stride);

typedef uint32_t (*AzReleaseResourceFn)(void *resource);
```

| Address | Practical role | Confidence |
| ---: | --- | --- |
| `0x82772300` | create texture; returns the resource pointer or null | proven ABI, argument meanings partly inferred |
| `0x827722E0` | texture `LockRect` forwarding wrapper; validate its output structure, not `r3` | proven |
| `0x827706B8` | texture `UnlockRect` forwarding wrapper; `r3` is unspecified | proven |
| `0x82772680` | bind texture/fetch at a stage; use flags `0x80000000` | proven |
| `0x8277EC20` | copy `vector4_count` VS constants beginning at `start_register`; `r7` is the 64-bit dirty-block mask | proven |
| `0x82784520` | transient `DrawPrimitiveUP` wrapper | proven |
| `0x82779DE0` | release a D3D resource | proven |

The names of `arg5`, `arg6`, `arg7`, `arg9`, and `arg10` in the creation ABI
are deliberately not invented. The call tuple shown above is copied from a
successful Aurora CPU-written texture path at `0x826D9FCC..0x826DA06C`.

## Quad rendering

At 1280x720, use atlas origin X `179` and Y `549` initially. The 922-pixel ink
advance therefore ends at X `1101` and is centered exactly on X `640`; the
remaining transparent part of the 1024-pixel texture extends to X `1203`.
Atlas ink appears at screen Y `569..602`, with baseline Y `597`. These constants
are within a few pixels of the visual contract and should be tuned only from a
NOVA comparison.

The ATG font path disables `D3DRS_VIEWPORTENABLE`. Its vertex shader applies
only the half-pixel correction `In.Pos.xy - 0.5`, matching ATG's own DrawText
implementation, which writes raw screen-pixel positions. Submit logical pixel
edges directly:

```c
vx = pixel_x;
vy = pixel_y;
```

Do not normalize to clip space or invert Y. Doing so compresses the complete
922x33 row to roughly a single screen pixel in this disabled-viewport path.

For the full atlas quad, submit vertices in triangle-strip order:

```text
top-left      screen (179, 549),  texel (   0,  0)
top-right     screen (1203,549),  texel (1024,  0)
bottom-right  screen (1203,613),  texel (1024, 64)
bottom-left   screen (179, 613),  texel (   0, 64)
```

This clockwise perimeter order is copied from Aurora's native
`(5, 4, vertices, 16)` quad caller at `0x822D5944..0x822D59F8`. The alternate
`TL, TR, BL, BR` ordering renders only one triangular half in this path.

Before drawing, bind the atlas and set the two recovered shader constants:

```c
set_texture(device, 0, atlas_texture, 0x80000000u);
set_vs_constant_f(device, 2,
                  (float[4]){ 1.0f / 1024.0f, 1.0f / 64.0f, 0, 0 }, 1,
                  0x8000000000000000ull);
set_vs_constant_f(device, 1, rgba, 1, 0x8000000000000000ull);
draw_primitive_up(device, 5, 4, vertices, 16);
```

Registers c1 and c2 are both in vertex-constant dirty block zero. The final
64-bit mask is mandatory: the low-level routine copies constant memory but
does not derive or publish the GPU dirty bit itself.

The complete row needs at most three draws:

1. draw the full row at `(+2,+2)` in translucent black for the initial hard
   shadow;
2. draw the full row at its exact origin in inactive light gray/white;
3. while selector mode is active, redraw only the selected glyph's atlas crop
   at the identical screen position in full white.

Crop the selected sub-quad with one or two transparent pixels on each side so
Roboto's antialiasing is not cut off. Do not change glyph positions or row
spacing when selection changes. Index zero is `#`; indices 1 through 26 are
`A` through `Z`.

Suggested first-pass shader colors are expressed as float RGBA because the
shader constant is a `float4`:

```text
shadow    = {0.00, 0.00, 0.00, 0.45}
inactive  = {0.88, 0.88, 0.88, 0.92}
selected  = {1.00, 1.00, 1.00, 1.00}
```

The hard two-pixel shadow is deliberately the interaction milestone. A later
fidelity pass may replace it with a small multi-tap soft shadow without
changing the hook or texture path.

## State restoration contract

Do not call `Font::Begin()` from the RenderMenu hook. At the chosen site it is
already active, and the original `Font::End()` must remain the sole owner of
balancing it.

The resident `Begin()` captures and the resident `End()` restores the state
families below:

| Saved by `Begin()` | Restored by `End()` |
| ---: | ---: |
| `0x82774E38` | `0x82774DB8` |
| `0x82774F58` | `0x82774ED8` |
| `0x82774FE8` | `0x82774F68` |
| `0x82774EC8` | `0x82774E48` |
| `0x82774DA8` | `0x82774D80` |
| `0x82775218` | `0x827751E0` |
| `0x82775260` | `0x82775240` |
| `0x82774D70` | `0x82774D50` |
| `0x82774D40` | `0x82774D20` |
| `0x82775480` | `0x82775448` |
| `0x82775528` | `0x827754F0` |
| `0x82775F90` | `0x82775F50` |
| `0x82776688(device,0)` | `0x827765C0(device,0,value)` |
| `0x82776830(device,0)` | `0x82776768(device,0,value)` |
| `0x82776D70(device,0)` | restored in device `+0x480` |
| `0x82776DC0(device,0)` | restored in device `+0x480` |

`End()` also unbinds stage-zero texture, vertex declaration, vertex shader,
and pixel shader in the same way Aurora already expects at this final pass.
`0x82784520` uses Aurora's transient upload path rather than installing a
persistent stream source. Consequently the overlay should touch only:

- stage-zero texture;
- vertex constants c1 and c2;
- the transient DrawPrimitiveUP allocation.

Do not set blend, depth, raster, sampler, viewport, scissor, render-target,
declaration, or shader state in the overlay. The host bracket has already
configured the needed font states. This narrow write set is the principal
safety advantage of the chosen renderer.

The ATG path intentionally writes font constants without restoring them; the
original frame immediately resolves after `End()`. Do not introduce a guessed
constant getter merely to restore c1/c2. If a hardware test finds cross-frame
leakage, recover and validate the matching getter before adding it.

## Modal and visibility gate

The RenderMenu epoch proves that coverflow rendered during the current XUI
pass. It does not prove that no Aurora modal was rendered over it. Therefore:

- always suppress when `XamIsUIActive()` reports the Guide/system UI;
- R3, Left/Right, and A may affect selector state only under the same current
  epoch gate;
- RB remains untouched;
- QuickView, Details, Settings, message boxes, and the Guide must be captured
  with NOVA before release;
- if QuickView still advances the epoch behind its modal, add a proven active
  scene/modal predicate. Do not ship an LR, timer, or color-sampling heuristic.

For an interaction canary only, observing RB to clear selector focus and
temporarily suppress drawing is acceptable, provided RB itself is never
consumed. It is not the final modal-scene gate.

## Resource and unload rules

- Lazy initialization occurs once on the render thread.
- No allocation, texture upload, screenshot HTTP request, or file I/O occurs
  per frame.
- Remove/disable the render hooks and wait for an in-flight hook to leave
  before releasing the atlas.
- Release the texture through `0x82779DE0` on the render thread. Never release
  Aurora's font texture or shader globals.
- If render-thread cleanup cannot be scheduled safely during process teardown,
  leaking one 1024x64 texture until Aurora exits is safer than racing the GPU.
- Restore original instructions and invalidate the instruction cache before
  allowing `AuroraAZ.xex` to unload.

## Required byte guards

The image hash remains the primary allowlist. Validate these windows as a
second line of defense before installing hooks or calling internal routines:

| Purpose | VA | Required bytes |
| --- | ---: | --- |
| final Begin/End call context | `0x82211838` | `48 26 C8 89 7F C3 F3 78 48 26 CB 51 81 7F 00 BC 39 40 00 00` |
| `RenderMenu` entry | `0x82358A08` | `7D 88 02 A6 48 60 F2 BD DB C1 FF C8 DB E1 FF D0` |
| ATG shader initializer | `0x8247DD10` | `7D 88 02 A6 48 4E 9F B1 94 21 FF 60 3D 60 82 BC` |
| ATG `Begin()` | `0x8247E0C0` | `7D 88 02 A6 48 4E 9C 09 DB A1 FF C8 DB C1 FF D0` |
| ATG `End()` hook | `0x8247E390` | `7D 88 02 A6 48 4E 99 35 94 21 FF 80 81 63 00 B4` |
| texture unlock | `0x827706B8` | `81 63 00 30 81 43 00 20 55 65 00 26 55 44 00 26 48 00 86 58` |
| texture lock | `0x827722E0` | `7C E9 3B 78 7C C8 33 78 7C A7 2B 78 7C 86 23 78` |
| texture create | `0x82772300` | `7D 88 02 A6 48 1F 59 AD 94 21 FF 20 7C 7C 1B 78` |
| stage texture bind | `0x82772680` | `7D 88 02 A6 48 1F 56 25 94 21 FF 40 39 64 0C 9E` |
| VS constants | `0x8277EC20` | `39 44 00 78 7C AB 2B 78 55 4A 20 36 7C C9 33 78` |
| transient draw | `0x82784520` | `7D 88 02 A6 48 1E 37 A5 94 21 FF 80 7C DC 33 78` |

The 16-byte unlock prefix occurs twice in the image, so its listed 20-byte
window is mandatory. The other listed windows are unique in the extracted PE.

## Hardware bring-up order

1. Install both detours but only log one `RenderMenu` epoch and one matching
   `Font::End` event. Confirm caller LR `0x82211844` and nested count `1`.
2. Lazily create/upload the 1024x64 texture, then draw a 32x16 crop at screen
   center. Confirm alpha, orientation, and the normalized-position convention.
3. Draw the full inactive row at X `179`, Y `549`; capture through NOVA and
   compare its ink bounds and baseline to `VISUAL_SPEC.md`.
4. Add the two-pixel shadow.
5. Enter selector mode at `#`, move to a middle letter and `Z`, and redraw the
   selected crop at full white without moving the base row.
6. Capture Default, CleanNXE, and two structurally different skins.
7. Open QuickView with RB, then Details, Settings, Guide, and a message box.
   The overlay must be absent in all of them before the build can be called
   skin agnostic.
8. Run a sustained navigation/skin-change test and check that coverflow,
   dialogs, and subsequent frames show no texture, shader, or blend-state
   leakage.

The first failure must disable only Aurora A-Z and still call the original
`Font::End()`. It must never skip Aurora's cleanup or final resolve.
