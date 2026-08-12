# FrostInk development handoff

Last updated: 2026-08-12 (Australia/Melbourne)

This file is the continuation point for FrostInk work started in the Codex X4 workspace. The source of truth is the `codex/feat-ui-polish` branch in `FrostyMead/CrossInk`.

## Continue on another computer

```sh
git clone https://github.com/FrostyMead/CrossInk.git
cd CrossInk
git switch codex/feat-ui-polish
```

Before changing EPUB layout or image rendering, read `AGENTS.md` and `.claude/CONTEXT.md`. Generated firmware, PlatformIO output, and `dist-publish/` are intentionally ignored; rebuild them from the committed source.

## Session history

The work started from `uxjulia/crossink` and was forked as `FrostyMead/CrossInk`.

Published branch history before this handoff includes:

- `a3197934` — FrostInk identity, branded boot/portal UI, and the safe custom OTA channel.
- `6ab8a256` — OTA release checks use a 30-second network timeout instead of ESP-IDF's 5-second default.
- `f948b8ea` — OTA accepts ordinary three-part release assets such as `firmware-x3-x4-v1.0.2.bin`.
- `bc7fe41b` — rounded, minimal FrostInk device UI and clearer settings/loading states.

The current handoff commit adds the library and reading experiments developed afterward:

- Separate Books and Comics shelves backed by `/Books` and `/Comics`.
- Nested shelf folders for series collections, plus an Unsorted compatibility collection.
- SD-card discovery that finds newly transferred books without opening Browse Files first.
- Browse Files moved to Settings > System.
- Reader Light/Normal/Dark Ink Weight without changing pagination.
- Frost Clean EPUB and Frost Manga Lab in the device web portal.
- Guided-panel EPUB generation from comic EPUB, CBZ, ZIP, or images.
- Conservative panel detection, spread splitting, reading-order controls, border trimming, four-shade tone processing, and full-page fallback.
- Standard EPUB output so the normal reader menu can switch portrait/landscape.
- Up to eight guided panels per EPUB spine section to avoid a section transition on every page turn.
- Vertical centring for EPUB pages containing exactly one image and no text.
- A standalone Manga Lab builder at `scripts/build_guided_panels_test.mjs`.

An attempted XTCH runtime scaling/orientation implementation was deliberately reverted after it made the X4 laggy and unstable. Do not restore that experiment. The current direction is ordinary, cacheable EPUB output.

## Validation completed

- JavaScript syntax checks pass for `web/pages/files.js` and `scripts/build_guided_panels_test.mjs`.
- A generated ten-panel EPUB was structurally checked: correct EPUB mimetype, two cached spine sections, forced page breaks, RTL spine behavior, and chapter anchors.
- `node scripts/build_guided_panels_test.mjs` regenerates `dist-publish/FrostInk-Guided-Panels-Test.html`.
- PlatformIO `simulator` build completed successfully with the Books/Comics smoke-test route.
- PlatformIO X3/X4 firmware build completed successfully in the local `debug` environment.
- App touch-gate audit passed.
- Test firmware size: 6,347,488 bytes, leaving 206,112 bytes in the OTA application partition.
- Last local test binary: `dist-publish/FrostInk-X4-Guided-EPUB-Centred-Test.bin`.
- Last local test binary SHA-256: `6ae37acc1bcf6c56f3d86b71e0f5a727fd320d5c3f1cf4d46024b7b8ca5fc27c`.

The test binary and standalone HTML are reproducible outputs and are not committed.

## Loose threads

1. Hardware-test the latest centred/grouped EPUB build on an X4. Regenerate the comic under a new filename or clear its `.crosspoint/epub_<hash>/` cache first.
2. Check portrait centring, landscape fit, ordinary book pages with text, and page-turn latency. Specifically compare turns within an eight-panel group with the boundary from panel 8 to panel 9.
3. Decide whether eight panels per section is the best X4 balance. Larger sections reduce transitions but raise first-open layout and memory costs.
4. Exercise `/Books`, `/Comics`, nested series folders, Unsorted, newly transferred files, delete/move actions, and Back navigation on hardware.
5. Add translations for the new Books, Comics, Unsorted, empty-shelf, and Ink Weight strings. English fallback currently keeps other locales functional.
6. Continue tuning conservative panel detection for irregular manga layouts and full-bleed artwork; retain the whole-page fallback when gutters are ambiguous.
7. After hardware acceptance, choose the next FrostInk version and publish an OTA release. The preferred asset format is `firmware-x3-x4-vMAJOR.MINOR.PATCH.bin` with a matching `vMAJOR.MINOR.PATCH` GitHub release tag. Publish the release rather than leaving it as a draft.

## Useful commands

```sh
node --check web/pages/files.js
node --check scripts/build_guided_panels_test.mjs
node scripts/build_guided_panels_test.mjs
pio run -e simulator
pio run -e default
```

For the standalone test, open `dist-publish/FrostInk-Guided-Panels-Test.html`. For device testing, transfer a newly generated guided EPUB into `/Comics`, open it once to create its image cache, and use the normal reader menu to change orientation.
