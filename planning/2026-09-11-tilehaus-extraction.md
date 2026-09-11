# Tilehaus Extraction Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extract the `poc/` native-LVGL Home Assistant panel out of the `espcontrol` repo into a new standalone project, **Tilehaus**, at `~/Code/tilehaus` (repo `jsenecal/tilehaus`) — relocated, de-"poc"'d, self-contained (build/test/CI/docs), crediting EspControl.

**Architecture:** A fresh git repo with `firmware/` (ESPHome YAML + `native/` C++ headers), `web/` (TS configurator + the `deck.ts`/`grid_layout.ts` codec it owns), `scripts/`, `tests/{web,firmware}/`, `docs/` (Zensical), and CI. This is **relocation + rename + packaging only** — no behavioural change. `espcontrol`'s committed files are never edited.

**Tech Stack:** TypeScript (strict, esbuild, `node --test`), C++17 (ESP-IDF/LVGL, host CMake/ctest), ESPHome 2026.7.x, Zensical docs (Python), GitHub Actions.

**Reference spec:** `docs/superpowers/specs/2026-09-11-tilehaus-extraction-design.md`

**Path variables used below:**
- `SRC` = `/home/jsenecal/Code/espcontrol` (read-only source; never edited)
- `DEST` = `/home/jsenecal/Code/tilehaus` (the new repo)

**Scope guard:** `poc_`-prefixed *function symbols* (e.g. `poc_build_grid`, `poc_build_cards`) are intentionally **left as-is** inside `namespace tilehaus` — renaming them is explicit future work, out of scope here. This plan renames the `namespace`, the 5 `poc_*` *filenames*, the ESPHome config name, the web imports, the deck media type, and the package/paths only (matching the spec).

---

## Task 1: Scaffold the Tilehaus repo

**Files:** Create `$DEST/` with `.gitignore`, `mise.toml`, `LICENSE`, `NOTICE`, `package.json`, `tsconfig.json`, `README.md` (stub), and empty dirs.

- [ ] **Step 1: Create the directory tree + init git**
```bash
mkdir -p /home/jsenecal/Code/tilehaus/{firmware/native,firmware/assets,web/model,scripts,tests/web/unit,tests/web/helpers,tests/firmware/fixtures,docs,.github/workflows}
cd /home/jsenecal/Code/tilehaus
git init
```

- [ ] **Step 2: `.gitignore`** — write `/home/jsenecal/Code/tilehaus/.gitignore`:
```gitignore
node_modules/
firmware/.esphome/
firmware/native/deck_ui_asset.h
tests/firmware/build/
.venv/
docs/site/
docs/.zensical/
*.log
```

- [ ] **Step 3: `LICENSE`** — copy EspControl's license verbatim:
```bash
cp /home/jsenecal/Code/espcontrol/LICENSE /home/jsenecal/Code/tilehaus/LICENSE
```

- [ ] **Step 4: `NOTICE`** — write `/home/jsenecal/Code/tilehaus/NOTICE`:
```
Required Notice: Copyright (c) 2026 Jonathan Senecal

Tilehaus is licensed under the PolyForm Noncommercial License 1.0.0
(see LICENSE).

Tilehaus is an independent project inspired by EspControl
(https://github.com/jtenniswood/espcontrol). It reuses EspControl's product
concepts and visual language; it shares no firmware code with EspControl's
button-grid implementation. The bundled Material Design Icons font and Roboto
are under their respective licenses (see firmware/assets/).
```

