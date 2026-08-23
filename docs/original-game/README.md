# Original-game analysis

Clean-room map of X-COM Apocalypse binaries onto OpenApoc. This folder is the public, in-repo deliverable. Ghidra projects, decompiler exports, and Steam/ISO executables live only in the sibling lab:

`/Users/khallmark/Desktop/Code/OpenSource/OpenApoc-og-research`

## Isolation rules

- Do not commit `depot_7661/`, Ghidra databases, decompiled C, or sibling-lab catalogs. Place the Steam depot beside the repo and symlink `data/cd.iso` — see [../local-development.md](../local-development.md).
- Do not paste decompiler listings into this tree.
- Forbidden residue: Ghidra auto-generated function names, default labels, default data labels, `undefined` plus a width, and `param` plus an underscore and index. Write behavior, not reconstructed identifiers.
- Allowed evidence: table names, file offsets from the rebase CSVs, printable strings, OpenApoc paths, GitHub issue numbers.
- Weight evidence as table / string / xref first. Decompiler-only claims stay `confidence=low` and are written as behavior, not as reconstructed source.
- Do not index Ghidra C in codebase-memory-mcp. Index OpenApoc only.
- Codebase-memory skip-lists `tools/` and `docs/`, so the extractor class `OpenApoc::UFO2P` is not in the graph. The smoke test is `OpenApoc.City` in `game/state/city/city.h`.
- Do not write OpenApoc game logic in the same session that reads decompiler output.

## Method

1. Treat ISO `UFO2P.EXE` / `TACP.EXE` (non-4) as the extractor-canonical pair.
2. Treat depot / ISO `UFO2P4.EXE` / `TACP4.EXE` as the Steam-running pair (this depot ships the `4` files under the un-suffixed names).
3. Confirm every extractor table at the non-4 file offset, then relocate the same bytes on the `4` build. Never assume a global slide.
4. Import bound Linear Executables with the community LX loader. Do not unbind; extractor offsets are bound-file offsets.
5. Seed labels from the matching rebase column. Walk strings and xrefs for systems OpenApoc still approximates.
6. Record gaps in [openapoc-gap-matrix.md](openapoc-gap-matrix.md).

## Spec template for binary notes

Each `binaries/*.md` file uses:

- Role in the original process graph
- Format and compiler (MZ vs LE+DOS4GW, Watcom)
- Identity (size, CRC32, SHA256)
- OpenApoc mapping (paths)
- Known tables (names + which generation the offset applies to)
- Gaps (issue numbers, evidence kind, confidence)

## Validation

From the sibling lab:

```bash
/Users/khallmark/Desktop/Code/OpenSource/OpenApoc-og-research/scripts/check_docs_clean.sh
git check-ignore -v depot_7661/cd.iso
```

`depot_7661/` is ignored via the root `.gitignore`.
