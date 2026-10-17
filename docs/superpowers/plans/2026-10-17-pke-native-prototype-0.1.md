# PKE Native Prototype 0.1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a Windows 10/11 x64 C++20 native PKE prototype that loads existing PKE song/character assets unchanged, plays one full 4-key song with V1.14-compatible timing/judgement/sustain behavior, and returns cleanly to song selection without WebView2/Chromium/JavaScript.

**Architecture:** A small native executable owns SDL events/rendering and a miniaudio/WASAPI audio device. Existing PKE JSON/XML/PNG/WebP/OGG assets are adapted into native data models by focused loaders; audio output progression is the master song clock, and gameplay/animation/rendering consume absolute song time rather than advancing by frame count.

**Tech Stack:** C++20, MSVC x64, CMake, SDL2, SDL_image, SDL_ttf, miniaudio, stb_vorbis, nlohmann/json, tinyxml2, Catch2 (tests).

**Spec:** `docs/superpowers/specs/2026-10-17-pke-native-engine-design.md`

## Global Constraints

- Target Windows 10/11 x64 only.
- Build with C++20, Visual Studio 2022 / MSVC x64, and CMake.
- Runtime must not depend on WebView2, Chromium, JavaScript, or browser storage APIs.
- Existing PKE song and character assets must load without conversion for normal Phase 1 use.
- During active gameplay: no asset discovery scans, large JSON serialization, synchronous disk writes, texture creation/destruction per frame, or heap allocation in the realtime audio callback.
- Gameplay timing must derive from the audio output frame cursor; render FPS must not advance rhythm state.
- Default judgement windows: SICK! ±45 ms, GOOD ±90 ms, BAD ±135 ms, MISS deadline ±180 ms.
- Default score values: SICK! 350, GOOD 200, BAD 100, MISS 0.
- Default head-note health gains: SICK! +0.040, GOOD +0.025, BAD +0.010.
- Default sustain grace: hold start 60 ms, hold release 90 ms.
- Logical gameplay coordinate space: 1920x1080; upscroll first; default non-VSync cap 240 FPS.
- Unsupported Phase 1 events/note types must be logged and ignored when safe; they must not crash or freeze gameplay.
- Dependencies must be vendored or reproducibly fetched and pinned by exact release/commit in CMake during implementation.

## Review Focus

1. Charts combining `mustHitSection` with `noteData > 3` must map side/lane exactly once, without double inversion — pinned by Task 3 mapping tests.
2. A song with no `Voices.ogg` must still start and finish when voices are optional — pinned by Task 7 audio tests and Task 10 lifecycle test.
3. Audio-device loss during play must stop gameplay visibly and return to a recoverable state, never continue on a wall clock — pinned by Task 7 device-loss test and Task 10 integration test.
4. Sparrow atlases using trimming, `frameX/frameY`, rotation, and explicit Psych `indices` must produce deterministic source/destination geometry — pinned by Task 5 atlas tests.
5. Malformed JSON/XML or unsupported chart metadata must surface one actionable diagnostic and return to selection rather than softlock — pinned by Tasks 3, 5, and 10 error-path tests.

---

## File Structure

Create the native project under `native/` while leaving the existing PKE distribution untouched.