- [ ] **Step 5: `package.json`** — write `/home/jsenecal/Code/tilehaus/package.json`. Pin `esbuild`, `typescript`, and `@types/node` to the **same versions** found in `/home/jsenecal/Code/espcontrol/package.json` `devDependencies` (read them first):
```json
{
  "name": "tilehaus",
  "version": "0.1.0",
  "private": true,
  "license": "PolyForm-Noncommercial-1.0.0",
  "type": "commonjs",
  "scripts": {
    "build": "node scripts/build_deck_ui.js",
    "gen:icons": "node scripts/build_icon_catalog.js",
    "gen:fixtures": "node scripts/gen_deck_fixture.js",
    "typecheck": "tsc --noEmit -p tsconfig.json",
    "test:web": "node --test tests/web/unit/*.test.js",
    "test:firmware": "cmake -S tests/firmware -B tests/firmware/build && cmake --build tests/firmware/build && ctest --test-dir tests/firmware/build --output-on-failure"
  },
  "devDependencies": {
    "esbuild": "<match espcontrol>",
    "typescript": "<match espcontrol>",
    "@types/node": "<match espcontrol>"
  }
}
```

- [ ] **Step 6: `tsconfig.json`** — base it on `$SRC/poc/webserver/tsconfig.json` but root it at the repo. Write `/home/jsenecal/Code/tilehaus/tsconfig.json` (copy `compilerOptions` from the source tsconfig; set includes):
```jsonc
{
  "compilerOptions": {
    /* copy the compilerOptions block verbatim from
       $SRC/poc/webserver/tsconfig.json (strict, target, module, lib, etc.) */
  },
  "include": ["web/**/*.ts", "tests/web/**/*.ts", "scripts/**/*.js"]
}
```

- [ ] **Step 7: `mise.toml`** — base on `$SRC/mise.toml`; keep the node + python (+ esphome) tool versions. Write `/home/jsenecal/Code/tilehaus/mise.toml` mirroring the node/python versions and `TZ=UTC` env from the source.

- [ ] **Step 8: `README.md` stub** — a one-paragraph placeholder (fleshed out in Task 5):
```markdown
# Tilehaus

An ESPHome-based, native-LVGL Home Assistant control panel for affordable
ESP32-P4 touchscreens. Arrange a deck of tiles across pages, edited from a
browser configurator served by the device.

Inspired by [EspControl](https://github.com/jtenniswood/espcontrol).

_Setup docs: see `docs/`._
```

- [ ] **Step 9: Install dev deps + initial commit**
```bash
cd /home/jsenecal/Code/tilehaus
npm install
git add -A
git commit -m "chore: scaffold Tilehaus repo"
```
Expected: `npm install` succeeds; commit created.

---

## Task 2: Assets + web configurator + model + scripts + web tests

**Files:** copy assets, `web/`, `scripts/`, `tests/web/`; rewrite paths; rename media type. Goal: `gen:icons`, `typecheck`, `test:web` all green.

- [ ] **Step 1: Copy the shared font/glyph assets**
```bash
cd /home/jsenecal/Code/tilehaus
cp /home/jsenecal/Code/espcontrol/common/assets/fonts/materialdesignicons-webfont-7.4.47.ttf firmware/assets/
cp /home/jsenecal/Code/espcontrol/common/assets/fonts/materialdesignicons-LICENSE.txt firmware/assets/
cp /home/jsenecal/Code/espcontrol/common/assets/icon_glyphs.yaml firmware/assets/
cp /home/jsenecal/Code/espcontrol/common/assets/text_glyphs.yaml firmware/assets/
```
(Roboto is fetched via `gfonts://` at build time — no file needed.)

- [ ] **Step 2: Copy the web modules + the codec**
```bash
cp /home/jsenecal/Code/espcontrol/poc/webserver/*.ts web/
cp /home/jsenecal/Code/espcontrol/poc/webserver/index.html web/
cp /home/jsenecal/Code/espcontrol/src/webserver/model/deck.ts web/model/
cp /home/jsenecal/Code/espcontrol/src/webserver/model/grid_layout.ts web/model/
```

