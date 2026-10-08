# v_market: market prices, selling, and organisation raid loot, checked against UFO2P.EXE

Scope: the weekly market simulation, the Buy/Sell screen's price and stock effects, craft sale, the per-organisation
raid-loot table and how loot is generated, what a raid does to X-COM's view of the raided organisation, and the
"O2: worth is always zero" closure. Binary: `UFO2P.EXE` ISO non-4 (size 1,702,206; the canonical copy in OGR). TACP.EXE
was only string-searched (copy `v_market_2`); nothing in it is bound here.

Conventions. **OBSERVED** = decompile/listing text, bytes read from the EXE, code lines, or simulation output produced in
this session. **INFERRED** = my reading of observed facts. File offsets are physical offsets in the bound EXE. Conversion
used throughout (re-verified by prologue bytes, table at the end of section 1): code `file = Ghidra VA + 0x626A4`, file-backed
data `file = Ghidra VA + 0x616A4`. Engine cites are `LIVE:` (frozen snapshot) and `HEAD:` only where HEAD differs; for every
file cited below `economyinfo.cpp`, `organisation.cpp`, `battlemap.cpp`, `transactioncontrol.cpp`, `buyandsellscreen.cpp`
are byte-identical between LIVE and HEAD, `gamestate.cpp` differs elsewhere but `updateItemMarket` is identical (HEAD
1659-1695, LIVE 1692-1728), `battle.cpp` loot code has identical line numbers except `isCargoCarrierPresent` (HEAD 4038,
LIVE 4045).

Ghidra calls used: 15 headless runs on private copy `v_market` plus 2 on `v_market_2` (TACP), each ~10-20 s here; I went past the 8-call guideline on the
`worth` and loot-recovery sub-questions, which are still unresolved (sections 6.5, 6.6). Raw output
is in `validate/raw/` (`b1_full.txt` market/UI functions, `b3_full.txt` week counter/new game, `b4_full.txt` loot and
post-battle, `b7_full.txt` mission descriptor and save/load, `b6_range.txt`/`b9_range*.txt` reference censuses).

---

## 0. Result in ten lines

1. I decompiled `FUN_00096538` (stock, file 0xF8BDC) and `FUN_0009667c` (price, file 0xF8D20) myself. The prior claim is
   **confirmed in every detail**: the EXE selects the price rule by `week == 0`, not by manufacturer, and the absolute
   post-decay stock drives the price cut. My own Python replay reproduces the prior replay's numbers to within Monte Carlo noise.
2. Engine consequence: Psiclone, Elerium and the two AG missiles (4 `aequip` items, all manufacturer != X-COM, week 0) are
   **frozen at base price forever** in the engine and fall toward 50% in the EXE. For craft this selector difference is
   irrelevant (see section 4).
3. **Correction to the prior note (income_engine_exe_check section 3c / campaign-plan 11.1 worry): selling a craft does NOT add market
   stock in the EXE** (the sale path of `FUN_0008f7f4` has no write to the vehicle market rows), and the engine agrees
   (`buyandsellscreen.cpp:443-460` adds no stock). A craft's price therefore never falls from sales in either model. "Sell at
   most one craft a week" is not needed. Craft are the one market line that is an unbounded, un-decaying income source.
4. Sale price equals buy price (no spread) in both, no slippage inside a transaction, balance credited immediately; buying from a manufacturer is blocked when the
   *manufacturer's* view of X-COM is <= -50 (observed argument order), in both. Match.
5. Craft valuation differs: EXE scales hull value by current/max health and ignores loaded ammo; engine does neither.
6. Org loot table is at file **0x192004** (27 rows x 60 B, rows 0 and 1 all zero, 25 populated, no Civilian row). The gap
   matrix's `0x192184` is not row-aligned (it is row 6 + 24 bytes). Cult of Sirius row is byte-identical to the engine's.
7. Loot generation mechanism (`FUN_000b3f14`, file 0x1165B8) matches the engine's per-spot uniform draw. The EXE gate is
   mission type 3 AND a hostility test; the same function first forces X-COM's view of the raided owner to <= -60.
8. That forced -60 is **persistent** in the EXE (matrix is in the savegame) and **transient** in the engine. Diverges.
9. `worth` (the post-raid org-funds debit): I could not find any UFO2P path that makes it non-zero, but I also found that the descriptor
   it lives in is `.bss`, not in the savegame, and (per the launcher strings) UFO2P and TACP are separate processes, so *something* must refill the descriptor
   after a battle that neither census found. O2's closure is therefore **not safe to rely on**; left `exe_unbound`.
10. Loot recovery rules (cargo capacity, where loot goes) are TACP-side and **not bound**. The engine drops all cargo loot if
    no craft in the party has cargo space; the EXE rule is unknown.

Dollar effect (section 5): a Cult temple raid is worth $39-77k gross at base prices (spot count not re-derived). At one raid per week the EXE market takes at
most 5% of the Psiclone proceeds in the first six weeks (7% at two raids/week, 13% at three), 10-19% by week 13 and 25-34% by week 26, up to 40% at three raids/week over 26 weeks;
all of it comes from the Psiclone rule the engine skips. For manufacture-and-sell, craft and the slow high-value goods are identical in both models ($22-37k/week/workshop on the
unverified 400 man-hours/hour), while for fast cheap goods the engine overstates $/week/workshop by 1.2x to 9x depending on item and volume (section 5.3).

---

## 1. Method and controls

1. Read `income_engine_exe_check.md` and `exe_evidence_index.md` sections 3, 8, 9 for conversions and for the prior claim.
2. Decompiled (QueryFullFunction: decompile plus full listing) the functions below on a private Ghidra copy and **re-derived** each
   rule from the listing, not from the prior text.
3. Read the 4 market tables (34/49/59/87 rows x 22 B at VA 0x12C50C/0x12C7F8/0x12CC30/0x12D144 = file 0x18DBB0/0x18DE9C/0x18E2D4/0x18E7E8)
   and the manufacturing table (43 x 50 B, file 0x13FD34) straight from the EXE with Python and joined them to the engine's
   generated XML (`economy.xml`, `vehicle_types.xml`, `vehicle_equipment.xml`, `vehicle_ammo.xml`, `agent_equipment.xml`).
4. Wrote an independent replay (`validate/work/market_sim.py`) of both rule sets and ran the scenarios in section 5.