- `native/CMakeLists.txt` — top-level build, executable, tests, dependency wiring.
- `native/cmake/Dependencies.cmake` — pinned third-party dependencies.
- `native/include/pke/Config.hpp` / `native/src/Config.cpp` — `pke-native.json` parsing and defaults.
- `native/include/pke/AssetLocator.hpp` / `native/src/AssetLocator.cpp` — song/character path resolution.
- `native/include/pke/Chart.hpp` — normalized chart/note/BPM metadata types.
- `native/include/pke/ChartParser.hpp` / `native/src/ChartParser.cpp` — existing Psych/Kade JSON adapter.
- `native/include/pke/RhythmEngine.hpp` / `native/src/RhythmEngine.cpp` — judgement, sustain, combo, score, health, accuracy.
- `native/include/pke/Character.hpp` — character/atlas/animation data types.
- `native/include/pke/CharacterLoader.hpp` / `native/src/CharacterLoader.cpp` — character JSON + Sparrow XML adapter.
- `native/include/pke/AudioEngine.hpp` / `native/src/AudioEngine.cpp` — OGG decode, shared Inst/Voices cursor, WASAPI/miniaudio output, pause/resume/device-loss state.
- `native/include/pke/CharacterAnimator.hpp` / `native/src/CharacterAnimator.cpp` — absolute-time animation state and sing/idle transitions.
- `native/include/pke/Renderer.hpp` / `native/src/Renderer.cpp` — SDL gameplay rendering and logical-space scaling.
- `native/include/pke/Diagnostics.hpp` / `native/src/Diagnostics.cpp` — preallocated frame/audio/input telemetry.
- `native/include/pke/GameSession.hpp` / `native/src/GameSession.cpp` — one loaded song session and state transitions.
- `native/include/pke/App.hpp` / `native/src/App.cpp` — SDL startup, selection screen, config, session ownership, shutdown.
- `native/src/main.cpp` — process entry point only.
- `native/tests/` — Catch2 unit/integration tests.
- `native/tests/fixtures/` — copies of user-provided real PKE fixtures used by tests, including `ejected-hard.json`.
- `native/README.md` — build/run/config/fixture instructions and known Phase 1 exclusions.

---

### Task 1: Native project bootstrap and configuration

**Files:**
- Create: `native/CMakeLists.txt`
- Create: `native/cmake/Dependencies.cmake`
- Create: `native/include/pke/Config.hpp`
- Create: `native/src/Config.cpp`
- Create: `native/src/main.cpp`
- Create: `native/tests/test_config.cpp`
- Create: `native/README.md`

**Interfaces:**
- Produces: `struct Config { std::filesystem::path songsRoot; std::filesystem::path charactersRoot; std::array<SDL_Scancode,4> playerKeys; bool vsync; int fpsCap; double inputOffsetMs; double audioLatencyCompensationMs; };`
- Produces: `Config LoadConfig(const std::filesystem::path& executableDir, const std::filesystem::path& configPath);`
- Produces: CMake targets `pke_native_core`, `pke_native`, and `pke_native_tests`.

- [ ] **Step 1: Write the failing config-defaults test**

Test `LoadConfig` with no file and assert `Songs/`, `Characters/`, arrow keys, `vsync=false`, `fpsCap=240`, and both offsets `0.0`.

- [ ] **Step 2: Configure/build tests and verify RED**

Run: `cmake -S native -B native/build -A x64 && cmake --build native/build --config Debug && ctest --test-dir native/build -C Debug --output-on-failure`
Expected: FAIL because `LoadConfig`/targets do not exist yet.

- [ ] **Step 3: Add pinned dependency wiring and minimal config implementation**

Use `native/cmake/Dependencies.cmake` as the only place that fetches/vendors SDL2, SDL_image, SDL_ttf, miniaudio, stb_vorbis, nlohmann/json, tinyxml2, and Catch2. Record exact tags/commits in that file and `README.md`.

- [ ] **Step 4: Run config test and verify GREEN**

Run the same CMake/build/ctest command.
Expected: PASS for `test_config` and successful launch of a minimal SDL window from `pke_native.exe`.

- [ ] **Step 5: Commit**

```bash
git add native
git commit -m "build: bootstrap PKE native prototype"
```

---

### Task 2: Asset root and file resolution

**Files:**
- Create: `native/include/pke/AssetLocator.hpp`
- Create: `native/src/AssetLocator.cpp`
- Create: `native/tests/test_asset_locator.cpp`

