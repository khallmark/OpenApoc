# Alien dimension and city UFO flow

`UFO_mission_data` at file `0x13DDFC` (VA `0x1119FC`) and `UFO_growth_rates` at `0x155010` (VA `0x128C10`) are extracted (`ufoincursion.h`, `ufogrowth.h`). Do not XInclude the reference XMLs — serialize appends vectors and doubles fleet counts. `UFO_mission_patterns` at `0x155164` (VA `0x128D64`) is still a patch stand-in (`ufo_mission_preference.xml`). Do not use file `0x154710` for growth. Crew / drop-troop / alien-building-defense tables **are** extracted (`crew_ufo_downed` `0x13E560` / VA `0x112160`, P↔P4 identical). Bound Scenario Generator xrefs are empty.

Implemented: dest-gate pairing (`destinationPortalIndex`), Overspawn `InfiltrateSubvert`, organic-factory growth gate. Reference EXE dumps live in [exe-tables](../exe-tables/).

Still missing or approximate in OpenApoc ([issue 264](https://github.com/OpenApoc/OpenApoc/issues/264)):

- Umbilical collapse — **no printable `umbilical` in UFO2P**
- UFO mushrooms as next-week spawn feedback — **no printable `mushroom` in UFO2P**
- Large-UFO bombing after first alien-dimension entry — no recovered trigger table
- City-wide “Apocalypse” attack after the control centre dies — no recovered trigger table
