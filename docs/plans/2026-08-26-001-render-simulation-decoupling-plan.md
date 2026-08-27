---
artifact_readiness: requirements-only
execution: code-and-evidence
---

# Plan: Decouple rendering, control, and game simulation

Analysis coverage: code-read ✓ (local `develop@4a8e01ac`, remote `develop@78319df5`, reconciled
base `11ca3c08`) · call-graph ✓/~ (fresh codebase-memory index is structurally available, but its
git binding is unavailable and `tools/` is deliberately excluded; exact-worktree source remains
ground truth) · requirements ✓/~ (GitHub-visible bodies, comments, and exact-head diffs for upstream
PRs #1237, #1270, #1166 and issues #997/#1216/#1336 are dispositioned; #997's linked Discord and
Trello sources remain unavailable/unverified and cannot authorize code) · prior art ✓ (`git`
history, `docs/solutions/`, existing tests, and the abandoned timer/multiplier branches) · runtime
profiling ✗ (Phase 0 gate)

## 1. Problem

`Framework::run()` advances the active `Stage` exactly once per rendered frame. City and battle
views mix simulation with input/UI maintenance, while other stages require `update()` only to boot,
finish loading, advance video state, or expose controls. Rendering also mutates presentation timers,
and automation obtains throughput by requesting 1000 rendered frames per second. The result is a
game whose simulation speed, UI cadence, automation throughput, and some gameplay constants are
coupled to display cadence.

The core objective is exact compatibility with the current default 60-FPS behavior at
`TICKS_MULTIPLIER == 4` and `UPDATE_EVERY_TICK == false`, followed by separately validated combat
parity and experimental 180-TPS work. A global multiplier flip is not the implementation.

The empirical backstop is intentionally excessive: the robot must repeatedly play the **entire
accumulated game** from fresh processes to classified victory, not merely exercise the edited
subsystem. Candidate admission requires three victories on the exact immutable candidate; every
exact landed post-R1b head then requires twenty new victories before any later non-recovery source
branch may progress; an independent, gap-free nightly schedule contributes twenty more without
receipt reuse. These runs supplement focused tests and original-game evidence—they do not permit a
candidate to choose its oracle, reuse a prior head's result, or outvote a red campaign.
This is continuous train control, not a final regression run: no runtime slice advances on a robot
result from a predecessor, a candidate-only tree, a partial campaign, an in-process reload, or a
different integrated head. Every credited whole-game campaign also carries its own same-attempt,
same-fence, same-campaign raw proof of a genuine turn-based action epoch and a planned save,
acknowledged clean exit, orderly reap, fresh-process reload, and continued victory. A normalized
coverage bit without that bijective raw evidence is a logical red, everywhere—not only during oracle
calibration.

## 2. Current state

```text
Framework.TargetFPS loop
  -> sleep-until-frame-deadline; continue without work while early
  -> harness + SDL events
  -> active Stage::update()          control + simulation
  -> copied StageCmd batch           lifecycle-generated commands can be discarded
  -> Stage::render() + swap          presentation state can mutate
  -> advance/rebase frame deadline
```

Material sites:

- `framework/framework.cpp::Framework::run` computes a frame duration by dividing by
  `Framework.TargetFPS`, runs one `Stage::update()`, applies a copied command batch, renders, and
  swaps.
- `framework/stage.h::Stage` already exposes `begin()`, `pause()`, `resume()`, `finish()`,
  `eventOccurred()`, `update()`, `render()`, `isTransition()`, and `harnessDetail()`; it has no
  separate control-heartbeat or simulation capability contract.
- `Framework::processEvents()` drains one mixed queue before the sole update, including re-entrant
  form events and game-state events emitted by the prior update. A game-state event from pulse N is
  therefore delivered before pulse N+1; that causal barrier is compatibility-critical.
- `game/ui/tileview/cityview.cpp::CityView::update` advances 0/1/2/4/6 ticks or calls turbo, then
  performs control/UI maintenance. Several callbacks call it only to refresh controls and
  accidentally advance time.
- `game/ui/tileview/battleview.cpp::BattleView::update` mixes pre-simulation hotseat/turn gates,
  exact state-update chunking, post-simulation mission transitions, audio, camera, selection, and
  UI timers.
- `game/ui/boot.cpp`, `game/ui/general/loadingscreen.cpp`,
  `game/ui/general/videoscreen.cpp`, and `game/ui/battle/battlebriefing.cpp` need control updates even
  when simulation is manually frozen.
- `framework/harness.cpp` supports multiple commands per connection, but `tools/oa_play.py::Harness`
  and the module-level `tools/oa_harness.py::send()` helper open a new TCP connection per command.
- `tools/oa_play.py` passes `--Framework.TargetFPS=1000`, copies `TICKS_PER_DAY`, and returns the
  same value for target reached and timeout.

Normative target (all later phases use this exact ordering):

```text
bounded outer loop
  -> service urgent window/harness/STEP-controller controls
  -> immediate live StageCmd barrier; terminal result exits before automatic work
  -> REALTIME only: 0..4 historical simulation transactions from the single sampled
       `historical_limit` after any due-primary reservation
       drain prior-pulse causal events -> live StageCmd barrier -> reacquire
       simulation-only eligibility + exact pulse + causal finalization -> live StageCmd barrier
       stop on stage generation change, NeedsControl, transition, pause, or failure
  -> zero or one due 60 Hz primary control transaction; exactly one when its deadline is due
       drain prior-pulse causal events -> live StageCmd barrier -> reacquire
       dispatch stage-tagged fresh, async, and prior control-originated queued deliveries to quiescence
       same-stage beginControlHeartbeat()
       REALTIME only: zero or one exact simulation transaction when permitted
       same-stage finishControlHeartbeat() (paired after every typed begin result)
       one live StageCmd barrier; there is never a mid-pair barrier
  -> MANUAL only: bounded explicit job slice
       before each pulse: drain prior-job causal events -> live StageCmd barrier -> reacquire
       simulation transaction -> live StageCmd barrier; no repeated wall/control work
  -> presentation at an independent deadline
  -> interruptible wait until earliest control, eligible REALTIME simulation, capped presentation,
     or F lease/job deadline; wake immediately for ready I/O/completion
```

`StageCmd`s may be queued by input, causal-event dispatch, either control half, or a simulation
transaction, but they are applied only at the named barrier. A causal-event barrier is not fresh
input or a control heartbeat: it preserves the existing pulse-N-event-before-pulse-N+1 ordering.
Urgent QUIT is accepted and drained before catch-up. A stage pointer is never used after a barrier
without reacquiring it and checking `stageGeneration`.

### Fixed-scheduler deadline algebra

E represents an injected `MonotonicInstant` as a checked signed integer nanosecond count and defines
`one_second = 1'000'000'000 ns`. Control and automatic simulation share one
`Cadence60{generation, epoch, next_control_ordinal, oldest_due_simulation_ordinal,
next_simulation_ordinal, automatic_gate}` and the single deadline function
`D(k) = epoch + floor(k * one_second / 60)`, evaluated with checked 128-bit intermediate arithmetic.
They never derive one another from elapsed-time multiplication, so the `floor`/`ceil` adjoint cannot
separate them by one nanosecond. Capped presentation owns a separate
`DeadlinePhase{epoch, ordinal, rate}` using the same checked deadline function at its configured
rate. At fixed-scheduler startup both 60-Hz ordinals are zero, so `D(0)` makes the first control and
simulation opportunities immediate, matching the legacy first frame.

For a sampled `now >= epoch`, define
`U(now) = ceil_div((((now - epoch) + 1) * 60), one_second)` in checked unsigned 128-bit arithmetic;
exactly the ordinals in `[0,U(now))` are due. Automatic debt is the constant-space identity-bearing
range `[oldest_due_simulation_ordinal,next_simulation_ordinal)`. Sampling extends only its exclusive
end to `max(next_simulation_ordinal,U(now))`; it never saturates or silently loses a disposition.
Control is due exactly when `next_control_ordinal < U(now)`. After its one transaction that began at
ordinal `c`, a post-transaction sample used only for control computes `u=U(post_control_now)`, adds
`u-(c+1)` to `control_deadline_missed`, and sets `next_control_ordinal=u`. A backwards clock or any
checked conversion/arithmetic failure is a typed fatal scheduler result before further state
mutation. A half-open interval `[epoch + m*one_second, epoch + (m+1)*one_second)` therefore contains
exactly 60 opportunity ordinals; the right endpoint is ordinal `60*(m+1)` in the following interval.
Tests never call the closed interval from ordinal zero “one second of 60 opportunities,” because it
correctly contains both the startup opportunity and the next interval's endpoint.

Each outer iteration takes exactly one scheduling sample after urgent service and before catch-up.
That sample both extends the simulation range through its `U(schedule_now)` horizon and immutably
latches `control_due`, then computes `reserved = control_due && debt_nonempty ? 1 : 0` and
`historical_limit=min(4,debt_count-reserved)` only after the nonempty guard. A reservation is only a
promise to leave the oldest remaining slot available for the primary pair; it does not remove or
separately own a slot. Deadlines crossed while historical/control work is
running belong to the next outer iteration; a post-control clock read may advance/count skipped
control ordinals and record latency, but may not accrue simulation or change the reservation. Thus a
typed result cannot make reservation arithmetic depend on a second sample.

`consumeOldest(result)` requires a nonempty debt range, advances its beginning once, and records that
ordinal's result. Every scheduling sample that extends the exclusive end increments a checked
`debt_version`; the version is unchanged by consumption. `settleAll(reason)` copies the remaining
half-open range into one receipt carrying generation, debt version, first ordinal, exclusive last
ordinal, count, and reason, then atomically sets beginning equal to end and records that version as
settled. It finalizes that debt version, never the cadence generation. A second call for the same
version is an asserted empty no-op; a later sample that opens a new nonempty range creates a new
version and resets the settlement guard. The generic tail acts only on a still-nonempty, unsettled
version. This accounts for millions of missed slots without millions of files while preserving
every slot's exact identity and preventing both duplicate drops and a false generation-wide latch.

MANUAL entry `settleAll(controller_rebase)` and disables automatic admission while control
continues. RELEASE or expiry atomically starts a new generation with
`epoch=acceptance_timestamp`, `next_control_ordinal=0`, empty/disabled automatic debt, and suppresses
automatic work for that acceptance iteration. Because urgent service precedes the iteration's sole
scheduling sample, that same acceptance iteration latches and runs immediate control-only `D(0)`
reconciliation. When that control closes, both simulation cursors are set to the resulting future
control ordinal and automatic admission is enabled for the following iteration; `D(1)` is therefore
the first possible pair when `D(0)` completes before it, and a slow reconciliation creates no
release-period backlog.
A same-generation typed no-attempt calls `settleAll(semantic_reason)`, disables automatic admission
pending control reconciliation, and never advances or cancels the already-latched control. After
that primary closes, both simulation cursors rejoin its future control ordinal. A stage transition
instead finalizes old-generation debt as `transition`, invalidates the old-stage latch, creates a
new generation at the transition timestamp with immediate replacement-stage `D(0)` control, and
keeps automatic work disabled until that control closes. The following
table is normative and is exercised against C0's fake clock:

| Domain/state | Due test | Work per outer iteration | Next deadline / missed work |
|---|---|---|---|
| control, fixed scheduler | latched from `next_control_ordinal < U(schedule_now)` | exactly one primary control transaction; never a burst | the control-only post sample advances to `U(post_control_now)`; intervening ordinals are `control_deadline_missed` and never replayed |
| automatic simulation, REALTIME | nonempty ordinal range after the same scheduling sample | reserve only when control is latched and debt is nonempty; up to four historical attempts plus one reserved primary attempt, subject to the eight-ms stop. A due control with empty debt is control-only and subtraction never underflows | consumed attempts call `consumeOldest`; semantic rebase calls `settleAll` once; otherwise the remaining range is settled once as `time_budget` or `pulse_cap` |
| automatic simulation, MANUAL | absent | zero implicit attempts | entry settles existing debt once; only STEP creates attempts; RELEASE/expiry follows the control-only reconciliation above |
| presentation, `TargetFPS=N>0`, presentable | `now >= presentation_deadline` | at most one render/swap | advance to the first rational deadline strictly greater than post-swap `now`; count and discard missed presentation ordinals |
| presentation, `TargetFPS=0`, presentable | always due; it has no deadline | one render/swap on every completed outer iteration | no presentation wait; swap/driver cost is the only pacing. A deterministic presenter that advances the fake clock 1 ms per swap produces 1,000 opportunities in one fake second, strictly more than capped 60 |
| presentation, hidden/minimized | never due; it has no deadline | zero render/swap | discard presentation phase; restoration schedules one immediate opportunity and starts a fresh rational epoch |

Drop reasons are exactly `pulse_cap`, `time_budget`, `transition`, `covered`, `paused`,
`unsupported`, `needs_control`, or `controller_rebase`; no ordinal silently survives and one ordinal
can receive only one result/reason. The next REALTIME simulation due instant is exactly
`D(next_simulation_ordinal)` and is absent in MANUAL or whenever automatic simulation is ineligible.

The fake-clock lock includes exact inverse points `U(epoch)=1`, `U(D(1)-1)=1`, `U(D(1))=2`,
`U(epoch+one_second-1)=60`, and `U(epoch+one_second)=61`; `D(1/2/3/59/60)` equals respectively
`16,666,666`, `33,333,333`, `50,000,000`, `983,333,333`, and `1,000,000,000 ns` after epoch, and
`D(60*n)=epoch+n*one_second` over 24 simulated hours. It locks the former one-nanosecond skew,
single-sample historical clock advance, a D(0) control/pulse ending at 50 ms followed by exact
ordinal-1..3 historical debt, the 5/6-slot cap matrix, time-budget settlement, empty-debt control,
same-generation no-attempt reconciliation, MANUAL entry/RELEASE/expiry, transition generation,
stale reservation invalidation, backwards-clock/overflow fatality, and TargetFPS/vsync invariance.
Named cadence-gap constants distinguish the lattice floor `16,666,666 ns` from ceiling
`16,666,667 ns`; each performance assertion states whether a shorter value is deliberate headroom.

`desktop-performance-v1` measures each due non-Speed5 fixed-REALTIME control opportunity against
its mathematical deadline, not only the duration of code inside the primary pair. At the existing
single scheduling sample it latches `{generation, epoch, control_ordinal=c, D(c), schedule_now}`.
After the full primary transaction and closing live StageCmd barrier, the existing authoritative
post-control scheduler read supplies `post_control_now`; no profiler-only clock read is added.
`completion_lag_ns = post_control_now - D(c)` therefore includes wake lateness, urgent service that
crosses the deadline, scheduler/debt bookkeeping, historical catch-up, fresh input, both control
halves, an optional non-turbo primary pulse, causal/closing barriers, and OS descheduling. Component
control/pulse/swap/readback metrics remain separately attributed but can never be subtracted from
the lag.

The executable gate runs three independently predeclared 10,000-record windows in each late-city,
realtime-battle, and turn-based-battle class—exactly 30,000 retained records per class. It evaluates
the predicates below separately for each 10,000-record window/class and again for the 30,000-record
aggregate/class; pooling cannot hide one bad run. In every evaluation, nearest-rank p99.9 must be
`<=8,000,000 ns` for both control and non-turbo pulse components, `<=15,000,000 ns` for
deadline-relative completion lag, and `<=16,666,666 ns` for urgent-ready-to-first-service. At most
0.1% of retained lag records may exceed 15 ms and at most 0.1% of urgent records may exceed
16.666666 ms; both have a `100,000,000 ns` hard ceiling. Code-attributable missed control deadlines,
forbidden drops, and non-turbo debt drops remain exactly zero in every individual run/class and in
each aggregate/class. Turbo candidate p99 and p99.9 must likewise each be `<=125%` of the matched
admitted-F-parent run/class and aggregate/class; no macOS-host absolute worst sample is treated as a
deterministic code contract.

Each raw record carries generation, epoch, ordinal, schedule/post times, primary kind, and typed
outcome. The validator recomputes `D(c)`, lag, `U(post_control_now)`, every percentile, outlier
fraction, and component bound; proves one-to-one gap-free correlation with every due-control latch;
and never trusts summaries. A transition, terminal, missing completion, backwards clock, overflow,
or omitted record inside a measurement window is logical red. An independently produced host-
health/preemption receipt may classify contamination for diagnosis, but after
`attempt-execution-began` it still selects `desktop-performance-failed` and cannot authorize a
retry. Only the common pre-execution infrastructure window may restart a complete workset. The
candidate may not self-classify contamination. Tests lock nearest-rank boundaries, 0.1% outlier
cardinality, 100-ms ceilings, deadline misses, pre-pair overhead, and the no-post-execution-retry
boundary. The old paired-duration and absolute-worst fields are rejected, not accepted as aliases.

The loop services bounded nonblocking harness/SDL work before evaluating deadlines and after every
wait. `waitOnce` is an interruptible readiness wait, not a blind sleep. Its timeout is zero when
urgent work, readable SDL/harness input, writable queued output, or an asynchronous completion is
ready. Otherwise its deadline is the minimum of the next control deadline, next eligible REALTIME
simulation instant, next capped/presentable deadline, and any F-owned monotonic lease/job deadline.
Uncapped presentation contributes zero timeout; suppressed presentation contributes no deadline.
An early I/O wake changes no phase/ordinal until the single sampling step. On return the loop re-reads the injected clock, services
bounded urgent work, and re-evaluates due tests; readiness left after the service cap forces another
zero-timeout iteration. There is no independent third oscillator. Hidden/minimized operation remains
responsive without spinning, while uncapped presentation never becomes accidentally 60-Hz capped.
Control and presentation lateness are discarded/reported; only simulation has bounded historical
catch-up. Control and automatic simulation use distinct consumption ordinals over one shared
opportunity lattice, not independently quantized clocks. Legacy pacing remains characterized
separately until E is selected.

### Scheduler, protocol, controller, and TargetFPS truth table

These are independent axes. A negative TargetFPS is normalized once to 60 and is never a separate
mode. `Framework.FixedSimulationScheduler` chooses only the executor.
`Framework.HarnessProtocol` chooses only wire grammar/capabilities. Protocol v1 supports the legacy
commands in REALTIME on either executor and can neither acquire MANUAL nor issue STEP. Protocol v2
adds MANUAL/STEP on either executor; it never silently selects fixed scheduling. Without a v2 lease,
the controller is REALTIME. `TargetFPS` chooses presentation pacing and affects simulation only in
legacy REALTIME:

| Executor | Protocol/controller | `TargetFPS>0` | `TargetFPS=0` |
|---|---|---|---|
| legacy | v1 or v2 / REALTIME | current compatibility path: one complete control transaction with one implicit pulse per paced legacy outer-loop slot, whether or not presentation is suppressed | same compatibility path uncapped; this host-speed-dependent mode is diagnostic only |
| legacy | v2 / MANUAL | each paced legacy outer-loop slot, whether or not presentation is suppressed, runs one primary control transaction with zero implicit pulses; after its closing barrier F may run one bounded STEP slice | identical ordering with uncapped presentation/control opportunities; only STEP advances simulation |
| fixed | v1 or v2 / REALTIME | 60-Hz control plus bounded automatic simulation, independent N-Hz presentation | 60-Hz control plus bounded automatic simulation, uncapped presentation |
| fixed | v2 / MANUAL | 60-Hz control, zero automatic pulses, one bounded STEP slice after a due primary barrier | the same, with uncapped presentation |

Legacy MANUAL uses C's extracted control/pulse API; it is not a second scheduler. A queued job never
runs in the socket handler. It activates after its accepted-mutation fence, then runs after the
legacy primary transaction's closing barrier with the same causal-drain/generation checks and
eight-pulse/eight-ms cap as fixed MANUAL. `needs_control` stops the slice and requires the next
primary transaction. Because its control heartbeat is still paced by the legacy outer loop, low-FPS
legacy MANUAL is a bounded protocol/control diagnostic—not a full-campaign throughput path.

Selection is orthogonal and launch-fixed: E can run fixed+v1 REALTIME before F; F keeps
legacy+v1 as the compatibility default while bundled automation explicitly selects protocol v2 and
its intended executor; G changes only the supported-desktop executor default. Protocol v2 never
implies fixed. A legacy MANUAL STEP slice runs after the closing primary barrier and before optional
render even while hidden/minimized; TargetFPS=0 with vsync off is its activation placement.

### One process-global admitted-mutation fence

E0 assigns every complete admitted line
`{connection_id, connection_epoch, request_ordinal}` before producing success or error. Ordinals are
connection-local and monotonic. Each connection has two independent cursors: its execution cursor
permits ordinal N to reach its linearization point only after every lower ordinal has reached its
own, while its reply cursor emits bytes only for the lowest ordinal whose reply is ready and whose
predecessor bytes have completely sent. An ordinary observation linearizes at its snapshot;
Screenshot linearizes at main-thread surface capture rather than asynchronous encode/write
completion; a mutation linearizes only at terminal settlement. Parse/protocol errors occupy an
ordinal and complete normally but create no mutation. Later work may therefore execute after a
Screenshot capture while its slow write still blocks later bytes on that connection. Admission may
allocate a later mutation sequence, but no side-effecting dispatch for ordinal N may begin until
every lower ordinal on that connection has linearized or been terminal-cancelled. Consecutive
mutations never share a primary transaction: each performs its own dispatch, causal/form drain,
closing StageCmd barrier, snapshot commit, and settlement. Thus `observation → mutation` snapshots
before that mutation, while `mutation → observation → mutation` settles the first mutation,
snapshots the observation, and only then begins the second.

E owns one process-global order on the single framework thread:
`last_accepted_mutation_seq`, `settled_mutation_watermark`, and one terminal-disposition slot for
every sequence above the watermark, all initially zero/empty. At the atomic classify/admit point,
every request or external-input envelope captures
`required_mutation_fence=last_accepted_mutation_seq`; a mutation then receives
`mutation_seq=++last_accepted_mutation_seq`. There is no separate same-connection, settled-action,
accepted-input, or RUN-input fence. Connection ordinals govern one connection's linearization/reply
bytes; `required_mutation_fence` is the sole causal visibility fence. This contract is named
`process_global_mutation_fence` everywhere else in the plan.

Every mutation settles exactly once as `applied`, `rejected`, `failed`, `coalesced_retired`,
`stale_generation_retired`, or `process_terminal_retired`. A primary mutation settles only after
its complete primary transaction, causal/form work, closing live StageCmd barrier, resulting stage
generation, and control/liveness snapshot commit. Split Move/resize work settles only after every
urgent and primary half applies or explicitly retires. Named urgent controller mutations may settle
out of sequence, but the watermark advances only across a contiguous terminal prefix; compacted
entries never leave a hole.

An observation may snapshot/capture only when
`settled_mutation_watermark >= required_mutation_fence`; later mutations never extend its captured
fence. When that predicate is false, the observation enters `WAIT_FENCE` at its connection execution cursor:
it consumes neither an urgent-service slot nor a zero-timeout readiness spin, and every later
ordinal on that connection remains behind it, but unrelated connections and prerequisite mutation
dispatch continue. Each contiguous watermark advance promotes every now-satisfied observation and
posts one coalesced, level-triggered `FENCE_READY` internal wake. Clearing that wake requires
draining the ready queue and then rechecking it, so an advance racing with clear cannot be lost. The
next bounded urgent window snapshots promoted observations in connection-execution order; terminal
cancellation is the only other way they leave the parked state. Screenshot advances the execution
cursor at main-thread capture, while its separately tagged
`{connection_id, connection_epoch, request_ordinal}` completion changes only reply readiness. While
ScreenshotService is OPEN, the worker first enqueues that tagged terminal completion and only then sets one coalesced,
level-triggered `SCREENSHOT_READY` wake. The framework thread drains the completion queue, clears
the wake only after the drain, and immediately rechecks the queue; a completion racing the clear
therefore leaves the wake set or is observed by the recheck. Epoch/ordinal mismatch retires the
completion and can never ready a reused connection. The completion wake and queue are independent
from `FENCE_READY`, so neither can clear the other's work.
Parked-only work does not make `waitOnce` immediately ready—the next real control deadline,
socket/event readiness, or already-posted internal completion capable of advancing its fence does.
Merely admitting a primary mutation is not a zero-timeout source.
This rule is process-global across connections. A mutation admitted from connection A is
therefore visible to a subsequently admitted observation on fresh connection B even after A
disconnects. If B's complete line is admitted first, it receives no retroactive ordering against
A's later mutation. Socket-accept time, caller wall-clock intent, and numeric fd reuse confer no
authority. Disconnect terminal-cancels only that epoch's parked observations/reply destinations and
advances its execution cursor; its accepted mutations still settle. Cancelled observations retire
at the connection execution cursor so later accepted mutations cannot strand. A reused fd receives
a new epoch and no old fence or completion. Rejected, failed, coalesced-retired, and
stale-generation-retired mutations may satisfy a fence by advancing the contiguous watermark;
process-terminal-retired instead cancels observations, so none runs after terminalization.

After loop admission opens, every process-ending runtime cause enters one idempotent atomic main-
thread terminal procedure: StageCmd/Harness/SDL QUIT, SDL window close or an external
`shutdownFramework()` request, a committed empty stack after the complete live StageCmd transaction
tail drains, `FrameLimitReached`, `SimulationPulseLimitReached`, and fatal/overflow/invalid failure.
`TerminalReason` is a closed typed enum carrying success/failure and reply policy. A natural fatal,
QUIT, or final empty-stack reason discovered at the same closing barrier wins over a configured
limit; otherwise first accepted reason wins and repeats are idempotent. Pre-loop initialization
failure retains B's separate typed exit. `shutdownFramework()` becomes only a terminal-request
wrapper and never directly clears the stack or sets `quitProgram` while scheduler state is live.
`StageStack::clear()` remains a stack primitive: REPLACEALL and post-loop destruction do not
terminalize, and POP-to-empty followed by a command-tail PUSH continues; only still-empty committed
state after the live transaction drains is terminal.
SDL window close retains its current pre-update ordering, but the main-thread `processEvents()` path
must synchronously complete the full retirement procedure before returning to the loop; it cannot
set a flag and escape around cursor, mutation, ScreenshotService, or reply retirement.

The terminal procedure first closes admission and all dispatch lanes. The mutation or transaction
that discovered terminal receives its natural `applied` or `failed` disposition; every other
accepted mutation without a disposition receives `process_terminal_retired` exactly once, including
all derived halves. Every unlinearized observation is terminal-cancelled before fence-ready work can
run. One process-owned, dedicated single-worker `ScreenshotService` serves both fenced Harness
Screenshot and SDL PRINTSCREEN. Its exact capacity is 16 total active, queued, or reserved-but-not-
yet-enqueued jobs. At the eligible main-thread execution barrier, a source reserves a service slot
under the completion gate **before** surface readback, destination ordinal allocation, or filename
publication; only then does it capture the last completed surface into an owned CPU image.
PRINTSCREEN captures at its ordered primary dispatch and has no connection/reply token. Each job owns
`shared_ptr<ScreenshotServiceState>`, captured bytes, stable
encoder/writer, destination policy, global capture sequence, per-canonical-destination publication
ordinal, cancellation epoch, and optional Harness reply token. It owns no `Framework*`, `Data*`,
renderer, Client/fd, global `fw()`, or borrowed callback dependency.

If all 16 slots are reserved, Harness Screenshot performs no readback or enqueue and completes its
ordinal through the existing ordered collapsed Screenshot error path; SDL PRINTSCREEN performs no
readback, logs typed `screenshot_queue_full`, drops that input, and releases any provisional auto-
name reservation. A no-surface/readback failure likewise releases its slot and terminally retires
its destination ordinal before a later job may publish. Terminal close racing a reservation cancels
that slot under the same gate; the submitter then performs no enqueue, wake, publication, or global
access. Tests cover 16/17 for each source and mixed-source saturation.

The service lifecycle is `OPEN -> TERMINAL_CLOSED -> JOINING -> DESTROYED`. Completion enqueue,
publication, and terminal close share one short gate. In OPEN, a worker may enqueue its tagged
completion and only then arm the level-triggered `SCREENSHOT_READY` wake. Terminal takes the gate,
changes to TERMINAL_CLOSED, increments the cancellation epoch, closes completion admission, drains
already-enqueued completions as reply-retired without callbacks, disarms the wake, and cancels every
unpublished job. A worker reaching completion later performs only job-local temp cleanup and returns:
it may not enqueue, wake, touch connection state, invoke a callback, or access Framework globals. If
publication won before terminal, the final is preserved and only the reply retires; if terminal won
first, the previous final remains unchanged and the temp is removed. Terminal may contend only with
the bounded metadata gate, never encoding or chunked file I/O.

Publication is policy-aware and compatibility-preserving. Harness explicit paths use
`ReplaceExisting`: encode and flush a unique no-clobber same-directory temp, then atomically replace
the destination in capture/publication order, preserving the previous bytes on failure or
cancellation. SDL PRINTSCREEN uses `CreateNew`: main-thread naming reserves a path absent both on
disk and from pending service destinations, so consecutive slow requests receive distinct names;
an external create race fails rather than overwrites. The single worker plus monotonic destination
ordinals means an earlier capture can never overwrite a later final. No-replace applies to temps,
claims, PRINTSCREEN names, and immutable coordinator evidence—not to the legacy Harness destination.

Before the atomic scheduler terminal procedure returns, each connection advances its execution
cursor through the highest admitted ordinal, abandons every nonterminal unsent reply suffix,
advances its reply cursor through the same ordinal, and closes both cursors/socket. Harness QUIT
preserves the existing `OK quitting` only as the sole terminal-reply exception: after lower replies
are abandoned and only when no predecessor line was partially emitted, one bounded nonblocking
flush attempts the complete `OK quitting\n` buffer. `EINTR` retries within that same budget; a full
write closes successfully; zero bytes, would-block, or an error closes with no line; a positive
short write closes immediately with only that prefix and never queues the suffix or another line.
Clients treat EOF before newline as no reply. If a predecessor was partial, close without attempting
QUIT bytes. Other terminal reasons emit no synthetic reply. Only after cursor retirement, any
allowed QUIT flush, socket closure, and screenshot cancellation publication are complete may the
atomic procedure return; it never waits for screenshot I/O.

The exiting path inside `Framework::run()` then enters JOINING with no further observation,
mutation, control, simulation, presentation, socket reply, or ordinary completion callback. It
cancels and joins the dedicated screenshot worker, cleans idempotent normal residue, and seals the
runtime-image receipt while data, renderer, encoder/writer, and wake state are still alive; only then
may `Framework::run()` return and later UI/resource destruction begin. The join waits for worker
encode/chunk cancellation only, never for a main-thread completion drain and never after a worker
pool stop. Excess join time beyond the validation process-drain deadline makes the cell fail. A
crash, SIGKILL, or power loss can leave a uniquely named temp and the design does not claim otherwise.
Such a file is never a successful Screenshot. Validation confines destination/temp paths to the
cell-owned mutable root, which the coordinator may remove only after exact PID/start identity proves
death; normal application mode never scans or deletes arbitrary user paths. Tests cover terminal
before enqueue, between enqueue/wake/drain, during encode/temp/publish, PRINTSCREEN followed by every
terminal reason, same-path and auto-name ordering, renderer/data/wake lifetime sentinels, N/N+1
queue bounds, scheduler-terminal latency, normal teardown cleanup, honest abnormal-exit residue,
and both internal-wake drain/clear races. The terminal barrier never permits later work; in-run
teardown owns only cancellation, join, cleanup, and the per-process realized runtime-image seal.

The exhaustive classification is:

| Commands/events | Mutation? | Interrupts queued/active RUN after RUN? | Linearization/settlement |
|---|---:|---|---|
| `BuildInfo`; `Lifecycle <cursor>`; `Status`; `UiDump`; `HELP`; `CONTROLS`; Forms-parsed terminal `CONTROL <id> (item <N>)* get`; STEP `HELLO`/`CURRENT`/`STATUS` | no | no | fenced observation; Lifecycle snapshots outside a journal mutation; BuildInfo bytes remain invariant |
| Harness `Screenshot` | no | no | wait for captured fence, capture last completed surface on main thread, encode/write asynchronously; reply ready only after file result |
| SDL `PRINTSCREEN` | no | no | capture last completed surface at ordered primary event barrier; never wait for future render |
| every `Query`/`GS` spelling and `Save` | yes | yes | primary FIFO with accepted generation/session resolution; success, legacy error, stale stage, and query/save failure all settle because this opaque namespace contains mutations |
| `CONTROL ... click`/`toggle`/`set <value...>` or omitted op; registered mutating Action | yes | yes | Forms alone parses/classifies the full item chain; `set get` sets literal `get`, omitted op clicks; missing target or rejected value still settles |
| unknown/malformed/oversized/unregistered Action or command | no | no | reject before mutation admission; ordered error only; a future action must add an explicit read/mutate/interrupt row |
| Harness `Move`; SDL mouse motion | yes | no | one sequence covers urgent cursor position plus coalescible primary stage event; settle after stage half applies/retires/stales |
| Harness/SDL resize, minimize, restore, visibility | yes | no | urgent presentation/control mutation plus any generation-tagged stage half; settle after every required half commits or retires |
| Harness `Down`/`Up`/`Click`/`Scroll`/`Key`/`KeyDown`/`KeyUp`/`Text`; SDL button/keyboard/controller/text | yes | yes | generation-tagged fresh primary FIFO; multi-event command settles after every derived event applies/retires |
| STEP `ACQUIRE`/`RENEW`/`CANCEL`/`RELEASE`; lease expiry | yes | controller table: authorized matching `CANCEL`, authorized `RELEASE`, and expiry cancel; `ACQUIRE`, `RENEW`, replay, and every semantic rejection do not | urgent controller mutation; exact protocol replay is the same logical mutation and allocates no new sequence; semantic rejection settles |
| STEP `RUN` | yes | no; a later RUN returns `job_busy` | install/reject queued job urgently and settle RUN's own mutation; pulses never execute in the socket handler |
| Harness/StageCmd/SDL `QUIT`; window close/external `shutdownFramework`; committed final empty stack; presentation/pulse limit; fatal/overflow/invalid | terminal escape | terminalizes | may overtake only through the typed atomic terminal procedure; every suppressed accepted mutation becomes `process_terminal_retired`; no later observation/pulse/render executes |

Automatic pulses and causal events inside an owned transaction allocate no external sequence; they
remain inside that transaction's settlement. RUN stores the
`required_mutation_fence` captured immediately before its own sequence plus accepted stage
generation. Its sequence settles when enqueue/reply state is installed, not when the job ends. The
job activates only when the process-global watermark reaches the captured fence and generation
still matches. Any later non-controller external mutation marked interrupting and having
`mutation_seq > run.mutation_seq` terminates queued/active work before another pulse, then dispatches
normally; observations do not interrupt. Controller mutations use their explicit table.

E0 owns connection IDs/epochs, request ordinals, execution/reply cursors, bounded transport/storage,
disconnect/fd-reuse handling, and immutable optional sequence/fence fields; legacy stays immediate
and byte-identical. E owns exhaustive Framework/Forms classification, process-global sequencing and
settlement, primary mutation FIFO, cross-connection observation gating, and asynchronous Screenshot.
F adds STEP classes to the same sequence, replay identity, controller overtake rules, RUN settlement,
activation, and interruption—never a second watermark.

### Integration precedence

#### Immutable implementation base

The plan does not use an ambiguous bare `develop`:

| Name | Value |
|---|---|
| `BASE_REMOTE` / PR repository | `origin` / `khallmark/OpenApoc` |
| `BASE_BRANCH` / PR target | `develop` |
| Remote target before reconciliation | `78319df59e0a9f307e206bd192133075fc748cf8` |
| Evolved local source | `4a8e01acc0dc07ebc11155bee659ce633e9899a3` |
| Reconciliation branch | `khallmark/timing-base-reconciliation` |
| Reconciled PR | `khallmark/OpenApoc#5` |
| Immutable reconciled `BASE_SHA` | `11ca3c08fa5bab5ac5f9871968990bd8fbe7ad00` |
| Reconciled tree | `c6517311a9d4d16f68478ad06c982db5708f15e6` |

The reconciliation merge preserves local `develop` at every overlapping conflict, retains only the
remote-only six-file main-menu skirmish change, and completes the icon-pipeline wiring already
partially present on local `develop` so the base builds cleanly. PR Z targets
`origin/develop@78319df5`; R0, A, and B branch independently from the immutable Z head. Until Z
lands, their GitHub base is the Z branch. After Z lands, they retarget to `develop` without rewriting
their reviewed commits.

This train requires merge commits so reviewed heads remain ancestors of the landed result. If a
maintainer instead squash-merges or rebases a parent, every descendant is rebuilt with
`rebase --onto`, checked with `git range-diff`, retested, re-reviewed when material, and treated as a
new integration/robot input. No receipt follows a changed head or tree.

This branch starts from the fork's current `develop`. The reviewed three-clock architecture in this
document is authoritative. Other timing work is an evidence and completeness corpus, not merge
authority:

1. `feat/vanilla-time-base@1fa0b82a` contributes recovered constants and multiplier-safety tests.
   `bc25e7f6` is exclusively a T-series failure-case and test corpus for chronological large
   advances. None of its category-at-final-clock handler loops are ported into the core scheduler
   or another PR.
2. Upstream PR #1270 contributes the separation questions, overload/starvation/exit cases, and
   render-blocking risks. Its SDL timer threads and dropping `locked_queue` are rejected.
3. Upstream PR #1166 contributes the real-time-versus-simulation-time distinction and its complete
   list of suspect timing sites. Its 36-Hz `Stage::update()` loop and relocation of gameplay time
   constants into `framework.h` are rejected.
4. Upstream PR #1237 and linked #997/#1216 contribute the 180-TPS hypothesis and combat-parity
   acceptance matrix. Its raw multiplier flip and `TICK_SCALE` denominator changed from the locked
   unit factor 4 to the new multiplier 5 are rejected; movement corrections belong in the separate
   evidence-first movement slice, never in this unit conversion. Eyeballed combat constants remain
   rejected unless independent evidence and red-first tests establish them.
5. `harness/socket-infrastructure` contributes provenance for the loopback protocol. The evolved
   harness already present on `develop` wins; only missing bounded-work and STEP behavior is added.

Every source item appears in `docs/timing/source-disposition.md` with its provenance and final
decision. No source branch or upstream PR is merged wholesale.

### Pull-request delivery map

The integration worktree is a planning and final-composition surface, not one giant PR. Delivery is
split into independently reviewable branches:

| PR | Base | Scope | Dependency |
|---|---|---|---|
| Z — base reconciliation | `origin/develop@78319df5` | Reconcile the complete local line, retain the remote-only skirmish entry, and complete the already-partial icon pipeline | none |
| P0 — planning-scope trust root | `Z@11ca3c08` | Base-owned, fail-closed `pull_request_target` admission workflow; read-only, full-SHA-checkout hardening for the existing CMake, lint, and harness workflows; canonical `pr-p-policy-v1` containing P's exact identity and 22-path/status/mode/type/schema/SHA-256 map; and the validator that inspects the untrusted P head only as bounded Git-object data. It freezes the complete Actions workflow tree, runs no head bytes, and cannot validate itself | Z; separate exact-head human and independent review plus explicit merge authorization. After landing: install/read back a zero-bypass create-only tag ruleset; prove both fixed refs absent; atomically create direct annotated trust+receipt tags; bind their object IDs/peeled P0 commit/tree/provider receipt; run a sacrificial exact-head check; then install/read back strict expected-source protection before P opens |
| P — design and source ledger | landed `develop` containing the exact reviewed P0 trust root | One exact commit adding exactly 22 planning artifacts: this plan; committed canonical GitHub source snapshots plus deterministic capture/verification tooling; executable body/comment/file/hunk→disposition map for #1166/#1237/#1270; supplied 41-site audit mapping plus plan-discovered sites; immutable `train_scope_v1`; and defense-in-depth structural tests for the closed 37-slice DAG, proof partitions, attempt kinds, complete turn-based/save→clean-exit→fresh-process-reload receipts, and 36-54-3-20-20-100-plus-soak robot gates. The base-owned P0 policy—not P's manifest or validator—proves the exact diff and that no runtime/build or trust-root implementation entered P; one-commit history forbids transient unreviewed artifact bytes | P0 landed; sacrificial head-binding and branch-protection receipts current; exact P bytes match P0 policy; must land before R1a is created |
| R0 — robot result truth | `Z@11ca3c08` | PR #7 at `56b167b7`; make legacy automation report reached/timeout/transition/game-over distinctly, require clean process/evidence commits, validate exact workloads and forces, and fail runs nonzero | Z; must merge before robot evidence is trusted |
| R1a — engine admission observability | landed `develop` containing P+B | Add process-invariant launch-captured `BuildInfo` and process-scoped monotonic `Lifecycle <cursor>` harness queries; replace the signed-int RNG-seed option path with a checked full-domain `uint32` path while preserving `0 = SeedRng`; generate/query exact build, process-instance, normalized timing/canonical Data+CD identity, and capability ranges; add default-compatible `Framework.CDPrompt` with a validation fail-closed mode; route every game-Framework config persistence path through `Config.Save`; add a generation/transaction-keyed lifecycle journal with explicit overflow invalidation and no gameplay behavior change; add focused `test_admission_observability` coverage | approved/landed P and B; required by R1b |
| R1v — victory-recipe tooling | landed `develop` containing R0 | Repair the observation-only `Victory.record()` path; freeze the five named whole-game recipes, deterministic seed derivation, the exact cadence-free `campaign_acceptance_projection.v1`, expectation-free `milestone_schema_v1`, exact-three-replicate `oracle-calibration-v1`, closed ten-key/100-row feasible legacy/fixed REALTIME/MANUAL activation topology, and the closed compatibility-delta component-policy schema for both campaign projections and active replay expectations; prove the repaired driver changes no engine/game state | R0; independent of R1a and contains no oracle expectations or admission code |
| R1c — executable campaign controls | landed `develop` containing R1a+R1v | Tools-only implementation of recipe-selected realtime/turn-based control state machines, a fresh-process continuation request contract, and closed raw TB/save-reload receipt schemas shared by every later whole-game gate. It requests one planned CityView save → acknowledged clean exit → immutable relaunch → continued campaign victory; R1p later owns the process boundary and continuation closure. Realtime substitution, recovery restart, in-process load, forced exit, mutable-binary fallback, copied progress, or normalized coverage without same-campaign raw-receipt bijection cannot satisfy the schema. Add socket-free transition tests plus one bounded non-admissible real-game probe. No C++, subprocess launch/materialization/realization implementation, or oracle expectations | R1a+R1v; required by R1p and R1o; any later executable/schema change invalidates R1p-produced evidence and restarts R1o |
| R1p — oracle producer | landed `develop` containing P+A+B+R0+R1a+R1v+R1c | Land and independently review the standalone exact-build `runtime_execution_closure_v1` family: process-invariant `runtime_launch_template_v1`; discriminated `process_launch_plan_authority_v1` binding either a finite exact `planned_process_launch_set_v1` or immutable soak `planned_process_launch_prefix_policy_v1`; attempt-local `soak_execution_window_v1`; separate soak campaign-root and process-launch prefixes; no-clobber `materialized_process_launch_v1`; per-process realization plus exact finite-set/dual-prefix `realized_runtime_closure_receipt_v1`; own subprocess/PID/port/output reservation, orderly wait/reap and relaunch, attempt-local `continuation-input-closure-v1`, and common pre-ledger attempt/fence authority with their schemas/tests; no oracle expectations, baseline receipts, candidate, or admission code | A, R1a, R1v, and R1c; must land before any oracle-producing run |
| R1o — pre-candidate victory oracle freeze | landed `develop` containing R1p | Evidence-only slice: materialize 18 immutable R1v exact-oracle key groups (`oracle-v1` all five, `merge-3` first three, and `activation-a`/`activation-b` all five). First bind and exhaust a canonical 18-key × `{legacy-240,legacy-1000}` execution-cell manifest—36 complete fresh-process victories with each key's two projections byte-identical. Then bind and exhaust a distinct canonical 18-key × replica `{0,1,2}` manifest at legacy/TargetFPS=1000/vsync-off—54 more complete victories—against the exact landed R1p parent using R1p's unchanged collector. Every manifest row bijects its launch, terminal, projection, and same-campaign raw TB/save-reload evidence. Require byte-identical acceptance projections within each key and publish/review seed-keyed `victory_oracle_v1` plus all content-addressed receipts. Low-FPS legacy whole-game runs are not required. There is no prose/a-priori substitute. Only `docs/timing/baselines/**` manifest/digest references may change | R1p; any executable/tool/config/recipe/schema change or unstable/missing/duplicate/remapped cell invalidates the applicable complete 36- or 54-run attempt and returns to R1v/R1c/R1p; must land before an R1b branch exists |
| R1b — robot admission coordinator | landed `develop` containing R1o | Consume the landed R1p closure collector, R1o oracle, R1v recipes/topology/component policy, and P's typed ordinary-admission contract; derive tier materialization without caller input; add manifest-bound batches, ordered lifecycle assertions, port leases, transcript bundles, immutable epoch refs, admission/delta ledger, and soak coordination without an expectation-write path | R1o; its exact reviewed head is the source head of the one-time two-parent genesis integration candidate, which must earn the first admission before C0 |
| A — vanilla time base | `Z@11ca3c08` | PR #6 at exact head `44840ff1`; retain the observational 36-TPS base with recovered cadence-ratio corroboration, derived constants, red-first multiplier canaries, and dead duplicate removal | Z |
| B — StageCmd transaction | `Z@11ca3c08` | PR #8 at `ff20bfee`; restore bounded live-FIFO lifecycle command semantics, production transaction/process truth, and exact regression locks | Z |
| C0 — monotonic clock/pacer seam | landed `develop` containing P+R1b | Add one injectable monotonic clock, pure production `LegacyFramePacer`, production/fake `LoopWaiter`, and exact prospective clock allowlist while preserving cadence | P plus R1b's exact admission, landing reconciliation, and blocking `integrated-repeat-20-v1`; required by C1/C/D/E |
| C1 — production timing policy substrate | landed `develop` containing C0 | Add the dedicated Forms/GameUI test target plus production-owned timing value types and side-effect-free City/Battle/Video policies, test renderer/null audio, programmatic fixtures, and no shadow test models or proprietary data | C0; production-policy link/run proof must land before C or D1 |
| C — parity-preserving simulation capability | landed `develop` containing C1 | Add `SimulationStage`; wire City/Battle runtime adapters to C1's exact types/policies and split simulation, event provenance, control epoch, and control under the legacy frame executor while preserving both active effective replay and campaign oracles byte-for-byte | C1; required by C2 |
| C2 — intentional compatibility deltas | landed `develop` containing C | Own only the oracle-observable portions of CD-03, CD-12, and CD-16. Before its candidate exists, review/seal its delta-only source head, execute three replicas of every active replay cell on both heads plus the paired 54+54 campaign reference, prove every unowned component equal on both surfaces, and obtain a single-use authorization that materializes candidate-private prospective campaign/replay chains. The exact candidate then reruns all ordinary gates against those prospective chains; admission atomically promotes both or promotes neither | C plus activated `control-epoch-v2` and pre-candidate authorization; required by D0/D1 and all later descendants |
| D0 — shared TileView presentation | landed `develop` containing C2 | Own `tileview.{h,cpp}` exclusively: add the shared animation-duration/phase API and elapsed-time viewport motion using C0's clock while retaining the exact legacy `const int` names/values as source-compatible shims until both independent consumers migrate | C2; required by D2/D3/G |
| D1 — VideoScreen render purity | landed `develop` containing C2 | Move video progression/end detection into wall-time control, preserve the last decoded frame until transition, and make render side-effect free | C2; lands before D5 |
| D2 — city presentation clocks | landed `develop` containing D0 | City selection/portal animation, border blink, and palette pulsation; no shared TileView files | D0; required by G |
| D3 — coherent battle presentation clocks | landed `develop` containing D0 | Atomically own `battletileview.{h,cpp}` and `battleview.{h,cpp}`: all render counters, hidden-bar/palette/fire audio, pause icon, hover/attack, throw, and impossible-action timing | D0; one cross-file state migration, required by G |
| D5 — form visibility render purity | landed `develop` containing D1+D3 | Remove action-registry mutation from render; preserve the lifetime registry; bind action registration to C's control epoch; explicitly register BattleTileView's render-only `hiddenForm` tree without updating it | D1 and landed D3; required by E |
| E0 — harness dispatch substrate | landed `develop` containing D3 | Stable connection epochs/request ordinals, independent local execution/reply cursors, bounded nonblocking output, immutable optional mutation-fence fields, deferred handles, and generation-checked live-stage query dispatch replacing the global raw-callback chain while legacy execution remains immediate | D3; required by E/F; independent of D5 |
| E — fixed scheduler | landed `develop` containing E0+D5 | Rational fixed scheduler, exhaustive mutation classification, process-global fence/watermark, deferred primary execution, asynchronous screenshot completion, TargetFPS/vsync/minimize behavior, default off | E0 and D5 are ancestors of one concrete landed branch point |
| F — STEP automation | landed `develop` containing E | Retry-safe leases, bounded jobs on E's same global mutation watermark, timing introspection, typed Python migration | E |
| G — supported-desktop activation | landed `develop` after F, D0, D2, D3, and D5 have landed | Supported-desktop default flip, explicit unsupported-unmeasured platform map, documentation, remeasured STEP capacity, candidate-independent `desktop-presentation-v1` against the admitted-F-frozen support-map digest, bounded-percentile performance receipts, exact feasible ten-profile-per-key activation-100, and uninterrupted 24-hour soak evidence bound to both effective oracle chains | starts only from that concrete landed develop head after `manual-step-v3` activates; all four machine-readable G gates must bind the exact candidate and support-map digest; never uses a synthetic multi-parent PR base |
| P-movement — battle movement parity | landed `develop` containing A+R1b | Walking/direct-flight distance and travel cadence only | A plus R1b's admitted/landed/repeat fence, original evidence, and deterministic travel fixtures |
| P-projectile — trajectory/collision parity | landed `develop` containing A+R1b | City/battle projectile integration, collision, and impact placement without ROF changes | A plus R1b's admitted/landed/repeat fence, original evidence, and position/collision fixtures |
| P-ROF — weapon cadence parity | landed `develop` containing A+R1b | Fire-event cadence independently of projectile velocity/animation; explicitly research/reject #1237 comment 1848612767's raw divisor `4 → 5` unless original-game fire-event evidence supports the resulting interval | A plus R1b's admitted/landed/repeat fence, original fire-event cadence evidence, and TargetFPS 30/60/240 plus multiplier-4/5 event-timeline fixtures |
| P-explosion — explosion/propagation parity | landed `develop` containing A+R1b | Expansion, visible radius, damage radius/falloff, propagation, and instant-damage option behavior | A plus R1b's admitted/landed/repeat fence, original evidence, and tile/timestamp fixtures |
| P-fire — vanilla fire scheduler locks | landed `develop` containing A+R1b | Fire-row/contact helpers and the independent turn-based 400-iteration characterization | A plus R1b's admitted/landed/repeat fence and exact vanilla-domain fixtures |
| P-MAC — Megapol Auto Cannon parity | landed `develop` containing A+R1b | Starting ammunition and controlled target behavior, preserving data/calculation provenance boundaries | A plus R1b's admitted/landed/repeat fence, original/extracted evidence, and new-game/target fixtures |
| P-physics — falling/jumping/item integration | landed `develop` containing A+R1b | Time-normalized acceleration/integration with collision-path parity | A plus R1b's admitted/landed/repeat fence, original evidence, and partition-invariant trajectory fixtures |
| P-smoke — residue parity | landed `develop` containing A+R1b+P-explosion evidence fixtures | Separate smoke-grenade, explosive-residue type/lifetime, and secondary-hazard behavior with original-evidence locks | A, R1b's admitted/landed/repeat fence, and P-explosion fixture substrate; required by T-180 |
| P-save — tick-resolution save policy | landed `develop` containing A+R1b | Define persisted tick-domain/version metadata, multiplier mismatch behavior, migration/rejection policy, and old/current roundtrip fixtures | A plus R1b's admitted/landed/repeat fence; required by T-180 before any multiplier-5 save is admitted |
| T-calendar — chronological tick hardening | landed `develop` containing A+R1b | Calendar/fuel boundary ordering and partition invariance | A plus R1b's admitted/landed/repeat fence; separate from core |
| T-animation — animation tick hardening | landed `develop` containing A+R1b | Individually evidenced animation/tick-unit scaling without presentation-clock work | A plus R1b's admitted/landed/repeat fence; separate from core |
| T-180 — multiplier experiment | landed `develop` after G, A, P-movement, P-projectile, P-ROF, P-explosion, P-fire, P-MAC, P-smoke, P-physics, P-save, T-calendar, and T-animation have landed | Experimental multiplier 5 differential only | every constants/collision/save/combat/platform lock named by R1237-02 is an ancestor of one concrete landed develop head; never implied by another PR |

Z establishes the only reconciliation base. P0, R0, A, and B review independently on Z. P follows
only after the exact P0 trust root lands and its check-source/head-binding protection is proven; R1v follows
landed R0, R1a follows one landed develop head containing P+B, R1c follows the landed convergence of
R1a and R1v, R1p follows a landed head containing A plus R1c's full ancestry, and R1o freezes evidence from that exact landed R1p parent without changing
executable behavior, and R1b follows landed R1o without a synthetic multi-parent PR base.
B does not depend on R0; R1a requires
P+B, and C0 waits for R1b's first
admissible exact-head receipt. C1
then proves the UI test link graph before C can rely on those tests. C preserves the active oracle;
C2 follows only after its delta-only source head earns a pre-candidate compatibility authorization,
and every later runtime slice descends from admitted C2. D0/D2 form the city
presentation line; D0/D3 is the atomic battle-presentation line; D1 and landed D3 converge at D5.
E0 follows landed D3 because it replaces BattleView's handler chain, but it does not depend on D5;
E begins only after both E0 and D5 are ancestors of one landed head. Each child is created only after
its named parent is actually landed on `develop`; F follows E, and G starts only after all
named prerequisites are ancestors of one concrete landed `develop` head. No PR has a synthetic
multi-parent Git base. P-series, T-calendar, and T-animation branch only from one landed head
containing A+R1b after R1b's exact-landing repeat is green. T-180 alone waits
for the exact named P-smoke/P-save and other locks in its table row. These tracks never block E unless they
discover a default-multiplier-4 regression in the exact pulse matrix. Every PR carries its own
red/green tests, rollback statement, and automated review for its material head.

Landing alone never releases the next runtime child. After R1b and every later runtime slice in the
machine scope—including G, every P-series parity slice, both timing-hardening slices, and T-180—is
admitted and reconciled to one exact landed `develop` SHA/tree, R1b must run and append one green
`integrated-repeat-20-v1` against that exact accumulated head before any descendant runtime source
branch or any later runtime candidate/landing exists. Sibling branches may be cut only after their
common predecessor's repeat is green, but a global landing lock permits no runtime landing while
another landing/reconciliation/repeat is open. All twenty runs are fresh-process, whole-game
victories; no smoke, candidate, prior-head, diagnostic, or partial run substitutes. A failure
revokes that head and descendants and blocks ordinary progress; only its ledger-derived remediation
recovery may proceed. A completed repeat remains proof for its immutable landing even after later
heads land. Completion requires one such proof for the digested set of every landing that authorized
later non-recovery progress plus the final live landing—not a vague “current green” flag.
`nightly_schedule_v1` independently keeps
running 20-campaign batches on the current live head until a terminal ledger state.

Owner aliases in the source ledger are normative: `R1` means the combined R1v+R1a+R1c+R1p+R1o+R1b admission
oracle; bare `P` means design/evidence governance in PR P; bare `D` means the concrete
D0/D1/D2/D3/D5 slice named by the anchored file; bare `T` means the concrete
T-calendar, T-animation, or T-180 slice named by the acceptance test; and `Integration` means the
sealed evidence-gated epoch-ref sequence, not a mergeable runtime PR. Every `P-*` owner resolves to the
explicit delivery row above. `Integration` means the sequence of sealed evidence-gated epoch refs,
never a mutable mergeable runtime PR.

`D4` is intentionally retired: its former battle-audio half was collapsed into atomic D3 after
review proved the state crosses BattleTileView/BattleView. The number is not reused so old review
notes and receipt manifests cannot be misread as naming a current slice.

P freezes `docs/timing/train-scope-v1.json` as the machine-readable closed 37-slice set and dependency DAG
for this train. R1b genesis binds its JCS SHA-256, and every candidate/admission records the active
scope digest. A caller cannot define “scoped”: `train-completed` derives required landed source
heads and reconciliation receipts only from that registry. A newly discovered in-scope timing or
combat slice may be added before its candidate exists only through a separately reviewed
`train-scope-extended` append that names the prior digest and a strict superset registry; removal,
renaming, dependency weakening, or completion-rule weakening is forbidden. The active scope is the
latest valid extension chain, so the initial file is immutable without pretending discovery stops.

`train_scope_v1.contracts.ordinary_admission` is the normative ordinary-admission specification;
its digest is `SHA256(JCS(/contracts/ordinary_admission))`. Prose tier names are explanatory only.
The initial contract requires exactly `cold-lifecycle`, `active-schema-replay`, and `merge-3-v1`.
A reviewed scope extension may add a tier only as a strict contract superset: every prior tier ID,
definition, resolver, predicate, and binding surface remains byte-identical. Removal, replacement,
aliasing, or semantic weakening is invalid and makes every older open candidate/intent stale.
Activation topology and oracle calibration are produced by R1v, but their authority, exact
cardinality, extension validator, and ledger binding fields are declared in the same scope file.
The same file normatively declares eight executable contracts: oracle calibration, activation
topology, replay-schema upgrade, the common fenced-batch attempt protocol, gap-free nightly
scheduling, ordinary admission, exact-landing successor release, and dual-surface compatibility
delta. The plan prose explains those contracts;
it cannot broaden, weaken, or substitute for their validator-owned predicates.

Readiness is per track, not global:

| Track | Readiness |
|---|---|
| Z | frozen at `11ca3c08`; local gates, all CI, and exact-head automated review green; awaiting human merge decision |
| P0 | requirements-only bootstrap until its workflow, validator, canonical P policy, hardened CMake/lint/harness workflows, and complete workflow-tree freeze receive separate exact-current human and independent hostile review. It cannot use its own gate. After explicit merge authorization and exact landing, first install/read back the no-bypass tag ruleset, prove both refs absent, and atomically create the direct annotated trust and canonical receipt tags. The external append-only bootstrap receipt binds both remote tag-object OIDs, the direct peeled P0 commit/tree, actor/provider push receipt, exact trust-root blobs, complete workflow tree, and ruleset. Only then may a sacrificial PR prove the native required check attaches to the event head; strict/up-to-date expected-source protection is subsequently configured, read back, receipt-bound, and revalidated before P admission. Any crash, mismatch, preexisting/moved/deleted/nested/lightweight tag, context collision, skipped/neutral check, or protection bypass is hard red and never authorizes tag recreation |
| P | requirements-only until three internal adversarial passes, Cursor Grok 4.6 xhigh fast, and cold Claude Opus max all return **IMPLEMENT** on the exact one-commit, 22-artifact head. It remains additionally blocked until P0 lands, its sacrificial check and protection receipts are green/current, the base-owned validator accepts the exact event head, and a separate explicit merge authorization is given |
| R0 | frozen at signed head `56b167b7` / tree `f8027d6f` on Z; every finite Skirmish mode requires clean typed shutdown, adversarial battle/generation ledgers are durable before score/evolution/success with rollback and unwritable-path tests, all local gates and CI 6/6 are green, and independent exact-head review returned **FREEZE** |
| R1a | newly required after P+B: engine-generated build identity, effective-config introspection, and a generation/transaction lifecycle journal must make exact execution observable without changing simulation |
| R1v | newly required after R0: the broken victory recorder, five complete immutable recipes, exact-three oracle calibration, activation topology, and compatibility component policy must be fixed/frozen without oracle expectations |
| R1c | newly required after R1a+R1v: deterministic typed turn-based victory control and planned save/fresh-process/reload continuity must make both mandatory recipe obligations executable rather than inferred from recovery |
| R1p | newly required after A plus R1c's ancestry: the exact-build/launch closure collector, process/relaunch ownership, continuation-input closure, and pre-ledger common attempt/fence authority must be independently reviewed and landed before they can produce oracle evidence |
| R1o | newly required after R1p: all 18 keys must first pass the fenced 36-campaign 240/1000 stability probe, then each win three fresh-process calibration replicas (54 more) with stable retained projections; the independently reviewed evidence-only oracle manifest/receipts must land before any R1b branch exists |
| R1b | newly required after R1o: frozen truth/oracle semantics remain necessary but insufficient because fresh exact-head binding, immutable epoch refs, manifests, admission ledger, and batch/soak orchestration are not yet executable; no whole-game receipt is admissible until R1b is reviewed and its exact head is green |
| A | frozen at signed head `44840ff1` / tree `f317287c` on Z; both evidence-language blockers are closed, focused/full local gates and CI 6/6 are green, and independent exact-head review returned **FREEZE** |
| B | frozen at signed head `ff20bfee` / tree `6e42295c` on Z; production transaction/process seams and resume-POP, empty-tail recovery, initial ordering, terminal no-render, exit-status, and 64/65 locks are green in 39/39 local CTest and CI 6/6, and independent exact-head review returned **FREEZE** |
| C0, C1, C, C2, D0, D1, D5, E0, E | specified below but not implementation-authorized until P is landed; afterward they remain dependency-blocked until their named parents and the first R1b admission gate land/pass; C2 additionally requires its single-use pre-candidate delta authorization |
| D2–D3 | slice-ready only after their exact shared/control dependency lands; D3 deliberately owns the complete cross-file battle timing state atomically |
| F | specified below but not implementation-authorized until P is landed; afterward blocked until E and binary/client compatibility fixtures exist |
| G | evidence-blocked by the campaign matrix, numeric desktop performance gates, and soak; mobile remains explicitly unsupported/unmeasured and legacy-default until a separate receipt |
| P/T | research/design; each item needs original evidence and a red test before implementation |

### Integrated robot-validation branch

The present documentation branch is `khallmark/render-simulation-design`; it is never an
integration epoch. There is no mutable “current integration branch.” The R1b coordinator allocates
one monotonically increasing epoch ordinal under the admission-ledger lock. Before appending any
candidate transition or publishing any ref, it verifies the remote ruleset that covers the exact
epoch-ref pattern permits creation but denies update, deletion, force-push, and actor bypass; a
missing, mutable, or bypassable rule blocks before state changes. It then creates exactly one
Git ref named `refs/heads/khallmark/render-simulation-integration-eNNNNNN` through `gitw`, where
`NNNNNN` is the zero-padded ledger ordinal. Genesis points its ref at a no-ff two-parent integration
commit whose ordered parents are exact current remote `develop` and the exact reviewed R1b source
head, whose tree equals that source tree, and which lands only by compare-and-swap from the transport
base. An ordinary integration epoch likewise begins with a no-ff
integration commit whose first parent is the unique live admitted epoch ref—also the exact current
remote `develop` transport/CAS base—and whose second parent is the exact reviewed source head. A
full-clone ancestry check with shallow history, grafts, and replace refs forbidden must prove that
the transport parent is an ancestor of that reviewed source head; the source PR's reviewed diff
base is that same transport. Candidate tree equality therefore carries every already-landed slice,
while every deliberate change from transport is visible in the exact reviewed source patch. A
code-corrected recovery instead has exact current remote `develop`, including every revoked commit,
as its first/transport/CAS parent and binds the ledger-selected last live admitted ancestor only as
its logical oracle parent. A recovery-root has no live logical parent and uses the active original
or reviewed-replacement baseline as oracle authority while retaining that same current remote
transport history. A baseline-only recovery is valid only when its candidate exactly equals the
transport SHA/tree; it creates no source/candidate commit and never moves `develop`, but it must
create the new immutable epoch ref at that same transport SHA/tree. The new baseline, epoch ref, and
ledger records—not a fake source commit—distinguish it; updating/deleting/forcing that ref or any
other remote mutation is forbidden.
The entire parent resolution, ledger publication, create-only remote epoch-ref publication, and
remote/ruleset revalidation is one `candidate-epoch-publication` operation in the closed global
mutex. Its first physical ledger append is the exact atomic
`candidate-epoch-ledger-open-v1` vector `[epoch-init, candidate]`; a visible `epoch-init` without its
candidate is impossible. The operation then create-only publishes the remote ref and finally
appends either `candidate-ref-published`, binding the provider creation receipt and revalidated
immutable rule, or `candidate-publication-failed`. The candidate is not executable or admissible
before the success terminal. Its idempotent operation ID remains owned across all three steps and
cannot interleave with admission, robot execution, landing, reconciliation, lifecycle, or another
candidate publication. A crash before the opening envelope exposes nothing. After the opening
envelope with no ref, recovery revalidates unchanged transport/source/parent/ruleset authority
before creating the ref; movement or ambiguity terminalizes failure. After an exact authorized ref
create but before the terminal, recovery verifies the provider create receipt and current immutable
rule before appending success. A mismatched, pre-existing, unauthorized, or mutable ref permanently
fails those visible candidate records; it is never adopted.
Once created, an
epoch ref is sealed: it is never moved, deleted, reset, rebased, force-pushed, or extended. The
integration operator is the human or release agent invoking the reviewed R1b CLI; the CLI alone
preflights protection, allocates the ordinal, appends the idempotent opening transaction, creates
and verifies the create-only ref, and appends the publication terminal. No test worker may start
until `candidate-ref-published` proves that ref exists remotely at the recorded SHA/tree under the
verified rule.
An `admitted` or `admission-failed` attempt terminal later classifies the already-sealed candidate;
admission never creates or moves it. A subsequent proven red is a separate lock-serialized
`revoked` lifecycle operation binding the earlier admission, trigger, policy digest, operation ID,
and pre-transition ledger head rather than reterminalizing that successful attempt. Epoch refs are
evidence-bearing convergence points, not substitutes for the small PRs and
are never merged wholesale.

Admission is serial and fail-closed, with two bounded no-live-parent transitions: pre-admission
epoch genesis and post-revocation recovery-root. R1v first repairs only the observational
`Victory.record` path and lands the reviewed `robot-recipes-v1` recipe tooling on R0; it contains no
expected outcomes, oracle provenance, or admission code. After P, A, B, R0, R1a, R1v, and R1c have
landed, R1p lands and independently reviews the complete `runtime_execution_closure_v1` template/
plan/materialization/realization collector-launcher family and its
tests, still without expectations or candidate/admission code. Only after R1p lands—and before an
R1b candidate branch exists—the evidence-only R1o slice has the release operator check out and
build the exact landed R1p parent and materialize the exact 18-key oracle registry (`oracle-v1` for
all five recipes, `merge-3` for the first three, and `activation-a`/`activation-b` for all five).
R1v/R1p first execute a bounded projection-stability probe at legacy TargetFPS 240 and 1000: all 36
rows are fresh-process, full-new-game-to-classified-victory campaign roots and a prior save or other
game-load input is forbidden. Both profiles must produce byte-identical cadence-free projections for the same recipe/slot/seed before
R1o may observe an expected value. R1o then binds every oracle campaign to the feasible immutable
`pre-step-legacy-1000-v1` profile: legacy scheduler, TargetFPS 1000, vsync off, automatic simulation.
TargetFPS 1/30/60 legacy whole-game campaigns are not part of the pre-F oracle; their known coupling
is covered by bounded diagnostics until STEP exists.
The append-only pre-ledger log derives one subject key from the exact source/tree, process-invariant
runtime launch template, input closure, collector, launcher, driver, recipe set, milestone schema,
18-key manifest, and both phase
manifests. One global pre-ledger sequence mutex permits at most one open intent and exactly one
intent of each phase for that key, ordered stability-36 then calibration-54. Parallel intents,
duplicate phase intents, and a later green for a logically failed unchanged key are invalid.
Only a proven pre-execution `attempt-infrastructure-failed` recorded before
`attempt-execution-began` may restart the complete current phase under the same intent with a new
attempt/fence. `oracle-stability-failed` or `oracle-calibration-failed`
permanently burns the subject key; continuing requires a distinct key plus an independently
reviewed causal source/tool/manifest change bound to the failed sequence, then reruns all 36 and all
54. Operator cancel, timeout selection, or abandonment cannot manufacture an infrastructure retry.
`oracle-calibration-v1` fixes three fresh-process replicas per key before execution, producing
exactly 54 full-new-game-to-classified-victory campaign roots; a prior save or other game-load input
is forbidden. Every replica has a distinct planned process-launch row, process nonce, port,
output/HOME/TMPDIR/cwd roots, attempt-local claim, and replica ordinal. The stability-36 and
calibration-54 phases intentionally have distinct planned-launch-set digests and every full realized
closure intentionally differs by process; equivalence is only the identical source/tree,
process-invariant launch-template digest, immutable input closure, recipe/slot/seed, and closed
runtime-invariant projection. Neither cardinality is a free-standing number: stability binds the
canonical exact Cartesian manifest `18 oracle keys × {legacy-240, legacy-1000}` and calibration
binds `18 oracle keys × replica {0,1,2}`. Manifest rows biject initial launch rows, campaign
terminals, and retained projections; missing, duplicate, remapped, cross-phase, or unmanifested rows
fail the complete phase. Each campaign in both manifests also binds the shared raw turn-based and
cross-process save/reload receipt set and its exact campaign-control bijection.
All 54 must reach classified terminal victory and coverage; RFC-8785 bytes of the retained
`acceptance_projection` must be identical across all three replicas of each key. The coordinator
permits no majority vote, tolerance, subset selection, optional stopping, field dropping, receipt
mixing, or replicate reuse. Missing, duplicate, partial, unstable, or failed evidence invalidates
the complete attempt and is a logical failure that burns the unchanged subject key; only a proven
common-protocol infrastructure failure before `attempt-execution-began` restarts all 54 under the
same intent. Once any worker or campaign may start, interruption burns the subject key. R1o then freezes a reviewed parent-baseline artifact under
`docs/timing/baselines/<baseline-id>/`. Its canonical manifest binds the R1p-parent BuildInfo, full
`runtime_launch_template_v1`, each phase's planned-launch set and exact realized-closure receipt set,
`milestone_schema_v1`, every exact recipe/slot/derived-seed key, expected
outcome, one stable seed-specific ordered acceptance projection, and all three replica receipt
digests per key, plus external content-addressed receipts. It also binds the subject-key digest and
the exhaustive ordered history of every stability/calibration intent, attempt, infrastructure
failure, logical failure, success, and reviewed subject change. R1b imports those exact history
bindings into `epoch-init` without rewriting them. R1o may change only that baseline manifest/digest-reference subtree. Any executable,
collector, launcher, config, recipe, milestone schema, or driver change invalidates every produced receipt and must
return through reviewed R1v/R1p heads before the complete 54-run attempt is repeated. A retained
field that varies cannot be silently excluded: R1v must independently revise the expectation-free
schema or recorder, R1p must be reviewed again, and R1o must restart. R1o lands only after
that baseline commit/tree/digest has been reviewed, so no R1b candidate binary, log, receipt, or
observation can define its own oracle. There is no a-priori/prose substitute: if any of the 18 exact
parent keys cannot produce three stable victories through the frozen recorder, R1b genesis is
blocked. A later baseline replacement likewise requires executable receipts for every affected
recipe/slot/seed with the same three-replica consensus under the already-frozen milestone schema; textual original-game evidence may
motivate the replacement but cannot itself supply an expected value. The ledger's genesis
`epoch-init` names that frozen baseline and creates the R1b integration candidate without a live
logical admitted parent; exact current remote `develop` remains its transport/first/CAS parent.
Genesis cannot admit on lifecycle/replay alone: the exact
candidate must complete cold lifecycle, active-schema replay, and the immutable `merge-3-v1` whole-game
manifest before the first `admitted` record is created for the already-sealed candidate epoch ref.

After genesis, every normal integration candidate first requires a live admission for the exact
current integration tip, records that logical parent plus the reviewed commit/tree, and no-ff
merges only that reviewed head. Post-land recovery deliberately separates roles: its logical
acceptance parent is the last live admitted ancestor selected from the ledger closure, while its
transport parent is the exact current remote `develop` SHA/tree even when that head is revoked. A
code-corrected recovery commit is a fast-forward child of the transport parent and is judged against
the logical parent's effective oracles; rewriting or dropping the revoked commit from history is
forbidden. Baseline-only same-SHA recovery may re-admit the exact transport SHA/tree under a new
baseline and completes landing as a validated no-op without manufacturing an empty commit, but
only when the ledger-derived trigger union contains baseline invalidation and zero robot-red
members requiring code correction and independent review proves the code tree sound. An
ordinary candidate `run` requires a candidate record and a live logical admitted **parent**, not an
admission for the candidate being judged. Recovery-root is the sole **post-admission** no-live-
logical-parent exception; genesis is the pre-admission one. Both use reviewed active baseline
authority and exact current remote `develop` transport. Every run that can lead to
admission—genesis, recovery-root, ordinary recovery, or integration merge—executes cold lifecycle,
active-schema deterministic replay, and exactly three decided whole campaigns on the resulting
integration commit. The machine resolver—not the caller—binds subjects without the fixed scheduler
to `pre-step-legacy-1000-v1`; subjects with fixed scheduling but without STEP use
`fixed-realtime-60-v1`; F and later `manual-step-controller-v1` subjects use `post-step-fixed-manual-v1`
(fixed scheduler, TargetFPS 60, vsync off, STEP-only simulation).
Phase 0 may use untrusted probes to size capacity, but inability to run the
exact `merge-3-v1` blocks the first admission rather than weakening it. Only a
green immutable attempt permits `admit`; a red attempt appends `admission-failed` and admits nothing.
A later red against an already admitted candidate appends the separate `revoked` lifecycle
transition. The
next candidate head may not be merged and its dependent PR may not be created until all required
receipts are durable and name that exact integration commit/tree, generated build identity,
invariant launch template, exact planned process set, exact realized per-process closure set, and
immutable input set. This makes the
robot exercise the whole accumulated game after every slice, not merely the files changed by that
slice. The nightly twenty-cell rotation continues against the currently admitted head even when no
new code lands, so repeated play detects delayed and state-accumulation failures.

This order is mechanically enforced, not a release convention. Every normal post-R1b train slice maps one
exact reviewed source PR head to one sealed candidate integration commit whose first parent is the
unique live admitted tip and whose second parent is that exact source SHA. A post-land recovery
candidate instead has the exact current remote transport parent as first parent and the reviewed
correction head as second, while separately binding the last live logical admission used for oracle
resolution. The source PR may be
opened and reviewed before robot execution, but it may not merge and no dependent source PR may
branch from it until the candidate is admitted. R1b publishes the required
`robot-admission/exact-head` check on `candidate_parent_resolution_jcs_sha256`, whose tagged variant
and construction-mode fields bind source, ordered parents, transport/CAS base, candidate SHA/tree,
and either the logical-parent admission or active-baseline authority as applicable. Normal candidate creation requires logical and transport parents to be
the same unique live admitted tip. Ordinary recovery requires the ledger-derived live logical
ancestor; recovery-root forbids one. Both use exact remote `develop` as transport/first/CAS parent,
and a reviewed-source candidate uses the exact reviewed source head second only after proving that
source descends from transport and its exact PR patch was reviewed against transport. Branch protection has no
bypass actor and treats any movement of either source head **or** `develop` base as a stale required
check: it must construct and run a new candidate before landing.

Landing is one reviewed, ledger-serialized `land-exact` compare-and-swap transaction, never the
hosting platform's independently synthesized merge commit. `land-exact`, logical failure, and
revocation share the admission-ledger lock, so validation cannot race a delayed red. While holding
that lock, the workflow validates the complete scope chain, exact candidate/live admission, no
revocation or unresolved logical red, immutable candidate ref/ruleset, source/base tuple, approvals,
   required checks and gate digests, and `origin/develop==transport_parent_sha`. It then appends and fsyncs
one `land-exact-intent` containing a fresh operation ID and the complete validated tuple; still
holding the lock, it performs one bounded remote compare-and-swap from `transport_parent_sha` to the
already-admitted `candidate_merge_sha`, re-reads the remote ref, candidate ref, ruleset, and source-PR
state, and appends/fsyncs exactly one `land-exact-completed` before releasing the lock. Construction
mode is dispatched **before** comparing remote values. A reviewed-source merge must have distinct
transport and candidate SHA/trees and uses the CAS/pending/reconciliation state machine. A
baseline-only no-op must have equal transport and candidate SHA/trees; after the same complete
authority validation it appends one atomic `baseline-only-noop-land-v1` envelope containing
`[land-exact-intent, land-exact-completed]`, performs no remote update, requires no update receipt,
and can never enter pending-land recovery or reconciliation. Missing mode or an equality/mode
collision fails before an intent. A red that
linearizes first blocks landing; a land that linearizes first may complete, but a later proven red
still records the normal revocation and stops descendants. The workflow is the sole permitted
update actor for this train and has no rule/check bypass.

A pending land intent is crash-recovered only under the same lock, original intent, and operation
ID; SHA/tree equality alone never proves success or authority. If remote `develop` still equals the
recorded base, the workflow revalidates the entire initial land authority—active scope,
candidate/live admission and parent resolution, red/revocation state, immutable ref and ruleset,
source/transport tuple, exact source-PR head/state, approvals, checks/gate digests, authorized actor,
and no-bypass policy—then retries the same bounded CAS. If remote equals the recorded candidate, it
reruns that same complete authority validation and additionally requires an unambiguous provider
update receipt proving old=recorded base, new=exact candidate, the authorized land actor, and the
exact ruleset/no-bypass policy both at update time and now. Only then may it append a recovered
completion. Missing, ambiguous, or unauthorized evidence atomically appends the exact
`land-abort-reconcile-open-v1` vector `[land-exact-aborted, land-exact-reconcile-intent]` and must
fail closed through that already-open reconciliation rather than laundering the exact SHA into a
success. Any other ref value appends/fsyncs that same compound vector: the original intent closes,
exactly one reconciliation opens, the mutex never releases between them, and startup resumes that
same reconciliation operation ID until a terminal record. It never guesses that the push
succeeded, and a crash can expose neither a lone abort nor a missing reconciliation intent.
Reconciliation re-reads the remote ref but SHA/tree equality alone never proves authority. It reruns
the initial land checks over the active scope, candidate/live admission and parent resolution,
revocation/red state, immutable candidate ref/ruleset, exact source/transport tuple, source PR head/
state, approvals, checks/gate digests, actor identity, and no-bypass policy. When remote equals the
intended candidate, it additionally requires a provider update receipt proving old=transport,
new=candidate, the authorized land actor, and the exact ruleset/no-bypass policy at update time and
now; missing, ambiguous, or unauthorized movement never becomes a recovered success. Only then may
it append `land-exact-reconciled`. If remote still equals the pre-
intent transport base, it appends the distinct terminal `land-exact-reconcile-not-applied`, closes
the reconciliation, and permits a fresh land intent only after the same current authority
revalidation. Every other failure atomically commits
`land-reconcile-failure-revocation-recovery-authority-v1` as
`[land-exact-reconcile-failed, revoked, land-recovery-authorized]`: the reconciliation terminal,
subject revocation (which also closes any G pending state), and one machine-derived single-use
recovery authority are indivisible. That authority binds the failed intent/reconciliation and
operation ID, pre-transition ledger head, parent-resolution and candidate-ref publication,
pre-intent/intended/observed remote tuples, provider-query evidence, active scope/spec/tier/oracle/
launch-template/input digests, exact recovery mode, independently reviewed decision, and derived
`remediates` set. It is consumed irrevocably by the first later candidate publication, successful or
not; optional candidate retries require a newly reviewed causal authority. No original or
reconciliation intent remains open after the compound terminal. Every
logical terminal publication plus its failure/revocation/recovery-authority append is one
transaction under this same lock. The epoch ref is not merged as a later aggregate PR—the candidate
commit itself is the one-slice, two-parent source-PR resolution, and landing it causes the source PR
to be recorded closed/merged at that exact reviewed head. After CAS, the landed `develop` SHA/tree
must exactly equal the admitted candidate SHA/tree; tree-only equivalence, a platform-created merge
SHA, squash, rebase, cherry-pick, or metadata-different reconstruction is rejected. Any mismatch
blocks the next slice and forces a reconciliation epoch through all ordinary tiers. Tests reject a
skipped slice, moved base, red/landing race, CAS race, crash at every intent/CAS/completion boundary,
non-transport first parent, wrong/duplicate second parent, conflated logical/transport recovery
roles, non-fast-forward recovery, duplicate live tip, direct merge without
admission, amended reviewed head, stale green check, unauthorized updater, and landed-SHA/tree
mismatch.

Each integration epoch ref is immutable from creation, including before its first robot receipt.
Functional edits, conflict resolutions, manifests, and receipts are forbidden on it; an ordinary
epoch's single convergence commit is produced from reviewed source heads before the ref is sealed.
Immutable artifacts live outside the code branch under the attempt layout
below, and a hash-bound admission ledger names the tested code head separately from any evidence
storage ref/head. Each state records source branch, parent/base SHA, review head SHA, head tree SHA,
integration merge SHA/tree, test commands, and artifact receipt IDs without changing the tested
code tree. Every candidate, attempt, receipt, and admission also binds the exact active baseline ID
and digest plus every invalidation/replacement predecessor ID, so dependency closure never relies
on prose or branch-history inference.

“Live admission” is derived, never toggled by a mutable flag. The append-only admission graph has
one node per `admitted` record and one logical-parent edge for every non-root admission. Any node
targeted by a valid `revoked` record is permanently ineligible. The live set is exactly the maximal
eligible nodes—those with no transitive eligible admitted descendant—and its cardinality must be
zero or one. A newly admitted child therefore makes its parent non-live without rewriting or
“superseding” that parent; revoking the child recomputes the set and revives the nearest unique
unrevoked ancestor, or leaves zero. Recovery-root is legal only from that zero-live prestate and
then becomes the unique live root. More than one maximal eligible node makes the ledger invalid and
blocks admission, landing, and successor-branch authorization. The canonical resolution and digest
are inputs to every candidate's parent-resolution digest.

If an integration admission attempt or any later integrated-repeat, nightly, desktop-presentation,
desktop-performance, activation, or soak
run against the admitted head turns the robot red,
atomically revoke that admission plus every then-admitted descendant and seal every affected epoch.
Reproduce from the exact failing head and implement the minimal correction on the owning
develop-based review branch. Recovery accepts exactly one caller-supplied `trigger_record_id` as a
lookup key, not as authority over scope. Under the same ledger lock, the coordinator walks the
trigger's dependency closure and derives the union of **every** unresolved robot-red receipt and
baseline invalidation affecting the candidate ancestry, their revocations/sealed epochs, and their
required tiers. Robot-red contributes each failed tier; baseline invalidation contributes every
retired schema/manifest/expectation tier; the union always adds the complete ordinary admission set
`{cold-lifecycle, active-schema-replay, merge-3-v1}`. Callers cannot choose, omit, or narrow these sets.
When a live admission remains, open a new named recovery epoch whose logical acceptance parent is
the last exact live admitted ancestor selected by the ledger closure and whose transport parent is
the exact current remote `develop` SHA/tree, even when that transport head is revoked. When code
changes, the exact reviewed corrected head must be a fast-forward child of that transport parent;
landing compare-and-swaps from the transport parent while the robot judges against the logical
parent. Emit the candidate's `remediates` field from the canonical union. A green admission marks those revocations resolved
**for that new head only**; the original
head/receipts remain revoked. Any old descendant not rebuilt and re-admitted in the recovery epoch
remains revoked. A merge conflict is resolved in the owning PR and reviewed there, never hidden in
the integration merge. Any material head or tree change invalidates that head's receipts and every
downstream receipt that includes it.

Baseline-invalidated recovery with a surviving live parent uses that same union-derived flow. Land
reconciliation first classifies exact-intended remote state without conflating provider evidence
with source authority. Exact intended SHA/tree plus the exact authorized provider update receipt and
all current authority appends `land-exact-reconciled`. Exact intended SHA/tree with only a missing or
non-conflictingly ambiguous provider receipt—and every non-provider source, ruleset, approval,
check, actor, no-bypass, branch-protection, and robot predicate valid—atomically fails/revokes and
issues `receipt-authority-only`. An unauthorized actor, bypass, ruleset conflict, conflicting
provider evidence, invalid non-provider authority, or any other remote SHA/tree atomically issues
`reviewed-source-required`; an exact tree never launders invalid authority.

The same construction mode may consume exactly one live, unconsumed
`land_recovery_authority_reference_v1`—either original `land-recovery-authorized` generation zero or
a causally derived `land-recovery-reauthorized` successor. The original logical record contains
neither its not-yet-computable generic authority digest nor its generation-zero lineage root or
generation. Only after that complete record has its JCS digest does the coordinator derive the
external closed reference projection: generation zero is variant metadata, the completed original
record digest is the lineage root, and a domain-separated JCS hash of that non-self-referential
projection is the generic authority digest. A successor record binds its predecessor authority,
checked generation, and inherited lineage root, but likewise never contains its own generic digest;
its external reference is derived only after its complete record digest exists. Either variant is
usable in `receipt-authority-only` mode only when
an independent review proves the tree sound and the failure
limited to that unavailable non-conflicting provider update evidence. If independent review proves
the code is sound and only expectations or that exact provider evidence were wrong/unavailable, the
reviewed recovery candidate may name the exact same code SHA/tree; the new epoch/ref points at that
immutable commit without manufacturing a merge or empty code commit. It runs every retired-schema/
manifest/expectation tier plus all ordinary tiers under the separate replacement baseline or
unchanged active oracle as applicable. A baseline-only no-op is forbidden if the derived unresolved
trigger union contains any robot-red member requiring code correction; that path requires a
reviewed corrected source tree. Any remote divergence, source/ruleset/approval/actor defect, or
anything other than the exact receipt-only case likewise requires a reviewed source merge. The new
ordinary-recovery subject reruns every ordinary tier; a G recovery also reruns all four G gates from
zero and reuses no old attempt, receipt, gate, or land evidence. The failed landing remains failed.
If code also changes, its exact reviewed head is merged normally.

That generic recovery authority burns in the atomic candidate-epoch ledger-open envelope, before
the remote-ref step, whether the
candidate later admits or fails. A second candidate is never implicit reuse. One closed
`land-recovery-reauthorization-v1` transaction may follow one causally bound failed recovery
candidate terminal; it binds the consumed predecessor authority of either variant, its lineage root
and checked generation successor, failed candidate SHA/tree and terminal
evidence, current remote and authority revalidation, independent review, one canonical remediation
set, and
pretransition ledger head. Receipt-only mode may survive solely when publication failed before
`attempt-started`, the sound tree is unchanged, and there was no execution or robot red. Admission
failure, revocation, any execution, or any tree change forces `reviewed-source-required`. The new
authority is single-use at the next candidate publication; reviewed-source-required never
downgrades; sibling successors, double consumption, generation overflow, and unresolved authority
at train completion are invalid. The causal chain may repeat for finitely many independently
reviewed failed recovery generations, but no failed-candidate, prior-authority, attempt, robot-gate,
or landing evidence may be credited again. That one remediation-set field is the ledger-derived
exact union of every unresolved trigger at the reauthorization linearization and is byte-bound by
both authority revalidation and independent review; no second "new" field, caller choice, omission,
or narrower reviewed set exists.

If cascading revocation of the admitted root leaves **zero** live admissions, use a distinct
recovery-root initialization; do not reuse genesis. The complete ledger must prove that an
admission previously existed and no live admission remains. The caller still supplies only one
`trigger_record_id`; the coordinator derives the same full unresolved multi-trigger union, including
typed robot-red and baseline-invalidated payloads, proof-review receipts, required tiers, named
sealed epochs, and exact revocation set. None of those derived sets is caller-authority.
The new immutable epoch retains the original reviewed root SHA/tree and active original-or-reviewed-
replacement baseline chain as oracle provenance only; neither may act as transport. Its exact
current remote `develop` SHA/tree, including revoked history, is the transport/first/CAS parent.
Its initial candidate is either a two-parent reviewed-source merge with that transport first and
the exact reviewed recovery head second, or a baseline-only no-op exactly equal to transport, with
`recovery_root=true` and gate-emitted `remediates` equal to that full unresolved set. Any robot-red
member requires a reviewed code-corrected root whose SHA/tree differs from the failed code tree;
same-SHA recovery is legal only when the union contains baseline-only invalidations and independent
review proves no code correction is required. The new epoch/baseline/attempt identities—not a fake
code commit—distinguish a legal same-SHA retest. The union-derived run executes every contributed
tier and all ordinary admission tiers exactly once per manifest cell.
Only a green result creates the new root admission;
every prior red record remains immutable, and every descendant must be rebuilt and admitted from
that new root. This exception cannot target a descendant, cannot reopen a sealed epoch, and does not
make the genesis no-live-logical-admission exception available again.

The original baseline is immutable, but it is not infallible. A candidate mismatch alone never
authorizes changing the oracle. When independent proof plus explicit human review establishes that
one or more frozen expectations are wrong, commit a `baseline-invalidated` logical record instead
of editing or deleting anything. It binds the invalid baseline ID/digest, original parent code
SHA/tree, independent proof/review receipt, exact schema/manifest/expectation IDs and fields being
retired, and the ledger-computed transitive closure of every dependent candidate, attempt, receipt,
admission, descendant, and epoch. When that closure contains admitted subjects, one
`authority-invalidation-revocation-v1` envelope atomically commits ordered
`[baseline-invalidated, revoked]`; with no admitted subject it contains only the invalidation. The
revocation payload names every affected admission and sealed epoch and permanently rejects
`run`/`admit` against that invalid baseline. A claimed closure mismatch is fatal.

Expectations become usable again only after a separate immutable `baseline-replacement` record. It
binds the invalidation and replaced baseline, a separately reviewed expectation-artifact commit/
tree/digest, the complete schema/manifests/inputs/hashes/outcomes/milestones, and its source kind:
an independently reviewed corrected-parent engine with executable receipts for every affected
recipe/slot/seed under the unchanged frozen milestone schema. Calibration must prove exact
`BuildInfo` and the closed runtime invariant projection from a parent containing none of the
candidate-under-test changes;
textual or a-priori evidence cannot provide an acceptance value. The replacement must be reviewed/frozen before candidate
creation or execution; candidate binaries, logs, receipts, or observed outputs can never source it.
If no trustworthy independent replacement exists, admission remains blocked. A later error repeats
the same immutable invalidation/replacement cycle against the currently active replacement.

A compatibility-ledger row authorizes an implementation goal, never an oracle difference. By
default every candidate must reproduce its parent's active effective oracle byte-for-byte across
both expectation-bearing surfaces: every active replay-schema expectation and the retained
whole-campaign `acceptance_projection`. A difference on either surface is admissible only through
the distinct pre-candidate `compatibility-delta-authorized` transition defined by
`train_scope_v1.contracts.compatibility_delta`. It can authorize neither non-victory, recipe/seed
change, coverage weakening, milestone-schema change, nor a wildcard projection change. P declares
every CD row's oracle disposition. Only CD-03, CD-12, and CD-16 are initially eligible. R1v's
closed component-policy schema assigns campaign component IDs and defines the closed per-schema
owner-map format for replay components; each replay schema is inadmissible unless its own
non-wildcard, non-overlapping CD-owner map was independently frozen with the schema before that
schema became active. Neither policy surface contains expected old/new values. Eligibility is not
permission; all other rows and every unowned component on both surfaces must equal the active
effective parent oracle exactly. A new
eligible row requires a strict reviewed scope extension and revised R1v policy before any reference
run or candidate exists.

To keep that exception independent of the candidate's other work, C is split. C lands its
parity-preserving simulation/control substrate first. After C admission, the separately reviewed
`control-epoch-v2` schema and its closed CD-owner map are frozen on C before any C2 reference
starts. C2 then owns only the oracle-observable portions of CD-03, CD-12, and CD-16. Before a C2
candidate record or integration commit exists, its
exact reviewed source head must be based directly on the live admitted C SHA/tree and sealed with a
diff manifest mapping every behavior-changing hunk to exactly one eligible row. Recipe, seed,
schema, oracle, collector, runner, launch configuration, data, ISO, mod, or input changes are
forbidden. Under `compatibility-delta-intent` and the common attempt fence, that delta-only source
head executes every active replay manifest three fresh-process times against both the admitted
parent and the delta-only source, requiring one byte-identical retained replay projection per
manifest on each head, then executes all 18 active exact-seed keys three fresh-process times on each
head—54 parent plus 54 delta-source campaigns, 108 total. The parent consensus must equal its active
effective campaign oracle before any comparison. A parent mismatch appends the compatibility
attempt's terminal `parent-reference-mismatch`, closes that exact intent/attempt/fence, burns its
evidence, and blocks a fresh parent-first compatibility intent until the mandatory exact-parent audit is
opened; it does not immediately revoke the otherwise live parent. That audit
reruns every active replay cell and all 54 parent campaigns at three replicas each. A durable
independent reproduction appends `parent-audit-reproduced`, is robot red, and revokes the parent and
descendants. Nonreproduction appends `parent-audit-not-reproduced`, does not pass or clear the
mismatch, and permits one fresh parent-first compatibility attempt under a new intent/fence. That
attempt must complete and decide every parent replay cell and all 54 parent campaigns before it may
launch any delta-source work. If the fresh parent half matches both active effective oracles, the
coordinator appends `parent-reference-cleared` binding the mismatch, audit, fresh intent/attempt/
claim/fence, both complete parent receipt sets, and both parent consensus sets. It then launches the
delta-source half under the same attempt: source success may authorize the delta; source-only
logical failure appends `compatibility-delta-failed`, burns that reference, leaves the parent clear,
and permits a later corrected source under a new compatibility intent. If the fresh parent half
mismatches before any delta launch, it terminalizes the fresh attempt with a new mismatch and opens
the second full parent audit. Only common pre-execution infrastructure failure before
`attempt-execution-began` restarts the entire parent-first workset under a new attempt with no
reuse; any later interruption is the selected logical failure. At most two full parent audits are permitted per
mismatch chain; a second audit without either durable reproduction or a valid clear appends
`parent-audit-escalated` and blocks the train. Only `train-abandoned` or a separately reviewed strict
scope/contract extension may follow escalation; a human assertion can never manufacture a pass.
Omitting a
schema, replay cell, replay replica,
expectation field, campaign key, or campaign replica fails the attempt. Every campaign must win and
satisfy coverage; each head/key must have one byte-identical three-replica consensus. Parent-to-reference differences must be zero
outside the policy-owned components on both surfaces; every changed component has exactly one row
owner. The record freezes exact old/new replay and campaign component values rather than a tolerance
or prose relation. Only common infrastructure failure before `attempt-execution-began` restarts the
complete replay comparison and all 108 campaigns under a new attempt with no receipt reuse; any
later interruption is logical failure.

`compatibility-delta-authorized` binds the active scope/policy/component digests; live parent
admission/SHA/tree; reference source SHA/tree and diff-manifest digest; active baseline and ordered
effective replay/campaign oracle-chain digests; exact active replay-schema-set/materialized-manifest digests and
three-replica parent/source replay receipt sets; runtime/input/schema/recipe/seed manifests; all 18 keys, three
replicas/head, 108 terminal campaign receipts, and paired per-head/key consensus digests; exact owned overlays for
both expectation surfaces; proof of no other replay/outcome/projection/coverage/schema delta; and a
role-signed independent-review receipt whose
reviewer differs from source author and calibration operator. It precedes candidate creation and is
single-use for that exact tuple. C2's later candidate must name that authorization, use the exact
reference source head as second parent, and produce an integration tree equal to the reference
tree. Authorization materializes two candidate-private prospective chain digests: parent active
campaign chain plus the exact campaign overlay, and parent active replay chain plus the exact replay
overlay. Every candidate, attempt, claim, terminal receipt, admission, revocation, land, and
reconciliation record binds both digests. The exact integration candidate still executes cold
lifecycle, active-schema replay, and `merge-3-v1`, but those tiers resolve against its prospective
chains—not the unchanged global parent chains. Neither the reference receipts nor the 108 paired
campaigns count toward admission. Successful admission atomically compare-and-swaps both
prospective chains into the global active chains; an immediate `compatibility-delta-failed`
terminal burns both. A later `revoked` or `compatibility-delta-invalidated` record is a new,
globally serialized lifecycle/chain operation binding the earlier success, trigger, policy digest,
operation ID, and pre-transition ledger head; it burns both without reterminalizing the
authorization attempt and permits neither
partial activation nor reuse. Descendants then resolve the base plus ordered
active overlays. No prospective chain is globally visible before admission, and no baseline or
replay-schema file is rewritten.

`compatibility-delta-failed` burns the reference head but does not revoke the sound live parent.
`compatibility-delta-invalidated` is serialized with landing/revocation: before admission it burns
the authorization and any bound candidate; after admission it revokes that admission and every
descendant. Base or replay-schema invalidation invalidates every dependent overlay; delta-only
invalidation preserves the base. A baseline replacement or replay-schema replacement cannot inherit
an overlay without a fresh complete replay comparison and paired 108-run calibration.
Any parent-side mismatch follows the bounded `parent-reference-mismatch` audit above and cannot be
misclassified as a delta-source pass or ordinary infrastructure failure. A delta-source-only
logical, stability, ownership, or equality failure
appends `compatibility-delta-failed` and burns only the reference while the still-sound parent stays
live. Infrastructure failure may close only in the common pre-execution window after proving no
worker/process/observation or durable logical-red artifact exists, then restarts both replay heads
and all 108 campaigns with no receipt or consensus reuse. After `attempt-execution-began`, any
interruption is `compatibility-delta-failed` or `parent-reference-mismatch` as selected by the
evidence, never infrastructure.
Corrected source, amended head, moved parent, changed policy, changed baseline, recovery, or
recovery-root likewise requires a new authorization; reuse is forbidden. Recovery binds the valid
base plus complete active overlay chain and adds every affected expectation-bearing tier to P's
machine-derived ordinary/recovery union. The delta reference is never a live admission and earns no
nightly credit. `land-exact` revalidates the current, non-invalidated, single-use authorization under
the ledger lock.

Every recovery binds the currently active reviewed baseline: the original receipt when valid, or
the complete invalidation chain with one valid matching replacement after every invalidation. An
invalidation without its separately reviewed replacement means there is no active baseline and all
candidate/run/admit operations remain blocked; wording such as “if one exists” never makes the
replacement optional. If no admission has ever existed, a
new genesis may use that valid replacement. If prior admissions existed but none remain,
recovery-root uses it and genesis stays disabled. If a valid earlier admission survives, normal
recovery names that parent and the replacement expectations. Every path retains the original
receipt and complete chain for provenance; neither record type admits code.

Before the train has any admission, a failed R1b genesis candidate seals only its attempted epoch.
A newly reviewed R1b head may open another immutable genesis attempt against the same frozen parent
baseline or its valid reviewed replacement; the genesis no-live-logical-admission exception becomes permanently unavailable as soon as the first
admission is appended. Recovery-root is separately authorized only by the zero-live state and
immutable evidence contract above.

If a parent lands by a history-changing method or a child is rebuilt/retargeted, start a new named
integration epoch from the new landed base rather than rewriting the tested epoch. Re-merge the
exact rebuilt heads and rerun affected tiers. Old epochs and receipts remain immutable provenance.

No campaign result before R1b is admissible as a pass. R0 makes terminal classification truthful but
does not bind source, build, and binary or make the victory path executable. Pre-R1b runs may
characterize runtime and fixtures, but an ambiguous timeout, transition, partial advance, unknown/
dirty source, stale binary, or incomplete provenance is untrusted. The permitted pre-R1b assertion
is an exact expected-red ordered lifecycle trace; it proves the driver reaches the named StageCmd
boundary, not that a battle or campaign completed.

The whole-game rows below share one non-negotiable control-evidence rule. Every individual campaign
must contain both a genuine executable turn-based action epoch and a planned CityView save followed
by acknowledged clean exit, orderly process reap, a newly identified process loading the sealed
save, and continued terminal victory. Their closed raw receipts must biject the normalized campaign
coverage rows on the same tested SHA/tree, attempt, fence, and campaign ID. This applies to the
36-run stability probe, 54-run calibration, candidate three, landed-head twenty, independent
nightly twenty, activation hundred, and every soak campaign. A generic coverage flag, a different
campaign's receipt, an in-process load, a recovery restart, or an inherited prior-head receipt
cannot satisfy it.

Validation tiers:

| Tier | Trigger | Required robot work |
|---|---|---|
| Smoke | every pushed PR head; diagnostic/untrusted until the exact head contains reviewed R1b | boot/load, one city day, realtime battle, turn-based battle, save/reload, clean exit; an admissible pass receipt additionally proves the R1a/R1b build/lifecycle oracle, exact source/tree/launch-template/planned-and-realized-process/input binding, and clean provenance |
| Cold Skirmish lifecycle | Z known-red; B source/unit green; R1a diagnostic journal green; R1b and every integration merge admissible | Z's legacy driver may emit only an untrusted STATUS/log expected-red at the named boundary; it cannot emit an R1a journal. B's focused red/green production tests prove the command loss and fix. Beginning with R1a, require two exact ordered lifecycle-journal milestone subsequences: cold prefix `MainMenu —BUTTON_SKIRMISH/PUSH→ LoadingScreen —ready/REPLACE→ Skirmish —begin/PUSH→ MapSelector —POP/resume→ Skirmish —PUSH→ SelectForces —goToBattle/PUSH→ AEquipScreen —POP→ SelectForces::resume —POP→ Skirmish::resume —REPLACEALL→ BattleBriefing —mode/REPLACEALL→ BattlePreStart —OK/PUSH→ LoadingScreen —ready/REPLACE→ BattleView`, plus existing-game prefix `CityView —PUSH→ InGameOptions —PUSH→ Skirmish` followed by the same Skirmish command/lifecycle subsequence. Both distinct `LoadingScreen` milestones are mandatory in the cold fixture. An optional data-driven `BattlePreStart —begin/PUSH→ MessageBox —POP/resume→ BattlePreStart` is journaled and validated when present. A `Skirmish` observed after the initial entry click without journaled `begin/PUSH MapSelector` is stale/failure and is never repaired by a second click; later journaled Skirmish resumes are required. R1a makes the green trace observable but untrusted; first exact R1b binds both routes to admissible provenance |
| Transcript replay | every genesis, recovery, and integration merge; genesis uses its active reviewed R1o original-or-pre-admission-replacement baseline, ordinary paths use the admitted logical parent's active set, and recovery-root uses its active reviewed original/replacement baseline | Every schema requires an immutable starting save, recorded action transcript, and exact expected observable-state hashes. Initial genesis freezes `journal-v1` through `epoch-init`; it uses one persistent request/reply connection plus R1a transaction/stage-generation milestones, but exact hashes are permitted only after a control-only stage or game Pause is query-confirmed and remains identical across two later request/reply turns. Every baseline carries an immutable `replay_schema_set` of schema IDs/digests/manifests/expectations. Recovery uses that active set: the valid original begins with v1; any invalidation requires a separate replacement containing the complete currently applicable set. It never hardcodes v1 after later schema upgrades and never calibrates expectations on the recovery candidate, including same-SHA baseline-only recovery. C itself is judged by v1; after C is admitted, `control-epoch-v2` is calibrated/reviewed/frozen on that exact C head for descendants and adds explicit closing-control-epoch fences, still with simulation suppressed at exact checkpoints. F itself is judged by v2; after F is admitted, `manual-step-v3` is calibrated/reviewed/frozen on that exact F head for descendants and adds exact MANUAL STEP pulse counts. Running-simulation exact equality is unavailable before v3 and is covered meanwhile by unit/pulse transcripts plus campaign outcomes/milestones; unexplained divergence within any schema's honest scope blocks admission |
| Integrated campaign | every genesis, recovery-root, ordinary recovery, and integration merge; one of the three machine-required tiers resolved from `contracts.ordinary_admission` | exact `merge-3-v1`: the three named full-victory recipes and deterministic seed slots defined above, each in a fresh process from a full new-game campaign root through classified terminal victory, collectively covering realtime/turn-based and standard/high. A prior save or other campaign-root game-load input is forbidden; same-campaign save/reload coverage may add continuation processes but cannot replace the root or victory. Expectations come from the active effective oracle chain, never the candidate. Missing, equivalent, partial, timed-out, or candidate-authored cells fail; capacity probes never waive the tier or its digest-bound materialization |
| Integrated repeat | after exact landing/reconciliation of R1b and every later machine-listed runtime slice through G, the parity line, and T-180; blocks descendant non-recovery branches and every later non-recovery runtime landing | exact capability-resolved `integrated-repeat-20-v1` on the landed `develop` SHA/tree: twenty distinct deterministic slots, five recipes exactly four times, fresh process per full new-game-to-terminal campaign, all victorious with schema/recipe coverage. It binds both active effective oracle chains and all 20 receipts. A subject without fixed scheduling uses legacy/1000/automatic; fixed scheduling before MANUAL/STEP uses fixed/60/REALTIME; fixed scheduling with MANUAL/STEP uses fixed/60/MANUAL-STEP. Candidate, diagnostic, smoke, partial, or prior-head runs cannot substitute. Repeat and nightly are separate globally serialized intents, attempts, fences, manifests, and twenty-campaign receipt sets; dual credit and receipt reuse are forbidden |
| Nightly/extended | from first R1b admission until `train-completed` or `train-abandoned` | exact phase-resolved 20-campaign batch on the current live admitted head: pre-E is five recipes × four distinct legacy/1000 slots; E-before-F is two keys × ten feasible legacy/fixed REALTIME rows; F+ is two keys × the exact ten-profile activation topology. Every row is a fresh-process full-new-game-to-classified-victory campaign root; a prior save or other campaign-root game-load input is forbidden. `nightly_schedule_v1` makes ordinal zero immediate and every later ordinal due at an authenticated 24-hour deadline; completion proves the due range is gap-free. Every cell must win and satisfy the closed projection/schema/recipe coverage, and all ten same-key projections in cross-axis phases must be byte-identical. Low-FPS legacy diagnostics remain outside cardinality and cannot substitute |
| Desktop presentation | before PR G only | exact candidate-independent `desktop-presentation-v1` on every support-map profile, including immutable device/context revalidation and only manifest-authored window-state transitions; all exact surface/checkpoint receipts must pass under the common full-attempt restart/fence contract |
| Desktop performance | before PR G only | exact `desktop-performance-v1` on the reference desktop and admitted-F comparison authority: three predeclared 10,000-record windows per late-city/realtime-battle/turn-based-battle class; every predicate—p99.9 component/lag/urgent bounds, bounded 0.1% tails, 100-ms hard ceilings, zero code-attributable misses/forbidden drops, and matched admitted-F-relative turbo p99/p99.9—must pass separately in each 10,000-record run/class and again in the 30,000-record aggregate/class. A host-health receipt can diagnose contamination but cannot retry after execution begins; only common pre-execution infrastructure may restart |
| Activation | before PR G only | exact `activation-100-v1` projected from R1v's pre-R1o ten-key/100-row topology plus F's exact execution overlay; F cannot change oracle keys, axes, profiles, or expectations. Every row is a fresh-process full new-game-to-classified-victory campaign. Per oracle key the exact profiles are legacy REALTIME at 1000; fixed REALTIME at 0/1/30/60/240; and fixed MANUAL/STEP at 0/1/60/240. MANUAL rows acquire the lease at query-confirmed control-only MainMenu and prove zero automatic pulses before game commit. Every row performs one base R1o join and applies ordered active overlays; all ten projections per key must be byte-identical to the active effective oracle. Low-FPS legacy remains a bounded diagnostic outside the 100 |
| Soak | before PR G only | one fresh uninterrupted half-open 86,400,000,000,000-ns root-launch window on the exact G candidate, followed by bounded drain of every root started before cutoff. The F-frozen policy and G intent are attempt-independent; the exact origin/cutoff and clock domain are created atomically with `attempt-execution-began`, so a legal pre-execution retry receives a genuinely fresh full 24-hour window while any later interruption is logical red. A gap-free campaign ordinal covers roots and a separate gap-free process-launch ordinal covers every root plus reload continuation. Cutoff blocks only new roots: immediately after a successful spawn and PID/start-identity capture, the coordinator publishes one no-clobber root-start receipt with a timestamp sampled from that execution-window clock domain. Only `origin <= root_start < cutoff` is started; a planned root lacking that exact receipt before cutoff fails, while a started campaign may launch only its predeclared same-campaign continuation during bounded drain computed by checked addition from the bound root-start timestamp. Root-start receipts and campaign terminals each biject the finalized campaign prefix; realized process receipts biject the finalized process prefix. Every root is a fresh-process full-new-game-to-classified-victory campaign; prior campaign-root load is forbidden. The admitted-F gate spec supplies the positive minimum and every resource/evidence/drain predicate, and every campaign/replay decision binds both exact effective oracle chains under the common attempt/fence contract |

Every run owns a unique directory under
`<out>/<epoch>/<tested-head>/<batch-attempt-id>/`:

```text
attempt-claim.json
manifest.json
cells/<cell-id>/claim.json
cells/<cell-id>/events.jsonl
cells/<cell-id>/campaigns/<campaign-id>/terminal.json
cells/<cell-id>/cell-terminal.json
```

Every executable batch first has exactly one durable intent under
`train_scope_v1.contracts.batch_attempt_protocol`. Before R1b exists, R1p's reviewed append-only,
content-addressed calibration log owns `oracle-stability-intent` and `oracle-calibration-intent`;
R1b imports those exact records at `epoch-init` without rewriting them. The admission ledger owns
the closed post-R1b set: `admission-intent`, `replay-schema-intent`,
`compatibility-delta-intent`, `parent-audit-intent`, `integrated-repeat-intent`, `nightly-intent`,
`desktop-presentation-intent`, `desktop-performance-intent`, `activation-intent`, and
`soak-intent`. An unknown intent kind is invalid. The intent
binds the complete candidate/ref/SHA/tree, base, baseline, runtime/input closure, manifest, and
thresholds but starts no worker. Under the ledger lock, `attempt-started` allocates one globally
monotonic fencing ordinal and `batch-attempt-id`, binds the intent plus coordinator/host/PID/OS-start/
nonce identities, and becomes that intent's sole current attempt before any worker starts. A worker
may publish only while its intent and fence are current; every create/link/terminal operation
rechecks both under the durable authority, so a late artifact from a terminalized fence is rejected
even if its path was once valid.

A coordinator exclusively creates that attempt's leaf directory after `attempt-started`; an
existing or stale empty path is a collision and is never adopted, resumed, deleted, reused, merged,
renamed over, or treated as a retry. It no-clobber-creates `attempt-claim.json` binding the intent,
fencing ordinal, epoch, candidate, tested SHA/tree, active baseline ID/digest, manifest digest,
coordinator nonce/PID/OS-start identity, and attempt ID. Infrastructure takeover may append
`attempt-infrastructure-failed` only inside a deliberately tiny **pre-execution window**. Immediately
before any worker, subject process, candidate-observing command, or gameplay/profiling execution may
start, the coordinator durably appends `attempt-execution-began` under the current intent/attempt/
fence. Retryable infrastructure requires proof that this record does not exist, every coordinator/
worker identity is dead, no worker or subject-process spawn receipt exists, and no candidate
observation, progress, output, or logical-red artifact exists. Inability to prove every negative is
not infrastructure. A crash after `attempt-execution-began` but before the actual spawn is
conservatively on the logical side of the line.

After `attempt-execution-began`, **every** crash, reboot, sleep discontinuity, lost lease, host loss,
coordinator/worker/process loss, forced exit, timeout, ambiguous exit, inaccessible evidence root,
or malformed/truncated terminal selects the exact intent kind's logical failure; an external host-
health receipt may diagnose it but cannot create a retry. Any durable logical-red evidence likewise
selects that failure and cascading revocations. This removes operator-controlled optional stopping:
once the subject can have influenced evidence, interruption burns the attempt/intent rather than
rerolling it. When the
subject is already admitted, the selected failure and `revoked` are one required compound
transaction envelope, so the embedded terminal cannot become visible or release the mutex alone.
Only a legal pre-execution `attempt-infrastructure-failed` terminalizes the old fence before a later
`attempt-started` may allocate a new ID/fence, and that replacement restarts the intent's complete
manifest from zero. Ordinary genesis/recovery/integration attempts receive exactly this
protection—`Crash recovery allocates a new ID` is never sufficient by itself. Logical/process/gameplay red uses the exact intent-kind
table's terminal failure (`oracle-stability-failed`, `oracle-calibration-failed`, `admission-failed`,
`replay-schema-failed`, `compatibility-delta-failed` or `parent-reference-mismatch`, `parent-audit-escalated`,
`integrated-repeat-failed`, `nightly-failed`, `desktop-presentation-failed`,
`desktop-performance-failed`, `activation-failed`, or `soak-failed`) and is not retryable
infrastructure. `attempt-infrastructure-failed` is the sole infrastructure terminal for every
kind, is legal only before `attempt-execution-began`, and closes the attempt while leaving its intent
open for either one complete pre-execution retry or the train-owner abandonment path; there is no
nightly-, audit-, schema-, or calibration-specific alias. A green ordinary admission and every other
terminal completion bind the attempt-claim/fence and manifest digests. A manifest-bound attempt has
an exact cell set; cardinality is enforced within that attempt. A later complete green attempt may
replace a prior attempt for the **same intent** only when the prior terminal is the proven
`attempt-infrastructure-failed`; the failed attempt remains immutable and the replacement restarts
the complete manifest. A logical terminal closes its intent and can never be superseded by a later
green for that intent. The pre-ledger oracle rule is stricter still: a logical failure permanently
burns the complete subject key, so the same tuple can never produce an admissible later green. The
`parent-reference-mismatch` terminal closes and burns its compatibility-delta attempt before a
parent-audit intent or fresh parent-first compatibility intent can start. `revoked`,
`replay-schema-invalidated`, and `compatibility-delta-invalidated` are not attempt terminals: each
is a later lifecycle/chain operation serialized under the global ledger lock, binding the prior
success, trigger, policy digest, operation ID, and pre-transition ledger head and never reusing or
reterminalizing that attempt. The
coordinator claims each cell with exclusive creation, never overwrite-capable rename, and publishes
every campaign terminal plus the cell summary with no-clobber atomic create/link semantics. Parallel
cells never share JSONL, checkpoint, progress, campaign, or terminal files. A normal cell has the
manifest's exact campaign cardinality. The Python R1b coordinator owns soak duration through an
injectable wrapper around `time.monotonic_ns()`; this wall-time authority is deliberately separate
from C0's engine loop clock. The F-frozen policy and G `soak-intent` bind only duration, topology,
row-generation, capacity, oracle, and runtime-template authority—not an absolute execution origin or
cutoff. The soak-specific `attempt-execution-began` atomically publishes
`soak_execution_window_v1`, binding the attempt/fence and a checked monotonic origin plus exactly
86,400,000,000,000 nanoseconds. Failure before that publication may create a new attempt/fence and
new full window under the unchanged intent; interruption after it is `soak-failed` and cannot retry.
No new campaign root may be planned or spawned at or after cutoff. A root planned before cutoff but
without a no-clobber `soak_campaign_root_start_receipt_v1` published before cutoff is red and cannot
leak into drain. The receipt binds materialization, PID/start identity, nonce, campaign/process
ordinals, execution-window clock domain, and coordinator-sampled `root_start_monotonic_ns`; child,
caller, wall-clock, cross-domain, missing, or duplicate timestamps are invalid. Already-started roots may run
only their predeclared same-campaign reload continuations through the bounded drain deadline. The
coordinator then publishes one cell summary binding both finalized gap-free prefix chains and the
dynamic completed count; it can never overwrite an earlier terminal.

One blocking schema-consistency test enumerates all twelve executable batch kinds and proves that
each resolves to exactly one closed intent-kind row; every immediate success/logical-failure record
belongs to that row; the three post-success lifecycle transition types are provably outside it;
every claim, receipt, consensus, and attempt terminal binds the current attempt/fence;
`attempt-infrastructure-failed` is the only infrastructure terminal and is legal only before the
durable `attempt-execution-began`; a legal pre-execution retry restarts the complete immutable
manifest, while any later interruption is logical failure; and partial/cross-fence reuse fails for both pre-ledger oracle attempts and all
ten post-ledger kinds. R1c campaign-control receipts are leaves of the existing oracle-stability and
oracle-calibration attempts, not a thirteenth intent kind.

All post-R1b mutation/execution operations reference one closed mutex owned under the ledger-and-
landing lock: candidate/epoch publication, ordinary admission, land, reconciliation, replay-schema upgrade, compatibility
delta, parent audit, integrated repeat, nightly, all four G gates, baseline/lifecycle transition,
and train terminal. “Open” begins when an intent/atomic transition is appended and ends only at its
terminal/atomic commit; running workers do not release ownership. No pairwise local rule may weaken
that matrix. The only same-operation kind transitions are `land-exact` → `land-exact-reconcile`
through the atomic abort/open vector and an open operation → its train-abandonment terminal branch;
both preserve the same operation ID and continuous mutex ownership and are never a second
overlapping operation. Every other kind transition is forbidden. `g-gates-pending` is a durable
long-lived reservation with one `g_pending_operation_id`; it does not continuously hold an OS mutex
between child operations. Each gate, land, reconciliation, abandonment, and terminal operation
acquires the global lock for its own open interval and binds that reservation ID. From G admission
through G land or revocation, only the exact
next ordered gate (or its full retry), exact G abandonment vector, or subject revocation may open; after `soak-completed`, only
the exact `land-exact` operation may open, and after a land abort only its exact reconciliation may
open. `land-exact-reconcile-not-applied` returns that same exact G subject to a fresh `land-exact`
operation after full current authority revalidation. Successful land or subject revocation closes
the pending state.

Before any G source branch, reviewed head, candidate record, or execution exists, exact admitted and
landed F appends one independently reviewed `gate-spec-frozen`
record for every typed entry in `train_scope_v1.required_gates[]`. Each record binds admitted F's
admission/ref/SHA/tree, the active scope and immutable gate-manifest/schema JCS digest, the support-
map digest, every objective threshold/cardinality/profile/recipe/save/seed/profiler field, and the
independent-review receipt. The scope entry identifies `F` plus `gate-spec-frozen` as its authority,
the JCS digest field, intent/success/failure record types, subject slice, required bindings, and one
machine-evaluable predicate. A G candidate cannot create, select, or amend the specification that
judges it. `train-completed` iterates the active scope's typed predicates; it does not hardcode a
list of friendly gate names or accept a label-only success.

The exact sealed G candidate must first earn its ordinary `admitted` terminal. Only afterward does
the release operator append the four gate intents, in fixed order:
`desktop-presentation-intent` → `desktop-performance-intent` → `activation-intent` → `soak-intent`,
under the ledger lock. Each later intent requires the preceding gate's success, and each binds the
exact G candidate ref/SHA/tree plus `subject_admission_jcs_sha256`, admitted-F authority receipt,
active oracle/replay/runtime/input digests, support-map digest, gate-spec ID/JCS SHA-256, and all
required profile/device/runtime/input/profiler/scenario/save/seed digests, F-parent comparison
receipts, sample cardinalities, and objective predicates required by that typed spec. The generic
reviewed coordinator verbs are `intent-create --kind <gate-kind>`, `attempt-start --intent-id`,
`attempt-fail-infrastructure --attempt-id`, `intent-complete --intent-id --attempt-id`, and
`intent-fail-logical --intent-id --attempt-id`; every kind uses the same fence and no gate can close
through an ad-hoc file or manual label. A coordinator/
process/host crash, reboot, sleep discontinuity, or lost lease **before**
`attempt-execution-began` may follow the common pre-execution `attempt-infrastructure-failed`
transition; a new attempt under the unchanged intent restarts every presentation checkpoint, all
100 activation cells, or the full continuous 24-hour interval from zero. After execution begins,
any such interruption is the gate's logical failure and cannot retry. No campaign, elapsed
nanosecond, or terminal is carried between legal pre-execution attempts. Exactly one uninterrupted
complete attempt may close an intent with `desktop-presentation-completed`,
`desktop-performance-completed`, `activation-completed`, or `soak-completed`; a presentation,
performance, oracle, protocol, resource, or gameplay red closes it with
`desktop-presentation-failed`, `desktop-performance-failed`, `activation-failed`, or `soak-failed` and requires a
reviewed new candidate or baseline-replacement flow, not an operator retry that erases the red.
Each attempt may start only after its manifest, claims, process-invariant runtime launch template,
attempt-specific planned process-launch set, and common fence are
durable. A G `land-exact-intent` cannot start until it binds all four ordered success records for
that same candidate and admission; any gate failure or subject revocation blocks every later gate
and landing. If the ledger-derived remediation closure contains the G admission, any G gate
failure, or the G subject's revocation, the correction is a new G recovery generation: its new
subject must first pass ordinary admission, then execute all four gates from zero in the same fixed
order with fresh intents, attempts, fences, manifests, processes, and evidence. No historical gate
success, receipt, attempt, manifest, sample, campaign, or cardinality may count toward the recovered
subject, and its land terminal binds only that new subject's ordered four-success set. The soak
launch window is exactly half-open
`[start_ns, start_ns + 86,400,000,000,000)`; launch at `deadline-1` is legal and at `deadline` is
forbidden. Every already-started campaign must terminalize within its manifest-bound drain deadline.
A game/process crash is a logical red, while coordinator/host failure is retryable infrastructure
failure only through the fresh full-attempt rule above.
Each receipt contains campaign ID,
engine/source/head/tree SHAs, clean-worktree proof, generated build identity, app-bundle/runtime-
closure hashes, configure/build log hashes, compiler/CMake/submodule/data/ISO/mod/config provenance,
effective-options/argv/environment/cwd digests, platform-profile identity, driver version, seed,
`rng_seed_consumed` record JCS digest/ordinal/mode/configured/consumed fields, configuration,
scheduler/protocol version, starting-save hash, expected terminal outcome and
milestones, active baseline ID/digest and invalidation/replacement chain, observed terminal result,
ticks/days advanced, battles entered/resolved,
research/invasion/funding/portal milestones, save/reload checkpoints, transition reasons, dropped
pulses, lease/job failures, wall time, and log/artifact paths. Lease capabilities are never written.

Two evidence classes are deliberately separate:

- **Deterministic replay** is capability-versioned rather than claiming a
  `process_global_mutation_fence` capability that the
  current immediate harness reply does not provide. `journal-v1` uses an immutable save, recorded
  input/action transcript, sequential request/reply turns on one persistent connection, and R1a
  lifecycle transaction/stage-generation milestones; it hashes only query-confirmed paused or
  control-only quiescent states after two later request/reply turns return the same hash. The C
  candidate is judged by parent-frozen v1; only after C admission does `control-epoch-v2` add
  explicit closing-control-epoch checkpoints for paused/control-only states. The F candidate is
  judged by parent-frozen v2; only after F admission does `manual-step-v3` add exact MANUAL STEP
  pulse checkpoints and running-simulation equality for F descendants. These are typed
  `replay-schema-upgrade-v1` transitions, not prose freezes: `replay-schema-intent` names the exact
  newly admitted and landed/reconciled SHA/tree plus a pre-observation schema definition and closed
  owner map; three fresh-process replicas of every declared cell must be byte-identical; independent
  review produces `replay-schema-authorized`; and one atomic `replay-schema-activated` append extends
  the active effective replay schema/oracle chain without mutating its base. Authorization is an
  intermediate record: the same replay intent/attempt/fence stays open through independent review
  and atomic activation, and `replay-schema-activated` is its success terminal. A later
  `replay-schema-invalidated` is a separate lifecycle/chain transition binding that success, not a
  second terminal for the attempt. Activation happens
  after the exact C or F land terminal but before that same landing's `integrated-repeat-intent`;
  therefore the mandatory twenty-victory landed-head repeat and all later nightlies bind the new
  effective replay-chain digest. The activation transaction holds the global ledger/landing lock
  and is mutually exclusive with land/reconcile/repeat/nightly/G-gate/compatibility-delta/parent-audit
  transitions. Failure or `replay-schema-invalidated` blocks and revokes dependents. No same-landing
  repeat, nightly satisfying current-chain freshness, or descendant candidate may exist until its
  required upgrade is active, and no expected checkpoint value may exist before the first calibrated
  parent run.
- **Campaign execution** uses a required nonzero seed and immutable manifest, but wall-time sleeps
  and asynchronous policy decisions mean seed-only runs are not bit-exact replay. Merge/activation
  cells pass only against their exact seed-keyed parent oracle. Rotating nightly cells instead
  require victory, the frozen milestone-schema predicates, recipe coverage, and relational equality
  of the acceptance projection across that night's equivalent FPS/scheduler variants; seed-specific
  values remain observations. A new early defeat is always a failure requiring reproduction. The
  100 activation campaigns use the exact feasible ten-profile topology per oracle key: one
  legacy-REALTIME/1000 profile, five fixed-REALTIME profiles at 0/1/30/60/240, and four
  fixed-MANUAL/STEP profiles at 0/1/60/240. All ten projections for a key are byte-identical.
  Separate bounded low-FPS legacy diagnostics characterize coupling; they never contribute an
  oracle, admission, nightly, or activation campaign.

R1v introduces five immutable, complete new-game-to-terminal recipe specifications under
`tools/fixtures/robot_campaigns/recipes/`: `victory-standard-mixed-v1`,
`victory-high-mixed-v1`, `victory-realtime-heavy-v1`, `victory-turnbased-heavy-v1`, and
`victory-alien-dimension-v1`. Each specifies difficulty, battle-mode weighting, mandatory city,
research, invasion, dimension, battle, save/reload, and ending coverage actions, a declared
full-campaign-victory driver goal, wall/game-time budgets, and driver schema. These are executable
instructions and coverage obligations, not observed acceptance values: R1v contains no expected
terminal bytes, expected ordered milestone sequence, baseline receipt, or permitted-delta oracle.
R1v does freeze expectation-free `milestone_schema_v1`: the closed event vocabulary and types,
canonical JCS ordering, per-recipe required categories/cardinalities/partial order, the stable fields
included in an `acceptance_projection`, and the seed-dependent timestamp/tick/day/count/entity fields
excluded as observations. Its exact root is
`{schema_id,recipe_id,slot_id,derived_seed,terminal,ordered_milestones,coverage}`. `terminal` is
exactly `{classification,ending_asset_id}` and a passing oracle requires
`victory/wingame2.smk`. Every milestone is exactly
`{milestone_id,event_type,stable_subject_id,result_class}` and uses only
`advanced_workshop_construction_started`, `dimension_shifter_started`, `crossed_to_alien_dimension`,
`alien_building_raided`, or `campaign_terminal`; every coverage entry is exactly
`{obligation_id,category,result_class}` over the closed city/research/invasion/dimension/
battle_realtime/battle_turnbased/save_reload/ending categories. Recipe-declared milestone partial
order plus stable milestone ID and recipe-declared obligation order are canonical. Wall/monotonic
timestamps, game ticks/date/day, durations, attempt/process/port/path identity, scheduler/FPS/
controller, runtime entity IDs, raw status/log/screenshot values, and accumulated battle/win/UFO/
recovery/restart counts are observations and may never appear in the projection. Unknown, missing,
extra, duplicate, or retyped fields fail. A variable retained field forces a reviewed R1v schema or
recorder correction and a complete R1p/R1o restart; candidate observations may not silently drop it.
The schema contains no observed value. No recipe is a smoke fragment. A seed
slot is deterministic: interpret the first four bytes of
`SHA256("openapoc-robot-v1:" + recipe_id + ":" + slot_id)` as an unsigned little-endian integer and
replace zero with one. Receipts store the slot, derived nonzero seed, and derivation version.
That derivation deliberately spans the complete unsigned 32-bit domain. Before R1o can launch a
cell, R1a replaces the current signed `ConfigOptionInt`/`ConfigFile::getInt` route for
`OpenApoc.NewFeature.RngSeed` with one checked `uint32` option/accessor path. Its only accepted text
is canonical ASCII decimal in `[0, 4294967295]`: no sign, whitespace, empty value, trailing bytes,
or overflow. Zero retains the existing `SeedRng()` behavior; every value in `[1, 4294967295]` is
used verbatim, losslessly widened to the RNG's `uint64_t` input, and never wrapped through a signed
type. Existing canonical configurations in `[0, 2147483647]` remain source-compatible; rejection
of formerly parseable negative or signed-wrap values is intentional validation tightening. The
registered-option map reports type `uint32` and the exact normalized numeric value. R1b emits the
manifest seed once as canonical decimal and requires `BuildInfo.launch_effective`, the effective
config evidence, and the seed actually consumed by `GameState::startGame` to agree before any
campaign action. That final equality is production-observable: every `GameState::startGame`
invocation appends exactly one process-scoped `rng_seed_consumed` lifecycle record after the seed
branch resolves and before `agentEquipmentTemplates.resize()` or the first RNG-dependent setup.
It carries `{start_game_ordinal, mode, configured_uint32, consumed_uint64}`, where `mode` is exactly
`explicit`, `seed_rng`, or `default_state`; `consumed_uint64` is required for the first two and is
JSON null for `default_state`. An explicit nonzero validation cell requires ordinal 1, mode
`explicit`, and manifest seed = configured value = consumed value. For configured zero, tests prove
the existing `SeedRng=true` wall-clock-seed path or `SeedRng=false` default-state path and never
assert that the consumed seed equals zero. R1b waits for this record, binds its JCS digest and fields
into the cell receipt, and permits no post-start campaign input before that check; missing,
duplicate, late, or mismatched records fail the cell. Boundary tests cover `0`, `1`, `2147483647`,
`2147483648`, and `4294967295`; parser
tests reject `-1`, `+1`, leading/trailing whitespace, empty input, and `4294967296`; all frozen
canaries below must round-trip unchanged. Changing the derivation or clipping its results is
forbidden.
The registry locks derivation canaries: `merge-3` yields `1907306698`, `3375284003`, and
`1396814465` for the first three recipes; `activation-a`/`activation-b` respectively yield
`3421478395/110904055`, `2398816147/3309056633`, `2627168541/3220622441`,
`1154012601/718744052`, and `724564549/263314150` for all five in listed order. Each nightly batch has
exactly twenty execution cells, but its seed-key namespace is phase-specific. Before the fixed
scheduler, execution indices `00..19` use
`nightly-<zero-padded-20-digit-night-ordinal>-<zero-padded-2-digit-execution-index>` and recipe index
`execution_index mod 5`, so every recipe appears four times. After fixed scheduling, key indices
`00..01` use
`nightly-<zero-padded-20-digit-night-ordinal>-key-<zero-padded-2-digit-key-index>` and recipe index
`(night_ordinal + 2*key_index) mod 5`; each key is then executed across ten phase-owned profiles,
whose execution-cell ID appends `-profile-<zero-padded-2-digit-profile-index>`. A mismatch, seed
collision, caller-selected key/profile, or reuse of the pre-fixed slot formula in a fixed phase is
a manifest-generation failure, not permission to choose another seed.

R1v also freezes the expectation-free all-five oracle slot `oracle-v1`; its listed-recipe canaries
are exactly `3606680928`, `3298006273`, `2668641414`, `1501127850`, and `1173335661`. R1o must use
those five seeds, plus the three `merge-3` and ten `activation-a`/`activation-b` seeds above; all 18
key groups must each run three fresh processes, and all 54 campaigns must reach R0's classified
`game_over/victory` result with one byte-identical retained projection per key. Defeat,
timeout, partial progress, parked stage, transport/process failure, or a missing coverage action
blocks the oracle and can never be normalized into an expected result. R1o records an exact
seed-keyed acceptance projection only from the three-replica consensus of those successful
full-victory runs.

Manifest authority has one bounded pre-admission bootstrap. R1v freezes closed-schema,
expectation-free recipe, seed, milestone, merge, nightly, `oracle-calibration-v1`,
`activation-oracle-topology-v1`, and compatibility-delta component-policy artifacts. The activation
topology contains exactly ten `{recipe digest, activation slot, derivation ID, derived seed}` oracle
keys and exactly ten future execution rows per key: legacy REALTIME at 1000; fixed REALTIME at
`0/1/30/60/240`; and fixed MANUAL/STEP at `0/1/60/240`. The row key includes an explicit execution
replica ordinal. The ten oracle keys must
be byte-identical to the 10-key activation partition of `oracle-calibration-v1`, whose remaining
partition is exactly five `oracle-v1` plus three `merge-3` keys. Neither topology nor row may contain outcomes, milestone values,
acceptance projections, filters, skip flags, disabled/optional rows, or unknown fields. The
component policy declares the only CD rows eligible to own an oracle-observable change, gives each
a closed, non-wildcard, non-overlapping set of `milestone_schema_v1` campaign-component IDs, and
freezes the schema for an equally closed owner map embedded in every replay schema before that
schema becomes active. The policy and replay owner maps contain no expected old/new value; an active
replay schema without the map is invalid for admission or compatibility-delta reference work.

Evidence-only R1o is the first authority allowed to attach expected values. Its 54 campaigns
materialize one three-replica consensus for each of 18 keys. Every activation-a/b receipt and
`victory_oracle_v1` bind `activation_oracle_topology_jcs_sha256` and exactly one of its ten oracle
keys. Missing, extra, duplicate, remapped, cross-key, or topology-mismatched evidence invalidates
the complete calibration. The baseline materializes `merge-3-v1` and supplies the exact seed-keyed
oracles later joined into activation; `epoch-init` names its exact digests. R1b may land
schema/runner code but has no expectation-write path.

After the first admission, manifest authority advances only from a live admitted parent, except
for separately typed baseline replacement and compatibility-delta authorization. R1b freezes the
registry schema and runner for later materialized manifests. Before E candidate creation, the
then-admitted parent freezes `nightly-e-pre-f-template-v1`; before F candidate creation, admitted E
freezes `nightly-fplus-template-v1`. Before G candidate creation, admitted F may freeze
`activation-100-template-v1` only as a closed-schema overlay over all 100 R1v execution-row IDs.
The overlay adds concrete launch-profile, protocol-schema, controller-precondition, and budget
digests; it cannot repeat or replace a base field, select a subset, add a row, or contain expected
values. `validate_activation_topology_extension_v1` requires exactly one overlay entry per base row
and none otherwise; composing the overlay then projecting extension fields away must reproduce the
R1v topology byte-for-byte and by RFC-8785 SHA-256. G performs an exactly-one base equijoin from
every execution row to R1o by `oracle_key_jcs_sha256`, then mechanically applies every ordered
active campaign overlay for that key; recipe-only, slot-only, positional, fallback, cross-seed, or
base-only joins fail. F binds the active campaign-chain digest but cannot rewrite it. Changing any recipe digest, activation slot, seed, scheduler/FPS axis,
control-profile assignment, oracle key, or execution row requires a new R1v artifact, unchanged
R1p producer review, complete R1o 54-run calibration, and new epoch initialization; F and G cannot
self-author it.

Automatic REALTIME is deliberately outside that expectation-bearing Cartesian product. F freezes a
bounded diagnostic matrix for city, realtime-battle, and turn-based-battle scenarios at both
schedulers and TargetFPS 0/1/30/60/240. It characterizes legacy frame coupling and must prove fixed
scheduler invariance/deadlines, but no diagnostic row can satisfy an oracle, admission, nightly,
integrated-repeat, activation, or soak campaign.

A candidate may add parser/runner support but cannot write or activate the template, oracle, or
manifest that judges itself. Every merge/activation entry copies recipe/slot/seed, expected
terminal outcome, and seed-keyed acceptance projection byte-for-byte from the active effective
oracle, joins only template-owned execution fields, and names the complete oracle chain; absent,
altered, widened, cross-seed, or candidate-sourced expectations fail generation. A rotating nightly entry names `milestone_schema_v1`, requires
`game_over/victory` and recipe coverage, and carries no copied seed-specific expected value; the
intent instead freezes equality groups whose equivalent FPS/scheduler cells must produce identical
acceptance projections. Replay manifests name the exact replay schema and frozen baseline receipt.

R1b `epoch-init` freezes the `nightly_schedule_v1` schema, RFC-3161 authority identity, and period
of exactly 86,400 UTC seconds; the first `admitted` record atomically instantiates its epoch at that
admission's independently timestamped instant. Ordinal zero is due at the epoch, and deadline
`night_deadline(n)=schedule_epoch+n*86,400s`. Epoch time carries a receipt from the reviewed
RFC-3161 time-authority adapter; unavailable, invalid, backwards, or changed authority blocks
scheduling instead of trusting a caller clock. The due ordinal set at a cutoff is exactly the
gap-free integer range `[0,floor((cutoff-epoch)/86,400s)]`. The coordinator must materialize every
due ordinal in order; if no live admission exists at a deadline, that ordinal remains due and is
backfilled against the first later live admitted head rather than skipped. Ordinal zero therefore
guarantees at least one nightly batch even if the train finishes inside one day. `night_ordinal` is
canonical uint64 rendered as exactly twenty decimal digits; noncanonical text, ordinal or checked
deadline overflow, duplicate ID, or seed collision fails manifest generation.

The completion cutoff is fresh, authority-bound state and the signed instant is the completion
operation's **conditional linearization point**, not merely a stale time observation. After every
non-time completion predicate appears satisfied, `train-completed` acquires the admission-ledger/
landing lock, establishes a durable completion-operation fence, and generates a fresh internal
256-bit nonce. It JCS-encodes a challenge containing the operation ID/fence, nonce, current ledger-
head digest and next ordinal, active scope/schedule/period/epoch digests, live admission ID/SHA/tree,
exact remote `develop` SHA/tree, all non-time proof digests, latest nightly terminal ID, and the
exact completion payload without the still-unknown token. While continuously holding that lock and
fence, it performs one bounded RFC-3161 request and verifies the exact message imprint, nonce,
authority chain/policy, and canonical DER `genTime`. `genTime` is converted once to checked UTC
nanoseconds; fractional precision loss, leap-second ambiguity, or overflow fails closed.

The verified `genTime` is the normative completion cutoff **if and only if** the exact bound
`train-completed` transaction envelope later becomes durable and revalidates under that same
uninterrupted lock/fence and unchanged ledger head/next ordinal. Its due set is exactly every ordinal
whose authenticated deadline is `<= genTime`. If any due ordinal is absent or nonterminal, the
workflow publishes no completion, burns the nonce/token, releases the lock, backfills, and begins a
new operation with a fresh later token. If the envelope commits, the nightly active interval is
defined to have ended at the token's signed instant—even when rename/fsync happens after another
nominal deadline—so no append-time race can add an ordinal behind the validator's back. If crash,
lock/fence loss, payload/head movement, or failed durable publication occurs after token receipt, no
transition ever linearized and the token is permanently unusable; recovery obtains a fresh token,
whose later `genTime` necessarily includes every newly due night. Caller time, append wall time, and
file mtime have no authority. TSA unavailability blocks completion.
`train-completed` additionally requires exactly one terminal intent for every due ordinal, no gap,
every historical logical red resolved, and at least one `nightly-completed` whose intent was created
after the latest transition that can change the current head's effective judging state: its current
live `admitted` record, a campaign-oracle-chain activation, or a replay-schema-chain activation.
That nightly must bind the exact current SHA/tree and both resulting effective-chain digests. This
requirement resets after **every** new admission—including an ordinary final slice such as T-180—and
after every effective campaign/replay transition, not only after recovery. A failed or absent
scheduler invocation cannot make a night cease to be due.

This transition is executed by `validate_train_completion_v1`, not reconstructed from those
sentences. Its record binds the active scope, ordinary-admission spec and validated chain, both
active effective oracle chains, live admission, remote `develop`, the machine-derived required-
repeat landing-set digest plus its exact success-record set, every required-gate terminal, all due-
nightly terminals, and the fresh TSA challenge. The
validator derives the exact transitive slice closure and requires its proof partition to be exact,
disjoint, and exhaustive. The pre-ledger set is exactly
`{Z,P0,P,R0,A,B,R1v,R1a,R1c,R1p,R1o}`; R1b `epoch-init` must bind one immutable
`preledger-ancestry-proof-set` entry per member containing the frozen source SHA/tree, verified
remote landing SHA/tree, review receipt, CI receipt set, and proof that the source head is an
ancestor of the R1b genesis integration head. The land-exact set is exactly
`{R1b,C0,C1,C,C2,D0,D1,D2,D3,D5,E0,E,F,G,P-movement,P-projectile,P-ROF,P-explosion,P-fire,P-MAC,P-physics,P-smoke,P-save,T-calendar,T-animation,T-180}`;
each member requires `land-exact-completed` or `land-exact-reconciled`. No slice may occur in both
sets or outside their exact union. The validator proves remote `develop` equals the unique live
admission. It derives the required repeat set as every landing whose
`integrated-repeat-completed` release digest is cited by a `successor-branch-authorized` record or a
later **non-recovery** candidate/landing, union the exact current live final landing. Recovery cites only its red/
remediation authority, never successor-release authority from the failed landing. Resolved revoked
no-release landings and their reds remain immutable but are excluded; any unresolved revocation
still fails completion. The validator requires one green exact-landing repeat for every member and rejects every open
intent/attempt/claim/reconciliation/delta/schema-upgrade and every unresolved red/parent audit;
proves every admitted delta has one atomic dual-overlay activation; proves due nights are gap-free
and the live head has a green night after its latest admission or effective campaign/replay-chain
transition. It resolves the effective G subject by walking the current final admission's logical-
parent/recovery-remediation ancestry and selecting the unique latest non-revoked G or corrected-G
admission whose exact land terminal binds its ordered four-success set. Exactly one success and zero
failure/open state per gate are required **for that effective admission**. Historical G failures
remain immutable but do not enter current cardinality only when their compound transaction revoked
that historical subject and the current recovery ancestry resolves it; zero or multiple effective
G subjects fails completion. It then permits exactly one `train-completed` and zero
`train-abandoned` records after the cutoff. No caller supplies a slice, gate, repeat, or nightly set.

Successor release is a separate machine transition, not an informal use of nightly evidence. After
R1b and every machine-listed runtime slice through G, the P/T parity line, and T-180 reaches
`land-exact-completed` or `land-exact-reconciled`, the coordinator derives
the phase-appropriate twenty-cell manifest, appends `integrated-repeat-intent`, and binds the exact
live admission, landed `develop` SHA/tree, both active effective oracle chains, runtime launch
template, planned process-launch set, realized-closure receipt set, and input closure,
attempt fence, and all cell IDs. Twenty fresh processes must each reach classified whole-game
victory with schema/recipe coverage; equivalent cells must have byte-identical acceptance
projections. Only common pre-execution infrastructure retry reruns all twenty; after
`attempt-execution-began`, interruption is `integrated-repeat-failed`, logical red, and
atomically revokes the subject/descendants. The canonical post-ledger operation mutex stays owned
from intent creation through terminalization, allowing no overlapping admission, land,
reconciliation, replay upgrade, compatibility delta, parent audit, nightly, G gate, baseline/
lifecycle transition, or train terminal. No later non-recovery runtime landing may begin while a
repeat is open. A non-recovery source branch may be created only after a durable
`successor-branch-authorized` record binds its exact ref/slice/base landing, green repeat, operation
ID, and ledger head; every resulting candidate and landing cites that record. The failed repeat may open only the ledger-derived remediation recovery bound to
that red and its `remediates` set; it never supplies successor-release authority. Only one matching
`integrated-repeat-completed` releases descendant runtime source
branch and later candidate/landing creation. Sibling work branches may coexist only after their
common predecessor's repeat is green, but an authorization is candidate-usable only while its bound
base remains the current unique live landed transport. Once one sibling lands, every other sibling's
old-base authorization and exact-head review are stale before candidate creation. That sibling must
rebase or merge its slice onto the new transport, obtain a fresh exact-head review, wait for the new
landing's repeat, and receive a fresh `successor-branch-authorized` record. The two-sibling sentinel
locks the sequence `C2 -> D0`, rejects `D1_from_C2`, then accepts only D1 rebased onto D0 with final
tree markers `{C2,D0,D1}`. Landings remain serialized. Repeat and nightly never
share evidence: each has a distinct serialized intent, attempt, fence, manifest, and twenty fresh
campaign receipts, with zero receipt reuse even if head and profile coincide. Each success belongs
permanently to one immutable landing; a later landing neither invalidates nor substitutes for it.
`train-completed` binds the resolver digest and exact success-record set for every landing in the
derived required-repeat set.

Each night is materialized before execution under the same admission-ledger lock and canonical
post-ledger operation mutex; its ownership spans intent creation through terminalization and blocks
every other listed operation, which likewise blocks nightly. The coordinator
appends one `nightly-intent` for the next ledger-derived due `night_ordinal`, its exact deadline and
time-authority receipt, and binds the exact live
admitted epoch/ref/SHA/tree, active baseline and replay-schema digests, phase-template ID/digest,
all twenty cell IDs, the exact phase-specific seed-slot and execution-cell IDs described above, all derived seeds,
budgets, and one content-addressed JCS manifest digest. It durably publishes that manifest before
workers start. Workers may consume only the intent-bound manifest and the common
`attempt-started`/fencing state machine above. Common infrastructure failure before
`attempt-execution-began` does not revoke code or close the intent: after proven-dead pre-execution
terminalization, the replacement attempt reruns all twenty cells from zero and any late artifact
from the stale fence is rejected. Any later interruption is `nightly-failed` and cascading red.
Completion appends
exactly one terminal `nightly-completed` or logical-red `nightly-failed` record naming one complete
attempt and all twenty cell receipts; logical red atomically emits its required cascading
revocations. Only those two states normally close the intent, exactly one may exist, and no cell
receipt is mixed across attempts. A different intent
or manifest cannot supersede unfinished evidence, and neither a mutable local clock date nor a
caller-provided ordinal has authority.

The materialized `merge-3-v1` is exactly the first three recipes above with slot `merge-3`, yielding three distinct
nonzero seeds; the mixed recipes guarantee collective realtime/turn-based and standard/high
coverage, and all three run through the full victory recorder. Its execution profile is resolved
from the subject capabilities: legacy/1000/automatic without fixed scheduling,
fixed/60/REALTIME with fixed scheduling but no MANUAL/STEP, and fixed/60/MANUAL-STEP once both
capabilities are active.
Every integrated-repeat batch is exactly the five recipes repeated across twenty distinct
deterministic slots, four per recipe, on the capability-resolved feasible primary profile: legacy
REALTIME/1000 before E, fixed REALTIME/60 from E until F, and fixed MANUAL/STEP/60 from F onward.
Pre-E nightly uses that same five-by-four legacy/1000 topology. E-before-F nightly selects two
recipe/seed keys and runs ten rows per key: legacy REALTIME/1000; fixed REALTIME
0/1/30/60/240; and four fresh fixed-REALTIME replicas at 0/1/60/240 distinguished by execution
replica ordinal. F+ nightly selects two keys and uses the exact activation ten-profile topology.
Profile indexes are assigned canonically by listed rule order, then listed TargetFPS order, then
listed replica-ordinal order, starting at zero. All ten projections for a nightly key must be
byte-identical. Materialized `activation-100-v1` is
the ten activation oracle keys times that exact ten-profile topology. The nightly period begins with the first R1b admission and runs against the current live
admitted head on the exact gap-free `nightly_schedule_v1` over and over until either
`train-completed` proves the exact pre-ledger ancestry set and exact post-ledger land-terminal set,
the current admitted tree, every member of the derived required-repeat landing set, every due night, and the absence of
open intent/red state, or the
train owner appends `train-abandoned`. G landing alone never stops surveillance while a scoped slice
remains; `desktop-presentation-v1`, `desktop-performance-v1`, activation-100, and the soak all block
G through their typed active-scope predicates.

Genesis, every recovery, and every integration merge bind exact `merge-3-v1`; a capacity probe is never
an admission substitute. Merge/activation cells are exact-seed expectation-bearing; nightly cells
are rotating-seed schema/coverage robustness gates and never borrow another seed's values. Every
MANUAL row binds protocol v2, a query-confirmed control-only MainMenu, lease acquisition and zero
automatic pulses before any City/Battle-producing action, then STEP-only simulation; any early game
commit, automatic pulse, lease gap, unexpected outcome/milestone, or process/evidence failure blocks
the batch. Fixed REALTIME rows prove presentation-FPS invariance, and the sole legacy/1000 row gives
a feasible same-head cross-scheduler comparison. Separately named low-FPS legacy diagnostics are
never counted toward nightly-20, integrated-repeat-20, or activation-100.
Merge/activation expected outcomes and acceptance projections always come
byte-for-byte from the matching recipe/slot/seed key in the active reviewed baseline, never the
candidate or a different seed; epoch genesis uses the reviewed R1o parent-engine calibration named
by `epoch-init`, and recovery-root uses
that exact still-valid original immutable receipt or
the complete hash-chained `baseline-invalidated` record followed by its distinct independently
reviewed `baseline-replacement`. It never
recalibrates on the recovery candidate, including a same-SHA baseline-only recovery. A newly rotated
nightly seed is judged only by `game_over/victory`, `milestone_schema_v1`, recipe coverage, and its
intent's cross-configuration equality groups; timestamps, counts, and other excluded fields are
observations. A new recipe, schema, or exact-seed expectation
requires a separately reviewed baseline replacement produced on the current admitted head before
the candidate exists. A batch pass requires
exactly the manifest's requested decided cells/campaigns; missing, duplicate, partial, equivalent-
duplicate, or extra evidence inside that attempt fails it. R1b refuses `git HEAD=unknown`, dirty
source, stale/copied binary mismatch, mutable or unhashed data inputs, and a victory run that has
not traversed the milestone-recording path.

R1a generates a build identity into the executable during configure/build. Immediately after argv
and config parsing, app-bundle/path normalization, and CD validation, but before harness service or
the main loop, it captures one
immutable `BuildInfo.launch_effective` snapshot. The process-invariant read-only query returns
commit/tree/submodule/toolchain/configuration identity; requested TargetFPS separately from the
normalized render-cap mode/value; the launch-selected scheduler implementation separately from
current REALTIME/MANUAL controller state; supported protocol versions and the launch-selected
engine protocol separately from per-connection negotiation;
PID, OS process-start identity, and the exact R1b-provided process-instance nonce echoed by that
process. It also returns every registered option as a lexicographically sorted
`section.key -> {type, normalized_value, provenance}` map plus `effective_options_digest`; no
hand-selected option list can masquerade as the full effective configuration. Canonical JSON uses
RFC 8785/JCS UTF-8 bytes and every digest in this contract is lowercase SHA-256 over those bytes.
`OpenApoc.NewFeature.RngSeed` is represented as type `uint32`; its normalized value is the exact
unsigned decimal value accepted by the engine, including values above `INT_MAX`, and never a
negative or wrapped surrogate.
E extends/tests the scheduler fields and F extends/tests the protocol fields without
changing `BuildInfo`'s process-invariant urgent-read classification. Two simultaneous connections
that negotiate different per-connection protocol modes must receive byte-identical `BuildInfo`;
negotiated/runtime controller state is reported only by connection-scoped protocol commands,
`Status`, or `Lifecycle`. Every repeated `BuildInfo` reply in one process is byte-identical.

The same snapshot contains the requested and canonical effective `Framework.Data` and
`Framework.CD` paths, their option provenance, `Framework.CDPrompt`, and a successful
`cdPathLooksValid` result. R1a adds `Framework.CDPrompt=true` as the compatibility default. With
`CDPrompt=false`, an invalid CD path fails initialization nonzero before harness/main-loop entry and
never calls `pickCdPath()` or saves config; a valid path proceeds without calling the picker. R1b
always sets false, so no interactive or platform picker can alter an admission input.
`Lifecycle <cursor>` returns ordered
journal entries, the next cursor, and the overflow-invalid latch. The bounded journal keys every
entry by transaction, `stageGeneration`, command, lifecycle callback, stage type, and resulting
top-of-stack; overflow latches an explicit invalid-evidence condition and can never silently drop a
required milestone. It also carries the process-scoped `rng_seed_consumed` record defined above;
that record is ordered before the first RNG-dependent game setup and is not inferred from
`BuildInfo`. `Lifecycle` is process-scoped and monotonic, not invariant: each reply is one
point-in-time snapshot taken on the single framework thread. Both commands remain urgent read-only
after E0/E and obey the `process_global_mutation_fence`; E's urgent window never overlaps a
lifecycle barrier or journal mutation. The R1p collector checks out the
exact integration SHA in a fresh isolated worktree, configures an empty build directory, preserves
configure/build logs, and requires the launched process's queried identity to equal the expected
commit/tree/submodules/toolchain/configuration. It hashes every file in the app bundle and
project-shipped runtime closure. Static Mach-O dependency resolution is a preflight cross-check,
not the authority for runtime-loaded images.

`runtime_execution_closure_v1` is a closed schema family with separate pre-execution authority and
post-execution reality; no singular `runtime_closure_jcs_sha256` field exists. R1p owns the family,
evidence-only R1o uses the exact landed implementation to freeze the parent oracle, and R1b invokes
that same ancestor implementation for candidates.

`runtime_launch_template_v1` contains only process-invariant, pre-execution-known state: exact clean
build/static executable closure, registered-option schema, argv/environment/cwd templates and their
closed substitution slots, input-catalog and runtime-image-observation policies, presentation-device
policy, compiler/CMake/submodule identity, build/configure logs, and the closed runtime-invariant
projection schema. It explicitly forbids PID/start identity, process nonce, port, output/HOME/TMPDIR/
cwd instances, actual argv/environment, observed images/device state, terminal seals, or realized
outputs. Directory closure walks regular files in lexicographic UTF-8 relative-path order and records
path, file type, POSIX mode, byte length, SHA-256, and for a symlink its literal link target plus the
separately attested resolved target; sockets/devices are rejected.

Every attempt freezes exactly one `process_launch_plan_authority_v1` discriminated union before
`attempt-started`. Non-soak work selects `finite_exact` and binds one immutable
`planned_process_launch_set_v1`: one canonical row for every initial or save/reload-continuation
process in the recipe process DAG, with exact process-launch ID, parent launch ID/role,
cell/campaign/recipe/slot/seed/profile, and unique nonce/port/output/HOME/TMPDIR/cwd reservations.
An initial row alone binds its pre-known argv substitution and input closure and explicitly forbids a
prior-save campaign root. A continuation row instead names its same-attempt producer, required save
receipt schema, output selector, and frozen relaunch-delta policy. Before execution it is forbidden
from claiming the future save hash, continuation input closure, concrete game-load value, or concrete
argv bytes. Those values become knowable only after the producer's acknowledged save, clean exit,
orderly reap, and receipt.

The 24-hour soak selects `deterministic_gap_free_prefix` and binds a different immutable authority:
`planned_process_launch_prefix_policy_v1`, containing the F-frozen duration, row generator,
transport allocator, separate first campaign/process ordinals, and minimum—but no attempt-specific
origin or cutoff. `soak_execution_window_v1` supplies those only when the attempt actually begins.
Afterward the coordinator maintains two independently gap-free hash chains: one
`campaign-root-planned` entry for exactly the next campaign ordinal before each new root, and one
`process-launch-planned` entry for exactly the next process-launch ordinal before every root or
continuation spawn. A continuation retains its campaign ordinal and consumes only a process-launch
ordinal. Immediately after a root spawn succeeds and its PID/start identity is captured, the
coordinator samples the execution-window monotonic clock and atomically publishes one no-clobber
`soak_campaign_root_start_receipt_v1`; that PID/nonce-bound receipt—not a plan timestamp or later
terminal claim—is the authoritative cutoff and per-campaign drain origin. Prefix append/finalization
and root cutoff admission are serialized. Success or logical failure seals both prefix
digest/count/head triples; root-start receipts and campaign terminals must each biject the campaign
prefix and realized runtime closures must biject the process prefix. Thus a time-bounded soak neither
pretends to know a final process count before execution nor hides reload processes inside campaign
cardinality or lets realized receipts self-describe what should have run.

`attempt-started` and `attempt-execution-began` bind both the invariant template and the immutable
discriminated plan-authority digest. A finite attempt terminal additionally binds its complete
planned-set digest; a soak terminal additionally binds its execution-window and prefix-policy
digests plus both finalized campaign and process prefix digest/count/head triples.
Only afterward may `materialized_process_launch_v1` substitute one authorized row into concrete argv/
environment/cwd immediately before spawn; continuation materialization waits for the producer's
acknowledged save, clean exit, orderly reap, and sealed continuation input closure. Each spawned
process later seals one `realized_runtime_closure_receipt_v1` binding its exact planned row and
materialization, PID/start/nonce, and for soak its campaign's exact root-start receipt, queried
BuildInfo/effective options, observed image journal, device
profile, actual argv/environment/cwd, terminal class, return code, and orderly wait/reap. Full
realized closures are expected to differ. Required equivalence compares only the template's closed
runtime-invariant projection. A success terminal binds an exact bijection between the finite plan or
separately finalized soak prefix and realized receipts. A logical failure binds the immutable partial realized-
evidence set plus every unfulfilled launch ID; it never fabricates a complete set. A pre-execution
infrastructure failure forbids all realized/partial closure evidence, and its retry burns every
reserved launch ID/nonce/port/path before building an entirely fresh set.

The macOS adapter preserves the existing `installCrashHandler()` call as the first executable
statement of `main()`, then immediately initializes process-lifetime preallocated storage for the
bounded append-only runtime-image journal before option parsing, Framework construction, SDL, Qt,
renderer, audio, or plugin selection. It registers the remove callback first, then registers
`_dyld_register_func_for_add_image`; the API's mandatory immediate callback for every already-loaded
image is the **sole canonical initial add set**, followed by future adds. The adapter never performs
the non-thread-safe `_dyld_image_count()`/`_dyld_get_image_*()` index-enumeration family and never
combines a manual snapshot with the mandatory callback sweep.

Both callbacks are allocation-, file-I/O-, logging-, and path-API-free. They copy only checked
`LC_UUID`, supplied header identity, slide, journal sequence, and add/remove kind into preallocated
bounded records; absent/duplicate UUID, malformed load-command bounds, capacity exhaustion, or a
conflicting record immediately latches `invalid_runtime_image_journal`. The callbacks never retain
or later dereference a borrowed path or Mach-O header. At a safe main-thread barrier, the collector
drains additions and resolves a path only for an image still proven active with matching
`{header, slide, LC_UUID}`; it copies the resolved bytes immediately and rechecks identity plus the
ordered remove journal before committing the path, then derives canonical path, slide-independent
identity, system classification, hashes, and code-sign identity. An orphan remove, removal during
registration/resolution, or unload before the post-copy recheck becomes first-class
`unresolved_unloaded` evidence and admission fails; it is never silently omitted or “resolved”
through stale memory. The remove record remains in order and never deletes its addition; a later
load at the same address is distinct by UUID/slide/add sequence. After normal terminal and
ScreenshotService join but before renderer/
plugin destruction, the process seals one nonce/PID/start-identity-bound realized-closure receipt over the
launcher-owned evidence channel. Missing, unsealed, overflowed, unresolved-unloaded, or identity-
mismatched receipts cannot admit.

For every observed non-system image, including Homebrew/package-manager/plugin paths, the collector
records canonical path, mode, size, SHA-256, Mach-O UUID, and code-sign identity at first safe
observation and re-verifies after the seal. Mutation, disappearance, path/UUID/hash disagreement, or
an unapproved dependency root fails. Every statically resolved non-system dependency must appear in
the observed journal; extra late-loaded images remain in the closure and baseline comparison. The
observed set digest—not `otool` output—is `loaded_images_digest`. Fixtures cover the mandatory
existing-image add sweep with no duplicate manual snapshot, remove-before-add registration order,
late `dlopen`, main-thread resolution before unload, removal during registration/resolution and add-
then-unload before drain as typed `unresolved_unloaded`, orphan remove, reload at a reused address,
malformed UUID/header records, journal overflow, and static guards forbidding index enumeration,
path/file/allocation APIs, or deferred borrowed metadata in callbacks.

The manifest also binds one effective `presentation_device_profile` after context/renderer creation
and first successful present. Its immutable `device_context_identity` contains requested/effective
renderer and audio backend, SDL video driver, graphics API/context pixel-format configuration ID
and attributes,
requested/effective swap interval, GPU vendor/device/registry identity, driver-bundle UUID/version,
and renderer/API strings. Its separately typed mutable `window_state` contains SDL window pixel/
color format, display UUID/index/mode, color-space/HDR state, logical/drawable size and scale, focus,
visibility, and minimized state. The immutable context identity retains the graphics-context pixel-
format configuration ID/attributes; only the window/display-following format is mutable. Every display/device
notification and terminal seal re-queries the immutable identity; any drift is fatal. Mutable state
may change only through the ordered transitions explicitly authored by the active validation
manifest, with before/after snapshots and event identity recorded; resize, minimize, restore, focus,
and manifest-authored display movement—including authored window pixel/color/scale changes—are not
misclassified as device drift. The same unmanifested transition, or any context pixel-format, GPU,
driver, renderer, or API change even during an authored move, is fatal. Missing fields, an
unmanifested mutable transition, a dummy/headless driver in a desktop-admission cell, device-mapping
failure, first-present absence, or immutable-identity drift returns
`unsupported_presentation_profile`; it cannot hide under a broad platform label. This is an explicit declared-game-input and executable-dependency closure, not a
claim that v1 observes every incidental file opened by libc, locale, font, display, audio, or other
OS services. Its canonical `coverage` field is exactly `{declared_game_inputs, executable_bundle,
observed_loaded_images, presentation_device_profile, macos_system_profile}` and records
`unobserved_runtime_reads_attested=false`. The profile digest covers immutable identity plus the
canonical authorized-transition specification and observed transition journal, never one mutable
window snapshot falsely labeled immutable. The first admissible adapter is macOS only: immutable Apple system libraries under
`/System/Library` or `/usr/lib` are represented by OS build, dyld shared-cache UUID, resolved Mach-O
UUID, and code-sign identity. Linux and Windows return `unsupported_runtime_closure` and cannot
produce admission evidence until a separately reviewed adapter defines equivalent system-library
closure; platform claims are never inferred from the macOS rule. Data/ISO/mod/starting-save/config
inputs are copied into content-addressed, read-only snapshots before launch and verified before and
after the run; equality checks validate the immutable snapshot and never substitute for it. Mutable
outputs use separate per-cell paths. The coordinator launches from an otherwise empty controlled
cwd and the normalized registered-option map is the authoritative inventory of game Data/CD/config/
mod/load/save roots: any declared game input, path-valued option, symlink resolution, or executable/
library dependency outside the enumerated closure fails admission. Incidental system-service reads
outside those categories are covered only by the recorded platform profile and are never described
as byte-complete or used as an oracle expectation. A future lossless read-audit adapter would be a
versioned `runtime_execution_closure_v2`, not an unstated v1 guarantee.

`target_fps` is a required validation-manifest field and is emitted exactly once in launch argv;
duplicate scalar options are rejected. Each finite planned process-launch row's `initial_state` is exactly `new_game` or
`argv_save`: `new_game` requires an empty `Game.Load`, while `argv_save` requires one immutable
starting-save snapshot and its digest. Every process receives a unique empty output directory, so
the starting save and output save tree can never alias. Validation emits exactly one each of
`--Framework.Data=<canonical-read-only-snapshot>`,
`--Framework.CD=<canonical-read-only-CD-snapshot>`, `--Framework.CDPrompt=0`,
`--Config.File=<read-only-content-addressed-snapshot>`, `--Config.Read=1`, `--Config.Save=0`,
`--Game.Mods=<canonical-ordered-mod-set>`, `--Game.ModPath=<read-only-mod-snapshot>`,
`--Game.Load=<empty-or-read-only-save>`, `--Game.Save.Directory=<unique-empty-output>`,
`--Game.Save.Pack=<manifest-value>`, `--Framework.Harness.Enable=1`,
`--Framework.Harness.Port=<leased-port>`, `--Game.SkipIntro=1`,
`--Framework.AudioBackends=null`, the deterministic RNG seed exactly once as canonical unsigned
decimal, TargetFPS, scheduler, and protocol
selection. If the current option registry spells one of these differently, R1a first freezes the
actual registered spelling and R1b uses that spelling; aliases and duplicate occurrences are
rejected. The first admissible macOS adapter is deny-by-default and retains exactly `PATH`, `PWD`,
`HOME`, `TMPDIR`, `LANG`, `LC_ALL`, `LC_CTYPE`, `USER`, `LOGNAME`, and
`__CF_USER_TEXT_ENCODING`. `PWD` equals the canonical controlled cwd; HOME/TMPDIR are fresh
cell-owned roots; identity/locale values are manifest literals; PATH contains only the recorded
launcher/tool/runtime roots. Every other key is removed. In particular all `DYLD_*`, `LD_*`,
`SDL_*`, `QT_*`, `XDG_*`, `DISPLAY`, `WAYLAND_DISPLAY`, and search/injection overrides are rejected,
not merely hashed. If the reference macOS launch proves another key indispensable, R1p must revise
and independently review a new adapter version before evidence runs. Every retained key/value is
recorded and cross-checked against the observed image journal and `presentation_device_profile`.
All config/mod/save path
overrides outside argv are removed, and admission fails if any registered or configured game-input
path resolves outside its manifest snapshot. Tests inject extra option roots, relative cwd paths,
symlink escapes, config includes, mods, and starting saves and require fail-closed classification;
they do not pretend to observe arbitrary OS-service reads.
The coordinator rejects missing/duplicate input options, pre-validates the exact CD snapshot with
the engine's path contract, rejects any validation launch that can prompt or write its input config,
queries the complete registered-option map plus CD validity after startup, and writes a separate
`effective-config.json` evidence output
instead of relying on engine config write-back. It fails any mismatch. R1a routes every
game-Framework persistence path—including the startup CD-picker path and shutdown—through one
`Config.Save` guard; `Config.Save=0` performs no config create/open/write attempt even when a test
picker returns a valid replacement CD path or initialization later fails. `Config.Save=1` preserves
ordinary persistence. The launcher's explicit user-triggered save is a separate executable contract
and remains functional. A source-scan test rejects any new game-Framework save that bypasses the
guard. R1b verifies snapshot bytes, hash, and mode before/after and writes only its coordinator-owned
`effective-config.json`. It allocates each cell a coordinator-owned port lease held for the cell's
full lifetime, supplies a fresh process-instance nonce at launch, and requires `BuildInfo` to echo
that nonce together with the OS-verified PID/start identity. It never kills a process merely because
its argv mentions a port; cleanup requires all recorded process identities to match. Collision,
stale-PID, spoofed/missing-nonce, and concurrent nightly-20 tests are blocking.

The R1b evidence root is a configured write-once durable volume; a local temporary test root is
marked `admission_capable=false` and can never emit a pass. It contains the authoritative ledger at
`<out>/ledger/records/<ordinal>-<transaction-sha256>.json`. Under one OS file lock, the coordinator reads
and validates the complete chain. For a ref-bearing transition it first verifies the exact remote
immutable-ruleset ID/digest, confirms that the proposed ref is absent, and proves the operator
identity has no update/delete/force/bypass path. It then assigns the next ordinal, creates a
same-directory temporary file
with exclusive creation, writes one RFC-8785 `admission_ledger_transaction.v1` envelope containing
`{transaction_id, operation_id, ordinal, previous_transaction_sha256, logical_records,
logical_record_vector_jcs_sha256}`, `fsync`s the file and directory, and
atomically renames it to its content-addressed final name without replacement. Each ref transition
inside the ordered nonempty logical-record vector contains the transaction's idempotent
`operation_id`, expected ref, SHA, and tree. Candidate publication first commits the exact
`[epoch-init, candidate]` opening vector, then creates the local ref atomically, pushes it
create-only to `origin`, and re-verifies the remote target and same immutable ruleset. A final
`candidate-ref-published` envelope binds the exact provider create receipt before the operation may
release the lock. A crash after the opening vector but before ref publication/verification may
replay that same operation ID only. An absent ref may be created only after full current authority
revalidation. An already-existing exact ref may complete only when the provider receipt proves that
this operation's authorized actor created that ref at the recorded SHA/tree under the recorded
ruleset/no-bypass policy; target equality alone is insufficient. A missing/ambiguous receipt,
mismatched target/rule, source/transport movement, or unauthorized create appends
`candidate-publication-failed` and permanently bars execution of those candidate records. Startup reconstructs
authority solely by enumerating these immutable envelopes and flattening their logical vectors; a mutable convenience pointer may exist
but is never trusted. Any gap, duplicate ordinal, bad previous hash, filename/content mismatch,
invalid envelope/vector/transition, unprotected remote, or pre-existing mismatched ref blocks all mutation. The release operator
invoking the reviewed R1b CLI owns the lock; workers publish attempt artifacts but cannot append
ledger state or create refs.

A one-transition operation uses the same envelope with one logical record. Multi-transition effects
never publish a sequence of files: candidate ledger-open commits `[epoch-init, candidate]`;
baseline-only land commits `[land-exact-intent, land-exact-completed]`; CAS abort opens
reconciliation as `[land-exact-aborted, land-exact-reconcile-intent]`; G admission commits
`[admitted, g-gates-pending]`; an admitted-head robot failure commits
`[selected-failure, revoked]`; reproduced parent audit commits
`[parent-audit-reproduced, revoked]`; an authority invalidation with admitted dependants commits
`[selected-invalidation, revoked]`; failed land reconciliation commits
`[land-exact-reconcile-failed, revoked, land-recovery-authorized]`; and train abandonment atomically
selects its one context-total generic or G-specific vector. G land/reconciliation abandonment may
use the intermediate same-operation `train-abandon-requested` member only inside the exact abort/
reconcile/open vector described above; it can never stand alone as a terminal.
A logical record is referenced by the JCS
digest of its transaction SHA, vector index, and logical-record SHA. Validators expose no logical
prefix: before the no-replace rename zero members exist, and afterward every member exists. A
compound member outside its exact required vector invalidates the ledger. The global operation
mutex remains owned through rename, directory `fsync`, and revalidation; an embedded attempt
terminal never releases it early. A crash before rename exposes no transition and may retry only the
same operation ID; a crash after rename observes the entire idempotent transaction. Startup rejects
temporary/partial visibility, missing compound members, unknown vectors, or bad hashes. Crash-point
tests cover every boundary before write, after file `fsync`, after directory `fsync`, before/after
rename, and before/after post-rename validation.

This append-only, hash-chained ledger's closed logical-record vectors admit `epoch-init`, `candidate`,
`candidate-ref-published`, `candidate-publication-failed`, `admission-intent`,
`attempt-started`, `attempt-execution-began`, `attempt-infrastructure-failed`, `intent-abandoned`,
`admitted`, `admission-failed`, `revoked`, `superseded`,
`baseline-invalidated`, `baseline-replacement`, `train-scope-extended`, `nightly-intent`,
`compatibility-delta-intent`, `compatibility-delta-authorized`, `compatibility-delta-failed`,
`compatibility-delta-invalidated`,
`parent-reference-mismatch`, `parent-audit-intent`, `parent-audit-reproduced`,
`parent-audit-not-reproduced`, `parent-reference-cleared`, `parent-audit-escalated`,
`replay-schema-intent`, `replay-schema-authorized`, `replay-schema-failed`,
`replay-schema-activated`, `replay-schema-invalidated`,
`integrated-repeat-intent`, `integrated-repeat-completed`, `integrated-repeat-failed`,
`successor-branch-authorized`,
`nightly-completed`, `nightly-failed`, `desktop-presentation-intent`,
`desktop-presentation-completed`, `desktop-presentation-failed`, `desktop-performance-intent`,
`desktop-performance-completed`, `desktop-performance-failed`, `activation-intent`,
`activation-completed`, `activation-failed`, `soak-intent`, `soak-completed`, `soak-failed`,
`g-gates-pending`,
`gate-spec-frozen`, `land-exact-intent`, `land-exact-completed`, `land-exact-aborted`,
`land-exact-reconcile-intent`, `land-exact-reconciled`, `land-exact-reconcile-not-applied`,
`land-exact-reconcile-failed`, `land-recovery-authorized`, `land-recovery-reauthorized`, `train-abandon-requested`,
`train-completed`, and `train-abandoned`
states keyed by epoch and tested
code SHA/tree or baseline digest as appropriate. Candidate, attempt, receipt, and admission records
all carry that digest and complete predecessor chain.

Every `epoch-init` binds `ordinary_admission_spec_jcs_sha256` and
`activation_oracle_topology_jcs_sha256`. Before candidate creation, the coordinator derives—never
accepts—the required tier materialization. Genesis and ordinary integration use exactly the three
IDs in `train_scope_v1.contracts.ordinary_admission`; recovery uses their exact set union with every
unresolved tier requirement derived from the ledger. A trigger ID is only a lookup key. Resolution
binds both the active effective campaign-oracle chain and active effective replay-oracle chain, the
complete replay-schema set, both cold-lifecycle routes, and the exact active three-cell merge
manifest. The coordinator sorts entries by ascending UTF-8 bytes of RFC-8785 scalar values in the
exact field order `{requirement_id,tier_id,tier_spec_jcs_sha256,resolved_manifest_jcs_sha256}` and
sorts `resolved_cell_ids` as unique ascending UTF-8 bytes. Two distinct unresolved requirements
never collapse because they share a friendly tier name. It hashes
`{requirement_id, tier_id, tier_spec_jcs_sha256, resolved_manifest_jcs_sha256, resolved_cell_ids}` as
`materialized_required_tiers_jcs_sha256`. No CLI/API accepts a tier list, even if it names the right
three.

Ordinary candidates without a compatibility delta resolve those tiers against the active parent
campaign/replay chains. An authorized delta resolves them against the exact candidate-private
prospective pair, and every `candidate`, intent, attempt, claim, terminal, admit/revoke, land, and
reconciliation surface binds both prospective digests. No caller supplies a chain or profile.
Subject-capability resolution is legacy/1000 before fixed scheduling, fixed-REALTIME/60 with E, and
fixed-MANUAL/60 with F.

`candidate`, `admission-intent`, `attempt-started`, every claim/terminal artifact,
infrastructure failure, `admitted`/`admission-failed`, every later `revoked`, and every land-exact record bind the ordinary spec and
materialization digests. A retry under one intent retains the same digest and reruns every cell
from zero. `admit` requires exactly one successful tier summary for each materialized entry, its
exact cell set from one current attempt/fence, and zero missing, duplicate, extra, failed, open,
stale-baseline, or cross-attempt receipts. `land-exact` re-evaluates it; `train-completed` binds the
active spec digest and the complete validated admission-chain digest. A valid scope extension makes
every older open candidate/intent stale; nothing is grandfathered under a smaller digest. Smoke is
diagnostic, nightly/extended is scheduled surveillance, and desktop-presentation/performance,
activation, and soak all block G.

Appending `baseline-invalidated`
atomically verifies the ledger-computed
dependency closure and, when admissions are affected, places it beside `revoked` in the same
required compound transaction; that transaction seals affected epochs and makes every dependent
candidate permanently unrunnable/unadmittable. Only a separately reviewed `baseline-replacement`
frozen before candidate creation/execution can become active; candidate-derived evidence is an
invalid source. Missing replacement leaves the train blocked.
`superseded` may target only a candidate with no `admitted`, `admission-failed`, `revoked`, or other
logical terminal and with neither a live nor green attempt. It binds the obsolete reviewed source SHA/tree, every terminal attempt ID,
reason `review_head_replaced` or `slice_cancelled`, and any replacement reviewed SHA/tree. It
permanently forbids `run`/`admit` for that candidate, but cannot revoke or resolve evidence, advance
the live tip, close surveillance/presentation/performance/activation/soak intent, satisfy a gate, or authorize
genesis/recovery. A pre-admission logical red is `admission-failed`; a later red against an admitted
candidate is `revoked`; neither may ever be rewritten as `superseded`.

The genesis ledger freezes `authorization-policy-v1`; every later record carries its digest and
verified signer role. Mechanical candidate/attempt/admission/land transitions require
`release_operator`; baseline/oracle proof and each pre-G `gate-spec-frozen` authority require a
distinct `independent_reviewer` receipt; only `train_owner` may append
irreversible `train-abandoned`, referencing a reviewed decision artifact. Inactivity, a crash, a
closed PR, or missed night never implies abandonment. With no open operation, `train-terminal`
proves zero live processes/attempts and appends one terminal envelope. With one open operation,
abandonment is a terminal branch of that **same mutex-owned operation and operation ID**, not a
second overlapping operation: after stopping and orderly reaping every live process, a subject that
crossed `attempt-execution-began` selects its intent-kind logical failure (plus `revoked` when
admitted) and `train-abandoned` in one compound envelope. If the latest attempt legally ended
`attempt-infrastructure-failed` before execution—or the open intent never had an attempt—the exact vector is
`[intent-abandoned, train-abandoned]`. `intent-abandoned` is invalid anywhere else. Thus an open
intent can never deadlock abandonment, and abandoning a live execution cannot become a retry or
optional-stopping path; no retry, resume, or later success may follow `train-abandoned`.
A live but pre-execution non-G attempt first proves the complete no-execution/no-spawn condition,
terminalizes `attempt-infrastructure-failed`, and appends that abandonment vector without releasing
the mutex.

G-pending abandonment is an explicit exhaustive dispatch, never the generic two-record shortcut.
Before the first gate, between every pair of gates, after activation, after soak, or after
`land-exact-reconcile-not-applied`, idle abandonment atomically commits
`[revoked, train-abandoned]`. A gate intent with no attempt or only an already-terminal pre-execution
infrastructure attempt commits `[intent-abandoned, revoked, train-abandoned]`; a still-live attempt
whose no-execution/no-spawn proof passes commits
`[attempt-infrastructure-failed, intent-abandoned, revoked, train-abandoned]`; any gate after
`attempt-execution-began` commits `[selected-current-gate-failure, revoked, train-abandoned]` after
all processes are orderly reaped. Open land is resolved inside the same operation lineage: proven
authorized success commits `[land-exact-completed, train-abandoned]`; every other state commits
`[land-exact-aborted, land-exact-reconcile-intent, train-abandon-requested]`. An already-open
reconciliation receives `train-abandon-requested` under its existing operation ID. Thereafter only
one of `[land-exact-reconciled, train-abandoned]`,
`[land-exact-reconcile-not-applied, revoked, train-abandoned]`, or
`[land-exact-reconcile-failed, revoked, train-abandoned]` may terminalize it; an ordinary
reconciliation terminal is invalid. Selection order is reconciliation, land, gate partitioned by
execution-began, then idle, and exactly one vector must match. Every non-land path closes pending by
revocation; every land-success path closes it by land; every path contains exactly one terminal
`train-abandoned` and retains one continuous operation lineage.

A non-G non-intent operation follows its defined crash recovery before `train-terminal`; it has no
direct abandonment shortcut. For G, the exact land/reconciliation vectors above are that recovery
and terminal branch, so there is never an unlocked gap or permanently pending subject.
`train-completed` is mechanical: it takes no caller-supplied slice set and may
be appended only when the latest digest-bound `train_scope_v1` strict-superset chain is exactly
covered by two disjoint proof sets: R1b `epoch-init`'s immutable
`preledger-ancestry-proof-set` for `{Z,P0,P,R0,A,B,R1v,R1a,R1c,R1p,R1o}`, and a
`land-exact-completed` or `land-exact-reconciled` terminal for every machine-listed slice from R1b
onward. Every pre-ledger entry binds its frozen source SHA/tree, verified remote landing SHA/tree,
review receipt, CI receipts, and ancestry into the R1b genesis integration head. The landed
`develop` SHA/tree must exactly match the live admitted epoch, every applicable exact post-ledger
runtime landing has exactly one immutable green
`integrated-repeat-completed` binding its exact landed head/tree, every typed G predicate in the active scope resolves through its
exact admitted-F `gate-spec-frozen` authority to exactly one success terminal, zero failure
terminals, zero open intents, and zero open attempts for the same candidate/bindings,
the authenticated completion cutoff's complete due-nightly ordinal range is gap-free and terminal
under `nightly_schedule_v1`, the current admitted head has a successful nightly whose intent
postdates its latest admission or effective campaign/replay-chain transition and binds the resulting
chain digests, no intent/attempt is open, and no unresolved red exists. The record binds the active scope/
schedule digests, fresh cutoff challenge/token, derived required slice IDs, due nightly range,
every gate-spec/success digest, and every satisfying receipt; a caller cannot omit or redefine any
of them. Neither terminal state rewrites earlier evidence.
A pre-first-admission genesis candidate names an active reviewed R1o original-or-replacement
baseline as oracle authority instead of a live admitted parent, while exact current remote `develop`
remains transport/first/CAS parent; failed genesis attempts stay immutable but do not consume that exception.
The only later use of that receipt is the separately typed recovery-root transition below.
Every run requires an existing candidate record; every ordinary post-genesis candidate names the
exact live admitted parent. Green receipts precede `admitted`; the candidate need not already be
admitted in order to run. A delayed red revokes the affected admission and all then-live
descendants. The gate refuses another merge into that sealed epoch. Recovery normally starts a new
epoch with the ledger-selected last live admitted ancestor as its logical acceptance parent and the
exact current remote `develop` SHA/tree as its transport parent, even when that transport head is
revoked. The corrected candidate must fast-forward from the transport parent; it is judged against
the logical parent's effective oracles and lands by compare-and-swap from the transport parent. The
flow accepts only the gate-emitted multi-trigger union; after every union-contributed tier and the
three ordinary tiers pass, its admission resolves those IDs for that new head without erasing any
red record or rewriting history.

The sole post-admission no-live-parent transition is `recovery_root=true`. It is valid only when
the complete ledger has prior admissions but zero live admissions after cascading root revocation.
Its `epoch-init` binds the original immutable reviewed root code SHA/tree as oracle provenance plus
the complete active original-or-reviewed-replacement baseline chain; exact current remote `develop`
is separately bound as transport/first/CAS parent. If any invalidation lacks its matching reviewed
replacement, initialization is blocked. The caller supplies only one trigger record ID. The ledger
derives and binds sealed epochs, the exact full unresolved-revocation set, and the union of every
reachable `robot-red {failing_receipt_id, failed_tiers}` and
`baseline-invalidated {invalidation_id, proof_review_receipt_id, affected_tiers}` payload. Its candidate
binds a reviewed recovery R1b/root source commit/tree and copies only the gate-computed `remediates`
set. For a baseline-only union the candidate must equal current transport SHA/tree—not an old
revoked root merely because it supplied provenance; any robot-red member requires
a reviewed code-corrected head with a different tree. It binds the active baseline's complete reviewed/frozen
`replay_schema_set`, manifests, expected hashes, outcomes, and milestones; candidate-derived
recalibration is forbidden. The gate
rejects a subset, superset, descendant target, reused sealed epoch, stale/skipped invalidation,
dependency-closure mismatch, unreviewed or post-candidate replacement, expectation change not
exactly authorized by the active replacement, old-baseline run/admit, or
attempt to label this transition genesis. A green
union-tier-plus-all-ordinary-tier run creates the new root admission. The general genesis exception
remains permanently unavailable.
Outside these explicit recovery flows, another integration merge or dependent-PR creation requires
the exact current tip to have one live admission.

Phase 0 uses explicitly untrusted probes to measure one full-campaign wall time, storage, safe
parallelism, retention, and stop cost. Those measurements size concurrency only: the exact
`merge-3-v1` tier is blocking for the first genesis admission and every later recovery/merge, and
failure to afford it blocks admission rather than reducing cardinality.
F remeasures the same capacity/retention/stop-cost matrix under STEP before G sizes or begins its
100-campaign gate. Paired legacy/fixed execution begins only after E/F exist. A timeout, parked
stage, partial `advance()`, crash, protocol mismatch, transport failure, or unexpected outcome is
always a failure. The robot campaign is the primary cross-subsystem regression oracle; unit tests
remain the prerequisite that makes failures diagnosable.

There is no evidence downgrade if Phase 0 finds the gate unaffordable. Admission remains blocked;
R1b or a separate reviewed throughput/parallelism slice must reduce cost, then rerun capacity and one
complete victory before the train resumes. The plan may reduce duplicate scenario composition only
through a reviewed manifest delta that preserves three whole campaigns per merge, twenty nightly
cells, and the final requested 100 plus 24-hour soak.

### Compatibility pulse matrix

At multiplier 4, one canonical simulation pulse preserves these exact calls:

| Mode | Existing calls per 1/60-second pulse |
|---|---|
| City Pause | no `GameState::update`; the post-attempt dimension decision still runs |
| City Speed1 | alternate none / `GameState::update(1)`, first pulse skipped |
| City Speed2/3/4 | `update(2)` / `update(4)` / `update(6)` |
| City Speed5 | one `updateTurbo()` |
| Battle Pause, display visible | none |
| Battle Speed1/2/3 | `update(1)` / `update(2)` / `update(4)` |
| Battle hidden display, including Pause | four `update(4)` calls, with the existing early-exit checks |

City Speed5 with `canTurbo()==false` changes to Speed1 and applies the Speed1 alternator in that same
pulse. `BUTTON_SPEED5` enablement is control/UI work and never repeats during catch-up.
`BattleView::updateHiddenForm()` is UI plus RNG, not pulse eligibility; a historical pulse that
requires it returns `NeedsControl` and stops catch-up. Catch-up never combines deltas. Serialized
battle interrupt/mission counters remain call counters; the core does not convert them to tick
units.

## 3. Out of scope

- Threaded simulation or concurrent read/write access to `GameState`.
- Interpolation, network lockstep, or simulation beneath a covering modal stage.
- Treating `GameState::update(TICKS_PER_SECOND)` as equivalent to smaller update partitions.
- Claiming `UPDATE_EVERY_TICK == true` parity.
- Enabling 180 TPS in production before collision, save, combat, and mobile gates pass.
- Changing `TICK_SCALE = TICKS_PER_SECOND / 4` or the unrelated quarter-second grenade slider.
- Copying the SDL timer/`locked_queue` implementation from #1270.
- Copying #1237's `TICK_SCALE / 5`: the denominator is the locked unit factor 4, not the tick
  multiplier; any evidenced movement correction changes a separate movement parameter. The `/1.5`
  explosion depletion and other eyeballed constants likewise remain excluded without original-game
  evidence and regression tests.

## 4. Assumptions and decisions

- `[VERIFIED]` The repository's current observational compatibility contract models 36 vanilla
  ticks/second and applies a
  4× resolution scalar to obtain 144. The recovered invasion coefficients establish the
  `1:60:86400` cadence ratios and are consistent with the observational 36-TPS interpretation, but
  those ratios do not independently establish an absolute seconds-per-tick binding. Framework
  timing constants remain separate from gameplay tick constants in `game/state/gametime.h`.
- `[VERIFIED]` The `TICK_SCALE` expression covaries with `TICKS_PER_SECOND`; the effective movement/
  projectile rate it produces is multiplier-invariant. Replacing `/4` with a multiplier or vanilla-
  TPS constant changes that effective behavior.
- `[VERIFIED]` Normal catch-up can repeat existing small pulses without changing GameTime boundary
  behavior. The count-based turbo/calendar commit is not a core prerequisite.
- `[VERIFIED]` The copied StageCmd batch introduced by `56b9b36a8` discards commands intentionally
  queued by lifecycle callbacks such as WeeklyFunding -> ScoreScreen and Notification resume -> POP.
- `[DECISION]` Control and automatic simulation target 60 Hz. Presentation remains default-capped
  at 60 but may be independently capped, vsynced, uncapped, or suppressed.
- `[DECISION]` Only the fixed executor makes TargetFPS presentation-only. The retained legacy
  executor deliberately remains frame/simulation coupled for rollback compatibility; E merely
  makes its zero and negative inputs safe and characterizes uncapped legacy behavior.
- `[DECISION]` Presentation `frameNumber`, the control epoch, and simulation-pulse count are three
  distinct monotonic concepts; no cache or timer may substitute one for another.
- `[DECISION]` Historical catch-up runs before fresh gameplay input. Window close, quit, visibility,
  STEP controller mutations (`ACQUIRE`, `RENEW`, `CANCEL`, `RELEASE`, expiry), and fatal engine
  controls remain immediately serviceable. ACQUIRE/RELEASE/expiry suppress automatic pulses in
  their acceptance iteration.
- `[DECISION]` A simulation pulse's source-tagged causal events are delivered to quiescence before
  any following pulse, with a live StageCmd barrier after delivery. They never share, coalesce with,
  or drop through the fresh external-input queue.
- `[DECISION]` Form emission has two distinct observations of one event object:
  `Control::pushFormEvent()` and `Control::click()` transfer ownership to the queue, then invoke
  registered callbacks synchronously through a borrowed pointer to that same object.
  Callback effects occur at emission inside the current transaction and are never described as
  deferred. Queue ownership pins that object until the outermost synchronous callback stack
  unwinds: no re-entrant queue drain/clear or terminal/lifecycle StageCmd barrier may destroy it,
  and every StageCmd it queues waits for the named post-callback barrier. Nested emissions own and
  pin their distinct objects independently. The object receives its queued-delivery origin/generation tag before admission; without
  an active dispatch envelope that delivery is `control_originated`, waits for the next primary heartbeat, and never
  executes in historical catch-up; nested callback emissions inherit the active envelope for their
  queued deliveries while their callbacks remain synchronous. Admission/cap checks happen before both
  phases so a rejected 4,097th emission cannot still run its callback.
- `[DECISION]` SDL and harness gameplay commands are buffered as fresh input until the primary
  heartbeat. They are never applied in the urgent-service path or replayed through historical
  catch-up. Gameplay action/control/key/button/mouse/text and state-mutating load/save operations
  execute only at a primary consistent-state barrier. Every `Query` command, including all `GS`
  spellings, is primary because the registered namespace contains save and view mutations. Harness
  `Status` and `UiDump`, and only the exact `Action` allowlist `CONTROLS`, `HELP`, and a Forms-parsed
  `CONTROL <id> (item <N>)* get`, are fenced process-global observations.
  `CONTROL ... set get` is a mutation whose value is `get`; a missing operation retains legacy
  `click` and is primary. Terminal quit, safe presentation visibility/resize state, and
  the STEP controller commands above are urgent. After its captured
  `process_global_mutation_fence`,
  Harness Screenshot synchronously reads the most recently completed surface in urgent service;
  legacy SDL PRINTSCREEN reads the same completed surface at primary dispatch. Neither
  waits for a future render. Cursor presentation position follows urgent
  latest mouse motion while the stage-facing event remains primary input. The Phase 4 table
  exhaustively classifies every concrete command; an unclassified command is rejected.
- `[DECISION]` A post-RUN non-controller external mutation classified as interrupting cancels an
  active or queued STEP job before dispatch; observations do not. Automation owns stepping, not
  exclusive process control.
- `[DECISION]` Seeded campaign soaks compare expected outcomes and milestones. Exact state equality
  is claimed only by transcript-driven replay fixtures.
- `[DECISION]` A modal/control-only stage clears simulation debt. Returning never replays modal time.
- `[DECISION]` Only the explicit compatibility-delta ledger may differ from current default-60
  behavior.
- `[DECISION]` When a source prototype conflicts with this plan, this plan wins. The prototype is
  retained as a requirement, test case, rejected-alternative record, or research lead rather than
  imported mechanically.
- `[NEEDS RUNTIME EVIDENCE]` The frozen reference desktop must meet the exact 8-ms control/pulse,
  15-ms deadline-relative opportunity-lag, 16.666-ms urgent, turbo-relative, activation-100-v1,
  and soak gates in Phase 6.
- `[BLOCKED EXTERNALLY]` No mobile-class device is selected yet. PR G cannot claim mobile support
  until a named device/build/configuration receipt exists; desktop activation may document mobile
  as unsupported rather than inventing a measurement.

## 5. Phased plan

### Phase 0 — Trustworthy robot outcomes, baselines, and performance receipts

**Goal:** Preserve R0's truthful terminal semantics, add R1v/R1a/R1c/R1p/R1o/R1b so the whole-game oracle is
executable, candidate-independent, and bound to the exact integrated binary, then lock current behavior and classify every
inherited proposal before refactoring.

**Changes:**

1. In independent PR R0, replace legacy `advance()`'s ambiguous dict return with
   `AdvanceResult(outcome, requested_ticks, start_ticks, target_ticks, end_ticks, stage_before,
   stage_after, transition, status_detail, process_exit, wall_seconds, diagnostic)`. `outcome` is one
   of `reached`, `transition`, `game_over`, `timed_out`, `parked`, `transport_error`,
   `process_exit`, or `protocol_error`; only `reached` credits the complete requested leg.
2. Give tactical runs one typed `BattleResult`; only `resolved` and `lost` are decisions.
   `returned`, `wrong_mode`, `timed_out`, setup/transport/process failure, and every unknown result
   are non-decisions: they enter no battle/Elo/fitness denominator and make finite validation runs
   fail. Score survivors/duration from `Driver.last_battle`, which is captured before debriefing
   destroys `current_battle`.
3. Migrate the exhaustive direct caller set:
   `tools/oa_play.py::play_campaign`, `tools/oa_campaign.py::Campaign.tick`, and
   `tools/oa_adversarial_arena.py::CampaignEvaluator.evaluate`, plus the independent terminal/budget
   logic in `Campaign.run`, `Victory.run`, every sequential/parallel/probe path in `oa_arena.py`, and
   adversarial generation/training paths in `oa_adversarial.py` and `oa_adversarial_arena.py`.
   Victory/defeat comes from validated `Status.detail`, never merely reaching a `VideoScreen`; match
   only the exact normalized engine endings `wingame2.smk` and `lose1.smk`. A source-scan test fails
   if a new unchecked `advance()`, battle result, or process-stop consumer is added.
4. Require exact requested decided-battle cardinality. Every parallel assignment emits one bounded
   worker terminal record, including startup/recovery exceptions; coordinator waits have deadlines,
   and missing/short/failed batches cannot exit zero or evolve a generation. Validate positive
   finite days, legs, budgets, generations, battle counts, seeds, Skirmish force counts, and known
   force-slider names wherever zero is not explicitly documented as forever; a nominally nonempty
   force may not collapse to an empty effective force.
5. Add `--single-campaign`, require an explicit nonzero `--seed` in every validation runner, carry
   it through every process restart, require the manifest's expected outcome/milestones, and exit
   nonzero for timeout, partial advance, parked stage, protocol/transport/process failure, short
   workload, or an unexpected terminal outcome. Capture engine status/return code before cleanup;
   an already-dead/nonzero engine or forced kill vetoes success, scoring, and evolution in every
   receipt, arena, probe, sequential, parallel, and adversarial path.
6. In R1v, repair only the observational `Victory` milestone recorder (the current win path calls
   undefined `Victory.record()`), execute every workshop/shifter/crossing/alien-building and
   `wingame2.smk` milestone path in tests, freeze the five recipe/seed contracts, and prove the
   recorder mutates no engine/game state. Freeze closed-schema `oracle-calibration-v1` with exactly
   three replicas/key, the expectation-free ten-key/100-row `activation-oracle-topology-v1`, and the
   non-wildcard/non-overlapping compatibility-delta component-policy schema for CD-03/CD-12/CD-16
   across campaign projections and active replay expectations. Require every replay schema to freeze
   a closed per-CD component-owner map before activation. Reject every expectation value, optional
   row, filter, unknown field, duplicate/remapped key/row, seed mismatch, missing replay owner map,
   wildcard selector, or overlapping owner. R1v contains no expected outcome or milestone oracle.
7. In R1a, generate a build identity into the executable and add read-only process observability:
   invariant `BuildInfo` and monotonic point-in-time `Lifecycle <cursor>`. Capture
   `BuildInfo.launch_effective` once after argv/config parsing and normalization and before harness
   service/main-loop entry. Path normalization includes app-bundle defaults and CD validation. It
   returns exact build/config/process identity, requested versus normalized TargetFPS mode/value,
   the complete lexicographically sorted registered-option map with normalized value/provenance and
   JCS/SHA-256 digest, requested/canonical effective Framework.Data and Framework.CD paths,
   CDPrompt/validity, launch scheduler/protocol selections, supported capability ranges, and nonce;
   per-connection negotiation and runtime controller state are excluded. Add compatibility-default
   `ConfigOptionUInt32`/`ConfigFile::getUInt32` plumbing and migrate only
   `OpenApoc.NewFeature.RngSeed` to it: canonical decimal `[0, 4294967295]`, zero retaining
   `SeedRng()`, and nonzero values losslessly widening into the RNG. Expose the exact `uint32` type
   and normalized value in `BuildInfo`. Add exactly one `rng_seed_consumed` lifecycle record per
   `GameState::startGame` invocation immediately after the explicit/SeedRng/default branch and
   before the first RNG-dependent setup, with ordinal, mode, configured value, and nullable
   consumed value. R1b waits for and receipt-binds that record before campaign input; explicit
   validation requires manifest/configured/consumed equality, while zero-mode tests prove both
   legacy branches without equating zero to a consumed seed. Lock the five boundary values, every frozen recipe canary, and rejection
   of signed, whitespace, empty, trailing, and overflowing forms. Add compatibility-default
   `Framework.CDPrompt=true`; false makes an invalid CD path fail nonzero without calling the picker
   or saving config. Route both current game-Framework `config().save()` paths through one helper that
   honors `Config.Save`; preserve the separate launcher's explicit user save. `Lifecycle` returns ordered bounded
   transaction/generation entries, next cursor, and the overflow-invalid latch. Add the focused
   `test_admission_observability` target covering query parsing/results, identity mismatch, journal
   order/generation/transaction, overflow invalidation, and an unchanged gameplay transcript.
   STATUS polling is never substituted for ephemeral lifecycle evidence.
8. In tools-only R1c, implement the two mandatory recipe control state machines and raw receipt
schemas that R0 does not provide, using existing engine controls rather than adding an engine API.
R1c owns the fresh-process continuation request contract but neither launches/reaps subprocesses nor
defines the runtime execution closure family; R1p supplies that implementation before any
admissible oracle attempt.
`battle_mode` is an explicit closed
   `rt|tb` recipe field, never inferred from tactical policy. The TB path selects only
   `BUTTON_TURN_BASED`, query-confirms `GS battle mode=tb`, and treats a player-action window as open
   only while live UI reports `BUTTON_ENDTURN visible=1`; static-coordinate fallback is forbidden.
   It issues no player input outside that window, performs one bounded tactical pass inside it,
   calls direct `CONTROL BUTTON_ENDTURN`, observes a complete visible→hidden→visible epoch, handles
   `BattleTurnBasedConfirmBox` with bounded direct `BUTTON_OK`, and ultimately requires
   engine-confirmed `player_won=1`. The raw `battle-mode-coverage-receipt-v1` binds the common
   attempt/claim/fence, recipe/slot/seed, obligation and battle identity, requested/observed TB mode,
   direct-control reply, complete epoch count, popup trace, terminal digest, and satisfied result.
   Realtime, wrong mode, static-click fallback, incomplete epoch, loss/return/timeout, or process/
   transport failure cannot satisfy it.

   The planned reload path uses a unique immutable save distinct from crash recovery. In CityView it
   pauses and requires two equal tick samples, receives synchronous SAVE acknowledgement, and binds
   the nonempty file path/hash/size. Its strict clean stop requires QUIT acknowledgement, observed
   process death, `rc=0`, and orderly wait/reap; terminate, kill, or escalation is forbidden. R1p
   seals the acknowledged output as an attempt-local, read-only `continuation-input-closure-v1`
   under the same fence, then launches a fresh process from the exact same immutable app/binary with
   `--Game.Load` bound to that closure. Evidence mode forbids executable snapshot fallback. Pre/post
   process claims, PID/start identities, nonces, ports, and output roots are distinct; source,
   runtime-image, executable, and bundle digests are equal. The exact relaunch delta set is
   `{Game.Load, Framework.Harness.Port, Game.Save.Directory, process_nonce_transport}`: the load path
   consumes the sealed continuation, while port, empty output root, and nonce transport identify the
   distinct post-reload process. Every other normalized argv, environment, and immutable input field
   is byte-equal. R1a BuildInfo exposes the effective load path; R1p independently proves that path names
   the closure whose saved-input hash equals the save-production receipt.
   settled CityView must report the same city, healthy GS, and `post_ticks >= saved_ticks`, after
   which another campaign action and eventual classified victory are mandatory. The raw
   `planned-save-reload-receipt-v1` binds those facts plus the attempt/claim/fence and recipe identity.
   Crash `restart()`, in-process load, copied progress, mutable-binary fallback, forced/nonzero exit,
   same nonce, wrong/mutated input, unhealthy/wrong-city/tick-regressed state, or double credit after
   recovery cannot satisfy it. Normalize either raw receipt into the acceptance projection only as
   `{obligation_id, category, result_class}`; path, timing, process, battle, and trace fields remain
   raw. Add `tools/test_oa_campaign_controls.py`, both closed receipt schemas, all socket-free red
   cases, and one bounded real-data review probe proving one complete TB epoch and planned reload.
   The probe is non-admissible; only R1o's fenced whole-game attempts create oracle evidence. No C++,
   expected milestone values, or new intent kind belongs in R1c.
9. In R1p, land and independently review the standalone complete normative
   `runtime_execution_closure_v1` template/plan/materialization/realization family before any
   oracle-producing run; R1o and R1b invoke that unchanged landed collector/launcher and cannot
   redefine its schemas. It performs fresh isolated exact-SHA worktree/build, RFC-8785 manifests,
   app/file/symlink closure, a post-crash-handler preallocated journal that registers remove first
   and uses add registration's mandatory existing-image sweep as the canonical initial set, and
   bounded malloc/file/path-API-free add/remove callbacks that own only checked UUID/header/slide/
   order records; orphan removal or unload before/re-during main-thread path resolution is typed
   invalid evidence. It binds exact argv,
   sanitized environment and cwd, full registered-option digest, effective presentation-device
   profile split into immutable device/context identity and manifest-authored mutable window
   transitions with event/terminal revalidation, macOS system-profile identity, and
   content-addressed read-only Data/CD/config/mod/load inputs. Freeze the process-invariant launch
   template, then one attempt-specific finite launch set (or deterministic soak prefix policy),
   materialize each planned row only after `attempt-execution-began`, and seal each actual process's
   realized closure plus the exact success bijection or logical-failure partial/unfulfilled set.
   Emit each required launch option once,
   including Game.Mods, Game.ModPath, Game.Load, Game.Save.Directory/Pack, Harness Enable/Port,
   SkipIntro, null audio, RNG seed, TargetFPS, scheduler, and protocol. New-game versus argv-save
   initial state is explicit; attempt output directories are unique and never alias input saves.
   Verify inputs before/after, keep every output separate, and fail for unknown/dirty source,
   identity/nonce/config mismatch, a declared game-input path outside the immutable snapshots,
   unresolved/mutated/unsealed/overflowed image journal, dummy or drifting desktop presentation,
   or unsupported platform profile. Its expectation-free runner supports one immutable calibration
   attempt containing 18 keys × three fresh-process replicas with no subset/majority/reuse path.
   The first admissible profile is macOS;
   Linux/Windows fail closed until reviewed. R1p contains no oracle expectations, baseline receipts,
   candidate records, or admission code.
10. In evidence-only R1o, materialize all 18 exact recipe/slot/seed keys (`oracle-v1` all five,
   `merge-3` first three, and `activation-a`/`activation-b` all five). Under one separately fenced
   complete attempt, first bind the canonical Cartesian execution-cell manifest containing each key
   once at legacy/240 and once at legacy/1000—exactly 36 unique rows—then run those 36 fresh-process
   victories and require the two cadence-free projections per key to be byte-identical. Only after
   that stability receipt is reviewed may a new complete calibration attempt run exactly three fresh
   processes per key from a second canonical 18-key × replica `{0,1,2}` manifest—exactly 54 unique
   rows—against a fresh build of the exact landed R1p parent with the unchanged R1p
   collector. All 54 campaigns must reach classified victory and satisfy R1v's frozen
   `milestone_schema_v1`; each key's three retained projections must be byte-identical. Every
   manifest row in both phases bijects one initial process launch, one campaign terminal, and one
   retained acceptance projection; every campaign binds its same-attempt/fence/campaign raw
   turn-based and clean-exit/fresh-process-save-reload receipts. Missing, duplicate, remapped,
   cross-phase, or unmanifested evidence fails the whole phase. Every activation receipt binds the
   exact R1v topology/key digest. Publish the immutable seed-keyed
   `victory_oracle_v1` manifest, all three receipt digests per key, one consensus projection per
   key, and external content-addressed evidence. There is no a-priori substitute, majority vote,
   field dropping, or partial retry. Permit only baseline manifest/digest-reference changes
   under `docs/timing/baselines/**`; any executable, tool, config, recipe, schema, or driver delta
   invalidates the evidence and requires reviewed R1v/R1p correction plus a complete 54-run rerun.
   Failure to freeze the complete oracle blocks R1b.
11. In R1b, add separate MainMenu and existing-game lifecycle-journal assertions, immutable
   manifest-bound batch attempts, no-clobber per-cell/per-campaign directories, required single-
   occurrence TargetFPS/config injection and effective-config query, transcript bundles, exact
   cardinality validation, coordinator-owned port/PID/nonce leases, sealed per-epoch Git refs, and
   the concrete locked/content-addressed hash-chained admission ledger. Bind P's JCS
   `train_scope_v1` plus its ordinary-admission spec digest; derive exact tier materialization from
   scope/ledger state with no caller tier input; and bind the materialization and
   activation-topology digests through candidate/attempt/receipt/admit/land/completion. At
   `epoch-init`, import R1p's immutable pre-ledger attempt log and freeze the exact
   `preledger-ancestry-proof-set` for `{Z,P0,P,R0,A,B,R1v,R1a,R1c,R1p,R1o}`; each entry binds source
   SHA/tree, verified remote landing SHA/tree, review and CI receipt sets, and ancestry into the R1b
   genesis integration head. Freeze authenticated gap-free `nightly_schedule_v1`, and use the one
   closed `common-intent-attempt-fence-v1` protocol for both pre-ledger oracle attempts and every
   post-ledger admission, replay-schema upgrade, compatibility delta, parent audit,
   integrated-repeat, nightly, desktop-presentation, desktop-performance, activation, and soak
   attempt. `attempt-infrastructure-failed` is the sole infrastructure terminal and is legal only
   before `attempt-execution-began`; only that pre-execution case restarts the complete immutable
   manifest under a new fence with no partial or cross-fence reuse. Freeze the four typed admitted-F
   `gate-spec-frozen` authorities and evaluate active-scope predicates mechanically. Serialize
   `land-exact` intent/CAS/completion and every logical failure/revocation under the same ledger lock,
   with exact crash recovery, and bind completion to a fresh nonce/current-head RFC-3161 challenge
   obtained under that lock. Include explicit frozen
   oracle/`epoch-init`, atomic `baseline-invalidated`, separately reviewed `baseline-replacement`,
   compatibility-delta intent/authorization/failure/invalidation and overlay activation, and
   recovery-root transitions. Bind every candidate/attempt/receipt/admission to the active
   effective-oracle-chain digest, and exclusively create each attempt root/claim
   before workers start. Test candidate-only invalidation rejection, dependency-closure mismatch,
   old-baseline run/admit rejection, candidate-derived or post-candidate replacement rejection,
   independent replacement success, pre-admission replacement genesis, live-parent replacement
   recovery, zero-live robot-red and external-proof-only recovery-root, ledger-derived multi-trigger
   union/tier/revocation sets, caller-set mismatch, same-SHA baseline-only recovery, missing-replacement hard
   block, active-schema-set recovery after v2/v3, exact caller-independent ordinary-tier
   materialization, scope-extension staleness, mandatory genesis/recovery `merge-3-v1`, exact
   activation base/overlay projection and oracle-key join, three-replica calibration invariance,
   paired delta-reference replay plus 108-campaign isolation; parent drift on a non-merge key or
   replay cell revoking C; reference-only instability burning only the source; unowned/missing/
   extra/retyped/unknown/multiply-owned/wildcard/foreign-CD component rejection; candidate/ref/
   artifact-before-authorization and source/tree mismatch; crash-atomic dual-overlay activation;
   descendant replay/campaign overlay resolution; complete retry with no reuse; authorization
   reuse/invalidation/landing races; repeated
   invalidation, two-coordinator collision, stale-empty root, manifest mismatch, immutable-ruleset
   preflight failure, create-only candidate-ref crash replay, remote tamper, source-head/develop-base/
   merge-result staleness, red-versus-land races and land crash points, stale cutoff rejection,
   ordinary-attempt stale-worker publication, exact landed-head integrated-repeat child blocking,
   cardinality/restart and strict repeat/nightly separation with zero receipt reuse, nightly due-ordinal gaps, presentation/performance/
   activation/soak full-attempt restart, and post-crash non-reuse. Include mutation tests for the
   exact 36/54 Cartesian manifests; shared same-campaign raw-control bijections across candidate,
   repeat, nightly, activation, and soak; recovery-authority chains across multiple generations with
   no sibling or double-consumption path; and distinct retry execution windows plus separate
   campaign/process soak prefixes at cutoff-minus-one, cutoff, drain, and crash boundaries. Add a
   continuous-soak coordinator. Implement
   quiescent-only `journal-v1` replay over one persistent request/reply
   connection; C and F own post-admission calibration/review of the `control-epoch-v2` and
   `manual-step-v3` schema upgrades. One process running several post-defeat campaigns emits
   one exclusive terminal receipt per campaign plus one cell summary, not one overwrite-capable
   receipt per process.
12. In P0, add the base-owned `.github/workflows/planning-scope.yml`,
   `tools/validate_pr_p_scope.py`, and `docs/timing/pr-p-policy-v1.json`, and harden the ordinary
   CMake, lint, and harness workflows. Each ordinary workflow has explicit `contents: read`, a
   full-commit-SHA-pinned checkout, and `persist-credentials: false`. The privileged workflow always
   runs on a GitHub-hosted ephemeral runner with
   only `contents: read`; executes only direct-tagged landed P0 bytes after the inline default-branch
   wrapper proves `GITHUB_WORKFLOW_SHA == pull_request_target GITHUB_SHA == event base == fetched
   develop` and proves all six trust-root paths and the complete
   `.github/workflows/**` path/mode/type/blob-OID tree equal the anchor. The validator fetches the
   immutable event base and `refs/pull/<n>/head` into a second fresh isolated bare repository;
   verifies repository IDs, refs, SHAs, direct annotated trust/receipt tags, receipt-bound remote tag
   OIDs and P0 commit/tree, ancestry, unique required-context markers, the exact one-commit
   base-to-head edge, exact P0 trust-root blob identities, the closed pre-P/post-P state machine, and
   the exact final 22-path tree; and treats every candidate blob as bounded data. It never checks out, imports,
   sources, shells, invokes actions from, builds, tests, caches, submodule-fetches, LFS-filters, or
   otherwise executes the candidate. The ordinary workflows have read-only permissions,
   GitHub-hosted ephemeral execution, full-SHA-pinned checkout with credentials not persisted, and
   no secrets/OIDC/environment/cache trust; the harness's head tests are defense in depth only. P0 itself ships only
   through existing gates plus separate exact-head human and independent review; any later trust-root
   or workflow-tree edit repeats that bootstrap. After P0 lands, install and read back a tag ruleset
   that permits one authorized create but forbids update/delete/admin/bypass; prove both fixed refs
   absent; atomically create-only push the direct annotated trust tag and canonical receipt tag; and
   record their remote object IDs, peeled P0 commit/tree, actor/provider receipt, workflow tree, and
   ruleset in an external append-only receipt. A preexisting, lightweight, nested, moved, deleted,
   mismatched, partially created, or recreated tag is hard red. Next prove native check attachment to
   the exact event head on a sacrificial non-P PR, configure strict/up-to-date expected-source
   protection with no bypass/admin/direct/force/delete path and a unique context, read it back into a
   receipt, and revalidate all receipts immediately before P admission and landing.
13. In P, add `docs/timing/source-disposition.md` with one row for every direct requirement in #1237,
   #1270, #1166, #997, #1216, and #1336, plus a stable supplied 41-site audit mapping and a separate
   table for plan-discovered sites: `adopt`, `replace`, `lock`, `reject`, or `research`. Each row
   carries an immutable source snapshot and
   path/symbol/comment anchor, desired invariant, decision rationale, owner PR/phase, executable
   red test or receipt path, and implementation status. Unavailable Discord/Trello material is
   named as unavailable and cannot silently count as reviewed evidence. Freeze
   `docs/timing/train-scope-v1.json` as the machine-readable slice/dependency/completion registry;
   later scope changes may only append a separately reviewed strict superset. Add
   `source-disposition-map-v1.json` and `test_timing_source_disposition.py` so every frozen body,
   issue comment, file, and patch hunk in #1166/#1237/#1270 maps to an existing decision row, with
   exact count and zero-review assertions. Add the untrusted `pr-p-scope-v1.json` claim and
   `test_pr_p_scope.py` defense-in-depth test, but give neither admission authority. The complete
   tracked-plus-untracked P diff against the landed P0 base is one exact commit adding the 22 planning artifacts
   frozen by P0; P modifies no workflow, base-owned validator, canonical policy, engine, game,
   forms, CMake, robot launcher, runtime-closure collector, candidate, admission, or landing code.
14. Record legacy executor-order—including pre-resync lateness, the strict resync threshold, and the
   current unreachable-warning count—pulse-matrix, Speed1, hidden-battle, mission/interrupt, terminal,
   modal, city, realtime battle, turn-based battle, turbo, video, loading, old-save, pulse-originated
   event, cursor, screenshot, and initialization-failure transcripts. C owns the extracted-
   transaction/causal-event fixtures; C0 owns legacy pacing/resync fake-clock fixtures, and E owns
   fixed-scheduler catch-up/debt fake-clock fixtures.
15. Record quiescent city/battle, video, loading, and old-save replays plus seeded campaign-soak
   baselines with expected outcomes/milestones. Genesis freezes R1b `journal-v1`. Judge C with v1,
   then calibrate/review `control-epoch-v2` on admitted C before its descendants. Judge F with v2,
   then calibrate/review `manual-step-v3` on admitted F before its descendants; only v3 claims exact
   running city, realtime battle, turn-based battle, and turbo pulse/hash equality.
16. Benchmark late-campaign desktop p50/p95/p99/p99.9 pulse, control, deadline-relative
   opportunity-completion lag, urgent-service latency, bounded tail fractions, and hard ceilings
   on the frozen reference host. Record #997's 5-ms non-desktop target, but mark mobile as
   `unsupported_unmeasured` until its separate named hardware receipt; it does not block the scoped
   desktop G.
17. Add original-game evidence requirements for walking, projectiles, rate of fire, explosions,
   smoke, and MAC behavior. Eyeballing is not an oracle.
18. Establish legacy receipts and measure runner capacity/retention in Phase 0. Add paired
   legacy/fixed execution only after E/F make fixed MANUAL STEP runs available.

**Done when:** R0 proves a timeout/non-decision/short or zero generation/invalid force or workload/
late engine failure cannot pass, score, or evolve; R1v proves the observation-only victory path and
freezes candidate-independent recipes, exact-three oracle calibration, the expectation-free
ten-key/100-row activation topology, and closed delta-component policy without expectations; P
freezes the machine-readable train scope and all eight typed contracts, including the common
attempt/fence protocol, exact-landing twenty-victory release fence, and gap-free nightly-20
schedule; R1c proves the mandatory turn-based action epoch and planned save/clean-exit/fresh-process
reload/continued-victory controls are executable with closed raw receipts; R1p proves the
independently reviewed collector; evidence-only R1o freezes all 18 exact-parent seed keys only after
the separate 36-victory legacy-240/1000 projection-stability probe and three stable calibration
victories each (54 more campaigns), and binds all ten activation keys to the frozen topology;
R1a proves full-domain unsigned seed parsing plus an ordered one-shot `rng_seed_consumed` record,
and R1b proves exact manifest/launch/BuildInfo/record/receipt equality for every frozen nonzero
canary before campaign input, caller-independent ordinary-tier materialization/digest propagation,
and activation overlay round-trip/oracle-key joining; R1a/R1b prove exact
source/tree/build/launch-template/planned-process-set/realized-process-set/input binding, both cold lifecycle-journal assertions, transcript/batch/soak
cardinality, common stale-worker fencing, a gap-free immediate-first nightly schedule, and at least one full victory; every caller handles every terminal result; all truth
tests run in CI; every inherited proposal has an explicit disposition; legacy transcripts are
green; and benchmark hardware/save/seed/build configuration plus runner capacity are recorded.

**Rollback:** R0 remains frozen. Revert R1b before descendants, then R1o, then R1p, then R1c, and
finally R1a or R1v only if no descendant remains. R1o's evidence and every R1b/integration receipt stay
append-only and are never rewritten or parsed as the older ambiguous format. Any downstream
integration receipt depending on any reverted slice is revoked and must be
regenerated; no engine save format changes are involved.

### Phase 1 — Restore bounded live-FIFO StageCmd transactions

**Goal:** Restore intentional lifecycle command chaining without iteration invalidation or hangs.

**Changes:**

1. Red tests first in a new focused framework test: commands appended by `pause()`, `begin()`,
   `finish()`, and `resume()` execute in the same FIFO transaction.
2. Extract `Framework::drainStageCommands()` from `Framework::run()`.
3. Pop and apply the live front until empty. Mark every stack mutation with a monotonically
   increasing `stageGeneration`.
4. Treat `REPLACEALL` as an authority boundary: preserve the commands already behind it, suppress
   commands emitted while doomed stages finish/resume, push the replacement, then run the
   pre-existing tail before commands emitted by the replacement's `begin()`.
5. Enter the initial stage through the same live transaction and drain commands from its `begin()`
   before any first update.
6. Cap one barrier at exactly 64 accepted commands. “Accepted” means dequeued for execution and
   includes `CONTINUE`, every stack command, `QUIT`, and the initial synthetic `PUSH`; callbacks
   suppressed during terminal/authority teardown are never dequeued and do not count. Before
   dequeuing command 65, overflow wins even if that command is `QUIT`. Overflow logs a fatal
   scheduler error, clears pending and teardown-generated commands, terminates the process with
   failure, and renders nothing.
7. Explicit `QUIT` is a successful terminal result, while overflow is a process failure. Both
   discard pending and teardown-generated commands. Empty-stack success is evaluated only after the
   live queue drains, so a later accepted `PUSH` may recover from an earlier `POP` to empty. A still-
   empty stack returns success and renders nothing. Framework initialization failure is distinct
   from requested quit, returns failure cleanly, and renders nothing. Rendering while commands
   remain is forbidden.

Normative StageCmd truth table:

| Command | Lifecycle/order | Commands emitted by callbacks | Terminal result |
|---|---|---|---|
| `CONTINUE` | no stack mutation; still consumes one of 64 | join the live tail normally | continue |
| `PUSH S` | current `pause()` → push/generation++ → `S.begin()` | append after the tail that already existed | continue |
| `POP` | current `finish()` → pop/generation++ → exposed `resume()`; continue draining even if temporarily empty | append after the tail that already existed; a later tail `PUSH` may repopulate | after the entire live tail drains, continue if repopulated; otherwise success, no render |
| `REPLACE S` | exact existing composition: `POP` then `PUSH S`, including exposed-stage `resume()` then `pause()` | all lifecycle commands append after the pre-existing tail; no authority suppression | continue |
| `REPLACEALL S` | snapshot the not-yet-dequeued pre-existing FIFO tail; repeatedly pop the old stack, preserving each `finish()` and each newly exposed stage's `resume()` even though that stage will also be popped, while suppressing every teardown emission; push/begin `S`; restore queue order as saved tail then `S.begin()` emissions | every finish/resume teardown emission is discarded; replacement-begin emissions run after the saved tail | continue |
| nested `REPLACEALL` in saved tail | apply the same rule when reached; the prior replacement is cleared | commands already queued by the prior replacement's `begin()` are now part of the nested command's saved tail and remain ordered before the nested replacement's `begin()` emissions | continue |
| `QUIT` in positions 1–64 | discard pending tail; clear stack; discard teardown emissions | none escape | success, no render |
| any command in position 65 | do not execute it; discard pending tail; clear stack; discard teardown emissions | none escape | overflow failure, no render |

Null/mismatched replacement stages are accepted only as testable command values, validated before
any lifecycle dereference in the drain, and terminate with typed `Invalid` failure/no render. The
initial synthetic `PUSH` follows the same table, so a pathological initial `begin()` chain can also
hit the 64-command bound. `Framework::run()` maps explicit QUIT/empty-stack to `true` and
overflow/invalid/initialization failure to `false`; `main()` maps that boolean to process success or
failure.

**Done when:** WeeklyFunding-style `finish() -> PUSH`, Notification/SelectForces-style
`resume() -> POP`, every row above (including `REPLACE`, nested `REPLACEALL`, null replacement,
initial-command accounting, `QUIT` at 64, and `QUIT` at 65), FIFO ordering, doomed-stage
suppression, POP-empty-then-tail-PUSH recovery, forced initialization failure, and process
exit/render suppression all pass through production-owned Framework transaction/process seams. B
ships on those focused locks and its ordinary local/CI gates; a pre-R1b cold-Skirmish run is
diagnostic only and cannot satisfy its ship gate. Runtime evidence becomes admissible only when the
exact R1b genesis integration candidate containing the reviewed R1b source produces both ordered lifecycle-journal assertions, reaches a battle, proves
source/build/launch-template/planned-process-set/realized-process-set identity, and emits a green
terminal receipt. Z/R0 without B remain
expected red at the named begin-PUSH/resume-POP boundaries; B without R1a has green focused code
tests but no exact journal, and R1a without R1b has a diagnostic journal but no admissible build/
campaign receipt.

**Rollback:** Revert this isolated behavior-fix commit. No serialized state changes.

### Phase 1B — Introduce the monotonic-clock seam

**Goal:** Land the deterministic timing dependency before any presentation or scheduler slice needs
it, without changing legacy cadence.

**Changes:**

1. C0 lands three separate production pieces: an injectable `MonotonicClock::now()` backed by
   `std::chrono::steady_clock`; a pure
   `LegacyFramePacer::decide(state, now, positive_period)` returning `WorkNow` or `WaitUntil` plus
   exact next state/raw lateness/resync; and `LoopWaiter::waitUntil(deadline, wake_set)`. The
   production waiter preserves today's sleep/continue behavior, while its fake records the request
   and advances only under explicit test control. Clock reads and blocking are never hidden inside
   the pure pacer. E reuses these exact clock/pacer value types and upgrades the waiter to the
   readiness contract above; it introduces neither another clock nor another deadline policy.
2. Lock legacy pacing precisely: the pacer captures `expectedOld`, computes
   `expectedNext=expectedOld+D`, and applies strict `now > expectedOld+6D`. One duration tick below,
   exactly equal, and one tick above prove equality does not resync; `+1` rebases to exactly `now+D`
   and the following decision requests that wait. Characterize the existing warning as unreachable
   because its identical predicate is rechecked only after rebase. Keep wall-clock-only logging
   conversions outside scheduler decisions.
3. C0's forbidden-clock scan is prospective and explicit. It removes direct Framework scheduler/
   profiler clock reads and sleeps outside the production clock/waiter. Exactly two pre-existing
   reads are allowlisted and may not grow: `game/ui/general/videoscreen.{h,cpp}`
   `high_resolution_clock` playback state is temporary debt removed by D1; `framework/jukebox.cpp`
   `system_clock` seeds shuffle RNG and is not a scheduler/deadline clock. No other Stage may add a
   direct clock. D0/D1/D2/D3/D5, E, and F consume the landed seam.

**Done when:** Production legacy TargetFPS pacing, strict-threshold resynchronization, zero-warning
count, frame-limit, and Stage ordering goldens are unchanged. The fake waiter sees the exact
requested deadline with no implicit time advance; early/spurious wake re-enters the pacer without
work; below/equal/+1 and the following wait are exact. A source scan proves the exact two-entry
allowlist, and D1 makes the Video entry disappear.

**Rollback:** Revert C0 before any descendant lands. No save, protocol, or gameplay state changes.

### Phase 1C — Prove the UI timing test substrate

**Goal:** Make every City, Battle, Form, and Video contract named below compile and run in CI before
the extraction depends on it.

**Changes:**

1. Add a dedicated timing-test target linked to `OpenApoc_Forms` and `OpenApoc_GameUI` in addition
   to Library/Framework/GameState. Do not broaden every existing test's link graph.
2. Add a minimal `TestRenderer` implementing the abstract renderer without GL/window creation, use
   the existing null sound backend, and provide programmatic Form/Control/RadioButton fixtures that
   require no XML or proprietary game data.
3. Land the production-owned value types and side-effect-free policies exercised by the test—not
   test-only mirrors: `PulseOrigin`, `ControlDecision`, `PulseResult` with explicit accounting
   fields, `DimensionRecheckContext`, City speed/Speed1/turbo/dimension decisions, Battle preflight/
   ordering decisions, the City refresh observer contract, and Video playback-state transitions.
   They accept immutable snapshots and return typed plans/effects; they do not call Stage, forms,
   GameState mutation, audio, renderer, or StageCmd, and C1 changes no runtime path. C later wires
   City/Battle adapters to these exact types/functions; D1 wires Video to the exact playback policy.
   Adapters may not duplicate or redefine policy or types. Drive them with C0's fake clock and
   explicit fake game-time/form/video sinks. The
   dedicated target proves the pure refresh controller cannot request a pulse; the exact-binary R1b
   fixture must additionally construct the real `CityView`, enumerate all fourteen registrations,
   and observe the actual `skipSpeed1Tick` and GameTime state. Other real `CityView`/`BattleView`
   glue remains covered by R1b stage transcripts and per-head robot runs.
4. Add an explicit test-data rule: CI fixtures may use generated objects and repository-owned data
   only. ISO/CD-derived or mutable extracted data is runtime evidence, never an undeclared unit-test
   prerequisite.
5. Prove the substrate before C starts with one queue-before-callback Form test, one radio-group
   ordering test, one no-frame/EOF Video test, one pure City refresh-controller no-pulse test, and
   one City/Battle decision-fixture link/run test. Measure build and runtime cost and keep this
   target independently runnable. R1b separately proves that its exact-binary City fixture can
   select each real registration and read both alternator state and GameTime without proprietary
   data leaking into the unit-test target.

**Done when:** A clean CI checkout builds and runs the dedicated target without display, audio,
ISO, or mutable user data; deliberate defects in callback order, video state, and the production
decision policy turn it red; a source/ODR check proves no shadow timing policy/type exists. Later
acceptance rows name either this target or an R1b exact-binary receipt rather than assuming an
unbuildable full-view unit fixture.

**Rollback:** Revert C1 before C/D1 descendants exist. It changes test support and pure seams only,
not saves, gameplay, or protocol behavior.

### Phase 2A — Parity-preserving Stage/simulation capability (C)

**Goal:** Make City and Battle the only simulation-capable stages while preserving the legacy
frame-coupled executor.

**Changes:**

1. In `framework/stage.h`, retain the complete existing `Stage` lifecycle and `Stage::update()` for
   ordinary control-only stages. Add an RTTI-free optional `Stage::simulationHooks()` returning a
   borrowed `SimulationStage*`; only City and Battle return one. The pointer is valid only while
   `stageGeneration` is unchanged and is reacquired after every StageCmd barrier. A null hook is an
   explicit control-only result: automatic simulation is ineligible, whole debt is rebased/dropped
   with its named reason, and neither historical nor primary code may manufacture a pulse or an
   `unsupported` attempt merely because wall time elapsed.
2. Define a closed runtime API that consumes C1's already-landed value types and policies rather
   than redefining them:
   `beginControlHeartbeat()` returns a typed `ControlDecision`,
   `advanceSimulationPulse(PulseOrigin)` executes one atomic simulation transaction and returns a
   typed `PulseResult`, and
   `finishControlHeartbeat(ControlDecision, optional<PulseResult>, skip_remaining)` is called on the
   same object after every typed return from begin, including `terminal_success` and `fatal`.
   Returning a `ControlDecision` means begin completed and established the pair; only a Framework
   failure before method entry (null/stale stage after reacquisition) has no finish half.
   `skip_remaining` is the OR of the begin and pulse results, so an early return discovered inside
   the pulse suppresses ordinary post-control correctly. Terminal begin results receive a no-pulse,
   cleanup-only finish and then exit without render. There is no StageCmd barrier between the
   control halves. `StageCmd`s queue normally and apply only at the closing barrier.
3. One simulation transaction contains only simulation-coupled eligibility, the exact legacy
   `GameState::update`/`updateTurbo` partition, and causal state finalization. It never updates
   forms, audio, camera, presentation timers, controls, or presentation RNG; it may only mark typed
   control-dirty state that finish-control later applies. `PulseResult` is exactly one of
   `advanced`, `skipped_speed1`, `paused`, `needs_control`, `transition_before_pulse`,
   `transition_after_pulse`, `game_over_before_pulse`, `game_over_after_pulse`, `covered`,
   `unsupported`, or `fatal`. Every variant states `attempt_consumed`, `simulation_advanced`,
   `ticks_advanced`, and `skip_remaining`; STEP count exhaustion uses `attempt_consumed`, never
   inferred tick advancement. A returned
   `fatal` terminates the framework with failure, suppresses queued non-terminal work and render,
   and is never retried; a primary pair still invokes its same-object finish half as a no-op cleanup
   before terminalization, while a historical pulse has no control pair to finish.
4. Extract City speed selection, Speed1 alternator, turbo/fallback, exact update calls, and
   dimension-transition detection without pretending the current `setUpdateSpeed()` mutates model
   state: today it sets `lastSpeed`, changes a radio, and relies on that radio's synchronous
   `CheckBoxSelected` callback to assign `updateSpeed`. Replace it with three explicit operations.
   Programmatic `requestUpdateSpeed()` retains the current model-equality guard: a redundant request
   changes neither `lastSpeed` nor `updateSpeed`; an actual change copies the old speed to
   `lastSpeed`, updates `updateSpeed`, and marks the radio dirty. A user-radio callback assigns
   `updateSpeed` without changing `lastSpeed`, preserving
   current mouse semantics; `syncUpdateSpeedRadio()` ignores model equality as a reason to skip UI
   synchronization and selects the matching radio during control. Its synchronous callback observes
   the already-selected model value and is an exact same-state no-op. A pulse may call only the
   direct model operation and mark dirty state; it never dereferences a form or constructs a
   `CityView`. Preserve both legacy fallback orders explicitly: if Speed5 is already unavailable
   before the pulse, begin-control performs the programmatic fallback and radio sync before writing
   `BUTTON_SPEED5::Enabled=false`, then the pulse uses Speed1; if turbo becomes unavailable because
   of the pulse, finish-control applies the dirty radio sync after the pre-pulse enable write.

   Dimension eligibility is primary-control reconciliation that runs after the optional pulse and
   also when Pause or the Speed1 alternator executes zero ticks; MANUAL idle therefore still performs
   the alien-city return check. Preserve the source's exact two-level predicate. Compute
   `naturalSwitch` as follows: in the human city it is true only when `preDay != liveDay` and a
   player-owned vehicle is in the other city; in `CITYMAP_ALIEN`, this day branch is overwritten,
   `naturalSwitch` begins true, and becomes false if any player vehicle is in the current alien
   city, any alien-owned vehicle is falling (in either city), or the current city has projectiles.
   Then compute `outerTransition = DEBUG_SHOW_ALIEN ? currentCity != CITYMAP_ALIEN : naturalSwitch`.
   Thus debug-on while already alien suppresses even a true natural switch, while debug-on in the
   human city can request a transition with either value of `naturalSwitch`.

   C extracts a side-effect-free `DimensionRecheckContext` policy carrying only pre-pulse day,
   observed post-pulse day, attempt result, and `stageGeneration`, but the compatibility-preserving
   legacy executor does not yet submit historical/job pulses or alter event/barrier order. Its one
   primary pair evaluates the policy at the same source-equivalent point as today's mixed update.
   C1/C tests lock the future historical result and fresh-state recomputation without activating it:
   `observedPostDay` is evidence-only, stale generation fails, and final `naturalSwitch` plus
   `outerTransition` are recomputed from `context.preDay`, current day, current city, live
   `DEBUG_SHOW_ALIEN`, and every current vehicle/projectile guard. C2 alone activates the historical
   `needs_control(reason=dimension_recheck, context=...)` result and next-primary suppression after
   its compatibility-delta authorization. Restoration to `preDay` suppresses only the human-city
   day branch; the alien/debug branches still evaluate normally.

   If `outerTransition` remains true, one uninterrupted finish transaction performs the exact
   order: request Speed1 and synchronize its dirty radio state; choose the first other city; mutate
   through `setCurrentCity()` when recomputed `naturalSwitch` is true, otherwise assign
   `current_city` directly so a pure debug warp does not unlock research; construct the replacement
   `CityView`; copy live `DEBUG_SHOW_ALIEN`; queue `REPLACEALL`; and skip `CityTileView::update()` plus
   ordinary remaining control. A debug-on human-city transition with `naturalSwitch=true` therefore
   intentionally uses `setCurrentCity`, while one with `naturalSwitch=false` uses raw assignment.
   On the ordinary
   non-transition path, `CityTileView::update()` remains in City
   finish-control after the optional pulse, preserving its opposite ordering from Battle's parent
   update. The dormant historical policy never mutates forms or constructs views.
5. Split Battle at its actual ordering boundaries. Parent/debug, hotseat dialog creation,
   turn-tab/confirmation UI, and hidden-form RNG live in begin-control; exact tick dispatch and
   call-counted battle updates live in the pulse; action music, selection, camera, forms, and the
   mission-end dialog check remain in finish-control. A pure pulse preflight returns
   `needs_control` instead of calling `updateHiddenForm()` or opening a hotseat dialog. C locks this
   return path as a dormant policy seam; C2 activates it for historical catch-up so a pulse that
   makes a mission/control transition due stops before another pulse and the next primary pair
   handles it.
6. `ControlDecision` is exactly one of `proceed`, `control_only`, `skip_remaining`,
   `terminal_success`, or `fatal`; it carries a reason and whether simulation
   is permitted. Every returned decision is a completed begin and therefore receives exactly one
   finish call on the same stage generation. Finish performs only same-heartbeat cleanup when either
   decision or pulse sets `skip_remaining`; `terminal_success` and begin-originated `fatal` pass no
   pulse result, force `skip_remaining`, finish once, and then exit success/failure without render.
   Pulse-originated `fatal` follows the same paired cleanup rule. No behavior is inferred merely
   from a queued StageCmd.
7. Add source tags to the existing framework event envelopes without changing C's single-queue
   delivery/barrier behavior. Events emitted during a
   simulation pulse are `simulation_causal`; SDL/harness input is `fresh_external`; timer/audio/
   unknown-thread producers are `async_control`; events emitted by begin-control, finish-control,
   ordinary control-only `Stage::update()`, or `Form::update()` without an active dispatch envelope
   are `control_originated`. A form emission has one event object with queued ownership and a
   borrowed callback view of that same object; origin/generation metadata is attached before queue
   admission, then registered callbacks run synchronously and may mutate the object the Stage later
   observes. Queue ownership pins the object through the outermost callback stack; re-entrant
   drains/clears and StageCmd barriers are forbidden until that stack unwinds, and nested event
   objects are pinned independently. Before callback iteration, `Control` snapshots the selected
   callback sequence in registration order and holds `shared_from_this()` through completion; the
   outermost form-dispatch scope also retains the originating committed Stage when one exists,
   independently of the mutable event `RaisedBy`. A callback added to or removed from that same
   event type during dispatch affects the next emission only; clearing `RaisedBy` or destroying/
   removing every external Control/Stage owner cannot invalidate `this`, the originating Stage, or
   the remaining snapshot callbacks. Nested emissions receive independent event, Control, Stage,
   and callback-snapshot pins. These labels apply only to its queued Stage-facing delivery: callbacks already ran
   synchronously at emission and their nested callback or
   StageCmd effects are part of the current transaction. Re-entrant queued deliveries of that same
   event object inherit the envelope currently being dispatched, which takes precedence over
   ambient pulse/control origin. C retains one bounded queue and the current closing StageCmd
   barrier: tags are diagnostic/provenance only. A 4,097th event in one primary control-origin chain
   is rejected fatally before enqueue or synchronous callback dispatch rather than silently losing
   its Stage delivery or leaking callback state. The existing unknown/default form-event branch
   becomes a typed rejection and never calls callbacks with a null event. C2 alone turns
   `simulation_causal` into a separate quiescent lane and moves its barrier before a following
   pulse.
8. Under the legacy scheduler C's exact executor is: dispatch the existing queued deliveries and
   their re-entrant form events → begin-control → at most one primary pulse → paired finish-control
   → one live StageCmd barrier → render. An event emitted by pulse N remains queued until the next
   transaction; when its Stage delivery queues a StageCmd, that command still applies only at the
   closing barrier after the outgoing stage receives pulse N+1. This intentionally retains CD-16's
   characterized one-extra-pulse behavior. For an ordinary stage, its existing `update()` is the
   complete control transaction. C remains frame-coupled and activates no compatibility overlay.
9. Retain all fourteen City refresh callback registrations on their characterized mixed-update
   path in C. C1's pure no-pulse helper and the exact-binary red fixture exist, but C does not wire
   them; C2 owns that deliberate CD-03 behavior change.
10. Rename `vanillaCitySpeed1Ticks` to pulse-unit terminology and lock exact phase behavior: the
   first Speed1 attempt skips, Pause preserves the existing alternator phase, a replacement
   `CityView` resets it so the first Speed1 attempt in the new dimension skips again, and legacy/
   fixed schedulers produce the same attempt sequence. PR A owns only the time-base/canary changes;
   C alone owns this rename.
11. Introduce a monotonic control epoch in the legacy executor and advance it exactly once for each
    primary control transaction, independently of presentation `frameNumber` and simulation-pulse
    count. C exposes the epoch; D5 migrates visible-form registration to it and E preserves it under
    the fixed scheduler.

**Done when:** The complete pulse matrix and transition/counter goldens match; begin/finish pairing,
pulse-originated skip, fatal terminalization, and no-mid-pair-barrier tests pass; UfoRecoveryBegin,
DefendTheBase, battle ZoomView, and re-entrant form events retain the characterized legacy delivery
and one-extra-pulse ordering;
form callbacks run exactly once synchronously while the queued Stage delivery of each same object obeys origin/generation
delivery, and control-originated copies reach the next primary heartbeat but never historical work;
callback iteration uses a stable registration-order snapshot, a same-type callback registered
mid-dispatch waits until the next emission, and the emitting Control/originating Stage remain alive
after `RaisedBy` and their final external owners are dropped;
all fourteen City refresh registrations retain the characterized C-parent behavior and the
`city-refresh-14-v1` target remains demonstrably red for C2;
the 4,097th emission runs neither callback nor Stage delivery; both internal event caps fail rather
than drop; a simulation-only repetition probe
changes no wall/control/UI/audio/RNG state except typed dirty metadata; the dimension transition
constructs no view in a pulse; Pause, Speed1-skip, and MANUAL dimension reconciliation remain live;
user radio changes preserve `lastSpeed`, redundant programmatic changes preserve both fields,
actual programmatic changes update `lastSpeed`, and both speed-radio
ordering cases hold; City parent update remains post-pulse and transition-skipped while Battle's
remains pre-pulse; hotseat/modal gates still prevent simulation; Boot, Loading, Video, Briefing, and forms advance with zero simulation;
legacy pacing remains frame-coupled; C's ordinary gate reproduces both active replay expectations
and the whole-campaign parent oracle exactly and no compatibility overlay exists. Actual behavior
changes belong to C2; multi-pulse catch-up belongs to E, not C or Phase 0.

**Rollback:** Revert the extraction commit. No save changes.

### Phase 2B — Authorize and activate the intentional compatibility deltas (C2)

**Goal:** Land only the three deliberate oracle-observable corrections that C could not make while
remaining byte-compatible with its admitted parent: CD-03, CD-12, and CD-16.

**Changes:**

1. Before a C2 candidate, integration commit, or dependent source branch exists, seal a reviewed
   delta-only head based directly on the live admitted C SHA/tree. Its diff manifest maps every
   behavior-changing hunk to exactly one of CD-03/CD-12/CD-16; every other hunk is forbidden. Run
   the complete pre-candidate compatibility procedure: three fresh processes for every active
   replay cell on both C and the delta source, plus 18 keys × three fresh campaigns on both heads
   (54+54=108 complete victories). Require stable per-cell/per-key projections, exact agreement of
   C with both active effective oracle chains, exact equality of every unowned component, and one
   role-signed, single-use `compatibility-delta-authorized` record containing the precise old/new
   values. A parent mismatch is a robot red; reference receipts never count toward admission.
2. For CD-03, wire C1's control-only refresh helper into all fourteen City registrations: six fixed
   organisation filters and each of the eight `NUM_TABS` buttons. The registry/source scan is
   authoritative. `city-refresh-14-v1` selects every registration from both Speed1 alternator phases
   and Speed2 and proves zero GameTime delta plus exact preservation of
   `CityView::skipSpeed1Tick`; a single skipped Speed1 sample is insufficient.
3. For CD-12, activate C's typed `DimensionRecheckContext` and Battle control-recheck policy. A
   historical pulse that observes a due transition stops and requests primary control. The next
   primary performs no optional pulse, rejects stale generation, records live-day drift, recomputes
   every natural/debug/vehicle/projectile predicate from current state, then preserves the exact
   Speed1/radio → natural-keyed mutation → view construction/copy-debug/`REPLACEALL` order. E later
   invokes this same policy for accumulator catch-up and F for STEP jobs; neither may reimplement it.
4. For CD-16, turn `simulation_causal` provenance into its separate non-dropping lane. Before every
   possible following pulse, drain that lane to quiescence, apply one live StageCmd barrier, and
   reacquire the stage. More than 4,096 accepted causal events in one barrier is fatal. This changes
   the admitted legacy characterization from one outgoing-stage pulse after an event-issued command
   to zero; fresh external, async-control, and control-originated work remain primary-only.
5. Create C2 only after the authorization exists. Its candidate binds that authorization and exact
   sealed source head, and its integration tree must equal the reviewed reference tree. Run the
   machine-derived ordinary set `{cold-lifecycle, active-schema-replay, merge-3-v1}` from scratch on
   the exact integration candidate. Admission atomically activates both the replay and campaign
   overlays. After exact landing/reconciliation, the exact `develop` SHA/tree must win the blocking
   `integrated-repeat-20-v1` before any D0/D1 source branch or candidate exists; ongoing nightly-20
   surveillance continues until the head is superseded.

**Done when:** The paired 108-campaign and complete active-replay reference is stable; the
authorization is independently reviewed and unused; the C2 candidate/tree matches the sealed source;
all fourteen refresh paths are control-only; dimension and Battle rechecks use one live-state policy;
UfoRecoveryBegin, DefendTheBase, battle ZoomView, and re-entrant causal commands apply their barrier
before a following pulse; no unauthorized replay or campaign component changes; every ordinary gate
passes on the exact candidate; admission activates both overlays atomically; exact landing plus the
blocking 20-campaign integrated repeat is green. A later logical red revokes C2 and all descendants.

**Rollback:** Append `compatibility-delta-invalidated`, revoke C2 and descendants, and resume from
the still-sound admitted C head. Never rewrite C's baseline, replay schema, or failed reference
evidence.

### Phase 3 — Make presentation side-effect free

**Goal:** Rendering the latest state any number of times must not change gameplay or timer state.

**Changes:**

1. In D1, lock VideoScreen's actual legacy sequence before changing it: `render()` consumes EOF by
   clearing `current_frame`, presents one blank frame, and the next `update()` queues `REPLACE`;
   `eventOccurred()` also clears `current_frame` for keyboard, mouse, and finger skip and therefore
   currently aliases skip with EOF. Treat the blank as an accidental presentation defect rather
   than claiming compatibility. Introduce an explicit control-owned playback state such as
   `NoFramePending`, `Playing`, `EofPending`, and `SkipPending`. Empty paths, load failures, and a
   missing first decoded frame enter `NoFramePending` and queue `REPLACE` on the first control
   heartbeat, preserving the automation-critical `skipIntro` boot path. Move frame advancement/end
   detection from `render()` into wall-time control, retain the last decoded frame after EOF, and
   queue `REPLACE` on the following control heartbeat. Only Escape key-down, mouse-down, or
   finger-down enters `SkipPending`; it suppresses remaining control work and queues the transition
   at the current primary heartbeat's closing barrier, including from `EofPending`. It never relies
   on a null-frame sentinel. Use C0's injectable monotonic clock in `begin()` and control
   progression; do not use `high_resolution_clock` directly. This intentional blank-frame removal
   is the video compatibility delta and is locked for visible, suppressed, delayed, and skipped
   presentation.
2. D0 exclusively owns `game/ui/tileview/tileview.{h,cpp}` and is additive at its standalone landed
   head. It introduces the shared duration/phase API and makes isometric/strategic scrolling elapsed-
   wall-time based, while preserving `SELECTION_FRAME_ANIMATION_DELAY` and
   `PORTAL_FRAME_ANIMATION_DELAY` with identical names, `const int` type, visibility, and values 12
   and 4 as transitional source-compatibility shims. D2 and D3 independently migrate their disjoint
   consumers to the new API, add no new shim use, and never edit the shared files. The harmless dead
   shims remain through this train; removal requires a separately registered cleanup slice that
   depends on both D2 and D3 and exclusively owns `tileview.{h,cpp}`, never “whichever lands second.”
3. D2 owns CityTileView selection/portal counters, border blink, and palette pulsation. D3 is one
   coherent atomic battle-presentation PR owning both `battletileview.{h,cpp}` and
   `battleview.{h,cpp}`. It migrates the complete BattleTileView render-counter block—healing, low
   morale, psi, selection frame, icon, and focus animation—plus hidden-bar/palette state,
   closest-visible-fire selection, `playSample()` side effect, fire-sample assignment and
   countdown/replay, pause-icon pulsation, throw/action-impossible/
   path-preview/attack-cost behavior, and every cross-file field/reset/API in the same reviewed
   tree. There is no intermediate develop state with only half the fire-audio or inherited timing
   contract migrated.
4. Classify every migrated timer as presentation-wall-time or simulation-time beside its definition.
5. In D5, remove `notifyVisibleForm()` from `Form::onRender()` and retain/control-complete visible
   form registration through `Form::update()` plus explicit control traversal. Bind clearing and
   deduplication to the control epoch introduced by C, not presentation `frameNumber`; verify every
   currently visible form remains discoverable when render is slow or suppressed. Because
   `BattleTileView::hiddenForm` is rendered but never updated and is not a child of a traversed root,
   D5 (after D3) explicitly traverses/registers that form tree during Battle control without calling
   `hiddenForm->update()` or changing its behavior. Keep the separate
   constructor/destructor-owned `Form::liveForms()` liveness registry used by `dumpLiveUI()`; it is
   render-independent, intentionally includes every still-live form before its own `isVisible()`
   filter, and is not silently redefined as the control-epoch action registry. `UiDump` reads that
   liveness snapshot urgently on the single framework thread between StageCmd barriers.
6. Add a render-purity probe over GameState hash, RNG state, audio scheduling, StageCmd queue,
   presentation timer state, both the constructor/destructor liveness registry and the control-epoch
   visible/action registry contents/cardinality, control epoch, and presentation frame counter.

**Done when:** 10,000 repeated renders between control updates leave every protected field and both
   form registries unchanged; paused/covered/live-versus-control-visible UiDump and action transcripts
lock their intentionally different membership; VideoScreen no-path/load-failure/no-first-frame boots transition on
the first control heartbeat, Escape/mouse/finger skips transition without a simulation pulse from
both Playing and EofPending, a non-Escape key-down does not skip, and last-frame/next-heartbeat transition goldens prove the intentional
blank-frame removal under slow, suppressed, and repeated rendering; every named city/
battle counter and viewport motion retains the same real-time cadence at TargetFPS 1/30/60/240;
clean build/test passes for all four admissible source states D0, D0+D2, D0+D3, and D0+D2+D3; a
derived-TileView compile fixture at D0 proves both legacy names retain the exact `const int` contract
and values while D2/D3 phase/render-purity tests consume only the new API;
visible-form introspection follows control at TargetFPS 1/minimized; no D slice overlaps another's
file ownership at the same landed head—D5's later, named `battletileview.cpp` registration hook is
reviewed only after atomic D3 has landed.

**Rollback:** Revert per-site presentation commits. Core save data is untouched.

### Phase 3B — Add the harness dispatch substrate

**Goal:** Make later primary-barrier command execution safe without retaining raw stage, callback,
socket, or file-descriptor lifetimes, replace the already-broken process-global query-handler stack
with stage-owned dispatch, and preserve legacy-v1 bytes and immediate execution.

**Changes:**

1. In E0, cap accepted clients at 16 and give each socket a monotonic connection ID and reuse epoch plus request ordinals,
   receive storage, ordered completion slots, and a nonblocking output FIFO. A request/reply handle
   refers only to `{connection_id, connection_epoch, ordinal}`; it never stores `Client*` or an fd.
2. Split parse/classify/admit from execution. A deferred request stores the parsed immutable command,
   accepted `stageGeneration`, optional global mutation sequence/required fence, and reply
   handle—never `Stage*`, a
   handler `std::function`, or another raw-lifetime object. Disconnect marks the reply destination
   dead but does not revoke already accepted mutation work; completion is discarded with telemetry.
3. Preserve immediate legacy execution order in E0 and always preserve response order per
   connection. E0 assigns the IDs/ordinals and characterizes `Action set` then UiDump/Query, but it
   does not invent a second observation rule: E later assigns `mutation_seq` and implements the
   sole normative `process_global_mutation_fence` above, while F consumes its watermark. Only
   process-terminal QUIT and the explicitly classified urgent operations use their table rows.
   Partial
   writes with `n>0` advance only by that count; `EINTR`/`WSAEINTR` retries without advancing;
   `EAGAIN`/`EWOULDBLOCK`/`WSAEWOULDBLOCK` retains the exact suffix until writable service;
   `n==0` closes/retires that connection epoch without spinning; every other negative result is a
   fatal close/retirement. Every syscall attempt, including an interrupt, consumes one of exactly
   64 per-client send attempts for that service pass; would-block or attempt 64 retains the exact
   suffix and yields, so repeated interrupts cannot form a no-progress loop. Never deliver an old
   completion to a reused fd/epoch. Bound completed-plus-unsent output at
   256 KiB and unresolved requests at 256 per client; hard overflow closes that exact epoch.
4. Remove the process-global previous-handler stack. Add stage-owned query methods returning
   handled/unhandled separately from an empty payload, and dispatch over the live StageStack from
   top to bottom at a generation-checked barrier: equipment screens handle their own verbs, then
   covered City/Battle stages handle view verbs. If all stages return unhandled, a Framework-owned
   responder uses one admitted GameState-session identity. Committed CityView or BattleView admits
   or replaces `{monotonic_session_id, weak_owner}`. BattleBriefing, BattlePreStart, state-bearing
   LoadingScreen, and BattleDebriefing are carriers only: they expose a **request-scoped** provider
   only when their `sp<GameState>` has the same shared-owner identity as the admitted weak owner,
   and can never admit a new state. Covering modals fall through to a matching lower stage. A
   committed stack containing neither an admitting view nor a matching carrier clears the weak
   owner while retaining the availability tombstone. Thus an existing campaign remains queryable
   through Briefing→PreStart→Loading→BattleView and debrief, whereas a new cold-skirmish state from
   initial MainMenu remains unavailable until its BattleView commits; in-game Skirmish setup
   continues exposing only the prior admitted campaign state until the new BattleView admits its
   state. The responder compares shared ownership, not raw addresses, and never retains a callback,
   Stage, or strong `sp<GameState>` after the request.

   Admit/carry/clear is evaluated **exactly once** after the live StageCmd FIFO reaches its
   transaction barrier, using only the final committed stack and resulting `stageGeneration`. It
   is never recomputed between commands or from constructors, `begin()`, `finish()`, or destructors.
   A BattleView constructed while LoadingScreen is current cannot admit early; it admits only when
   present in the final committed stack. A carrier must expose a non-null state whose shared owner
   matches the still-live admitted weak owner; null LoadingScreen state is always no-match and can
   neither carry nor admit. The lifecycle journal records old/new session ID and final decision at
   that same barrier, and request execution requires that exact generation.

   A process-lifetime `never_seen`/`previously_seen` availability tombstone preserves legacy-v1
   absence bytes without retaining strong state: before any provider has existed, Query/Save return their
   current `ERR no gamestate` lines; after a provider existed and no committed stage exposes one,
   Query returns `ERR unknown query` and Save returns `ERR save failed`. Terminal win/loss truth
   remains in exact `Status.detail`. GameState Query/Save stays primary and generation-bound, never
   urgent or invariant. Constructors, `begin()`, and destructors install/restore no global callback;
   pending stages have no query side effect. A deferred request stores bytes plus accepted
   generation only; at execution it walks the exact-generation stack and transient resolver or
   returns stale-stage, retaining no method closure or Stage/strong-state pointer. E0 leaves legacy-v1
   execution immediate through this route; E defers it safely.
5. Keep all socket operations nonblocking. Parse errors and immediately executable legacy replies
   use the same ordered output path so one transport implementation covers immediate and deferred
   completion without changing reply lines. E0 retains today's synchronous screenshot readback/
   write behavior; E, which already owns the scheduler service boundary, is the slice that moves
   write completion asynchronous.

**Done when:** The focused `test_harness_dispatch` target's scripted transport/socketpair tests
cover partial→would-block→completion, one and 64
consecutive `EINTR`/`WSAEINTR` attempts with exact suffix retention/yield at the cap,
zero/no-spin, fatal negative error, multi-reply head-of-line ordering, disconnect, fd reuse, 16/17
clients, 256/257 unresolved slots, and output bounds; pending City/
Battle/equipment-screen constructors and outgoing destructors cannot install, steal, or restore a
dispatcher; multiple preconstructed replacements, nested AEquip/VEquip, City dimension
REPLACEALL, Battle/debrief, and PUSH/POP/REPLACE/REPLACEALL/QUIT use only the live stage stack;
one transaction containing several StageCmds proves only its final committed stack can
admit/carry/clear; pending BattleView construction has no effect and null-state LoadingScreen never
matches;
byte-identical `GS`/Save fixtures pass through both campaign and existing-game
Briefing→PreStart→Loading→BattleView paths, in BattleDebriefing reached through both
`BattleView::endBattle()` and InGameOptions abort, and on debrief→City. Cold
MainMenu→Skirmish→PreStart→Loading does not expose the new state before BattleView; in-game Skirmish
setup exposes only the prior admitted campaign state until BattleView admits the replacement.
Skirmish debrief→MainMenu, post-abandon MainMenu, and win/loss terminal VideoScreen prove the
admitted weak owner is cleared while exact
previously-seen unknown-query/save-failed bytes and `wingame2.smk`/`lose1.smk` Status truth remain;
initial MainMenu proves the distinct never-seen no-gamestate bytes;
E0 characterizes `Action set` followed by UiDump/Query in immediate ordinal order; legacy command/
reply fixtures remain byte-identical and immediate. E extends this same target with deferred
`process_global_mutation_fence` cases, and F extends it with RUN-watermark cases.

**Rollback:** Revert E0 before E lands. No protocol bytes, save state, gameplay state, or scheduler
cadence changes.

### Phase 4 — Implement the fixed three-clock scheduler behind a default-off flag

**Goal:** Decouple rendering without removing the legacy executor during compatibility bake.

**Changes:**

1. Add default-off option `Framework.FixedSimulationScheduler=false` and a pure
   scheduler/controller consuming C0's already-landed injectable monotonic clock. Use rational/
   integer phase accounting; do not truncate `1/60` to integer microseconds or introduce another
   clock abstraction.
2. Pump, classify, and admit external work continuously according to the exhaustive table below,
   using E0's stable request handles and ordered reply transport. Keep the source-tagged causal/
   control-origin event lanes defined by C. Gameplay input and primary harness requests are queued
   for the primary consistent-state barrier. Unknown commands/events are rejected and logged; they
   never inherit an accidental default class or enter the causal lane. Framework does not duplicate
   Forms grammar: Forms installs one classifier/executor pair that parses
   `CONTROL <id> (item <N>)* [get|click|toggle|set <value...>]` once. Only an operator token found
   after the complete item chain can classify the action; `set get` is mutation with value `get`,
   an omitted operator is legacy click/mutation, and every registered action supplies an explicit
   read-only/mutating class beside its executor.
3. Service urgent work before debt calculation. Window/process quit queues `QUIT` and drains it
   immediately; E's legacy read-only and presentation operations use the exhaustive table below,
   while state-mutating save/load and gameplay actions remain queued for primary dispatch. E does
   not register or partially implement STEP: every `STEP ...` line remains an unregistered typed
   error until F lands the negotiated controller/protocol atomically.
4. In REALTIME, run exactly up to the latched `historical_limit` from the normative table, never an
   unconditional four. At steady-state one-slot/due-control, the sole slot remains reserved for the
   primary pair; four historical plus one primary occurs only when at least five slots are due.
   Each historical transaction drains causal events
   from the prior pulse to quiescence, drains/reacquires through a live FIFO barrier, then executes
   `advanceSimulationPulse(Historical) -> live FIFO barrier`, without fresh input or a control
   heartbeat. Stop immediately on any result other than `advanced`/`skipped_speed1`, any
   stage-generation change, causal-event/StageCmd failure, or terminal drain. Historical work never
   calls either control half, but the causal event barrier may perform the exact eventOccurred work
   required before the next pulse.
5. Run the primary heartbeat at most once and exactly in the normative order from Section 2:
   drain prior-pulse causal events → live barrier/reacquire → dispatch stage-tagged fresh/async
   input → begin-control → zero/one REALTIME primary pulse → paired finish-control → one live
   FIFO barrier. A StageCmd queued anywhere in the control pair is not applied mid-pair. E owns one
   production `Cadence60` generation machine and its REALTIME/MANUAL enter, release, and expiry
   transitions; fixed launches expose only REALTIME to ordinary users, while E's production-API
   tests exercise MANUAL control-only reconciliation with no STEP command or lease. F adds the
   protocol-visible MANUAL lease and STEP jobs by reusing that same generation machine and control
   pair with zero implicit simulation. It runs explicit jobs as atomic pulse transactions after the
   primary barrier, draining a prior job pulse's causal events before a following job pulse; F may
   not introduce a second controller or generation state machine.
6. Execute at most five automatic pulses total per outer iteration (four historical plus one
   primary) and at most eight milliseconds of automatic simulation work measured between pulse
   boundaries. A pulse is atomic and never preempted, so the cooperative latency bound is
   `max(single pulse, 8 ms)`. When the time budget is exhausted, stop historical work, still run the
   due primary control pair, omit its implicit simulation if no budget remains, and sample/record
   raw lateness **before** any deadline rebase, pulse cap, time-budget stop, or debt discard,
   followed by executed pulses, stop reason, and dropped pulses. Use the checked shared ordinal
   lattice and exact half-open debt range; overflow is fatal rather than saturation, and no
   elapsed-time fractional accumulator exists. Every
   result with `attempt_consumed=true` retires and counts exactly one due 60-Hz slot, including
   `advanced`, `skipped_speed1`, and a post-attempt `needs_control`; a Speed1 skip never becomes
   replayable debt merely because it advanced zero ticks. Origin does not change debt accounting:
   any historical or primary pre-attempt `paused`/`covered`/`unsupported`/`needs_control` result
   consumes no current slot and invokes `settleAll(semantic_reason)` without changing the sampled
   future simulation ordinal or the same-generation latched due control ordinal, then disables
   automatic admission until that reconciliation control closes.
   `needs_control` separately latches the required next-primary
   reconciliation but does not preserve debt while waiting for it. Any historical or primary
   post-attempt `needs_control` first retires/counts its consumed current slot, then invokes that
   same `settleAll` operation for the remainder. Its range receipt supplies the exact retired count;
   the generic drop tail handles only a still-nonempty range, preventing duplicate
   telemetry. Thus paused, covered, unsupported, or
   control-required wall time is never replayed after resumption. Phase 0 latency samples explicitly include
   Speed5/turbo's full `updateTurbo()` transaction.
7. On any stage transition, covering modal entry, stage suspension, stage-stack uncover/modal
   return, and replacement, settle old-generation debt
   exactly once, invalidate the old-stage control latch, and schedule immediate ordinal-zero control
   for the replacement generation before enabling its automatic simulation. Never create or edit an
   elapsed-time fractional phase. SDL window minimize/restore is presentation state only and remains
   under item 10; it never invokes this stage-generation rule.
8. Normalize `TargetFPS` safely in both executors: `0` is uncapped without division, positive values
   pace the executor, and negative values warn and use 60. In fixed mode it is presentation-only;
   in legacy mode it intentionally continues to pace both render and simulation. Preserve the
   existing default and normalize once at session startup before constructing any frame duration,
   because the current value is captured before the loop and zero faults there. Prove the zero path
   cannot divide by zero before the scheduler flag is enabled. Telemetry and help text identify
   which semantic is active.
9. Move `SDL_GL_SetSwapInterval()` after successful context creation/current binding, validate its
   return, and log `SDL_GL_GetSwapInterval()`.
10. Suppress render/swap only for genuinely non-presentable hidden/minimized window states. Focus
   loss, pointer leave, and ordinary deactivate events do not suppress presentation. Reset only
   presentation phase on restoration; keep control/simulation semantics unchanged.
11. Keep `FrameLimit` as a presentation count and add `SimulationPulseLimit`; zero means unlimited
    for both. `FrameLimit` increments only after a completed render/swap. On the Nth completed
    presentation, terminalize before any next admission, control, pulse, or presentation; a hidden/
    minimized/suppressed presentation does not count. `SimulationPulseLimit` increments exactly once
    for every completed canonical pulse result with `attempt_consumed=true`, including advanced and
    Speed1-skipped results and regardless of Historical, Primary, or F STEP origin. Pre-attempt
    paused/covered/unsupported/needs-control results do not count. Evaluate the limit only after the
    pulse's atomic settlement, paired finish when applicable, causal drain, and closing StageCmd
    barrier; a natural fatal/QUIT/final-empty reason discovered there wins. No second work item runs.
    Add a source guard after E that forbids direct runtime `quitProgram=true` or terminal stack
    clearing outside initialization and the one terminal routine.
12. Preserve C's monotonically increasing control epoch as distinct from presentation
    `frameNumber`; D5 has already moved forms/harness visibility registration and deduplication to
    that epoch, so TargetFPS=1 or minimized rendering cannot duplicate or retain stale visible forms.
13. Bound the fresh-input queue at 4,096 accepted envelopes, preserve acceptance order, and tag every
    item with `stageGeneration`. At capacity, stop SDL polling and return typed `busy` to harness
    writers; never accept then silently drop a non-coalescible action. Coalesce only superseded mouse
    motion/resize. A generation mismatch after a transition discards the now-stale stage-targeted
    envelope with telemetry rather than leaking it into a newly exposed stage; its accepted input
    sequence is terminally `stale_generation_retired`, not left unsettled. The urgent command
    classifier and primary dispatcher share one exhaustive table.
14. Implement the normative rational deadline/debt algebra and interruptible `waitOnce` contract
    above, including control, next eligible REALTIME simulation, capped presentation, F lease/job
    deadlines, and immediate I/O readiness. Never catch up control heartbeats or presentation frames
    in bursts. Add a presenter seam plus a blocking-swap integration test so a fake clock is not the
    only evidence for `SDL_GL_SwapWindow()` stalls.
15. At the primary barrier, execute a deferred request only when its accepted generation matches the
    current stage stack. A mismatch completes `ERR stale stage` in the
    legacy line protocol (typed `stale_stage` after F negotiation) and invokes neither old nor new
    stage method; clients retry after a fresh Status/current-stage observation. Dispatch walks only
    E0's currently owned top-to-bottom stack and cannot observe a pending constructor or destroyed
    stage. Accepted mutations still execute once after a
    client disconnect, with only their dead-epoch reply discarded. Per-connection replies remain in
    ordinal order, and the `process_global_mutation_fence` means an observation cannot run or
    capture before every mutation preceding its admission settles, even across connections.
16. Move Harness Screenshot completion in E, not F. After its process-global fence is satisfied,
   capture the last completed surface on the main thread, advance the connection execution cursor
   at capture, and submit an owned job to the one bounded dedicated `ScreenshotService`. Route SDL
   PRINTSCREEN through that same service after its primary-barrier main-thread capture, without a
   socket reply. Completion posts an epoch/ordinal-tagged wake only while the service is OPEN and
   makes only the ordered Harness reply ready. Implement policy-aware atomic publication
   (`ReplaceExisting` for Harness explicit paths, reserved `CreateNew` for PRINTSCREEN), destination-
   ordinal ordering, terminal cancellation, no post-terminal enqueue/wake/callback/global access,
   and the in-`Framework::run()` cancel/join/cleanup/runtime-seal contract above. E logs non-
   preemptible readback duration and stops further harness service that iteration; F adds the formal
   2-ms service bound.

External-work classification (the implementation uses exhaustive enums/static checks over the same
rows):

| Owner | Source and concrete commands/events | Class | Interrupts queued/active RUN after RUN? | Execution barrier |
|---|---|---|---|---|
| R1a/E0/E/F | Harness `BuildInfo` | process-invariant urgent read-only | no | after its `process_global_mutation_fence`, return the byte-identical `launch_effective` snapshot captured once after argv/config/path normalization and CD validation but before service/loop entry; include the complete sorted registered-option map/digest, runtime-input identity, scheduler/protocol capability ranges, and process identity. Divergent per-connection negotiation never changes it |
| R1a/E0/E/F | Harness `Lifecycle <cursor>` | process-scoped monotonic urgent read-only | no | return one point-in-time journal snapshot after its `process_global_mutation_fence`; include the one-shot `rng_seed_consumed` record emitted after startGame seed-mode resolution and before first RNG-dependent setup; the single framework thread never services this command concurrently with a lifecycle barrier/journal mutation, and E0/E/F preserve that exclusion |
| E | Harness/StageCmd `Quit`; SDL quit/window-close; external `shutdownFramework`; committed final empty stack; FrameLimit/SimulationPulseLimit; fatal/overflow/invalid | urgent/derived terminal | terminalizes | invoke the one typed idempotent terminal procedure at the cause's normative boundary; transient clear/POP-empty-tail-PUSH and REPLACEALL are not terminal |
| F | STEP `HELLO`, `CURRENT`, `STATUS` | urgent read-only | no | unavailable before F; after F, wait for the captured `process_global_mutation_fence`, then snapshot during outer-loop urgent service |
| F | STEP `ACQUIRE`, `RENEW`, `CANCEL`, `RELEASE`; lease expiry | urgent controller mutation | controller table: authorized matching CANCEL, authorized RELEASE, and expiry cancel; ACQUIRE/RENEW/replay/rejection do not | unavailable before F; after F, outer-loop urgent service, and ACQUIRE/RELEASE/expiry suppress automatic pulses for that iteration |
| F | STEP `RUN` | urgent acknowledgement, fenced activation | no; later RUN is `job_busy` | unavailable before F; after F, capture the exact preceding `mutation_seq` watermark, create the queued job, and activate only when the contiguous settled watermark reaches it |
| E | Harness `Status`, `UiDump`; `Action` subverbs `CONTROLS`, `HELP`, and `CONTROL <id> (item <N>)* get` where Forms' parser identifies `get` only after the complete item chain | urgent read-only | no | execute against the last completed control/liveness snapshot only after the `process_global_mutation_fence`; `set get` is mutation with value `get`, missing op is click/mutation, and no lexical scan or framework-side grammar duplicate is permitted |
| E | Harness `Resize`; SDL resize/minimize/restore/visibility | split presentation + primary stage event | no | update presentation state urgently; if a live Stage receives a resize event, enqueue/coalesce it for the next primary heartbeat with generation and `mutation_seq`, whose terminal disposition satisfies the same fence |
| E | Harness `Move`; SDL mouse motion | split presentation + fresh control | no | update latest cursor presentation position during urgent service; enqueue/coalesce the stage event for the next primary heartbeat, tagged with stage generation |
| E | Harness `Down`, `Up`, `Click`, `Scroll`, `Key`, `KeyDown`, `KeyUp`, `Text`; SDL buttons/keyboard/controller/text input | fresh gameplay/control input | yes | FIFO at next primary heartbeat with generation and `mutation_seq`; terminally apply/coalesce/retire exactly once and release its reply in ordinal order |
| E | Every Harness `Query`, including all `GS` namespace spellings; Harness `Save`; every other registered `Action`, including missing/default/click/toggle/set `CONTROL` | primary query or mutation | yes | FIFO at next primary consistent-state barrier with accepted generation; walk matching live stages, then resolve GameState only for this request from the admitted session and same committed stack. City/Battle admit; matching Briefing/PreStart/Loading/Debrief carriers continue it; a no-match committed stack clears it. Preserve never-seen versus previously-seen absent-state bytes without retaining strong state, or complete stale-stage; clients cannot select an urgent barrier by spelling and deferred requests retain no handler/Stage/strong-state/socket pointer |
| E/F | Harness `Screenshot` | urgent presentation snapshot | no | after its captured `process_global_mutation_fence`, reserve one of 16 shared ScreenshotService slots before readback; full/no-readable-surface returns the existing ordered error without enqueue. Otherwise E captures the most recently completed surface on the main thread, never waits for render, submits `ReplaceExisting`, and completes its reply only after publication succeeds/fails; F adds only the cooperative service bound |
| E | SDL `PRINTSCREEN` | legacy screenshot input | no | at primary dispatch, reserve one of the same 16 slots before readback; full logs/drops with no enqueue. Otherwise capture the most recently completed surface, reserve a disk-and-pending-absent `CreateNew` name, and submit without a connection/reply token; do not wait for a new render |
| E | `EVENT_GAME_STATE` and any event pushed while a simulation pulse is active | simulation causal | no | source-tagged lane; drain to quiescence before a following pulse, then live StageCmd barrier; never coalesce or drop |
| E | Re-entrant `EVENT_FORM_INTERACTION` | inherited | inherits its external envelope; never independent | inherit the envelope being dispatched; causal stays causal, fresh stays in the current primary dispatch |
| E | Event pushed by begin/finish/control-only update with no active dispatch envelope | control originated | no | stage-tagged lane for the next primary dispatch; drain its inherited form chain to quiescence there; never historical catch-up |
| E | Timer/audio events not produced by a pulse | async control | no | next primary dispatch before begin-control; never historical catch-up |
| E | Harness/SDL internal wake events | internal | no | their owning scheduler/presentation barrier only; never forwarded as gameplay |
| E/F | `Unknown`, malformed, oversized, or unregistered command/event | rejected | no | typed error plus telemetry; before F every STEP line is unregistered; no state change |

**Done when:** Fixed-mode fake-clock runs at TargetFPS 0/1/30/60/240 and vsync 0/1 yield identical
automatic simulation/control opportunity ordinals in every no-lateness run; retained-legacy runs are
separately characterized as coupled. Tests prove `D(k)` alignment at every remainder boundary,
exactly 60 ordinals in each half-open one-second interval, the separately counted right endpoint,
no reserve-from-zero, typed-fatal checked overflow/backwards clock, and the first fixed iteration's one control, one
eligible pulse, and one presentation. MANUAL release has zero same-iteration automatic work, runs
control-only ordinal zero in that same acceptance iteration, and permits its first control/simulation
pair no earlier than ordinal one of the fresh shared epoch on a following iteration. A fake historical pulse that advances the clock across one or
many deadlines proves those deadlines accrue only on the next iteration; transition and typed
no-attempt rebases retire each slot exactly once without erasing the latched primary reconciliation.
Early/spurious I/O wakes advance no phase;
capped/uncapped/suppressed presentation, post-swap missed
ordinals, and the 5/6-slot cap/drop matrix are exact;
deferred request generation, live-StageStack generation/lifetime, disconnect, and ordered-reply tests pass;
`test_harness_dispatch` proves same/fresh-connection mutation→every observation, same-connection
observation→mutation and mutation→observation→mutation dispatch exclusion, Screenshot-capture→later-
mutation ordering, contiguous watermark gaps, Query/GS/Save failure settlement, nested CONTROL
grammar, and asynchronous screenshot file-before-OK/failure ordering. It also proves slow
Screenshot→mutation→every terminal reason retirement, a fresh connection observing while another
connection has pending work, Harness same-path replacement/global capture order, PRINTSCREEN
disk+reservation name uniqueness, published-final and previous-final preservation, unpublished-
final absence, late-worker temp cleanup without enqueue/wake/callback, blocked-encoder join/lifetime,
and QUIT full/zero/partial/EINTR/would-block/error reply branches. A parameterized terminal matrix
installs accepted mutation, parked fenced observation, every screenshot publication state, and a
partial reply before each StageCmd/Harness/SDL QUIT, window-close/external-shutdown, final-empty,
FrameLimit, SimulationPulseLimit, and fatal/overflow/invalid cause, then proves exactly-once
dispositions, closure, and no later work. `FrameLimit=1` counts exactly one completed presentation
and none while suppressed. `SimulationPulseLimit=1` counts advanced and Speed1-skipped pulses but no
pre-attempt result, with Historical/Primary/STEP origin coverage after F. POP-empty-tail-PUSH and
REPLACEALL continue, destructor clear cannot re-enter, simultaneous limit plus natural cause obeys
the typed precedence, and a source guard finds no direct runtime quit/terminal clear outside the
one routine. Overload, transition, suspension, and minimize pass; activation remains default-off.

**Rollback:** Switch to the legacy executor. This is a scheduler fallback, not a rollback of the
StageCmd, callback, or presentation fixes. Binary rollback is save-compatible.

### Phase 5 — Add retry-safe STEP leases and migrate bundled automation

**Goal:** Replace high-render-rate acceleration with deterministic, bounded simulation work over the
loopback request/reply transport established by E0.

**Changes:**

1. Preserve every legacy command and reply byte-for-byte. Add metadata only to the negotiated STEP
   namespace:

   ```text
   STEP HELLO
   STEP ACQUIRE client=<uuid> seq=<n>
   STEP RENEW client=<uuid> seq=<n> lease=<token>
   STEP RUN client=<uuid> seq=<n> lease=<token> count=<n>
   STEP CURRENT lease=<token>
   STEP STATUS job=<id>
   STEP CANCEL client=<uuid> seq=<n> lease=<token> job=<id>
   STEP RELEASE client=<uuid> seq=<n> lease=<token>
   ```

   `STEP HELLO` returns `protocol_min`, `protocol_max`, feature names, and a random process-instance
   nonce. Every STEP/2 reply uses an explicit envelope carrying that nonce; legacy parsers never see
   appended fields.
2. Make every mutation retry-safe with client UUID + monotonically increasing sequence. One mutation
   may be outstanding per client. Repeating the exact last sequence/payload returns the exact cached
   response; the same sequence with different bytes, a lower sequence, or a skipped sequence is
   rejected. Keep replay-cache entries and released/expired lease tombstones for 120 seconds. Lease
   TTL is 30 seconds on C0's injected monotonic clock; tokens are random 256-bit capabilities and
   never logged. Production therefore uses the steady-clock implementation while expiry tests use
   the deterministic fake.
3. Use one global lease and one global job, monotonic job IDs, and three independent bounded stores:
   per-client mutation replay, lease tombstones, and the last 64 terminal job records. Do not evict
   one class to make room for another.
4. Use this normative controller table:

   | State | Mutation/event | Resulting state / rule |
   |---|---|---|
   | `REALTIME` | `ACQUIRE` | atomically rebase automatic debt and enter `MANUAL_IDLE`; no realtime pulse in that iteration |
   | `REALTIME` | `RENEW`, `RUN`, `CANCEL`, or `RELEASE` | typed `no_active_lease`; remain `REALTIME` |
   | any lease-held state | `ACQUIRE` | typed `lease_busy` for every new sequence/client; exact replay of the original acquire is handled by the mutation cache; state unchanged |
   | any lease-held state | wrong/missing token or non-owner mutation | typed `unauthorized`; state unchanged and no capability disclosure |
   | `MANUAL_IDLE`, `JOB_QUEUED`, or `JOB_ACTIVE` | `RENEW` | extend lease only; queued/active job is unchanged |
   | `MANUAL_IDLE` | `RUN` | create `JOB_QUEUED`, capture RUN's pre-own `required_mutation_fence` plus stage generation, assign/settle RUN's own mutation sequence when enqueue/reply state installs, and return job ID immediately |
   | `MANUAL_IDLE` | `CANCEL` | typed `no_active_job`; lease remains `MANUAL_IDLE` |
   | `MANUAL_IDLE` | `RELEASE` / expiry | enter `REALTIME`; expiry has no implicit pulse that iteration |
   | `JOB_QUEUED` | primary dispatch of mutation `mutation_seq <= fence` | prerequisite work: apply it or explicitly retire/reject/fail it; do not interrupt the job |
   | `JOB_QUEUED` | `settled_mutation_watermark >= fence` across all six terminal dispositions, generation unchanged | enter `JOB_ACTIVE` at that primary heartbeat only |
   | `JOB_QUEUED` | stage generation changes while resolving `fence` | terminal `transition`, `attempt_consumed=false`, `simulation_advanced=false`; retain lease in `MANUAL_IDLE` and never activate on the replacement stage |
   | `JOB_QUEUED` | any accepted post-RUN mutation whose exhaustive classification says `interrupts=yes` and whose `mutation_seq > run.mutation_seq` | terminal `interrupted` before dispatching that later mutation; retain lease, then dispatch it at primary |
   | `JOB_QUEUED` or `JOB_ACTIVE` | another `RUN` | typed `job_busy` naming only the existing job ID; state unchanged |
   | queued/active | `CANCEL` with missing/stale/nonmatching `job` ID | typed `job_mismatch`; state and live job are unchanged |
   | queued/active | authorized `CANCEL` whose `job` exactly matches the live job | terminal `cancelled`, then `MANUAL_IDLE` with lease retained |
   | queued/active | `RELEASE` / expiry | terminal `cancelled`/`expired`, release owner, enter `REALTIME` |
   | `JOB_ACTIVE` | any accepted post-RUN mutation whose exhaustive classification says `interrupts=yes` | terminal `interrupted` before another job pulse; retain lease in `MANUAL_IDLE`, then dispatch that mutation at its named barrier |
   | `JOB_ACTIVE` | count exhausted | terminal `completed`, then `MANUAL_IDLE` |
   | `JOB_ACTIVE` | pause/covered/unsupported stage | terminal `paused`/`covered`/`unsupported` without consuming a pulse |
   | `JOB_ACTIVE` | `needs_control` | terminal `needs_control` preserving explicit accounting fields, then `MANUAL_IDLE`; pre-attempt preflight reports all advancement fields false, while post-attempt discovery consumes/decrements exactly when `attempt_consumed=true`; the client must allow/drive the next primary control heartbeat before starting another job |
   | `JOB_ACTIVE` | `transition_before_pulse` / `transition_after_pulse` | terminal `transition`; before-pulse reports both booleans false, while after-pulse preserves explicit `attempt_consumed`, `simulation_advanced`, and `ticks_advanced`; a skipped Speed1 attempt is consumed but advances zero ticks, then `MANUAL_IDLE` |
   | `JOB_ACTIVE` | `game_over_before_pulse` / `game_over_after_pulse` | terminal `game_over`; before-pulse reports both booleans false, while after-pulse preserves all three explicit accounting fields, then `MANUAL_IDLE` |
   | `JOB_ACTIVE` | `fatal` pulse result | best-effort terminal `engine_failure` with the reported attempt/advance/tick fields, then framework/process failure; no lease state survives |

   `TERMINAL` is a retained job record, not an engine mode. A restarted process begins in
   `REALTIME` with a new nonce; it cannot execute a cancellation callback in the dead process, and
   clients discover restart from the nonce.
5. `RUN` is asynchronous and never executes a pulse in the socket handler. It uses E's sole
   `process_global_mutation_fence`: every accepted mutation has one global `mutation_seq` and
   reaches exactly one terminal disposition—`applied`, `rejected`, `failed`,
   `coalesced_retired`, `stale_generation_retired`, or `process_terminal_retired`. At its atomic
   classify/admit point, RUN sets `required_mutation_fence=last_accepted_mutation_seq` before
   allocating its own sequence; this is the highest accepted sequence preceding RUN, not the current
   `settled_mutation_watermark` and captures the current stage generation. Disconnect retires only
   the reply destination; accepted mutations
   still settle exactly once. Maintain one contiguous settled watermark over all terminal
   dispositions. Activate only when `settled_mutation_watermark >= required_mutation_fence` on the
   same captured stage; a stage
   change terminates `transition`, and any accepted post-RUN mutation marked `interrupts=yes` in the
   exhaustive table interrupts before dispatch. Query/GS/Save, mutating Action/CONTROL, and button/
   scroll/wheel/keyboard/controller/text input interrupt; observations, Screenshot/PRINTSCREEN, Move/mouse
   motion, resize/visibility, and internal/causal/control events do not. Controller mutations follow the table and
   never create a competing fence or watermark.
6. Retain E0's connection-epoch, 256 KiB output, 256 unresolved-request, and ordered-completion
   bounds; cap at 16 clients, 64 KiB receive per client, 4 KiB per line, 32 admitted commands, and
   2 ms of accept/read/parse/admission, cheap urgent dispatch, completion draining, and nonblocking
   output per outer iteration. A complete line left after budget exhaustion remains unadmitted for
   the next iteration. RUN count is `1..1,000,000`. Reject duplicate/unknown fields, zero/negative
   counts, overflow, and oversized input; disconnect the exact client epoch whose bound is exceeded.
   The 2-ms budget excludes deferred primary execution and one admitted Screenshot: capture the
   most recently completed surface synchronously on the main thread, stop further harness work that
   iteration, submit owned bytes to E's already-landed dedicated ScreenshotService, and mark its
   ordered reply ready only after
   success/failure so the file exists before exact `OK path` and write failure still returns the
   existing error. The cooperative harness bound is `max(2 ms, one snapshot readback)` with duration
   telemetry; it never waits for a future render.
7. Slice job work at at most eight pulses or eight milliseconds after urgent/control priority.
   State the cooperative latency bound as `max(single pulse, slice budget)`.
8. Define pulse accounting over the complete atomic simulation transaction: a Speed1 skip reports
   `attempt_consumed=true`, `simulation_advanced=false`, `ticks_advanced=0` and decrements RUN count;
   Pause returns `paused` with both booleans false. Unsupported or covered stages consume nothing.
   `needs_control` always preserves its explicit fields: a pre-attempt preflight consumes nothing,
   while post-attempt discovery decrements RUN exactly when `attempt_consumed=true`, including a
   Speed1 skip with zero ticks. Pre-pulse transition/game-over reports both false; every post-attempt
   terminal result preserves all explicit fields rather than inferring them from ordering. A City
   dimension change found by primary control after Pause belongs to the control transaction and
   terminates a queued/active job as a generation transition before its next attempt; one found
   after a skipped Speed1 attempt preserves consumed=true and advanced=false.
   `fatal` records all explicit accounting fields and terminates the process; causal-event dispatch before
   a following pulse consumes no pulse; a non-pause speed change applies on the next pulse and is
   reported.
9. Adapt R0's typed `AdvanceResult` to jobs. A timeout sends CANCEL and confirms a terminal record
   before returning `timed_out`; process restart/protocol mismatch can never be reported as cancel
   success.
10. Add engine timing introspection and remove Python's `TICKS_PER_DAY` and TargetFPS=1000
    acceleration dependency. Re-run Phase 0's full-campaign wall-time, storage, safe-parallelism,
    retention, and stop-cost capacity study under STEP; PR G uses only this post-F measurement.
11. Add explicit launch option `Framework.HarnessProtocol=1|2`. For one compatibility release,
    protocol v1 remains the default and keeps byte-compatible replies independently of the selected
    scheduler; bundled new tooling explicitly launches v2 whenever it needs MANUAL/STEP and
    separately selects legacy or fixed execution. Protocol never selects the scheduler. New tooling
    fails fast on an old engine unless explicitly passed
    `--legacy-engine`, whose adapter uses the old protocol and admits no fixed-mode claim. After the
    compatibility window, changing/rejecting v1 is a separate reviewed decision. Test real
    old-engine/new-client, new-engine/old-client, v1 rollback, and v2 normal binaries; boot requires
    validated HELLO negotiation, not merely any reply to `status`.

**Done when:** Lost replies for every mutation replay exactly; every controller-table row including
no-lease operations, contention, second RUN, no-job cancel, wrong-owner/token, stale/skipped/
conflicting sequences, stale/mismatched job cancellation, applied/coalesced/stale terminal
mutation dispositions, pre-RUN `rejected` and `failed` mutations advancing the settled watermark
and permitting activation, pre-RUN `process_terminal_retired` causing process terminalization with
no activation, generation-changing RUN watermark, post-RUN gameplay-mutation interruption,
the complete post-RUN interrupt/non-interrupt classification for Query/GS/Save, Action/CONTROL,
low-level input, Move, resize/visibility, every observation, Screenshot/PRINTSCREEN, controller
mutations, and QUIT, bounds, ordered async screenshot completion and terminal retirement,
release/expiry/restart, `needs_control`, before/after transition and game-over accounting, and the
full mixed-version matrix pass; MANUAL idle advances control but not simulation; campaign
callers cannot misreport partial progress; post-STEP capacity receipts exist before G begins.

**Rollback:** Keep STEP available when selecting the legacy scheduler so new clients can receive an
explicit supported/legacy result. Binary rollback uses the tested `--legacy-engine` client adapter;
engine and Python are otherwise rolled back as a versioned pair.

### Phase 6 — Activate only after bake

**Goal:** Make fixed scheduling the default after engine and bundled tools prove compatible.

**Changes:**

The exact G state machine is ordinary admission → desktop presentation → desktop performance →
activation-100 → soak-24h → `land-exact`. Each step binds the same candidate/admission, active
oracle chains, process-invariant runtime launch template, its own planned process-launch set and
success/partial realized-closure evidence, input closure, and support-map digest plus the exact preceding success;
`land-exact` binds the canonical ordered four-success set. `g-gates-pending` owns the global
post-ledger reservation ID from admission until land or revocation; it does not hold the OS lock
between children, but every child acquires the one global lock and binds the reservation, so nightly,
repeat, replay/compatibility/baseline transitions, and unrelated admissions cannot interleave. Any gate red closes
its attempt and revokes the unlanded G admission as the ordered two-record vector of one atomic
`admitted-subject-logical-red-revocation-v1` transaction envelope. After the
soak succeeds, the only permitted next operation is exact landing; a land abort permits only its
exact reconciliation, and successful land closes the pending state.

1. Keep presentation defaults unchanged; update help text so fixed mode says TargetFPS is
   presentation-only while the legacy fallback explicitly retains frame-coupled game speed.
2. Test full campaign, realtime battle, turn-based battle, loading/video/briefing/debrief, modal,
   minimize/restore, and old-save/current-save flows.
3. Before any G source branch, reviewed head, candidate record, or execution exists, the exact live
   admitted-and-landed F integration head freezes `reference-desktop-v1`: its admission/SHA/tree,
   exact CPU, GPU, OS/build, power
   mode, compiler/build/runtime launch template, planned process set, realized per-process closure
   set, scenario/save/seed, profiler schema, and raw parent
   comparison receipts. P's machine contract owns the percentile, tail-fraction, and hard-ceiling
   thresholds; neither F nor G may tune
   them from candidate observations. Every F authority record and G intent/receipt binds the exact F
   admission, support/profile/spec/sample digests, and
   `admitted_f_performance_comparison_receipts_digest`. With fixed REALTIME and vsync/capture off,
   run three independently predeclared windows of exactly 10,000 retained samples in each
   late-city, realtime-battle, and turn-based-battle class. Evaluate all predicates separately over
   each 10,000-record run/class and again over the resulting 30,000-record aggregate/class; every
   individual and aggregate evaluation must pass. In each evaluation, nearest-rank p99.9 control
   transaction and non-turbo simulation transaction must each be
   `<=8,000,000 ns`; p99.9 control-opportunity completion lag from `D(c)` through the closing
   barrier must be `<=15,000,000 ns`, with at most 0.1% over 15 ms and a `100,000,000 ns` hard
   ceiling; p99.9 urgent-ready-to-first-service must be `<=16,666,666 ns`, with at most 0.1% over
   that threshold and the same hard ceiling. Non-turbo `pulse_cap` or `time_budget` debt drops,
   code-attributable missed control deadlines, and forbidden work drops must all be zero in each
   run/class and aggregate/class. Every raw
   record is retained and the validator recomputes deadlines, percentiles, fractions, and maxima.
   Presentation missed ordinals are measured/reported but excluded
   from this simulation/control gate because blocking swap/readback is measured separately. If
   swap/readback delays a due opportunity it still contributes to the deadline-relative lag and
   cannot be subtracted; separate attribution only explains where the time went.
4. `City Speed5/updateTurbo()` is the sole performance carveout. It remains atomic and is excluded
   from the component/lag limits, but candidate p99 and p99.9 must each be `<=125%` of the
   frozen admitted-F-parent values on the same recipe/hardware for the matched run/class and
   aggregate/class. Every opportunity executes exactly
   one turbo transaction, records duration/over-budget reason, starts no second automatic pulse,
   retires/drops debt exactly once, and completes the due primary control pair. Missing telemetry or
   timeout fails. Independently evidenced host contamination is retained as diagnosis, but after
   `attempt-execution-began` it is `desktop-performance-failed`; it cannot restart the workset.
5. F freezes—but cannot bind to a not-yet-existent G candidate—the candidate-independent
   `soak_schedule_policy_v1`: exact 24-hour duration, canonical ordinal and row/cycle formulas,
   concurrency, immediate replenishment/no-avoidable-idle policy, retention, per-campaign drain
   algebra, whole-attempt interruption behavior, and a `minimum_completed_campaigns` that is a
   multiple of 100 and at least 100. Candidate, subject-admission, runtime, input, support-map,
   topology-instance, and oracle-chain fields are forbidden from that F policy digest. Only after
   activation-100 succeeds, the serialized G `soak-intent` materializes
   `soak_workload_schedule_v1` for the exact subject admission by binding the F policy digest to the
   exact canonical 100-row activation topology/extension, G candidate/ref, support map,
   runtime/input closures, both active oracle chains, and capacity receipt. It does not bind an
   attempt origin, cutoff, process-plan authority, or realized prefix. The successful soak attempt's
   `attempt-execution-began` atomically publishes a fresh `soak_execution_window_v1` with the exact
   attempt/fence, monotonic clock domain, checked origin, and half-open cutoff. The workload repeats
   rows by gap-free campaign ordinal:
   `row = ordinal % 100`, `cycle = ordinal / 100`. Every
   campaign ordinal in the root prefix has one unique campaign claim and exactly one terminal;
   every root or continuation process separately consumes the next gap-free process-launch ordinal
   with a unique process/claim/port/output root. Campaign terminals biject the campaign prefix and
   realized process receipts biject the process prefix; root-start receipts separately biject the
   campaign prefix; repeats of a recipe/seed/profile occur only through the campaign formula. The
   checked monotonic root-launch window is half-open. A root becomes started only when the
   coordinator's successful-spawn/PID-start-identity linearization publishes its no-clobber
   same-clock `root_start_monotonic_ns` receipt. A root planned without that receipt before cutoff
   fails; its drain deadline is the checked sum of that bound timestamp and the F-frozen timeout. At
   cutoff, new roots stop, but already-started roots
   may execute only their predeclared save/reload continuation processes through bounded drain
   without allocating another campaign ordinal. A coordinator interruption after
   `attempt-execution-began` is `soak-failed` with no retry, resume, or receipt reuse; only a proven
   common pre-execution infrastructure terminal may create a new attempt/fence, distinct window,
   fresh dual prefixes, and full 24-hour duration before any worker/process begins. At 24 hours the Python
   coordinator stops root launches, drains every started campaign, and fails for completed below the
   minimum; missing/duplicate/extra/non-decided terminals; unexpected outcomes or milestones;
   timeout/partial advance/parked stage; crash/forced kill; stale nonce/PID/lease/job; lifecycle
   overflow; protocol/transport/input/runtime/evidence drift; unresolved accepted work;
   unauthorized MANUAL pulse; unexplained debt drop; or any started campaign missing its drain
   deadline. Only manifest-matched typed pause/transition/controller rebases and the turbo carveout
   are allowed.
6. Require exact `activation-100-v1` before the soak: exactly 100 terminal receipts and exact expected outcomes and
   ordered milestones. An “explained” gameplay-oracle delta is still failure; only non-oracle
   telemetry differences may be explained. Size it only from F's post-STEP capacity receipts.
   Before the G branch exists, freeze `desktop-presentation-v1` on the exact admitted F parent for
   every named supported presentation-device profile. Its candidate-independent manifest binds the
   R1p profile digest, one canonical `support_map_digest`, exact runtime/input closure, deterministic save/seed/transcript, expected
   Lifecycle/Status/UiDump values, and exact last-completed-surface SHA-256 checkpoints for MainMenu,
   paused City, every City speed selection, realtime and turn-based Battle, loading/video/briefing/
   debrief, modal entry/exit, old/current save reload, minimize/restore, focus loss/return, and resize.
   Pixel-equality checkpoints use MANUAL or query-confirmed Pause. The F-parent explicit-fixed run
   supplies expectations; G must reproduce them with the scheduler option omitted and with fixed
   explicit. Missing/extra/stale/blank/wrong-sized/unreadable/mismatched surfaces, candidate-authored
   expectations, a dummy/headless profile, presentation while minimized, or restore/resize failure
   within two seconds is red. The desktop-presentation, numeric-performance, activation-100, and
   24-hour-soak intents and every satisfying receipt bind that same admitted-F-authored
   `support_map_digest`; candidate G cannot add/remove/reclassify a profile. Presentation emits a
   typed receipt on every support-map presentation profile; performance, activation, and soak use
   only their separately frozen gate-specific execution profiles while binding that same complete
   support-map digest. The fixed 100-row activation cardinality has no hidden presentation-profile
   multiplier.
7. Flip the default only for the explicitly named desktop platform set. Until a separately reviewed
   mobile-class receipt exists, mobile/non-desktop stays legacy-default and reports
   `unsupported_unmeasured` in BuildInfo/docs; it does not block the scoped desktop G. A later mobile
   activation names hardware and freezes a separately reviewed #997 policy with `<=5,000,000 ns`
   p99.9 control, non-turbo pulse, and deadline-relative control-opportunity completion lag plus
   bounded tails and hard ceilings; it must pass the same activation/soak oracle before its own
   default flip.

**Done when:** The exact 100-run outcome/milestone oracle, every numeric desktop threshold, turbo
comparison, positive soak minimum with all started campaigns terminal, every
`desktop-presentation-v1` receipt, and an explicit profile-digest-keyed platform support map are
green on the exact G head. Human exploratory-play notes are
advisory: they cannot satisfy, waive, or reinterpret an automated gate, though a maintainer may
withhold release outside the evidence system. One compatibility release retains the legacy
scheduler fallback.

**Rollback:** Disable fixed scheduling. Save serialization is unchanged.

### Phase 7 — Combat parity inherited from #1237/#1216

**Goal:** Resolve the gameplay symptoms that motivated the timing PRs without conflating them with
the scheduler.

**Changes:** Red-first, original-evidence-backed tests for:

- battle walking and direct-flight speed;
- projectile speed and collision path in both city and battle;
- rate of fire independent of projectile, animation, render, and scheduler frequency, with the
  actual weapon fire-event interval matched to original-game evidence so a consistently wrong
  divisor cannot pass;
- explosion expansion cadence, visible/damage radius, depletion, and power;
- residual explosive smoke presence and dissipation versus smoke grenades;
- whether an explosion creates both its visual doodad/smoke and a distinct damage-type secondary
  hazard, rather than conflating that control-flow change with smoke lifetime;
- `Explosions damage instantly` both enabled and disabled;
- MAC starting AP ammunition, HE visible/damage radius, smoke, and effectiveness.
- HE projectiles detonating on wall collision rather than passing through or exploding beyond it;
- explosion damage/effects being contained by walls according to original-game collision rules.

Any correction ships per subsystem with a disposition-ledger update. Do not import the comment
1848612767 raw ROF divisor `4 → 5`, explosion depletion `/1.5`, smoke damage `30`, or other values
solely because #1237 looked closer.

**Done when:** Original-game evidence, deterministic OpenApoc regression, and both option states
agree for each subsystem.

**Rollback:** Revert the individual subsystem commit; scheduler remains unchanged.

### Phase 8 — Calendar, fuel, tick hardening, 180-TPS experiment, and saves

**Goal:** Make tick resolution a deliberate, testable parameter only after core parity is stable.

**Changes:**

1. Keep `FUEL_TICKS_PER_SECOND = TICKS_PER_SECOND`; derive `FUEL_TICKS_PER_UNIT` from a named vanilla
   duration while preserving the multiplier-4 value and strict threshold behavior.
2. Resolve `TICKS_PER_FRAME_UNIT`, `TICKS_PER_FRAME_MAP_PART`,
   `TICKS_PER_UNIT_TRAVELLED_BATTLEUNIT`, `Vehicle::nextFrame` animation delay, falling acceleration,
   explosion/hazard cadence, tactical AI interval, doodad lifetime, and projectile multiplier
   through the disposition ledger. Treat #1166's proposed 18-animation-FPS base as research, not a
   recovered fact.
3. Implement chronological calendar/invasion/turbo-fuel handling as a separate compatibility-
   controlled T-series change. Use `bc25e7f6` only as red-test and failure-case evidence: it proves
   boolean boundary flags saturate on large advances, but its category-at-final-clock loops are not
   the implementation. Segment an advance chronologically at the next relevant boundary, execute
   due work at that boundary's clock value, and prove equal results for equivalent partitions.
4. Define save behavior before changing the multiplier: versioned rescaling or explicit
   incompatibility for every serialized tick-denominated field.
5. Retain #1166's multiplier-1 variant as a negative/control experiment, then build multiplier 5/180
   experimentally and compare the Phase 0/7 matrices: movement, projectiles, ROF, explosions,
   smoke, AI, collision outcomes, fuel, turbo, existing saves, and mobile latency. Neither
   experiment changes the production default by implication.
6. Preserve `TICK_SCALE` unless evidence falsifies its multiplier-invariant role.

**Done when:** The 180-TPS experiment has a complete differential and save/mobile receipts. Enabling
it in production requires a separate reviewed decision; a failed experiment is a valid result.

**Rollback:** Keep production multiplier at 4; each hardening change remains independently tested.

### Compatibility-delta ledger

Only the rows below may differ from current behavior. “Default-60 match” means no lateness, visible
window, legacy multiplier 4, and no STEP lease. Everything else must match the goldens or add a
reviewed ledger row before code lands.

| Delta | Decision / owner | Current behavior | Intended behavior | Acceptance lock | Status |
|---|---|---|---|---|---|
| CD-01 lifecycle-reentrant StageCmd | **replace** / B | copied batch discards commands emitted during lifecycle callbacks | bounded live FIFO with the Phase 1 truth table | every StageCmd row plus cold SelectForces resume-POP | Frozen in B |
| CD-02 initial lifecycle | **replace** / B | initial stage is pushed directly; begin-emitted commands wait exactly one update, while lifecycle commands emitted during the copied batch can disappear | initial synthetic PUSH and begin-emitted commands share the first bounded transaction | begin-before-first-update and initial 64/65 tests | Frozen in B |
| CD-03 City refresh callbacks | **replace** / C2 | fourteen registrations call `CityView::update()` and may advance game time: six fixed organisation filters plus one for each of eight `NUM_TABS` buttons; the first Speed1 skip can conceal the defect | C preserves the characterized red path; authorized C2 wires named control refresh with zero simulation and no `skipSpeed1Tick` mutation | C1 production-policy `test_ui_timing`; C retains the red exact-binary fixture; C2 turns `tools/test_oa_city_refresh.py --manifest city-refresh-14-v1` green for all registrations from both Speed1 phases plus Speed2 | Pending C2 |
| CD-04 render/update-count presentation | **replace** / D0/D1/D2/D3 | animation, audio, hover, flash, and camera cadence follows render/update calls | named monotonic wall/control durations; render is pure | per-site 1/30/60/240 and 10,000-render purity tests | Pending D0/D1/D2/D3 |
| CD-05 TargetFPS meaning | **replace** / E/F/G | positive TargetFPS paces render and therefore simulation; zero divides by zero | zero is uncapped and negative falls back to 60 in both executors; fixed mode is presentation-only, while legacy REALTIME intentionally remains frame/simulation coupled. Activation uses the feasible frozen ten-profile set per oracle key: legacy REALTIME/1000, fixed REALTIME/0/1/30/60/240, and fixed MANUAL/STEP/0/1/60/240. Low-FPS legacy REALTIME/MANUAL whole campaigns remain outside cardinality as bounded diagnostics | E fixed 0/1/30/60/240 invariance; F/G fixed MANUAL pre-game lease/no-automatic-pulse proof; exact profile-set equality and all-ten same-key projection equality; separately characterized bounded legacy REALTIME diagnostics | Pending E/F/G |
| CD-06 overload debt | **replace** / C0/E | legacy advances the expected deadline once, resyncs only on strict `now > expectedOld+6D`, then runs one mixed update; its identical post-rebase warning predicate is unreachable | C0 locks below/equal/+1 and zero-warning behavior; E takes one scheduling sample, reserves one existing slot iff primary control is due **and debt is nonempty**, runs `min(4,debt-reserved)` historical plus at most the reserved primary, and enforces eight milliseconds between atomic boundaries. One-slot steady state is primary, not historical; empty debt is control-only and cannot underflow; it records exact ordinal-range stop/drop telemetry, completes one over-budget turbo, and still runs latched primary control | fake-clock threshold/rebase/next-sleep/zero-warning characterization plus exact inverse/lattice, 0/1/5/6-slot, raw-lateness/debt/time-stop/pulse/call-counter fixture with one over-budget turbo and no second pulse | Pending C0/E |
| CD-07 late-input ordering | **replace** / E | fresh events run before the only post-hitch update | historical debt runs before fresh gameplay input; urgent controls remain immediate | hitch plus input transcript | Pending E |
| CD-08 process terminal ordering | **replace** / B/E | several current paths directly set quit/clear stacks at different loop boundaries; harness QUIT is polled before update but its StageCmd drains after update | after admission opens, StageCmd/Harness/SDL QUIT, window close/external shutdown request, final committed empty stack, FrameLimit, SimulationPulseLimit, and fatal/overflow/invalid converge on one typed idempotent terminal procedure; transient clear/REPLACEALL/POP-empty-tail-PUSH do not; every suppressed accepted mutation retires with no watermark hole | parameterized no-post-terminal pulse/render/observation, limit boundaries/origin counts, cause precedence, direct-shutdown/window equality, and all-sequences-terminal tests | B portion frozen; E pending |
| CD-09 mutating harness work | **replace** / E | injected events/save/action execute as soon as polling/event dispatch reaches them | gameplay/mutating work receives one global sequence, executes at the next primary consistent-state barrier, and settles through its full closing barrier before any later-admitted observation on any connection | exhaustive classification plus same/fresh-connection ordering and failure-settlement tests | Pending E |
| CD-10 modal/suspension debt | **replace** / E | no separate debt exists | transition to control-only/covered state clears automatic debt; return never replays modal time | modal dwell/return transcript | Pending E |
| CD-11 frame identity and form registries | **lock/replace** / D5/E | `Form::liveForms()` tracks object lifetime for UiDump, while the separate harness-action visible-form registry is refreshed from update/render and deduplicated by `frameNumber`; BattleTileView's rendered `hiddenForm` is never updated | preserve the lifetime registry and UiDump unchanged; bind only the action registry to control epoch with existing order/hidden-descendant semantics; explicitly traverse/register `hiddenForm` without updating it; render mutates neither registry | C1 `test_ui_timing` synthetic dual-registry/controller fixture plus R1b exact-binary hiddenForm/form-tree transcript; D5 locks active-versus-covered, hidden-descendant, duplicate-ID order, and 10,000-render purity; E proves TargetFPS=1/minimized lookup | Pending D5/E |
| CD-12 transition discovery under lateness | **replace** / C2/E/F | only one mixed update can discover a hotseat/hidden/mission/dimension transition per render; City snapshots pre-pulse day, computes a human-city day/remote-player-vehicle branch that is overwritten by the alien-city local-player/falling-alien/projectile branch, then applies the debug outer predicate; speed sync precedes city mutation, whose `setCurrentCity` versus raw-assignment choice still uses the natural predicate | C locks the dormant parity seam; authorized C2 activates exact live-state recheck and target ordering; E invokes it for historical catch-up and F for jobs without reimplementation | C1/C characterize the complete City/Battle decision matrices; C2 exact-binary transcripts prove primary activation; E/F cover historical/job stop, stale/live drift, transition, and no-extra-pulse behavior | Pending C2/E/F |
| CD-13 video progression/EOF/skip | **replace** / D1 | `VideoScreen::render()` advances decoding, clears the final frame at EOF, presents one blank frame, then the next `update()` queues transition; empty/load-failed/no-first-frame video transitions on its first update; `eventOccurred()` uses the same null-frame sentinel for Escape/mouse/finger skip | explicit `NoFramePending`/`Playing`/`EofPending`/`SkipPending` control state owns progression; no-frame transitions on the first control heartbeat, EOF retains the last frame until the following heartbeat, and only Escape key-down/mouse-down/finger-down skip at the current primary closing barrier even from EOF pending | no-path/load-failure/no-first-frame fixtures, legacy blank characterization, last-frame suppressed/delayed/repeated-render goldens, Escape/mouse/finger skip tests from Playing/EofPending, and non-Escape negative test | Pending D1 |
| CD-14 presentation/window plumbing | **replace** / E | swap interval is set before a valid current context; hidden/minimized policy is implicit | set/query swap interval after context binding; suppress only genuinely non-presentable render/swap | context/swap integration and minimize/restore tests | Pending E |
| CD-15 STEP/manual execution | **replace** / F | automation accelerates by requesting 1000 renders and has no simulation lease | bounded retry-safe MANUAL jobs; control continues at 60 Hz and simulation advances only by accepted jobs | mixed-version, retry, expiry, cancellation, and TargetFPS-invariance tests | Pending F |
| CD-16 pulse-originated event ordering | **lock/replace** / C2/E/F | one mixed queue delivers events emitted by pulse N before the next frame's update, but a StageCmd queued by that delivery drains only after the outgoing stage executes that frame's pulse N+1 | C preserves and tags the one-extra-pulse characterization; authorized C2 activates causal-to-quiescence plus a live barrier before pulse N+1; E and F reuse it for historical/job pulses | C asserts legacy one-extra-pulse; C2 turns UfoRecoveryBegin, DefendTheBase, battle ZoomView, re-entrant form, and causal-overflow targets to zero-extra-pulse; E/F cover every origin | Pending C2/E/F |
| CD-17 cursor versus stage mouse motion | **replace** / E | every drained mouse event updates cursor and stage together before the frame renders | one mutation sequence covers latest urgent cursor position plus coalescible primary stage mouse/hover; it settles only after both halves commit or retire | TargetFPS 1/60/240 path plus same/fresh-connection Move→cursor/hover observation and coalescing fixture | Pending E |
| CD-18 initialization failure | **replace** / B | forced display initialization failure may crash/nonzero | framework exits cleanly but nonzero; never aliases explicit successful QUIT | forced SDL driver failure process test plus explicit QUIT control | Frozen in B |
| CD-19 stale accepted input after transition | **replace** / E/F | one-frame executor rarely exposes a generation mismatch | stage-targeted input accepted for an old generation is discarded with telemetry, never delivered to the replacement | transition-plus-input generation fixture | Pending E/F |
| CD-20 fresh-input overload | **replace** / E | framework translates/drains an effectively unbounded mixed queue each frame | accept at most 4,096 fresh envelopes, backpressure before accepting more, coalesce only motion/resize, and terminally settle every allocated mutation sequence | 4,096/4,097 harness+SDL pressure plus no-lost-sequence/watermark-gap fixture | Pending E |
| CD-21 control-originated form-event ownership | **lock/replace** / C/E | `pushFormEvent`/`click` transfer one event object to the queue, then iterate the emitting Control's live callback container synchronously on a borrowed pointer before later Stage delivery; callback mutation can alter iteration and dropping `RaisedBy`/final Control or Stage owners can invalidate live objects | preserve queue-before-callback; independently pin event, emitting Control, and originating committed Stage through outermost unwind; iterate a registration-order callback snapshot so same-type registration/removal applies next emission; give nested emissions independent pins/snapshots; forbid re-entrant drain/clear/StageCmd barriers until unwind; make callback event mutations visible to later delivery; retain exactly-once callbacks; tag queued delivery `control_originated`, deliver at the next primary with inherited nested origin, never during historical catch-up, and reject admission 4,097 before either observation | City speed-radio, `baseForm->update`, callback A clears `RaisedBy`/drops final Control+Stage owners/queues QUIT or REPLACE while B still runs once, callback-adds-C next-emission test, ordinary-stage, nested independently pinned objects, callback/delivery order and mutation visibility, pending-replacement generation, unknown-event rejection, and 4,096/4,097 fixtures | Pending C/E |
| CD-22 screenshot service barrier | **lock/replace** / E/F | harness Screenshot synchronously reads and writes during socket polling; SDL PRINTSCREEN captures then schedules an ad-hoc general-pool lambda that reaches global Framework data | preserve last-completed-surface semantics and the one existing collapsed wire error; after their respective urgent/primary barriers both sources reserve one of exactly 16 service slots before readback and main-thread-capture into one dedicated ScreenshotService with owned writer state. Harness uses ordered atomic replacement and file-before-`OK`; PRINTSCREEN reserves create-new names and has no reply. Full service gives Harness the ordered legacy error and logs/drops PRINTSCREEN without enqueue. Terminal cancels without post-close enqueue/wake/callback/global access, and `Framework::run()` joins before resources die | exact cross-connection mutation→Screenshot fence, 16/17 each/mixed saturation, pixel/hash fixture before update, internal no-surface/readback/write failures plus identical wire error, same-path/auto-name ordering, slow write, completion-gate races, published/old-final preservation, lifetime sentinels, and no-pending-request tests | Pending E/F |
| CD-23 deferred query lifetime | **replace** / R1a/E0/E/F | Query/Save execute against one global chain: GameState registration truncates, view handlers chain, and destructor-restored weak state keeps GS alive through campaign transition/debrief stages but leaves unknown-query/save-failed after expiry | remove global registration/restoration; E0 walks exact-generation view handlers, then resolves request-scoped GameState from an admitted session and same committed stack. Exactly once at the transaction barrier, the final committed stack/generation decides: City/Battle admits a weak-owner identity; non-null matching Briefing/PreStart/Loading/Debrief only carries it; no match clears it. Constructors/intermediate commands cannot change session state and null Loading never matches. Cold MainMenu setup cannot expose a new state before committed BattleView, while existing-game setup exposes only its prior admitted state. A tombstone preserves initial never-seen no-gamestate versus previously-seen unknown/save-failed bytes without retaining strong state; mismatch invokes neither route. BuildInfo/Lifecycle stay separate process reads | handler characterization; multi-command final-stack-only decision; pending BattleView/null Loading; pending/outgoing theft; nested equipment/dimension; campaign and cold Briefing→PreStart→Loading→BattleView; both debrief entries and debrief→City; initial versus existing-game Skirmish setup; skirmish-debrief/post-abandon MainMenu; win/loss Video Status plus no-provider errors; Save; disconnect/stale-generation; BuildInfo/Lifecycle tests | Pending R1a/E0/E/F |
| CD-24 ordered asynchronous execution/replies | **replace** / E0/E | inline sequential `readClient` executes each line before the next, while `sendAll` silently returns on any `n<=0`—including before the first byte or after a prefix—and has no errno-specific retry/suffix retention or stable connection identity | E0 assigns connection ID/epoch/ordinal and preserves immediate legacy execution plus reply order with bounded nonblocking output; E adds one global `mutation_seq`, terminal dispositions, contiguous watermark, and the sole `process_global_mutation_fence`; replies remain independently ordinal | `test_harness_dispatch`: partial→would-block→completion, one/64 interrupts, zero/fatal, disconnect/fd reuse, bounds; E proves Action/GS/input/resize→each observation across same/fresh connections, async replies, and every terminal disposition | Pending E0/E |
| CD-25 screenshot completion timing | **replace** / E/F | Screenshot blocks polling through readback, encoding, file write, and immediate reply | after its captured `process_global_mutation_fence`, E moves encode/write/publication to the shared ScreenshotService while retaining one measured main-thread readback and exact bytes; service OPEN/TERMINAL_CLOSED/JOINING states make reply retirement and teardown explicit; F's 2-ms budget covers admission/service/output but permits the measured non-preemptible readback and stops service afterward | cross-connection mutation→Screenshot order, slow-readback cooperative-bound telemetry, blocked encoder plus terminal/join deadline, slow/failing/cancelled write preserving prior bytes, exact legacy bytes, file-before-OK, dead/reused-connection suppression, and no post-terminal completion | Pending E/F |
| CD-26 config/input authority | **lock/replace** / R1a/R1b | shutdown honors `Config.Save`, but the startup valid-CD-picker path calls `config().save()` directly; `ConfigFileImpl::save()` has no internal option guard, and an invalid admission CD path can invoke an interactive picker and replace the input | every game-Framework persistence path passes one `Config.Save` authority check; Save=0 performs no create/open/write while Save=1 stays compatible; launcher explicit user-save remains separate. Compatibility-default CDPrompt permits current UI behavior, but validation sets false so invalid CD fails nonzero without picker/save. BuildInfo and receipts bind canonical effective Data/CD paths and validity | dirty shutdown, forced valid picker, init-failure/destruction Save=0 zero-write and unchanged bytes/hash/mode; CDPrompt false valid/invalid and picker-call sentinel; exact one Data/CD argv option plus queried path/digest equality; Save=1 persistence; launcher save; forbidden-ungated-call source scan | Pending R1a/R1b |
| CD-27 legacy lateness signal | **lock/replace** / C0/E | the source comment says the warning fired, but control flow rebases first and makes the warning predicate false; no usable legacy warning signal exists | treat pre-resync lateness and the exact strict threshold as the baseline, explicitly lock zero legacy warnings, and make E telemetry observe before rebase/cap/discard | `expectedOld+6D-1`, equality, `+1`, exact `now+D` rebase, following sleep, zero warning, and E raw-lateness fixtures | Pending C0/E |
| CD-28 cross-connection mutation visibility | **lock/replace** / E0/E/F | legacy inline execution orders an accepted mutation before a later command even though both bundled clients open a fresh TCP connection per command; a connection-local deferred fence would regress that behavior | connection ordinals retain local execution/reply order, while every command captures one process-global required mutation fence at admission; disconnect never revokes accepted mutation work and a fresh connection cannot bypass an earlier admitted mutation | `test_harness_dispatch` same/fresh-connection mutation→BuildInfo/Lifecycle/Status/UiDump/CONTROL-get/Screenshot matrix, fence-before/fence-after boundary, disposition gap, disconnect/fd reuse, plus `tools/test_oa_harness_protocol.py` using real per-command sockets | Pending E0/E/F |

Oracle disposition is closed: only CD-03, CD-12, and CD-16 may own replay or campaign projection
components, and only through C2's pre-candidate dual-surface authorization. CD-01/CD-02 and
CD-04..CD-11/CD-13..CD-15/CD-17..CD-28 authorize no oracle delta. Their prose goals and focused
tests never waive active replay, `merge-3-v1`, integrated-repeat-20, nightly, activation, or soak
requirements.

City preserves both Speed5 fallback orders: pre-pulse unavailability syncs the Speed1 radio before
the enable write, while post-turbo unavailability syncs the radio after the pulse; pending dimension
transition construction follows any dirty radio sync and skips ordinary remaining control.
Battle remains pre-control/pulse/post-control on the default-60 primary path. Pulse-originated events
remain before the next pulse. Control-originated queued deliveries wait for the next primary while their
registered form callbacks still execute synchronously at emission. Both screenshot paths still read
the last completed surface, and there is
no mid-heartbeat StageCmd barrier or change to window-close-before-update semantics. Those are
compatibility invariants, not deltas.

## 6. Risks

| Risk | Probability | Impact | Mitigation |
|---|---|---|---|
| City/Battle extraction omits a transition or side effect | Medium | High | Red transition/counter goldens; one pulse API; legacy executor first |
| Fresh SDL/harness input is replayed through historical catch-up | Medium | High | Classify and buffer gameplay commands; urgent allowlist; exact ordering tests |
| Pulse-originated game event is delayed past another pulse | Medium | High | Source-tagged causal lane; pre-pulse quiescence barrier; transition/pause event goldens |
| Accepted input is silently lost under queue pressure | Medium | High | Backpressure before acceptance; coalesce only motion/resize; stale-generation telemetry |
| Accepted QUIT still permits catch-up simulation | Low | High | Immediate pre-catch-up StageCmd barrier and terminal ordering test |
| Lifetime and action-form registries are conflated or still follow render cadence | Medium | High | Preserve distinct registries; separate control epoch; hidden/order and TargetFPS=1/minimized tests |
| Synchronous form callbacks drift from later Stage delivery, mutate the iterated callback container, or destroy the event/emitting Control/originating Stage | Medium | High | Atomic queue-before-callback admission, independent event/Control/Stage pins through unwind, registration-order callback snapshot, no re-entrant drain/barrier, terminal/destructor and same-type-registration sentinels, inherited origin scope, exactly-once mutation-visibility transcript, and 4,096/4,097 lock |
| Deferred query invokes a destroyed/pending view, loses debrief GameState, or retains state into MainMenu/Video | Medium | High | Exact-generation view walk plus request-scoped GameState resolved only from committed stages; no-state tombstone preserves bytes without ownership; constructor/destructor, debrief continuity, and clear-state ending tests |
| Deferred/partial socket replies truncate, reorder, or reach a reused fd | Medium | High | Connection epochs, request ordinals, bounded output slots, nonblocking suffix retention, and socketpair/fd-reuse tests |
| Fresh per-command connections bypass an earlier accepted mutation or leave a watermark hole | High before E | High | One process-global admission fence, exhaustive terminal dispositions, same/fresh-connection engine test, QUIT retirement, and real `oa_play`/`oa_harness` socket behavior |
| UI timing tests cannot link or require proprietary runtime data | High before C1 | High | Dedicated Forms/GameUI target, TestRenderer/null audio, generated forms, fake clock/video/controller seams, clean-checkout link/run gate |
| STATUS polling misses ephemeral lifecycle stages or journal overflow hides them | High before R1a | High | Transaction/generation lifecycle journal, explicit overflow-invalid result, source-accurate ordered milestone fixtures |
| Robot timeout/partial advance or stale unbound binary is recorded as success | High before R1b | High | Frozen R0 result truth plus R1a build/journal identity and R1b fresh source/tree/launch-template/planned-set/realized-set/input binding, executable victory path, and manifest-bound cardinality |
| Parallel cells collide on ports/PIDs or overwrite evidence | Medium | High | Coordinator-held port leases, PID+process nonce cleanup, exclusive cell claims, and no-clobber campaign/cell terminal publication |
| Robot-tested integration evidence is invalidated by rewritten history | Low | High | Append-only branch, exact-head ledger, fix-forward red stop rule |
| A frozen robot baseline or expectation is later independently proven wrong | Low | High | Append-only `baseline-invalidated` dependency closure immediately revokes every dependent receipt/admission and seals affected epochs; only a separate, pre-candidate, independently reviewed `baseline-replacement` can reactivate runs, and candidate-derived expectations are forbidden |
| A slice passes focused tests but breaks the accumulated game later | Medium | High | Block the next admission on cold lifecycle, replay, and three decided full campaigns; keep nightly rotation running on the exact integrated head |
| Two coordinators or a crash reuse one attempt root and mingle evidence | Medium | High | Exclusive attempt-directory and `attempt-claim.json` creation, baseline/manifest/coordinator binding, no adoption of stale empty roots, new ID after every crash, no-clobber terminal publication, and collision/non-reuse tests |
| A partial rollback creates an unsupported engine/client pair | Medium | High | Lifecycle matrix, HELLO negotiation, paired rollback, explicit legacy adapter |
| Lifecycle command cycle hangs | Low | High | Live FIFO with fatal 64-command cap |
| STEP response loss duplicates a lease/job | Medium | High | UUID idempotency, random lease, process ID, terminal records |
| Manual work starves control | Medium | High | 60-Hz priority; cooperative 8-pulse/8-ms slice |
| Single pulse or scheduler tail exceeds its latency budget | Medium | High | Phase 0 raw-sample benchmark; p99.9/tail/hard-ceiling activation gate; visible telemetry |
| Presentation conversion changes feel | Medium | Medium | Default-60 real-time goldens at 30/60/240 render rates |
| Combat constants are “fixed” by folklore | High | High | Original evidence required; disposition ledger; per-subsystem commits |
| 180 TPS changes collision/save behavior | High | High | Experimental only; collision differential and save policy first |
| Old driver silently loses acceleration | Medium | High | Version matrix and hard legacy-high-FPS diagnostic |

## 7. Parallel work streams

```text
Z -> P0 -> P
Z -> B
Z -> R0
Z -> A
R0 -> R1v
P + B -> R1a
R1a + R1v -> R1c
A + R1c -> R1p -> R1o -> R1b -> exact admission + landed repeat-20 -> C0 -> C1 -> C -> C2
C2 -> D0 -> D2
C2 -> D0 -> D3
C2 -> D1
D1 + D3 -> D5
D3 -> E0
D5 + E0 -> E -> F
F + D2 -> G
A + R1b -> P-movement/P-projectile/P-ROF/P-explosion/P-fire/P-MAC/P-physics/P-save/T-calendar/T-animation
A + R1b + P-explosion -> P-smoke
G + A + P-movement + P-projectile + P-ROF + P-explosion + P-fire + P-MAC + P-smoke + P-physics + P-save + T-calendar + T-animation -> T-180 experiment
```

True parallelism: presentation-site migration, combat evidence/tests, Python protocol tests, and
mobile benchmarking after their contracts are locked. Fake parallelism: Framework loop, Stage API,
City/Battle extraction, and STEP controller share lifecycle ordering and require one integrator.

Code/evidence admission path: Z -> P0 -> P; R0 -> R1v; P+B -> R1a; R1a+R1v -> R1c; A+R1c -> R1p -> R1o -> R1b -> exact R1b admission/landing/repeat-20 -> C0 -> C1 -> C -> C2 -> D0 -> D3, with
D1 parallel after C2; D3 then permits E0 and, together with D1, D5; E0+D5 -> E -> F -> G. R0 does
not block B's code merge, but no robot pass is admissible until reviewed R1b proves exact source/
build/launch-template/planned-process-set/realized-process-set identity. D2 runs after D0 beside the
D1 line and remains an activation prerequisite.
Combat P-series branches from A, not R0; T-180 waits for G and the exact landed constants,
movement/projectile/ROF/explosion/fire/MAC/smoke/physics/save/calendar/animation parents named above.

### Rollback lifecycle

Rollback means a different operation depending on how far the train has moved. Save compatibility,
engine/protocol compatibility, and bundled-tool compatibility are tracked separately; “the save
still loads” is not evidence that an older client can control a newer engine.

| Train state | Allowed rollback | Required follow-up |
|---|---|---|
| Slice has no descendants | Revert that owning PR/commit | Run its focused red/green test and the per-head smoke tier |
| Descendant PRs exist but are unmerged | Rebuild the descendant stack without the slice; never rewrite a tested integration epoch | `git range-diff`, full affected tests, new automated reviews for material heads, and new robot receipts |
| Integration epoch has robot receipts | Fix forward on the owning review branch and seal the revoked epoch. With a surviving live admission, use it as the ledger-selected logical acceptance parent but use exact current remote `develop`—including the revoked commits—as the transport/CAS parent; the correction must fast-forward from that transport head. If root revocation leaves none, use the currently active valid original-or-replacement baseline through the distinct recovery-root transition | The ledger—not the caller—names every remediated revocation and required tier. A robot-code red lands the exact reviewed fast-forward correction without rewriting history; an independently proven baseline-only error may validate and re-admit the same remote SHA/tree as a no-op land under its separately reviewed replacement. Rebuild affected descendants, rerun the union-specific plus every ordinary tier including exact `merge-3-v1`, and keep all failed receipts/red admissions immutable |
| Fixed scheduler is available but not default | Select the legacy executor with matching bundled tools | Re-run the mixed scheduler/protocol matrix; report scheduler fallback separately from binary rollback |
| STEP has shipped during its compatibility window | Keep STEP available with the legacy scheduler, or roll engine and Python back as a tested versioned pair | The `--legacy-engine` adapter is the only supported mixed rollback; never infer compatibility from a successful socket connection |
| Fixed scheduler is default, fallback retained | Disable fixed scheduling through the documented option | Re-run default-60 goldens, old-save load, and current-save roundtrip; presentation and StageCmd fixes remain in place |
| Compatibility window has ended | Revert the complete scheduler/protocol train to a previously tested release | Start a new integration epoch and regenerate every affected receipt; no partial wire-protocol rollback |
| Experimental multiplier 5 fails | Keep production at multiplier 4; revert only the owning T/P experiment | Retain negative differential evidence; no scheduler rollback is implied |

## 8. Validation strategy

Per code slice:

```sh
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix boost);/opt/homebrew" \
  -DENABLE_TESTS=ON -DBUILD_IMAGEDUMP=OFF -DBUILD_SERIALIZATIONTOOL=OFF \
  -S . -B build
cmake --build build -j$(sysctl -n hw.ncpu)
ctest --test-dir build --output-on-failure
./tools/lint.sh
./tools/check_ignored_binaries.sh
python3 tools/test_capture_timing_sources.py
python3 tools/capture_timing_sources.py verify-offline
python3 tools/test_timing_train_scope.py
python3 tools/test_timing_source_disposition.py
python3 tools/test_pr_p_scope.py --base <exact-landed-P0-event-base-sha>
python3 tools/test_oa_ai.py
python3 tools/test_oa_adversarial.py
python3 tools/test_oa_harness.py
python3 tools/test_oa_run_results.py
python3 tools/test_oa_skirmish.py
python3 tools/test_regen_compare_report.py

# Required once the named ancestor exists:
cmake --build build --target test_admission_observability
ctest --test-dir build --output-on-failure -R '^test_admission_observability$'
cmake --build build --target test_harness test_harness_dispatch
ctest --test-dir build --output-on-failure -R '^(test_harness|test_harness_dispatch)$'
python3 tools/test_oa_victory.py
python3 tools/test_oa_campaign_controls.py
python3 tools/test_oa_robot_admission.py
python3 tools/test_oa_campaign_batch.py
cmake --build build --target test_ui_timing
ctest --test-dir build --output-on-failure -R '^test_ui_timing$'
cmake --build build --target test_step_protocol
ctest --test-dir build --output-on-failure -R '^test_step_protocol$'
python3 tools/test_oa_harness_protocol.py
```

The Python scripts are invoked directly because generic `unittest discover` currently discovers
zero tests in this repository and exits nonzero; that command is not accepted as Python coverage.
P supplies the source-capture, source-disposition, train-scope, and planning-scope structural tests;
R1c supplies
`test_oa_campaign_controls.py`. `tools/test_oa_run_results.py` is supplied by R0 and is required on every branch whose base includes
R0; independent Z-based A/B branches run only the scripts present on their immutable base plus their
own focused tests. R1a supplies `test_admission_observability`; R1b supplies the three robot tests
shown above; C1 supplies the dedicated
`test_ui_timing` target. E0/E supply `test_harness_dispatch`, F supplies `test_step_protocol`, and
the actual short-lived socket behavior is covered by `tools/test_oa_harness_protocol.py`. A real admission run uses
`python3 tools/oa_robot_gate.py run --manifest <immutable-evidence-root>/baselines/<baseline-id>/<manifest-sha256>/merge-3-v1.json
--manifest-sha256 <manifest-sha256> --tested-head <exact-sha> --out <immutable-evidence-root>`.
That content-addressed file is the exact R1o materialized baseline manifest named by `epoch-init`;
the gate verifies its JCS digest before use and forbids any repo-side or candidate-generated judging
copy. The command itself enforces the live
candidate-record precondition, the ordinary post-genesis live-parent admission rule, and exact
manifest cardinality. `epoch-init` may create a genesis candidate with no live logical admission
only while the complete ledger contains no admission; exact current remote `develop` is still its
transport/first/CAS parent. Failed genesis attempts remain immutable and a new reviewed head may
retry against the same valid active baseline. `baseline-invalidate` atomically requires the invalid
baseline digest, independent proof/human-review artifact, exact retired fields, and exact ledger-
computed dependent closure; when admitted subjects are affected it commits
`[baseline-invalidated, revoked]` in one transaction envelope, seals the named epochs, and immediately blocks old-
baseline run/admit. `baseline-replace` requires that invalidation, a separately reviewed immutable
expectation artifact with complete schema/manifests/inputs/hashes/outcomes/milestones and source
provenance, and proof it was frozen before candidate creation/execution. Candidate output can never
source it. With no prior admission the replacement can seed a new genesis; otherwise recovery rules
apply. Recovery after an admitted-head red normally opens a new epoch with the ledger-selected last
live admitted ancestor as logical acceptance parent and exact current remote `develop` SHA/tree as
transport/CAS parent; a code correction must fast-forward from that transport parent. The caller
supplies one lookup trigger, while the gate
derives the union of every unresolved reachable trigger, the candidate's exact `remediates`, and
required tiers, rejecting caller-authored set differences. If
cascading root revocation leaves prior admissions but zero live admissions,
`epoch-init --recovery-root` instead retains the original immutable reviewed root code/tree as
oracle provenance only, requires the complete active original-or-replacement baseline chain with
its frozen `replay_schema_set`, manifests, expected hashes, outcomes, and milestones, and binds exact
current remote `develop`, including revoked history,
as transport/first/CAS parent. Its reviewed-source candidate has ordered parents
`[transport, reviewed recovery source]`; a baseline-only no-op equals transport exactly. The caller
supplies one lookup `trigger_record_id`. The ledger derives sealed epochs, all typed
trigger payloads, exact unresolved revocations, and the union of required tiers. Any robot-red member binds a reviewed
corrected R1b/root head/tree; baseline-only invalidation may retest the same exact SHA/tree under the
independently reviewed replacement. It rejects any missing replacement, stale baseline,
caller-supplied-set mismatch, dependency-closure mismatch, and candidate-derived
recalibration, reruns every robot-failed tier and every tier whose expectation/schema/manifest was
retired by any invalidation, plus all
ordinary tiers including exact `merge-3-v1`, and cannot reopen genesis. Genesis likewise requires
exact `merge-3-v1`. `admit` is available only after the immutable green
attempt is published.

Runtime matrix: TargetFPS 0/1/30/60/240; vsync 0/1; focused/unfocused/minimized/resumed; every
city/battle speed; hidden display; REALTIME/MANUAL/STEP success/retry/transition/cancel/expiry/
restart; Boot/loading/video/briefing/debrief; old save load and current save roundtrip; desktop and
mobile-class performance.

Combat matrix: city/battle projectiles, walking, ROF, explosions/smoke with instant-damage option
off/on, and MAC loadout/HE behavior against original evidence.

Bake: no pass evidence before reviewed R1b; ten-minute late-save profiling; per-head smoke. A
pre-R1b smoke is diagnostic and explicitly untrusted; only an exact head containing reviewed R1b, or an
integration head that records R1b as an ancestor and proves exact source/build, invariant launch
template, planned process set, realized per-process closure set, and input-closure equality,
may emit an admissible pass receipt. The bake requires exact `merge-3-v1` for genesis, every recovery,
and each append-only integration merge; capacity only sizes concurrency. Every exact runtime
landing from R1b through G, every P/T parity slice, and T-180 must then win twenty fresh whole-game
campaigns on its exact admitted-and-landed `develop` SHA/tree before any descendant branch or later
runtime landing may exist. The permanent proof belongs to that landing; later heads cannot replace
it. The independent gap-free nightly-20 continues every 24 hours while the train is active:
pre-fixed uses all five recipes four times at legacy/1000; fixed-before-STEP uses two rotating keys
across ten feasible legacy/fixed REALTIME rows; post-STEP uses two rotating keys across the exact
ten-profile activation topology. Then activation requires the exact `activation-100-v1` manifest:
ten frozen oracle keys across legacy REALTIME/1000, fixed REALTIME/0/1/30/60/240, and fixed
MANUAL/STEP/0/1/60/240, with every same-key projection identical to the resolved effective oracle.
Low-FPS legacy whole campaigns remain outside cardinality as bounded diagnostics. The separately
summarized 24-hour automation soak binds both effective oracle chains. Any
later red appends cascading revocations for the tested head and descendants and blocks the gate
until a reviewed fix-forward produces a new live admission.

## 9. Model and thinking recommendations

As of 2026-08-26:

- Framework/Stage/scheduler/STEP state machine: frontier coding model, extra-high reasoning, one
  staff review per material head.
- Presentation migrations and mechanical constants: strong coding model, medium reasoning, backed
  by purity/unit tests.
- Combat/Ghidra evidence and 180 differential: long-context research model plus runtime/original
  evidence; no model-only approval.
- Runtime performance and gameplay feel are poorly suited to model-only validation. Blocking claims
  require the measured manifests/receipts above; optional human exploratory play is advisory only.

## 10. Confidence

Core Phases 0–6: 70–82% design confidence after adversarial review. The scheduler order and
StageCmd controller are now closed on paper, but uncertainty remains in City/Battle extraction,
pending-control transitions during catch-up, real blocking-swap behavior, and pulse-latency tails.
The C extraction spike, exact goldens, reviewed R0/B heads, and a late-save benchmark would
raise this about ten points.

Combat Phase 7: 60–80% until original-game measurements are bound to deterministic fixtures.

Tick/save/180 Phase 8: 45–70%. The assumption most likely to invalidate it is that collision and
serialized tick state can be normalized without subsystem-specific semantics. A clean collision,
save, and mobile differential would raise confidence materially; otherwise multiplier 4 remains the
correct production choice.
