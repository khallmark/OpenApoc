# LX / LE import

Watcom Linear Executables bound to DOS4GW. Extractor offsets are **bound-file
offsets**. Do not unbind with DOS32A.

## Environment

From `/Users/khallmark/Desktop/Code/OpenSource/OpenApoc-og-research`:

```sh
./scripts/ghidra_env.sh          # OpenJDK 21 + Ghidra 12.1.3 on PATH
./scripts/extract_canonical_exes.py   # ISO → canonical/ if missing
./scripts/import_le.sh <exe> [file_offset] [--analyze]
./scripts/rebase_tables.py
```

`import_le.sh` must pass `-processor x86:LE:32:default -cspec gcc` so the
community LX loader wins. Do not analyze the DOS4GW stub.

Prebuilt lx-loader 12.0.1 crashes on Ghidra 12.1.3 (`setFieldName` ABI). Use
the rebuilt tree under `extensions/ghidra-lx-loader-src` (installed in the
user Ghidra settings dir, not Homebrew Cellar).

## Generations

| Pair | Files | Role |
|------|--------|------|
| non-4 | ISO `UFO2P.EXE`, `TACP.EXE` | Extractor-canonical offsets |
| `4` | ISO `UFO2P4.EXE`, `TACP4.EXE`; depot unsuffixed names | Steam-running pair |

Confirm each extractor table at the non-4 file offset, then relocate the same
bytes on the `4` build. Rebase CSVs: `labels/ufo2p_rebase.csv`,
`labels/tacp_rebase.csv`. Never assume a global slide (`+0xE00` / `−0x2200`
are per-table observations, not a rule).

## After import

`DumpLeImport.java` logs LE objects, memory blocks, string xrefs, optional
file-offset → VA. String catalogs: `export/strings/`.

Generic `devkit:ghidra-headless` export scripts assume PE/ELF. Prefer these
lab scripts for UFO2P / TACP / SMKP.
