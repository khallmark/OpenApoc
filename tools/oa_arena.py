#!/usr/bin/env python3
"""Combat arena: fight skirmish after skirmish, and let the X-COM side get better at it.

This runner does ONE thing -- tactical combat -- and does it on a loop. No research, no
purchasing, no city economy. A campaign run costs ~20 minutes of wall clock and yields a single
battle result buried in cityscape noise; four consecutive campaign runs in this project produced
`tactical=0` every time, meaning the battle code was never exercised at all. The arena exists so
that combat is the only variable.

The loop is: pick a policy -> fight N battles with it -> score it -> keep what wins, mutate, go
again. Every battle is appended to a JSONL ledger so improvement is measurable across runs rather
than asserted.

What a "policy" is, and what it deliberately is not: units default to
FirePermissionMode::AtWill and UnitAIDefault makes any conscious unit engage a hostile it can
see, so the engine does the shooting. What the driver actually controls is the *conditions* under
which that shooting happens -- fire mode and stance today, more as they are wired. This is not a
learned model; it is a search over a small, explicit policy space with results recorded honestly.
Calling it learning would overstate it.

KNOWN RISK, to be settled by running rather than assumed: oa_skirmish.py's own docstring records
that Skirmish battles never started -- `Skirmish::resume()` fires `loadBattle()` and then does
not transition, and one battlemap (BATTLEMAP_43sleep) threw "Failed to place mandatory sectors"
from a threadpool worker. That note predates this session's engine work. `--probe` fights exactly
one battle and reports precisely where it stopped, so the first question this tool answers is
whether the arena can run at all.
"""

from __future__ import annotations

import argparse
import json
import queue
import random
import sys
import threading
import time
from pathlib import Path

from oa_play import Driver, GameProcess, Harness, new_game
from oa_skirmish import run_one

# The policy space. Small and explicit on purpose: every axis here is a control the driver can
# actually operate through the harness, and nothing is included that it cannot.
FIRE_MODES = ["snap", "aimed", "auto"]
STANCES = ["run", "walk", "kneel"]


def policy_name(p: dict) -> str:
    return f"{p['fire_mode']}/{p['stance']}"


def all_policies() -> list[dict]:
    return [{"fire_mode": f, "stance": s, "name": f"{f}/{s}"}
            for f in FIRE_MODES for s in STANCES]


def score(results: list[dict]) -> dict:
    """Score a policy's battles. Win rate first, then survivors, then speed.

    Survivor fraction matters independently of winning: a win that costs the whole squad is not a
    result worth repeating, and in a campaign it is how you end up unable to fly the next mission.
    """
    n = len(results) or 1
    wins = sum(1 for r in results if r.get("outcome") == "resolved")
    surv = [r["survivor_frac"] for r in results if r.get("survivor_frac") is not None]
    secs = [r["seconds"] for r in results if r.get("seconds") is not None]
    return {
        "battles": len(results),
        "win_rate": wins / n,
        "survivor_frac": (sum(surv) / len(surv)) if surv else 0.0,
        "avg_seconds": (sum(secs) / len(secs)) if secs else 0.0,
        # Lexicographic-ish: wins dominate, survivors break ties, speed breaks those.
        "fitness": wins / n * 100.0 + ((sum(surv) / len(surv)) if surv else 0.0) * 10.0
                   - ((sum(secs) / len(secs)) if secs else 0.0) / 600.0,
    }