- [ ] **Step 3: Rewrite web imports** — in `web/*.ts`, replace the cross-repo model import prefix with the local one:
```bash
cd /home/jsenecal/Code/tilehaus
sed -i 's#\.\./\.\./src/webserver/model/#./model/#g' web/*.ts
```
Verify none remain: `grep -rn "src/webserver/model" web` → empty.

- [ ] **Step 4: Rename the deck media type** — in `web/deck_client.ts`:
```bash
sed -i 's#application/vnd\.espcontrol\.deck#application/vnd.tilehaus.deck#' web/deck_client.ts
```

- [ ] **Step 5: Copy + repath the scripts**
```bash
cp /home/jsenecal/Code/espcontrol/scripts/build_deck_ui.js scripts/
cp /home/jsenecal/Code/espcontrol/scripts/build_icon_catalog.js scripts/
cp /home/jsenecal/Code/espcontrol/scripts/gen_deck_fixture.js scripts/
cp /home/jsenecal/Code/espcontrol/scripts/load_typescript_module.js scripts/
```
Then edit the path constants:
  - `scripts/build_deck_ui.js`: `entryPoints: ["poc/webserver/index.ts"]` → `["web/index.ts"]`; `"poc/webserver/index.html"` → `"web/index.html"`; `"poc/native/deck_ui_asset.h"` → `"firmware/native/deck_ui_asset.h"`.
  - `scripts/build_icon_catalog.js`: `SRC` `"common/assets/icon_glyphs.yaml"` → `"firmware/assets/icon_glyphs.yaml"`; `OUT` `"poc/webserver/icon_catalog.ts"` → `"web/icon_catalog.ts"`.
  - `scripts/gen_deck_fixture.js`: `loadTypeScriptModule("src/webserver/model/deck.ts")` → `loadTypeScriptModule("web/model/deck.ts")`. (Fixture output paths `../tests/firmware/fixtures/*.bin` are unchanged.)

- [ ] **Step 6: Copy the web test suites + wrappers + helpers + fixtures**
```bash
cd /home/jsenecal/Code/tilehaus
for t in deck deck_client deck_editor_state card_fields deck_layout ha_client icon_from_entity icon_catalog; do
  cp /home/jsenecal/Code/espcontrol/tests/web/$t.test.ts tests/web/
  cp /home/jsenecal/Code/espcontrol/tests/web/unit/$t.test.js tests/web/unit/
done
cp -r /home/jsenecal/Code/espcontrol/tests/web/helpers/. tests/web/helpers/ 2>/dev/null || true
cp /home/jsenecal/Code/espcontrol/tests/firmware/fixtures/deck_basic.bin    tests/firmware/fixtures/
cp /home/jsenecal/Code/espcontrol/tests/firmware/fixtures/deck_v5_pages.bin tests/firmware/fixtures/
cp /home/jsenecal/Code/espcontrol/tests/firmware/fixtures/deck_v6_grid.bin  tests/firmware/fixtures/
```

- [ ] **Step 7: Rewrite test import paths** — in `tests/web/*.test.ts`:
```bash
sed -i 's#\.\./\.\./poc/webserver/#../../web/#g; s#\.\./\.\./src/webserver/model/#../../web/model/#g' tests/web/*.test.ts
```
Inspect the 8 `tests/web/unit/*.test.js` wrappers and the `helpers/` loader: if any reference `poc/webserver`, `src/webserver/model`, or a `tests/web/<name>.test.ts` path, repoint them to the new `web/` / `tests/web/` locations. (Fixture reads like `tests/firmware/fixtures/deck_basic.bin` are cwd-relative to the repo root — unchanged.) Verify: `grep -rn "poc/webserver\|src/webserver" tests/web` → empty.

- [ ] **Step 8: Generate the icon catalog + typecheck + run web tests**
```bash
cd /home/jsenecal/Code/tilehaus
npm run gen:icons        # writes web/icon_catalog.ts from firmware/assets/icon_glyphs.yaml
npm run typecheck        # tsc --noEmit → clean
npm run test:web         # node --test → all 8 suites pass
```
Expected: icon_catalog written; typecheck clean; `pass N fail 0`.

