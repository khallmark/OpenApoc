# GLES 3.0 on macOS, and a fair three-way renderer comparison

**Date:** 2026-08-27
**Branch:** `fix/ios-zero-viewport`
**Companion:** `docs/solutions/2026-08-27-metal-renderer.md` (read that first for the Metal backend)

Two goals: make `GLES_3_0` initialise on macOS so Metal has an architecturally comparable rival,
and improve `GL_2_0`. Both are done, and the resulting benchmark overturns two claims made
earlier in the Metal work.

## Result

All three backends are now **byte-identical** on macOS — `0` differing pixels out of `921,600`
at 1280x720 and out of `7,496,064` at 3456x2169.

3456x2169 borderless, paused city save, ~15,900 sprites, `--Framework.SwapInterval=0`:

| backend | draw (ms) | busy (ms) | fps | draw calls |
| --- | --- | --- | --- | --- |
| Metal | 4.07–4.18 | 4.13–4.36 | 229–242 | 7 |
| GL 2.0 (indexed, this work) | 4.55–4.70 | 5.65–5.86 | 171–177 | 32 |
| GL 2.0 (before this work) | 5.63–5.85 | 6.71–6.93 | 144–149 | 32 |
| GLES 3.0 core (this work) | 5.33–5.40 | 6.38–6.46 | 155–157 | not counted |

1280x720 windowed, ~4,200 sprites:

| backend | draw (ms) | fps |
| --- | --- | --- |
| GL 2.0 (indexed) | 1.02–1.10 | 333–370 |
| GLES 3.0 core | 1.20–1.25 | 305–325 |
| GL 2.0 (before) | 1.52–1.56 | 230–233 |

## Why GLES 3.0 could not initialise on macOS

Four faults blocked initialisation, each fatal on its own. The first is the root cause; the rest
were only reachable once it was fixed. A fifth fault -- the sampler mismatch in the next section
-- did **not** block initialisation: it let the renderer come up and corrupted its output.

1. **No core profile was ever requested.** `framework.cpp` guarded
   `SDL_GL_CONTEXT_PROFILE_CORE` behind `#ifdef SDL_OPENGL_CORE` — **nothing in the build ever
   defines `SDL_OPENGL_CORE`**. So macOS was asked for GL 3.0 with no profile, which it cannot
   grant, and the existing retry path lowered the request to 2.0 and got a **legacy 2.1
   context**. `GL_ARB_ES3_compatibility` is a GL 4.3 feature and Apple caps desktop GL at 4.1,
   so on macOS that extension can *never* be present — the check could not have succeeded on any
   Mac, ever.

   Fixed by an opt-in `--Framework.GLProfile=core`, which requests Core 4.1. It is opt-in
   because `GL_2_0` cannot run on a core profile (client-side vertex arrays are removed and its
   shaders are `#version 110`); `OGL20RendererFactory::create()` now declines cleanly on a core
   context rather than failing in ways that look like driver bugs.

2. **`Gles3::supported()` required an extension macOS cannot have.** Now accepts any desktop
   context reporting GL >= 3.3 by version string, which is where texture arrays, integer
   textures, instanced arrays, VAOs and explicit attribute locations all became core. The
   `GL_ARB_ES3_compatibility` path is kept for contexts below 3.3.

3. **`glGetString(GL_EXTENSIONS)` returns NULL in a core profile** and the `Gles3` constructor
   assigned it straight into a `std::string` — a SIGSEGV, not an empty string. All four
   `glGetString` results are now guarded, and `ExtensionString` is rebuilt from `glGetStringi`
   when the monolithic query is unavailable.

4. **The ES shading language is rejected by a desktop core profile.** `CreateShader()` now
   applies a small substitution table when `gl->isDesktopContext()`, leaving the ES sources
   untouched so iOS is unaffected.

## The signed/unsigned sampler mismatch — the interesting one

With the four faults above fixed, GLES rendered the city with **perfect geometry and wrong
colours**: 27.8% of pixels differed from Metal, with large deltas clustered at multiples of 4.

The paletted textures are `GL_R8UI`, an **unsigned** format. The shaders declared
`isampler2DArray` / `isampler2D` — **signed**. ES 3.0 drivers tolerate this; desktop GL requires
the sampler type to match the internal format and leaves the mismatch **undefined**.

The failure mode is what made it hard to read: zero still read back as zero, so the
`if (idx == 0) discard` that produces every sprite silhouette kept working perfectly, while
nonzero indices came back wrong and each affected sprite drew from the wrong palette entry.
Correct shapes, wrong colours. The multiple-of-4 delta clustering was the clue that pointed at
palette entries — original-game palettes are 6-bit VGA values scaled by 4.

Swapping to `usampler` in the desktop substitution table took the diff to **0 / 921,600**.

## GL 2.0: indexed quads

`PaletteBatchProgram` already batched well — 15,906 sprites into 32 draw calls — so the draw-call
count was never the problem. The cost was the CPU-side vertex expansion: six vertices per sprite
at 20 bytes each, about 1.9 MB built and uploaded every frame.

Storing four corners and expanding with a shared static index buffer (`glDrawElements`) cuts
both by a third. Measured: **-32% draw at 720p, -20% draw at 4K, +19% fps at 4K**, pixel-identical
throughout. No extension is required.

## Claims from the Metal work that this overturns

- **"GL_2_0 is the older backend without spritesheet batching."** Wrong. It has a palette atlas
  and a batch program, and collapses ~15,900 sprites into 32 draw calls.
- **"The architecturally fair rival is GLES_3_0, which can't run on macOS."** It can now, and it
  does **not** beat `GL_2_0` — the fully instanced ES3 design lands within a few percent of it,
  and after the indexed-quad change `GL_2_0` is the faster of the two.
- **"Metal is ~12x faster than GLES on iOS."** That number was the iOS *simulator* emulating
  GLES and is not a hardware figure. The comparable architectural gap **on macOS**, native on
  both sides, is ~1.4x. The gap on a real iOS device is still **unmeasured** -- the simulator
  remains the only iOS GLES data there is.

## Traps worth remembering

- **A windowed Metal layer on macOS is paced by the window server** regardless of
  `displaySyncEnabled`, so `--Framework.SwapInterval=0` does nothing there. A first pass at this
  benchmark ran windowed and had Metal pinned at exactly 120.4 fps (the panel rate) while both
  GL backends ran uncapped — Metal looked *slower* than GL 2.0. **Benchmark borderless or
  fullscreen only.** The same pacing also inflates `GPU ms/cmdbuf` to ~7.9 ms, because the held
  present lands inside the command buffer's GPU interval; that figure is not GPU work.
- **Run a same-renderer control before reading any A/B diff.** Both controls here were
  `0 / 921,600`, which is what made the 27.8% believable as a real defect rather than animation.
- `--Framework.ScreenWidth` is silently ignored; the options are `Framework.Screen.Width` /
  `Framework.Screen.Height`. Unrecognised options only produce a `W ... Ignoring option` line on
  stderr, which is easy to miss.
- `LogInfo` never reaches stderr — `defaultLogFunction` drops anything at Info or below. Renderer
  selection diagnostics only exist in
  `~/Library/Application Support/OpenApoc/log.txt`.
- One theory tested and falsified: the difference was **not** `uiScale`-dependent. It was 27.8%
  at uiScale 2 and 27.8% at uiScale 1. Do not retry that.
- One diagnosis made and retracted before it reached code: `flushBatch()` looked like it
  destroyed `batchVertices`' capacity on every flush. It does not — the last two lines swap the
  cleared vector back. Read the whole function.
