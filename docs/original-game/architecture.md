# Original process architecture

`XCOMAPOC.EXE` is a 16-bit Watcom launcher. Printable strings name the three children and the videos they play:

- `ufoexe/ufo2p.exe` — cityscape
- `tacexe/tacp.exe` — battlescape
- `ufoexe/smkp.exe` — Smacker (`smk/intro1.smk`, `smk/lose1.smk`, `smk/wingame2.smk`)
- Config: `xcom3.cfg`, optional `SKIP`

OpenApoc collapses this into one process: [framework](../../framework) + [game/state](../../game/state) + [game/ui](../../game/ui). Handoff from city to battle is an in-process stage change, not an EXE spawn.

Both strategy and tactical EXEs are Watcom Linear Executables bound to DOS4GW. Each ships a Pentium `4` twin. This Steam depot runs the `4` files under the un-suffixed names. Extractors were written against the non-4 pair.

Static rules live as blobs inside those EXEs. OpenApoc copies them out at build time (`OpenApoc_DataExtractor`) into XML under `data/mods/base`, then mounts the CD at runtime for pictures, samples, maps, and music.

City and battle also own their own music mixer. UFO2P strings include `Action music`, `getnextmusic`, and `/MUSIC/GROUP_*` RAW paths. SMKP only plays the named SMK files. SETUP.EXE is sound-card UI only; Miles/HMI, `PLAYER.OVL`, DOSBox, and SDL are middleware (see [skipped-middleware.md](binaries/skipped-middleware.md)).