def fight(d: Driver, aliens: dict, policy: dict, map_row: int, budget_s: float) -> dict:
    """One battle. Returns a flat record; never raises for an in-game failure."""
    t0 = time.time()
    before = {}
    try:
        before = d.h.gs("battle")
    except Exception:
        pass
    try:
        r = run_one(d, aliens, map_row=map_row, budget_s=budget_s, policy=policy)
        outcome = r.get("outcome", "error")
    except Exception as exc:  # a broken battle must not kill the arena
        outcome = f"exception:{type(exc).__name__}"
        r = {}
    after = {}
    try:
        after = d.h.gs("battle")
    except Exception:
        pass

    def num(dct, key):
        try:
            return int(dct.get(key, ""))
        except (ValueError, AttributeError):
            return None

    mine0, mine1 = num(before, "mine"), num(after, "mine_alive")
    frac = (mine1 / mine0) if (mine0 and mine1 is not None) else None
    return {
        "policy": policy["name"],
        "fire_mode": policy["fire_mode"],
        "stance": policy["stance"],
        "aliens": aliens,
        "map_row": map_row,
        "outcome": outcome,
        "seconds": round(time.time() - t0, 1),
        "squad_start": mine0,
        "squad_end": mine1,
        "survivor_frac": frac,
        "foes_start": num(before, "foes"),
        "foes_end": num(after, "foes_alive"),
    }


def recover(d: Driver, tries: int = 8) -> bool:
    """Get back somewhere a new skirmish can be started from, whatever we ended in."""
    for _ in range(tries):
        st = d.status()
        if st.stage == "CityView":
            return True
        if st.stage == "BattleDebriefing":
            d.click_id("BUTTON_OK", st)
        elif not d.dismiss_modal(st):
            d.h.key("Escape")
        time.sleep(0.6)
    return d.status().stage == "CityView"


def run_worker(wid: int, port: int, repo: Path, out: Path, aliens: dict, args,
               work: "queue.Queue", results: "queue.Queue", ledger_lock: threading.Lock,
               ledger: Path, stop: threading.Event) -> None:
    """One worker = one whole game process on its own port, fighting its own battles.

    Threads rather than a process pool because the work is almost entirely blocked on socket
    round-trips to a separate game process -- the GIL is irrelevant here, and threads keep the
    shared ledger and policy queue trivially coordinated. Each worker owns its own GameProcess,
    Harness port, screenshot dir and log, so nothing is shared with another worker except the
    work queue and the results queue.

    A worker that dies takes only its own battles with it: exceptions are caught, reported, and
    the policy it was holding is returned to the queue for another worker.
    """
    wout = out / f"w{wid}"
    wout.mkdir(parents=True, exist_ok=True)
    game = None
    try:
        game = GameProcess(repo, port, wout / "game.log")
        game.start(wait_s=240)
        d = Driver(Harness(port=port), repo / "data/forms", shots=wout / "shots", verbose=True)
        d.checks = {}
        new_game(d, 1)
        print(f"[arena] worker {wid} ready on port {port}", flush=True)
        while not stop.is_set():
            try:
                gen, pol = work.get(timeout=2.0)
            except queue.Empty:
                continue
            rs = []
            try:
                for i in range(args.battles_per_policy):
                    if stop.is_set():
                        break
                    rec = fight(d, aliens, pol, args.map_row, args.budget)
                    rec.update(generation=gen, worker=wid)
                    rs.append(rec)
                    with ledger_lock, ledger.open("a") as fh:
                        fh.write(json.dumps(rec) + "\n")
                    print(f"[arena] w{wid} gen{gen} {pol['name']} {i+1}/"
                          f"{args.battles_per_policy} -> {rec['outcome']} "
                          f"surv={rec['squad_end']}/{rec['squad_start']} {rec['seconds']}s",
                          flush=True)
                    if not recover(d):
                        print(f"[arena] w{wid} lost the UI; abandoning this policy", flush=True)
                        break
            except Exception as exc:
                print(f"[arena] w{wid} FAILED on {pol['name']}: "
                      f"{type(exc).__name__}: {exc}", flush=True)
            finally:
                sc = score(rs)
                sc.update(policy=pol["name"], generation=gen, worker=wid)
                results.put((sc["fitness"], pol, sc))
                work.task_done()
    except Exception as exc:
        print(f"[arena] worker {wid} could not start: {type(exc).__name__}: {exc}", flush=True)
    finally:
        if game is not None:
            try:
                game.stop()
            except Exception:
                pass
        print(f"[arena] worker {wid} stopped", flush=True)