**Interfaces:**
- Consumes: `Config` from Task 1.
- Produces: `class AssetLocator` with `ResolveSongFolder(std::string_view)`, `ResolveChartFile(const path&, std::string_view difficulty)`, `ResolveInst(const path&)`, `ResolveVoices(const path&)`, and `ResolveCharacter(std::string_view id)`.
- Produces: `struct CharacterAssetPaths { path json; path xml; path image; };`

- [ ] **Step 1: Write failing resolution tests**

Create temporary fixture directories and assert relative roots resolve against executable dir, absolute roots stay absolute, `Inst.ogg` is required, `Voices.ogg` is optional, and missing required paths return an actionable error containing the exact path.

- [ ] **Step 2: Run `test_asset_locator` and verify RED**

Run: `ctest --test-dir native/build -C Debug -R asset_locator --output-on-failure`
Expected: FAIL because `AssetLocator` is undefined.

- [ ] **Step 3: Implement path resolution without directory-wide gameplay scans**

Use direct convention-based lookups only; no recursive character/song scan may occur after a `GameSession` begins loading.

- [ ] **Step 4: Run `test_asset_locator` and verify GREEN**

Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add native/include/pke/AssetLocator.hpp native/src/AssetLocator.cpp native/tests/test_asset_locator.cpp
git commit -m "feat: resolve existing PKE asset paths"
```

---

### Task 3: Psych/Kade chart parser and golden compatibility fixture

**Files:**
- Create: `native/include/pke/Chart.hpp`
- Create: `native/include/pke/ChartParser.hpp`
- Create: `native/src/ChartParser.cpp`
- Create: `native/tests/test_chart_parser.cpp`
- Create: `native/tests/test_chart_ejected_golden.cpp`
- Create: `native/tests/fixtures/ejected-hard.json`

**Interfaces:**
- Produces: `enum class Side { Player, Opponent };`
- Produces: `struct Note { double timeMs; int lane; double sustainMs; Side side; std::string noteType; bool altAnim; bool gfNote; };`
- Produces: `struct BpmPoint { double timeMs; double bpm; };`
- Produces: `struct Chart { std::vector<Note> notes; std::vector<BpmPoint> bpmTimeline; double scrollSpeed; double baseBpm; std::string player1; std::string player2; std::string gfVersion; std::vector<std::string> warnings; };`
- Produces: `Chart ParseChart(const std::filesystem::path& jsonPath);`

- [ ] **Step 1: Copy the user-provided `ejected-hard.json` into test fixtures and write failing mapping tests**

Assert: lane=`noteData % 4`; normal side derives from `mustHitSection` and flips once when `noteData > 3`; sustain length is preserved; notes sort by `timeMs` with stable input order for equal timestamps; optional `noteType` is preserved.

- [ ] **Step 2: Add failing BPM/scroll/malformed-input tests**

Assert base BPM and section BPM changes form a monotonic timeline; accepted scroll-speed fields resolve deterministically; malformed JSON throws/returns a parser error containing the file path; unsupported metadata adds a warning instead of crashing.

- [ ] **Step 3: Run parser tests and verify RED**

Run: `ctest --test-dir native/build -C Debug -R "chart_parser|chart_ejected" --output-on-failure`
Expected: FAIL because parser is not implemented.

- [ ] **Step 4: Implement `ParseChart` and normalized types**

Preserve metadata even when Phase 1 does not consume it. Keep absolute-note-half mapping behind an explicit parser option/type rather than hidden heuristics.

- [ ] **Step 5: Generate a V1.14 golden snapshot for `ejected-hard.json` and compare native output**

The golden test must compare note count and every normalized note's `timeMs/lane/side/sustainMs`, plus BPM timeline and scroll speed. Store only derived expected data needed by the test, not a second handwritten chart.

- [ ] **Step 6: Run parser/golden tests and verify GREEN**

Expected: PASS, including the Review Focus double-inversion case.

- [ ] **Step 7: Commit**

```bash
git add native/include/pke/Chart.hpp native/include/pke/ChartParser.hpp native/src/ChartParser.cpp native/tests
git commit -m "feat: parse existing PKE charts"
```

---

### Task 4: Judgement, score, health, accuracy, and sustain state

**Files:**
- Create: `native/include/pke/RhythmEngine.hpp`
- Create: `native/src/RhythmEngine.cpp`
- Create: `native/tests/test_rhythm_judgement.cpp`
- Create: `native/tests/test_rhythm_sustain.cpp`

**Interfaces:**
- Consumes: `Chart`, `Note`, `Side` from Task 3.
- Produces: `enum class Judgement { Sick, Good, Bad, Miss };`
- Produces: `struct JudgementConfig { double sickMs=45; double goodMs=90; double badMs=135; double missMs=180; double holdStartGraceMs=60; double holdReleaseGraceMs=90; };`
- Produces: `struct PlayStats { int rawScore; int combo; int maxCombo; int sick; int good; int bad; int miss; double health; double accuracyPercent; };`
- Produces: `class RhythmEngine` with `Reset(const Chart&)`, `KeyDown(int lane,double songTimeMs)`, `KeyUp(int lane,double songTimeMs)`, `Update(double songTimeMs)`, `Stats()`, and read-only note-state access for rendering.

- [ ] **Step 1: Write failing exact-boundary judgement tests**

Test offsets exactly `±45`, just beyond 45, exactly `±90`, just beyond 90, exactly `±135`, just beyond 135, and the `180 ms` miss deadline. Assert score and health values from the spec and assert there is no SHIT judgement.

- [ ] **Step 2: Write failing sustain timing tests**

Cover sustain start within/outside 60 ms grace, release within/outside 90 ms grace, long hold through multiple `Update` call cadences, and identical result at 60 FPS vs 240 FPS simulated update spacing.

- [ ] **Step 3: Run rhythm tests and verify RED**

Run: `ctest --test-dir native/build -C Debug -R rhythm --output-on-failure`
Expected: FAIL.

- [ ] **Step 4: Implement minimal absolute-time rhythm state machine**

Hit selection must use `songTimeMs`, never frame count. Accuracy calculation must be deterministic and guarded for zero judged notes.

- [ ] **Step 5: Run rhythm tests and verify GREEN**

Expected: PASS at all exact boundaries and both simulated frame rates.

- [ ] **Step 6: Commit**

```bash
git add native/include/pke/RhythmEngine.hpp native/src/RhythmEngine.cpp native/tests/test_rhythm_*.cpp
git commit -m "feat: add native rhythm judgement engine"
```

---

### Task 5: Character JSON and Sparrow atlas compatibility

**Files:**
- Create: `native/include/pke/Character.hpp`
- Create: `native/include/pke/CharacterLoader.hpp`
- Create: `native/src/CharacterLoader.cpp`
- Create: `native/tests/test_character_loader.cpp`
- Create: `native/tests/test_sparrow_geometry.cpp`
- Create: `native/tests/fixtures/character/` fixture assets extracted/copied from an existing PKE character.

**Interfaces:**
- Produces: `struct AtlasFrame { std::string name; SDL_Rect source; int frameX; int frameY; int frameWidth; int frameHeight; bool rotated; };`
- Produces: `struct AnimationDef { std::string anim; std::string prefix; std::vector<int> indices; double fps; bool loop; SDL_Point offset; std::vector<size_t> frameIndices; };`
- Produces: `struct CharacterDefinition { path imagePath; std::unordered_map<std::string,AnimationDef> animations; double scale; SDL_Point position; bool flipX; bool noAntialiasing; double singDuration; std::vector<AtlasFrame> atlas; std::vector<std::string> warnings; };`
- Produces: `CharacterDefinition LoadCharacter(const CharacterAssetPaths&);`
- Produces: pure geometry helper `FramePlacement ComputeFramePlacement(const AtlasFrame&, SDL_Point authoredOffset, double scale, bool flipX);`

- [ ] **Step 1: Write failing character-field and prefix-order tests**

Assert required JSON fields, optional `indices`, numeric prefix ordering, offsets/fps/loop/scale/position/flip/no-antialiasing/sing-duration.

- [ ] **Step 2: Write failing trimmed/rotated Sparrow geometry tests**

Cover `frameX/frameY/frameWidth/frameHeight`, `rotated=true`, explicit indices, and a missing prefix that emits exactly one warning and leaves an idle fallback path available.

- [ ] **Step 3: Run character tests and verify RED**

Run: `ctest --test-dir native/build -C Debug -R "character|sparrow" --output-on-failure`
Expected: FAIL.

- [ ] **Step 4: Implement loader and pure atlas geometry math**

Do not create SDL textures in the parser. Texture creation belongs to Renderer/session preload so tests remain headless.

- [ ] **Step 5: Run character tests and verify GREEN**

Expected: PASS, including Review Focus trimmed/rotated/indices cases.

- [ ] **Step 6: Commit**

```bash
git add native/include/pke/Character.hpp native/include/pke/CharacterLoader.hpp native/src/CharacterLoader.cpp native/tests
git commit -m "feat: load existing PKE character atlases"
```

---

### Task 6: Character animation state from absolute song time

**Files:**
- Create: `native/include/pke/CharacterAnimator.hpp`
- Create: `native/src/CharacterAnimator.cpp`
- Create: `native/tests/test_character_animator.cpp`

**Interfaces:**
- Consumes: `CharacterDefinition` from Task 5 and rhythm hit/sustain events from Task 4.
- Produces: `class CharacterAnimator` with `SetCharacter(const CharacterDefinition*)`, `TriggerSing(int lane,double songTimeMs,double holdUntilMs)`, `ReturnToIdle(double songTimeMs)`, `Update(double songTimeMs)`, and `CurrentFrame()`.
- Produces: `struct AnimationFrameView { size_t atlasFrameIndex; std::string_view animationName; double animationStartMs; };`

- [ ] **Step 1: Write failing idle/sing timing tests**

Assert LEFT/DOWN/UP/RIGHT semantic mapping, non-loop animation frame selection from absolute song time, sustain hold until `holdUntilMs`, and deterministic return to idle independent of update frequency.

- [ ] **Step 2: Run animator tests and verify RED**

Run: `ctest --test-dir native/build -C Debug -R character_animator --output-on-failure`
Expected: FAIL.

- [ ] **Step 3: Implement absolute-time animator**

Frame index must be a pure function of `(songTimeMs - animationStartMs) * fps`; looping/non-looping behavior comes from `AnimationDef`.

- [ ] **Step 4: Run animator tests and verify GREEN**

Expected: PASS at multiple simulated frame rates.

- [ ] **Step 5: Commit**

```bash
git add native/include/pke/CharacterAnimator.hpp native/src/CharacterAnimator.cpp native/tests/test_character_animator.cpp
git commit -m "feat: animate PKE characters from song time"
```

---

### Task 7: Native audio engine, shared Inst/Voices cursor, and WASAPI master clock

**Files:**
- Create: `native/include/pke/AudioEngine.hpp`
- Create: `native/src/AudioEngine.cpp`
- Create: `native/tests/test_audio_mix_cursor.cpp`
- Create: `native/tests/test_audio_state.cpp`
- Create: `native/tests/fixtures/audio/` small deterministic OGG fixtures.

**Interfaces:**
- Produces: `enum class AudioState { Stopped, Ready, Playing, Paused, DeviceLost, Finished, Error };`
- Produces: `struct DecodedAudio { uint32_t sampleRate; uint32_t channels; std::vector<float> pcm; uint64_t frameCount; };`
- Produces: `class AudioEngine` with `Load(instPath, optional<voicesPath>)`, `Start()`, `Pause()`, `Resume()`, `Stop()`, `SeekFrames(uint64_t)`, `SongFrame()`, `SongTimeMs()`, `State()`, and `LastError()`.
- Produces: a testable non-device `MixFrames(float* output,uint32_t frameCount)` path that shares the same cursor rules as the realtime callback.

- [ ] **Step 1: Write failing decode/common-format/shared-start tests**

Assert Inst and Voices are fully decoded before `Ready`, converted to one sample rate/channel layout, and first mixed samples begin from the same song frame. Assert missing optional Voices is valid.

- [ ] **Step 2: Write failing master-clock/pause/resume tests**

Assert `SongFrame` advances only by frames supplied to the mixer, `SongTimeMs = SongFrame * 1000 / sampleRate + latencyCompensation`, pause freezes both streams, resume continues one shared cursor, and long synthetic playback never creates relative Inst/Voices drift.

- [ ] **Step 3: Write failing device-loss and realtime-safety instrumentation tests**

Simulate the device notification/state path and assert transition to `DeviceLost` with no wall-clock fallback. Add allocation instrumentation around `MixFrames` and assert zero heap allocations inside the callback/mix path after preload.

- [ ] **Step 4: Run audio tests and verify RED**

Run: `ctest --test-dir native/build -C Debug -R audio --output-on-failure`
Expected: FAIL.

- [ ] **Step 5: Implement full preload decode and miniaudio/WASAPI device ownership**

The realtime callback may only read prebuilt PCM/state and write output; it must not log, allocate, parse, perform file I/O, or take a blocking mutex.

- [ ] **Step 6: Run audio tests and verify GREEN**

Expected: PASS for shared cursor, optional Voices, pause/resume, long-run drift, device-loss state, and callback allocation instrumentation.

- [ ] **Step 7: Commit**

```bash
git add native/include/pke/AudioEngine.hpp native/src/AudioEngine.cpp native/tests/test_audio_*.cpp native/tests/fixtures/audio
git commit -m "feat: add WASAPI-backed master song clock"
```

---

### Task 8: Diagnostics ring buffers and frame/audio observability

**Files:**
- Create: `native/include/pke/Diagnostics.hpp`
- Create: `native/src/Diagnostics.cpp`
- Create: `native/tests/test_diagnostics.cpp`

**Interfaces:**
- Produces: `struct FrameSample { double intervalMs; double updateMs; double renderMs; double songTimeMs; };`
- Produces: `struct AudioSample { uint64_t songFrame; uint32_t callbackFrames; bool underrun; };`
- Produces: `class Diagnostics` with fixed-capacity `RecordFrame`, `RecordAudio`, `RecordInputOffset`, `Snapshot`, and `Reset`.

- [ ] **Step 1: Write failing fixed-capacity tests**

Assert recording past capacity overwrites oldest samples without allocating and snapshot order remains chronological.

- [ ] **Step 2: Run diagnostics tests and verify RED**

Run: `ctest --test-dir native/build -C Debug -R diagnostics --output-on-failure`
Expected: FAIL.

- [ ] **Step 3: Implement preallocated ring buffers**

No formatting/logging is allowed in `RecordAudio`; textual formatting occurs only when UI/export consumes a snapshot.

- [ ] **Step 4: Run diagnostics tests and verify GREEN**

Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add native/include/pke/Diagnostics.hpp native/src/Diagnostics.cpp native/tests/test_diagnostics.cpp
git commit -m "feat: add allocation-free runtime diagnostics"
```