- [ ] **Step 9: Commit**
```bash
git add -A
git commit -m "feat: web configurator, deck codec, scripts, and web tests"
```

---

## Task 3: Firmware native headers + de-poc + host tests

**Files:** copy `firmware/native/*.h`; rename 5 files + namespace + media type; copy/rename the 5 host tests; write a fresh `tests/firmware/CMakeLists.txt`. Goal: `test:firmware` green.

- [ ] **Step 1: Copy native headers (skip the generated asset)**
```bash
cd /home/jsenecal/Code/tilehaus
rsync -a --exclude 'deck_ui_asset.h' /home/jsenecal/Code/espcontrol/poc/native/ firmware/native/
```

- [ ] **Step 2: Rename the 5 `poc_*` files**
```bash
cd /home/jsenecal/Code/tilehaus/firmware/native
for f in grid ha clock splash surface; do git_name="poc_$f.h"; [ -f "$git_name" ] && mv "$git_name" "$f.h"; done
```

- [ ] **Step 3: Fix includes of the renamed files** — across `firmware/native/` and the host tests:
```bash
cd /home/jsenecal/Code/tilehaus
grep -rl --include='*.h' --include='*.cpp' 'poc_\(grid\|ha\|clock\|splash\|surface\)\.h' firmware/native tests/firmware 2>/dev/null | \
  xargs -r sed -i 's/poc_grid\.h/grid.h/g; s/poc_ha\.h/ha.h/g; s/poc_clock\.h/clock.h/g; s/poc_splash\.h/splash.h/g; s/poc_surface\.h/surface.h/g'
```

- [ ] **Step 4: Rename the C++ namespace** — in all `firmware/native/*.h`:
```bash
sed -i 's/namespace poc\b/namespace tilehaus/g; s/\bpoc::/tilehaus::/g' firmware/native/*.h
```
Verify: `grep -rn 'namespace poc\b\|poc::' firmware/native` → empty.

- [ ] **Step 5: Rename the deck media type (firmware side)** — in `firmware/native/deck_endpoint.h`:
```bash
sed -i 's#application/vnd\.espcontrol\.deck#application/vnd.tilehaus.deck#' firmware/native/deck_endpoint.h
```

- [ ] **Step 6: Copy + rename the 5 host tests, de-poc their namespace**
```bash
cd /home/jsenecal/Code/tilehaus
cp /home/jsenecal/Code/espcontrol/tests/firmware/deck_document_test.cpp   tests/firmware/
cp /home/jsenecal/Code/espcontrol/tests/firmware/poc_grid_layout_test.cpp tests/firmware/grid_layout_test.cpp
cp /home/jsenecal/Code/espcontrol/tests/firmware/poc_sensor_format_test.cpp tests/firmware/sensor_format_test.cpp
cp /home/jsenecal/Code/espcontrol/tests/firmware/poc_slider_map_test.cpp  tests/firmware/slider_map_test.cpp
cp /home/jsenecal/Code/espcontrol/tests/firmware/poc_toggle_state_test.cpp tests/firmware/toggle_state_test.cpp
sed -i 's/\bpoc::/tilehaus::/g' tests/firmware/*.cpp
```