Controls.
- Address conversion control: prologue bytes at `VA + 0x626A4` are Watcom prologues for every function used (table below);
  the Ghidra `FULL LISTING` entry bytes agree with them. Data conversion control: the week counter at VA 0xE0E7C is file
  0x142520 = `01 00 00 00`, the hostile-threshold byte at VA 0x1277E4 is file 0x188E88 = 0xCE, both read directly.
- Replay control: my replay of the EXE rule gives Psiclone 8/wk -> 0.836 realised / 0.629 final, 16/wk -> 0.666 / 0.500;
  the prior independent replay gave 0.834 / 0.63 and 0.666 / 0.50. Engine rule: Toxigun 8/wk 0.778 / 0.619 vs prior 0.775 / 0.62.
- Absence control for "no craft stock write": the table-reference census (prior `ghq_tables.txt`, re-read) lists every function
  that touches the vehicle market rows: `FUN_0001592c` (init), `FUN_000964d0`/`FUN_00096614` (weekly), `FUN_0008cb9c` (row builder),
  `FUN_0008f7f4` (commit), `FUN_0008da9c` (valuation), `FUN_000ab440` (manufacturing, +0x12 only), `FUN_000967a4`, `FUN_000962cc`
  (reads), `FUN_000a3234`/`FUN_000a4054` (save/load). The same census does find the known writers for the item tables, so the
  method sees stock writes where they exist.

| Function | Ghidra VA | file offset | first bytes |
|---|---|---|---|
| FUN_00096538 stock tick | 0x96538 | 0xF8BDC | 53 51 52 56 57 55 89 c3 |
| FUN_0009667c price tick | 0x9667c | 0xF8D20 | 53 51 52 56 57 55 89 c3 |
| FUN_000964d0 / FUN_00096614 (all rows) | 0x964d0 / 0x96614 | 0xF8B74 / 0xF8CB8 | 52 31 d2 0f bf c2 6b c0 |
| FUN_0008e89c quantity handler | 0x8e89c | 0xF0F40 | 53 51 52 56 57 55 83 ec |
| FUN_0008f7f4 commit | 0x8f7f4 | 0xF1E98 | 53 51 52 56 57 55 81 ec |
| FUN_0008da9c craft value | 0x8da9c | 0xF0140 | 53 51 52 56 57 55 89 c7 |
| FUN_0008cb9c row builder | 0x8cb9c | 0xEF240 | 53 51 52 56 57 55 83 ec |
| FUN_00091db0 hostile test | 0x91db0 | 0xF4454 | 53 51 89 d1 0f bf d8 8d |
| FUN_0001592c market init | 0x1592c | 0x77FD0 | 68 18 00 00 00 e8 dc a0 |
| FUN_0005d1d8 rand(n) | 0x5d1d8 | 0xBF87C | 53 51 52 56 57 89 c3 8b |
| FUN_0004ac40 Monday dispatch | 0x4ac40 | 0xAD2E4 | 53 51 52 56 57 66 8b 1d |
| FUN_000941dc weekly (calls market) | 0x941dc | 0xF6880 | 53 51 52 56 57 55 83 ec |
| FUN_000b94c0 new game | 0xb94c0 | 0x11BB64 | 53 51 52 56 57 e8 66 bd |
| FUN_000ac348 mission launcher | 0xac348 | 0x10E9EC | 56 57 55 83 ec 14 89 c7 |
| FUN_000b3f14 loot placer | 0xb3f14 | 0x1165B8 | 53 51 52 56 57 55 89 c6 |
| FUN_000b3fd8 loot spots from .SOB | 0xb3fd8 | 0x11667C | 56 57 55 81 ec 0c 01 00 |
| FUN_000aff9c mission descriptor builder | 0xaff9c | 0x112640 | 56 57 55 83 ec 04 89 04 |
| FUN_000b32ac post-battle dispatcher | 0xb32ac | 0x115950 | 53 51 52 56 57 55 83 ec |

---

## 2. The weekly market tick

### 2.1 What runs and when (OBSERVED)

- `FUN_0004ac40` (file 0xAD2E4, the game clock) on the Monday hour rollover does `DAT_000e0e7c += 1; FUN_000941dc();
  FUN_0006edac(); FUN_000967a4();` (b3_full.txt lines 1109-1212). The block sits inside `if (DAT_000d5060 == 0)` (human city) and fires when the weekday counter
  `DAT_000d4d64` wraps to 0, so the week counter and the market tick exist only in the human city. `FUN_000941dc` (file 0xF6880) calls `FUN_000964d0` at VA 0x947c0 then `FUN_00096614` at VA 0x947c5 (callers
  census, `b2_callers.txt`): **all rows' stock update first, then all rows' price update**. The next instruction (VA 0x947ca) is `CMP word [0xE0CA0],0 ; JNZ 0x94A19`, the
  funding block's entry test, so the market ticks every Monday regardless of the funding-termination state (listing, `b3_full.txt`).
- New game (`FUN_000b94c0`, file 0x11BB64): runs `FUN_000941dc(1)` + `FUN_000962cc()` twice as warm-up, **then calls `FUN_0001592c`
  (file 0x77FD0) which re-initialises every market row** (`curPrice = basePrice`, `curStock = 0`, `lastStock = 0`, then one stock tick), and
  only afterwards stores `DAT_000e0e7c = 1` (VA 0xB95CC). The static initial value of that dword in the file is also 1 (file 0x142520 =
  `01 00 00 00`). So the init tick sees week 1, and the first Monday tick sees week 2. This matches the engine, whose game starts in
  `getWeek() == 1` and runs `updateEndOfWeek(true)` (stock only; the `week > 1` guard blocks the price) at LIVE `gamestate.cpp:714`.
  The warm-up ticks are wiped by the re-init. **Match.**
- `FUN_0005d1d8(n)` (file 0xBF87C) returns `(rand16 + (A*B)/counter) % (n+1)`, i.e. **inclusive 0..n**. The engine's
  `randBoundsInclusive` calls are the right analogues.

### 2.2 Stock rule, `FUN_00096538` (OBSERVED)

Record layout used by every function (22 B): +0 week i16, +2 base i32, +6 min i16, +8 max i16, +0xA curPrice, +0xE curStock, +0x12 lastStock.
(Decompiler shows `*(int*)(p+6)>>16` = max and `*(int*)(p+4)>>16` = min; I checked this against `extract_economy`'s struct and against the
table bytes, e.g. Toxigun row 33: week 0, base 2780, min 0, max 8.)