def run_parallel(args, repo: Path, out: Path, aliens: dict) -> int:
    """Generation loop across N concurrent games.

    Each generation is a barrier: every policy is fought by some worker, then all of them are
    scored together before selection. That keeps the comparison fair -- policies are judged
    against the same generation's opposition, not against whatever a faster worker happened to
    race ahead into.
    """
    ledger = out / "battles.jsonl"
    ledger_lock = threading.Lock()
    work: queue.Queue = queue.Queue()
    results: queue.Queue = queue.Queue()
    stop = threading.Event()

    threads = []
    for wid in range(args.workers):
        t = threading.Thread(target=run_worker, name=f"arena-w{wid}",
                             args=(wid, args.port + wid, repo, out, aliens, args,
                                   work, results, ledger_lock, ledger, stop),
                             daemon=True)
        t.start()
        threads.append(t)
        time.sleep(3.0)  # stagger process starts; simultaneous boots thrash the disk

    pool = all_policies()
    random.shuffle(pool)
    gen = 0
    best_ever = None
    try:
        while args.generations == 0 or gen < args.generations:
            gen += 1
            print(f"=== generation {gen}: {len(pool)} policies across {args.workers} "
                  f"workers x {args.battles_per_policy} battles ===", flush=True)
            for pol in pool:
                work.put((gen, pol))
            scored = []
            for _ in range(len(pool)):
                scored.append(results.get())
            scored.sort(key=lambda t: t[0], reverse=True)
            best_fit, best_pol, _ = scored[0]
            if best_ever is None or best_fit > best_ever[0]:
                best_ever = (best_fit, best_pol)
            for f, p, sc in scored:
                print(f"[arena] {p['name']:<12} win={sc['win_rate']:.2f} "
                      f"surv={sc['survivor_frac']:.2f} fit={f:.1f}", flush=True)
                with (out / "policies.jsonl").open("a") as fh:
                    fh.write(json.dumps(sc) + "\n")
            print(f"=== gen {gen} best: {best_pol['name']} {best_fit:.1f} "
                  f"| best ever: {best_ever[1]['name']} {best_ever[0]:.1f} ===", flush=True)

            keep = [p for _f, p, _s in scored[: max(1, len(scored) // 2)]]
            children = []
            for p in keep:
                c = dict(p)
                if random.random() < 0.5:
                    c["fire_mode"] = random.choice(FIRE_MODES)
                else:
                    c["stance"] = random.choice(STANCES)
                c["name"] = policy_name(c)
                children.append(c)
            seen, pool = set(), []
            for p in keep + children:
                if p["name"] not in seen:
                    seen.add(p["name"])
                    pool.append(p)
    except KeyboardInterrupt:
        print("[arena] interrupted; stopping workers", flush=True)
    finally:
        stop.set()
        for t in threads:
            t.join(timeout=30)
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--port", type=int, default=17901)
    ap.add_argument("--repo", default=str(Path(__file__).resolve().parent.parent))
    ap.add_argument("--out", default=None)
    ap.add_argument("--battles-per-policy", type=int, default=3)
    ap.add_argument("--generations", type=int, default=0,
                    help="0 = run forever until stopped")
    ap.add_argument("--budget", type=float, default=240.0, help="seconds per battle")
    ap.add_argument("--map-row", type=int, default=0)
    ap.add_argument("--alien", action="append", default=[],
                    help="name=count, repeatable. Held FIXED across the run: the aliens are the "
                         "opponent, not another variable.")
    ap.add_argument("--workers", type=int, default=1,
                    help="concurrent game processes, each on its own port (port, port+1, ...)")
    ap.add_argument("--probe", action="store_true",
                    help="fight exactly one battle and report where it got to, then exit")
    args = ap.parse_args()

    aliens = {}
    for spec in args.alien:
        name, _, count = spec.partition("=")
        aliens[name] = int(count or 1)
    if not aliens:
        aliens = {"anthropod": 4, "skeletoid": 2}

    repo = Path(args.repo)
    out = Path(args.out) if args.out else repo / "build/arena"
    out.mkdir(parents=True, exist_ok=True)
    ledger = out / "battles.jsonl"

    game = GameProcess(repo, args.port, out / "game.log")
    game.start(wait_s=240)
    d = Driver(Harness(port=args.port), repo / "data/forms", shots=out / "shots", verbose=True)
    d.checks = {}
    new_game(d, 1)

    if args.probe:
        pol = {"fire_mode": "snap", "stance": "run", "name": "snap/run"}
        d.say(f"[arena] PROBE: one battle vs {aliens}")
        rec = fight(d, aliens, pol, args.map_row, args.budget)
        d.say(f"[arena] PROBE result: {json.dumps(rec)}")
        d.say(f"[arena] PROBE final stage: {d.status().stage}")
        with ledger.open("a") as fh:
            fh.write(json.dumps({"probe": True, **rec}) + "\n")
        game.stop()
        return 0 if rec["outcome"] in ("resolved", "lost", "withdrew") else 1

    if args.workers > 1:
        game.stop()  # the single probe/bootstrap process is not needed in parallel mode
        return run_parallel(args, repo, out, aliens)

    pool = all_policies()
    random.shuffle(pool)
    gen = 0
    best_ever = None
    while args.generations == 0 or gen < args.generations:
        gen += 1
        d.say(f"=== generation {gen}: {len(pool)} policies x {args.battles_per_policy} battles ===")
        scored = []
        for pol in pool:
            rs = []
            for i in range(args.battles_per_policy):
                d.say(f"[arena] gen{gen} {pol['name']} battle {i+1}/{args.battles_per_policy}")
                rec = fight(d, aliens, pol, args.map_row, args.budget)
                rec.update(generation=gen)
                rs.append(rec)
                with ledger.open("a") as fh:
                    fh.write(json.dumps(rec) + "\n")
                d.say(f"[arena]   -> {rec['outcome']} "
                      f"survivors={rec['squad_end']}/{rec['squad_start']} {rec['seconds']}s")
                if not recover(d):
                    d.say("[arena] could not get back to CityView; ending generation")
                    break
            sc = score(rs)
            sc.update(policy=pol["name"], generation=gen)
            scored.append((sc["fitness"], pol, sc))
            d.say(f"[arena] {pol['name']}: win_rate={sc['win_rate']:.2f} "
                  f"surv={sc['survivor_frac']:.2f} fitness={sc['fitness']:.1f}")
            with (out / "policies.jsonl").open("a") as fh:
                fh.write(json.dumps(sc) + "\n")

        scored.sort(key=lambda t: t[0], reverse=True)
        if not scored:
            d.say("[arena] no policies scored; stopping")
            break
        best_fit, best_pol, best_sc = scored[0]
        if best_ever is None or best_fit > best_ever[0]:
            best_ever = (best_fit, best_pol, best_sc)
        d.say(f"=== gen {gen} best: {best_pol['name']} fitness={best_fit:.1f} "
              f"| best ever: {best_ever[1]['name']} {best_ever[0]:.1f} ===")

        # Keep the top half, refill by mutating them one axis at a time.
        keep = [p for _f, p, _s in scored[: max(1, len(scored) // 2)]]
        children = []
        for p in keep:
            c = dict(p)
            if random.random() < 0.5:
                c["fire_mode"] = random.choice(FIRE_MODES)
            else:
                c["stance"] = random.choice(STANCES)
            c["name"] = policy_name(c)
            children.append(c)
        seen, pool = set(), []
        for p in keep + children:
            if p["name"] not in seen:
                seen.add(p["name"])
                pool.append(p)

    game.stop()
    return 0


if __name__ == "__main__":
    sys.exit(main())
