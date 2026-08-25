# Local development (macOS)

This checkout is set up for engine work against a **Steam depot** of X-COM Apocalypse. The original game files are not committed; only the OpenApoc source and extracted/generated data under `data/` are in git.

For reverse-engineering and parity work on the original binaries, see [original-game/](original-game/README.md) and [AGENTS.md](../AGENTS.md). Ghidra and decompiler output are allowed on this fork.

## Prerequisites

- macOS with Apple Command Line Tools (or Xcode)
- [Homebrew](https://brew.sh)
- A legitimate copy of X-COM Apocalypse (Steam depot or `cd.iso`)

Install build dependencies:

```sh
brew install cmake boost pkg-config sdl2 qt@6 libvorbis
```

Add Qt to your shell `PATH` (zsh example):

```sh
echo 'export PATH="/opt/homebrew/opt/qt@6/bin:$PATH"' >> ~/.zprofile
```

CMake **3.30+** is required (Homebrew `cmake` satisfies this).

## One-time setup

From the repository root:

```sh
git submodule update --init --recursive
ln -sfn "$PWD/depot_7661/cd.iso" data/cd.iso
mkdir -p build
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix boost);/opt/homebrew" \
  -DENABLE_TESTS=OFF \
  -DBUILD_IMAGEDUMP=OFF \
  -DBUILD_SERIALIZATIONTOOL=OFF \
  -S . -B build
```

The first build runs the data extractor (`EXTRACT_DATA=ON` by default) and needs `data/cd.iso` to exist before `cmake` configures.

## Working in a second worktree (agents, parallel branches)

A fresh `git worktree` has **no submodules, no `data/cd.iso`, and no configured build tree**, so it
cannot build or run the gamestate tests until it is provisioned. Do this first, before anything
else:

```bash
./tools/setup-worktree.sh                       # or: ./tools/setup-worktree.sh <branch>
```

It is idempotent and safe to re-run. It fast-forwards onto the target branch **only** when that is
a true fast-forward and the tree is clean, initialises submodules against the main checkout's
object store rather than re-cloning from the network, links `data/cd.iso`, and configures `build/`
with `ENABLE_TESTS=ON`.

**Install `ccache` — this is the difference between a cold build and a warm one.**

```bash
brew install ccache
```

`cmake/ccache.cmake` already wires it in automatically when present, but it is detected at
**configure** time, so a build tree configured before you installed it will not use it. Reconfigure
that tree once (`cmake -S . -B build …`, or delete `build/CMakeCache.txt`) to pick it up.

For the cache to be shared **across** worktrees rather than one silo per directory, relativise the
paths once:

```bash
ccache --set-config base_dir=/path/to/parent/of/checkouts
ccache --set-config hash_dir=false
ccache -M 25G
```

Without `base_dir`/`hash_dir`, every worktree hashes its own absolute paths and gets zero hits from
its siblings — which is the failure mode that makes N parallel worktrees cost N full builds.

Only re-run `cmake --build build --target extract-data` if you changed `tools/extractors/`; it is
by far the slowest step and is unnecessary for ordinary code changes.

## Game data layout

| Path | Purpose |
| ------ | --------- |
| `depot_7661/cd.iso` | Steam ISO (full CD image: `MUSIC`, `XCOM3/`, etc.) — **use this** |
| `data/cd.iso` | Symlink to the ISO above (gitignored) |
| `depot_7661/XCOMA/` | Partial DOSBox HDD install — **do not** use as `Framework.CD` |

The extractor reads paths such as `xcom3/ufoexe/ufo2p.exe` from the ISO. The `XCOMA/` tree alone is missing maps and music and uses different executable CRCs. See [GitHub issue #681](https://github.com/OpenApoc/OpenApoc/issues/681).

`depot_7661/` is listed in `.gitignore` and must never be committed.

## Building

```sh
cmake --build build -j$(sysctl -n hw.ncpu)
```

Re-run extraction after changing the extractor or replacing `cd.iso`:

```sh
cmake --build build --target extract-data
```

On macOS, the build copies OpenApoc `data/` into `OpenApoc.app/Contents/Resources/data` and **excludes** `cd.iso` / `*.iso` / `*.cue` / `*.bin` (the original game must stay user-supplied). If `open` launches an empty bundle, run `extract-data` again after the `.app` exists.

Finder launch (`open ./build/bin/OpenApoc.app`) uses Resources for OpenApoc data and `~/Library/Application Support/OpenApoc/OpenApoc/` for settings, saves, and the CD path. The first run opens a file panel if the CD is missing. CLI `--Framework.Data` / `--Framework.CD` still override. `portable.txt` keeps the cwd-relative layout for terminal work.

The window is resizable. `Framework.Screen.Width`/`Height` of `0` means desktop size. Mode is `windowed`, `fullscreen` (exclusive), or `borderless`. F11 or Alt+Enter toggles windowed ↔ borderless. City and battle tiles stay 1:1 with **window points** so a larger window shows more of the map; HiDPI backing-store pixels are one GPU upscale, not extra tile work. Forms stay at original 640-wide layout and are integer-scaled (`Framework.Screen.UiScale`, `0` = auto: 1× below 2560-wide, then `width/1280`). `Framework.Screen.AutoScale` is the older “scale the whole game to ~1280-wide” path and keeps UI and map tied together. Bundle first launch (no saved Screen.* overrides) uses native borderless + auto UI scale.

### Signed Mac app (Developer ID)

```sh
cmake --preset macos-signed \
  -DAPPLE_CODESIGN_IDENTITY="Developer ID Application: Your Name (TEAMID)"
cmake --build --preset macos-signed -j$(sysctl -n hw.ncpu)
# or, after a normal Homebrew build:
./cmake/macos/sign.sh build/bin/OpenApoc.app "Developer ID Application: Your Name (TEAMID)"
```

`macos-signed` expects `VCPKG_ROOT` and static `arm64-osx-static` (see `cmake/vcpkg-triplets/`). Do not use `codesign --deep`. Notarization is optional for machines you already trust.

### iPad / iPhone (Apple Development)

```sh
sudo xcode-select -s /Applications/Xcode.app/Contents/Developer
cmake --preset ios-device -DAPPLE_TEAM_ID=YOURTEAMID
cmake --build --preset ios-device --config RelWithDebInfo
```

The iOS preset sets `DEVELOPER_DIR` to Xcode.app so Homebrew CMake can see the `iphoneos` SDK. `VCPKG_ROOT` must be set for SDL2/Boost/libvorbis (`arm64-ios`).

Install from the generated Xcode project / archive onto a registered device. Copy the original ISO into the app Documents folder via the Files app (File Sharing is enabled), or use the in-app picker. The same binary targets iPhone and iPad (`UIDeviceFamily` 1,2), landscape only. OpenGL ES 3.0 is required. Developer ID certificates cannot install on iOS.

## Testing

Default `openapoc: configure` keeps `-DENABLE_TESTS=OFF` so the first boot stays short. Any game-logic change must reconfigure with tests on:

```sh
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix boost);/opt/homebrew" \
  -DENABLE_TESTS=ON \
  -DBUILD_IMAGEDUMP=OFF \
  -DBUILD_SERIALIZATIONTOOL=OFF \
  -S . -B build
cmake --build build -j$(sysctl -n hw.ncpu)
ctest --test-dir build --output-on-failure
```

Or run the Cursor task `openapoc: test`. After gap-matrix or TODO edits:

```sh
python3 tools/regen_compare_report.py
```

Confirm depot/ISO stay untracked:

```sh
./tools/check_ignored_binaries.sh
```

## Original-game / Ghidra parity

This fork allows Ghidra and decompiler listings in the same session as `game/` edits. Do **not** commit `depot_7661/`, `data/cd.iso`, original EXEs, or Ghidra `.rep` / `.gpr` databases.

| Resource | Location |
| -------- | -------- |
| Agent agreement | [AGENTS.md](../AGENTS.md) |
| Parity skill (import, rebase, workflow) | [`.cursor/skills/openapoc-parity/SKILL.md`](../.cursor/skills/openapoc-parity/SKILL.md) |
| Gap matrix & implement queue | [original-game/openapoc-gap-matrix.md](original-game/openapoc-gap-matrix.md), [next-implementation.md](original-game/next-implementation.md) |
| Extractor table index | [original-game/extractor-tables.md](original-game/extractor-tables.md) |
| Compare report (HTML) | [original-game/compare-report.html](original-game/compare-report.html) |
| Sibling lab (Ghidra projects, rebase CSVs) | `/Users/khallmark/Desktop/Code/OpenSource/OpenApoc-og-research` |
| Private analysis notes (gitignored) | `depot_7661/analysis/` |

Key rules:

- ISO **non-4** `UFO2P.EXE` / `TACP.EXE` = extractor-canonical file offsets.
- Steam depot unsuffixed names are the Pentium **`4`** pair — rebase per table, never assume a global slide.
- Import bound LE with the community LX loader; do not unbind before reading offsets.

After gap-matrix or TODO edits:

```sh
python3 tools/regen_compare_report.py
```

Or run task `openapoc: regen compare report`.

## Running

**Recommended** — Mach-O from the repo root with explicit data paths (works regardless of Finder cwd):

```sh
./build/bin/OpenApoc.app/Contents/MacOS/OpenApoc \
  --Framework.Data="$PWD/data" \
  --Framework.CD="$PWD/data/cd.iso"
```

`portable.txt` in the repo root keeps settings and logs in the checkout:

| File | Purpose |
| ------ | --------- |
| `OpenApoc_settings.conf` | Settings (gitignored) |
| `log.txt` | Runtime log (gitignored) |
| `saves/` | Save games (gitignored) |

Without `portable.txt`, logs and settings go to `~/Library/Application Support/OpenApoc/OpenApoc/`.

`open ./build/bin/OpenApoc.app` is supported after a successful build but may resolve `./data` incorrectly depending on launch cwd. Prefer the Mach-O command above when debugging data-path issues.

On some Macs GLES 3.0 context creation fails; OpenApoc falls back to the GL 2.0 renderer automatically.

### Input harness (localhost)

A loopback TCP command channel can inject the same mouse/key events SDL would, plus take a screenshot of the last presented frame. It is **off by default** and binds `127.0.0.1` only. Do not persist `Framework.Harness.Enable` in `OpenApoc_settings.conf` (`--Config.Save=0` on the launch line).

```sh
./build/bin/OpenApoc.app/Contents/MacOS/OpenApoc \
  --Framework.Data="$PWD/data" \
  --Framework.CD="$PWD/data/cd.iso" \
  --Framework.Harness.Enable=1 \
  --Framework.Harness.Port=17321 \
  --Game.SkipIntro=1 \
  --Config.Save=0 \
  --Config.Read=0 \
  --Framework.AudioBackends=null
```

Then, from another terminal:

```sh
python3 tools/oa_harness.py status
python3 tools/oa_harness.py click 320 200
python3 tools/oa_harness.py key Escape
python3 tools/oa_harness.py screenshot /tmp/oa.png
python3 tools/oa_harness.py quit
```

Click/move coordinates are **display** pixels (the size `status` reports), not OS window pixels when the window is scaled. Key names are SDL names (`Escape`, `Return`, `Space`, `Left Shift`). Task: `openapoc: launch harness`.

## Automated play (harness)

The game can be driven over a localhost socket so a whole campaign runs with no human input.
Enable it at launch:

```sh
./build/bin/OpenApoc.app/Contents/MacOS/OpenApoc \
  --Framework.Data="$PWD/data" --Framework.CD="$PWD/data/cd.iso" \
  --Framework.Harness.Enable=1 --Framework.Harness.Port=17321 \
  --Game.SkipIntro=1 --Config.Save=0 --Config.Read=0 \
  --Framework.AudioBackends=null --OpenApoc.NewFeature.SeedRng=0
```

`SeedRng=0` keeps the RNG fixed; without it `GameState::startGame()` reseeds from the clock.

### Protocol

One command per line, one line back, `OK ...` or `ERR ...` (see `framework/harness.h`).

| Command | Purpose |
| ------- | ------- |
| `STATUS` | current stage class name, display size, mouse position |
| `CONTROLS` | list named widgets on the currently visible forms |
| `CONTROL <id> [click\|toggle\|set <value>]` | invoke a form control by id (no pixel math) |
| `ACTION <verb> [args...]` | same named-action table (`click`, `set`, `toggle`, `controls`, `help`) |
| `CLICK x y [left\|right\|middle]`, `MOVE`, `DOWN`, `UP`, `SCROLL` | mouse input (for nameless widgets: map tiles, list rows) |
| `KEY <name>`, `KEYDOWN`, `KEYUP`, `TEXT <s>` | keyboard input; names may contain spaces (`Left Shift`) |
| `SCREENSHOT <path>` | write a PNG |
| `RESIZE <w> <h>` | resize the window, to measure a fixed scene at several resolutions |
| `GS <query>` | inspect the running game (below) |
| `SAVE <path>` | write a save, for checkpointing a long campaign run |
| `QUIT` | quit cleanly |

`GS` accepts `time`, `funds`, `bases`, `research`, `orgs`, `vehicles`, `agents`, `turbo`,
`battle`, `stage`, `all`, plus the CityView-only `ufos_screen`, `vehicles_screen`,
`centre_on_ufo` and `centre_on_own`. Replies are one line of `key=value` pairs.

`GS` is answered by the game layer, not the framework: `OpenApoc_GameState` links
`OpenApoc_Framework` and never the reverse, so `harness.cpp` cannot name a `GameState`. The
handler is installed with `setHarnessQueryHandler()` (the same shape as `logger.h`'s
`LogFunction`) from both `CityView` and `BattleView`, so it survives the `REPLACEALL` transition
between city and battle.

`Framework.Harness.WarpCursor` (default off) moves the OS pointer to follow injected input. The
engine tracks the cursor from the event itself, so this is cosmetic — leave it off or a long run
will fight you for the mouse.

### Driving a campaign

```sh
python3 tools/oa_play.py --days 30 --leg 6 --out build/e2e
```

It launches the game itself, plays, and writes `game.log`, `warnings.txt`, `events.txt` and
screenshots to `--out`. `tools/oa_harness.py` remains available for one-off commands.

Two pieces make this work without hard-coded pixel positions:

- Named harness commands (`CONTROL BUTTON_NEWGAME`, `CONTROLS`) invoke live form widgets
  through `Control::click()` / setters. The screens still render; the driver does not have to
  synthesise mouse events for anything that has an id.
- `tools/oa_forms.py` still resolves shipped `data/forms/**.form` files to screen rects for
  nameless widgets (map tiles, generated list rows). `oa_play.py` tries `CONTROL <id>` first
  and falls back to a resolved click.
- Every popup in this engine is its own `Stage`, so `STATUS` alone identifies the screen. The
  driver maps stage -> response and *acts* on events (dispatching a squad to an alien incident,
  entering a base defence) rather than closing them, and falls back to Return/Escape on any
  stage it does not recognise so an unattended run cannot deadlock.

Runs launch with every `Notifications.City.*` / `Notifications.Battle.*` pause option off. Each
one opens a modal that stops the clock; useful for a human, pure interruption unattended.

### Pacing

Ticks are advanced per frame, so wall-clock speed is frame-rate bound. City speeds 1-4 give
roughly 30/120/240/360 ticks per second — about 6.7 real hours per game day at speed 4. Only
turbo (speed 5, a five-minute jump per frame) is fast enough for a long campaign, and
`GameState::canTurbo()` disables it whenever an aggressive hostile craft, a live projectile or an
attack mission exists on the current map. `GS turbo` reports that gate and its causes, which is
usually the answer to "why has the campaign stopped advancing".

## Cursor / VS Code tasks

Tasks live in [`.vscode/tasks.json`](../.vscode/tasks.json). Run via **Tasks: Run Task** or **Terminal → Run Build Task** (`Cmd+Shift+B`).

| Task | What it does |
| ------ | ---------------- |
| `openapoc: init submodules` | `git submodule update --init --recursive` |
| `openapoc: link game data` | Symlink `depot_7661/cd.iso` → `data/cd.iso` |
| `openapoc: configure` | CMake configure (RelWithDebInfo, tests off) |
| `openapoc: configure tests` | Same configure with `-DENABLE_TESTS=ON` |
| `openapoc: build` | Default **build** task (`Cmd+Shift+B`) |
| `openapoc: extract data` | Re-run `OpenApoc_DataExtractor` |
| `openapoc: regen compare report` | Regenerate `docs/original-game/compare-report.html` |
| `openapoc: check ignored binaries` | Fail if binaries or Ghidra DBs are tracked |
| `openapoc: configure and build` | Configure, then build |
| **`openapoc: test`** | Reconfigure with tests on, build, `ctest` |
| **`openapoc: run`** | **Fresh run:** clean runtime state → build → launch |
| `openapoc: run (open .app)` | Fresh run, then `open` the `.app` |
| `openapoc: launch only` | Launch without clean or rebuild |
| `openapoc: launch harness` | Launch with the localhost input harness on port 17321 |

Debug: [`.vscode/launch.json`](../.vscode/launch.json) — **OpenApoc (lldb)** with pinned data paths.

**`openapoc: run`** clears runtime state, rebuilds, then launches. **`openapoc: test`** reconfigures with tests on, builds, and runs `ctest`.

## Troubleshooting

| Symptom | Check |
| --------- | -------- |
| CMake: `CD_PATH ... non-existant file` | `test -f data/cd.iso`; run `openapoc: link game data` |
| Configure cannot find Qt6 | `CMAKE_PREFIX_PATH` includes `$(brew --prefix qt@6)`; Qt `bin` on `PATH` |
| Black window / missing palette | Log for `Failed to open palette`; confirm ISO mount, not `XCOMA/` |
| `No functional renderer found` | OpenGL.framework from CLT SDK; check configure log for OpenGL warnings |
| Empty `.app` from Finder | `cmake --build build --target extract-data` after build |
| Extractor CRC warnings | Expected if not using Steam ISO; fatal if `xcom3/...` files are missing |
