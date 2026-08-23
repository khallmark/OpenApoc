# City economy and organisations

Non-4 UFO2P already holds research, manufacturing, funding, market, and starting-relationship tables. OpenApoc extractors emit those into `data/mods/base`. Weekly funding and a large part of the city economy are implemented (see alpha milestone [issue 263](https://github.com/OpenApoc/OpenApoc/issues/263)).

UFO2P strings: `Diplomacy`, `WEEKLY FUNDING ASSESSMENT`, `Funding adjustment>`, Senate hostility / reduced / increased / ceased funding copy, `ALIEN INFILTRATION`, `Alien infiltration graph`, and the Transtellar people-tube / diplomatic-relations failure line.

Remaining:

- Full two-way diplomacy with short/long bias ([issue 996](https://github.com/OpenApoc/OpenApoc/issues/996))
- Organisations buying/selling vehicles ([issue 1053](https://github.com/OpenApoc/OpenApoc/issues/1053))
- Infiltration graph curves ([infiltrationscreen.cpp](../../../game/ui/city/infiltrationscreen.cpp) TODO)
- Training rates wrong because of TPS ([issue 997](https://github.com/OpenApoc/OpenApoc/issues/997))
