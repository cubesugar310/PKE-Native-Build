# Block 1 compatibility calculations

Source authority is the supplied V1.14 extracted source and approved Block 1 plan.
The first imported calculations came from the embedded V1.12 executable, ZIP SHA-256
`29294463b238e10815c017ac2496567de24b91f4f1d6326f770d81f1125fac3c`.
Their V1.14 comparison is recorded below; subsequent character imports use
the actual supplied V1.14 files directly.
`provenance.json` records the extracted app hash, original declaration spans and
hashes, complete helper-file hashes, and the adapted boundaries.

Load order:

1. `cpu-timing.js`
2. `title-system.js`
3. `population-social-sessions.js`
4. `boss-battle.js`
5. `song-difficulty.js`
6. `population-imports.js`
7. `population.js`

The separate catalog loads `chart-utils.js`, `catalog-imports.js` and `catalog.js`.
`practice-replay.js` and `character-presentation.js` are explicit cold-path
contexts rather than population-context modules. All twelve are packaged.

The first four files are byte-identical reference modules. Difficulty retains
the original authored-head generator and range helpers; a synchronous generator
drain replaces only its browser-yield scheduler. `population-imports.js` keeps
the selected original profile, matchmaking, score, development, title, skin mood,
rest/sleep, social-session, LIVE note and spectator calculations. Each imported
declaration names its source lines. The full browser app is never evaluated.

## Native boundaries

`practice-replay.js` is loaded only for explicit completed-run export. Its ten original pure replay/hash/result declarations are recorded in `replay-provenance.json`; Native supplies the cached chart, audio-clock input events and final Practice statistics. It uses the original `PKE_REPLAY` schema and does not update ranked progress. Native owns the exported file and durable write.

| Reference responsibility | Native adaptation |
| --- | --- |
| Browser scheduling and secondary compute workers | The original in-context fallback runs synchronously on the dedicated Native worker. `async`/`await` tokens are removed; former browser yields do no work. Calculation and RNG order are retained. |
| Browser clock | An instance-local Date/stopwatch receives only the host's explicit UTC milliseconds. Normal five-second ticks retain their order. Coalesced requests and long gaps run once at the latest timestamp, retaining the original elapsed-bucket caps and recovery calculations. |
| Browser catalog discovery | Native supplies immutable JSON song metadata, authored chart arrays, skin IDs and bans. `chartFile.name`/`instFile` are plain metadata fields; no File objects or file handles enter JS. |
| localStorage, IndexedDB and save timers | Native owns durable files. Reference save/journal boundaries mark the current generation dirty. Explicit checkpoints contain records and the original bounded ten-per-player history, deduplicated by match ID. |
| Home labels and debug console output | Native reads a compact summary and owns presentation/diagnostics. JS errors still reach `QuickJsRuntime` diagnostics. |
| Player name and skin DOM controls | Native supplies the normalized name and selected skin. No DOM controls are modeled. |
| Browser language collation | `QuickJsRuntime::InstallLocaleCompare` supplies ICU default collation and the catalog's explicit numeric/base comparison. Default case-sensitive weekly selection stays unchanged. Fixtures pin `en-US`; the worker can select the Native locale. |

Original ONLINE/demand/cap calculations are preserved. ONLINE is the reference's
time-bucket/hash calculation, rather than a new count of presence flags.
REGISTERED is **14,999 CPUs + YOU = 15,000**. New registrations are IDs
10001–14999. The local player's Online record is never selected for CPU matches.
The Halloween TEAM target is explicitly checked at 255,000, with the approved
Oct 15 00:00 JST / Nov 1 00:00 JST boundaries and Native Auto/Force overrides.

## JSON surface

- `pkePopulationInitialize({now,songs,skins,bannedSkins,playerName,playerSkin})`
- `pkePopulationAdvanceTo({now})` — never rewinds; normal ticks keep their
  cadence and a coalesced gap invokes the original recovery tick once.
- `pkePopulationSummary({})` — scalar Home snapshot, not the CPU graph.
- `pkePopulationQuery({ids})` — at most 128 explicit IDs.
- `pkePopulationSerialize({})` / `pkePopulationRestore(checkpoint)` — explicit
  checkpoint operations, never per-frame work.
- `pkePopulationRestoreWithOptions({options,checkpoint})` — fresh application
  load with the Native catalog, without first simulating a discarded world.
- `pkeHalloweenState({now,override})`
- `pkeSongDifficulty({chart,scrollSpeed})`
- `pkeTitleAvailability({earned,stats})`

Production uses the engine's original `Math.random` call sites. Seeded randomness
exists only in the fixture harness. Restoring retains records, simulation time,
sessions and bounded history; the reference does not persist a future RNG stream.
Restore is a fresh-world load and retains the reference's new-app 15-second
startup recovery. It is not an in-process rollback promising identical future
note budgets. The worker must distinguish fresh load/recovery from uninterrupted
advancement; it must not silently rewind an active generation.

## Reference verification

