# PKE Beat Battle Native Engine — Windows C++/SDL2 Design

Date: 2026-10-17
Status: Design for review
Baseline: PKE Beat Battle V1.14 Ranked Save Durability
Target: Windows 10/11 x64

## 1. Goal

Replace the gameplay engine's WebView2/Chromium/JavaScript runtime with a native Windows C++ engine while preserving existing PKE content formats as closely as practical.

The first deliverable is a **Native Prototype** that can load existing PKE song and character assets without converting them, play one full 4-key song, judge player input, animate BF/DAD, keep Inst/Voices synchronized, and finish the song cleanly.

This is not a rewrite of Online Battle, the 15,000-player population simulation, Ranked, Boss Battle, Halloween, Replay, or persistence yet. Those remain in the current PKE until the native core proves timing and compatibility.

## 2. Non-goals for Phase 1

Phase 1 does not implement:

- Online Battle / LIVE rooms / matchmaking
- 15,000-player background population simulation
- Ranked RP and leaderboard persistence
- Normal Boss / Halloween Boss / Halloween Shop
- Spectator mode
- Replay recording/playback
- Discord Rich Presence
- GF rendering and advanced Psych events unless required by the selected test song
- Full editor/UI parity with current PKE
- Cross-platform support

Unsupported chart events must not crash the prototype. They should be reported in a diagnostic log and ignored unless they affect core 4-key gameplay.

## 3. Platform and toolchain

### Runtime

- Windows 10/11 x64 only
- Native Win32 executable
- No WebView2
- No Chromium
- No JavaScript runtime
- No browser storage APIs

### Language and build

- C++20
- Visual Studio 2022 / MSVC x64
- CMake project

### Libraries

- SDL2: window, input, timing, renderer
- SDL_image: PNG/WebP texture loading
- SDL_ttf: prototype HUD/text
- miniaudio: WASAPI output, mixer/device ownership, master clock integration
- stb_vorbis: OGG Vorbis decode for Inst/Voices, integrated behind the AudioEngine
- nlohmann/json: PKE/Psych/Kade JSON parsing
- tinyxml2: Sparrow XML parsing

Dependencies should be vendored or reproducibly fetched by CMake. The final distributed prototype should not require Visual Studio to run.

## 4. Compatibility principle

The native engine is an **adapter around existing PKE data**, not a new data format.

The prototype should first look for external asset roots configured in `pke-native.json`:

```json
{
  "songsRoot": "D:/PKE/Songs",
  "charactersRoot": "D:/PKE/Characters"
}
```

Relative paths are allowed. If the file is absent, the engine may fall back to `Songs/` and `Characters/` beside the executable.

No asset conversion step is required for normal use.

## 5. Existing chart compatibility

Source review of V1.14 shows the primary chart path is Psych/Kade-style section JSON:

- root/song object containing `notes`
- each section contains `sectionNotes`
- note array is at least `[timeMs, noteData, sustainMs]`
- optional fourth value is `noteType`
- `mustHitSection` determines side in the normal mapping
- `noteData > 3` flips side in the normal mapping
- lane is `noteData % 4`
- PKE also has an absolute-note-half mapping mode for charts that need it
- `song.speed`, `song.scrollSpeed`, or root `scrollSpeed` may supply scroll speed
- BPM changes are supported by the current engine
- current PKE also recognizes character metadata such as `player1`, `gfVersion`, and Psych Change Character events

The native parser must normalize loaded notes into an internal representation such as:

```cpp
struct Note {
    double timeMs;
    int lane;               // 0..3
    double sustainMs;
    Side side;              // Player or Opponent
    std::string noteType;
    bool altAnim;
    bool gfNote;
};
```

Phase 1 compatibility requirements:

1. Standard section chart loading
2. `mustHitSection` side mapping
3. noteData side inversion and 4-lane mapping
4. sustain length
5. base BPM and BPM-change timeline
6. chart scroll speed
7. safe handling of optional `noteType`
8. exact note ordering by song time

The parser should preserve metadata even when a Phase 1 subsystem does not yet use it.

## 6. Current PKE judgement behavior to preserve

V1.14 source uses the following default hit windows:

- SICK!: ±45 ms
- GOOD: ±90 ms
- BAD: ±135 ms
- MISS deadline: ±180 ms

The current score values are:

- SICK!: 350
- GOOD: 200
- BAD: 100
- MISS: 0

Current head-note health gain is:

- SICK!: +0.040
- GOOD: +0.025
- BAD: +0.010

The current PKE source path does **not** use a separate SHIT judgement in the normal player judgement function. Native Phase 1 should therefore match V1.14 as SICK!/GOOD/BAD/MISS rather than introduce a new rating.

All judgement constants must live in one configuration structure and be covered by golden tests so later changes are deliberate.

## 7. Sustain behavior

Native Phase 1 must support:

- sustain head judgement
- sustain body progression
- hold start grace and hold release grace compatible with V1.14 behavior
- correct visual body/end rendering
- no frame-rate dependency in sustain judgement
- directional singing pose held for the expected sustain duration

