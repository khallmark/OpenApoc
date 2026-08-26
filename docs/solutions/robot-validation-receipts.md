# Truthful robot campaign validation

The unattended drivers have two operating modes:

- Ordinary mode retains the existing resumable output directory and permits seed `0` and the
  shared-binary fallback.
- Validation mode creates one immutable directory per campaign attempt, requires an explicit
  nonzero RNG seed, and refuses to run if the executable cannot be copied away from rebuilds.

Use the victory driver for a complete single-campaign gate:

```sh
python3 tools/oa_victory.py \
  --validation --single-campaign --seed 101 \
  --out build/validation/victory --hours 72
```

The shorter campaign drivers expose the same evidence mode:

```sh
python3 tools/oa_play.py --validation --seed 101 --out build/validation/smoke --days 7
python3 tools/oa_campaign.py --validation --seed 101 --out build/validation/campaign --hours 48
```

The cold tactical path uses the same receipt contract:

```sh
python3 tools/oa_skirmish.py \
  --validation --cold-main-menu --seed 101 \
  --out build/validation/skirmish --rounds 1
```

On the R0 branch alone, this command is expected to exit nonzero with `setup_failed`: the current
snapshot-drain StageCmd loop loses a POP queued by `SelectForces::resume()` (and then the battle
transition queued by `Skirmish::resume()` if that POP is supplied manually). R0's promise is that
this is recorded as setup failure with no gameplay outcome. Once the live-FIFO scheduler PR is
integrated, the exact command becomes a required-green one-battle smoke.

Each invocation creates a unique child directory beneath `--out`. It contains:

- `provenance.json`: driver arguments, git commit, seed, source binary and its SHA-256;
- `events.jsonl`: append-only lifecycle and terminal records, plus advance and battle records for
  drivers that operate in discrete legs;
- `terminal.json`: the atomically-written final outcome and process exit code;
- the driver's existing logs, screenshots, checkpoints and warnings.

Only `AdvanceOutcome.REACHED` credits a requested clock leg. A battle transition is recorded but
does not credit time; partial progress, a parked stage, no-progress timeout and a terminal stage
are distinct outcomes. Validation exits nonzero for defeat, an unknown ending video, time-budget
exhaustion, partial or parked clock progress, protocol/transport/process failures, and tactical
timeouts. Victory is recognized only from `Status.detail` naming the winning video; merely reaching
`VideoScreen` is never accepted as a win.

Validation cannot be combined with `oa_play.py --no-launch`: an attached process has no provable
private binary snapshot, so it cannot produce the provenance this mode promises.

Run the socket-free regression suite before using a build as campaign evidence:

```sh
for test_file in tools/test_oa_*.py; do python3 "$test_file" || exit $?; done
```
