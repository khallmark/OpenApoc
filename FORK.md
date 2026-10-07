# khallmark/OpenApoc — the `develop` fork

This is a working fork of [OpenApoc/OpenApoc](https://github.com/OpenApoc/OpenApoc). It is not a
series of pull requests. `develop` is the only branch that matters here. It is built to do three
things:

1. **Play the game** on macOS. The default renderer is Metal; GL 2.0 and GLES 3.0 are fallbacks.
2. **Automate it.** A localhost command socket in the engine is driven by Python, through the
   real UI, with no cheats. Campaigns, skirmishes and AI-vs-AI arenas run unattended.
3. **Watch it happen.** The automation runs in a normal, visible window at a speed a person can
   follow.

**Upstream maintainers: take whatever you want.** There's no need to ask, and no PR will arrive.
Everything is GPL3, like the parent. The list at the bottom points at the commits that stand on
their own.

## Build, play, watch (macOS, Apple silicon)

You need your own copy of the game. Point `data/cd.iso` at it as a **real file**: PhysFS ignores
symlinks, so use `cp -c` for a free APFS clone.

```sh
brew install cmake boost pkg-config sdl2 qt@6 libvorbis ccache
git submodule update --init --recursive
cp -c /path/to/your/cd.iso data/cd.iso
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DENABLE_TESTS=ON \
  -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix boost);/opt/homebrew"
cmake --build build -j"$(sysctl -n hw.ncpu)"   # also extracts data/mods/base/base_gamestate
```

- **Play:** `./build/bin/OpenApoc.app/Contents/MacOS/OpenApoc --Framework.Data="$PWD/data" --Framework.CD="$PWD/data/cd.iso"`
- **Watch a robot play three game days:** `python3 tools/oa_play.py --watch --days 3`
  - `--watch` raises the window, caps the game at 60 FPS, and prints one narration line per
    decision.
  - `--city-speed 1..5` caps the city clock; 5 is turbo.
  - `--step-delay S` pauses between the robot's actions.
  - `--ai veteran` (or a plugin in `tools/ai_plugins/`) fights the battles.
  - The same flags work on `oa_campaign.py` (resumable, real-world hours), `oa_victory.py` (play
    to the win) and `oa_skirmish.py` (one battle).
- **Test:** `ctest --test-dir build --output-on-failure`. Always run tests through `ctest`. Ten of
  the binaries need the gamestate path that `ctest` supplies, and exit 0 without testing anything
  if run bare.

More detail:
- [`docs/local-development.md`](docs/local-development.md): renderers, Metal validation, sanitizers, iOS.
- [`docs/playing-the-game.md`](docs/playing-the-game.md): the automation.
- [`docs/HANDOFF.md`](docs/HANDOFF.md): where the work stands.

## What is different from upstream

- **Rendering:**
  - A Metal backend (`framework/render/metal/`), pixel-identical to GL on macOS.
  - GLES 3.0 on a macOS core profile.
  - GL 2.0 indexed-quad batching.
- **Automation:**
  - `framework/harness.{h,cpp}`: a localhost socket for clicks, keys, named controls, game-state
    queries and screenshots. It is off unless `--Framework.Harness.Enable=1`.
  - `tools/oa_*.py`: campaign, victory, skirmish, arena and adversarial co-evolution drivers.
- **Fidelity:**
  - Game-rule tables extracted from the original executables, with citations (`docs/original-game/`).
  - Many constants corrected to match the binary.
- **Skirmish mode** is reachable from the main menu.
- **iOS / iPadOS** builds (`ios-simulator` / `ios-device` CMake presets).

## Self-contained commits upstream may want

These are SHAs on `develop`. Each one makes sense on its own; most cherry-pick with little or no
conflict.

| Commit | What |
|---|---|
| `91e1234c` | Metal backend and renderer parity tooling |
| `4f475f86` | `NotificationScreen::resume()` unguarded `optionsMap.at()`: a covered alien-takeover notice aborted the game (upstream has the same line) |
| `353c3a7c` | `Control::click()` sent a click with no mouse button |
| `2b849afe` | GLES 3.0 on macOS behind an opt-in core profile |
| `586d8b33` | GL 2.0: batched quads through an index buffer |
| `63072fa2` | Mod path follows the data path |
| `e0bf7240` | Dangling lab `StateRef` after base destruction |
| `3f8f8ebf` | `initState` segfault on a resumed save |
| `5316eb12` | A missing footstep sound no longer aborts map generation |
| `84872867` | A fall diagnostic segfaulting battle generation |
| `356aa63d` | Developer tile dump running (and segfaulting) on every Ctrl-click |
| `43478c85` | Skirmish: two stage commands were swallowed, so no battle started |
| `0a56394a` | Skirmish: report the no-location case instead of hanging |
| `a2908c85` | City: stop destroying ground vehicles that merely cannot reach a target |
| `e83bc445` | City: the UFO withdrawal floor is inclusive |
| `eac72570` | Battle: large-unit occupancy and line-of-sight geometry |
| `efd0089a`, `f3ded10c` | Battle: disruptor shield absorption and recharge |
| `2044943c` | `AlienAI.Behaviour` was declared and read by nothing |
| `6bfa137a`, `0b02537e`, `36cf3070` | Framework: exceptions and hard faults say where they died |
| `a5a7ec96`, `c0a8ebbf`, `60cbaa95` | iOS: touch id truncation, presented framebuffer, 2x UI scale |

Older work that never reached `develop` is preserved as tags `archive/2026-10-07/<branch>`.
