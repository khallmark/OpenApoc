# Original-game analysis

Map of X-COM Apocalypse binaries onto this OpenApoc fork. Ghidra projects, bound EXEs, and Steam/ISO copies live in the sibling lab:

`/Users/khallmark/Desktop/Code/OpenSource/OpenApoc-og-research`

This is a fork. Ghidra and decompiler output are allowed here and in the same session as `game/` edits.

## Isolation (binaries, not listings)

- Do not commit `depot_7661/`, Ghidra databases (`.rep` / `.gpr`), or original EXEs. Place the Steam depot beside the repo and symlink `data/cd.iso` — see [../local-development.md](../local-development.md).
- Listings, `FUN_*` names, and reconstructed identifiers are allowed in this folder if they carry a binary + generation + file offset.
- Weight evidence as table / string / xref / decompiler / prior-art / openapoc-todo.
- Do not index Ghidra C in codebase-memory-mcp. Index OpenApoc only.
- Codebase-memory skip-lists `tools/`, so the extractor class `OpenApoc::UFO2P` is not in the graph. Grep `tools/extractors/common`. The smoke test is `OpenApoc.City` in `game/state/city/city.h`.

## Method

1. Treat ISO `UFO2P.EXE` / `TACP.EXE` (non-4) as the extractor-canonical pair.
2. Treat depot / ISO `UFO2P4.EXE` / `TACP4.EXE` as the Steam-running pair (this depot ships the `4` files under the un-suffixed names).
3. Confirm every extractor table at the non-4 file offset, then relocate the same bytes on the `4` build. Never assume a global slide.
4. Import bound Linear Executables with the community LX loader. Do not unbind; extractor offsets are bound-file offsets.
5. Seed labels from the matching rebase column. Walk strings, xrefs, and decompiler for systems OpenApoc still approximates.
6. Record gaps in [openapoc-gap-matrix.md](openapoc-gap-matrix.md). The finished extract↔runtime compare is [compare-report.html](compare-report.html) (pass 5 is the bound xref census: 24/14 live xrefs, gap needles empty). Extractor index: [extractor-tables.md](extractor-tables.md).
7. After matrix or TODO changes: `python3 tools/regen_compare_report.py`. Bound Ghidra addresses are not file offsets; cite binary + generation + file offset.

## Spec template for binary notes

Each `binaries/*.md` file uses:

- Role in the original process graph
- Format and compiler (MZ vs LE+DOS4GW, Watcom)
- Identity (size, CRC32, SHA256)
- OpenApoc mapping (paths)
- Known tables (names + which generation the offset applies to)
- Gaps (issue numbers, evidence kind, confidence)

## Validation

```bash
./tools/check_ignored_binaries.sh
git check-ignore -v depot_7661/cd.iso
```

`depot_7661/` is ignored via the root `.gitignore`.