---

### Task 9: SDL renderer and gameplay scene

**Files:**
- Create: `native/include/pke/Renderer.hpp`
- Create: `native/src/Renderer.cpp`
- Create: `native/tests/test_render_math.cpp`

**Interfaces:**
- Consumes: chart/note state (Tasks 3/4), character data/animators (Tasks 5/6), diagnostics (Task 8).
- Produces: `struct Viewport { SDL_Rect destination; double scale; };`
- Produces: `Viewport ComputeLetterboxedViewport(int outputW,int outputH,int logicalW=1920,int logicalH=1080);`
- Produces: `class Renderer` with preload methods for character/note textures and `Draw(const RenderState&)`.
- Produces: `struct RenderState` containing song time, visible note states, BF/DAD frame views, health, score, accuracy, combo/judgement, song name, and diagnostics toggle.

- [ ] **Step 1: Write failing viewport and note-position tests**

Assert 16:9 and non-16:9 outputs letterbox correctly, note Y position is a pure function of `(note.timeMs-songTimeMs)`, scroll speed, and receptor position, and dropped render frames do not mutate gameplay state.

- [ ] **Step 2: Run render-math test and verify RED**

Run: `ctest --test-dir native/build -C Debug -R render_math --output-on-failure`
Expected: FAIL.

