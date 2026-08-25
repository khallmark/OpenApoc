# Binary inventory

Source: Steam depot `7661` plus the bundled `cd.iso` (`XCOM APOC`). CRC32 is zlib/ISO-HDLC, the same family OpenApoc extractors use.

## Game executables

| File | Generation | Size | CRC32 | SHA256 (prefix) | Format |
| ------ | ------------ | ------ | ------- | ----------------- | -------- |
| ISO `XCOM3/UFOEXE/UFO2P.EXE` | non-4, extractor-canonical | 1,702,206 | `0x4749ffc1` | `99f8787d5f1cb532` | Watcom LE + DOS4GW |
| ISO `XCOM3/UFOEXE/UFO2P4.EXE` = depot `XCOMA/UFOEXE/UFO2P.EXE` | Pentium `4`, Steam-running | 1,705,790 | `0xdbd3b41d` | `997b43d64d49f4b4` | Watcom LE + DOS4GW |
| ISO `XCOM3/TACEXE/TACP.EXE` | non-4, extractor-canonical | 3,170,298 | `0xfebbe39e` | `a3143a692486bb0d` | Watcom LE + DOS4GW |
| ISO `XCOM3/TACEXE/TACP4.EXE` = depot `XCOMA/TACEXE/TACP.EXE` | Pentium `4`, Steam-running | 3,161,594 | `0x3ec9c268` | `b2cd9782ec812cae` | Watcom LE + DOS4GW |
| ISO/depot `SMKP.EXE` | single build | 271,436 | `0x93a0e620` | `f8cff3dde07efa13` | Watcom LE + DOS4GW |
| ISO/depot `XCOMAPOC.EXE` | single build | 18,171 | `0x991a9adf` | `123de1e6b94e0e75` | 16-bit Watcom MZ |

Extractor expected CRCs in [ufo2p.cpp](../../tools/extractors/common/ufo2p.cpp) and [tacp.cpp](../../tools/extractors/common/tacp.cpp) match the **ISO non-4** pair, not this depot’s renamed `4` files.

## Skip

| File | Why |
| ------ | ----- |
| `SETUP.EXE` / ISO `INSTALL/ESETUP.EXE` | Sound/setup UI, no gameplay tables |
| ISO `INSTALL.EXE`, `GSETUP.EXE`, `AUTORUN.EXE`, `WIN95INS.EXE`, `W95UNINS.EXE`, `MEMCHECK.OVL` | Installer only |
| `HMIDRV.386`, `HMIDET.386`, `HMIMDRV.386`, `HMIDET.DRV` | Miles/HMI audio middleware |
| `SOUND/PLAYER.OVL` | Bound LE audio player, third-party |
| `dosbox.exe`, `SDL.dll`, `SDL_net.dll`, `zmbv.dll` | Steam DOSBox 0.72 wrapper |

## ISO-only data (not in extracted `XCOMA/`)

`MAPS/` (tactical buildings), `SMK/`, `TACINI/`, and a 312 MB `MUSIC` track. OpenApoc needs these via `data/cd.iso` at extract/runtime. Wiring that path is out of scope for this analysis.
