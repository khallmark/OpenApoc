# OpenApoc — agent working agreement

This checkout is a **fork**. Ghidra and decompiler output are allowed, including
in the same session as game-logic edits. Prefer original-behavior evidence over
guessing.

Do **not** commit Steam/ISO binaries, Ghidra project databases (`.rep` / `.gpr`),
or `depot_7661/`. Listings and notes may live in `docs/original-game/`.

## Quick start (macOS)

```sh
git submodule update --init --recursive
ln -sfn "$PWD/depot_7661/cd.iso" data/cd.iso
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix boost);/opt/homebrew" \
  -DENABLE_TESTS=OFF -DBUILD_IMAGEDUMP=OFF -DBUILD_SERIALIZATIONTOOL=OFF \
  -S . -B build
cmake --build build -j$(sysctl -n hw.ncpu)
./build/bin/OpenApoc.app/Contents/MacOS/OpenApoc \
  --Framework.Data="$PWD/data" --Framework.CD="$PWD/data/cd.iso"
```

CMake **3.30+**. Use the ISO (`MUSIC` + `XCOM3/`), not `depot_7661/XCOMA/`.
Full setup: [docs/local-development.md](docs/local-development.md). Style:
[CODE_STYLE.md](CODE_STYLE.md) (C++17, tabs, clang-format 18).

## Verify

| Change | Command |
|--------|---------|
| Game logic | Task `openapoc: test`, or the command below |
| Extractor | `cmake --build build --target extract-data` |
| Ignore rules | `./tools/check_ignored_binaries.sh` |
| Format | `./tools/lint.sh` |
| Gap docs | `python3 tools/regen_compare_report.py` |

```sh
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix boost);/opt/homebrew" \
  -DENABLE_TESTS=ON -DBUILD_IMAGEDUMP=OFF -DBUILD_SERIALIZATIONTOOL=OFF \
  -S . -B build
cmake --build build -j$(sysctl -n hw.ncpu)
ctest --test-dir build --output-on-failure
```

Default `openapoc: configure` keeps `ENABLE_TESTS=OFF` for fast boots. Reconfigure
with tests on before claiming a logic change works. `test_serialize` equality is
disabled (`#if 0`); do not treat a green serialize test as a full roundtrip.

## Layout

| Path | Role |
|------|------|
| `game/state/` | City + battle simulation |
| `game/ui/` | Screens / forms |
| `tools/extractors/` | EXE table extractors (`common/ufo2p.*`, `tacp.*`) |
| `docs/original-game/` | Gap matrix, binary notes, compare report |
| `tests/` | Hand-rolled `ctest` mains + `test_helpers.h` |
| `docs/original-game/extractor-tables.md` | Extractor table index (graph skip-lists `tools/`) |
| `docs/solutions/` | Durable learnings |
| Sibling lab | `/Users/khallmark/Desktop/Code/OpenSource/OpenApoc-og-research` |
| `depot_7661/analysis/` | Private notes (gitignored; read via absolute path) |

## Original-game / Ghidra

Load [`.cursor/skills/openapoc-parity/SKILL.md`](.cursor/skills/openapoc-parity/SKILL.md)
for parity or Ghidra work.

- ISO **non-4** `UFO2P.EXE` / `TACP.EXE` = extractor-canonical offsets.
- Depot unsuffixed EXEs are the Pentium **`4`** pair. Per-table rebase; no global slide.
- Bound LE, community LX loader, `-processor x86:LE:32:default -cspec gcc`. Do not unbind.
- Work queue: [docs/original-game/openapoc-gap-matrix.md](docs/original-game/openapoc-gap-matrix.md)
  and [docs/original-game/next-implementation.md](docs/original-game/next-implementation.md).
- Evidence: `table` → `string` → `xref` → `decompiler` → `prior-art` → `openapoc-todo`.
  Cite binary + generation + file offset. Ghidra names (`FUN_*`) are fine if the
  offset is attached.
- Index OpenApoc with codebase-memory-mcp. **Do not** index the sibling lab or
  Ghidra C. Graph skip-lists `tools/`; grep `tools/extractors/common` for tables.
  Smoke test: `OpenApoc.City` in `game/state/city/city.h`.

## Cursor tasks

`.vscode/tasks.json`: `openapoc: run` (fresh launch), `openapoc: test` (enable
tests + `ctest`), `openapoc: extract data`.
