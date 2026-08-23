# City vehicles

Extractor tables for vehicle types, engines, weapons, cargo, slot layouts, and organisation vehicle parks already exist on non-4 UFO2P ([vehicle.h](../../../tools/extractors/common/vehicle.h), [vequipment.h](../../../tools/extractors/common/vequipment.h)). On the Steam `4` build those blobs sit at non-4 + 0xE00.

Hexa vehicle-data sample at `0x189C8C` labels byte `79 00` as “chance to evade bullets” ([hexa.txt](../../../tools/extractors/docs/hexa.txt)). OpenApoc ignores that field and uses attack-mode dodge 100 / 80 / 50 / 10 ([vehicle.cpp](../../../game/state/city/vehicle.cpp)). Reload is instant ([vequipment.cpp](../../../game/state/city/vequipment.cpp)).

UFO2P strings that name the road layer: `Road type:`, `TJunction/Crossroads`, `Road Vehicle Weapons`, `Road Vehicle Engines / Fuel`, `An illegal road vehicle has been detected.`, `Traveling by people tube`, `People Tubes`. Those confirm a distinct ground-vehicle / tube network; they do not encode lane-keep rules.

OpenApoc runtime:

- [vehiclemission.cpp](../../../game/state/city/vehiclemission.cpp) — all mission types have defensive `getNextDestination` / `update` / `start` defaults. `connection[dir]` is used for road and ATV tiles.
- Ground lanes / large-vehicle occupancy ([issue 785](https://github.com/OpenApoc/OpenApoc/issues/785)) are still incomplete. [version01readme.txt](../../../tools/extractors/docs/version01readme.txt) warned that ordering ground vehicles can crash.

Organisation park refill now checks funds and `purchase()` credits the seller. Selling surplus above park cap is still open ([issue 1053](https://github.com/OpenApoc/OpenApoc/issues/1053)).
