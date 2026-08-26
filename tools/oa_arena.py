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

KNOWN RISK, to be settled by running rather than assumed: the current framework can discard stage
commands queued by another stage's `resume()` while it drains a snapshot of the command queue.
That leaves the cold Skirmish path on SelectForces or Skirmish before BattleView is ever reached.
`--probe` fights exactly one battle and reports that lifecycle failure as `setup_failure`; it must
never be folded into the tactical loss rate.
"""

from __future__ import annotations

import argparse
import json
import math
import queue
import random
import sys
import threading
import time
from pathlib import Path

from oa_play import (
    BattleResult,
    Driver,
    GameProcess,
    Harness,
    battle_is_decided,
    new_game,
    require_positive,
)
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


def score(results: list[dict], expected_battles: int | None = None) -> dict:
    """Score a policy's battles. Win rate first, then survivors, then speed.

    Survivor fraction matters independently of winning: a win that costs the whole squad is not a
    result worth repeating, and in a campaign it is how you end up unable to fly the next mission.
    """
    gameplay = [r for r in results if r.get("result_kind", "gameplay") == "gameplay"]
    decided = [r for r in gameplay if battle_is_decided(r)]
    incomplete_battles = len(gameplay) - len(decided)
    setup_failures = sum(1 for r in results if r.get("result_kind") == "setup_failure")
    driver_errors = sum(1 for r in results if r.get("result_kind") == "driver_error")
    n = len(decided) or 1
    wins = sum(1 for r in decided if r.get("outcome") == "resolved")
    surv = [r["survivor_frac"] for r in decided if r.get("survivor_frac") is not None]
    secs = [r["seconds"] for r in decided if r.get("seconds") is not None]
    complete = bool(decided) and not setup_failures and not driver_errors and not incomplete_battles
    if expected_battles is not None:
        complete = complete and len(decided) == expected_battles and len(results) == expected_battles
    fitness = (wins / n * 100.0
               + ((sum(surv) / len(surv)) if surv else 0.0) * 10.0
               - ((sum(secs) / len(secs)) if secs else 0.0) / 600.0)
    if not decided or not complete:
        # A policy that never entered a battle is not a zero-win policy. Keep it well below any
        # real score without emitting non-standard JSON Infinity values into policies.jsonl.
        fitness = -1_000_000_000.0
    return {
        "attempts": len(results),
        "gameplay_attempts": len(gameplay),
        "battles": len(decided),
        "incomplete_battles": incomplete_battles,
        "setup_failures": setup_failures,
        "driver_errors": driver_errors,
        "expected_battles": expected_battles,
        "complete": complete,
        "win_rate": wins / n,
        "survivor_frac": (sum(surv) / len(surv)) if surv else 0.0,
        "avg_seconds": (sum(secs) / len(secs)) if secs else 0.0,
        # Lexicographic-ish: wins dominate, survivors break ties, speed breaks those.
        "fitness": fitness,
    }


def fight(d: Driver, aliens: dict, policy: dict, map_row: int, budget_s: float) -> dict:
    """One battle. Returns a flat record; never raises for an in-game failure."""
    t0 = time.time()
    try:
        r = run_one(d, aliens, map_row=map_row, budget_s=budget_s, policy=policy)
        result_kind = r.get("result_kind", "gameplay")
        outcome = r.get("outcome")
    except Exception as exc:  # a broken battle must not kill the arena
        result_kind = "driver_error"
        outcome = None
        r = {
            "reason": f"{type(exc).__name__}: {exc}",
            "stage": "unavailable",
        }
    captured = getattr(d, "last_battle", {}) if result_kind == "gameplay" else {}
    if isinstance(captured, BattleResult):
        stats = captured.as_dict()
    elif isinstance(captured, dict):
        stats = dict(captured)
    else:
        stats = {}

    def num(dct, key):
        try:
            value = dct.get(key)
            return int(value) if value is not None else None
        except (TypeError, ValueError, AttributeError):
            return None

    mine0, mine1 = num(stats, "started_with"), num(stats, "survivors")
    frac = (mine1 / mine0) if (mine0 and mine1 is not None) else None
    return {
        "policy": policy["name"],
        "fire_mode": policy["fire_mode"],
        "stance": policy["stance"],
        "aliens": aliens,
        "map_row": map_row,
        "result_kind": result_kind,
        "outcome": outcome,
        "decided": battle_is_decided(r),
        "stage": r.get("stage", ""),
        "reason": r.get("reason", ""),
        "seconds": float(stats.get("seconds", round(time.time() - t0, 1)) or 0.0),
        "squad_start": mine0,
        "squad_end": mine1,
        "survivor_frac": frac,
        "mission_type": stats.get("mission_type", "unknown"),
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


class ArenaCoordinatorError(RuntimeError):
    pass


def collect_policy_results(results: "queue.Queue", generation: int, policy_count: int,
                           timeout_s: float) -> list[tuple[float, dict, dict]]:
    """Collect one exact generation or fail on timeout, worker death, stale, or partial data."""
    deadline = time.monotonic() + timeout_s
    scored = []
    seen_policies = set()
    while len(scored) < policy_count:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise ArenaCoordinatorError(
                "coordinator deadline expired before all policy results arrived"
            )
        try:
            message = results.get(timeout=min(5.0, remaining))
        except queue.Empty:
            continue
        if message.get("kind") == "worker_terminal":
            raise ArenaCoordinatorError(
                f"worker {message['worker']} terminated: {message['status']} "
                f"({message['reason']})"
            )
        if message.get("kind") != "policy_result":
            raise ArenaCoordinatorError(f"unknown worker message: {message!r}")
        pol, sc = message["policy"], message["score"]
        if sc.get("generation") != generation or pol["name"] in seen_policies:
            raise ArenaCoordinatorError("duplicate or stale policy result")
        seen_policies.add(pol["name"])
        scored.append((message["fitness"], pol, sc))
        if message.get("error") or not sc.get("complete"):
            raise ArenaCoordinatorError(
                f"incomplete policy {pol['name']}: {message.get('error') or sc}"
            )
    return scored


def run_worker(wid: int, port: int, repo: Path, out: Path, aliens: dict, args,
               work: "queue.Queue", results: "queue.Queue", ledger_lock: threading.Lock,
               ledger: Path, stop: threading.Event) -> None:
    """One worker = one whole game process on its own port, fighting its own battles.

    Threads rather than a process pool because the work is almost entirely blocked on socket
    round-trips to a separate game process -- the GIL is irrelevant here, and threads keep the
    shared ledger and policy queue trivially coordinated. Each worker owns its own GameProcess,
    Harness port, screenshot dir and log, so nothing is shared with another worker except the
    work queue and the results queue.

    Every exit emits a terminal record. Every claimed policy emits exactly one policy-result
    record, complete or failed, so the coordinator never waits forever on an invisible worker.
    """
    wout = out / f"w{wid}"
    wout.mkdir(parents=True, exist_ok=True)
    game = None
    terminal_status = "clean"
    terminal_reason = "stop requested"
    try:
        game = GameProcess(repo, port, wout / "game.log", seed=(args.seed + wid) if args.seed else 0)
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
            error = ""
            try:
                for i in range(args.battles_per_policy):
                    if stop.is_set():
                        error = "coordinator requested stop"
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
                    if not battle_is_decided(rec):
                        error = (f"undecided attempt: {rec['result_kind']} / "
                                 f"{rec.get('outcome')} ({rec.get('reason', '')})")
                        print(f"[arena] w{wid} {error}", flush=True)
                        break
                    if not recover(d):
                        error = "could not recover UI for next battle"
                        print(f"[arena] w{wid} {error}", flush=True)
                        break
            except Exception as exc:
                error = f"{type(exc).__name__}: {exc}"
                print(f"[arena] w{wid} FAILED on {pol['name']}: {error}", flush=True)
            finally:
                sc = score(rs, expected_battles=args.battles_per_policy)
                sc.update(policy=pol["name"], generation=gen, worker=wid)
                results.put({
                    "kind": "policy_result",
                    "fitness": sc["fitness"],
                    "policy": pol,
                    "score": sc,
                    "error": error or ("" if sc["complete"] else "incomplete policy batch"),
                })
                work.task_done()
    except Exception as exc:
        terminal_status = "error"
        terminal_reason = f"{type(exc).__name__}: {exc}"
        print(f"[arena] worker {wid} could not start: {terminal_reason}", flush=True)
    finally:
        if game is not None:
            try:
                game.stop()
            except Exception:
                pass
        results.put({
            "kind": "worker_terminal",
            "worker": wid,
            "status": terminal_status,
            "reason": terminal_reason,
        })
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
    result_code = 0
    try:
        while args.generations == 0 or gen < args.generations:
            gen += 1
            print(f"=== generation {gen}: {len(pool)} policies across {args.workers} "
                  f"workers x {args.battles_per_policy} battles ===", flush=True)
            for pol in pool:
                work.put((gen, pol))
            # Allow worker startup plus one worst-case sequential wave per worker. A dead worker
            # now emits a terminal record, and this deadline covers the remaining failure mode:
            # a thread alive but stuck outside every battle's own budget.
            waves = math.ceil(len(pool) / args.workers)
            timeout_s = 300.0 + waves * args.battles_per_policy * (args.budget + 60.0)
            try:
                scored = collect_policy_results(results, gen, len(pool), timeout_s)
            except ArenaCoordinatorError as exc:
                print(f"[arena] {exc}", flush=True)
                return 1
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
        result_code = 130
    finally:
        stop.set()
        for t in threads:
            t.join(timeout=30)
    return result_code


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
    ap.add_argument("--seed", type=int, default=0,
                    help="explicit RNG seed; 0 keeps the engine default")
    ap.add_argument("--probe", action="store_true",
                    help="fight exactly one battle and report where it got to, then exit")
    args = ap.parse_args()
    try:
        require_positive(args.battles_per_policy, "--battles-per-policy")
        require_positive(args.budget, "--budget")
        require_positive(args.workers, "--workers")
        if args.generations < 0:
            raise ValueError("--generations must be zero (unbounded) or positive")
    except ValueError as exc:
        ap.error(str(exc))

    aliens = {}
    for spec in args.alien:
        name, _, count = spec.partition("=")
        aliens[name] = int(count or 1)
        if aliens[name] <= 0:
            ap.error(f"--alien count must be positive: {spec}")
    if not aliens:
        aliens = {"anthropod": 4, "skeletoid": 2}

    repo = Path(args.repo)
    out = Path(args.out) if args.out else repo / "build/arena"
    out.mkdir(parents=True, exist_ok=True)
    ledger = out / "battles.jsonl"

    if args.workers > 1 and not args.probe:
        return run_parallel(args, repo, out, aliens)

    game = GameProcess(repo, args.port, out / "game.log", seed=args.seed)
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
        return 0 if battle_is_decided(rec) else 1

    pool = all_policies()
    random.shuffle(pool)
    gen = 0
    best_ever = None
    fatal_driver_failure = False
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
                if not battle_is_decided(rec):
                    d.say(f"[arena] undecided attempt: {rec['result_kind']} / "
                          f"{rec.get('outcome')} stage={rec['stage']} reason={rec['reason']}")
                    fatal_driver_failure = True
                    break
                if not recover(d):
                    d.say("[arena] could not get back to CityView; ending generation")
                    fatal_driver_failure = True
                    break
            sc = score(rs, expected_battles=args.battles_per_policy)
            sc.update(policy=pol["name"], generation=gen)
            scored.append((sc["fitness"], pol, sc))
            d.say(f"[arena] {pol['name']}: win_rate={sc['win_rate']:.2f} "
                  f"surv={sc['survivor_frac']:.2f} fitness={sc['fitness']:.1f}")
            with (out / "policies.jsonl").open("a") as fh:
                fh.write(json.dumps(sc) + "\n")
            if not sc["complete"]:
                fatal_driver_failure = True
            if fatal_driver_failure:
                break

        if fatal_driver_failure:
            d.say("[arena] stopping: setup/driver failure is not a gameplay loss")
            break

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
    return 1 if fatal_driver_failure else 0


if __name__ == "__main__":
    sys.exit(main())