V1.14 currently exposes hold grace constants of 60 ms for hold start and 90 ms for hold release. These should be the initial native defaults, subject to golden-test confirmation against the JS engine.

## 8. Character compatibility

V1.14 supports Psych-style character JSON plus Sparrow XML atlas data.

### Character JSON fields required in Phase 1

- `image`
- `animations[]`
- `animations[].anim` — semantic animation name, e.g. `idle`, `singLEFT`
- `animations[].name` — Sparrow atlas prefix
- `animations[].indices` — optional explicit Psych indices
- `animations[].fps`
- `animations[].loop`
- `animations[].offsets`
- `scale`
- `position`
- `flip_x`
- `no_antialiasing`
- `sing_duration`

Other fields should be parsed or retained where practical but may be unused in Phase 1.

### Sparrow XML fields required

Each `SubTexture` must support:

- `name`
- `x`, `y`
- `width`, `height`
- `frameX`, `frameY`
- `frameWidth`, `frameHeight`
- `rotated`

Frames are matched to character animations either by explicit `indices` or by atlas prefix followed by numeric ordering, matching the current PKE approach.

### Atlas drawing

The renderer must support trimmed Sparrow frames and rotated atlas entries without creating new textures every frame. Frame placement must account for authored offsets and frame trim metadata.

## 9. Audio design

Audio timing is the most important native subsystem.

### Loading

For Phase 1, Inst.ogg and Voices.ogg are decoded fully before gameplay begins. This intentionally trades memory for deterministic playback and removes disk/decode work from active gameplay.

Typical flow:

1. load Inst.ogg
2. decode to PCM
3. load Voices.ogg if present
4. decode to PCM
5. convert both to one common output format/sample rate if needed
6. start both from the same output-frame boundary

### Output

- miniaudio owns the WASAPI device
- Inst and Voices are mixed in the same audio callback
- no file I/O, heap allocation, JSON parsing, locks, or logging is permitted in the realtime audio callback

### Master song clock

Gameplay time must derive from audio output progression, not render FPS and not `SDL_GetTicks()` alone.

Conceptually:

```text
renderedAudioFrames
        ↓
renderedAudioFrames / sampleRate
        ↓
songTimeSeconds
        ↓
notes / animation / BPM timeline / HUD
```

Pause and seek adjust an explicit song-frame cursor. Inst and Voices always share the same cursor.

The engine may apply a configurable audio/output latency compensation value, but the underlying source of truth remains the audio frame position.

## 10. Input timing

- SDL keyboard events are processed on the main thread
- default Phase 1 bindings include arrow keys
- bindings are configurable in `pke-native.json`
- judgement time is sampled from `AudioEngine::SongTimeMs()` when the input event is consumed
- input and render FPS are not allowed to alter chart timing

A user input offset can later be exposed as calibration, but the initial prototype should default to 0 ms and log raw timing offsets for comparison with current PKE.

## 11. Main loop

The main thread owns SDL events, gameplay update, animation state, and rendering.

Pseudo-flow:

```text
while running:
    poll SDL events
    songTime = AudioEngine.songTime()
    RhythmEngine.processMisses(songTime)
    CharacterAnimator.update(songTime)
    Renderer.draw(songTime)
    present
```

No gameplay system may advance by “one frame.” Everything that matters to rhythm state uses absolute song time.

Rendering may run at a different rate from audio/judgement. Phase 1 should expose:

- optional VSync
- configurable FPS cap
- default cap: 240 FPS when VSync is off

Dropping a rendered frame must not change note judgement or song progression.

## 12. Core components

### `App`

Owns startup, shutdown, configuration, and top-level state transitions.

### `AssetLocator`

Resolves existing Songs and Characters roots and finds files by current PKE conventions.

### `ChartParser`

Converts existing JSON into normalized notes, BPM timeline, speed timeline, and chart metadata.

### `CharacterLoader`

Loads character JSON, Sparrow XML, texture atlas, animation definitions, offsets, scale, and flip metadata.

### `AudioEngine`

Owns decoded Inst/Voices PCM, WASAPI device, song-frame cursor, pause/start/stop, and master timing.

### `RhythmEngine`

Owns hit selection, judgement, sustain state, combo, health, raw score, accuracy counters, and misses.

### `CharacterAnimator`

Maps note hits to semantic animations and determines the frame from absolute song time.

### `Renderer`

Draws background, characters, receptors, notes, sustains, HP, judgement text, score, accuracy, and debug timing.

### `Diagnostics`

Captures frame intervals, update/render cost, audio callback underruns, input offsets, and song-clock drift. Diagnostics must use preallocated/ring-buffer storage during gameplay.

## 13. Rendering target for Phase 1

The prototype does not need current PKE menu parity. It needs a faithful gameplay screen with:

