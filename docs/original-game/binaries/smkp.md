# SMKP.EXE

- Role: Smacker cutscene player spawned by `XCOMAPOC.EXE`.
- Format: Watcom LE + DOS4GW, 271,436 bytes, CRC `0x93a0e620` (same on ISO and depot).
- OpenApoc mapping: [framework/video/smk.cpp](../../../framework/video/smk.cpp) via vendored libsmacker. Runtime plays `smk/*.smk` from the mounted CD.

## Evidence

Bound LE import (community LX loader, do not unbind) produces two LE objects at the usual Watcom bases plus a bound-file overlay. After analysis, printable strings `smk/intro1.smk`, `xcom3/`, `xcom3.cfg`, and `GOFORIT` have live xrefs. Those same video paths also appear in `XCOMAPOC.EXE`.

Also present: `WVIDEO`, `WATCOM C/C++32`, `smk/lose1.smk`, `smk/wingame2.smk`, and DOS4GW banners. This is a video helper, not city or battle rules.

The only parity-relevant static artifact is the three-entry, 0x28-byte cutscene
manifest: bound file `0x3F920` / VA `0x304CC` (`intro1`), `0x3F948` /
VA `0x304F4` (`lose1`), and `0x3F970` / VA `0x3051C` (`wingame2`).
OpenApoc already consumes those three paths in boot and endgame flow. Codec,
sound-card, VGA/VESA, and DOS process-launch internals are replacement
middleware, not gameplay extractor targets.

## Gaps

Original dynamic/action music mixing is **not** in this binary; that lives in UFO2P/TACP plus the ISO `MUSIC` track ([issue 618](https://github.com/OpenApoc/OpenApoc/issues/618)).