- [ ] **Step 7: Write a fresh minimal `tests/firmware/CMakeLists.txt`** — covering only the 5 Tilehaus host tests. Read `$SRC/tests/firmware/CMakeLists.txt` for the exact C++ standard, include setup, and the `DECK_FIXTURE`/`DECK_V5_FIXTURE`/`DECK_V6_FIXTURE` define values used by `deck_document_test`, and reproduce only those. Write `/home/jsenecal/Code/tilehaus/tests/firmware/CMakeLists.txt`:
```cmake
cmake_minimum_required(VERSION 3.16)
project(tilehaus_firmware_tests CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../firmware/native)
enable_testing()

add_executable(deck_document_test deck_document_test.cpp)
target_compile_definitions(deck_document_test PRIVATE
  DECK_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/deck_basic.bin"
  DECK_V5_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/deck_v5_pages.bin"
  DECK_V6_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/deck_v6_grid.bin")
add_test(NAME deck_document_test COMMAND deck_document_test)

foreach(t grid_layout sensor_format slider_map toggle_state)
  add_executable(${t}_test ${t}_test.cpp)
  add_test(NAME ${t}_test COMMAND ${t}_test)
endforeach()
```
(If any of the 4 non-deck tests need extra include dirs or defines per the source CMakeLists, add them.)

- [ ] **Step 8: Build + run the host tests**
```bash
cd /home/jsenecal/Code/tilehaus
npm run test:firmware
```
Expected: 5/5 tests pass.

- [ ] **Step 9: Commit**
```bash
git add -A
git commit -m "feat: native firmware headers (namespace tilehaus) + host tests"
```

---

## Task 4: Firmware YAML + assets wiring + bundle + ESPHome compile

**Files:** copy/rename the 5 YAML files; repath fonts + de-poc lambdas; build the UI asset; compile.

- [ ] **Step 1: Copy the YAML**
```bash
cd /home/jsenecal/Code/tilehaus
cp /home/jsenecal/Code/espcontrol/poc/lvgl-native-poc.yaml firmware/tilehaus.yaml
cp /home/jsenecal/Code/espcontrol/poc/hardware.yaml firmware/
cp /home/jsenecal/Code/espcontrol/poc/ui.yaml firmware/
cp /home/jsenecal/Code/espcontrol/poc/fonts.yaml firmware/
cp /home/jsenecal/Code/espcontrol/poc/bindings.yaml firmware/
```

- [ ] **Step 2: `secrets.yaml.example`** — write `/home/jsenecal/Code/tilehaus/firmware/secrets.yaml.example`:
```yaml
wifi_ssid: "YOUR_WIFI_SSID"
wifi_password: "YOUR_WIFI_PASSWORD"
```
(Do **not** copy the real `poc/secrets.yaml`.) Confirm `firmware/secrets.yaml` is covered by the repo `.gitignore` — add `firmware/secrets.yaml` to `.gitignore` if not already implied.

- [ ] **Step 3: Rename the ESPHome identity** — in `firmware/tilehaus.yaml`:
  - `name: "lvgl-native-poc"` → `name: "tilehaus"`
  - update `friendly_name` to `"Tilehaus"`
  - `mdi_font_file` substitution → `"assets/materialdesignicons-webfont-7.4.47.ttf"` (relative to `firmware/`, the ESPHome config dir)
  - `includes: [native]` stays (native/ is a sibling).

- [ ] **Step 4: Repath the font glyph includes** — in `firmware/fonts.yaml`:
```bash
cd /home/jsenecal/Code/tilehaus
sed -i 's#\.\./common/assets/icon_glyphs\.yaml#assets/icon_glyphs.yaml#g; s#\.\./common/assets/text_glyphs\.yaml#assets/text_glyphs.yaml#g' firmware/fonts.yaml
```

- [ ] **Step 5: De-poc the YAML lambdas** — in `firmware/bindings.yaml`, `firmware/ui.yaml`, `firmware/hardware.yaml`:
```bash
sed -i 's/\bpoc::/tilehaus::/g' firmware/bindings.yaml firmware/ui.yaml firmware/hardware.yaml
```
Verify: `grep -rn 'poc::' firmware/*.yaml` → empty.

- [ ] **Step 6: Build the UI asset bundle**
```bash
cd /home/jsenecal/Code/tilehaus
npm run build        # esbuild → gzipped firmware/native/deck_ui_asset.h (gitignored)
```
Expected: prints `html=… js=… js.gz=…`; `firmware/native/deck_ui_asset.h` exists.

