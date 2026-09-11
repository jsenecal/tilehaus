# Tilehaus — Standalone Extraction Design

**Status:** Design (for review)
**Date:** 2026-09-11
**Author:** Jonathan Senecal

## Goal

Extract the native-LVGL Home Assistant control-panel POC (currently `poc/` inside
the `espcontrol` repo) into its own independent project — **Tilehaus** — with its
own GitHub repo (`jsenecal/tilehaus`), its own build/test/CI, its own docs, and
its own identity, while crediting EspControl as the inspiration.

Tilehaus is an **independent sibling** to EspControl, not a fork: it shares no
firmware code with EspControl's `button_grid` product (verified — see below) and
targets the same problem/hardware with a different, native-LVGL implementation.

## Decisions (locked)

- **Name:** Tilehaus (tile + haus — a home built from tiles). Availability
  checked: no namesake in the ESPHome / Home Assistant / firmware space.
- **Repo:** `jsenecal/tilehaus` (new, under the personal `jsenecal` account; the
  `espcontrol` repo lives under `jtenniswood`).
- **License:** PolyForm Noncommercial 1.0.0, © Jonathan Senecal (mirrors
  EspControl).
- **History:** Fresh start — a clean initial commit, lineage credited in README +
  NOTICE. (Not a `git filter-repo` history carry-over.)
- **EspControl repo:** Left **as-is**. Tilehaus begins as a clean extraction/copy;
  no files are removed from `espcontrol`. Any later de-duplication is out of scope.
- **C++ filenames:** drop the `poc_` prefix (`poc_grid.h` → `grid.h`, etc.); the
  `tilehaus::` namespace scopes them.
- **Docs:** [Zensical](https://zensical.org) (Material for MkDocs team's SSG;
  Markdown + `mkdocs.yml`, installed in a Python venv). Scaffold now (landing +
  getting-started), full pages as a follow-up.

## Why extraction is clean (context)

- **Firmware:** `poc/native/` (63 headers, ~7.7k lines) is a clean-room native-LVGL
  implementation with **zero** code dependency on `components/espcontrol/` (116
  files, ~52.7k lines). Only prose comments and the deck media-type string mention
  "espcontrol".
- **Shared codec:** `src/webserver/model/deck.ts` + `grid_layout.ts` are the binary
  DECK codec + grid math. They are surfaced via `model/index.ts` but **no shipping
  EspControl app code imports them** (verified) — they are POC-authored code that
  simply lives in the EspControl tree. They move into Tilehaus.
- **Assets:** the fonts under `common/assets/` (MDI TTF, text fonts, glyph lists)
  are shared *asset files*; they are copied into Tilehaus.

## What moves into Tilehaus (inventory)

Firmware:
- `poc/native/*.h` (63 headers). Rename the 5 prefixed files: `poc_grid.h`,
  `poc_ha.h`, `poc_clock.h`, `poc_splash.h`, `poc_surface.h` → `grid.h`, `ha.h`,
  `clock.h`, `splash.h`, `surface.h`. (`deck_ui_asset.h` is generated / gitignored.)
- `poc/lvgl-native-poc.yaml` → `firmware/tilehaus.yaml`; plus `hardware.yaml`,
  `ui.yaml`, `fonts.yaml`, `bindings.yaml`. `secrets.yaml` → `secrets.yaml.example`.
- Fonts referenced by `fonts.yaml` (MDI `materialdesignicons-webfont-7.4.47.ttf`
  and the text font(s)) + `icon_glyphs.yaml` + `text_glyphs.yaml`, copied from
  `common/assets/` into `firmware/assets/`.

Web configurator:
- `poc/webserver/*.ts` (12 modules: `index`, `editor_view`, `grid_view`,
  `editor_state`, `deck_client`, `card_fields`, `card_types`, `ha_client`,
  `icon_picker`, `icon_from_entity`, `icon_catalog`, `layout`) + `index.html`.
- `src/webserver/model/deck.ts` + `grid_layout.ts` → `web/model/` (Tilehaus owns
  them). All `../../src/webserver/model/*` imports become local `./model/*`.

Scripts:
- `scripts/build_deck_ui.js`, `build_icon_catalog.js`, `gen_deck_fixture.js`,
  `load_typescript_module.js`.

Tests:
- `tests/web/` deck suites: `deck.test.ts`, `deck_client.test.ts`,
  `deck_editor_state.test.ts`, `card_fields.test.ts`, `deck_layout.test.ts`,
  plus the `ha_client` / `icon_from_entity` / `icon_catalog` suites, the
  `tests/web/unit/*.test.js` `node --test` wrappers, and `helpers/`.
- `tests/firmware/`: `deck_document_test.cpp`, the grid-layout host test
  (`poc_grid_layout_test` → `grid_layout_test`), `CMakeLists.txt`, `fixtures/*.bin`.

## Target repo layout