Frozen original async calculations and the original async difficulty module live
under `tests/reference/`; they are excluded from product packages. Fixtures were
captured with Node using a fixed UTC clock and test-only seeded randomness.
The synchronous QuickJS port is compared against those fixtures for startup,
one minute, selected profiles, REST/SLEEP transitions, every serialized CPU record
via a deterministic fingerprint, history count and exact RNG call order. The
fingerprint is a test checksum, not a security hash. Additional fixtures cover
authored-note duplicates, irregular heads, chords, bursts, jacks and titles.

The reference harness uses the Native checkpoint container and catalog/file
boundary; it does not emulate a browser. These are component tests. Home/gameplay
integration, persistence hardening, visual parity, performance gates and Windows
device acceptance remain later Block 1 tasks.

To recapture the frozen reference fixtures with Node:

```bash
node native/tests/reference/capture_population_fixture.cjs
node native/tests/reference/capture_helper_fixtures.cjs
node native/tests/reference/capture_catalog_fixture.cjs
node native/tests/reference/capture_recovery_fixture.cjs
node native/tests/reference/capture_asset_fixtures.cjs
```

## Dedicated Native worker

`PopulationRuntime` constructs, uses and destroys its QuickJS backend on one
worker thread. `AdvanceTo()` submits an atomic maximum timestamp; it allocates no
per-frame jobs and performs no VM or file work. The worker respects the
five-second normal cadence and drains one latest coalesced target without rewinding.
Long gaps retain the original recovery caps instead of replaying hours of jobs;
explicit checkpoints can capture a partial interval before the next normal tick.
An atomic revision counter prevents missed wakeups without taking a render-thread
mutex. Only compact immutable counters/time are published to the renderer.

Explicit checkpoint futures capture after their fixed submitted target, so newer
render updates cannot postpone them indefinitely. A backend error retains the
last good summary, marks that generation failed, and resolves/refuses checkpoint
requests with the error. Shutdown cancels active VM execution, clears the borrowed
cancellation pointer, and destroys the context and ICU/COM state on the worker.
Callers must capture/save their final checkpoint before stopping the worker.

Diagnostics record initialization time and each completed step's last/max elapsed
time. These component timings do not replace the later full-game performance gate.

## Cached Native asset catalog

`AssetService` explicitly scans Songs and Characters on refresh. File reads,
chart analysis, candidate fallback and character/atlas parsing happen there.
It publishes immutable metadata/definitions; a retained old snapshot remains
valid after a refresh. Corrupt files produce item-specific diagnostics and do
not discard other playable songs or characters.

The catalog context loads `chart-utils.js` (byte-identical), the existing
`song-difficulty.js`, `catalog-imports.js`, then `catalog.js`. The separate
`catalog-provenance.json` records each original pure helper's source/hash.
The original hard-chart priority, natural/base sorting, audio extension list,
display name/opponent/icon rules, authored-note filtering and star calculations
are retained. Additional valid chart candidates are cached as difficulty
metadata without changing the original default selection.

Search caches normalized song/folder/chart names and side-specific stars.
Queries use the original NFKC, Japanese case conversion, whitespace/underscore/
hyphen splitting and all-terms matching, with no file reads or chart parsing.
Frozen original functions and async-difficulty fixtures independently verify
section/absolute ownership, harmless versus damage-only heads, rapid boundaries,
sustains, speed/BPM values, width/kana/Greek/accent search and BF/DAD filters.
# V1.14 authority check

The supplied V1.14 extracted-source archive was inspected directly (SHA-256
`7524ef2060505615167d90003617a58d83f0447f3174cbc14b11915644e9bbaa`).
`v114-reference-verification.json` records the comparison: index/styles and
ten helper modules are byte-identical; the 196 named app functions referenced
by these modules have no V1.12-to-V1.14 source revision differences. Native
boundary adaptations are recorded separately. Three changed functions and two
new helpers concern the browser Online-result lifecycle; none is an imported
Native calculation. Existing V1.12 extraction provenance remains intact.
Native event state/shop transactions and gameplay preferences use the V1.14
rules directly. The original browser host is not included in this Native build.

## Original character layout and geometry

`character-provenance.json` records 26 verbatim V1.14 declarations for median
anchors, trim stabilization, Psych frame placement, saved-layout keys, shared
transforms, identity-backed alias migration and pose maps. `character-presentation.js` resolves settings only
when a Practice session loads. It operates on a private copy; `SaveService`
preserves the complete imported layout JSON and identity history and owns all durable writes.
Native geometry caches reproduce the original frame/anchor calculations.
BF's full XML-name-group trim pass precedes JSON indices; Dad retains its
separate selected-animation trim pass. Matching shared keys synchronize scale
after opponent profile loading, preserving the original role-local positions.
Rendering uses those cached values without JavaScript calls or file reads.

The 60 frozen placement cases exercise both roles, Practice sides, rotations,
scale bounds, authored flip, override, trim and pose offsets. With the extracted
source available, reproduce their reference check and all declaration hashes:

```sh
node native/scripts/verify-character-reference.cjs /path/to/v1.14/web
```

The checker evaluates original declarations extracted from the supplied source,
independently of the Native implementation. Software-SDL tests additionally
check actual rotated/mirrored pixels and saved-transform session wiring.
Broader original animation timing, note presentation and scene acceptance
remain separate unfinished tasks.