```
if (0 < week && week <= DAT_000e0e7c) {            // item has become available
    stock = rand(0 .. (min+max)/2 + stock) & 0xffff; // (min+max)/2 truncated
    clamp stock to [min, max];  lastStock += stock - oldStock;
}
if (stock != 0 && min == 0) {                       // sold-in / alien-tech decay
    stock -= stock/8;                               // integer, always
    r = rand(0..100);                               // 101 values
    if (r > 40) stock -= stock/5;                   // P = 60/101
    if (r > 70) stock -= stock/3;                   // P = 30/101, cumulative with the line above
}
```
(Listing 0x96538-0x96613, file 0xF8BDC. Constants `0x28`, `0x46`, `/5`, `/3`, `>>3` are immediates in the listing.)

INFERRED consequences: expected multiplier on a large stock = 0.406 x 0.875 + 0.297 x 0.70 + 0.297 x 0.467 = **0.70** per week
(engine: 0.4 x 1.0 + 0.3 x 0.8 + 0.3 x 0.66 = **0.84**). Integer truncation makes stocks 1-2 immortal, 3-7 only shrink by the
`/3` term, and stock under 8 never takes the `/8` bite. All 43 week-0 rows have `min == 0`, so they all decay; every row with `week > 0` has
`min > 0` except ammo row 2 (min = max = 0), so third-party goods never take the decay line.

### 2.3 Price rule, `FUN_0009667c` (OBSERVED)

```
if (week == 0) {                                   // listing 0x96684 CMP word [EAX],0 ; JZ 0x966f5
    e = 0;
    if (max/2   <= stock) e = rand(0..2) + 3;      // 3..5 %
    if (max     <= stock) e = rand(0..5) + 5;      // 5..10 %
    if (2*max   <= stock) e = rand(0..5) + 10;     // 10..15 %
    price = price*(100-e)/100;                     // IMUL then IDIV, truncating
} else if (min > 0) {                              // third-party
    mid = (min+max)/2;
    if (stock < mid) price = price*(100 + min(mid-stock,3))/100;
    if (stock > mid) price = price*(100 - min(stock-mid,3))/100;
}
clamp price to [base/2, 2*base]                    // both branches, 0x9677a-0x9679d
```
`stock` here is the **post-decay** stock, because the stock pass for all rows ran first and the price function never reads `lastStock`.
The week-0 branch can only lower the price, never raise it. This agrees with `income_engine_exe_check.md`.

### 2.4 Engine behaviour (OBSERVED, LIVE = HEAD)

`economyinfo.cpp:16-88`, called from `GameState::updateItemMarket` (LIVE `gamestate.cpp:1692-1728`) with
`xcom = item->manufacturer == this->player` (`:1707`).
- `xcom` branch (`:29-61`): `sold = max(0, stock - lastStock)`, then decay x0.80 (30%) or x0.66 (30%) of the *previous* stock
  (`lastStock = stock` first), then price x(95..97)% if `sold > max/2`, x(90..95)% if `sold > max`, x(85..95)% if `sold > 2*max`;
  clamp [base/2, base].
- third-party branch (`:63-86`): only `if (weekAvailable != 0)`; stock = `clamp(rand(0..round((min+max)/2)+last), min, max)`; price x(97..100)% if stock above
  average, x(100..103)% if below, only from week 2, clamp [base/2, 2*base].
- **`weekAvailable == 0 && manufacturer != player` reaches neither branch**: price and stock never change.

### 2.5 Which items fall into the gap (OBSERVED, `validate/work/classify.py` over the generated XML and the EXE tables)

Engine classification of the 184 economy entries (xcom?, week==0?, priced?): (False,False,True) 93 ; (False,True,False) 27 ; **(False,True,True) 15** ;
(True,False,True) 1 (Zorium ammo, min=max=0, inert) ; (True,True,False) 5 ; (True,True,True) 43.

The 15 priced items that are week 0 and not X-COM in the engine: 11 craft (Space Liner $500,000, Construction Vehicle, Airtrans,
Rescue Transport, Dimension Probe, Police Hovercar, Police Car, Airtaxi, Autotrans, Autotaxi, Civilian Car), plus 4 `aequip`:
**Psiclone $4,500 max 40 (Psyke)**, **Elerium $3,000 max 40 (Solmine)**, **Mini-launcher AG Missile $900 max 30 (Marsec)**,
**Heavy-launcher AG Missile $700 max 15 (Marsec)**. X-COM can manufacture the two AG missiles (manufacturing rows 26, 27: itemIndex 19, 23) and the
Dimension Probe (row 0). Since craft sales never touch stock (section 4) the 11 craft are harmless; the 4 `aequip` items are the real gap.
All 43 (True,True,True) items are X-COM-made week-0 goods and use the X-COM branch in both.

---

## 3. Selling and buying (Buy/Sell screen)

### 3.1 Price symmetry (OBSERVED)

- Row builder `FUN_0008cb9c` (file 0xEF240) copies `market.curPrice` (+0xA) into the row's price field (+6) for every market-backed row
  (vehicle types, vehicle equipment, ammo, agent equipment); for agent equipment flagged as an un-researched alien artifact
  (`DAT_00183b3b[idx] != 0`) the row price is 0 for both directions. Psiclone (84) and Elerium (85) rows have `word@8 = -1` in the
  ammo-pair table and are ordinary `aequip` rows with pack size 1.
- Quantity handler `FUN_0008e89c` (file 0xF0F40): `DAT_00127648 += row.price * ((orig/pack) - (new/pack))`. One price, both directions,
  applied immediately (live balance copy `DAT_00127648`; `FUN_0008f7f4` finally stores it: `*(DAT_0017fb4c + 8) = DAT_00127648`).
- No quantity-dependent price inside a transaction: dumping 100 units pays 100 x the price shown. The weekly tick is the only price change.
- Engine: `buyandsellscreen.cpp:466-499` credits `order * economy.currentPrice`; purchases at `economy.currentPrice`
  (`organisation.cpp:215-347`). **Match.**

### 3.2 Sale adds stock (OBSERVED)

Commit `FUN_0008f7f4` (file 0xF1E98), every item row (vehicle equipment, vehicle ammo, agent equipment, including the type-2 rows):
`market.curStock += (orig - new)` (divided by the row pack size for clip-style rows); a *purchase* path calls `FUN_00090d68`/`FUN_00090fe8`
instead when the row class byte (+0x19) is not 0/1, and plain `curStock -= ...` otherwise. Engine: `economy.currentStock += order`
for the same cases (`buyandsellscreen.cpp:470, 490, 497`). **Match**, including that selling to a hostile manufacturer is not blocked.

