---
name: openapoc-parity
description: >-
  Maps original X-COM Apocalypse binaries onto this OpenApoc fork using Ghidra,
  extractor tables, and the gap matrix, then implements the matching game logic.
  Use when working on original-game parity, UFO2P/TACP/SMKP, gap-matrix rows,
  next-implementation targets, LX/LE import, rebase CSVs, TPS, battle AI, or
  city missions. Triggers on Ghidra, decompile, extractor offsets, issue 997,
  263, 264, 265, 785, 1053.
---

# OpenApoc parity

Ghidra is allowed. Same-session decompile + implement is allowed.

Do not load the whole generic Ghidra skill family. For these EXEs use the
sibling-lab scripts below, not stock `ghidra-analyze.sh` (PE/ELF defaults).
Read [lx-import.md](lx-import.md) before the first import.

## Paths

| What | Where |
|------|--------|
| Gap matrix | `docs/original-game/openapoc-gap-matrix.md` |
| Implement queue | `docs/original-game/next-implementation.md` |
| Extractor tables | `tools/extractors/common/ufo2p.h`, `tacp.h` |
| Lab | `/Users/khallmark/Desktop/Code/OpenSource/OpenApoc-og-research` |
| Analysis notes | `depot_7661/analysis/` (gitignored; readable) |

## Workflow

1. Pick one matrix row or `next-implementation.md` item.
2. Confirm the table on the **ISO non-4** EXE, then the same bytes on the **`4`**
   build. No global slide.
3. Pull strings, xrefs, and decompiler for that system. Cite binary + generation
   + file offset. `FUN_*` names are fine with an offset.
4. Edit OpenApoc. Match original behavior; do not invent hazard/TPS constants.
5. Update the matrix row status. Keep listings in docs if they help the next pass.
6. If the matrix or TODOs changed: `python3 tools/regen_compare_report.py`.
7. Verify:

```sh
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix boost);/opt/homebrew" \
  -DENABLE_TESTS=ON -DBUILD_IMAGEDUMP=OFF -DBUILD_SERIALIZATIONTOOL=OFF \
  -S . -B build
cmake --build build -j$(sysctl -n hw.ncpu)
ctest --test-dir build --output-on-failure
./tools/check_ignored_binaries.sh
```

## Lab import (do not unbind)

```sh
cd /Users/khallmark/Desktop/Code/OpenSource/OpenApoc-og-research
./scripts/ghidra_env.sh
./scripts/import_le.sh canonical/SMKP.EXE "" --analyze
./scripts/import_le.sh canonical/UFO2P.EXE 0x13EE80
./scripts/import_le.sh canonical/TACP.EXE
```

`-processor x86:LE:32:default -cspec gcc`. Rebuilt LX loader lives under
`extensions/ghidra-lx-loader-src` (Homebrew lx-loader 12.0.1 crashes on Ghidra
12.1.3).

## Evidence

`table` → `string` → `xref` → `decompiler` → `prior-art` → `openapoc-todo`.

ISO non-4 = extractor-canonical. Depot unsuffixed names = `4` pair. Graph
skip-lists `tools/`; grep extractors. Do not index lab C in codebase-memory.

## Do not

- Commit `depot_7661/`, `data/cd.iso`, `.rep`, `.gpr`, or original EXEs
- Use `XCOMA/` as `Framework.CD`
- Treat a green `test_serialize` as a full save roundtrip (`#if 0` equality)
