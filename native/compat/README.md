# Block 1 compatibility calculations

Source authority is the approved Block 1 plan. These calculations come from the
embedded V1.12 executable, ZIP SHA-256
`29294463b238e10815c017ac2496567de24b91f4f1d6326f770d81f1125fac3c`.
They are not presented as an inspection of the unavailable V1.14 executable.
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

The first four files are byte-identical reference modules. Difficulty retains
the original authored-head generator and range helpers; a synchronous generator
drain replaces only its browser-yield scheduler. `population-imports.js` keeps
the selected original profile, matchmaking, score, development, title, skin mood,
rest/sleep, social-session, LIVE note and spectator calculations. Each imported
declaration names its source lines. The full browser app is never evaluated.

## Native boundaries

| Reference responsibility | Native adaptation |
| --- | --- |
| Browser scheduling and secondary compute workers | The original in-context fallback runs synchronously on the dedicated Native worker. `async`/`await` tokens are removed; former browser yields do no work. Calculation and RNG order are retained. |
| Browser clock | An instance-local Date/stopwatch receives only the host's explicit UTC milliseconds. Five-second reference buckets are advanced in order, including missed buckets. |
| Browser catalog discovery | Native supplies immutable JSON song metadata, authored chart arrays, skin IDs and bans. `chartFile.name`/`instFile` are plain metadata fields; no File objects or file handles enter JS. |
| localStorage, IndexedDB and save timers | Native owns durable files. Reference save/journal boundaries mark the current generation dirty. Explicit checkpoints contain records and the original bounded ten-per-player history, deduplicated by match ID. |
| Home labels and debug console output | Native reads a compact summary and owns presentation/diagnostics. JS errors still reach `QuickJsRuntime` diagnostics. |
| Player name and skin DOM controls | Native supplies the normalized name and selected skin. No DOM controls are modeled. |
| Browser language collation | `QuickJsRuntime::InstallLocaleCompare` supplies ICU collation for the original one-argument string comparisons. This preserves case-sensitive weekly song selection without changing the reference module. Fixtures pin `en-US`; the worker can select the Native locale. |

Original ONLINE/demand/cap calculations are preserved. ONLINE is the reference's
time-bucket/hash calculation, rather than a new count of presence flags.
REGISTERED is **14,999 CPUs + YOU = 15,000**. New registrations are IDs
10001–14999. The local player's Online record is never selected for CPU matches.
The Halloween TEAM target is explicitly checked at 255,000, with the approved
Oct 15 00:00 JST / Nov 1 00:00 JST boundaries and Native Auto/Force overrides.

## JSON surface

- `pkePopulationInitialize({now,songs,skins,bannedSkins,playerName,playerSkin})`
- `pkePopulationAdvanceTo({now})` — never rewinds; the Native worker must split
  long catch-up into bounded calls.
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
```

## Dedicated Native worker

`PopulationRuntime` constructs, uses and destroys its QuickJS backend on one
worker thread. `AdvanceTo()` submits an atomic maximum timestamp; it allocates no
per-frame jobs and performs no VM or file work. The worker advances every elapsed
five-second reference bucket and handles a final partial bucket without rewinding.
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
