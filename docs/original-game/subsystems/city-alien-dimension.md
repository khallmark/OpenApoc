# Alien dimension and city UFO flow

Hexa ranges still **not** in extractor headers: `UFO_mission_data` at file `0x13DDFC` (VA `0x1119FC`), `UFO_growth_rates` at `0x155010` (VA `0x128C10`), `UFO_mission_patterns` at `0x155164` (VA `0x128D64`). Do not use file `0x154710` for growth. Crew / drop-troop / alien-building-defense tables **are** extracted (`crew_ufo_downed` `0x13E560` / VA `0x112160`, P↔P4 identical). OpenApoc stands in with `ufo_incursions.xml` and `ufo_growth_lists.xml`. `ufo_mission_preference.xml` exists but is not XIncluded from `gamestate.xml`. Bound Scenario Generator xrefs are empty.

Still missing or approximate in OpenApoc ([issue 264](https://github.com/OpenApoc/OpenApoc/issues/264)):

- Destination gate selection — string `Click on Dimension Gate to set destination` at UFO2P non-4 `0x149537`. `leaveDimensionGate` picks a random portal from `city->portals`.
- Switching copy — `Switching to Alien Dimension` at `0x14D6C1`; `Go into Dimension Gate`.
- Fixed alien-map portals are extracted as hardcoded `initial_portals` in [extract_city_map.cpp](../../../tools/extractors/extract_city_map.cpp) (`92,45,9` / `94,43,9` / `95,46,9`).
- Umbilical collapse — **no printable `umbilical` in UFO2P**
- Overspawn — strings `Overspawn`, `Overspawn Autopsy`. Primaries now `InfiltrateSubvert`; `attackList` still `AttackBuilding`.
- UFO mushrooms as next-week spawn feedback — **no printable `mushroom` in UFO2P**
- Stop UFO growth when the relevant building is destroyed
- Large-UFO bombing after first alien-dimension entry
- City-wide “Apocalypse” attack after the control centre dies
