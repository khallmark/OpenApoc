# Battle hazards (enzyme, fire, cloak)

[version01readme.txt](../../../tools/extractors/docs/version01readme.txt) lists enzyme, fire, and cloaking as approximations. [battlehazard.cpp](../../../game/state/battle/battlehazard.cpp) has a “made up” comment. [battlehazard.h](../../../game/state/battle/battlehazard.h) flags a made-up constant.

TACP strings name `Entropy Enzyme`, `Personal Cloaking Field`, `Gas`, `Fire`, `Stun Gas`, `FireGrenade`, and RAW paths `FIREXPLS` / `GASEXPLS`. Equipment names also come from TACP agent-general tables (non-4 offsets in [aequipment.h](../../../tools/extractors/common/aequipment.h); `4` build at −0x2200). Behavior of spread, duration, and cloak detection still needs original observation plus targeted TACP xrefs — not a full function dump.
