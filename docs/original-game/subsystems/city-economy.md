# City economy and organisations

Non-4 UFO2P already holds research, funding, market, and starting-relationship tables. Manufacturing lives in hexa `manufacturing_data` at `0x13FD34` and is **not** extracted; OpenApoc patches engineering topics in `data/common_patch/gamestate/research.xml`. Weekly funding and most of the city economy are implemented (see alpha milestone [issue 263](https://github.com/OpenApoc/OpenApoc/issues/263)).

Stores-capacity halt exists for manufactured equipment (`ManufactureHalted`). Craft output sets `storeSpace = 0` and skips that gate. Original string `Manufacturing halted` is at UFO2P non-4 `0x152458`.

`Organisation::purchase` settles both sides via `settleMarketPurchase`. `Cargo::refund` then credits the buyer and debits `originalOwner`, which can reverse the seller credit on expiry.

UFO2P strings: `Diplomacy`, `WEEKLY FUNDING ASSESSMENT`, `Funding adjustment>`, Senate hostility / reduced / increased / ceased funding copy (`0x154434`, `0x154505`, `0x15455A`), `ALIEN INFILTRATION`, `Alien infiltration graph` (`0x15482B`), and the Transtellar people-tube / diplomatic-relations failure line.

Remaining:

- Full two-way diplomacy with short/long bias ([issue 996](https://github.com/OpenApoc/OpenApoc/issues/996))
- Organisations selling surplus park vehicles ([issue 1053](https://github.com/OpenApoc/OpenApoc/issues/1053))
- Infiltration graph and UFOPaedia both plot `infiltrationValue / 2` (runtime 0..200 → display 0..100) ([infiltrationscreen.cpp](../../../game/ui/city/infiltrationscreen.cpp))
- Senate attitude table (`senate_relationships` `0x15055B`) vs patch `weekly_rating_rules`
- Training rates still inherit the 4× tick in places ([issue 997](https://github.com/OpenApoc/OpenApoc/issues/997))
