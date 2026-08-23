# Alien dimension and city UFO flow

Tables already extracted from non-4 UFO2P: UFO mission data, crew, drop troops, growth rates, mission patterns, alien building defenses ([hexa.txt](../../../tools/extractors/docs/hexa.txt)).

Still missing or approximate in OpenApoc ([issue 264](https://github.com/OpenApoc/OpenApoc/issues/264)):

- Portal locations and portal movement in the city — strings `Dimension Gates`, `Click on Dimension Gate to set destination`, `Switching to Alien Dimension`, `Go into Dimension Gate`
- Umbilical collapse in the alien dimension — **no printable `umbilical` in UFO2P**
- Overspawn — strings `Overspawn`, `Overspawn Autopsy`, “extremely dangerous Alien terror weapon”; [gamestate.cpp](../../../game/state/gamestate.cpp) still logs `Implement Overspawn, just attacking for now`
- UFO mushrooms as next-week spawn feedback — **no printable `mushroom` in UFO2P**
- Stop UFO growth when the relevant building is destroyed
- Large-UFO bombing after first alien-dimension entry
- City-wide “Apocalypse” attack after the control centre dies

[city.cpp](../../../game/state/city/city.cpp) still has a FIXME that alien-city portals stay put.
