# Next clean-room implementation targets

Written from [openapoc-gap-matrix.md](openapoc-gap-matrix.md) with Ghidra closed. Do not open decompiler output while implementing these.

Priority is milestone + evidence confidence + local file locality.

1. **Ticks per second ([issue 997](https://github.com/OpenApoc/OpenApoc/issues/997))** — highest leverage. Training, fire rates, AOE, and movement are all wrong until subsystems agree on 36 TPS. Fix one system at a time against original observation.
2. **Agent city mission defaults ([agentmission.cpp](../../game/state/city/agentmission.cpp))** — default `TODO: Implement` hooks; `Investigate Building` is string-backed. Teleport still falls into the default `getNextDestination`. High confidence, small file, blocks alpha agent flow ([issue 263](https://github.com/OpenApoc/OpenApoc/issues/263)).
3. **Vehicle mission stubs ([vehiclemission.cpp](../../game/state/city/vehiclemission.cpp))** — same shape as agent missions; needed before ground lanes. Road-layer strings exist; lane-keep rules do not ([issue 785](https://github.com/OpenApoc/OpenApoc/issues/785)).
4. **Organisation vehicle buy/sell ([issue 1053](https://github.com/OpenApoc/OpenApoc/issues/1053))** — tables already extracted (`organisation_vehicle_park`).
5. **Battle cover / potshot modes ([issue 265](https://github.com/OpenApoc/OpenApoc/issues/265))** — specified in [ai.txt](../../tools/extractors/docs/ai.txt); TACP only names Cautious / Aggressive / kneel. Implement from that note, not from TACP listings.
6. **Enzyme / fire / cloak constants** — names are string-backed (`Entropy Enzyme`, `Personal Cloaking Field`). Replace “made up” values in [battlehazard.cpp](../../game/state/battle/battlehazard.cpp) after original observation.
7. **Victory / defeat / takeover screens** — city strings `X-COM IS DEFEATED`, `THE ALIENS ARE DEFEATED`, `ALIEN TAKEOVER`; launcher already names `smk/lose1.smk` and `smk/wingame2.smk`. OpenApoc already plays SMK.

Extractor CRC follow-on (not this experiment): teach [ufo2p.cpp](../../tools/extractors/common/ufo2p.cpp) / [tacp.cpp](../../tools/extractors/common/tacp.cpp) the `4`-build CRCs and the per-table deltas in the sibling rebase CSVs, or keep extracting from the ISO non-4 pair.
