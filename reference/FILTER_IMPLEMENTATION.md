# Rev1655 in-memory A-Z filter implementation

Status: offline reverse engineering is sufficient for an **inert canary and a
gated hardware apply canary**. It is not yet sufficient to ship the apply path
enabled by default. The remaining release gate is the thread/queue timing check
in [Hardware gates](#hardware-gates); no console mutation was performed while
producing this document.

This document applies only to the checked Aurora 0.7b.2 Rev1655 binaries:

| File | SHA-256 |
| --- | --- |
| `original/Aurora.xex` | `583BCD442D8017D6FCB2645B93CDA987F4C0A43A688B652D7364CCAEDAEEFA9F` |
| extracted `original/Aurora.exe` | `5BB5BAF8DF4CCB197241B34935EB400F36C8C20648CC074E2C30FA80ADD37E3C` |

The hash allowlist is mandatory. Instruction signatures are a second check,
not a way to support an unknown Aurora revision.

## Decision

Do **not** put a raw identifier such as `NameFilter.A - F.A` in
`Work74+0x30`. That member is already-compiled Lua. A raw dotted identifier in
that slot is invalid Lua and replacing the member would also discard the
current QuickView filter.

The normal A-Z path is:

1. Deep-copy Aurora's current active state with the same aggregate-copy helper
   used by the stock additional-filter UI.
2. Preserve the current QuickView, sort, source, Favorites/Hidden flags, search
   text, and existing additional filters.
3. Replace exactly one existing canonical `NameFilter` predicate, or append the
   selected canonical identifier to `Work74+0x68` when none exists.
4. Submit through Aurora's asynchronous filter queue with flag `0x08`, the
   exact flag used by the stock additional-filter path.
5. Destroy the local snapshot after the scheduler returns. The scheduler has
   already deep-copied it.

`0x10` is the stock **QuickView-change** flag. A-Z is an additional filter over
the current QuickView, so `0x08` is the faithful choice.

## Proven object layouts

### `Work74`

The transient work object is `0x74` bytes:

| Offset | Size | Proven use |
| ---: | ---: | --- |
| `+0x00` | 4 | `includeHidden`; when zero the builder adds `not __Aurora__filter_Hidden(Content)` |
| `+0x04` | 4 | `favoritesOnly`; when one the builder adds `__Aurora__filter_Favorites(Content)` |
| `+0x08` | 4 | sort behavior/direction flag; copied from QuickView flag `0x00010000` |
| `+0x0C` | 4 | content/source context ID |
| `+0x10` | 4 | QuickView ID |
| `+0x14` | `0x1C` | Aurora narrow string: SortMethod |
| `+0x30` | `0x1C` | Aurora narrow string: compiled QuickView Lua filter expression |
| `+0x4C` | `0x1C` | Aurora narrow string: search text |
| `+0x68` | `0x0C` | `vector<AuroraString>` of additive dotted filter identifiers: begin/end/capacity |

Evidence:

- `0x8226A608` constructs the three strings and zeroes the vector.
- `0x8226A570` copies the five scalars, deep-assigns all three strings, and
  deep-assigns the vector through `0x822720C8`.
- `0x8222B7C8` destroys the vector, then the strings at `+0x4C`, `+0x30`, and
  `+0x14`.
- `0x823565E4..0x82356614` passes `+0x04`, `+0x00`, `+0x30`, `+0x4C`, and
  `+0x68` to the composite builder in that order.

The constructor does not initialize the five leading scalars. Construct a
destination and immediately deep-copy a valid source before reading it.

### Current-state aggregate

Aurora groups `Work74` and its dispatch context in a `0xD0`-byte aggregate:

```text
+0x00  Work74
+0x74  padding/other state
+0x78  AuroraString context[0]
+0x94  AuroraString context[1]
+0xB0  additional scalar state
+0xC0  reference-counted state
+0xC8  reference-counted state
+0xD0  end
```

`0x8222B3E8(out, gcm + 0x60, 0)` copy-constructs the active aggregate whose
source begins at `gcm+0x68`. Passing one selects the staging aggregate instead
and is not correct for A-Z. Destroy a successfully constructed snapshot with
`0x8222B6C8`.

This is a stock pattern, not a guessed layout. The additional-filter handler at
`0x822E5838` uses `0x8222B3E8(..., gcm+0x60, 0)` to capture the current context,
then calls the asynchronous scheduler at `0x822E589C` with flag `0x08`.
`0x822E5A60..0x822E5AAC` separately uses the same active snapshot to preserve
the compiled QuickView filter, context/QuickView IDs, and search text while it
builds a new additional-filter work item.

### Aurora narrow string

The narrow string object is `0x1C` bytes:

```text
+0x00  inline character storage, or heap pointer
+0x10  length
+0x14  capacity
```

Capacity below `0x10` means the characters are inline. Capacity at least
`0x10` means `+0x00` is a heap pointer. Never `memcpy` one of these objects and
never free its buffer with the plugin's allocator.

Relevant host helpers:

```cpp
AuroraString* StringConstructCString(AuroraString* dst, const char* src); // 0x82212CE8
AuroraString* StringAssignBytes(AuroraString* dst, const char* src,
                                uint32_t length);                         // 0x82212DB0
AuroraString* StringCopyConstruct(AuroraString* dst,
                                  const AuroraString* src);              // 0x82212D58
AuroraString* StringAssign(AuroraString* dst, const AuroraString* src,
                           uint32_t pos, uint32_t count);                 // 0x82212FA0
bool StringEqualsCString(const AuroraString* value, const char* rhs);     // 0x82211AF0
void StringLifecycle(AuroraString* value, uint32_t freeHeap,
                     uint32_t sizeArg);                                  // 0x82213580
```

Default construction uses `StringLifecycle(value, 0, 0)`; destruction uses
`StringLifecycle(value, 1, 0)`. A full deep assignment uses `pos=0` and
`count=0xFFFFFFFF`.

`0x822A6228(vector, string)` is the host `vector<AuroraString>::push_back`
path. It handles source aliasing and capacity growth before copy-constructing
the `0x1C`-byte element. Use it instead of manipulating vector storage.

## QuickView retrieval and compilation

The relevant `CQuickViewSettingObject` members are:

| Offset | Meaning |
| ---: | --- |
| `+0x00` | QuickView ID |
| `+0x04` | wide display name |
| `+0x3C` | narrow SortMethod |
| `+0x58` | raw database FilterMethod grammar |
| `+0x74` | flags |
| `+0xA0` | compiled Lua filter expression |

This corrects the reversed `+0x3C`/`+0xA0` description currently present in
`NATIVE_HOOKS.md`.

`0x823C4E60` copies the constructor's sort argument to `+0x3C` and raw filter
argument to `+0x58`. `0x823C2DC0` then:

- splits the raw filter on `;`;
- recognizes `(`, `)`, `AND`, `OR`, and `NOT`;
- validates each filter token with `0x82324C60(registry, 0, token)`;
- converts a dotted identifier into a Lua table lookup; and
- assigns the completed expression to QuickView `+0xA0` at `0x823C3288`.

For example, the raw database expression
`Xbox 360;AND;NOT;Kinect` becomes the equivalent of:

```lua
( GameListFilterCategories["Xbox 360"](Content)
  and not GameListFilterCategories["Kinect"](Content) )
```

`0x823567B0(gcm, context, work)` retrieves setting `DefaultQuickView`, retrieves
the `SettingQuickView` collection, looks up that ID through `0x823C3BD8`, and
falls back to QuickView 1 if necessary. It copies:

```text
QuickView+0x3C -> Work+0x14  SortMethod
QuickView+0xA0 -> Work+0x30  compiled filter
flags 0x10000 -> Work+0x08
flags 0x20000 -> Work+0x04
flags 0x40000 -> Work+0x00
```

It validates SortMethod as registry type 1 and falls back to literal
`"Title Name"`. It does not copy the raw database filter into `Work74`.

For A-Z, do not reconstruct state from `DefaultQuickView`: it can omit the
currently applied search and additional filters. The authoritative in-memory
snapshot is the active aggregate. Its `Work+0x10` is the most recently applied
QuickView ID.

## Composite-filter proof

Observed signature for `0x8235BDD8`:

```cpp
int BuildComposite(
    GameListManager* manager,                    // r3
    uint32_t favoritesOnly,                      // r4
    uint32_t includeHidden,                      // r5
    const AuroraString* compiledQuickViewFilter, // r6
    const AuroraString* searchText,              // r7
    const VectorAuroraString* extraFilterIds);   // r8
```

It builds predicates in this order:

1. search, if nonempty;
2. Favorites, when requested;
3. exclusion of Hidden, unless inclusion was requested;
4. the compiled QuickView expression, copied verbatim; and
5. each dotted identifier from the additive vector.

At `0x8235C1D4..0x8235C334`, an additive value such as:

```text
NameFilter.A - F.A
```

is converted to:

```lua
GameListFilterCategories["NameFilter"]["A - F"]["A"](Content)
```

At `0x8235C338..0x8235C3AC`, all predicates are joined with literal `" and "`
and wrapped as:

```lua
function __Aurora__filter_Composite(Content) return ( ... ) end
```

The result is assigned to `GameListManager+0x58`. This proves that `Work+0x68`
is the correct additive A-Z slot and that `Work+0x30` must remain compiled Lua.

## Exact replacement policy

The supported identifiers remain the 27 strings in `native/src/filters.c`;
`#` maps to `NameFilter.Other`. Validate the selected ID at runtime before any
apply:

```cpp
FilterRegistry* registry = RegistrySingleton();                 // 0x82271000
bool present = RegistryLookup(registry, 0, selectedIdentifier);  // 0x82324C60
```

The builder does not validate `Work+0x68` for the caller.

Classify both places where a current name predicate can exist:

- `Work+0x68` contains raw dotted identifiers.
- A custom QuickView can already contain a compiled NameFilter leaf in
  `Work+0x30`.

Use this fail-closed policy:

| Current state | Action |
| --- | --- |
| no NameFilter in either location | append selected raw ID to `+0x68` |
| exactly one canonical raw ID in `+0x68`, none in `+0x30` | deep-assign selected raw ID into that vector element |
| exactly one canonical compiled leaf in `+0x30`, none in `+0x68` | replace only that exact leaf in the compiled string |
| unknown `NameFilter` path, multiple leaves, or leaves in both locations | reject the apply and leave the current list untouched |

The exact compiled leaves are deterministic. For a canonical dotted ID, split
only on `.` and join the known-safe components as:

```text
GameListFilterCategories["component0"]["component1"]...(Content)
```

Examples:

```text
NameFilter.Other
-> GameListFilterCategories["NameFilter"]["Other"](Content)

NameFilter.A - F.A
-> GameListFilterCategories["NameFilter"]["A - F"]["A"](Content)
```

For compiled replacement, scan by the string's explicit length, require one
exact whole canonical leaf, rebuild into a bounded plugin-owned scratch buffer,
then call `0x82212DB0(Work+0x30, buffer, newLength)`. The helper deep-copies into
Aurora-owned storage; the plugin buffer is not retained. Preserve every byte
before and after the replaced leaf. Reject overflow or an invalid string/vector
invariant.

This handles a custom QuickView with one NameFilter without discarding its
other `AND`/`OR` terms. Ambiguous custom expressions are deliberately not
guessed at.

## Asynchronous dispatch ABI

Observed behavior of `0x823566D8`:

```cpp
int Dispatch(
    GameContentManager* gcm, // r3
    const Work74* work,      // r4
    const Context38* ctx,    // r5; two adjacent AuroraString objects
    uint32_t flag10,         // r6 == 1 adds 0x10
    uint32_t flag08,         // r7 == 1 adds 0x08
    uint32_t async);         // r8 == 1 queues the complete pipeline
```

When `async==1`, it calls:

```cpp
Schedule(gcm + 8, ctx, work,
         (flag10 == 1 ? 0x10 : 0) | (flag08 == 1 ? 0x08 : 0));
```

When `async!=1`, it only copies cached state and builds the composite. It does
not complete sort/filter/swap and must not be used for A-Z.

The wrapper also overwrites the scheduler result with zero. Call the scheduler
directly so allocation failure remains observable:

```cpp
int Schedule(void* queueAtGcmPlus8, const Context38* ctx,
             const Work74* work, uint32_t flags); // 0x82343628
```

`0x82343628` allocates a `0xB8` job, deep-copies context strings at job `+0x04`
and `+0x20`, deep-copies Work74 at job `+0x3C`, stores flags at `+0xB4`, and
enqueues under Aurora's queue mutex. It returns zero on success and one on an
allocation failure. The caller retains ownership of its objects.

The worker `0x82356588` caches the context/work, builds the composite, sorts,
filters, and submits event `0x102`. Aurora's event consumer performs the normal
active-list swap. Do not call the builder, sorter, filter evaluator, or swap
manually from the input/render hook.

The two decisive stock callsites are:

```text
0x822E589C  additional-filter apply -> Schedule(..., flags=0x08)
0x82359A30  QuickView change        -> Schedule(..., flags=0x10)
```

Use `0x08` for A-Z. The `0x08` event branch also preserves/reselects the prior
content when possible, matching an additional-filter change.

## Implementation pseudocode

All steps run only after hash/signature/readiness gates pass. Horizontal letter
movement never schedules work; only A does.

```cpp
bool ApplyAzLetter(uint8_t index)
{
    const char* selected = CanonicalNameFilter(index);
    GameContentManager* gcm = GcmSingleton();
    ActiveAggregateD0 snapshot;
    bool snapshotLive = false;
    AuroraString selectedHost;
    bool selectedLive = false;
    int rc = 1;

    if (!CoverflowIsInteractive() || FilterJobIsBusyOrQueued())
        return DeferLatestSelection(index);
    if (!RegistryLookup(RegistrySingleton(), 0, selected))
        return DisableAzFailClosed();

    // Copy constructor: snapshot was intentionally not pre-constructed.
    CopyActiveAggregate(&snapshot, (uint8_t*)gcm + 0x60, 0);
    snapshotLive = true;

    if (!ValidateWorkAndStrings(&snapshot.work))
        goto out;

    switch (ClassifyCanonicalNamePredicates(&snapshot.work)) {
    case NoNamePredicate:
        StringConstructCString(&selectedHost, selected);
        selectedLive = true;
        VectorPushBack(&snapshot.work.extraFilterIds, &selectedHost);
        break;

    case OneRawVectorPredicate:
        StringAssignBytes(foundVectorElement, selected, strlen(selected));
        break;

    case OneCompiledQuickViewPredicate:
        if (!ReplaceExactCompiledLeaf(&snapshot.work.compiledFilter, selected))
            goto out;
        break;

    default:
        goto out; // no mutation of live Aurora state
    }

    rc = Schedule((uint8_t*)gcm + 8,
                  (Context38*)((uint8_t*)&snapshot + 0x78),
                  &snapshot.work,
                  0x08);

out:
    if (selectedLive)
        StringLifecycle(&selectedHost, 1, 0);
    if (snapshotLive)
        DestroyActiveAggregate(&snapshot);
    return rc == 0;
}
```

Production code must use a single cleanup path like the above. It must not
destroy a partially constructed aggregate, and must always destroy a
successfully copied one even when classification or scheduling fails.

Only one apply may be in flight. Further R3-held selector sessions remain
unavailable until observed queue/busy activity ends and the queue stays idle
for 200 ms. If the short activity window is missed, retain the conservative
timeout before accepting stable idle; otherwise every release could queue
Aurora's measured multi-second sort.

## Hardware gates

### Stage 0: fail-closed binding

- Require both Rev1655 hashes.
- Require every callable helper signature below.
- Confirm the GCM singleton is initialized and active `Work+0x10` is nonzero.
- Confirm all string and vector invariants before dereferencing heap pointers.
- On any failure, leave the transient overlay hidden and do not claim R3.

### Stage 1: read-only registry and snapshot canary

- From the coverflow/UI thread, validate all 27 canonical names as registry
  type 0.
- Copy and destroy the active `0xD0` aggregate repeatedly without mutation or
  scheduling.
- Record only bounded metadata: thread ID, QuickView ID, string lengths,
  vector count, and pass/fail. Do not retain host pointers.
- Verify the canary runs on the same thread as stock additional-filter apply.

### Stage 2: inert ownership canary

- Copy the snapshot, replace/append each canonical ID locally, validate the
  resulting object, and destroy it without calling the scheduler.
- Exercise SSO and heap strings, vector growth, same-letter replacement, and
  every cleanup branch.
- Run enough iterations to expose leaks or double frees.

### Stage 3: one live apply

- On an otherwise stock `Show All` list, schedule A once with flag `0x08`.
- Require the normal log sequence: composite build, sort, filter, event
  `0x102`, active-list swap.
- Confirm the QuickView title and sort did not change and that A returns control
  immediately while work proceeds asynchronously.
- Capture before/after Nova screenshots and list counts; do not infer success
  only from the selected glyph.

### Stage 4: constraint and race matrix

- Test all stock QuickViews, Favorites, Hidden behavior, search text, and at
  least one unrelated additional category filter.
- Test `#` and A-Z, zero-result lists, rapid navigation, repeated A, opening RB
  filter UI during/after A-Z, and changing QuickView while a filter job runs.
- Confirm queued/busy detection. `gcm+0x2D8` is set only after the worker copies
  cached state and is cleared before the swap event; by itself it is not a
  complete queue lock.
- Hardware-trace whether the stock active-aggregate copy can overlap a worker
  when invoked from the plugin input hook. Until this is excluded, defer apply
  whenever Aurora is not demonstrably idle.
- Test a custom QuickView containing exactly one canonical NameFilter and
  verify byte-for-byte preservation of the rest of its compiled expression.
- Verify all ambiguous/malformed NameFilter cases reject without scheduling.

### Release gate

Apply remains canary-only until Stage 4 proves UI-thread affinity and a reliable
idle/queue condition. The offline binary proves the object and ownership ABI,
but it does not prove the runtime scheduling interleave. That is the remaining
release blocker.

## Rev1655 signatures

All byte windows below are exact and unique in the extracted Rev1655 `.text`.
Longer windows are used where common compiler-generated prologues collide.

| Purpose | VA | Required bytes |
| --- | ---: | --- |
| GCM singleton | `0x82223060` | `7D 88 02 A6 91 81 FF F8 FB E1 FF F0 3B E1 FF A0 94 21 FF A0 3D 40 82 BC 81 6A 05 7C` |
| copy active/staging aggregate selector | `0x8222B3E8` | `7D 88 02 A6 91 81 FF F8 FB E1 FF F0 94 21 FF A0 7C 7F 1B 78 2F 05 00 01 40 9A 00 0C 38 84 00 D8 48 00 00 08 38 84 00 08` |
| aggregate copy constructor | `0x8222B4B8` | `7D 88 02 A6 48 73 C8 09 3B E1 FF 70 94 21 FF 70` |
| aggregate destructor | `0x8222B6C8` | `7D 88 02 A6 48 73 C6 01 3B E1 FF 80 94 21 FF 80` |
| Work copy assignment | `0x8226A570` | `7D 88 02 A6 48 6F D7 59 94 21 FF 90 81 64 00 00` |
| Work constructor | `0x8226A608` | `7D 88 02 A6 91 81 FF F8 FB E1 FF F0 94 21 FF A0 7C 7F 1B 78 38 63 00 14` |
| Work destructor | `0x8222B7C8` | `7D 88 02 A6 91 81 FF F8 FB E1 FF F0 94 21 FF A0 7C 7F 1B 78 38 63 00 68 4B FF A5 D9` |
| string C-string constructor | `0x82212CE8` | `7D 88 02 A6 91 81 FF F8 FB C1 FF E8 FB E1 FF F0 94 21 FF 90 7C 9E 23 78 38 A0 00 00 38 80 00 00 7C 7F 1B 78 48 00 08 75` |
| string byte assignment | `0x82212DB0` | `7D 88 02 A6 48 75 4F 19 94 21 FF 90 7C 7F 1B 78` |
| string copy constructor | `0x82212D58` | `7D 88 02 A6 91 81 FF F8 FB C1 FF E8 FB E1 FF F0 94 21 FF 90 7C 9E 23 78 38 A0 00 00 38 80 00 00 7C 7F 1B 78 48 00 08 05` |
| string deep assignment | `0x82212FA0` | `7D 88 02 A6 48 75 4D 25 94 21 FF 80 81 64 00 10` |
| string/C-string equality | `0x82211AF0` | `7D 88 02 A6 91 81 FF F8 94 21 FF A0 7C 85 23 78 7C 8B 23 78` |
| string lifecycle | `0x82213580` | `7D 88 02 A6 48 75 47 49 94 21 FF 90 7C 7F 1B 78` |
| vector push-back | `0x822A6228` | `7D 88 02 A6 48 6C 1A A1 3B E1 FF 80 94 21 FF 80` |
| filter registry singleton | `0x82271000` | `7D 88 02 A6 91 81 FF F8 FB E1 FF F0 3B E1 FF A0 94 21 FF A0 3D 40 82 BD 81 6A 9E 44` |
| registry lookup | `0x82324C60` | `7D 88 02 A6 48 64 30 69 3B E1 FF 60 94 21 FF 60` |
| scheduler/deep-copy queue | `0x82343628` | `7D 88 02 A6 48 62 46 8D 94 21 FF 60 7C 7B 1B 78` |
| filter worker | `0x82356588` | `7D 88 02 A6 48 61 17 29 3B E1 FF 30 94 21 FF 30` |
| dispatch wrapper | `0x823566D8` | `7D 88 02 A6 48 61 15 E9 94 21 FF 80 7C 7D 1B 78` |
| QuickView translator | `0x823567B0` | `7D 88 02 A6 48 61 14 FD 3B E1 FE C0 94 21 FE C0` |
| composite builder | `0x8235BDD8` | `7D 88 02 A6 48 60 BE D5 3B E1 FB 30 94 21 FB 30` |
| QuickView filter compiler | `0x823C2DC0` | `7D 88 02 A6 48 5A 4E CD 3B E1 FC 00 94 21 FC 00` |
| stock additional-filter handler | `0x822E5838` | `7D 88 02 A6 91 81 FF F8 FB C1 FF E8 FB E1 FF F0 3B E1 FE 40 94 21 FE 40 7C 7E 1B 78 38 7F 00 50` |

## Useful exact literals

```text
0x8212E5A0  "DefaultQuickView"
0x8212E554  "SettingQuickView"
0x82135F18  "Title Name"
0x8213A5F4  "GameListFilterCategories"
0x8213A728  "__Aurora__filter_Favorites"
0x8213A744  "__Aurora__filter_Hidden"
0x8213A75C  "__Aurora__filter_Search"
0x8213EC94  "function __Aurora__filter_Composite(Content) return ( "
0x8213ECCC  " ) end "
0x8213ECD4  "@"
0x8213ECE0  "true)"
0x8213ECE8  "false)"
0x8213ECF0  "\", "
0x8213ECF4  "(Content, \""
0x8213ED00  "(Content)"
0x8213ED0C  "not "
0x8213ED14  "\"]"
0x8213ED18  "[\""
0x8213ED1C  "\"][\""
0x8213ED24  " and "
0x82124884  "."
```

The quotation marks delimit each literal and are not part of it; spaces inside
the quotation marks are significant.

The literals and function boundaries were read from `original/Aurora.exe`
using its PE section mappings. `.rdata` addresses must be translated with the
section's `PointerToRawData + (RVA - VirtualAddress)`; `pefile`'s generic RVA
offset result for this image adds an erroneous `0x400` and must not be used as
evidence.