- [ ] **Step 3: Implement SDL texture preload and gameplay drawing**

Draw opponent/player receptors, note heads, sustain body/end, DAD/BF, health bar, song name, combo/judgement, raw score, accuracy, and toggleable FPS/audio diagnostics. Apply CharacterLoader trim/rotation geometry; do not create/destroy textures per frame.

- [ ] **Step 4: Run render tests and verify GREEN**

Expected: PASS for pure rendering math; manual smoke launch displays a static fixture scene at 1920x1080 logical coordinates.

- [ ] **Step 5: Commit**

```bash
git add native/include/pke/Renderer.hpp native/src/Renderer.cpp native/tests/test_render_math.cpp
git commit -m "feat: render native PKE gameplay scene"
```

---

### Task 10: GameSession integration, song lifecycle, input, and recoverable errors

**Files:**
- Create: `native/include/pke/GameSession.hpp`
- Create: `native/src/GameSession.cpp`
- Create: `native/include/pke/App.hpp`
- Create: `native/src/App.cpp`
- Modify: `native/src/main.cpp`
- Create: `native/tests/test_game_session.cpp`
- Create: `native/tests/test_song_lifecycle.cpp`

**Interfaces:**
- Consumes: Tasks 1–9.
- Produces: `enum class SessionState { Loading, Ready, Countdown, Playing, Completed, Failed, DeviceLost };`
- Produces: `class GameSession` with `Load(SongSelection)`, `HandleKeyDown(SDL_Scancode)`, `HandleKeyUp(SDL_Scancode)`, `Update()`, `Render()`, `State()`, `FailureMessage()`, and `RequestStop()`.
- Produces: `class App` with states `SongSelection`, `Gameplay`, and `FatalStartupError`.

