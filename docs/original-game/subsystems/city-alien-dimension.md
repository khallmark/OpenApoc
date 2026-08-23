# Alien dimension and city UFO flow

`UFO_mission_data` at file `0x13DDFC` (VA `0x1119FC`), `UFO_growth_rates` at `0x155010` (VA `0x128C10`), and `UFO_mission_patterns` at `0x155164` (VA `0x128D64`) are extracted (`ufoincursion.h`, `ufogrowth.h`, `ufomissionpattern.h`). Pattern slots: 3=Infiltration, 1=Attack, 2=Subversion, 5=Overspawn (19 weeks + DEFAULT). Do not XInclude the reference XMLs — serialize appends vectors and doubles weights. Do not use file `0x154710` for growth. Crew / drop-troop / alien-building-defense tables **are** extracted (`crew_ufo_downed` `0x13E560` / VA `0x112160`, P↔P4 identical). Bound Scenario Generator xrefs are empty.

Implemented: dest-gate pairing (`destinationPortalIndex`), Overspawn `InfiltrateSubvert`, organic-factory growth gate, unmanned RecoverVehicle loot (probe/scout have no `battle_map`; `Vehicle::loot` from `loaded_equipment_slots`), post-battle retreat / `alienMovement` housing (15 closest intact buildings within 15 tiles), role-11 escort `followVehicle` of `craft[follow_slot]` (`FUN_0006da88` @ file `0xD012C`; `0xFFFF` = no follow), `FUN_0003b724` @ file `0x2B723` zone/scatter spawn XY (`FUN_0005d1d8(scatter*2)` @ `0x4D1D7`), and `type_percent` × constitution (`VehicleType::health`) with the scatter>50→10 clamp. Role 8 is a real incursion slot; `tryMicronoidRain` → `takeOver` is an OpenApoc stand-in (`micronoidRainChance` is difficulty-patch only). `building_function` (+0x1B) is copied to vehicle `+0x171` and indexed into org tables at VA `0x1439E0` (`FUN_0006d384` @ file `0xCFD53`); that table rebuild + `FUN_0005e7f4` stay unbound. The `FUN_000588f8` `+0x168` gate stays extracted-only. Do not invent an `acquireTargetBuilding` name filter. Reference EXE dumps live in [exe-tables](../exe-tables/).

Still missing or approximate in OpenApoc ([issue 264](https://github.com/OpenApoc/OpenApoc/issues/264)):

- Umbilical collapse — **no printable `umbilical` in UFO2P**
- UFO mushrooms as next-week spawn feedback — **no printable `mushroom` in UFO2P**
- Large-UFO bombing after first alien-dimension entry — no recovered trigger table
- City-wide “Apocalypse” attack after the control centre dies — no recovered trigger table
