# Twist Your Guts — parallel bass processor (bass)

Per-repo working memory for Claude Code sessions on this plugin. Part of the **Metal up your ass** symphonic-metal plugin suite (`github.com/metal-up-your-ass`).

## What this is
Twist Your Guts is a Parallax-style **parallel bass processor** for metal: it LR4-splits the bass into low/high bands, parallel-compresses the lows, runs the highs through selectable distortion voicings, then sums back through a 4-band EQ and an IR cabinet loader. AU / VST3 / Standalone.

## Status (pre-1.0)
M0 bootstrap complete (scaffold, CI, docs, ADRs). The LR4 crossover + latency framework is the first DSP landed with tests; the full v1.0 signal path (compressor, distortion voicings, gate, EQ, IR loader) is in progress. See GitHub **milestones/issues** for open work, and `README.md` for the full v1.0 feature scope and signal-flow diagram.

## DSP (v1.0 target)
`Input Trim → Gate → LR4 split (60–1000 Hz) → [Low: parallel comp] + [High: distortion voicing (Gnaw/Wool/Razor) → drive → tone → blend] → delay-compensated sum → 4-band EQ → IR loader → Output`.
- Crossover already implemented in `src/dsp/Crossover.{h,cpp}` (LR4, flat-sum tested) — this is the canonical crossover the suite's `triptych` multiband reuses.
- Params via APVTS (`src/params/`).

## Build & test
```sh
export CPM_SOURCE_CACHE="$HOME/.cache/CPM"
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target Tests TwistYourGuts_Standalone --parallel 4
ctest --test-dir build --output-on-failure
```
Release/universal + pluginval + auval run in CI.

## Conventions & guardrails
- JUCE 8.0.14 via CPM · C++20 · AGPLv3 · Pamplejuce `SharedCode` · manufacturer `Yvsv`, plugin code `Tygt`, `com.yvesvogl.twistyourguts`.
- Real-time safety (no alloc/lock/IO/log on the audio thread; allocate in `prepareToPlay`; `reset()` clears state; `ScopedNoDenormals`; smoothed params).
- DryWetMixer gotcha: prime `setWetMixProportion` before `reset()` (see the suite's overture for the pattern).
- `main` protected — feature branch + PR, green CI required, Conventional Commits. New DSP needs tests (flat-sum/null, NaN/Inf, state round-trip).

## Roadmap
GitHub milestones (M1 DSP & tests · M2 presets/state · M3 GUI & a11y · M4 release/signing/v1.0.0) + issues, plus the README roadmap table.

## Suite context
This is the bass member of the suite; its LR4 crossover is the reference pattern reused by `triptych`. Sibling plugins: overture, tenebrae, nave, silentium, requiem, seraph, aureate, firmament, triptych, apotheosis.