Buying a hostile manufacturer's goods is blocked in the EXE. OBSERVED from the listing (`b1_full.txt`, `FUN_0008e89c`): at VA 0x8EAC2-0x8EADD, after `new > orig`,
`AL = byte[row + 0x19]` (the row's manufacturer index, copied from the `aequip`/`vequip` data), `EDX = 0`, `CALL 0x91DB0`, `TEST AL,AL ; JZ` (continue), else the row is reset to `orig`
and a message (string group 0x26) is shown. `FUN_00091db0(a=manufacturer, b=0)` is `rel[manufacturer][X-COM] <= -50`: **the manufacturer's attitude toward X-COM**, not X-COM's attitude toward it.
Consequence: the raid-launch write of -60 into `rel[X-COM][owner]` (section 6.4) does **not** by itself stop X-COM buying from the raided organisation; it is the owner's own
view of X-COM that does. The engine's buy block (`organisation->isRelatedTo(player) == Hostile`, the manufacturer's view) is the same direction. **Match**, except the 1-point boundary (section 6.3).

### 3.3 Who is credited on a purchase (INFERRED, exe_unbound)

Engine `settleMarketPurchase` (`organisation.cpp:26-35`) also credits the seller organisation's `balance`. In `FUN_0008f7f4`, `FUN_00090d68`
and `FUN_00090fe8` I found no write to any organisation record other than the X-COM balance. The only seller that could matter for funding is the
Government (`balance/2` caps X-COM income) and it makes exactly one market item (Space Liner, week 0, never purchasable), so the dollar effect is nil.
Megapol (31 items), Marsec (41) are credited in the engine and not in what I decoded.

---

## 4. Craft sale

OBSERVED. `FUN_0008da9c` (file 0xF0140) valuation of an owned craft:

```
v  = curPrice(vehicleType)                          // market +0xA of the type row
v -= sum over the type's default-equipment slots of curPrice(equipment)   // vehicle_data+0x51+i, ids 0 and 0xFF skipped
v  = (health * v) / constitution                    // health = vehicle +0x12E, constitution = vehicle_data +0x2E
v += sum over installed slots (vehicle +0x18D+i) of curPrice(equipment)   // no ammo term
```
`FUN_00058280` (file 0xBA924) zeroes `+0x12E` and frees the slot. The commit's vehicle section (`FUN_0008f7f4`, lines for rows with owned-craft
class) on `new < orig` calls `FUN_0007ae60` for each assigned person and `FUN_00058280` for the craft. **There is no write to the vehicle
market stock** (and no other function in the table-reference census could do it). The *first* loop of the commit over the 34 vehicle-type market
rows (listing 0x8F8C0-0x8F934, file 0xF1F64-0xF1FD8) is the only place the commit touches a vehicle-type `curStock`: the census DATA references at
VA 0x8F8D9, 0x8F903 and 0x8F92E are `ptr = 0x12C50C + 0xE` (the row's `curStock`), pushed as the first argument of `CALL 0x90FE8` **only when
`row.+0x12 < row.+0xE`** (`JGE 0x8F921` skips the call when `new >= orig` is false, i.e. on a decrease). `FUN_00090fe8` (file 0xF368C) then does `*param_1 -= n`:
purchase only, stock goes down, never up.

Engine: `transactioncontrol.cpp:552-585` value = type price + installed equipment + `ammo x ammoPrice` - default equipment, **no health term**.
`buyandsellscreen.cpp:443-460` sells a craft with `vehicle->die(); balance += c->price;` and adds no stock.

INFERRED: health/constitution identification of `+0x12E` / `vehicle_data+0x2E` rests on `FUN_00058280` zeroing `+0x12E` when a craft is destroyed
and on `U1b-gate-consumer.md` placing constitution at `vehicle_data+0x2E`; I did not decode the damage code.

Effects: (a) no price slide from craft sales in either model, so craft revenue is constant per unit; (b) damaged craft are overpaid by the engine
and loaded ammo is over-valued; manufactured craft are new and sell at full value in both.

---

## 5. Quantification

Simulation notes. `validate/work/market_sim.py` (EXE rules transcribed from the listings above; engine rules transcribed from `economyinfo.cpp`).
The player sells n units at the start of each week (price p_w), the units are added to `curStock`, then the Monday tick runs. 2,000-3,000 Monte Carlo
runs per row. Output files: `run_sims.out`, `raid_sim.out`, `mfg_sim.out`.

### 5.1 Selling loot and goods: realised price / base over a 26-week campaign, EXE vs engine

| Item (base, max) | n per week | EXE realised | EXE final price | Engine realised | Engine final |
|---|---|---|---|---|---|
| Psiclone (4500, 40) engine: frozen | 4 | 0.995 | 0.982 | 1.000 | 1.000 |
| | 8 | 0.836 | 0.629 | 1.000 | 1.000 |
| | 16 | 0.666 | 0.500 | 1.000 | 1.000 |
| | 32 | 0.600 | 0.500 | 1.000 | 1.000 |
| Elerium (3000, 40) engine: frozen | 8 / 16 / 32 | 0.836 / 0.666 / 0.600 | 0.629 / 0.5 / 0.5 | 1.0 | 1.0 |
| Mini AG missile (900, 30) engine: frozen | 8 / 16 / 32 | 0.744 / 0.633 / 0.585 | 0.514 / 0.5 / 0.5 | 1.0 | 1.0 |
| Toxigun (2780, 8) X-COM | 2 | 0.667 | 0.500 | 1.000 | 1.000 |
| | 4 | 0.614 | 0.500 | 1.000 | 1.000 |
| | 8 | 0.583 | 0.500 | 0.778 | 0.619 |
| | 22 | 0.556 | 0.500 | 0.590 | 0.500 |
| Stasis Bomb (2650, 20) X-COM | 4 | 0.802 | 0.571 | 1.000 | 1.000 |
| | 8 | 0.654 | 0.500 | 1.000 | 1.000 |
| | 16 | 0.598 | 0.500 | 0.785 | 0.624 |
| Disruptor Armor torso (3480, 20) | 4 / 8 / 18 | 0.803 / 0.654 / 0.593 | 0.571 / 0.5 / 0.5 | 1.0 / 1.0 / 0.779 | 1.0 / 1.0 / 0.619 |
| Personal Teleporter (18200, 40) | 1-3 | 1.000 | 1.000 | 1.000 | 1.000 |
| | 8 | 0.837 | 0.630 | 1.000 | 1.000 |

Single dump, then nothing (the "sell in lumps" idea): the dump itself is paid at the price shown; what differs is the *permanent* price afterwards.

| Item | N dumped in week 1 | EXE price after 26 weeks | Engine price after 26 weeks |
|---|---|---|---|
| Psiclone | 40 / 100 / 200 | 0.939 / 0.757 / 0.607 | 1.000 |
| Toxigun | 16 / 40 / 100 / 200 | 0.698 / 0.546 / 0.503 / 0.500 | 0.925 / 0.899 / 0.899 / 0.899 |
| Personal Teleporter | 40 / 100 / 200 | 0.939 / 0.757 / 0.607 | 0.960 / 0.899 / 0.899 |

Reading (INFERRED from the rules): the EXE charges for *standing stock*, so the equilibrium stock for a steady n per week is about 2.3 n
(0.70 survival per week) and the 3-5% weekly cut starts at about 8-9 units/week for a max-40 item, 3-4 for max 8, and never recovers.
The engine charges for *this week's sold delta* and caps a one-off dump at a single 5-15% cut, so a lump is nearly free there and
costly in the EXE. The prior plan's advice "dump in lumps, not on a timer" is therefore **wrong against the EXE for every week-0 item (X-COM goods and Psiclone/Elerium alike)**.
The concrete EXE rule the decode supports (strategy_rule_affected for F-M2/F-M3): *a week-0 price is cut on a Monday tick if and only if the post-decay standing stock is
at least max/2* (tiers at max/2, max, 2 max: 3-5%, 5-10%, 10-15%), the cut is permanent, and stock of 1-2 never decays. With a survival factor of ~0.70/week the steady
full-price selling rate is about 0.2 x max per week (Psiclone/Elerium max 40: about 8/week; Toxigun max 8: about 1.7/week, and integer stickiness makes it worse: Toxigun at 2/week already
slides to the floor in the replay). For items with max <= 8 the market saturates for good after a handful of units. Pacing sales *below* that rate, or spreading
them over many distinct item types, is what the EXE rewards; the engine rewards neither.

### 5.2 $ per raid (Cult of Sirius loot)

Per-spot expected value at base price (EXE table, item ids -> `aequip` base): Cult A $1,934, B $3,100, C $4,500 (Psiclone probability
2/5, 3/5, 5/5). Temple of Sirius has 23 loot spots (A7 B6 C10) per the prior's read of the `.SOB` files; **I did not re-derive that count or the
number of spots present in one generated battle** (the sector placement in `battlemap.cpp:548-880` is randomised; the prior's 14.8 is a crude
estimate). Gross at base price: 23 spots $77,138 (16.4 Psiclone), 14.8 spots $49,637 (10.6 Psiclone), 12 spots $39,536 (8.4 Psiclone).

Psiclone proceeds when selling the whole weekly haul, EXE vs engine (engine = 100% of base; this is the "rule gap" only):

| Raids/week (Psiclone/week) | 4 wk | 6 wk | 13 wk | 26 wk |
|---|---|---|---|---|
| 0.5 (5.3) | 0.0% | 0.0% | 0.8% | 3.8% |
| 1 (10.6) | 0.4% | 1.9% | 10.0% | 24.7% |
| 1, full 23 spots (16.4) | 2.0% | 5.2% | 18.7% | 33.7% |
| 2 (21.2) | 2.6% | 6.8% | 22.7% | 36.3% |
| 3 (31.8) | 6.7% | 12.7% | 30.1% | 40.0% |

(EXE shortfall relative to the engine's frozen price; absolute: 1 raid/week of 10.6 Psiclone, 13 weeks: engine $616k, EXE $555k.)
For the benchmark horizon (loses by day 12-45, i.e. 2-6 weeks) the gap is **at most 7% even at 2 raids/week**; it only becomes strategic if the
strategy is meant to sustain months of raiding.

Other raid facts that change $/raid: the Cult drops 16.4 Psiclone x 3 score each (equipment captured score); guards `avg_guards 10` give 7-12 kills; every
corporate row (Government, Superdynamics, General Metro, Cyberweb, Transtellar, Sensovision, Lifetree, Nutrivend, Evonet, Sanctuary Clinic, Nanotech, Energen, Synthemesh, Gravball League, Extropians, Technocrats: 16 organisations) has the same table (A: Lawpistol, AP grenade, Stun grenade, Prox mine, Elerium; B: M4000, HE, Auto
cannon, Elerium x2; C: Elerium x5), i.e. $744 / $1,760 / $3,000 per spot; Solmine drops Elerium in all 15 slots ($3,000 per spot);
Psyke/Diablo/Osiron $2,680-4,500 per spot. Raiding a *friendly* org forces X-COM's view of it to <= -60 persistently in the EXE (section 7).

### 5.3 $ per manufactured item (one Workshop, 400 man-hours/hour = 67,200/week; engineer wages $4,000/week; 26 weeks)

The 400 man-hours/hour figure is taken from the prior note (5 engineers x skill 80), not re-derived here. Net $/week/workshop = (revenue - unit cost x units)/26 - 4,000.
**Conditional numbers.** Every row marked "large lab" in the EXE manufacturing table (`labSize = 1`: all five craft, the disruptor beams/bombs/shields, Teleporter, Cloaking, Dimension Shifter) needs an
Advanced Workshop (string file 0x1521AD), which the prior note gives 10 slots, so for those rows the $/week/workshop column is roughly 2x on the same assumption (unverified here); the small-lab goods
(Toxigun, armor, Personal Teleporter, AG missiles) fit a plain Workshop. Treat the per-unit and per-man-hour figures as the robust results: margin at base price per man-hour is Stasis Bomb 0.725,
Bio-Transport 0.629, Toxigun 0.527, Annihilator 0.500, Personal Teleporter 0.488, Retaliator 0.467, Explorer 0.400, Teleporter 0.371, Dimension Probe 0.160 (all from the EXE table at file 0x13FD34 joined to the market base price).

| Item | hrs | cost | base | units/wk | EXE $/unit | Engine $/unit | EXE net $/wk | Engine net $/wk |
|---|---|---|---|---|---|---|---|---|
| Bio-Transport (craft) | 35,000 | 12,000 | 34,000 | 1.88 | 34,000 | 34,000 | 37,462 | 37,462 |
| Annihilator (craft) | 100,000 | 50,000 | 100,000 | 0.65 | 100,000 | 100,000 | 28,692 | 28,692 |
| Retaliator (craft) | 75,000 | 35,000 | 70,000 | 0.88 | 70,000 | 70,000 | 26,962 | 26,962 |
| Explorer (craft) | 55,000 | 22,000 | 44,000 | 1.19 | 44,000 | 44,000 | 22,231 | 22,231 |
| Personal Teleporter | 25,000 | 6,000 | 18,200 | 2.65 | 18,200 | 18,200 | 28,377 | 28,377 |
| Teleporter (vequip) | 45,000 | 11,000 | 27,700 | 1.46 | 27,677 | 27,700 | 20,374 | 20,408 |
| Heavy Disruptor Beam | 35,000 | 8,000 | 20,500 | 1.88 | 17,604 | 20,500 | 14,100 | 19,558 |
| Medium Disruptor Beam | 20,000 | 4,000 | 10,500 | 3.35 | 6,908 | 10,500 | 5,730 | 17,750 |
| Disruptor Multi-Bomb launcher | 10,000 | 4,000 | 9,260 | 6.69 | 5,861 | 9,260 | 8,454 | 31,202 |
| Disruptor Multi-Bomb | 5,000 | 1,800 | 4,230 | 13.4 | 2,740 | 4,230 | 8,613 | 28,618 |
| Disruptor Armor (torso) | 3,800 | 1,500 | 3,480 | 17.7 | 2,067 | 2,715 | 6,006 | 17,449 |
| Stasis Field Bomb | 2,000 | 1,200 | 2,650 | 33.6 | 1,505 | 1,688 | 6,230 | 12,385 |
| Toxigun | 3,000 | 1,200 | 2,780 | 22.4 | 1,546 | 1,640 | 3,742 | 5,859 |
| Heavy Launcher AG Missile (engine: frozen) | 1,200 | 280 | 700 | 56.0 | 389 | 700 | 2,129 | 19,520 |
| Mini Launcher AG Missile (engine: frozen) | 1,600 | 360 | 900 | 42.0 | 514 | 900 | 2,485 | 18,680 |

Reading: **craft and the slow high-value goods (Personal Teleporter, Teleporter, Bio-Transport, Annihilator) are insensitive to the market model**
($22-37k/week/workshop, price never decays at <3 units/week, craft never decay at all). Fast cheap goods are where the engine
overstates (up to 9x for the AG missiles, 3.5x for Multi-Bomb, 2-3x for Disruptor armor). The EXE caps a base at 6 small or 4 large
labs/workshops (string at file 0x152252, see F-M12); the engine has no such cap, so the engine also overstates by allowing unbounded parallel workshops
(not quantified here).

---

## 6. Organisation raid loot

### 6.1 Table location (OBSERVED)

`organisation_raid_loot_data` = 27 rows x 60 B (15 u32 = 3 priorities A/B/C x 5 slots) at file **0x192004** (extractor constant
`ORGANISATION_RAID_LOOT_DATA_OFFSET_START 1646596`, `..END 1648216`; `FUN_000b3f14` reads it at Ghidra VA 0x130960 = file 0x192004).
Rows 0 (X-COM) and 1 (Alien) are all zero, rows 2-26 (25 organisations) are populated, row 27 would be Civilian but the bytes at
0x192658 are ASCII, so there is none (extractor special-cases Civilian). The gap matrix's `0x192184` is not row-aligned
(0x192184 - 0x192004 = 0x180 = 6 rows + 24 bytes): **a citation error, not a second table**. The u32 at file 0x192000 (VA 0x13095C) is the loot-spot
counter and is 0 on disk (filled at map load by `FUN_000b3fd8`).

Row 4 (Cult of Sirius): A = [6 Megapol Lawpistol, 1 Megapol AP Grenade, 84 Psiclone, 4 Marsec Prox Mine, 84], B = [84, 5 Marsec HE, 12 Megapol Auto Cannon, 84, 84],
C = [84 x5]. Byte-identical to `organisations.xml` ORG_CULT_OF_SIRIUS. No row anywhere contains item 32.

### 6.2 Generation (OBSERVED, `FUN_000b3f14`, file 0x1165B8)

For each 8-byte spot record (`DAT_001c3650`, count `DAT_0013095c`; byte +6 is the priority 1..3): `i = rand(4)` (inclusive 0..4),
`item = table[owner*15 + (prio-1)*5 + i]`; if `item != 0 && item != 0x20 && DAT_002440c4 < 500` allocate an item record via `FUN_000ad7ac`
(file 0x10FE50), copy x/y/z from the spot, `type += 100`. Spots come from the sector `.SOB` files (`FUN_000b3fd8`, file 0x11667C:
up to 20 entries per sector, kept only if inside the map bounds `DAT_00244014/18/1C`, cap 200 spots).

Engine: `battlemap.cpp:1161-1173` `pickRandom` over the 5 slots, null slots place nothing. **Match** in mechanism. Engine omits the item-32 skip
and the 500/200 caps (no data hits them).

### 6.3 Gate (OBSERVED)

`FUN_000ac348` (file 0x10E9EC) calls `FUN_000b3f14` at VA 0xAC758 only if `param_2 == 3` (and `DAT_000d5060 == 0`, human city) and
`FUN_00091db0(0, owner) || FUN_00091db0(owner, 0)`. `FUN_00091db0` (file 0xF4454): `rel[a][b] = byte[0x16EC28 + a*0x1C + b]`, returns
`rel <= byte[0x1277E4]` where that byte is file 0x188E88 = 0xCE = -50. Engine has no gate (every `RaidHumans` battle places loot) and uses
`x < -50` for Hostile (`organisation.cpp:1089-1103`): a **1-point divergence** (Cult at the starting -50 is hostile in the EXE, "Unfriendly" in
the engine) with no effect on loot, but it matters for who will sell to / transfer for X-COM.

### 6.4 What the launch does to relations (OBSERVED)

`FUN_000ac348` listing, type 3 branch (file 0x10EC6A-0x10EC7F): `owner = byte[building + 0xC8]`; if `byte[0x16EC28 + owner] > -60` then
`byte[0x16EC28 + owner] := 0xC4 (-60)` and `byte[0x16F078 + owner] := 0xC4`. Row 0 of the 28x28 matrix is X-COM's view of each organisation
(`FUN_00091db0(0, owner)` indexes the same cell), so the launch sets **X-COM's attitude to the raided owner to <= -60**. Type 4 branch
(compare at file 0x10EC0E, writes 0x10EC15/0x10EC1B) does the same to the *other* direction, `rel[org][0]`, for an org passed in a register. Both matrices (0x16EC28 and 0x16F078, plus
0x16F388) are among the blocks written by the save routine `FUN_000a3234`, so the value is persistent state (INFERRED: 0x16F078 is the long-term copy, which
stops the value relaxing back).

Engine: `battlemap.cpp:1542-1543` sets `player->current_relations[target] = -100` (X-COM's row only), which `adjustRelationTo` re-mirrors from each
organisation's own view at the next relation change involving X-COM (`organisation.cpp:1056-1076`). The only persistent launch effect is the optional mod
`RaidHostileAction` (-200, `buildingscreen.cpp:248`, default off). **Diverges: EXE persistent <= -60 on X-COM's row; engine transient.**

`FUN_00091db0` has 34 call sites (`b14_hostile_callers.txt`): the loot gate (VA 0xAC73E/0xAC74B), the buy block (0x8EAD6), the guard-count function (`FUN_000aec70`, 0xAECA5/0xAECB8),
the projectile/relation code (`FUN_00054a28`, four sites), vehicle obstruction (`FUN_00041644`), `FUN_0004e48c`, `FUN_00058b88`, `FUN_0007a730`, `FUN_00097abc/b14` and others. I decoded the arguments of
only the first two. INFERRED: because most of the remaining sites are in vehicle-combat and hit code, the persistent `rel[X-COM][owner] <= -60` is not cosmetic; it makes X-COM's side treat the raided organisation's
craft as hostile in whatever those tests do. Impact is therefore medium, not low, with the specific effect unproven.

Caveat (not bound): the mapping between the engine's `RaidHumans` / `RaidAliens` (Raid / Exterminate buttons) and the EXE's launcher types is not established.
`FUN_000ac348`'s second parameter is the vehicle-mission type; `FUN_000ac08c` (file 0x10E730) turns a vehicle order of 3 into type 4; I did not decode the callers (the
O2 note placed all seven call sites in the vehicle-mission family rooted at `FUN_0003a910`). So "a raid on a building" = type 3 is a reading, not a proven mapping of the UI button.

### 6.5 Post-battle `worth` debit: the O2 closure (OBSERVED + census, unsettled)

`FUN_000b32ac` (file 0x115950) for descriptor kind 1 (and 4 outside the alien dimension): `owner.balance += worth * -50` and a tiered relation delta through
`FUN_0005faf0`: `worth <= 50 -> 0`; `51..250 -> -((worth-50)/20)`; `> 250 -> max(-50, -10 - (worth-250)/50)` (`owner` is read as `byte[DAT_0018276c + 200]`,
`worth = DAT_00244090`).

What I found (OBSERVED):
- The descriptor block (VA 0x244020..0x2440B0) is in `.bss`: VA 0x244020 + 0x616A4 = file 0x2A56C4, beyond the file size 1,702,206.
- Writers to `worth` (VA 0x244090): one in `FUN_000aff9c` (VA 0xB027D, `MOV [0x244090],EBP` with `EBP = 0` two instructions earlier); O2 additionally found a
  second zero-writer at 0xBC3D1 in an unlinked routine. Reference census over 0x244020-0x2440D8: all other references are reads in `FUN_000b32ac`.
- The savegame routines `FUN_000a3234`/`FUN_000a4054` make about 129 fwrite/fread calls; **none lies in 0x238000-0x249000** (listing grep, `b7_full.txt`): neither the
  descriptor nor the 60-slot participant array at 0x241D98 round-trips through `scenario/savegame.dat`.
- The file readers I decompiled (`FUN_000a555c` file-copy loops, `FUN_000b1b78` whole-file read, `FUN_000b3fd8` `.SOB` spots, `FUN_000b4bbc`, `FUN_000b61bc`) target
  0x244D06 and 0x2483D0-0x248B24, not the descriptor.
- TACP search: no `CANUNIT.DAT`/`CANPLAY.DAT`/`CANUSE.DAT` string (UFO2P has them, file 0x131DC0-0x131DD8); strings `ecovered`, `cargo`, `Cargo`, `storage`, `Stores`,
  `no room`, `capacity` have no hit in either TACP block.

Reading (INFERRED): I found no mechanism that makes `worth` non-zero, so O2's conclusion survives. Two reasons it is not closed: (1) the *victory* flag
`DAT_00244084` beside it has the identical single-zero-writer shape yet gates the alien-dimension and craft-recovery bodies of the same function, which must work in the
real game; so a static zero census does not prove a field dead (an out-of-band source, e.g. TACP-modified state reloaded through a path I did not find, is not excluded);
(2) `FUN_0005faf0` arguments were not decoded. Treat "raids debit the target's funds" as **unproven in both directions**; no strategy rule should depend on it.

**Process-boundary problem (new, and the top lead).** `docs/original-game/binaries/xcomapoc.md` shows `XCOMAPOC.EXE` launches `ufoexe/ufo2p.exe` and `tacexe/tacp.exe` as separate
child processes (flags `TACTICAL`, `SAVEGAME`). If UFO2P exits to run the battle and is started again afterwards, its `.bss` descriptor (kind, owner, victory, worth) would be all zero on re-entry, yet
`FUN_000b1dc0` (file 0x114464) immediately branches on `DAT_00244024` after calling the loader `FUN_000a4054`. Since the savegame blocks do not contain the descriptor and the helpers I decompiled
(`FUN_000b4b50`, `FUN_000a4f4c`, `FUN_000a4fc8`, `FUN_000a5028`, `FUN_000b5adc`; `b15_full.txt`) touch no address in 0x238000-0x249000 except 0x2483xx-0x248bxx, **some writer that both my census and O2's census missed must
repopulate the descriptor** (a register-based `fread` of a block whose base I did not match, or the process model is not what the launcher strings suggest). That reopens the question of whether `DAT_00244084`/`DAT_00244090` can be non-zero. I kept
the verdict at `exe_unbound` and did not chase it further (past the call budget).

### 6.6 Loot recovery (not bound)

Engine: loot items left on the map become `cargoLoot`; unless a craft in the party has `getMaxCargo() > 0` (or the raid is a base defence with Stores) all of it goes to
`leftoverCargoLoot` and is lost; the deposit-at-building branch is guarded by `leftoverCargoLoot.empty()` so its loop never runs (LIVE `battle.cpp:3259-3270, 3296-3326`;
`isCargoCarrierPresent` at LIVE 4045-4052 / HEAD 4038). `EnforceCargoLimits` default is false (`options.cpp:445-446`), so capacity is not checked, only
presence. Captured items add `type->score` (Psiclone 3, Elerium 3) to the battle score (`battle.cpp:2990`). EXE: the recovery routine lives in TACP.EXE and
the UFO2P post-battle code (`FUN_000b21a8`) reads a participant array, not an item list; I could not bind it.

---

## 7. Findings and engine-fix sketches

Verdict words follow the harness schema (`match`, `diverges`, `engine_missing`, `engine_bug`, `exe_unbound`).

| ID | Mechanic | Verdict | Strategy impact | blocked_on_re |
|---|---|---|---|---|
| F-M1 | Market rule selector (`week==0` vs `manufacturer==X-COM`) | diverges | high | no |
| F-M2 | Week-0 price tiering (post-decay standing stock, `>=`, 3-5/5-10/10-15%) vs weekly sold delta | diverges | high | no |
| F-M3 | Stock decay law (integer /8, /5, /3 cumulative vs x0.8/x0.66) | diverges | medium | no |
| F-M4 | Third-party (week>0) price and stock rule, game-start init | diverges | low | no |
| F-M5 | Sale price = buy price, immediate credit, sale adds stock, buy block direction | match | low | no |
| F-M6 | Craft sale adds no market stock (corrects the prior note) | match | high | no |
| F-M7 | Craft valuation (health scaling, no ammo term) | diverges | medium | no (health field inferred) |
| F-M8 | Seller credit on purchase | exe_unbound | low | yes (FUN_00090d68 not fully decoded) |
| F-M9 | Raid loot table location and content | match | low | no |
| F-M10 | Loot generation and hostility gate | match | low | no |
| F-M11 | Raid launch forces X-COM's view of owner to <= -60 (persistent) | diverges | medium | partly (type 3 vs UI button) |
| F-M12 | Per-base lab/workshop cap (6 small / 4 large) | engine_missing | medium | partly (EXE enforcement function unbound) |
| F-M13 | `worth`: post-raid owner debit and tiered relation hit | exe_unbound | medium | yes |
| F-M14 | Loot recovery and cargo capacity | exe_unbound | medium | yes (TACP) |
| F-M15 | Hostile threshold `<= -50` vs `< -50` | diverges | low | no |

Fix sketches (engine). All numeric constants below are observed in the listings (section 2.2-2.3).

- **F-M1/F-M2/F-M3** `economyinfo.cpp::update`: stop passing `xcom`. Select the branch by `weekAvailable == 0` (EXE `FUN_0009667c` test on the record's first word), and run the EXE order for the whole table: pass 1
  stock for every row, pass 2 price for every row. Week-0 stock pass: no roll; if `stock != 0 && minStock == 0`: `stock -= stock/8; r = rand(0..100); if (r > 40) stock -= stock/5; if (r > 70) stock -= stock/3;` (integer
  division). Week-0 price pass: `e = 0; if (max/2 <= stock) e = rand(0..2)+3; if (max <= stock) e = rand(0..5)+5; if (2*max <= stock) e = rand(0..5)+10; price = price*(100-e)/100;` clamp `[base/2, 2*base]`. Drop `lastStock`
  from the price decision (keep it only for list visibility). Items with `weekAvailable != 0` keep the third-party path. Delete the `item->manufacturer == player` selector in `GameState::updateItemMarket`.
- **F-M4** third-party branch: replace the random 97-100/100-103% with `price = price*(100 +/- min(|mid-stock|,3))/100` when `minStock > 0`, apply from the first Monday tick (the init tick is stock-only already), truncate `(min+max)/2` instead of rounding.
- **F-M6** none (already matches). Keep craft sales out of `economy.currentStock`; add a regression test so a future "tidy-up" does not add stock for `Type::Vehicle`.
- **F-M7** `transactioncontrol.cpp:552-585`: price = `(typePrice - sum(defaultEquipPrice)) * health / maxHealth + sum(installedEquipPrice)`, no ammo term (EXE `FUN_0008da9c`).
- **F-M8** none until the buyer path is decoded; if the EXE truly credits nobody, remove the seller credit in `settleMarketPurchase` for organisations whose balance gates funding (Government).
- **F-M11** `battlemap.cpp:1542` plus `Organisation::adjustRelationTo`: the engine's X-COM row is a *mirror* of every organisation's view (`organisation.cpp:1066-1076` rewrites it on every relation change involving X-COM), so setting
  `player->current_relations[target]` at launch is overwritten at the next adjustment. A faithful fix needs a **directed X-COM row that the mirror no longer overwrites** (or a separate `xcomView[target]` cell consulted by the
  hostility tests the EXE routes through `rel[0][x]`), and the launch must `min(cell, -60)` it persistently (and in the long-term copy). Do not apply the write without removing the mirror, or it is a no-op.
- **F-M12** `Base::canBuildFacility` (LIVE `base.cpp:267-306`, no count check today): refuse a 7th small or 5th large Lab/Workshop with the EXE text at file 0x152252.
- **F-M15** `Organisation::isRelatedTo`: `x <= -50` for Hostile (EXE `FUN_00091db0`).
- **F-M13/F-M14** none; do not wire either until the leads in section 8 are resolved.

## 8. Leads for a later pass

0. **Top lead:** find who repopulates the mission descriptor (VA 0x244020-0x2440B0) after the battle given the separate-process model; start with every `CALL 0xBFA7A`/`0xBF24C` site whose destination register is not a literal, in `FUN_000a4054` (the loader), `FUN_000b1b78`, and any reader of `CANPLAY/CANUSE/CANUNIT` (UFO2P file 0x131DC0-0x131DD8).
1. TACP side of loot recovery and of `worth`: find the TACP end-of-mission routine that writes the savegame/CAN*.DAT files; look for a store to a result record that UFO2P
   later loads into 0x241D98 (the 60-slot participant array) and whether any value flows into 0x244084/0x244090. Start from `scenario/savegame.dat` (UFO2P file 0x134888)
   in TACP (`savegame.dat` hit at TACP VA 0xE30E5).
2. Decode `FUN_0005faf0`'s register arguments at the two calls inside `FUN_000b32ac` (listing 0xB3xxx) to settle whether the tier delta is applied to the owner->X-COM or X-COM->owner cell.
3. Callers of `FUN_000ac08c` (file 0x10E730) to map UI Raid / Exterminate to launcher types 3 and 4.
4. Count loot spots in a generated Temple battle (or from a real raid save): the dollar scale of section 5.2 is proportional to it.
5. Decode `FUN_00090d68` fully (what does the 'class != 0/1' purchase path credit, and does the seller's balance move).

## 9. Artifacts

`validate/raw/*.txt` (Ghidra output), `validate/work/market_sim.py`, `run_sims.py`, `raid_sim.py`, `mfg_sim.py` and their `.out` files,
`classify.py`, `exe_market_tables.json`, `engine_econ_join.json`, `mfr_join.json`, `aequip_names.json`.