- [ ] **Step 1: Write failing lifecycle integration test with fake audio clock**

Inject a test audio-clock interface and assert `Loading -> Ready -> Countdown -> Playing -> Completed -> SongSelection`; the same chart must yield identical judgement/miss state under simulated 60 Hz and 240 Hz render/update schedules.

- [ ] **Step 2: Write failing recoverable-error tests**

Cover missing Inst, malformed chart, missing character asset with diagnostic fallback when available, unsupported note/event warning, optional missing Voices, and audio-device loss. Assert every case reaches selection/error UI and never remains in a frozen gameplay state.

- [ ] **Step 3: Run lifecycle tests and verify RED**

Run: `ctest --test-dir native/build -C Debug -R "game_session|song_lifecycle" --output-on-failure`
Expected: FAIL.

- [ ] **Step 4: Implement `GameSession` orchestration and SDL input sampling**

Consume input time from `AudioEngine::SongTimeMs()` plus configured input offset. Preload all chart/audio/character/textures before countdown. On `DeviceLost` stop rhythm progression and surface the exact audio error.

- [ ] **Step 5: Implement simple song selection and folder/config launch path in `App`**

Allow selection from configured Songs root and an optional command-line/config song folder. Do not add current PKE menu parity.

- [ ] **Step 6: Run lifecycle tests and verify GREEN**