```
tilehaus/
├── README.md                # identity + quickstart + "inspired by EspControl"
├── NOTICE                   # PolyForm NC 1.0.0 notice + EspControl lineage credit
├── LICENSE                  # PolyForm Noncommercial 1.0.0
├── package.json             # name "tilehaus"; web build + test scripts
├── tsconfig.json
├── mise.toml                # node + python (+ esphome) toolchain
├── .gitignore               # deck_ui_asset.h, .esphome/, node_modules, docs build, .venv
├── firmware/
│   ├── tilehaus.yaml
│   ├── hardware.yaml · ui.yaml · fonts.yaml · bindings.yaml · secrets.yaml.example
│   ├── native/              # 63 headers, namespace tilehaus
│   └── assets/              # fonts + icon_glyphs.yaml + text_glyphs.yaml
├── web/
│   ├── index.html + 12 .ts modules
│   └── model/{deck.ts, grid_layout.ts}
├── scripts/                 # build_deck_ui, build_icon_catalog, gen_deck_fixture, load_typescript_module
├── tests/{web,firmware}/
├── docs/                    # Zensical (mkdocs.yml + docs/index.md + getting-started)
└── .github/workflows/ci.yml
```

## Transformations (the "refactor" pass)

1. **Namespace:** `namespace poc` / `poc::` → `tilehaus` / `tilehaus::` across
   `firmware/native/`, the firmware host tests, and the `poc::` calls inside the
   YAML lambdas (`bindings.yaml`, `ui.yaml`, `hardware.yaml`).
2. **Filenames:** drop `poc_` prefix (5 files) and update every `#include`.
3. **ESPHome identity:** config `lvgl-native-poc.yaml` → `tilehaus.yaml`;
   substitution `name: lvgl-native-poc` → `tilehaus`; `friendly_name` updated.
4. **Web imports:** `../../src/webserver/model/*` → `./model/*`.
5. **Deck media type:** `application/vnd.espcontrol.deck` →
   `application/vnd.tilehaus.deck` in **both** `deck_endpoint.h` and
   `deck_client.ts` (`DECK_MEDIA_TYPE`). The on-disk `DECK` format name is kept.
6. **Fonts path:** `fonts.yaml` / substitutions point at local `firmware/assets/`
   instead of `../common/assets/`.
7. **Package/paths:** new `package.json` (name `tilehaus`), `tsconfig.json`,
   and updated script/test paths (no `poc/` or `src/webserver/` prefixes).
8. **Prose:** update working "espcontrol" references in comments; keep a single
   explicit lineage credit (README + NOTICE) rather than scattered mentions.
9. **Firmware host test:** rename `poc_grid_layout_test` → `grid_layout_test`;
   update `CMakeLists.txt` fixture defines/paths.

## Build / test / CI

- `package.json` scripts:
  - `build` → `node scripts/build_deck_ui.js` (esbuild bundle → gzipped
    `firmware/native/deck_ui_asset.h`).
  - `gen:icons` → `build_icon_catalog.js`; `gen:fixtures` → `gen_deck_fixture.js`.
  - `test:web` → `node --test tests/web/unit/*.test.js`.
  - `test:firmware` → CMake configure + `ctest` (host tests).
  - `typecheck` → `tsc --noEmit`.
- ESPHome compile via mise-managed toolchain: `esphome compile firmware/tilehaus.yaml`.
- `.github/workflows/ci.yml`: web typecheck + `test:web` + `test:firmware`
  (+ `esphome compile` if feasible in CI) on push / PR.

## Identity & credit

- **README:** Tilehaus is an ESPHome-based, native-LVGL Home Assistant control
  panel for affordable ESP32-P4 touchscreens — a configurable deck of tiles across
  pages, edited from a browser configurator served by the device, applied by
  reboot. Includes a "Credits / Inspired by EspControl" section linking
  `jtenniswood/espcontrol`.
- **NOTICE:** PolyForm NC required notice + an explicit line crediting the
  EspControl project's concepts and visual language.
- **docs/** (Zensical): landing page + a getting-started stub (hardware, flash,
  first config); full card/device reference is a follow-up.

## Logistics

1. Build the whole project in a fresh local directory: `~/Code/tilehaus`.
2. `git init` + a single clean initial commit.
3. Create `jsenecal/tilehaus` on GitHub (user action, or authorize `gh repo
   create`) and push. `espcontrol` is never modified by this work.
4. Verify: `test:web`, `test:firmware`, and `esphome compile` all pass in the new
   repo; the bundle builds; the docs site builds.

## Success criteria

- New `~/Code/tilehaus` repo with the layout above; clean initial commit.
- `npm run test:web` and `test:firmware` pass; `tsc --noEmit` clean.
- `esphome compile firmware/tilehaus.yaml` succeeds (Flash/RAM comparable to the
  current POC build).
- No `poc`/`espcontrol`/`../../src` references remain except the intentional
  lineage credit; deck media type is `application/vnd.tilehaus.deck`.
- Zensical docs build; README credits EspControl.
- `espcontrol`'s committed code is unchanged — no edits to `poc/`,
  `components/`, `src/`, `common/`, scripts, or tests. (This spec + its plan are
  uncommitted working docs that move into `tilehaus/` once the repo exists; they
  are not committed to `espcontrol`.)

## Out of scope

- Removing `poc/` or the shared model files from `espcontrol` (explicitly deferred —
  leave EspControl as-is).
- Full docs content (card/device reference pages) beyond the getting-started stub.
- Reviving the removed Camera card.
- Any behavioural change to the firmware or configurator — this is relocation +
  rename + packaging only.
- Publishing to npm / a released firmware artifact.