- 1920x1080 logical coordinate space
- scalable window/fullscreen output
- four opponent receptors
- four player receptors
- upscroll support first
- note heads and sustain bodies
- BF and DAD Sparrow animations
- health bar
- song name
- combo/judgement
- raw score and accuracy
- FPS and audio diagnostic overlay toggle

Asset dimensions and positions must not be hardwired to one character.

## 14. Song selection for the first playable prototype

The first prototype should support launching a song folder from a simple file/folder selection screen or command-line/config entry.

The first compatibility milestone is not “all 123 songs.” It is:

1. load one known existing PKE song folder unchanged
2. load its chart unchanged
3. load existing BF/DAD assets unchanged
4. play Inst + Voices in sync from start to finish
5. complete the chart with correct judgements and sustains
6. return to a simple song-selection screen without hanging

After that, compatibility is expanded by fixtures rather than by rewriting the engine for each song.

## 15. Error handling

Native errors must fail visibly and recoverably.

Examples:

- missing Inst.ogg: refuse to start song and display exact path
- missing Voices.ogg: allow song if Voices is optional
- malformed chart JSON: display parser error and return to song selection
- missing character JSON/XML/image: report the missing asset and use a diagnostic fallback character if available
- missing animation prefix: warn once and fall back to idle where safe
- unsupported event/note type: log it, preserve metadata, and continue if core gameplay remains valid
- audio device loss: stop gameplay cleanly rather than continuing with a fake wall clock

No exception should leave the program in a frozen gameplay state.

## 16. Performance rules

Phase 1 is specifically intended to avoid the class of stalls seen in the WebView2 build.

During active gameplay:

- no large JSON serialization
- no population/ranking work
- no asset discovery scans
- no texture creation/destruction per frame
- no heap allocation in the audio callback
- avoid per-frame heap allocation in gameplay/render paths
- no synchronous disk writes
- all song/character assets are prepared before countdown

The diagnostic overlay should make frame-time and audio-underrun problems visible immediately.

## 17. Testing strategy

### Unit tests

- chart side/lane mapping
- sustain parsing
- BPM timeline conversion
- judgement boundaries at exactly 45/90/135/180 ms
- score/health changes
- character JSON animation mapping
- Psych `indices` ordering
- Sparrow frame trim placement math

### Golden compatibility tests

For selected existing charts, run both V1.14 parsing logic and Native parsing and compare normalized output:

- note count
- each note's time/lane/side/sustain length
- BPM timeline
- scroll speed
- player/opponent counts

The uploaded `ejected-hard.json` is suitable as one initial fixture because it includes section-style notes, sustains, side changes, and BPM-related metadata.

### Audio tests

- Inst/Voices start on the same output frame
- pause/resume keeps both streams on one cursor
- long playback does not accumulate relative Inst/Voices drift
- song clock remains monotonic while playing
- no allocations in audio callback instrumentation

### Manual Windows verification

- Windows 10/11 x64 launch
- keyboard latency sanity check
- fullscreen/windowed transition
- 165/240-FPS rendering while judgement remains stable
- one complete existing PKE song
- repeated song restarts
- unplug/change audio device behavior

## 18. Migration phases after the prototype

Only after Phase 1 timing is proven:

### Phase 2 — gameplay parity

- more note/event compatibility
- GF
- chart Change Character events
- current PKE UI modes/scroll options
- practice/freeplay settings
- character transform profiles

### Phase 3 — persistence and replay

- native settings
- score/history storage
- replay format compatibility or migration layer

### Phase 4 — multiplayer simulation

- Ranked
- matchmaking
- LIVE/background population system
- spectator

Heavy population work must run outside the render/audio thread from the beginning.

### Phase 5 — Boss and seasonal systems

- Boss Battle
- 3-player party rendering
- Halloween Boss
- Candy/shop/titles

## 19. Acceptance criteria for Native Prototype 0.1

Prototype 0.1 is accepted when all of the following are true:

1. It is a Windows x64 native executable with no WebView2/Chromium runtime dependency.
2. It loads an existing PKE song folder without converting its chart/audio.
3. It loads existing Psych-style BF/DAD character JSON + Sparrow XML + PNG/WebP.
4. Inst and Voices start together and stay synchronized for the whole song.
5. The audio frame cursor is the master song clock.
6. Arrow-key input produces V1.14-compatible SICK!/GOOD/BAD/MISS results at the tested boundaries.
7. Sustains can be started, held, released, missed, and rendered without frame-rate dependence.
8. BF/DAD change singing animations from note events and return to idle correctly.
9. Score, accuracy, combo, health, and song completion work.
10. The song returns to selection without a softlock.
11. A diagnostic run shows no periodic browser/GC-style stall source because no browser runtime exists.
12. Automated parser/judgement/animation tests pass and at least one real existing PKE song completes on Windows.

## 20. Key design decision

The project deliberately prioritizes **existing PKE asset compatibility over redesigning the content format**.

If a native subsystem and the current asset format disagree, the first choice is to add a compatibility adapter. We only introduce a new asset format later if an existing format makes correct native behavior impossible or unreasonably expensive.
