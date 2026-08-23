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
