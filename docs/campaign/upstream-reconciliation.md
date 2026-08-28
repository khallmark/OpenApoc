# Upstream reconciliation — OPE-1

This document is the deliverable for OPE-1 ("Reconcile landed and in-flight upstream PRs"). It produces no code change against `master`. It records which upstream pull requests are landed or in flight, classifies every file in `origin/develop`'s outgoing range against `OpenApoc/master`, and flags every place a campaign mini (OPE-7..OPE-22) would collide with an in-flight upstream PR.

Remotes used: `OpenApoc` = canonical `OpenApoc/OpenApoc` (read-only reference point for this classification). `origin` = `khallmark/OpenApoc` (the fork this document is committed and PR'd against). The remote named `upstream` in some local checkouts is a mirror of the fork, not the canonical project, and was not used for any comparison here.

A note on cross-repo references: this document refers to upstream pull/issue numbers as plain text ("upstream PR 1634", "upstream issue 1631") rather than `owner/repo#N` or a full URL, everywhere it appears in a commit message or PR title/body for this change, to avoid generating an unwanted cross-repo notification on `OpenApoc/OpenApoc`. Inside this file's own body, bare numbers (`#1634`) are used interchangeably with "PR 1634" purely for readability — GitHub does not generate cross-repo notification events from file content, only from commit messages, and issue/PR titles, bodies, and comments.

## 1. Method

1. Read OPE-1 and the parent project ("OpenApoc Develop Cleanup — Mini PR Campaign") in full via the Linear MCP tools.
2. Confirmed each named PR's current state and exact file list with `gh pr view <n> --repo OpenApoc/OpenApoc --json ...` (read-only; generates no notification).
3. Read the full body of PRs 1628, 1633, 1634, 1635, and 1636 to understand what each one actually changes, not just its title.
4. Computed the outgoing file range two ways (see section 2 — this surfaced a real methodological gap) and settled on the tip-to-tip diff as authoritative.
5. For every file named in one of the six PRs, cross-checked whether `origin/develop`'s own history touches the same function/hunk (not just the same file) by reading the actual diff content, not just the file list.
6. Grouped `origin/develop`'s 16 squashed commits by file, which gave a first-pass thematic grouping (e.g. "city: UFO mission-counter transition, damaged withdrawal, and org fixes" as one commit spanning ~30 files) that the per-file classification below refines.
7. Classified every path into exactly one of: `merged-upstream`, `in-flight-upstream` (PR named), `campaign-mini` (OPE ticket named where known), `out-of-scope`, `generated-or-private`, `obsolete`.

## 2. Comparison methodology — a discrepancy worth flagging

The ticket brief and the project's own "Current evidence" section quote two different file counts for the outgoing range: **336 files** (brief) and **337 files, +46,132/-2,959** (project description). Both numbers are real, and the reason they differ matters for anyone extracting a mini from this range:

- `git diff --name-only OpenApoc/master...origin/develop` (**triple-dot**, i.e. against the *merge-base* of the two refs) returns **336 files**, +46,098/-2,958.
- `git diff --name-only OpenApoc/master origin/develop` (**direct tip-to-tip**) returns **337 files**, +46,135/-3,161 — matching the project description almost exactly.

The triple-dot form is anchored at `git merge-base OpenApoc/master origin/develop` (`ec4b231b`), **not** at `master`'s current tip (`771ba686`). Between those two commits, `master` picked up three independent changes that `develop` never saw:

- **`game/state/city/base.cpp`** — PR 1627 merged here (the `Base::die()` iterator-invalidation fix).
- **`CODE_STYLE.md`** — rewritten by PR 1630 ("docs: add development guide"), independent of develop's own edits to the same file.
- **`DEVELOPMENT.md`** — added by PR 1630; develop never had this file at all.

The triple-dot diff shows `develop`'s content **against the pre-1627/pre-1630 baseline**, which makes `base.cpp` look like a full duplicate of PR 1627 and makes `CODE_STYLE.md` look like a bigger diff than it really is against current `master`. The direct tip-to-tip diff is the one that answers the actual question this ticket asks ("what does `develop` still need to contribute, given what's already on `master`"), so **this document classifies the 337-file tip-to-tip range**, and calls out `base.cpp` and `CODE_STYLE.md` individually (sections 9 and the table below) rather than silently trusting either file count. `DEVELOPMENT.md` is the one row in the 337 that isn't really a `develop` contribution at all (`master` has it, `develop` doesn't) — see its row for detail.

Anyone re-running this classification after any of the six named PRs merge should re-diff tip-to-tip against the new `master`, not trust a cached 336/337 count.

## 3. Named PR status

| PR | State | Base → Head | Files | What it actually is |
|---|---|---|---|---|
| upstream PR 1626 | **OPEN** | `master` ← `develop` | 100 (GitHub caps the listing; the real diff is 337) | The superseded umbrella PR ("So what do we do about this.") that first proposed breaking up `develop`. Superseded by this per-mini campaign; stays open until Wave 5 explicitly retires it (OPE-24). |
| upstream PR 1627 | **MERGED** (2026-08-26) | `master` ← `fix/base-die-iterator-invalidation` | 1 (`game/state/city/base.cpp`) | `Base::die()` use-after-free fix (copy `currentAgents`/`currentVehicles` before iterating). Confirmed already in `771ba686`. |
| upstream PR 1628 | **OPEN** | `master` ← `feat/main-menu-skirmish` | 12 | Adds a Skirmish-mode button to the main menu, plus four follow-up UI/crash/data cleanup commits. Explicitly out of scope for this bug-fix campaign (feature stack) per the project's Out-of-Scope section. |
| upstream PR 1633 | **OPEN** | `master` ← `harness/socket-infrastructure` | 22 | A localhost command-socket driver for automated play (`CONTROL`, `CLICK`, `SCREENSHOT`, etc.). Explicitly out of scope (harness/live-tooling feature stack). |
| upstream PR 1634 | **OPEN** | `master` ← `fix/flying-target-scan-bounds` | 1 (`game/state/city/vehiclemission.cpp`) | Bounds-checks the `PickNearbyPoint` branch of `VehicleTargetHelper::adjustTargetToClosestFlying` before `getTile()`. `Refs #1617` (reporter save not replayed). |
| upstream PR 1635 | **OPEN** | `master` ← `fix/stage-commands-iterator` | 1 (`framework/framework.cpp`) | Iterates a copy of `stageCommands` in `Framework::run()` so a `REPLACEALL`/`QUIT` teardown that re-queues a stage command can't invalidate the range-for. Defensive fix, not a reproduced crash. |
| upstream PR 1636 | **OPEN** | `master` ← `khallmark/gl20-palette-format` | 1 (`framework/render/gl20/ogl_2_0_renderer.cpp`) | The macOS black-window fix: palette-index textures use `GL_LUMINANCE` instead of `GL_RED` on a GL 2.1 context (`GL_RED` needs GL 3.0's `ARB_texture_rg`, and the GL2 backend on macOS never gets 3.0). **Must not be duplicated by any mini** — see section 8. |

## 4. Classification legend and bucket counts (337 files)

| Bucket | Count | Meaning |
|---|---|---|
| `campaign-mini` | 172 | Belongs to the bug-fix campaign; owning OPE ticket named where confidently known, "needs triage" flagged where not. |
| `out-of-scope` | 132 | Feature stack (skirmish, harness, adversarial tooling, render/timing/platform), research/manufacturing parity without a demonstrated bug, or fork-private tooling/process docs — all explicitly excluded by the project's Out-of-Scope section. |
| `in-flight-upstream` | 23 | Already covered by an open upstream PR (1628, 1633, 1635, or 1636). No mini may duplicate these. |
| `obsolete` | 7 | Reference XML deleted by develop's own later commit once the extractor supplies the same data — matches the project's explicit "delete only after all consumers moved" rule. |
| `generated-or-private` | 3 | Must never be tracked (tracked `.pyc`, agent-local Claude Code config). |
| **Total** | **337** | |

`merged-upstream` has **zero** file-level rows. The only file touched by a merged PR (`game/state/city/base.cpp`, PR 1627) also carries substantial *unmerged* new content, so at file granularity it's classified `campaign-mini` with the merged hunk called out explicitly — see section 9. No file in the range is *purely* a merged, closed-out duplicate.

## 5. Full file classification

Grouped by directory/theme for readability; every one of the 337 files appears in exactly one group and has exactly one classification. Expand each section.

<details>
<summary><strong>Root, editor/agent config, and CI</strong> (21 files)</summary>


| Path | Classification | Note |
|---|---|---|
| `.cbmignore` | out-of-scope | Fork-private codebase-memory tool ignore file; not upstream-relevant. |
| `.claude/devkit-hud.local.json` | generated-or-private | Agent-local Claude Code config; not upstream-relevant, must not be tracked. |
| `.claude/devkit.local.md` | generated-or-private | Agent-local Claude Code config; not upstream-relevant, must not be tracked. |
| `.cursor/rules/openapoc-fork.mdc` | out-of-scope | Fork-private Cursor IDE tooling/skill config; not upstream-relevant. |
| `.cursor/skills/openapoc-parity/SKILL.md` | out-of-scope | Fork-private Cursor IDE tooling/skill config; not upstream-relevant. |
| `.cursor/skills/openapoc-parity/lx-import.md` | out-of-scope | Fork-private Cursor IDE tooling/skill config; not upstream-relevant. |
| `.cursorignore` | out-of-scope | Fork-private Cursor ignore file; not upstream-relevant. |
| `.github/PULL_REQUEST_TEMPLATE.md` | campaign-mini | OPE-5. Confirmed by content: Gap-matrix/Testing checklist referencing check_ignored_binaries.sh and regen_compare_report.py, i.e. codifies the Wave-0 hygiene conventions as a contributor template. |
| `.github/workflows/harness.yml` | campaign-mini | OPE-5, NOT the harness feature despite the filename. Confirmed by content: this workflow only runs check_ignored_binaries.sh and regen_compare_report.py --check -- the hygiene checks Wave 0 describes, unrelated to the framework/harness socket driver (upstream PR 1633). |
| `.gitignore` | campaign-mini | OPE-5: adds depot_7661/, docs/original-game/.local/, tools/__pycache__/ ignore rules -- exactly the ignored-binary-policy scope. |
| `.vscode/launch.json` | out-of-scope | Fork-private editor config; not upstream-relevant. |
| `.vscode/tasks.json` | out-of-scope | Fork-private editor config; not upstream-relevant. |
| `.worktree/init.sh` | out-of-scope | Fork-private dev-environment tooling; not upstream-relevant. |
| `.worktree/test.sh` | out-of-scope | Fork-private dev-environment tooling; not upstream-relevant. |
| `AGENTS.md` | out-of-scope | Fork-private agent contribution conventions; not upstream-relevant. |
| `CMAKE_CXX_FLAGS_RELWITHDEBINFO` | out-of-scope | Fork build-tooling file. |
| `CMakeLists.txt` | out-of-scope | Root build file changes bundled with Apple/iOS platform-support tooling. |
| `CMakePresets.json` | out-of-scope | Fork build-tooling file. |
| `CODE_STYLE.md` | out-of-scope | Fork editor/style doc. NOTE: master independently rewrote this file's prose via upstream PR 1630 (typo fixes, references DEVELOPMENT.md); develop's copy is based on the pre-1630 wording plus fork-tooling edits. Any future touch must diff against current master tip, not develop, or it regresses master's wording. |
| `DEVELOPMENT.md` | out-of-scope | METHODOLOGY NOTE (see section 2): master-only file added by upstream PR 1630 after the fork point; develop never had it, so this is not really a develop contribution to reconcile. Classified out-of-scope for schema completeness only; excluded from the 336 vs 337 discussion as a non-content row. |
| `README.md` | out-of-scope | Top-level README, fork-process content, not slice-specific. |

</details>


<details>
<summary><strong>CMake / Apple-iOS-macOS platform build</strong> (10 files)</summary>


| Path | Classification | Note |
|---|---|---|
| `cmake/apple.cmake` | out-of-scope | Apple/iOS/macOS platform-support build tooling -- feature stack tracked elsewhere, per the project's Out-of-Scope section. |
| `cmake/apple/copy_bundle_data.cmake` | out-of-scope | Apple/iOS/macOS platform-support build tooling -- feature stack tracked elsewhere, per the project's Out-of-Scope section. |
| `cmake/ios/Info.plist.in` | out-of-scope | Apple/iOS/macOS platform-support build tooling -- feature stack tracked elsewhere, per the project's Out-of-Scope section. |
| `cmake/ios/LaunchScreen.storyboard` | out-of-scope | Apple/iOS/macOS platform-support build tooling -- feature stack tracked elsewhere, per the project's Out-of-Scope section. |
| `cmake/macos/Info.plist.in` | out-of-scope | Apple/iOS/macOS platform-support build tooling -- feature stack tracked elsewhere, per the project's Out-of-Scope section. |
| `cmake/macos/OpenApoc.entitlements` | out-of-scope | Apple/iOS/macOS platform-support build tooling -- feature stack tracked elsewhere, per the project's Out-of-Scope section. |
| `cmake/macos/generate_icon.py` | out-of-scope | Apple/iOS/macOS platform-support build tooling -- feature stack tracked elsewhere, per the project's Out-of-Scope section. |
| `cmake/macos/sign.sh` | out-of-scope | Apple/iOS/macOS platform-support build tooling -- feature stack tracked elsewhere, per the project's Out-of-Scope section. |
| `cmake/vcpkg-triplets/arm64-ios.cmake` | out-of-scope | Apple/iOS/macOS platform-support build tooling -- feature stack tracked elsewhere, per the project's Out-of-Scope section. |
| `cmake/vcpkg-triplets/arm64-osx-static.cmake` | out-of-scope | Apple/iOS/macOS platform-support build tooling -- feature stack tracked elsewhere, per the project's Out-of-Scope section. |

</details>


<details>
<summary><strong>framework/ (engine runtime)</strong> (29 files)</summary>


| Path | Classification | Note |
|---|---|---|
| `framework/CMakeLists.txt` | in-flight-upstream (PR 1633) | Listed in upstream PR 1633's file set. |
| `framework/configfile.cpp` | out-of-scope | Harness-adjacent config plumbing (explicit RNG seed for deterministic harness runs) -- tracked with the harness feature stack (upstream PR 1633 theme). |
| `framework/configfile.h` | out-of-scope | Harness-adjacent config plumbing (explicit RNG seed for deterministic harness runs) -- tracked with the harness feature stack (upstream PR 1633 theme). |
| `framework/framework.cpp` | in-flight-upstream (PR 1635) | MULTI-OWNER, CONTENT-LEVEL DUPLICATE with upstream PR 1635 -- see section 7. The stageCommands iterator-copy fix in Framework::run() is functionally identical to 1635 (copy into a local vector before iterating, comment explains the same REPLACEALL/QUIT re-entrancy hazard). This hunk must not be reintroduced by any mini. The REST of this file's changes (harness query plumbing tied to upstream PR 1633; drawableSize/HiDPI display-init changes tied to platform-support work) are out-of-scope feature-stack content, not campaign material. |
| `framework/framework.h` | in-flight-upstream (PR 1633) | Listed in upstream PR 1633's file set (harness declarations). |
| `framework/harness.cpp` | in-flight-upstream (PR 1633) | Listed in upstream PR 1633's file set. |
| `framework/harness.h` | in-flight-upstream (PR 1633) | Listed in upstream PR 1633's file set. |
| `framework/image.cpp` | out-of-scope | Render/platform feature stack -- tracked elsewhere. |
| `framework/image.h` | out-of-scope | Render/platform feature stack -- tracked elsewhere. |
| `framework/logger.cpp` | out-of-scope | Harness-adjacent: disables the modal SDL error dialog under harness mode per upstream PR 1633's own description. |
| `framework/logger_file.cpp` | out-of-scope | Harness-adjacent: disables the modal SDL error dialog under harness mode per upstream PR 1633's own description. |
| `framework/logger_sdldialog.cpp` | out-of-scope | Harness-adjacent: disables the modal SDL error dialog under harness mode per upstream PR 1633's own description. |
| `framework/options.cpp` | out-of-scope | MULTI-OWNER: primarily the Framework.Harness.Enable option flag (upstream PR 1633 territory, not in its file list but same theme) plus later 'alien AI tunables' additions (commit 323161d8) that may be genuinely Wave-4-relevant config surface for OPE-19/20/21. Flagged for a closer look before any Wave 4 mini lands; default bucket out-of-scope pending that triage. |
| `framework/options.h` | out-of-scope | See framework/options.cpp. |
| `framework/os/app_paths.cpp` | out-of-scope | Platform (display-size/resizable-window, file-picker, app-paths) feature stack -- tracked elsewhere per Out-of-Scope. |
| `framework/os/app_paths.h` | out-of-scope | Platform (display-size/resizable-window, file-picker, app-paths) feature stack -- tracked elsewhere per Out-of-Scope. |
| `framework/os/display_size.cpp` | out-of-scope | Platform (display-size/resizable-window, file-picker, app-paths) feature stack -- tracked elsewhere per Out-of-Scope. |
| `framework/os/display_size.h` | out-of-scope | Platform (display-size/resizable-window, file-picker, app-paths) feature stack -- tracked elsewhere per Out-of-Scope. |
| `framework/os/file_picker.h` | out-of-scope | Platform (display-size/resizable-window, file-picker, app-paths) feature stack -- tracked elsewhere per Out-of-Scope. |
| `framework/os/file_picker_stub.cpp` | out-of-scope | Platform (display-size/resizable-window, file-picker, app-paths) feature stack -- tracked elsewhere per Out-of-Scope. |
| `framework/os/ios_file_picker.mm` | out-of-scope | Platform (display-size/resizable-window, file-picker, app-paths) feature stack -- tracked elsewhere per Out-of-Scope. |
| `framework/os/macos_file_picker.mm` | out-of-scope | Platform (display-size/resizable-window, file-picker, app-paths) feature stack -- tracked elsewhere per Out-of-Scope. |
| `framework/render/gl20/ogl_2_0_renderer.cpp` | in-flight-upstream (PR 1636) | MUST NOT BE TOUCHED BY ANY MINI -- upstream PR 1636 is the macOS black-window fix (GL_LUMINANCE vs GL_RED by context version). Confirmed by content: develop's own hunks in this file do NOT contain that fix at all -- they hardcode gl20::RED unconditionally as part of an unrelated shared-atlas-page batching/perf refactor (matches local branches khallmark/gl20-batching, khallmark/gl20-indexed-quads). Develop's version still has the underlying black-window bug. Treat the batching content as its own out-of-scope feature stack, tracked separately -- it is NOT a substitute for 1636 and must not be merged in a way that reintroduces the GL_RED regression 1636 fixes. |
| `framework/render/gles30_v2/gleswrap_gles3.cpp` | out-of-scope | GLES3 render backend feature stack -- tracked elsewhere. |
| `framework/render/gles30_v2/ogles_3_0_renderer_v2.cpp` | out-of-scope | GLES3 render backend feature stack -- tracked elsewhere. |
| `framework/renderer.cpp` | out-of-scope | Render feature stack -- tracked elsewhere. |
| `framework/renderer.h` | out-of-scope | Render feature stack -- tracked elsewhere. |
| `framework/sound/null_backend.cpp` | out-of-scope | Headless/CI sound-backend stub, build tooling. |
| `framework/stage.h` | in-flight-upstream (PR 1633) | Listed in upstream PR 1633's file set. |

</details>


<details>
<summary><strong>forms/ (UI framework)</strong> (13 files)</summary>


| Path | Classification | Note |
|---|---|---|
| `forms/CMakeLists.txt` | in-flight-upstream (PR 1633) | Listed in upstream PR 1633's file set. |
| `forms/control.cpp` | campaign-mini | Not in upstream PR 1633's file list (only control.h is); likely OPE-8 briefing-UI control plumbing -- needs triage to confirm. |
| `forms/control.h` | in-flight-upstream (PR 1633) | Listed in upstream PR 1633's file set. |
| `forms/form.cpp` | in-flight-upstream (PR 1633) | Listed in upstream PR 1633's file set (liveForms()/notifyVisibleForm()). |
| `forms/form.h` | in-flight-upstream (PR 1633) | Listed in upstream PR 1633's file set. |
| `forms/harness_actions.cpp` | in-flight-upstream (PR 1633) | Listed in upstream PR 1633's file set. |
| `forms/harness_actions.h` | in-flight-upstream (PR 1633) | Listed in upstream PR 1633's file set. |
| `forms/harness_ui.cpp` | in-flight-upstream (PR 1633) | Listed in upstream PR 1633's file set. |
| `forms/harness_ui.h` | in-flight-upstream (PR 1633) | Listed in upstream PR 1633's file set. |
| `forms/listbox.cpp` | in-flight-upstream (PR 1633) | Listed in upstream PR 1633's file set (selectItemByIndex()). |
| `forms/listbox.h` | in-flight-upstream (PR 1633) | Listed in upstream PR 1633's file set. |
| `forms/scrollbar.cpp` | campaign-mini | Needs triage -- not in upstream PR 1633's file list and not clearly one behaviour; bundled with the OPE-8 briefing-UI commit. |
| `forms/ticker.cpp` | campaign-mini | Needs triage -- same bundling as scrollbar.cpp. |

</details>


<details>
<summary><strong>data/ (gamestate patch data)</strong> (20 files)</summary>


| Path | Classification | Note |
|---|---|---|
| `data/common_patch/gamestate.xml` | campaign-mini | Umbrella patch include file, moves with whichever Wave 2/3 mini touches its listed rows. |
| `data/common_patch/gamestate/agent_equipment.xml` | campaign-mini | Recovered gamestate patch data (Wave 2/3), consumed together with its runtime slice per the project's vertical-slice rule. |
| `data/common_patch/gamestate/research.xml` | campaign-mini | Recovered gamestate patch data (Wave 2/3), consumed together with its runtime slice per the project's vertical-slice rule. |
| `data/common_patch/gamestate/ufo_incursions.xml` | obsolete | Same pattern as above, for OPE-14 (extract_ufo_incursions.cpp). |
| `data/common_patch/gamestate/ufo_mission_preference.xml` | obsolete | Same pattern as above, for OPE-18 (extract_ufo_mission_preference.cpp). |
| `data/common_patch/gamestate/vehicle_ammo.xml` | campaign-mini | Recovered gamestate patch data (Wave 2/3), consumed together with its runtime slice per the project's vertical-slice rule. |
| `data/common_patch/gamestate/vehicle_equipment.xml` | campaign-mini | Recovered gamestate patch data (Wave 2/3), consumed together with its runtime slice per the project's vertical-slice rule. |
| `data/common_patch/gamestate/vehicle_types.xml` | campaign-mini | Recovered gamestate patch data (Wave 2/3), consumed together with its runtime slice per the project's vertical-slice rule. |
| `data/difficulty0_patch/gamestate.xml` | campaign-mini | Difficulty-tier gamestate patch data (Wave 2/3), consumed together with its runtime slice. |
| `data/difficulty0_patch/gamestate/vehicle_equipment.xml` | obsolete | Same pattern as above, for OPE-16 (extract_vehicle_equipment.cpp), difficulty tier 0. |
| `data/difficulty1_patch/gamestate.xml` | campaign-mini | Difficulty-tier gamestate patch data (Wave 2/3), consumed together with its runtime slice. |
| `data/difficulty1_patch/gamestate/vehicle_equipment.xml` | obsolete | Same pattern as above, for OPE-16, difficulty tier 1. |
| `data/difficulty2_patch/gamestate.xml` | campaign-mini | Difficulty-tier gamestate patch data (Wave 2/3), consumed together with its runtime slice. |
| `data/difficulty2_patch/gamestate/vehicle_equipment.xml` | obsolete | Same pattern as above, for OPE-16, difficulty tier 2. |
| `data/difficulty3_patch/gamestate.xml` | campaign-mini | Difficulty-tier gamestate patch data (Wave 2/3), consumed together with its runtime slice. |
| `data/difficulty3_patch/gamestate/vehicle_equipment.xml` | obsolete | Same pattern as above, for OPE-16, difficulty tier 3. |
| `data/difficulty4_patch/gamestate.xml` | campaign-mini | Difficulty-tier gamestate patch data (Wave 2/3), consumed together with its runtime slice. |
| `data/difficulty4_patch/gamestate/vehicle_equipment.xml` | obsolete | Same pattern as above, for OPE-16, difficulty tier 4. |
| `data/forms/mainmenu.form` | in-flight-upstream (PR 1628) | Comes from develop's own 'feat(ui): add skirmish mode to the main menu' commit -- the same feature already tracked by open upstream PR 1628. Do not re-derive; out-of-scope for the bug-fix campaign per the project's Out-of-Scope section. |
| `data/readme.txt` | out-of-scope | Data-directory readme, meta only. |

</details>


<details>
<summary><strong>game/state/city/</strong> (14 files)</summary>


| Path | Classification | Note |
|---|---|---|
| `game/state/city/agentmission.cpp` | campaign-mini | No exact OPE-7..22 title match; needs triage (likely feeds OPE-8's briefing/assault flow, unconfirmed). |
| `game/state/city/base.cpp` | campaign-mini | PARTIALLY MERGED. The Base::die() iterator-invalidation copy-fix (agentsCopy/vehiclesCopy) is byte-for-byte the same mechanism as upstream PR 1627 (merged) -- that hunk is already satisfied on master; develop's copy differs only in comment wording. Remaining NEW content not on master: Base::alienExposureRollSucceeds() (base-exposure roll, ties to finding U1-U2-V1-incursion.md's 'U2' code), ufo2pSlot allocation (Wave 3 extractor plumbing), XComDefeated event wiring on total base loss (replaces a LogError stub -- a real player-facing bug: game just froze with no game-over screen), and canDestroyFacility() distinguishing NoFacility from OutOfBounds (fixes a log-spam issue during startingBase()). None of these map cleanly to an existing OPE-7..22 ticket -- flag as new-ticket candidates. See section 9. |
| `game/state/city/base.h` | campaign-mini | Header for base.cpp's new content (see base.cpp note); no PR overlap. |
| `game/state/city/building.cpp` | campaign-mini | OPE-8: alien-building data/assault objectives. |
| `game/state/city/building.h` | campaign-mini | OPE-8: alien-building data/assault objectives. |
| `game/state/city/city.cpp` | campaign-mini | General city-state surface, Wave 3 shared (OPE-11/13/14/16/18). |
| `game/state/city/city.h` | campaign-mini | General city-state surface, Wave 3 shared (OPE-11/13/14/16/18). |
| `game/state/city/research.cpp` | out-of-scope | Research-tree mechanics -- project's Out-of-Scope explicitly excludes 'research/manufacturing parity work that does not demonstrate a bug fix'. |
| `game/state/city/research.h` | out-of-scope | Research-tree mechanics -- project's Out-of-Scope explicitly excludes 'research/manufacturing parity work that does not demonstrate a bug fix'. |
| `game/state/city/scenery.cpp` | campaign-mini | Needs triage -- no exact OPE-7..22 title match; likely Wave 3 support content. |
| `game/state/city/vehicle.cpp` | campaign-mini | OPE-16 (vehicle-park costs) and OPE-10/12 (mission/geometry) surface. |
| `game/state/city/vehicle.h` | campaign-mini | OPE-16 (vehicle-park costs) and OPE-10/12 (mission/geometry) surface. |
| `game/state/city/vehiclemission.cpp` | campaign-mini | MULTI-OWNER, file-level collision with upstream PR 1634 -- see section 6 (deep dive). Content is disjoint: PR 1634 guards ONLY the PickNearbyPoint branch of VehicleTargetHelper::adjustTargetToClosestFlying (bounds check). Develop's copy contains that exact same bounds guard (duplicate -- do not re-add once 1634 merges) PLUS an adjacent, distinct footprint-fit check in the same function (belongs to OPE-12, not OPE-10) PLUS the OPE-10 ground-vehicle no-road-path give-up rewrite in VehicleMission::setPathTo (fully disjoint function) PLUS a large-unit occupancy footprint check in GroundVehicleTileHelper::canEnterTile (OPE-12) PLUS UFO mission-counter/withdrawal fields threaded through getNextDestination/update (OPE-11/14/18). OPE-10 remains independently valid and must be extracted, but its mini must exclude the PickNearbyPoint bounds-guard hunk (already covered by 1634) and must rebase around 1634's guard once merged. |
| `game/state/city/vehiclemission.h` | campaign-mini | Header for the above; no PR overlap (upstream PR 1634 touches only the .cpp). Declares fields for OPE-10/11/14/18. |

</details>


<details>
<summary><strong>game/state/battle/</strong> (21 files)</summary>


| Path | Classification | Note |
|---|---|---|
| `game/state/battle/ai/tacticalaivanilla.cpp` | campaign-mini | OPE-19/20: attack-priority and cover-seeking AI. |
| `game/state/battle/ai/tacticalaivanilla.h` | campaign-mini | OPE-19/20: attack-priority and cover-seeking AI. |
| `game/state/battle/ai/unitaibehavior.cpp` | campaign-mini | OPE-19/20/21: shared AI behaviour surface. |
| `game/state/battle/ai/unitaibehavior.h` | campaign-mini | OPE-19/20/21: shared AI behaviour surface. |
| `game/state/battle/ai/unitaihelper.cpp` | campaign-mini | MULTI-OWNER: OPE-19/20/21 AI helper content plus later 'alien AI tunables' additions (commit 323161d8) -- same file, no PR overlap, just two develop commits stacked. |
| `game/state/battle/ai/unitaihelper.h` | campaign-mini | OPE-19/20/21 AI helper surface. |
| `game/state/battle/ai/unitailowmorale.cpp` | campaign-mini | OPE-21: filename is a direct match ('Restore low-morale retreat and panic movement'). |
| `game/state/battle/ai/unitailowmorale.h` | campaign-mini | OPE-21: filename is a direct match ('Restore low-morale retreat and panic movement'). |
| `game/state/battle/ai/unitaivanilla.cpp` | campaign-mini | OPE-19/20/21: general vanilla AI behaviour. |
| `game/state/battle/ai/unitaivanilla.h` | campaign-mini | OPE-19/20/21: general vanilla AI behaviour. |
| `game/state/battle/battle.cpp` | campaign-mini | FILE-LEVEL COLLISION with upstream PR 1628 (skirmish) -- see section 10. Confirmed NO content overlap: develop's hunks here are all AI/hazard/geometry gameplay-fix content (OPE-12/15/17/19/20/21/22 territory: initBattle, initialMapPartRemoval/LinkUp, updatePathfinding, update, endTurn, finishBattle, exitBattle). The skirmish invisible-building fix that PR 1628's own description discusses for this file is NOT present anywhere in develop's history for battle.cpp. A rebase once 1628 merges is a mechanical file-level exercise, not a content conflict. |
| `game/state/battle/battle.h` | campaign-mini | Paired with battle.cpp above; same file-level-only collision with upstream PR 1628. |
| `game/state/battle/battlehazard.cpp` | campaign-mini | OPE-15/17/22: filename is a direct match for the hazard tickets. |
| `game/state/battle/battlehazard.h` | campaign-mini | OPE-15/17/22: filename is a direct match for the hazard tickets. |
| `game/state/battle/battleitem.cpp` | campaign-mini | OPE-15 (hazard item effects); flag for a TU-reservation-item carve-out per the Wave-4 exclusion note. |
| `game/state/battle/battleitem.h` | campaign-mini | OPE-15 (hazard item effects); flag for a TU-reservation-item carve-out per the Wave-4 exclusion note. |
| `game/state/battle/battlemappart.cpp` | campaign-mini | OPE-15: terrain-damage thresholds/resistance application. |
| `game/state/battle/battlemappart.h` | campaign-mini | OPE-15: terrain-damage thresholds/resistance application. |
| `game/state/battle/battleunit.cpp` | campaign-mini | MULTI-TICKET SHARED FILE: OPE-7 (shield buffer fields), OPE-9 (LOS), OPE-19/20/21 (AI-consumed unit state). Expect several minis to touch this file; sequence them and keep each hunk independently reviewable. |
| `game/state/battle/battleunit.h` | campaign-mini | MULTI-TICKET SHARED FILE: OPE-7 (shield buffer fields), OPE-9 (LOS), OPE-19/20/21 (AI-consumed unit state). Expect several minis to touch this file; sequence them and keep each hunk independently reviewable. |
| `game/state/battle/battleunitmission.cpp` | campaign-mini | OPE-9/12: unit movement/mission geometry. |

</details>


<details>
<summary><strong>game/state/rules/</strong> (11 files)</summary>


| Path | Classification | Note |
|---|---|---|
| `game/state/rules/aequipmenttype.cpp` | campaign-mini | OPE-7: disruptor-shield equipment rule definitions. |
| `game/state/rules/aequipmenttype.h` | campaign-mini | OPE-7: disruptor-shield equipment rule definitions. |
| `game/state/rules/battle/battlemap.cpp` | campaign-mini | MULTI-OWNER: OPE-12/14/16 spawn/footprint tables plus a later 'tileset bounds guard' (commit 323161d8, same defect class as upstream PR 1634/1627 -- worth checking whether it's actually OPE-12 content that arrived in a second commit). |
| `game/state/rules/battle/battlemap.h` | campaign-mini | MULTI-OWNER: OPE-12/14/16 spawn/footprint tables plus a later 'tileset bounds guard' (commit 323161d8, same defect class as upstream PR 1634/1627 -- worth checking whether it's actually OPE-12 content that arrived in a second commit). |
| `game/state/rules/city/ufogrowth.cpp` | campaign-mini | OPE-13: filename is a direct match. |
| `game/state/rules/city/ufogrowth.h` | campaign-mini | OPE-13: filename is a direct match. |
| `game/state/rules/city/ufoincursion.h` | campaign-mini | OPE-14: filename is a direct match. |
| `game/state/rules/city/ufopaedia.cpp` | out-of-scope | Ufopaedia unlock rules; not one of the OPE-7..22 restoration targets (feeds a UI browsing feature, not a gameplay bug). |
| `game/state/rules/city/ufopaedia.h` | out-of-scope | Ufopaedia unlock rules; not one of the OPE-7..22 restoration targets (feeds a UI browsing feature, not a gameplay bug). |
| `game/state/rules/city/vequipmenttype.cpp` | campaign-mini | OPE-16: vehicle-equipment costs. |
| `game/state/rules/city/vequipmenttype.h` | campaign-mini | OPE-16: vehicle-equipment costs. |

</details>


<details>
<summary><strong>game/state/ (shared, tilemap, root)</strong> (18 files)</summary>


| Path | Classification | Note |
|---|---|---|
| `game/state/CMakeLists.txt` | campaign-mini | Wave 3/4 build wiring for new source files. |
| `game/state/gameevent.cpp` | campaign-mini | Shared event-plumbing infra feeding OPE-8/13/14/18 UI/state updates. |
| `game/state/gameevent.h` | campaign-mini | Shared event-plumbing infra feeding OPE-8/13/14/18 UI/state updates. |
| `game/state/gameeventtypes.h` | campaign-mini | Shared event-plumbing infra feeding OPE-8/13/14/18 UI/state updates. |
| `game/state/gamestate.cpp` | campaign-mini | Wave 3 city-fix state fields (OPE-11/13/14/16/18). |
| `game/state/gamestate.h` | campaign-mini | MULTI-OWNER: touched by develop's own city-fixes commit (OPE-11/13/14/16/18 state fields) AND by develop's own main-menu-skirmish commit (adds a skirmishFromMainMenu-style flag) which is the SAME feature upstream PR 1628 implements. The skirmish flag addition here should be dropped/reconciled against 1628 rather than re-proposed; the city-fix state fields are legitimate Wave 3 campaign content. |
| `game/state/gamestate_serialize.xml` | campaign-mini | Serialization schema for the new Wave 3 state fields. |
| `game/state/gamestateintrospect.cpp` | campaign-mini | Introspection schema for the new Wave 3 state fields. |
| `game/state/gamestateintrospect.h` | campaign-mini | Introspection schema for the new Wave 3 state fields. |
| `game/state/gametime.h` | out-of-scope | Timing feature stack -- explicitly tracked elsewhere per Out-of-Scope. |
| `game/state/shared/aequipment.cpp` | campaign-mini | OPE-7: disruptor-shield equipment fields. |
| `game/state/shared/agent.cpp` | campaign-mini | OPE-7/8: agent-level shield/briefing state. |
| `game/state/shared/agent.h` | campaign-mini | OPE-7/8: agent-level shield/briefing state. |
| `game/state/shared/organisation.cpp` | campaign-mini | OPE-16: organisation vehicle-park finances -- direct match. |
| `game/state/shared/organisation.h` | campaign-mini | OPE-16: organisation vehicle-park finances -- direct match. |
| `game/state/tilemap/pathfinding.cpp` | campaign-mini | OPE-12: constrained/narrow-gap pathing. |
| `game/state/tilemap/tileobject.cpp` | campaign-mini | OPE-9/12: tile-object geometry. |
| `game/state/tilemap/tileobject_battleunit.cpp` | campaign-mini | OPE-9: multi-tile line-of-sight -- filename is a direct match. |

</details>


<details>
<summary><strong>game/ui/</strong> (35 files)</summary>


| Path | Classification | Note |
|---|---|---|
| `game/ui/base/basescreen.cpp` | campaign-mini | OPE-16: base/vehicle-equip and transfer screens (vehicle-park finances UI). |
| `game/ui/base/basestage.cpp` | campaign-mini | OPE-16: base/vehicle-equip and transfer screens (vehicle-park finances UI). |
| `game/ui/base/transferscreen.cpp` | campaign-mini | OPE-16: base/vehicle-equip and transfer screens (vehicle-park finances UI). |
| `game/ui/base/vequipscreen.cpp` | campaign-mini | OPE-16: base/vehicle-equip and transfer screens (vehicle-park finances UI). |
| `game/ui/base/vequipscreen.h` | campaign-mini | OPE-16: base/vehicle-equip and transfer screens (vehicle-park finances UI). |
| `game/ui/battle/battledebriefing.cpp` | in-flight-upstream (PR 1628) | Same skirmish-main-menu feature as upstream PR 1628 (debrief-returns-to-MainMenu logic). |
| `game/ui/battle/battleprestart.cpp` | campaign-mini | OPE-19/20/21: pre-battle AI-facing setup screen -- needs triage to confirm which ticket. |
| `game/ui/city/alertscreen.cpp` | campaign-mini | OPE-8: alien-building assault alert UI. |
| `game/ui/city/alertscreen.h` | campaign-mini | OPE-8: alien-building assault alert UI. |
| `game/ui/city/buildingscreen.cpp` | campaign-mini | OPE-8: alien-building assault briefing UI -- direct match. |
| `game/ui/city/buildingscreen.h` | campaign-mini | OPE-8: alien-building assault briefing UI -- direct match. |
| `game/ui/city/infiltrationscreen.cpp` | campaign-mini | OPE-8: alien-building infiltration/assault UI. |
| `game/ui/components/agentassignment.cpp` | campaign-mini | OPE-8/16 general agent-assignment UI support. |
| `game/ui/components/basegraphics.cpp` | campaign-mini | OPE-16 base-UI support. |
| `game/ui/components/controlgenerator.cpp` | campaign-mini | Shared UI-generation plumbing, Wave 2/3 support. |
| `game/ui/components/controlgenerator.h` | campaign-mini | Shared UI-generation plumbing, Wave 2/3 support. |
| `game/ui/components/equipscreen.cpp` | campaign-mini | OPE-7/16: equipment UI (shield display, vehicle-park equip). |
| `game/ui/general/aequipscreen.cpp` | campaign-mini | OPE-7: agent-equipment screen (shield display) -- direct match. |
| `game/ui/general/aequipscreen.h` | campaign-mini | OPE-7: agent-equipment screen (shield display) -- direct match. |
| `game/ui/general/cheatoptions.cpp` | out-of-scope | Developer cheat/debug tooling, not a player-facing bug fix. |
| `game/ui/general/mainmenu.cpp` | in-flight-upstream (PR 1628) | Same skirmish-main-menu feature as upstream PR 1628. |
| `game/ui/general/transactioncontrol.cpp` | campaign-mini | OPE-16: vehicle-park purchase transactions -- direct match. |
| `game/ui/general/transactioncontrol.h` | campaign-mini | OPE-16: vehicle-park purchase transactions -- direct match. |
| `game/ui/general/videoscreen.cpp` | out-of-scope | Cutscene/video UI, no owning OPE ticket. |
| `game/ui/general/videoscreen.h` | out-of-scope | Cutscene/video UI, no owning OPE ticket. |
| `game/ui/skirmish/skirmish.cpp` | in-flight-upstream (PR 1628) | Same skirmish-main-menu feature as upstream PR 1628. |
| `game/ui/skirmish/skirmish.h` | in-flight-upstream (PR 1628) | Same skirmish-main-menu feature as upstream PR 1628. |
| `game/ui/tileview/battleview.cpp` | campaign-mini | OPE-9/20: tile-view rendering for cover/LOS -- needs triage to confirm. |
| `game/ui/tileview/battleview.h` | campaign-mini | OPE-9/20: tile-view rendering for cover/LOS -- needs triage to confirm. |
| `game/ui/tileview/citytileview.cpp` | campaign-mini | OPE-14: incursion/spawn city-tile visualisation -- needs triage to confirm. |
| `game/ui/tileview/cityview.cpp` | campaign-mini | Wave 3 city UI support (OPE-8/14/18) -- needs triage to confirm; conceptually referenced (not touched) by upstream PR 1628's skirmish description. |
| `game/ui/tileview/cityview.h` | campaign-mini | Wave 3 city UI support (OPE-8/14/18) -- needs triage to confirm; conceptually referenced (not touched) by upstream PR 1628's skirmish description. |
| `game/ui/tileview/tileview.cpp` | campaign-mini | Shared tile-view plumbing, Wave 2/3 support. |
| `game/ui/tileview/tileview.h` | campaign-mini | Shared tile-view plumbing, Wave 2/3 support. |
| `game/ui/ufopaedia/ufopaediacategoryview.cpp` | out-of-scope | Ufopaedia browsing UI, no owning OPE ticket. |

</details>


<details>
<summary><strong>game/main.cpp, library/</strong> (2 files)</summary>


| Path | Classification | Note |
|---|---|---|
| `game/main.cpp` | out-of-scope | MULTI-OWNER: harness bootstrap (upstream PR 1633 theme) plus gameevent plumbing init. Primary bucket out-of-scope (harness); re-check for a residual event-plumbing hunk before Wave 3/4 minis land. |
| `library/CMakeLists.txt` | out-of-scope | Generic build-infra file, no behavioural content. |

</details>


<details>
<summary><strong>tests/</strong> (28 files)</summary>


| Path | Classification | Note |
|---|---|---|
| `tests/CMakeLists.txt` | campaign-mini | MULTI-OWNER: upstream PR 1633 adds a test_harness.cpp target here; OPE-6 needs the minimal shared CMake test-seam helper. Primary bucket is campaign-mini (OPE-6) -- when extracting, exclude the harness-target lines (they belong with 1633). |
| `tests/test_agent_mission.cpp` | campaign-mini | Needs triage -- no exact OPE-7..22 title match; likely OPE-8 support. |
| `tests/test_app_paths.cpp` | out-of-scope | Pairs with framework/os/app_paths -- platform feature stack. |
| `tests/test_base_die.cpp` | campaign-mini | Locks Base::die() iterator-invalidation behaviour -- upstream PR 1627 (merged) shipped the production fix WITHOUT a regression test. This file is a legitimate, small, standalone follow-up mini: add the missing regression lock for an already-merged fix. Use 'Related to' framing, not 'Fixes', since 1627 (not this test) closed the bug. |
| `tests/test_battle_disruptor_shield.cpp` | campaign-mini | OPE-7: confirmed by file header -- direct, exact match. |
| `tests/test_battle_hazard.cpp` | campaign-mini | OPE-15/17/22. |
| `tests/test_battle_large_unit.cpp` | campaign-mini | OPE-9/12: direct match. |
| `tests/test_battle_use_item.cpp` | campaign-mini | OPE-15; flag for TU-reservation-item carve-out. |
| `tests/test_city_rules.cpp` | campaign-mini | OPE-6 explicitly requires splitting this monolith into test_ufo_growth.cpp / test_ufo_missions.cpp / test_org_vehicle_park.cpp -- do not upstream as one file; it also carries OPE-11/13/14/16/18 coverage that must move with each owning mini. |
| `tests/test_damage_predicates.cpp` | campaign-mini | Shared damage-system test support, no single ticket. |
| `tests/test_diplomacy.cpp` | out-of-scope | No owning OPE ticket; matches parked research (finding O1-O2-M1-city.md) rather than a campaign behaviour. |
| `tests/test_display_size.cpp` | out-of-scope | Pairs with framework/os/display_size -- render/resize feature stack. |
| `tests/test_economy.cpp` | campaign-mini | OPE-16. |
| `tests/test_enum_traits.cpp` | out-of-scope | Generic language/meta-programming utility test, not a behaviour fix. |
| `tests/test_game_end.cpp` | campaign-mini | Needs triage -- plausibly the regression lock for base.cpp's new XComDefeated wiring (see base.cpp note); no OPE ticket currently owns this. |
| `tests/test_gametime.cpp` | out-of-scope | Timing feature stack. |
| `tests/test_ground_vehicle_path.cpp` | campaign-mini | OPE-10: confirmed direct match. |
| `tests/test_harness.cpp` | in-flight-upstream (PR 1633) | Listed in upstream PR 1633's file set. |
| `tests/test_helpers.h` | campaign-mini | MULTI-OWNER: shared by upstream PR 1633 (harness tests) and OPE-6 (shared test helper). Primary bucket campaign-mini (OPE-6); keep any harness-only helper additions out of the OPE-6 mini. |
| `tests/test_line.cpp` | campaign-mini | OPE-9: line-of-sight geometry primitive. |
| `tests/test_organisation.cpp` | campaign-mini | OPE-16. |
| `tests/test_psionics.cpp` | out-of-scope | Project's Wave-4 note: psionics items remain test-hardening only absent a demonstrated production behaviour delta. |
| `tests/test_research.cpp` | out-of-scope | Research parity -- explicitly excluded. |
| `tests/test_tactical_ai_retreat.cpp` | campaign-mini | OPE-21: direct match. |
| `tests/test_tu_reservation.cpp` | out-of-scope | Project's Wave-4 note: TU-reservation items remain test-hardening only absent a demonstrated production behaviour delta. |
| `tests/test_unit_ai_priority.cpp` | campaign-mini | OPE-19: direct match. |
| `tests/test_vec.cpp` | out-of-scope | Generic math-primitive test, not a behaviour fix. |
| `tests/test_vehicle_mission.cpp` | campaign-mini | OPE-11/14/18: mission-counter/withdrawal/incursion coverage, distinct from test_ground_vehicle_path.cpp's OPE-10 scope. |

</details>


<details>
<summary><strong>tools/</strong> (53 files)</summary>


| Path | Classification | Note |
|---|---|---|
| `tools/ai_plugins/__pycache__/example_doctrine.cpython-314.pyc` | generated-or-private | Tracked Python bytecode. Must never be committed; this is the artifact OPE-5 explicitly removes. |
| `tools/ai_plugins/example_doctrine.py` | out-of-scope | Adversarial/AI-plugin tooling -- explicitly excluded. |
| `tools/check_ignored_binaries.sh` | campaign-mini | OPE-5. New script; confirmed by content it fails the build if Steam/ISO binaries or Ghidra DBs would be committed. |
| `tools/extractors/CMakeLists.txt` | campaign-mini | Wave 3 build wiring. |
| `tools/extractors/common/aequipment.h` | campaign-mini | OPE-7/8 extractor table types (agent equipment, incl. disruptor shield). |
| `tools/extractors/common/agent.h` | campaign-mini | Shared agent extractor types, used by OPE-7/8. |
| `tools/extractors/common/building.h` | campaign-mini | OPE-8 extractor table types (alien-building data). |
| `tools/extractors/common/economy.h` | campaign-mini | OPE-16 extractor table types. |
| `tools/extractors/common/exe_slide.h` | campaign-mini | Shared extractor infra (binary offset sliding), consumed by all extractors. |
| `tools/extractors/common/organisations.h` | campaign-mini | OPE-16 extractor table types. |
| `tools/extractors/common/research.h` | out-of-scope | Research parity -- explicitly excluded. |
| `tools/extractors/common/tacp.cpp` | campaign-mini | Shared TACP string/table extractor infra -- supports OPE-8's recovered briefing text. |
| `tools/extractors/common/tacp.h` | campaign-mini | Shared TACP string/table extractor infra -- supports OPE-8's recovered briefing text. |
| `tools/extractors/common/ufo2p.cpp` | campaign-mini | Shared core extractor infra consumed by most Wave 3 tables. |
| `tools/extractors/common/ufo2p.h` | campaign-mini | Shared core extractor infra consumed by most Wave 3 tables. |
| `tools/extractors/common/ufogrowth.h` | campaign-mini | OPE-13 extractor table types. |
| `tools/extractors/common/ufoincursion.h` | campaign-mini | OPE-14 extractor table types. |
| `tools/extractors/common/ufomissionpattern.h` | campaign-mini | OPE-18 extractor table types. |
| `tools/extractors/common/ufopaedia.h` | out-of-scope | Ufopaedia -- not an OPE-7..22 target. |
| `tools/extractors/common/vequipment.h` | campaign-mini | OPE-16 extractor table types (vehicle equipment/pricing). |
| `tools/extractors/extract_agent_equipment.cpp` | campaign-mini | OPE-7/8. |
| `tools/extractors/extract_agent_types.cpp` | campaign-mini | OPE-8 support. |
| `tools/extractors/extract_battlescape_map.cpp` | campaign-mini | OPE-12/15/17/22 (battle map tables). |
| `tools/extractors/extract_buildings.cpp` | campaign-mini | OPE-8 (alien-building data). |
| `tools/extractors/extract_city_map.cpp` | campaign-mini | General city map extractor, Wave 3 shared support. |
| `tools/extractors/extract_economy.cpp` | campaign-mini | OPE-16. |
| `tools/extractors/extract_manufacturing.cpp` | out-of-scope | Manufacturing parity -- explicitly excluded. |
| `tools/extractors/extract_organisations.cpp` | campaign-mini | OPE-16. |
| `tools/extractors/extract_research.cpp` | out-of-scope | Research parity -- explicitly excluded. |
| `tools/extractors/extract_ufo_growth.cpp` | campaign-mini | OPE-13. |
| `tools/extractors/extract_ufo_incursions.cpp` | campaign-mini | OPE-14. |
| `tools/extractors/extract_ufo_mission_preference.cpp` | campaign-mini | OPE-18. |
| `tools/extractors/extract_vehicle_equipment.cpp` | campaign-mini | OPE-16. |
| `tools/extractors/extractors.cpp` | campaign-mini | Shared extractor driver infra, Wave 3. |
| `tools/extractors/extractors.h` | campaign-mini | Shared extractor driver infra, Wave 3. |
| `tools/extractors/main.cpp` | campaign-mini | Shared extractor driver infra, Wave 3. |
| `tools/launcher/launcherwindow.cpp` | out-of-scope | Launcher tool, platform feature. |
| `tools/oa_adversarial.py` | out-of-scope | Experimental adversarial co-evolution tooling -- explicitly excluded by the project. |
| `tools/oa_adversarial_arena.py` | out-of-scope | Experimental adversarial co-evolution tooling -- explicitly excluded by the project. |
| `tools/oa_ai.py` | out-of-scope | Python harness/automation driver -- tracked by the harness feature stack (matches local branch harness/python-driver); explicitly excluded as a broad harness stack. |
| `tools/oa_arena.py` | out-of-scope | Python harness/automation driver -- tracked by the harness feature stack (matches local branch harness/python-driver); explicitly excluded as a broad harness stack. |
| `tools/oa_campaign.py` | out-of-scope | Python harness/automation driver -- tracked by the harness feature stack (matches local branch harness/python-driver); explicitly excluded as a broad harness stack. |
| `tools/oa_capabilities.py` | out-of-scope | Python harness/automation driver -- tracked by the harness feature stack (matches local branch harness/python-driver); explicitly excluded as a broad harness stack. |
| `tools/oa_executor.py` | out-of-scope | Python harness/automation driver -- tracked by the harness feature stack (matches local branch harness/python-driver); explicitly excluded as a broad harness stack. |
| `tools/oa_forms.py` | out-of-scope | Python harness/automation driver -- tracked by the harness feature stack (matches local branch harness/python-driver); explicitly excluded as a broad harness stack. |
| `tools/oa_harness.py` | out-of-scope | Python harness/automation driver -- tracked by the harness feature stack (matches local branch harness/python-driver); explicitly excluded as a broad harness stack. |
| `tools/oa_play.py` | out-of-scope | Python harness/automation driver -- tracked by the harness feature stack (matches local branch harness/python-driver); explicitly excluded as a broad harness stack. |
| `tools/oa_skirmish.py` | out-of-scope | Python harness/automation driver -- tracked by the harness feature stack (matches local branch harness/python-driver); explicitly excluded as a broad harness stack. |
| `tools/oa_victory.py` | out-of-scope | Python harness/automation driver -- tracked by the harness feature stack (matches local branch harness/python-driver); explicitly excluded as a broad harness stack. |
| `tools/regen_compare_report.py` | campaign-mini | OPE-5. Regenerates docs/original-game/compare-report.html. |
| `tools/setup-worktree.sh` | out-of-scope | Fork-private dev-environment tooling. |
| `tools/test_oa_adversarial.py` | out-of-scope | Experimental adversarial co-evolution tooling -- explicitly excluded. |
| `tools/test_oa_ai.py` | out-of-scope | Harness driver test tooling. |

</details>


<details>
<summary><strong>docs/</strong> (62 files)</summary>


| Path | Classification | Note |
|---|---|---|
| `docs/README.md` | out-of-scope | Fork-process meta documentation (README/campaign-plan/local-development/memo-log/playing-the-game/xcom-ai-plugins), not slice-specific, not upstream-relevant. |
| `docs/campaign-plan.md` | out-of-scope | Fork-process meta documentation (README/campaign-plan/local-development/memo-log/playing-the-game/xcom-ai-plugins), not slice-specific, not upstream-relevant. |
| `docs/local-development.md` | out-of-scope | Fork-process meta documentation (README/campaign-plan/local-development/memo-log/playing-the-game/xcom-ai-plugins), not slice-specific, not upstream-relevant. |
| `docs/memo-log.md` | out-of-scope | Fork-process meta documentation (README/campaign-plan/local-development/memo-log/playing-the-game/xcom-ai-plugins), not slice-specific, not upstream-relevant. |
| `docs/original-game/README.md` | out-of-scope | Whole-of-project RE documentation (architecture/inventory/gap-matrix/parity-guide/binaries/address-maps) -- spans many tickets; the project explicitly forbids moving this as one giant evidence-doc PR. Slice-specific excerpts should move WITH each owning mini instead of the whole file moving here. |
| `docs/original-game/address-maps/README.md` | out-of-scope | Whole-of-project RE documentation (architecture/inventory/gap-matrix/parity-guide/binaries/address-maps) -- spans many tickets; the project explicitly forbids moving this as one giant evidence-doc PR. Slice-specific excerpts should move WITH each owning mini instead of the whole file moving here. |
| `docs/original-game/architecture.md` | out-of-scope | Whole-of-project RE documentation (architecture/inventory/gap-matrix/parity-guide/binaries/address-maps) -- spans many tickets; the project explicitly forbids moving this as one giant evidence-doc PR. Slice-specific excerpts should move WITH each owning mini instead of the whole file moving here. |
| `docs/original-game/binaries/setup.md` | out-of-scope | Whole-of-project RE documentation (architecture/inventory/gap-matrix/parity-guide/binaries/address-maps) -- spans many tickets; the project explicitly forbids moving this as one giant evidence-doc PR. Slice-specific excerpts should move WITH each owning mini instead of the whole file moving here. |
| `docs/original-game/binaries/skipped-middleware.md` | out-of-scope | Whole-of-project RE documentation (architecture/inventory/gap-matrix/parity-guide/binaries/address-maps) -- spans many tickets; the project explicitly forbids moving this as one giant evidence-doc PR. Slice-specific excerpts should move WITH each owning mini instead of the whole file moving here. |
| `docs/original-game/binaries/smkp.md` | out-of-scope | Whole-of-project RE documentation (architecture/inventory/gap-matrix/parity-guide/binaries/address-maps) -- spans many tickets; the project explicitly forbids moving this as one giant evidence-doc PR. Slice-specific excerpts should move WITH each owning mini instead of the whole file moving here. |
| `docs/original-game/binaries/tacp.md` | out-of-scope | Whole-of-project RE documentation (architecture/inventory/gap-matrix/parity-guide/binaries/address-maps) -- spans many tickets; the project explicitly forbids moving this as one giant evidence-doc PR. Slice-specific excerpts should move WITH each owning mini instead of the whole file moving here. |
| `docs/original-game/binaries/ufo2p.md` | out-of-scope | Whole-of-project RE documentation (architecture/inventory/gap-matrix/parity-guide/binaries/address-maps) -- spans many tickets; the project explicitly forbids moving this as one giant evidence-doc PR. Slice-specific excerpts should move WITH each owning mini instead of the whole file moving here. |
| `docs/original-game/binaries/xcomapoc.md` | out-of-scope | Whole-of-project RE documentation (architecture/inventory/gap-matrix/parity-guide/binaries/address-maps) -- spans many tickets; the project explicitly forbids moving this as one giant evidence-doc PR. Slice-specific excerpts should move WITH each owning mini instead of the whole file moving here. |
| `docs/original-game/compare-report.html` | campaign-mini | OPE-5. 901-line regenerated report (was stale/absent) -- exactly what OPE-5 restores. |
| `docs/original-game/conversation-synthesis.html` | out-of-scope | Whole-of-project RE documentation (architecture/inventory/gap-matrix/parity-guide/binaries/address-maps) -- spans many tickets; the project explicitly forbids moving this as one giant evidence-doc PR. Slice-specific excerpts should move WITH each owning mini instead of the whole file moving here. |
| `docs/original-game/exe-tables/aequip_alien_artifact.xml` | out-of-scope | No owning OPE ticket (alien-artifact equipment, not restored by any Wave 2 ticket). |
| `docs/original-game/exe-tables/cequip_score_req.xml` | out-of-scope | No owning OPE ticket. |
| `docs/original-game/exe-tables/ufo_growth_lists.xml` | campaign-mini | OPE-13: recovered growth-table evidence, consumed by extract_ufo_growth.cpp. |
| `docs/original-game/exe-tables/ufo_incursions.xml` | campaign-mini | OPE-14: recovered incursion-table evidence, consumed by extract_ufo_incursions.cpp. |
| `docs/original-game/exe-tables/ufo_mission_preference.xml` | campaign-mini | OPE-18: recovered mission-preference evidence, consumed by extract_ufo_mission_preference.cpp. |
| `docs/original-game/exe-tables/ufopaedia_start_visible.xml` | out-of-scope | Feeds the ufopaedia browsing feature, not an OPE-7..22 target. |
| `docs/original-game/exe-tables/vehicle_park_spawn.xml` | campaign-mini | OPE-16: recovered vehicle-park spawn/pricing evidence. |
| `docs/original-game/extractor-tables.md` | out-of-scope | Whole-of-project RE documentation (architecture/inventory/gap-matrix/parity-guide/binaries/address-maps) -- spans many tickets; the project explicitly forbids moving this as one giant evidence-doc PR. Slice-specific excerpts should move WITH each owning mini instead of the whole file moving here. |
| `docs/original-game/findings/A2-psi-panic-upkeep-divergence.md` | out-of-scope | Psionics research note; Wave-4 excludes psionics absent a demonstrated production delta. |
| `docs/original-game/findings/B1-cover-metric-pass2.md` | campaign-mini | OPE-20: cover-tile metric evidence (both B1 passes). |
| `docs/original-game/findings/B1-cover-metric.md` | campaign-mini | OPE-20: cover-tile metric evidence (both B1 passes). |
| `docs/original-game/findings/B3-G1-wounds-gadgets.md` | out-of-scope | Parked research (wound penalty / MultiTracker); no OPE ticket currently claims this content -- needs triage before a mini can cite it. |
| `docs/original-game/findings/B5-F1-K1-hazards.md` | campaign-mini | OPE-15/17/22: hazard evidence (enzyme overlay, fire remainder, cloak is an aside -- see K1 note). |
| `docs/original-game/findings/B5-enzyme-overlay-type.md` | campaign-mini | OPE-15: entropy-enzyme overlay evidence. |
| `docs/original-game/findings/B5-enzyme-pass3.md` | campaign-mini | OPE-15: entropy-enzyme overlay evidence. |
| `docs/original-game/findings/C1-C4-no-evidence-items.md` | out-of-scope | Documents an ABSENCE of evidence; project's Out-of-Scope excludes parity work that does not demonstrate a bug fix. |
| `docs/original-game/findings/C2-organic-factory-objectives.md` | campaign-mini | OPE-8: 'Alien-building destroy objectives' matches OPE-8's assault-briefing/objective-data scope. |
| `docs/original-game/findings/C2-secondary-objectives.md` | out-of-scope | Alien-dimension secondary objectives (Queen capture, Sectoid rescue, evacuation) -- not covered by any current OPE-7..22 ticket; needs a new ticket if pursued. |
| `docs/original-game/findings/F1-fire-remainder.md` | campaign-mini | OPE-22: 'the two unbound spread-primitive inputs' is exactly this ticket's own description of its scope. |
| `docs/original-game/findings/G1-multitracker-downstream.md` | out-of-scope | Parked research (MultiTracker is passive, not dead); no OPE ticket currently claims this content. |
| `docs/original-game/findings/K1-cloak.md` | out-of-scope | Personal Cloaking Field research; no owning OPE ticket. |
| `docs/original-game/findings/METHOD-tacp-string-regions.md` | campaign-mini | OPE-8: TACP string-extraction methodology directly supports the recovered briefing/objective text OPE-8 restores. |
| `docs/original-game/findings/METHOD-tacp-string-resolver.md` | campaign-mini | OPE-8: TACP string-extraction methodology directly supports the recovered briefing/objective text OPE-8 restores. |
| `docs/original-game/findings/O1-O2-M1-city.md` | out-of-scope | Org bribe/rift, cargo-seize diplomacy, city music -- none map to an OPE-7..22 ticket; needs triage/new ticket. |
| `docs/original-game/findings/O2-cargo-seize-pass2.md` | out-of-scope | Cargo-seize diplomacy -- not covered by any current OPE ticket. |
| `docs/original-game/findings/README.md` | out-of-scope | Findings-directory index, spans all topics -- not slice-specific. |
| `docs/original-game/findings/STATUS.md` | out-of-scope | Whole-of-project research status, spans all topics -- not slice-specific. |
| `docs/original-game/findings/U1-U2-V1-incursion.md` | campaign-mini | OPE-18: UFO mission-counter transition evidence (arrived-flag, ordertype, retarget-reconciliation). |
| `docs/original-game/findings/U1-arrived-flag-and-0x168.md` | campaign-mini | OPE-18: UFO mission-counter transition evidence (arrived-flag, ordertype, retarget-reconciliation). |
| `docs/original-game/findings/U1-ordertype-0x12c.md` | campaign-mini | OPE-18: UFO mission-counter transition evidence (arrived-flag, ordertype, retarget-reconciliation). |
| `docs/original-game/findings/U1-retarget-reconciliation.md` | campaign-mini | OPE-18: UFO mission-counter transition evidence (arrived-flag, ordertype, retarget-reconciliation). |
| `docs/original-game/findings/U1-scheduler-population.md` | campaign-mini | OPE-14: incursion/raid scheduled-population evidence. |
| `docs/original-game/findings/U1b-gate-consumer.md` | campaign-mini | OPE-18: mission-counter gate consumer. |
| `docs/original-game/inventory.md` | out-of-scope | Whole-of-project RE documentation (architecture/inventory/gap-matrix/parity-guide/binaries/address-maps) -- spans many tickets; the project explicitly forbids moving this as one giant evidence-doc PR. Slice-specific excerpts should move WITH each owning mini instead of the whole file moving here. |
| `docs/original-game/next-implementation.md` | out-of-scope | Whole-of-project RE documentation (architecture/inventory/gap-matrix/parity-guide/binaries/address-maps) -- spans many tickets; the project explicitly forbids moving this as one giant evidence-doc PR. Slice-specific excerpts should move WITH each owning mini instead of the whole file moving here. |
| `docs/original-game/openapoc-gap-matrix.md` | out-of-scope | Whole-of-project RE documentation (architecture/inventory/gap-matrix/parity-guide/binaries/address-maps) -- spans many tickets; the project explicitly forbids moving this as one giant evidence-doc PR. Slice-specific excerpts should move WITH each owning mini instead of the whole file moving here. |
| `docs/original-game/parity-guide.md` | out-of-scope | Whole-of-project RE documentation (architecture/inventory/gap-matrix/parity-guide/binaries/address-maps) -- spans many tickets; the project explicitly forbids moving this as one giant evidence-doc PR. Slice-specific excerpts should move WITH each owning mini instead of the whole file moving here. |
| `docs/original-game/subsystems/battle-ai.md` | out-of-scope | Whole-of-project RE documentation (architecture/inventory/gap-matrix/parity-guide/binaries/address-maps) -- spans many tickets; the project explicitly forbids moving this as one giant evidence-doc PR. Slice-specific excerpts should move WITH each owning mini instead of the whole file moving here. |
| `docs/original-game/subsystems/battle-hazards.md` | out-of-scope | Whole-of-project RE documentation (architecture/inventory/gap-matrix/parity-guide/binaries/address-maps) -- spans many tickets; the project explicitly forbids moving this as one giant evidence-doc PR. Slice-specific excerpts should move WITH each owning mini instead of the whole file moving here. |
| `docs/original-game/subsystems/city-alien-dimension.md` | out-of-scope | Whole-of-project RE documentation (architecture/inventory/gap-matrix/parity-guide/binaries/address-maps) -- spans many tickets; the project explicitly forbids moving this as one giant evidence-doc PR. Slice-specific excerpts should move WITH each owning mini instead of the whole file moving here. |
| `docs/original-game/subsystems/city-economy.md` | out-of-scope | Whole-of-project RE documentation (architecture/inventory/gap-matrix/parity-guide/binaries/address-maps) -- spans many tickets; the project explicitly forbids moving this as one giant evidence-doc PR. Slice-specific excerpts should move WITH each owning mini instead of the whole file moving here. |
| `docs/original-game/subsystems/city-vehicles.md` | out-of-scope | Whole-of-project RE documentation (architecture/inventory/gap-matrix/parity-guide/binaries/address-maps) -- spans many tickets; the project explicitly forbids moving this as one giant evidence-doc PR. Slice-specific excerpts should move WITH each owning mini instead of the whole file moving here. |
| `docs/original-game/subsystems/timing-tps.md` | out-of-scope | Timing feature stack documentation -- explicitly excluded. |
| `docs/playing-the-game.md` | out-of-scope | Fork-process meta documentation (README/campaign-plan/local-development/memo-log/playing-the-game/xcom-ai-plugins), not slice-specific, not upstream-relevant. |
| `docs/solutions/2026-08-23-ghidra-allowed-on-this-fork.md` | out-of-scope | Fork policy note, not slice-specific. |
| `docs/solutions/2026-08-23-tps-is-a-4x-scalar.md` | out-of-scope | Timing feature stack documentation -- explicitly excluded. |
| `docs/xcom-ai-plugins.md` | out-of-scope | Fork-process meta documentation (README/campaign-plan/local-development/memo-log/playing-the-game/xcom-ai-plugins), not slice-specific, not upstream-relevant. |

</details>
## 6. Deep dive: `game/state/city/vehiclemission.cpp` vs upstream PR 1634 (OPE-10 collision)

This is the collision named explicitly in the ticket brief, and the one place the acceptance criteria ask for a determination, not just a flag. Verified by reading both diffs side by side (`git diff OpenApoc/master origin/develop -- game/state/city/vehiclemission.cpp`, `gh pr diff 1634`), not by filename alone.

**PR 1634's entire change** is inside `VehicleTargetHelper::adjustTargetToClosestFlying`, specifically its `PickNearbyPoint` branch:

```cpp
if (!map.tileIsValid(x, y, z))
{
    continue;
}
auto t = map.getTile(x, y, z);
```

**`develop`'s change to the same function** contains that exact guard, verbatim in effect, plus an adjacent and distinct addition in the same hunk:

```cpp
if (!map.tileIsValid(x, y, z) || !footprintFitsOnMap({x, y, z}))
{
    continue;
}
```

The `tileIsValid` half is the duplicate of PR 1634. The `footprintFitsOnMap` half is new: it rejects candidate tiles where a multi-tile craft's footprint would hang off the map edge — a large-unit geometry concern, not a flying-bounds concern. That maps to **OPE-12** ("large-unit occupancy and constrained pathing"), not OPE-10.

**OPE-10's actual content** ("Terminate unreachable ground-vehicle orders safely") lives entirely in a different function, `VehicleMission::setPathTo`, and is not touched by PR 1634 at all:

```cpp
const bool stuckInPlace = path.empty() && maxIterations > (int)distance;
if (giveUpIfInvalid)
{
    cancelled = true;
    if (stuckInPlace)
    {
        v.setCrashed(state);
    }
    return;
}
```

This rewrites the give-up logic so only a vehicle with *no* path at all and a destination close enough that one should exist gets crashed; a vehicle with a partial path (e.g. a severed road network) is left to drive or retry, never destroyed. Confirmed disjoint from PR 1634's hunk by line range, function, and behavior.

**Determination:** OPE-10 is not a duplicate and remains independently valid — the ticket should stay open and proceed. Its mini must:
- Touch only `VehicleMission::setPathTo`'s give-up logic (and `tests/test_ground_vehicle_path.cpp`), not `adjustTargetToClosestFlying`.
- Explicitly exclude the `tileIsValid` bounds-guard hunk in `adjustTargetToClosestFlying` (that's PR 1634's, and duplicating it would be exactly the collision the campaign exists to prevent).
- Rebase mechanically around PR 1634's guard once it merges, since both land in the same file.
- Leave the `footprintFitsOnMap` addition and the `GroundVehicleTileHelper::canEnterTile` footprint/occupancy block (further down the same file) for **OPE-12**, not fold them into OPE-10.

`game/state/city/vehiclemission.cpp` additionally carries UFO mission-counter/withdrawal state threaded through `getNextDestination`/`update` (OPE-11/14/18 territory) from develop's "city:" commit — a fourth ticket surface in the same file. Sequencing note: OPE-10, OPE-11, OPE-12, OPE-14, and OPE-18 minis all eventually touch this one file; land them one at a time and rebase, don't stack them blind.

## 7. Deep dive: `framework/framework.cpp` vs upstream PR 1635

Confirmed by content, not just file name. PR 1635's fix in `Framework::run()`:

```cpp
const auto commandsThisFrame = stageCommands;
for (const StageCmd &cmd : commandsThisFrame)
```

`develop`'s `framework.cpp` has the identical fix, same location, same rationale (a `REPLACEALL`/`QUIT` teardown that queues another stage command mid-iteration would otherwise invalidate the range-for), same mechanism (copy before iterating). This is a genuine content-level duplicate, not just a file-level touch.

No OPE ticket owns `framework.cpp` — it isn't part of the OPE-7..22 restoration set. Its only relevance to the campaign is negative: **no mini may reintroduce this hunk.** The rest of the file's changes in `develop` (harness query plumbing tied to PR 1633; `drawableSize`/HiDPI display-init changes tied to the platform-support feature stack) are out-of-scope feature content, not campaign material, and are classified accordingly in the table.

## 8. Deep dive: `framework/render/gl20/ogl_2_0_renderer.cpp` vs upstream PR 1636 — must not be touched

PR 1636 fixes the macOS black-window bug: palette-index textures upload as `GL_RED`, which is only legal on a GL 3.0+ context; the GL2 backend on macOS reports 2.1, so the upload silently fails and every paletted sprite samples the zero texture. The fix resolves the external format once per context version (`GL_LUMINANCE` below 3.0).

**`develop`'s own changes to this file do not contain that fix at all.** Grepping the diff for `RED`/`LUMINANCE` shows `develop` still hardcodes `gl20::RED` unconditionally, as part of an unrelated shared-atlas-page batching/perf refactor (matches the local branches `khallmark/gl20-batching` and `khallmark/gl20-indexed-quads` seen in this checkout's branch list). In other words: **`develop`'s version of this file still has the black-window bug.** The two changes are not alternatives to each other; they're orthogonal, and `develop`'s batching work would need PR 1636's fix rebased into it regardless of what this campaign does.

**Rule for the campaign:** no OPE ticket may touch this file. PR 1636 owns the correctness fix outright. The batching/perf content is its own separate, already-out-of-scope feature stack (tracked outside this bug-fix campaign per the project's render-stack exclusion) and must not be merged in a way that silently reintroduces the `GL_RED` regression PR 1636 exists to fix.

## 9. Deep dive: `game/state/city/base.cpp` vs upstream PR 1627 (merged)

See section 2 for why this needed a tip-to-tip diff rather than the triple-dot range to read correctly. Once compared against current `master` (which already contains PR 1627), the picture is:

- **Already satisfied on `master`:** the `agentsCopy`/`vehiclesCopy` iterator-invalidation fix in `Base::die()`. `develop`'s copy differs only in comment wording, not mechanism. This hunk needs no further action.
- **Genuinely new, not on `master`, and not owned by any current OPE ticket:**
  - `Base::alienExposureRollSucceeds(int, int)` — a new pure function for a base-exposure roll (ties to the `U2` code in `docs/original-game/findings/U1-U2-V1-incursion.md`, described there as "base-exposure leftovers").
  - `ufo2pSlot = state.allocateUfo2pBaseSlot();` in the constructor — Wave 3 extractor-slot plumbing.
  - Real player-facing bug fix: total base loss previously did `LogError("Player lost, but we have no screen for that yet!")` and just returned — the game froze with no game-over screen. `develop` wires a proper `GameEventType::XComDefeated` event instead.
  - `canDestroyFacility()` now distinguishes `NoFacility` from `OutOfBounds`, which stops `startingBase()` from logging thousands of spurious warnings per campaign start.

**Recommendation:** these four items are legitimate, small, independent bug-fix candidates, but none of them is OPE-7..22's territory. Flag for OPE-23/OPE-24 triage as new-ticket candidates rather than silently folding them into an unrelated existing ticket. `tests/test_base_die.cpp` and `tests/test_game_end.cpp` (both new in `develop`) look like the natural regression locks for the iterator fix and the `XComDefeated` wiring respectively — `test_base_die.cpp` in particular is worth landing on its own even though PR 1627 is already merged, since PR 1627 shipped without a regression test.

## 10. Deep dive: skirmish files (`battle.cpp`/`battle.h`/`gamestate.h`/UI) vs upstream PR 1628

PR 1628 lists 12 files; 8 of them intersect `develop`'s outgoing range. Reading the actual diff content (not just the file-name overlap) splits them into two very different situations:

**Same feature, in-flight — no independent content (`in-flight-upstream`):** `data/forms/mainmenu.form`, `game/ui/battle/battledebriefing.cpp`, `game/ui/general/mainmenu.cpp`, `game/ui/skirmish/skirmish.cpp`, `game/ui/skirmish/skirmish.h`. These come from `develop`'s own "feat(ui): add skirmish mode to the main menu" commit, which is the same feature PR 1628 already implements upstream (main-menu Skirmish button, debrief-returns-to-MainMenu). No independent bug-fix content here; this is the feature stack itself, already tracked.

**File-level collision only, no content overlap:** `game/state/battle/battle.cpp`, `game/state/battle/battle.h`. PR 1628's own description discusses touching `battle.cpp`/`battle.h` for a "keep the synthetic skirmish building out of the cityscape" fix (`Battle::skirmish`, `SKIRMISH_BUILDING_ID`). Grepping `develop`'s diff of these two files for `skirmish`/`SKIRMISH` returns nothing — `develop`'s hunks here are exclusively AI/hazard/geometry gameplay-fix content (`initBattle`, `initialMapPartRemoval`/`LinkUp`, `updatePathfinding`, `update`, `endTurn`, `finishBattle`, `exitBattle` — OPE-12/15/17/19/20/21/22 territory). **Determination: no content conflict.** Once PR 1628 merges, minis touching `battle.cpp`/`battle.h` need a routine textual rebase, not a behavioral reconciliation.

**Mixed, one field needs reconciling:** `game/state/gamestate.h` carries both `develop`'s own city-fix state fields (OPE-11/13/14/16/18 — legitimate campaign content) and a skirmish-mode flag from the same main-menu-skirmish commit that duplicates PR 1628's territory. The flag addition should be dropped from any mini touching this header rather than re-proposed.

## 11. Per-OPE-ticket collision check (OPE-7 through OPE-22)

Every OPE-7..22 ticket's expected file surface was checked against all six named PRs' file lists. Only OPE-10 collides; every other ticket is clear. Recorded explicitly, per the acceptance criteria ("no mini duplicates an open or merged upstream PR"):

| Ticket | Expected surface (from develop's commit grouping + ticket text) | Checked against | Result |
|---|---|---|---|
| OPE-7 (disruptor shield) | `battleunit.cpp/h`, `aequipmenttype.cpp/h`, `aequipment.cpp`, `test_battle_disruptor_shield.cpp` | All 6 PR file lists | No collision. |
| OPE-8 (assault briefing data) | `building.cpp/h`, `alertscreen.*`, `buildingscreen.*`, `infiltrationscreen.cpp`, extractor tables | All 6 PR file lists | No collision. |
| OPE-9 (large-unit LOS) | `tileobject_battleunit.cpp`, `battleunitmission.cpp`, `test_battle_large_unit.cpp`, `test_line.cpp` | All 6 PR file lists | No collision. |
| OPE-10 (ground-vehicle no-path termination) | `vehiclemission.cpp` (`setPathTo`), `test_ground_vehicle_path.cpp` | All 6 PR file lists | **Collides at file level with PR 1634 in `vehiclemission.cpp`; no content overlap once the specific hunks are compared — see section 6.** Proceed, excluding the duplicate hunk. |
| OPE-11 (damaged-UFO withdrawal) | `vehiclemission.cpp/h`, `city.cpp/h`, `gamestate.cpp/h` | All 6 PR file lists | Shares a file (`vehiclemission.cpp`) with PR 1634 and OPE-10, but its own content (withdrawal-threshold fields in `getNextDestination`/`update`) is in neither PR 1634's hunk nor OPE-10's `setPathTo` hunk. No content collision; sequence after OPE-10 when both land. |
| OPE-12 (large-unit occupancy/pathing) | `vehiclemission.cpp` (`footprintFitsOnMap`, `GroundVehicleTileHelper::canEnterTile`), `pathfinding.cpp`, `tileobject.cpp`, `battlemap.cpp/h`, `test_battle_large_unit.cpp` | All 6 PR file lists | Shares `vehiclemission.cpp` with PR 1634 (adjacent hunk, not the same lines — see section 6) and with OPE-10/11 (same file, disjoint functions). No content collision with any PR; sequence carefully among OPE-10/11/12/14/18 on this one file. |
| OPE-13 (UFO growth/organic factory) | `ufogrowth.cpp/h`, extract_ufo_growth.cpp, exe-tables | All 6 PR file lists | No collision. |
| OPE-14 (UFO incursion selection/spawn) | `ufoincursion.h`, extract_ufo_incursions.cpp, `vehiclemission.cpp` (shared), `citytileview.cpp` | All 6 PR file lists | Shares `vehiclemission.cpp` per above; no content collision. |
| OPE-15 (hazard overlay/terrain) | `battlehazard.cpp/h`, `battleitem.cpp/h`, `battlemappart.cpp/h`, `test_battle_hazard.cpp` | All 6 PR file lists | No collision. |
| OPE-16 (vehicle-park finances) | `organisation.cpp/h`, `vequipmenttype.cpp/h`, base/vehicle-equip UI, extract_economy/organisations/vehicle_equipment.cpp | All 6 PR file lists | No collision. |
| OPE-17 (hazard scheduler state) | `battlehazard.cpp/h`, `battle.cpp` (update/endTurn) | All 6 PR file lists | Shares `battle.cpp`/`battle.h` with PR 1628 at file level only (see section 10); no content overlap. |
| OPE-18 (UFO mission-counter departure) | `ufomissionpattern.h`, extract_ufo_mission_preference.cpp, `vehiclemission.cpp` (shared) | All 6 PR file lists | Shares `vehiclemission.cpp` per above; no content collision. |
| OPE-19 (attack priority/AOE) | `tacticalaivanilla.cpp/h`, `unitaivanilla.cpp/h`, `test_unit_ai_priority.cpp` | All 6 PR file lists | No collision. |
| OPE-20 (cover/exposure scoring) | `tacticalaivanilla.cpp/h`, `unitaibehavior.cpp/h`, finding `B1-cover-metric*` | All 6 PR file lists | No collision. |
| OPE-21 (low-morale retreat/panic) | `unitailowmorale.cpp/h`, `test_tactical_ai_retreat.cpp` | All 6 PR file lists | No collision. |
| OPE-22 (hazard spread primitives) | `battlehazard.cpp/h`, finding `F1-fire-remainder.md` | All 6 PR file lists | No collision. |

Wave 0 tickets (OPE-2 through OPE-5) were also checked: none of their scope (CI action modernization, Objective-C++ clang-format, clang-tidy honesty) appears anywhere in `develop`'s outgoing range at all — `.github/workflows/cmake.yml`, `lint.yml`, and `.clang-format` are untouched by `develop`. That work is net-new against `master`, not an extraction from `develop`, so there's nothing to collide with. OPE-5 is the exception: its hygiene scope (`.gitignore`, `check_ignored_binaries.sh`, `harness.yml`, `PULL_REQUEST_TEMPLATE.md`, `compare-report.html`) *is* present in `develop` and is classified `campaign-mini` throughout the table above. OPE-6's shared test-seam scope is present too, and collides at the file level (not content level) with PR 1633 in `tests/CMakeLists.txt` and `tests/test_helpers.h` — noted in the table.

## 12. `Fixes` vs `Related to`/`Refs` — required semantics per reference

Per the project's non-negotiable rule 5 and OPE-1's acceptance criteria ("reporter-save-dependent fixes retain `Refs` semantics until reproduced"):

| Reference | Required verb | Why |
|---|---|---|
| upstream issue 1617 (PR 1634's target) | `Refs` | PR 1634's own body states the reporter's save was not replayed; treat as "fixes the reported error," not confirmed-closing. |
| upstream issue 785 | `Related to` | OPE-10 explicitly "does not implement two-way lanes, overtaking, or drive-side rules" — partial coverage only. |
| upstream issue 264 | `Related to` | Broad 1.0 roadmap umbrella; project's Out-of-Scope explicitly forbids treating it as solved by partial parity work. |
| upstream issue 265 | `Related to` | Broad beta-roadmap umbrella; same rule. |
| upstream issue 607 (closed) | `Related to` | OPE-7's disruptor-shield mini is related to the closed report, not a fresh `Fixes` of a currently-open issue. |
| upstream issue 251 (closed) | `Related to` | OPE-8's briefing-data mini is related to the closed report. |
| upstream issue 1053 | `Related to` | OPE-16 explicitly "does not complete the full organization-finance system." |
| upstream issue 997 | `Related to` | Listed alongside 264/265/785/1053 in the project's explicit "do not treat as solved by partial work" list. |
| upstream issue 1631 (CI modernization) | `Related to` | Wave 0's own text: "keep #1631 open until its larger modernization scope is actually complete" — use plain text `Related to upstream issue 1631` in commit/PR bodies, not a cross-repo `#1631`. |
| upstream PR 1627 (base.cpp regression test, `test_base_die.cpp`) | `Related to` | The test is a follow-up lock for an already-merged fix, not itself the fix; don't claim `Fixes` for a PR that already merged the behavior change. |

General rule embedded above: `Fixes` is reserved for a mini that reproduces the *exact* reported failure and demonstrably closes the *whole* linked issue. Every reference in this table currently fails that bar (unreproduced, partial, or already-closed-elsewhere) and must use `Related to`/`Refs` until proven otherwise.

## 13. Open items for OPE-23 / OPE-24

- **New-ticket candidates surfaced here, not currently owned by any OPE ticket:** `Base::alienExposureRollSucceeds`/`XComDefeated` wiring/`canDestroyFacility` NoFacility distinction (base.cpp, section 9); `docs/original-game/findings/O1-O2-M1-city.md` and `O2-cargo-seize-pass2.md` (org bribe/rift, cargo-seize diplomacy); `docs/original-game/findings/C2-secondary-objectives.md` (alien-dimension secondary objectives); `docs/original-game/findings/B3-G1-wounds-gadgets.md` and `G1-multitracker-downstream.md` (parked research, no demonstrated bug yet).
- **"Needs triage" rows** (marked individually in the table above, ~15 files): mostly UI-support files bundled into develop's large squashed commits (`forms/control.cpp`, `forms/scrollbar.cpp`, `forms/ticker.cpp`, `game/ui/tileview/*`, `tests/test_agent_mission.cpp`, `tests/test_game_end.cpp`, `game/state/city/agentmission.cpp`, `game/state/city/scenery.cpp`, `framework/options.cpp/h`, `game/main.cpp`, `game/state/rules/battle/battlemap.cpp/h`'s tileset-bounds-guard hunk). None block starting Wave 2/3 work; they should be resolved when the specific mini that would touch them is drafted.
- **`CODE_STYLE.md`** (section 2): if any mini touches this file, diff against current `master` tip, not `develop` — `develop`'s copy predates master's independent PR-1630 rewrite and would regress it.
- Re-run this classification after any of PRs 1628/1633/1634/1635/1636 changes state, per OPE-1's acceptance criteria, and after re-diffing tip-to-tip (section 2) rather than trusting a cached file count.
