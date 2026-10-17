# PKE Native Prototype 0.1

Windows 10/11 x64 C++20 prototype that replaces the WebView2 gameplay runtime while reading existing PKE song/character assets.

## Windows build

Requires Visual Studio 2022, CMake 3.24+, Git, and network access for the first configure. Dependencies are pinned in `cmake/Dependencies.cmake`:

- SDL2 `release-2.30.12`
- SDL_image `release-2.8.4`
- SDL_ttf `release-2.22.0`
- miniaudio `0.11.21`
- stb `f0569113c93ad095470c54bf34a17b36646bbbb5`
- nlohmann/json `v3.11.3`
- tinyxml2 `10.0.0`
- Catch2 `v3.7.1`

```powershell
cmake -S native -B native/build -A x64
cmake --build native/build --config Debug
ctest --test-dir native/build -C Debug --output-on-failure
```

## Offline/core verification mode

For source-only CI environments without third-party packages/network:

```bash
cmake -S native -B native/build-offline -G Ninja -DPKE_NATIVE_OFFLINE_CORE=ON
cmake --build native/build-offline
ctest --test-dir native/build-offline --output-on-failure
```

This mode verifies portable gameplay/data logic but does not verify SDL/WASAPI integration.
The Linux sandbox used for the included verification cannot produce or run the Windows/MSVC executable; the Windows checklist below remains mandatory before calling the build production-ready.

## Phase 1 runtime layout

The native executable reads existing PKE assets directly. No conversion step is required.

- `Songs/<song-id>/Inst.ogg` is required.
- `Voices.ogg` is optional.
- Psych/Kade-style `<song-id>-<difficulty>.json`, `<difficulty>.json`, or `<song-id>.json` charts are supported.
- Character JSON + Sparrow XML are loaded by current PKE conventions; missing character art falls back to a diagnostic placeholder instead of softlocking.
- Arrow keys are the default bindings. `pke-native.json` can override roots, FPS cap, offsets, and keys.

The master rhythm clock is the mixed audio output frame cursor. Dropped render frames do not advance judgement state.

### Prototype controls

- Arrow keys: player lanes (configurable in `pke-native.json`)
- Enter: start the selected song
- Esc: leave gameplay / quit from selection
- F3: toggle the allocation-light timing diagnostics overlay
- F11: toggle borderless desktop fullscreen

Character compatibility accepts both common PKE/Psych animation schemas: `anim` + atlas `name`, and `name` + atlas `prefix`. If a character JSON points at an image basename different from the JSON filename, the sibling Sparrow XML/PNG/WebP is resolved from the JSON `image` field.

## Legacy PKE save migration

Prototype 0.1 now imports the existing PKE **Full Save v1** format without modifying or deleting the old save. On startup it checks, in order, for exported `PKE_Beat_Battle_FULL_SAVE_*.json` files next to the Native executable and the old Windows native-backup location under `%LOCALAPPDATA%/PKE.BeatBattle.v0.2.42/full-save-v1.json`. `PKE_LEGACY_SAVE_PATH` can point at one specific Full Save JSON when manual recovery is needed.

The migrated data is written separately to `%LOCALAPPDATA%/PKE.Native.BeatBattle/` on Windows (or `Save/` beside the executable when `LOCALAPPDATA` is unavailable):

- `legacy-full-save-v1.json` — a byte-for-byte copy of the complete old Full Save, including its IndexedDB payload.
- `native-save-v1.json` — Native-readable player summary containing RP, W/L/D, match count, Accuracy marker, peak RP/titles, selected skin/title, core settings, and Halloween Candy/purchases.

V1.14's `PKE_BEAT_BATTLE_PLAYER_PROGRESS_CHECKPOINT_V1` is the primary progression source, so the 15,000-player population blob does not have to be decompressed just to rescue the player's ranked progress. If that checkpoint is absent or malformed, the importer falls back to `PKE_BEAT_BATTLE_RANKING_JOURNAL_V1` for RP/W-L-D/peak-RP/title recovery; malformed optional settings or Halloween metadata do not block ranked-progress recovery. When more than one old save is found, all candidates are compared by the same safety marker order used by PKE (matches -> most recent match time -> accuracy match count); an equal or newer Native save is never replaced. The original Web/old-Native files are never deleted.

## Phase 1 exclusions

Prototype 0.1 intentionally does not yet implement Online Battle, Ranked RP/leaderboard persistence, Boss/Halloween systems, spectator/replay, Discord Rich Presence, GF rendering, or full menu parity.

## Windows acceptance checklist

After a Release build on Windows 10/11 x64, verify all of the following before calling the prototype production-ready:

1. Launches without WebView2/Chromium runtime files.
2. Loads one unchanged existing PKE song folder and unchanged BF/DAD character assets.
3. Inst/Voices start together and stay synchronized through the full song.
4. Arrow-key SICK/GOOD/BAD/MISS windows and sustain hold/release behave sanely.
5. Judgement does not change between 165 FPS and 240 FPS rendering.
6. Repeated song restarts and window/fullscreen transitions do not leak or stall.
7. Audio-device removal enters a visible recoverable state instead of continuing on wall-clock time.
8. Song completion returns cleanly to selection.
9. Capture diagnostics: worst frame interval, average update/render cost, audio underruns, and song-clock anomaly.

## Packaging

```powershell
cmake -S native -B native/build -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build native/build --config Release
ctest --test-dir native/build -C Release --output-on-failure
powershell -ExecutionPolicy Bypass -File native/scripts/package.ps1
```

The package script copies the executable and built runtime DLLs, example config, and README, then refuses to package WebView2/Chromium-named runtime files.

## Native compatibility runtime

QuickJS-NG v0.17.0 is pinned at commit `6d46d07d04041b40f4f49eaa7fdebe44c314c699` in `native/third_party/quickjs-ng`. Initialize it before configuring a fresh checkout:

```bash
git submodule update --init native/third_party/quickjs-ng
```

Only the `qjs` library is linked. The standalone CLI and std/os libc helpers are not built into the compatibility runtime. `QuickJsRuntime` has a synchronous JSON call boundary, explicit memory/stack/execution limits, exception stack diagnostics, and explicit garbage collection. Context creation, calls and destruction belong to one thread. `RequestInterrupt()` is a thread-safe, permanent stop request for that instance.

The offline-core mode still avoids fetching GUI/audio dependencies; it uses the already initialized pinned QuickJS submodule. A separate `quickjs_runtime` CTest entry covers the wrapper. The runtime is a Block 1 foundation; population and scene parity are separate implementation tasks.
