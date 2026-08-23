# City vehicles

Extractor tables for vehicle types, engines, weapons, cargo, slot layouts, and organisation vehicle parks already exist on non-4 UFO2P ([vehicle.h](../../../tools/extractors/common/vehicle.h), [vequipment.h](../../../tools/extractors/common/vequipment.h)). On the Steam `4` build those blobs sit at non-4 + 0xE00.

Hexa vehicle-data sample at `0x189C8C` labels `79 00` at +0x28 as “chance to evade bullets” ([hexa.txt](../../../tools/extractors/docs/hexa.txt)). That field is `VehicleData::loftemps_index` (file `0x189CB4` for vehicle 0). The extractor uses it for misaligned voxelmaps — the original “dodge cheat” ([extract_vehicles.cpp](../../../tools/extractors/extract_vehicles.cpp)). Attack-mode dodge is a **second** path: hardcoded 100 / 80 / 50 / 10 ([vehicle.cpp](../../../game/state/city/vehicle.cpp):167). `unknown2` / `unknown3` are not mapped.

Ammo refill is instant ([vequipment.cpp](../../../game/state/city/vequipment.cpp):190). Inter-shot delay already uses extracted `fire_delay` × `VEQUIPMENT_RELOAD_TIME_MULTIPLIER(2)` × `TICKS_MULTIPLIER(4)` = ×8. UI `Reload time:` at UFO2P non-4 `0x151086` has empty bound xrefs. There is no separate mag-reload duration field in `VehicleWeaponData` (only `fire_delay` at +8, table `0x18B1E0`).

UFO2P strings that name the road layer: `Road type:`, `TJunction/Crossroads`, `Road Vehicle Weapons`, `Road Vehicle Engines / Fuel`, `An illegal road vehicle has been detected.`, `Traveling by people tube`, `People Tubes`. Those confirm a distinct ground-vehicle / tube network; they do not encode lane-keep rules.

OpenApoc runtime:

- [vehiclemission.cpp](../../../game/state/city/vehiclemission.cpp) — all mission types have defensive `getNextDestination` / `update` / `start` defaults. `connection[dir]` is used for road and ATV tiles.
- Ground 1-tile occupancy now uses the same `intersectingObjects` vehicle block as flyers. Large-vehicle footprint and engagement tables ([issue 785](https://github.com/OpenApoc/OpenApoc/issues/785)) are still incomplete. [version01readme.txt](../../../tools/extractors/docs/version01readme.txt) warned that ordering ground vehicles can crash.

Organisation park refill now checks funds and `purchase()` credits the seller. Surplus idle non-liner park vehicles sell at `currentPrice` ([issue 1053](https://github.com/OpenApoc/OpenApoc/issues/1053)).
