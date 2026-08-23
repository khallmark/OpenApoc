# SMKP.EXE

- Role: Smacker cutscene player spawned by `XCOMAPOC.EXE`.
- Format: Watcom LE + DOS4GW, 271,436 bytes, CRC `0x93a0e620` (same on ISO and depot).
- OpenApoc mapping: [framework/video/smk.cpp](../../../framework/video/smk.cpp) via vendored libsmacker. Runtime plays `smk/*.smk` from the mounted CD.

## Evidence

Bound LE import (community LX loader, do not unbind) produces two LE objects at the usual Watcom bases plus a bound-file overlay. After analysis, printable strings `smk/intro1.smk`, `xcom3/`, `xcom3.cfg`, and `GOFORIT` have live xrefs. Those same video paths also appear in `XCOMAPOC.EXE`.

Also present: `WVIDEO`, `WATCOM C/C++32`, `smk/lose1.smk`, `smk/wingame2.smk`, and DOS4GW banners. This is a video helper, not city or battle rules.

## Gaps

Original dynamic/action music mixing is **not** in this binary; that lives in UFO2P/TACP plus the ISO `MUSIC` track ([issue 618](https://github.com/OpenApoc/OpenApoc/issues/618)).
