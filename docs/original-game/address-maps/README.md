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
- `crew_ufo_downed` at the documented non-4 offset is all zeros; do not stamp a `4` address from that needle.

Ghidra `locateAddressesForFileOffset` on a bound LE hits the raw file overlay. The same table bytes also exist in the remapped LE data object (UFO2P data object starts at the usual Watcom data base; TACP’s data object uses the next selector). Sibling `labels/offset_to_va.txt` records the overlay-to-object match. Do not paste those virtual addresses into this tree.
