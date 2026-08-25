# Address maps

Do not duplicate [tools/extractors/docs/hexa.txt](../../../tools/extractors/docs/hexa.txt) here.

Extractor headers under [tools/extractors/common](../../../tools/extractors/common) are file offsets into the **ISO non-4** `UFO2P.EXE` and `TACP.EXE`.

This-build and `4`-build file offsets live only in the sibling lab:

- `/Users/khallmark/Desktop/Code/OpenSource/OpenApoc-og-research/labels/ufo2p_rebase.csv`
- `/Users/khallmark/Desktop/Code/OpenSource/OpenApoc-og-research/labels/tacp_rebase.csv`

Columns: `name`, `file_off_non4_hex`, `file_off_non4_dec`, `file_off_4_hex`, `file_off_4_dec`, `delta_hex`, `delta_dec`, `method`, `confidence`.

Observed slides (not a global rule):

- UFO2P `4` vs non-4: most located tables are `+0xE00`.
- TACP `4` vs non-4: most located tables are `-0x2200`.
- `crew_ufo_downed` at non-4 `0x13E560` (4-build `0x13F360`, object-2 VA `0x112160`) is 560 B and byte-identical on both builds. The old “all zeros” rebase row is wrong.

Ghidra `locateAddressesForFileOffset` on a bound LE hits the raw file overlay. The same table bytes also exist in the remapped LE data object (UFO2P object 2; typical `file − VA = 0x2C400` for mapped tables). Vehicle / economy / park / rawsound sit in `file_tail` and have no VA. Cite a virtual address in this tree only with the binary, generation, and bound-file offset beside it. Bound Ghidra program addresses from the 23 August `LeLoader` import are a third space (example: `Mind Shield` file `0x149A4C` → VA `0x11D64C` → Ghidra `0xF83A8`).
