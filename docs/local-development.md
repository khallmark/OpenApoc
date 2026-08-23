# Local development (macOS)

This checkout is set up for engine work against a **Steam depot** of X-COM Apocalypse. The original game files are not committed; only the OpenApoc source and extracted/generated data under `data/` are in git.

For reverse-engineering notes on the original binaries, see [original-game/](original-game/README.md).

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

On macOS, the build also copies `data/` into `build/bin/OpenApoc.app/Contents/Resources/data`. If `open` launches an empty bundle, run `extract-data` again after the `.app` exists.

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

Or run the Cursor task `openapoc: test`. Confirm depot/ISO stay untracked:

```sh
./tools/check_ignored_binaries.sh
```

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
| `openapoc: configure and build` | Configure, then build |
| **`openapoc: test`** | Reconfigure with tests on, build, `ctest` |
| **`openapoc: run`** | **Fresh run:** clean runtime state → build → launch |
| `openapoc: run (open .app)` | Fresh run, then `open` the `.app` |
| `openapoc: launch only` | Launch without clean or rebuild |

**`openapoc: run`** clears `log.txt`, `OpenApoc_settings.conf`, `saves/`, stops any running `OpenApoc`, rebuilds incrementally, then launches with pinned `Framework.Data` / `Framework.CD`.

## Troubleshooting

| Symptom | Check |
| --------- | -------- |
| CMake: `CD_PATH ... non-existant file` | `test -f data/cd.iso`; run `openapoc: link game data` |
| Configure cannot find Qt6 | `CMAKE_PREFIX_PATH` includes `$(brew --prefix qt@6)`; Qt `bin` on `PATH` |
| Black window / missing palette | Log for `Failed to open palette`; confirm ISO mount, not `XCOMA/` |
| `No functional renderer found` | OpenGL.framework from CLT SDK; check configure log for OpenGL warnings |
| Empty `.app` from Finder | `cmake --build build --target extract-data` after build |
| Extractor CRC warnings | Expected if not using Steam ISO; fatal if `xcom3/...` files are missing |