Expected: PASS, including Review Focus optional Voices/device-loss/malformed-data paths and clean return after song completion.

- [ ] **Step 7: Commit**

```bash
git add native/include/pke/GameSession.hpp native/src/GameSession.cpp native/include/pke/App.hpp native/src/App.cpp native/src/main.cpp native/tests/test_game_session.cpp native/tests/test_song_lifecycle.cpp
git commit -m "feat: complete native song lifecycle"
```

---

### Task 11: Real PKE compatibility run and Prototype 0.1 packaging

**Files:**
- Modify: `native/README.md`
- Create: `native/tests/test_acceptance_fixture.cpp`
- Create: `native/pke-native.example.json`
- Create: `native/scripts/package.ps1`

**Interfaces:**
- Consumes: complete prototype from Tasks 1–10.
- Produces: reproducible Release package containing `PKE Native Prototype.exe`, required SDL runtime DLLs, example config, and README; external PKE assets remain external and are not duplicated into the package.

- [ ] **Step 1: Add automated acceptance fixture test**

Load the real `ejected-hard.json` fixture plus selected real BF/DAD fixture metadata and assert parser/character/rhythm initialization succeeds, note counts match the golden test, and a synthetic perfect-input playback reaches `Completed` without softlock.

- [ ] **Step 2: Run the complete automated suite**

