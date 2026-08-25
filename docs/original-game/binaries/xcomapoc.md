# XCOMAPOC.EXE

- Role: 16-bit process launcher. Starts cityscape, battlescape, and the Smacker player.
- Format: Watcom C/C++16 MZ, 18,171 bytes, CRC `0x991a9adf`.
- OpenApoc mapping: [game/ui/general/mainmenu.cpp](../../../game/ui/general/mainmenu.cpp) and [game/ui/general/videoscreen.cpp](../../../game/ui/general/videoscreen.cpp) replace the launcher; there is no separate process spawn.

## Strings (printable)

- Child processes: `ufoexe/ufo2p.exe`, `tacexe/tacp.exe`, `ufoexe/smkp.exe`
- Videos: `smk/intro1.smk`, `smk/lose1.smk`, `smk/wingame2.smk`
- Config / flags: `xcom3.cfg`, `SKIP`, `GOFORIT`, `SOUND`, `TACTICAL`, `SAVEGAME`
- Path prefix: `xcom3/`

## Gaps

None for simulation. Useful only as the original process graph (see [architecture.md](../architecture.md)).
