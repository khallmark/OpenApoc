# Baseline: `develop` reboot, 2026-10-07

This is the control arm. Every later merge into `develop` is judged against these numbers. The
tree was `khallmark/memo-log-archive-20260828` (`d35334c0`), which is metal-renderer plus the
mod-path fix, plus the `OA_WATCH` tweak (`da3255c9`).

| Check | Result |
|---|---|
| Configure | `-DCMAKE_BUILD_TYPE=RelWithDebInfo -DENABLE_TESTS=ON` (Makefiles), clean worktree |
| Build | clean, 1m27s wall time with a warm ccache, 0 errors |
| `data/mods/base/base_gamestate` | **306k**, from this tree's own extractor. Master-lineage data is ~299k and fails 7 `test_city_rules` cases. |
| `ctest --output-on-failure -j6` | **38/38 passed**, 3.1 s |
| Metal, validation on, MainMenu | `MetalRenderer` selected; 600-frame profile: draw 1.17 ms, GPU 0.43 ms/cmdbuf; stderr clean |
| Watched run | `OA_WATCH=1 MTL_DEBUG_LAYER=1 MTL_DEBUG_LAYER_ERROR_MODE=nslog MTL_SHADER_VALIDATION=1 python3 tools/oa_play.py --days 3` |
| — outcome | exit 0. Day 1 to day 5, research complete 1 to 3, score 100, funds 130000 to 127380 |
| — stages | MainMenu, DifficultyMenu, CityView, Research, BuyAndSell, Recruit, Base, Building, Alert, Score, Ufopaedia |
| — Metal validation | **0** `AGX:` / failed-assertion lines over ~10k frames at 60 FPS |
| Launches | 1 of 1 clean. The crash rate is not yet measured; the Phase 4 soak (40 launches) sets it. |

Known open at baseline (`docs/HANDOFF.md`, `tools/oa_skirmish.py`):
- `map::at` SIGABRT, about 1 in 20 runs;
- battle-generation SIGSEGV after `initialMapPartLinkUp`;
- skirmish round-2 process vanish;
- `BATTLEMAP_43sleep` generation failure.

## Changes since baseline

### Skirmish ports from `feat/main-menu-skirmish`
Commits `6405f509`, `69f67eca` and `44b17cf7`, cherry-picked from `1244e3ac`, `90dfe6c0` and
`b0cdbfa6`.

**Not ported:**
- `81fb2feb`: a cosmetic rewrite of the forms. It renames controls that `oa_skirmish.py` addresses by name.
- `b0da7472` (GL 2.0 `GL_LUMINANCE`): refuted three ways as the black-screen cause, and inert.

**Gate:** clean build; 38/38 `ctest`; `base_gamestate` 306k.

**A/B, `oa_skirmish.py --rounds 1 --map-row 0` (base defence, default roster).** Same data and
driver; only the binary differs. The control is the baseline snapshot, run via `OA_BUILD_DIR=build-control`.

| arm | foes at start | outcome |
|---|---|---|
| control (`f8693d39`) | 8 | never resolved: 12 "no progress" stalls, summary "other" |
| ported (`44b17cf7`) | 20 | resolved in 78 s, 14 survivors |