Run: `cmake --build native/build --config Release && ctest --test-dir native/build -C Release --output-on-failure`
Expected: all tests PASS, zero failures.

- [ ] **Step 3: Perform Windows manual timing/compatibility verification**

Verify on Windows x64: launch without WebView2, load one unchanged existing PKE song folder, unchanged BF/DAD assets, Inst/Voices sync start-to-finish, Arrow-key judgement sanity, sustain start/hold/release, 165/240-FPS rendering with stable judgement, pause/resume, repeated restarts, fullscreen/windowed transition, audio-device loss recovery, and clean song-end return.

- [ ] **Step 4: Capture diagnostic evidence**

Run at least one full song with diagnostics enabled and record: worst frame interval, average update/render cost, audio underrun count, and maximum observed song-clock anomaly. Acceptance requires no periodic browser/GC-style stall source because no browser runtime exists; any native stall discovered must be documented rather than hidden.

- [ ] **Step 5: Package Release build**

Run: `powershell -ExecutionPolicy Bypass -File native/scripts/package.ps1`
Expected: package contains executable, dependency DLLs, example config, README, and no WebView2/Chromium runtime files.

- [ ] **Step 6: Re-run packaged executable smoke test from a clean folder**

Expected: executable opens, resolves external configured asset roots, plays the selected real song, completes, and returns to selection.

- [ ] **Step 7: Commit**

```bash
git add native/README.md native/tests/test_acceptance_fixture.cpp native/pke-native.example.json native/scripts/package.ps1
git commit -m "release: verify PKE Native Prototype 0.1"
```

---

## Plan Self-Review

- **Spec coverage:** All Phase 1 requirements map to Tasks 1–11. Online/Ranked/Boss/Halloween/Replay remain explicitly excluded.
- **Type consistency:** Chart -> RhythmEngine -> GameSession -> Renderer uses one normalized `Note`/`Chart` model; character data is immutable after preload; AudioEngine owns the authoritative song frame.
- **Timing consistency:** No task advances rhythm state by frame count; tests explicitly compare simulated 60 Hz and 240 Hz schedules.
- **Error recovery:** Required failure modes return to selection or a visible error state; device loss never falls back to a fake wall clock.
- **Performance:** Realtime audio path is allocation-free after preload; gameplay avoids per-frame texture creation and synchronous disk writes; diagnostics use preallocated ring buffers.
- **Compatibility:** Real PKE fixtures are introduced early (Task 3/5) and exercised again in final acceptance (Task 11), reducing the risk of building a clean engine that cannot read actual PKE content.