- [ ] **Step 7: ESPHome compile** — create `firmware/secrets.yaml` locally (copy of the example with real creds, or the values from `$SRC/poc/secrets.yaml`) so the build resolves, then:
```bash
cd /home/jsenecal/Code/tilehaus/firmware
# use the mise/venv-managed esphome toolchain
esphome compile tilehaus.yaml
```
Expected: "Successfully compiled program." (Flash/RAM comparable to the current POC: ~22% flash.) If `esphome` isn't on PATH, use the same invocation pattern as EspControl's `.venv-esphome/bin/esphome` (document the chosen toolchain in the README in Task 5).

- [ ] **Step 8: Commit**
```bash
cd /home/jsenecal/Code/tilehaus
git add -A
git commit -m "feat: ESPHome config (tilehaus.yaml) + fonts/assets wiring; compiles"
```

---

## Task 5: Docs (Zensical) + README + NOTICE + CI

**Files:** `README.md`, `docs/` (Zensical), `.github/workflows/ci.yml`.

- [ ] **Step 1: Full `README.md`** — overwrite the stub with: what Tilehaus is, supported hardware (ESP32-P4, e.g. Guition JC1060P470), quickstart (flash firmware → open the device's web page → arrange tiles → Save reboots to apply), the toolchain (mise: node/python/esphome; `npm run build`/`test:web`/`test:firmware`; `esphome compile firmware/tilehaus.yaml`), and a **Credits** section crediting [EspControl](https://github.com/jtenniswood/espcontrol) as the inspiration and noting Tilehaus is an independent sibling sharing no firmware code.

- [ ] **Step 2: Zensical config + pages** — create the docs site:
```bash
cd /home/jsenecal/Code/tilehaus
python3 -m venv .venv && . .venv/bin/activate && pip install zensical
```
Write `/home/jsenecal/Code/tilehaus/mkdocs.yml` (Zensical reads `mkdocs.yml`):
```yaml
site_name: Tilehaus
site_description: A native-LVGL Home Assistant control panel for ESP32-P4 touchscreens.
docs_dir: docs
nav:
  - Home: index.md
  - Getting started: getting-started.md
```
Write `docs/index.md` (landing: what it is, a screenshot placeholder, link to getting-started, credit to EspControl) and `docs/getting-started.md` (stub: hardware, flashing via ESPHome, opening the configurator, first tile). Keep them short — full card/device reference is a follow-up.

- [ ] **Step 3: Build the docs**
```bash
cd /home/jsenecal/Code/tilehaus
. .venv/bin/activate
zensical build      # (or `python -m zensical build` per zensical.org/docs)
```
Expected: a static site is produced under the Zensical output dir (gitignored). If the exact CLI differs, follow `https://zensical.org/docs/get-started/`.

- [ ] **Step 4: CI workflow** — write `/home/jsenecal/Code/tilehaus/.github/workflows/ci.yml`:
```yaml
name: CI
on: [push, pull_request]
jobs:
  web:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-node@v4
        with: { node-version: "24" }
      - run: npm ci
      - run: npm run gen:icons
      - run: npm run typecheck
      - run: npm run test:web
  firmware-tests:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - run: npm ci
      - run: npm run test:firmware
```
(An `esphome compile` job can be added later once the toolchain pin is settled; leave it out of CI for now to keep it green.)

- [ ] **Step 5: Commit**
```bash
cd /home/jsenecal/Code/tilehaus
git add -A
git commit -m "docs: Zensical site + README (credits EspControl) + CI"
```

---

## Task 6: Full verification + residual-reference sweep + push hand-off

- [ ] **Step 1: Run everything**
```bash
cd /home/jsenecal/Code/tilehaus
npm run gen:icons && npm run typecheck && npm run test:web && npm run test:firmware && npm run build
(cd firmware && esphome compile tilehaus.yaml)
. .venv/bin/activate && zensical build
```
Expected: all green.

- [ ] **Step 2: Residual-reference sweep** — these should be **empty** except intentional lineage credit in README/NOTICE:
```bash
cd /home/jsenecal/Code/tilehaus
grep -rniE 'lvgl-native-poc|vnd\.espcontrol\.deck|src/webserver/model|poc/webserver' \
  --exclude-dir=node_modules --exclude-dir=.venv --exclude-dir=.git .
grep -rnE 'namespace poc\b|\bpoc::' firmware tests
```
Fix any stragglers and re-run Step 1.

- [ ] **Step 3 (optional): single clean root commit** — if a single initial commit is preferred over the per-task history:
```bash
cd /home/jsenecal/Code/tilehaus
git reset --soft $(git rev-list --max-parents=0 HEAD) && git commit --amend -m "Initial commit: Tilehaus"
```
(Skip to keep the readable per-task history.)

- [ ] **Step 4: Relocate the planning docs into Tilehaus**
```bash
mkdir -p /home/jsenecal/Code/tilehaus/docs/design
cp /home/jsenecal/Code/espcontrol/docs/superpowers/specs/2026-09-11-tilehaus-extraction-design.md /home/jsenecal/Code/tilehaus/docs/design/
cp /home/jsenecal/Code/espcontrol/docs/superpowers/plans/2026-09-11-tilehaus-extraction.md /home/jsenecal/Code/tilehaus/docs/design/
git -C /home/jsenecal/Code/tilehaus add -A && git -C /home/jsenecal/Code/tilehaus commit -m "docs: include extraction design + plan"
```

- [ ] **Step 5: Create the GitHub repo + push (hand-off — user action)**
```bash
cd /home/jsenecal/Code/tilehaus
gh repo create jsenecal/tilehaus --private --source=. --remote=origin --push
# or: create jsenecal/tilehaus in the GitHub UI, then:
#   git remote add origin git@github.com:jsenecal/tilehaus.git && git push -u origin main
```
This is an outward action on the user's account — confirm with the user before running, or hand them the commands.

- [ ] **Step 6: Confirm `espcontrol` untouched**
```bash
git -C /home/jsenecal/Code/espcontrol status --short
```
Expected: only the pre-existing untracked files (`CLAUDE.md`, `mise.toml`, `builds/…`) and the uncommitted planning docs under `docs/superpowers/` — **no edits** to `poc/`, `components/`, `src/`, `common/`, `scripts/`, or `tests/`.

---

## Self-Review Notes

- **Spec coverage:** repo scaffold + license/notice (T1), web+model+scripts+tests relocation & repath & media-type (T2), native headers + namespace + filename renames + host tests (T3), YAML + fonts + compile (T4), Zensical docs + README credit + CI (T5), full verify + sweep + push hand-off + espcontrol-untouched check (T6). All spec sections mapped.
- **Type/path consistency:** media type `application/vnd.tilehaus.deck` in both `web/deck_client.ts` (T2) and `firmware/native/deck_endpoint.h` (T3). Model imports `./model/*` (T2). `namespace tilehaus` across native + tests + YAML lambdas (T3/T4). Scripts repointed to `web/`, `web/model/`, `firmware/assets/`, `firmware/native/` (T2/T4). `DECK_*_FIXTURE` defines reproduced in the new CMakeLists (T3).
- **Scope:** `poc_`-prefixed function *symbols* deliberately retained inside `namespace tilehaus` (noted up top) — filename prefixes and namespace are renamed, symbols are a future cleanup. `espcontrol` is never edited (verified in T6).
- **Placeholders:** two deliberate "read-from-source" fill-ins — devDependency versions (T1.5) and the `tsconfig` compilerOptions block (T1.6) — are copied verbatim from `espcontrol` rather than guessed, to avoid drift. Every code-changing step shows the command or file content.
