"""Player-known equipment policy; every statistic comes from gs equipment_catalog.

Roles and supply margins are runner policy, not claims about original-game constants.
docs/campaign-plan.md sections 12.2 and 10.3 supply the combat doctrine.
"""
from __future__ import annotations

from collections import Counter
from dataclasses import dataclass, field
import math

ARMOR_SLOTS = ("Body", "Legs", "Helmet", "LeftArm", "RightArm")
HANDS = {"RightHand", "LeftHand"}
RESERVE = 40000


def rows(detail: str) -> list[dict]:
    return [dict(id=bits[0], **dict(v.split("=", 1) for v in bits[1:] if "=" in v))
            for part in detail.split("|") if part and part != "-"
            for bits in [part.split(":")]]


def pairs(value: str) -> dict[str, int]:
    return {key: int(count) for part in value.split("+") if "~" in part
            for key, count in [part.split("~", 1)]}


def catalog_from_reply(reply: dict) -> dict[str, dict]:
    catalog = {}
    for row in rows(reply.get("detail", "-")):
        for key in ("armor", "weight", "flight", "damage", "range", "accuracy", "fire_ticks",
                    "capacity", "recharge", "burst", "explosive", "research", "usable",
                    "price", "market", "store_space"):
            row[key] = int(row.get(key, "0"))
        for key in ("stores", "incoming", "modifiers"):
            row[key] = pairs(row.get(key, "-"))
        row["ammo"] = [a for a in row.get("ammo", "-").split("+") if a != "-"]
        if row["research"] and row["usable"]:
            catalog[row["id"]] = row
    return catalog


def roster_from_reply(reply: dict) -> list[dict]:
    result = rows(reply.get("detail", "-"))
    for row in result:
        row["kit"] = [dict(id=p[0], slot=p[1], payload=p[2], rounds=int(p[3]), armor=int(p[4]))
                      for value in row.get("kit", "-").split("+") if value != "-"
                      for p in [value.split("~")] if len(p) == 5]
        for stat in ("speed", "strength", "accuracy"):
            row[stat] = int(row.get(stat, "0"))
    return result


