# City economy and organisations

Non-4 UFO2P already holds research, funding, market, and starting-relationship tables. Manufacturing is extracted from `manufacturing_data` at `0x13FD34` (names `0x1501F3`). Type-02 ammo IDs come from `craft_ammo_names` at `0x14B18E`, not the manufacture title. Patch `research.xml` still overlays descriptions and order. Weekly funding and most of the city economy are implemented (see alpha milestone [issue 263](https://github.com/OpenApoc/OpenApoc/issues/263)).

Stores-capacity halt exists for manufactured equipment (`ManufactureHalted`). Craft output sets `storeSpace = 0` and skips that gate. Original string `Manufacturing halted` is at UFO2P non-4 `0x152458`.

`Organisation::purchase` settles both sides via `settleMarketPurchase`. `Cargo::refund` credits the buyer only; the seller keeps the original `settleMarketPurchase` credit. Surplus idle park vehicles sell at `currentPrice` when count exceeds the park cap.

UFO2P strings: `Diplomacy`, `WEEKLY FUNDING ASSESSMENT`, `Funding adjustment>`, Senate hostility / reduced / increased / ceased funding copy (`0x154434`, `0x154505`, `0x15455A`), `ALIEN INFILTRATION`, `Alien infiltration graph` (`0x15482B`), and the Transtellar people-tube / diplomatic-relations failure line.

Remaining:

- Org-org bribe/rift dollar formulas ([issue 996](https://github.com/OpenApoc/OpenApoc/issues/996))
- Senate numeric funding bands stay patch `weekly_rating_rules` until `~0xF6EC7` is fully listed (`0x15055B` is UI copy)
- `craft_ammo_manufacturers_data` `0x13EB6A` uint16[15] disagrees with patch on Zorium (EXE X-COM vs patch Solmine) — leave patch until a consumer is decompiled
- Training rates still inherit the 4× tick in places ([issue 997](https://github.com/OpenApoc/OpenApoc/issues/997))
