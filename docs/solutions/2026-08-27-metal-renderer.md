# The Metal renderer, and how to tell whether it is at fault

`framework/render/metal/metal_renderer.mm` is a Metal backend ported from the GLES 3.0 v2
renderer. It is first in `RENDERERS` on macOS and iOS, with GL behind it. It fixes the all-black
macOS window: `GLES_3_0` cannot even initialise under `sdl2-compat` on Apple silicon ("Failed to
find ES3-compatible device"), leaving only `GL_2_0` and Metal.

Most of this document is about *evidence*, because the useful question is rarely "how does it
work" but "is the renderer why this looks wrong". Usually it is not.

## Run the validation layer. It is not optional.

**Do this first, before any pixel comparison:**

```sh
# macOS -- both layers
MTL_DEBUG_LAYER=1 MTL_DEBUG_LAYER_ERROR_MODE=nslog MTL_SHADER_VALIDATION=1 \
  ./build/bin/OpenApoc.app/Contents/MacOS/OpenApoc ... 2>&1 | tee /tmp/val.log

# iOS simulator -- API layer ONLY, prefixed for simctl. Do not add MTL_SHADER_VALIDATION here.
SIMCTL_CHILD_MTL_DEBUG_LAYER=1 SIMCTL_CHILD_MTL_DEBUG_LAYER_ERROR_MODE=nslog \
  xcrun simctl launch --console-pty <udid> org.openapoc.OpenApoc ...
```

`MTL_SHADER_VALIDATION=1` on the simulator interrupts the shader-compiler XPC service, so every
pipeline fails to build, the renderer declines, and **GL runs instead** -- meaning the Metal path
is never validated at all while appearing to be. Dropping that one variable makes it work: three
consecutive runs built every pipeline, and the Metal path on iOS is clean (0 reports, 0 AGX,
GPU 0.16 ms/cmdbuf, resize included). If a tool makes the target fail, isolate which part of the
tool did it before concluding the target is unreachable.

It found **three** real defects that thirty-five rounds of pixel diffing, same-renderer controls,
a 57000-frame soak, GPU timing and a whole-campaign robot run had all missed:

| defect | why every pixel test passed anyway |
| --- | --- |
| zero-area `replaceRegion` for a space glyph (`{0,15}`) -- `AGX: Texture read/write assertion failed: width > 0` | a space renders as nothing whether uploaded or not, so the output was identical. GL's `TexSubImage3D` silently no-ops on the same input |
| `present()` blitting into the drawable -- `destinationTexture must not be a framebufferOnly texture` | the driver tolerates it with validation off; the frame was correct every time |
| a nil `MTLRenderPipelineState` used after `makePipeline()` failed -- `renderPipelineState must not be nil`, then SIGSEGV | needs a pipeline-creation failure to trigger, which never happens in a normal run |

Every one produced *correct pixels while violating the API contract*. Pixel comparison is
structurally blind to this class of bug; only the validation layer sees it.

Both platforms are clean under API validation after the fixes -- macOS across windowed, scaled,
autoscaled-Retina and battle scenarios each with a resize, and iOS on the city view with a
resize. The campaign robot was also run *under* validation (the environment variables propagate
into its child process; check for the "Metal API Validation Enabled" banner in `game.log` to be
sure): 42 minutes, one tactical mission, 0 reports, 0 AGX, 0 crashes -- covering research, base
management, alert and score screens that no hand-driven test here reached. Budget for it: under
validation the robot managed 1 mission in 42 minutes against 3 in 9 unvalidated, roughly a 4x
slowdown.

Two collection notes that hid these for a long time. `AGX:` messages come from the **GPU driver
on stderr**, not through the engine's logger -- a run that redirects stdout to `/dev/null` and
greps `log.txt` cannot see them, which is exactly what every earlier run here did. And under
shader validation the compiler XPC service can be interrupted, so all three pipelines fail; that
is what exposed the nil-pipeline crash, and the fix (decline in the factory so GL is tried) turns
it into the fallback that already existed.

## Prove it before blaming it

Launch the same save twice with `--Framework.Harness.Enable=1`, once with
`--Framework.Renderers=GL_2_0` (macOS) or `=GLES_3_0` (iOS), take a `screenshot` from each, and
diff. **Always run a same-renderer control as well** -- two runs of Metal against each other --
because several screens in this game are not deterministic between launches.

| scene | Metal vs GL | same-renderer control |
| --- | --- | --- |
| macOS city view, default | **0 / 921600** | -- |
| macOS battlescape (saved in `BattleView`) | **0 / 921600** | **0.000%** |
| macOS live battle, 8 hostiles, 25 units | 995 px (0.108%) | **995 px — identical** |
| iOS city view | 0.93% | **0.000%** |
| iOS battle debriefing | 3.42% | **0.000%** |
| iOS, either screen, at `UiScale=1` | **0 / 1420032** | -- |
| agent equip screen | 1.004% | 1.088% (*larger* -- agents are randomised) |

Two rows only make sense with their control beside them. The equip screen looks like a 1%
renderer defect until you see the control is *larger*. The live-battle row is better still: Metal
vs GL and Metal vs Metal differ by **exactly the same 995 pixels**, so the renderer contributes
nothing at all and the residue is the scene animating between captures.

Getting a live battle to reload is fiddly and I got it wrong twice. The sequence that works:
check `CUSTOMISE_FORCES` on the Skirmish screen, then on SelectForces **explicitly set
`DEFAULT_ALIENS` to 0** (toggling it blind will leave it checked, and the screen then overwrites
your counts with its own mix), then set the per-species sliders -- `NUM_ANTHROPOD_SLIDER`,
`NUM_SKEL_SLIDER` and friends, mapped in `tools/oa_skirmish.py`. Save from inside `BattleView`,
not `BattlePreStart`, which refuses. And confirm with `gs battle` -- `foes_alive=8` -- rather than
the stage name: a finished mission still reports `BattleView` while its "you win" dialog is up,
because that dialog is a form overlay and not a stage change.

## What is verified

Every method on the `Renderer` interface:

| path | how |
| --- | --- |
| batched palette + RGB sprites | city view, 5420 sprites/frame, 0-pixel diff vs GL |
| textured quad (images too large to pack) | menu backgrounds, scale-surface composite |
| Surface used as a draw source | scale-surface composite; every cached control surface |
| `drawRotated` | loading-screen spinner, the only caller in the game -- Metal and GL both put it in an identical 50x50 box at x1229..1278, y669..718 |
| `drawFilledRect` / `drawRect` | form chrome; message-box borders |
| `drawLine` thickness 1 | `vequipscreen` selection boxes -- a real line primitive |
| `drawLine` thickness 2 | `equipscreen` highlight boxes -- the quad path, a deliberate divergence since Metal has no line width |
| `readBack` | every screenshot in this document |
| palette switching | city and battle |

Backend selection, which happens *before* the window is created because a window is built for one
graphics API and cannot be handed to the other:

| `Framework.Renderers` | selected | why it matters |
| --- | --- | --- |
| `Metal` | Metal Renderer | the default path |
| `Vulkan:Metal` | Metal Renderer | an unknown name is skipped, not mistaken for a GL request |
| `GL_2_0:Metal` | OGL2.0 Renderer | naming GL first wins; Metal is not forced on |
| `GLES_3_0:GL_2_0` | OGL2.0 Renderer | the pre-existing within-GL fallback still works |

The last row is the regression guard. When Metal is preferred but its factory fails, the Metal
window is torn down and rebuilt for GL rather than aborting -- verified by forcing
`MetalRendererFactory::create()` to return null.

`present()` has three paths, and the harness `screenshot` command **cannot verify any of them**:
`readBack()` reads the renderer's *owned* texture and is structurally blind to what reached the
drawable. Use `xcrun simctl io <udid> screenshot`, which captures the real thing.

- **blit** (sizes match, normal) and **rescale** (the drawable resized underneath us, e.g. a
  device rotation) produce byte-identical drawables: 0 / 5680128 when the rescale branch is
  forced on. That proves its quad geometry, Y flip and sampling.
- **backgrounded and resumed** -- foreground another app and return: 0 / 5680128, nothing logged.
  This is what a user hits on every app switch.
- **`nextDrawable` returning nil** cannot be triggered *naturally* here -- backgrounding on the
  simulator does not stop the frame loop (the harness keeps answering) and the simulator keeps
  issuing drawables where a device would refuse. It can be *forced*, though, the same way as the
  rescale branch: replace the `nextDrawable` call with `nil` and rebuild. Over **1348**
  executions the game kept running and answering the harness, logged **no crash**, and RSS went
  *down* (679 -> 432 MB), confirming the branch still commits its command buffer rather than
  accumulating. The frame is dropped, as intended.

Memory, checked two ways. A ~57000-frame soak held RSS flat at 643 MB. That only shows *net*
growth is zero, so `leaks` was run against a live process as well: 277 leaks / 18272 bytes out of
280 MB, all of it `NSSet`/`NSArray`/`CFDictionary` alongside `_NSXPCConnectionClassCache` and
`dispatch_queue_t` -- system framework and XPC bookkeeping -- and **zero** leak backtraces
mentioning `OpenApoc::`, `MetalRenderer`, `Spritesheet` or `BufferPool`. ARC and the hand-managed
autorelease pool are both accounted for.

ThreadSanitizer reports **0 warnings** over a live city at speed 3 for 30s plus a resize --
exercising the `BufferPool`'s cross-thread recycle from Metal's completion handler, the
`GpuTimer` accumulating from that same thread, and texture teardown during resize.

Building for TSan needs a detour: `CMakeLists.txt` sets `CMAKE_CXX_FLAGS_SANITIZE` with a plain
`set()`, so `-DCMAKE_CXX_FLAGS_SANITIZE=` on the command line is **silently overridden** and you
get `-fsanitize=address` compiled against `-fsanitize=thread` linked, which fails. Pass the flag
through `CMAKE_CXX_FLAGS` (which appends) with a normal build type instead:

```sh
cmake -S . -B build-tsan -DCMAKE_BUILD_TYPE=Debug -DEXTRACT_DATA=OFF -DENABLE_TESTS=OFF \
  -DCMAKE_CXX_FLAGS="-fsanitize=thread -fno-omit-frame-pointer" \
  -DCMAKE_C_FLAGS="-fsanitize=thread -fno-omit-frame-pointer" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=thread"
```

Whatever sanitizer you run, **check the binary exists and that the run actually reached a
rendering stage before reading the result** -- a sanitizer run that never launched reports zero
of everything.

AddressSanitizer (`cmake -DCMAKE_BUILD_TYPE=Sanitize`) reports **0 errors** across city, battle
and scaled scenarios, each driven through three resizes -- the path that destroys and rebuilds
textures while a frame may still reference them. Between the soak, `leaks` and ASan the memory
question is answered by three instruments with different blind spots: flat RSS alone would hide a
leak offset by an equal release, and neither RSS nor `leaks` would catch an overrun landing in
owned memory.

Resize: ten resizes spanning 640x480 to 1920x1080, including the clamp minimum, moved RSS from
678 MB to 679 MB, logged no drawable mismatch, and left the city correct at the final size. The
rescale branch never fired, which is the point -- `displayRefreshSize()` and the layer stay in
step. Soak: ~57000 frames, RSS flat at 643 MB, no drift.

`Framework.Screen.Mode` `borderless` and `fullscreen` both work; every other measurement here was
taken windowed.

**End to end, the project's own robot plays the game on Metal.** `tools/oa_campaign.py` does not
override `Framework.Renderers`, so it runs the build default -- the run's `log.txt` confirms
"Metal device: Apple M2 Max" / "Using renderer: Metal Renderer", at `--Framework.TargetFPS=1000`.
A short campaign fought **3 tactical missions, won 3**, reached game day 20 of week 3, with
**0 restarts and 0 crashes** and nothing logged about drawables. That path covers far more of the
UI than any hand-driven test here -- CityView, ScoreScreen, AlertScreen, BaseDefenseScreen,
briefing, debriefing, research and labs, base management -- and it is the only thing that has
exercised **real-time** battle mode; every battle tested by hand was turn-based. Re-run it after
any renderer change:

    python3 tools/oa_campaign.py --hours 0.15 --difficulty 1 --out /tmp/campaign

## Read the frame profile correctly

`--Framework.ProfileFrames=120` logs, from the Metal backend, passes per frame, how many of those
loaded the full target, and true GPU time from `MTLCommandBuffer.GPUStartTime/GPUEndTime`.

**`swap` is not GPU time on Metal.** `present()` blocks in `nextDrawable` waiting for the display,
so `swap` reports display throttling. The same iPad scene measured `swap 10.13 ms` foregrounded
and `swap 0.04 ms` on a freshly booted simulator -- with GPU at 0.17-0.26 ms in both. Read
`GPU x.xx ms/cmdbuf` for cost; read `swap` for pacing.

`Framework.SwapInterval` maps to `CAMetalLayer.displaySyncEnabled` and bites **only when the layer
is not composited by the window server** -- borderless or fullscreen, and always on iOS:

    borderless   SwapInterval=0   swap 0.04 ms   busy 4.16 ms   240.5 fps
    borderless   SwapInterval=1   swap 3.74 ms   busy 8.31 ms   120.3 fps

Windowed, the two are indistinguishable (both 120.5 fps on a 120 Hz panel): the window server
paces a windowed layer at the display rate whatever the layer asks for. So an automation run that
wants to outpace the display must be borderless too; `--Framework.SwapInterval=0` alone does
nothing in a window. It is also why every iOS figure here shows `swap 0.04 ms`.

The hardest case measured, and the one to quote if anyone asks whether Metal is fast enough:
macOS borderless at **3456x2169** (full Retina, 7.5M pixels), city live at **15945 sprites/frame**:

    7 draw calls   1.1 passes/frame   GPU 1.48 ms/cmdbuf
    update 1.18 ms   draw 3.97 ms   swap 0.04 ms   busy 5.19 ms   (193 fps)

Against 1280x720 that is 8x the pixels and 4x the sprites for 7x the GPU time -- what fill-bound
work should do -- and 1.48 ms is under a tenth of a 60 Hz budget. Note 15945 sprites collapsing
into 7 draw calls: the batching carries the frame. iPad simulator, city view, for comparison:
5420 sprites in 7 draw calls, 1.0-2.7 passes/frame, GPU 0.17 ms, busy 2.26 ms.

## The one known difference

Against `GLES_3_0` on iOS a small residue remains. What is established:

- Every differing channel differs by **exactly 1** -- 22378 of them on the city view, 57938 on the
  debriefing. Nothing differs by 2.
- It is **entirely the filtered upscale**. Re-run the A/B at `Framework.Screen.UiScale=1`, so
  controls composite 1:1 through `draw()` instead of a filtered `drawScaled()`, and the two
  backends are byte-identical: **0 / 1420032**. (Sanity-checked: uiScale 1 really did change the
  render, 83% of pixels differ from the uiScale 2 capture.) Remove the upscale, remove the
  difference.
- The proportion tracks the path mix: the debriefing is nearly all chrome (3.42%), the city view
  mostly exact texel-fetched map (0.93%).

What it is *not*: **precision**. The GLES shader declares `mediump sampler2D rgb_texture` where
Metal samples at full float, which is the obvious culprit -- so it was raised to `highp` and
rebuilt. The diff was **unchanged, 48614 pixels before and after, to the pixel**. The simulator
most likely ignores the qualifier. Do not retry that theory.

What remains, untested: the bilinear filtering itself -- Metal's `sample()` against GLES's
`texture()` with `GL_LINEAR` -- differing in texel-centre convention or float-to-unorm8 rounding.
Worth at most one part in 255.

## Things that are deliberate

- The Y flip is unconditional. GL flips only for the window, because a GL framebuffer texture's
  row 0 is its bottom; Metal render targets are uniformly top-left origin.
- The default surface owns a texture that is blitted to the drawable at present. That is what
  makes `readBack()` work: screenshots are taken during event processing, long after the previous
  frame's drawable went back to CoreAnimation.
- New render targets record a clear rather than being filled eagerly. A new `MTLTexture`'s
  contents are undefined, where a GL framebuffer texture comes back zeroed on every driver this
  engine has run on -- and `Control::render()` leans on that, rendering only when dirty and not
  necessarily touching every pixel. Left undefined, those pixels showed as solid magenta.
- The blend function is asymmetric on purpose, matching GL's
  `BlendFuncSeparate(SRC_ALPHA, ONE_MINUS_SRC_ALPHA, SRC_ALPHA, DST_ALPHA)`.

## Bugs found alongside, none of them in a renderer

- **Aspect ratio.** `computeDisplaySize()` clamped x and y to the 640x480 minimum *independently*,
  so a 16:9 window at 50% became 640x360 -> 640x480 and was stretched back across the drawable.
  Both backends suffered it. Now lifted by one uniform factor; covered by
  `tests/test_display_size.cpp`.
- **iOS discarded every display option.** `applyAppBundleDisplayDefaults()` set width, height,
  mode, auto-scale and UI scale *unconditionally* on iOS, so none of the five could be configured
  -- `UiScale=1` was silently replaced by 2. The desktop branch below it guards the same options
  with `config().optionOverridden(...)`. Each iOS default now applies only where the user has not
  spoken; copying the desktop's bail-out-entirely rule would be wrong, because an iOS app has no
  windowed mode to fall back to and setting one option must not cost the others their defaults.
  The rest of the pattern was audited: `os/app_paths.cpp` guards its bundle defaults with
  `isRelativeDefaultPath()`, and the launcher and fullscreen toggle write options deliberately.
  The iOS display branch was the only unguarded site.
- **The harness was off on all of iOS**, so no scripted test could run on the platform at all. It
  stays off on devices, where a listening socket is a privacy and App Store surface, and is
  enabled on the simulator via `TARGET_OS_SIMULATOR`.
- **`cmake --build --preset ios-simulator` silently produced Debug**, measured at 2.4x the CPU
  cost per frame (draw 4.97 ms vs 2.09 ms). The presets now pin `RelWithDebInfo`.

## Theories that were killed -- do not retry these

- **TBDR pass churn.** Ending an encoder and reopening it costs a full-target store and load, and
  `forms/control.cpp` binds a cached Surface per dirty control. Two independent analyses rated
  that cascade CRITICAL with detailed cost models. The pass counter measured **1.0-2.7 passes per
  frame** with the clock ticking. The cascade is rare because `Control::render()` only binds when
  `dirty`, and a label re-dirties only when its text actually changes. Do not perform that
  refactor without a measurement that contradicts this one.
- **`mediump` precision** as the cause of the ±1 residue. Falsified by experiment, above.
- **Frame pacing changing simulation progression.** Metal and GLES appeared to land in *different
  stages* from the same save -- `MessageBox` versus `BattleView` -- consistently across several
  runs, and the tempting explanation was that the simulation advances once per rendered frame
  while Metal presents on vsync and GLES tears. **Wrong.** Sampling the GLES run every five
  seconds instead of once shows `BattleView` for ~15 s and then `MessageBox` from t=20 s onward --
  the identical end state. The earlier runs merely caught GLES inside its brief window. Poll a
  stage repeatedly before concluding two backends diverge.

The pattern in all three: a tidy mechanism that fitted the data, asserted before it was tested.
Two of them survived several *consistent* observations, because the same flawed method produces
the same wrong answer every time. Change the method, not the sample size.

## Testing on iOS

The harness works on the simulator (see above), which is what makes the iOS half of this document
possible. Two things that cost time:

**Config beats the command line.** For any key present in `settings.conf`, the file wins -- so
`--Framework.Screen.UiScale=1` is silently ignored if the file names `UiScale`. Edit the file.

**Checking the device target without an `arm64-ios` dependency tree.** Building one is a long
detour; the platform risk is in our own translation units, so syntax-check those against the
device SDK using the simulator's headers:

```sh
export DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer
SDK=$(xcrun --sdk iphoneos --show-sdk-path)
VC=build-ios-sim/vcpkg_installed/arm64-ios-simulator/include
xcrun --sdk iphoneos clang++ -x objective-c++ -std=c++17 -fobjc-arc -fsyntax-only \
  -target arm64-apple-ios16.0 -isysroot "$SDK" \
  -I. -I"$VC" -Idependencies/glm -Idependencies/fmt/include \
  -DOPENAPOC_GLES -DOPENAPOC_METAL -DGLM_ENABLE_EXPERIMENTAL -Wall -Wextra \
  framework/render/metal/metal_renderer.mm
```

Both `metal_renderer.mm` and `framework.cpp` are clean this way against iPhoneOS 26.5. That also
proves the `TARGET_OS_OSX` guards are right: `MTLStorageModeManaged` does not exist on iOS, so a
mis-guarded block fails to compile rather than failing silently on a device.

## A note on "38/38 tests pass"

That figure is quoted throughout, so two traps behind it are worth knowing.
`data/mods/base/base_gamestate` is untracked, generated by the `extract-data` target, and its
contents differ by branch -- and the *default* build target re-runs the extractor, so a full
`make` can change what the gate is testing against. After the full build here it is 306260 bytes
(develop-lineage size, produced by this checkout's own extractor at 17:00), and
`ctest -R city_rules` passes.

Worse for a casual check: ten of the 38 test binaries take the gamestate path as an argument and
**exit 0 while testing nothing** when run bare -- `test_city_rules`, `test_psionics`,
`test_serialize`, `test_base_die`, `test_battle_large_unit`, `test_battle_disruptor_shield`,
`test_ground_vehicle_path`, `test_lab_assignment`, `test_tu_reservation`,
`test_unit_ai_priority`. They print "Must provide common and gamestate paths" and return success.
Run `ctest`, never a single binary by hand.

## Limits

- The `nextDrawable`-returns-nil branch cannot be triggered naturally on the simulator, but has
  been verified by forcing it (above). What is untested is the *real device* behaviour that would
  trigger it.
- (closed) The battlescape is pixel-diffed, including a **live combat frame with 8 hostiles**.
  See the results table.
- Everything here is macOS and the iOS *simulator*. No physical device was available.
