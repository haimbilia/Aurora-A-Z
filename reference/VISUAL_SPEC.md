# Selector visual specification

This is the renderer contract derived from the supplied 1280×720 mockup and
the live CleanNXE baseline. It describes the overlay owned by `AuroraAZ.xex`;
it does not authorize a skin edit.

## Reference geometry

- Render in a logical 1280×720 coordinate system and scale the completed row
  uniformly to the active viewport.
- Draw one uninterrupted sequence: `# A B C D E F G H I J K L M N O P Q R S T U V W X Y Z`.
- Center the sequence at logical X = 640.
- Target a row extent of approximately X = 182 through X = 1098. This leaves
  the existing `1 of 2232` counter at the lower left unobstructed and matches
  the mockup's roughly 72% viewport width.
- Target the visible glyph box around Y = 568 through Y = 600, with its
  baseline aligned to the existing lower-left game counter.
- Use an approximately 34–35 px center-to-center pitch, adjusted optically for
  narrow glyphs such as `I` without changing the overall center.
- Never move the skin's title, counter, button legend, or coverflow to make
  room. The overlay must hide outside the main coverflow instead.

All coordinates are provisional constants until the first hardware overlay
spike is captured. The NOVA comparison, not a desktop font preview, decides the
final values.

## Typography and contrast

- Use an embedded, redistributable light-weight sans-serif face with metrics
  close to the mockup; do not read a font from the selected skin.
- Target a 34–36 logical-pixel em size.
- Inactive glyphs are light gray/white and remain readable over both the dark
  upper background and the pale coverflow reflection.
- Draw the selected glyph at full white without changing its position or the
  row's spacing. A restrained glow or two-pixel underline may be used only if
  luminance alone is not distinguishable on hardware.
- The final fidelity pass adds a soft black shadow approximately 2 logical
  pixels down and 2 pixels right. The interaction milestone may initially use
  a hard two-pass shadow so behavior can be proven before blur work.

## State presentation

- **Coverflow state:** the complete row remains visible, faithful to the
  mockup, but no navigation focus is advertised.
- **Selector state:** exactly one glyph is emphasized; every other glyph stays
  in place. Entry begins at `#`.
- **Applying:** retain the selected glyph while Aurora refreshes the list. Do
  not show a popup or replace the row with a busy panel.
- **Non-coverflow scenes:** draw nothing and consume no input.

## Screenshot acceptance

For each supported skin, retain a NOVA capture for these states:

1. stock coverflow before module load;
2. module loaded, selector inactive;
3. selector active at `#`;
4. selector active at a middle letter and at `Z`;
5. filtered coverflow after A;
6. QuickView opened with RB, with no Aurora A-Z overlay visible above it.

The release comparison checks row center, baseline, title/counter overlap,
selected-letter contrast, and whether any skin-owned pixels changed outside
the overlay bounds.
