# City vehicles

Extractor tables for vehicle types, engines, weapons, cargo, slot layouts, and organisation vehicle parks already exist on non-4 UFO2P ([vehicle.h](../../../tools/extractors/common/vehicle.h), [vequipment.h](../../../tools/extractors/common/vequipment.h)). On the Steam `4` build those blobs sit at non-4 + 0xE00.

UFO2P strings that name the road layer: `Road type:`, `TJunction/Crossroads`, `Road Vehicle Weapons`, `Road Vehicle Engines / Fuel`, `An illegal road vehicle has been detected.`, `Traveling by people tube`, `People Tubes`. Those confirm a distinct ground-vehicle / tube network; they do not encode lane-keep rules.

OpenApoc runtime:

- [vehicle.cpp](../../../game/state/city/vehicle.cpp) — dodge/engagement FIXMEs; cloak and turning speed called approximate.
- [vehiclemission.cpp](../../../game/state/city/vehiclemission.cpp) — `getNextDestination`, `update`, `isFinishedInternal`, and some `start` paths still log `TODO: Implement`.
- Ground lanes / drive-on-right ([issue 785](https://github.com/OpenApoc/OpenApoc/issues/785)) are not implemented. [version01readme.txt](../../../tools/extractors/docs/version01readme.txt) warned that ordering ground vehicles can crash.

Organisation buy/sell of vehicles from funds and relationships is [issue 1053](https://github.com/OpenApoc/OpenApoc/issues/1053).
