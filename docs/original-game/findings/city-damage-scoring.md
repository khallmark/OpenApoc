# Damage to City: who is charged, and for what

**BOUND:** UFO2P.EXE ISO non-4 (SHA-256 `99f8787d5f1cb532e2620af2130bffb2558a869f9d4540d7fcf1b7487833d00f`).
Category 6 is written in exactly two places: the projectile hit at file `0xB8AFC` and the vehicle
obstruction collision at file `0xA423D`.

Static investigation, Ghidra 12.1.4 `-readOnly -noanalysis` on the `OpenApocOG` project, plus a
byte scan of the executable for every call to the score updater (file `0xFAA80`). That scan found
nine direct calls, one of them missing from Ghidra's xrefs, and exactly two that pass category 6.
No runtime session of the original. Offsets are physical file offsets (code: file = VA + `0x626A4`).

## Score storage

Eight records of two int32, at file `0x142300`: this week, then the previous weeks. Damage to City
is category 6 (this week file `0x142330`, previous weeks file `0x142334`). The displayed total is
this week + previous weeks. The rollover (file `0xFA234`) adds each week into its previous-weeks
member and zeroes the week. The score screen (file `0xFA2F4`) resolves the label as string group
`0x25`, item `0x9A`.

## Projectile hits (file `0xB70CC`, deduction at file `0xB8AFC`)

The projectile update charges a scenery hit when all of these hold:

- the current city is the human city (the alien city skips the whole score block);
- the hit lies inside a building's footprint;
- the projectile's firer vehicle is still an active slot;
- that vehicle's organisation is **0 (X-COM) or 1 (Alien)**. Other organisations skip the
  deduction. The organisation-name table at file `0x14AF11` binds 0 = X-COM, 1 = Alien.

It subtracts the scenery type's unsigned value byte (record `+0x0C`, imported as
`SceneryTileType::value`), read from the tile type **before** the hit. The deduction happens on
the qualifying **hit**: the scenery-damage test (file `0xBE3AC`) runs first, and its success
result is checked only after the score call. The tile does not have to be destroyed.

## Vehicle obstruction (file `0xA3CE8`, deduction at file `0xA423D`)

A separate routine scans a moving vehicle's swept footprint for obstructing scenery. With collision
state `vehicle+0xF6 == 5` it removes the tile (file `0xA3EEC`), finds the building covering it and
charges the saved value. The responsible organisation is hard-coded to 1 rather than read from
the vehicle, so this charge applies **whoever owns the vehicle**. It has no human-city check of its
own; its caller filters vehicles to the current city. That this path fires in the alien city in
practice is untested.

## Not charged

The shared tile-change routine (file `0xE1CD8`) and the secondary-collapse routines that call it
contain no score update. Collapses and generic scenery death are not charged.

## What OpenApoc does with this

- `Scenery::hitChargesCityDamage` and `Scenery::chargeCityDamage` charge a projectile hit on
  building scenery in the human city fired by X-COM or the Aliens, by the pre-hit value.
- The falling-vehicle plow-through charges regardless of owner.
- `Scenery::die` no longer charges every destroyed tile, so collapses and other organisations' fire
  cost X-COM nothing.
- `tests/test_city_damage_score.cpp` covers who is charged and checks that the charge lands in both
  the week and the running total.