def assign_roles(roster: list[dict], mission: str) -> dict[str, str]:
    """One scout/support/heavy per six soldiers; remaining soldiers are assault.

    Scouts use speed, heavies strength, support accuracy. Small squads still keep a primary
    shooter. Base defenders trade the heavy's explosives for another assault rifle.
    """
    roles = {r["id"]: "assault" for r in roster}
    remaining = list(roster)
    quota = max(1, len(roster) // 6)
    for role, stat, minimum in (("scout", "speed", 2), ("support", "accuracy", 3),
                                ("heavy", "strength", 4)):
        if len(roster) < minimum or (role == "heavy" and mission == "base_defence"):
            continue
        for row in sorted(remaining, key=lambda r: (-r[stat], r["id"]))[:quota]:
            roles[row["id"]] = role
            remaining.remove(row)
    return roles


def held_units(kit: list[dict], catalog: dict[str, dict]) -> Counter:
    units = Counter()
    for e in kit:
        item = catalog.get(e["id"])
        if item:
            units[e["id"]] += e["rounds"] if item["kind"] == "Ammo" else 1
        if e["payload"] in catalog:
            units[e["payload"]] += e["rounds"]
    return units


def weapon_rank(gun: dict, payload: dict, role: str, mission: str,
                enemies: tuple[str, ...] = ()) -> tuple:
    """Hit-weighted damage per firing tick; only known enemy modifiers affect this estimate.

    Accuracy is a ranking proxy, not an asserted hit probability. Aimed/snap fire is retained
    by the battle policy (campaign-plan 12.2). Structural damage uses terrain modifiers.
    """
    factors = [payload["modifiers"][e] for e in enemies if e in payload["modifiers"]]
    damage = payload["damage"] * (sum(factors) / len(factors) / 100 if factors else 1)
    efficiency = damage * max(1, payload["accuracy"]) / max(1, payload["fire_ticks"])
    weight = max(1, gun["weight"] + (payload["weight"] if payload is not gun else 0))
    toxin = payload["damage_type"] in ("DAMAGETYPE_TOXIN_B", "DAMAGETYPE_TOXIN_C")
    doctrine = int(mission == "alien_dimension" and role != "heavy" and toxin)
    if role == "heavy":
        terrain = [v for k, v in payload["modifiers"].items() if "TERRAIN" in k]
        structural = payload["damage"] * min(terrain, default=100) / 100
        devastator = int(mission == "alien_dimension" and
                         gun["id"] == "AEQUIPMENTTYPE_DEVASTATOR_CANNON")
        return (devastator, structural, efficiency, payload["range"], -weight)
    # Avoid destructive splash as the primary for scouts, captures and defenders (12.2).
    safe = int(not payload["explosive"])
    if role == "scout":
        return (doctrine, safe, payload["accuracy"] / weight, efficiency, payload["range"])
    if role == "support":
        return (doctrine, safe, payload["accuracy"] * payload["range"], efficiency, -weight)
    # Defenders/raiders fight in corridors (12.2); recovery retains a range preference.
    reach = min(payload["range"], 20) if mission in ("base_defence", "alien_building") else payload["range"]
    return (doctrine, safe, efficiency, payload["accuracy"], reach, -weight)


@dataclass
class Loadout:
    agent: str
    role: str
    mission: str
    base: str
    armor: dict[str, str] = field(default_factory=dict)
    weapon: str = ""
    ammo: str = ""
    clips: int = 0
    extras: Counter = field(default_factory=Counter)
    gaps: list[str] = field(default_factory=list)

    def demand(self, catalog: dict) -> Counter:
        needed = Counter(self.armor.values()) + self.extras
        if self.weapon:
            needed[self.weapon] += 1
        if self.ammo:
            needed[self.ammo] += catalog[self.ammo]["capacity"] * self.clips
        return needed


def plan_loadouts(catalog: dict[str, dict], roster: list[dict], mission: str,
                  difficulty: int = 0, enemies: tuple[str, ...] = (),
                  roles: dict[str, str] | None = None) -> list[Loadout]:
    # Availability includes our own kit, stores and visible market. Inbound purchases prevent
    # repeated ordering. Availability is local to a base; another base's kit cannot be issued.
    roles = roles or assign_roles(roster, mission)
    held = Counter()
    for row in roster:
        base = row["base"] if row["base"] != "-" else row.get("home", "-")
        for item, count in held_units(row["kit"], catalog).items():
            held[base, item] += count
    plans = []
    for row in roster:
        base = row["base"] if row["base"] != "-" else row.get("home", "-")
        role = roles[row["id"]]
        plan = Loadout(row["id"], role, mission, base)
        def available(item):
            return (held[base, item["id"]] + item["stores"].get(base, 0) +
                    item["incoming"].get(base, 0) + item["market"] *
                    (item["capacity"] if item["kind"] == "Ammo" else 1)) > 0
        choices = [item for item in catalog.values() if available(item)]
        for slot in ARMOR_SLOTS:
            armor = [e for e in choices if e["kind"] == "Armor" and e["slot"] == slot]
            if armor:
                # Marsec's flight is a property of the BODY piece, not a full-set bonus.
                # Vertical mobility matters (12.2, 10.3); don't replace protective limbs.
                flight = role == "scout" or (mission == "alien_dimension" and role == "heavy")
                rank = lambda e: ((int(e["flight"]) if flight else 0),
                                  -e["weight"] if role == "scout" else e["armor"],
                                  e["armor"], -e["weight"])
                plan.armor[slot] = max(armor, key=rank)["id"]
            else:
                plan.gaps.append(f"no researched {slot} armor available")
        weapons = []
        for gun in choices:
            if gun["kind"] != "Weapon":
                continue
            payloads = [catalog[a] for a in gun["ammo"] if a in catalog and available(catalog[a])]
            if not gun["ammo"]:
                payloads = [gun]
            for payload in payloads:
                # Melee/stunners, brainsucker pods and smoke guns are not primary firearms.
                if payload["damage"] <= 0 or payload["range"] <= 4 or any(
                        mark in payload["damage_type"] for mark in ("STUN", "BRAINSUCKER", "SMOKE")):
                    continue
                weapons.append((weapon_rank(gun, payload, role, mission, enemies), gun, payload))
        if weapons:
            _, gun, payload = max(weapons, key=lambda w: (w[0], w[1]["id"], w[2]["id"]))
            plan.weapon = gun["id"]
            if payload is not gun:
                plan.ammo = payload["id"]
                # Supply margin is policy: two spare clips normally; five in the dimension,
                # with one more on the two highest difficulties (10.3, 14).
                plan.clips = (6 if mission == "alien_dimension" else 3) + int(difficulty >= 3)
            if mission == "alien_dimension" and role != "heavy" and payload["damage_type"] not in (
                    "DAMAGETYPE_TOXIN_B", "DAMAGETYPE_TOXIN_C"):
                plan.gaps.append("alien dimension requires researched Toxin B or C and a Toxigun")
        else:
            plan.gaps.append("no researched firearm with matching ammunition available")
        def extra(kind, count=1, damage_type="", preferred=""):
            options = [e for e in choices if e["kind"] == kind and
                       (not damage_type or e["damage_type"] == damage_type)]
            if options:
                item = max(options, key=lambda e: (e["id"] == preferred,
                           e["damage"] / max(1, e["weight"]), -e["weight"]))
                plan.extras[item["id"]] += count
        if role == "scout":
            extra("MotionScanner")
        if role == "support":
            extra("MediKit")
            extra("Grenade", damage_type="DAMAGETYPE_SMOKE")
            if not any(catalog[i]["kind"] == "MediKit" for i in plan.extras):
                plan.gaps.append("support needs a researched medikit")
        if role == "assault":
            extra("Grenade", 2, "DAMAGETYPE_EXPLOSIVE")
        if mission == "alien_dimension":
            # 10.3: mines for objectives, Devastator for walls, toxin for aliens.
            extra("Grenade", 2, "DAMAGETYPE_EXPLOSIVE", "AEQUIPMENTTYPE_VORTEX_MINE")
            if "AEQUIPMENTTYPE_VORTEX_MINE" not in plan.extras:
                plan.gaps.append("alien dimension objective kit needs researched Vortex Mines")
        elif role in ("assault", "support"):
            stunners = [e for e in choices if e["kind"] == "Weapon" and
                        e["damage_type"] == "DAMAGETYPE_STUN" and e["recharge"] > 0]
            if stunners:
                plan.extras[max(stunners, key=lambda e: -e["weight"])["id"]] += 1
        plans.append(plan)
    return plans


def purchase_orders(plans: list[Loadout], roster: list[dict], catalog: dict,
                    base: str, funds: int, reserve: int = RESERVE) -> dict[str, int]:
    """Clip units for the market, round units in stores. Never budget another base's supply."""
    demand = Counter()
    for plan in plans:
        if plan.base == base:
            demand.update(plan.demand(catalog))
    planned_agents = {p.agent for p in plans}
    for row in roster:
        if row["id"] in planned_agents and (
                row["base"] if row["base"] != "-" else row.get("home", "-")) == base:
            demand.subtract(held_units(row["kit"], catalog))
    budget = max(0, funds - reserve)
    orders = {}
    # Weapon+ammo first, then each armor body slot, then role tools and consumables.
    priority = {"Weapon": 0, "Ammo": 1, "Armor": 2, "MediKit": 3, "MotionScanner": 3}
    for item in sorted(demand, key=lambda i: (priority.get(catalog[i]["kind"], 4), i)):
        e = catalog[item]
        missing = demand[item] - e["stores"].get(base, 0) - e["incoming"].get(base, 0)
        unit = e["capacity"] if e["kind"] == "Ammo" else 1
        if missing <= 0 or unit <= 0 or e["price"] <= 0:
            continue
        count = min(math.ceil(missing / unit), e["market"], budget // e["price"])
        if count > 0:
            orders[item] = count
            budget -= count * e["price"]
    return orders


def verify_loadout(plan: Loadout, row: dict, catalog: dict) -> list[str]:
    failures = list(plan.gaps)
    kit = row["kit"]
    for slot in ARMOR_SLOTS:
        item = plan.armor.get(slot)
        if item and not any(e["id"] == item and e["slot"] == slot and
                            e["armor"] >= catalog[item]["armor"] for e in kit):
            failures.append(f"{slot} expected {item}")
    gun = next((e for e in kit if e["id"] == plan.weapon and e["slot"] in HANDS), None)
    if not gun or gun["rounds"] <= 0 or (plan.ammo and gun["payload"] != plan.ammo):
        failures.append(f"loaded primary expected {plan.weapon}/{plan.ammo or 'built-in'} in hand")
    units = held_units(kit, catalog)
    if plan.ammo and units[plan.ammo] < plan.clips * catalog[plan.ammo]["capacity"]:
        failures.append(f"ammo expected {plan.clips} full clips of {plan.ammo} including loaded")
    for item, count in plan.extras.items():
        if units[item] < count:
            failures.append(f"utility expected {count} x {item}")
    return failures


def prepare_loadouts(d, mission: str = "base_defence", agents: int = 24,
                     vehicle: str = "", reserve: int = RESERVE,
                     enemies: tuple[str, ...] = ()) -> dict:
    """Buy and refit through ordinary UI paths; observe every effect through gs.

    The global roster query works without opening an equipment screen. Away soldiers can
    be verified but must come home for a refit. No success is inferred from an OK reply.
    """
    import oa_play as ui

    reply = d.h.gs("equipment_catalog")
    catalog = catalog_from_reply(reply)
    roster_reply = d.h.gs("loadout_agents")
    if vehicle == "selected":
        vehicle = roster_reply.get("selected_vehicle", "-")
        if vehicle in ("", "-"):
            d.say(f"  [loadout-deferred] mission={mission} no single selected player vehicle")
            return {"ready": False, "verified": 0, "gained": 0, "plans": []}
    all_roster = roster_from_reply(roster_reply)
    roster = [r for r in all_roster if not vehicle or r.get("vehicle") == vehicle]
    # A selected transport's entire crew must pass verification; never silently truncate it.
    if not vehicle:
        roster = roster[:agents]
    if not catalog or not roster:
        d.say(f"  [loadout] mission={mission} catalog/roster unavailable; verification failed")
        return {"ready": False, "verified": 0, "gained": 0, "plans": []}
    difficulty = int(reply.get("difficulty", "0"))
    plans = plan_loadouts(catalog, roster, mission, difficulty, enemies)
    # Selling must protect both the current plan and the endgame kit (10.3).
    keep = {i for p in plans for i in p.demand(catalog)}
    endgame = plan_loadouts(catalog, roster, "alien_dimension", difficulty)
    keep.update(i for p in endgame for i in p.demand(catalog))
    d.loadout_keep = keep
    d.say(f"  [loadout-plan] mission={mission} roles={dict(Counter(p.role for p in plans))} "
          f"reserve=${reserve} difficulty={difficulty}")
    orders = purchase_orders(plans, all_roster, catalog, reply.get("purchase_base", "-"),
                             int(d.h.gs("funds").get("balance", "0")), reserve)
    if orders:
        d.say(f"  [loadout-buy] mission={mission} orders={orders}; awaiting delivery to "
              f"{reply.get('purchase_base')}")
        ui.buy_named(d, orders, reserve=reserve)
    before = sum(any(e["slot"] in HANDS and e["rounds"] > 0 and
                     catalog.get(e["id"], {}).get("kind") == "Weapon" for e in r["kit"])
                 for r in roster)
    current = {r["id"]: r for r in roster_from_reply(d.h.gs("loadout_agents"))}
    targets = [p for p in plans if p.agent in current and current[p.agent]["base"] != "-" and
               verify_loadout(p, current[p.agent], catalog)]
    if targets and ui._open_equip_screen(d):
        try:
            for plan in targets:
                if not ui._select_agent(d, plan.agent)[0]:
                    d.say(f"  [loadout-deferred] {plan.agent} could not be selected")
                    continue
                apply_loadout(d, plan, catalog, difficulty)
        finally:
            ui.return_to_city(d)
    current = {r["id"]: r for r in roster_from_reply(d.h.gs("loadout_agents"))}
    verified = 0
    for plan in plans:
        row = current.get(plan.agent)
        failures = verify_loadout(plan, row, catalog) if row else ["soldier absent from roster"]
        if failures:
            d.say(f"  [loadout-deferred] mission={mission} agent={plan.agent} role={plan.role}: "
                  + "; ".join(failures))
        else:
            verified += 1
            d.say(f"  [loadout-verified] mission={mission} agent={plan.agent} role={plan.role} "
                  f"armor={plan.armor} weapon={plan.weapon} ammo={plan.ammo or 'built-in'} "
                  f"clips={plan.clips} extras={dict(plan.extras)}")
    after = sum(any(e["slot"] in HANDS and e["rounds"] > 0 and
                    catalog.get(e["id"], {}).get("kind") == "Weapon" for e in r["kit"])
                for aid, r in current.items() if any(p.agent == aid for p in plans))
    result = {"ready": verified == len(plans), "verified": verified,
              "gained": after - before, "plans": plans}
    d.say(f"  [loadout] mission={mission} verified={verified}/{len(plans)} "
          f"loaded-soldiers={before}->{after} ready={int(result['ready'])}")
    return result


def apply_loadout(d, plan: Loadout, catalog: dict, difficulty: int) -> None:
    import oa_play as ui

    def observed():
        return next(r for r in roster_from_reply(d.h.gs("loadout_agents")) if r["id"] == plan.agent)

    def action(verb, item, suffix=""):
        reply = d.h.send(f"action {verb} {item}{suffix}")
        if not reply.startswith("OK"):
            d.say(f"  [loadout-action] {plan.agent} {verb} {item}: {reply}")
        return reply.startswith("OK")

    def tab(armor=False):
        d.h.ok("control " + ("BUTTON_SHOW_ARMOUR" if armor else "BUTTON_SHOW_WEAPONS") + " click")

    def stocked(item):
        return any(r["id"] == item and int(r.get("count", "0")) > 0 for r in ui.aequip_items(d))

    tab()
    row = observed()
    gun = next((e for e in row["kit"] if e["id"] == plan.weapon and e["slot"] in HANDS), None)
    if not gun and plan.weapon and stocked(plan.weapon):
        # Do not strip a veteran unless the replacement can be loaded from THIS base.
        base = row["base"]
        fresh = catalog_from_reply(d.h.gs("equipment_catalog"))
        ammo_ready = not plan.ammo or fresh.get(plan.ammo, {}).get("stores", {}).get(base, 0) > 0
        if ammo_ready:
            old = [e for e in row["kit"] if catalog.get(e["id"], {}).get("kind") in ("Weapon", "Ammo")]
            for e in old:
                action("aequip_unequip", e["id"])
            action("aequip_equip", plan.weapon)
            row = observed()
            gun = next((e for e in row["kit"] if e["id"] == plan.weapon and e["slot"] in HANDS), None)
            if not gun:
                # Failed fit: restore the old firearm through the same player path.
                for e in old:
                    if catalog[e["id"]]["kind"] == "Weapon":
                        action("aequip_equip", e["id"])
                        if e["payload"] != "-":
                            action("aequip_reload", e["id"], " " + e["payload"])
                d.say(f"  [loadout-deferred] {plan.agent} replacement did not reach a hand; attempted to restore prior firearm")
    if gun and plan.ammo:
        # Explicit payload selection matters: the Shift+click auto-loader uses reverse id order,
        # which can load incendiary instead of AP, or the wrong toxin. Drag the chosen clip in.
        cap = catalog[plan.ammo]["capacity"]
        if gun["payload"] != plan.ammo or gun["rounds"] < cap:
            action("aequip_reload", plan.weapon, " " + plan.ammo)
        wanted = plan.clips * cap
        while held_units(observed()["kit"], catalog)[plan.ammo] < wanted and stocked(plan.ammo):
            prior = held_units(observed()["kit"], catalog)[plan.ammo]
            action("aequip_equip", plan.ammo)
            if held_units(observed()["kit"], catalog)[plan.ammo] <= prior:
                break
    # A recruit should not wait unarmed for the ideal gun to arrive. Keep the desired plan
    # unverified, while issuing a researched, loaded fallback from local stores.
    if not any(e["slot"] in HANDS and e["rounds"] > 0 and catalog.get(e["id"], {}).get(
            "kind") == "Weapon" for e in observed()["kit"]):
        local = catalog_from_reply(d.h.gs("equipment_catalog"))
        for item in local.values():
            item["market"] = 0
            item["incoming"] = {}
        fallback = plan_loadouts(local, [observed()], plan.mission, difficulty,
                                 roles={plan.agent: plan.role})[0]
        if fallback.weapon and stocked(fallback.weapon):
            action("aequip_equip", fallback.weapon)
            if fallback.ammo:
                action("aequip_reload", fallback.weapon, " " + fallback.ammo)
    row = observed()
    if any(e["id"] == plan.weapon and e["slot"] in HANDS and e["rounds"] > 0 and
           (not plan.ammo or e["payload"] == plan.ammo) for e in row["kit"]):
        # Mission swaps must release obsolete grenades/tools and wrong ammunition; otherwise
        # old role kits silently fill every free slot over repeated refits.
        wanted = {plan.weapon, plan.ammo, *plan.extras}
        for e in row["kit"]:
            if catalog.get(e["id"], {}).get("kind") in (
                    "Weapon", "Ammo", "Grenade", "MediKit", "MotionScanner") and e["id"] not in wanted:
                action("aequip_unequip", e["id"])
    tab(True)
    for slot, wanted in plan.armor.items():
        row = observed()
        old = next((e for e in row["kit"] if e["slot"] == slot), None)
        if old and old["id"] == wanted and old["armor"] >= catalog[wanted]["armor"]:
            continue
        if not stocked(wanted):
            continue
        if old and not action("aequip_unequip", old["id"]):
            continue
        action("aequip_equip", wanted)
        if not any(e["id"] == wanted and e["slot"] == slot for e in observed()["kit"]) and old:
            action("aequip_equip", old["id"])
    tab()
    for item, count in plan.extras.items():
        for _ in range(max(0, count - held_units(observed()["kit"], catalog)[item])):
            if not stocked(item):
                break
            prior = held_units(observed()["kit"], catalog)[item]
            action("aequip_equip", item)
            if held_units(observed()["kit"], catalog)[item] <= prior:
                break
