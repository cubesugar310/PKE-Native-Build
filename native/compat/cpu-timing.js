/* PKE Beat Battle v0.2.35 - cpu-timing */
(() => {
  "use strict";

  function createCpuTiming(dependencies) {
    const {
      hitWindows,
      replayHashString,
      beatBattleHumanTimingProfileStyleCache,
      BEAT_BATTLE_HUMAN_TIMING_VERSION,
      BEAT_BATTLE_POPULATION_TARGET_ACC_CAP,
      BEAT_BATTLE_HUMAN_TIMING_SIGMA_TABLE,
      BEAT_BATTLE_HUMAN_TIMING_DRIFT_BUCKET_MS,
      BEAT_BATTLE_HUMAN_TIMING_PHRASE_BUCKET_MS,
      BEAT_BATTLE_HUMAN_TIMING_FOCUS_BUCKET_MS,
      BEAT_BATTLE_HUMAN_TIMING_SQRT6,
      BEAT_BATTLE_HUMAN_TIMING_PROFILE_BIAS_WEIGHT,
      BEAT_BATTLE_HUMAN_TIMING_JITTER_WEIGHT,
      BEAT_BATTLE_HUMAN_DUPLICATE_EPSILON_MS,
      BEAT_BATTLE_FULL_SICK_AFFINITY_MIN,
      BEAT_BATTLE_FULL_SICK_CONDITION_MIN,
      BEAT_BATTLE_RAPID_TRAIT_MAX_ACC_DELTA,
      BEAT_BATTLE_TEMPO_TRAIT_MAX_ACC_DELTA,
    } = dependencies;

  function getBeatBattleHumanTimingSigmaMs(targetAccuracyPercent) {
    const target = Math.max(
      60,
      Math.min(
        BEAT_BATTLE_POPULATION_TARGET_ACC_CAP,
        Number(targetAccuracyPercent) || 60
      )
    );
    const table = BEAT_BATTLE_HUMAN_TIMING_SIGMA_TABLE;
    for (let i = 0; i < table.length - 1; i++) {
      const high = table[i];
      const low = table[i + 1];
      if (target <= high[0] && target >= low[0]) {
        const span = Math.max(0.000001, high[0] - low[0]);
        const t = (high[0] - target) / span;
        return high[1] + (low[1] - high[1]) * t;
      }
    }
    return table[table.length - 1][1];
  }

  function getBeatBattleHumanTimingScatterScale(targetAccuracyPercent) {
    const target = Number(targetAccuracyPercent) || 60;
    if (target >= 99.5) return 0.94;
    if (target >= 99.0) return 0.91 + (target - 99.0) * 0.06;
    return Math.min(
      0.99,
      0.91 + Math.max(0, 99.0 - target) * 0.0055
    );
  }

  function getBeatBattleHumanTimingProfileStyle(profile, styleRandomUnit) {
    const id = String(profile && profile.id || "");
    if (id && beatBattleHumanTimingProfileStyleCache.has(id)) {
      return beatBattleHumanTimingProfileStyleCache.get(id);
    }

    const readUnit = key => {
      if (id) {
        return getBeatBattlePopulationHashUnit(
          "human-timing-v1|" + id + "|" + String(key || "style")
        );
      }
      return typeof styleRandomUnit === "function"
        ? styleRandomUnit("profile|" + String(key || "style"))
        : 0.5;
    };

    const listedAccuracy = Math.max(
      60,
      Math.min(100, Number(profile && profile.accuracy) || 93)
    );
    const laneBiasRangeMs = Math.max(
      1.5,
      Math.min(5.5, 1.6 + (100 - listedAccuracy) * 0.12)
    );
    const style = {
      stability: 0.68 + readUnit("stability") * 0.30,
      laneBiasMs: [0, 1, 2, 3].map(lane =>
        (readUnit("lane-bias-" + lane) * 2 - 1) * laneBiasRangeMs
      )
    };

    if (id) beatBattleHumanTimingProfileStyleCache.set(id, style);
    return style;
  }

  function getBeatBattleHumanTimingRating(timingOffsetMs) {
    const abs = Math.abs(Number(timingOffsetMs) || 0);
    const sick = Math.max(0, Number(hitWindows.sick) || 45);
    const good = Math.max(sick, Number(hitWindows.good) || 90);
    const miss = Math.max(good, Number(hitWindows.miss) || 166.6667);
    if (abs <= sick) return "SICK";
    if (abs <= good) return "GOOD";
    if (abs <= miss) return "BAD";
    return "MISS";
  }

  function normalizeBeatBattleHumanTimingVersion(value, fallback = BEAT_BATTLE_HUMAN_TIMING_VERSION) {
    const version = Number(value);
    if (value !== null && value !== "" && (version === 1 || version === 2 || version === 3)) {
      return version;
    }
    return fallback === 2 || fallback === 3 ? fallback : 1;
  }

  function getBeatBattleLiveHumanTimingVersion(match) {
    return normalizeBeatBattleHumanTimingVersion(
      match && (match.humanTimingVersion ?? (match.noteSim && match.noteSim.humanTimingVersion)),
      1
    );
  }

  function getBeatBattleHumanTimingJitterUnit(rawUnit, timingVersion) {
    const unit = Math.max(0, Math.min(1, Number(rawUnit) || 0));
    if (normalizeBeatBattleHumanTimingVersion(timingVersion, 1) === 1) {
      return unit;
    }

    // FNV-1a keys differing only in their final a/b salt are strongly correlated.
    // Avalanche the existing 32-bit roll before forming the triangular jitter.
    // Keep profile/name/skin hashes, style drift, and lapse rolls untouched.
    let hash = Math.round(unit * 0xffffffff) >>> 0;
    hash ^= hash >>> 16;
    hash = Math.imul(hash, 0x85ebca6b);
    hash ^= hash >>> 13;
    hash = Math.imul(hash, 0xc2b2ae35);
    hash ^= hash >>> 16;
    return (hash >>> 0) / 0xffffffff;
  }

  function getBeatBattleHumanTimingOutcome(options = {}) {
    const profile = options.profile || null;
    const noteRandomUnit = typeof options.noteRandomUnit === "function"
      ? options.noteRandomUnit
      : (() => 0.5);
    const styleRandomUnit = typeof options.styleRandomUnit === "function"
      ? options.styleRandomUnit
      : noteRandomUnit;
    const timingVersion = normalizeBeatBattleHumanTimingVersion(options.humanTimingVersion);
    const condition = timingVersion >= 3 ? getBeatBattleHumanRunCondition(styleRandomUnit) : 0.5;
    const affinity = Math.max(-1, Math.min(1, Number(profile && profile.rapidAffinity) || 0));
    const density = Math.max(0, Math.min(4, Number(options.density) || 0));
    const strain = Math.max(0, Math.min(4, Number(options.strain) || 0));
    // Aptitude reduces the cost of density; it cannot turn faster streams into
    // an unconditional accuracy bonus. Sustained streams accumulate extra load.
    const densityPenalty = timingVersion >= 3
      ? (density * 2.2 + strain * 1.1) * (1 - affinity * 0.45)
      : 0;
    const targetAccuracyPercent = Math.max(
      60,
      Math.min(
        BEAT_BATTLE_POPULATION_TARGET_ACC_CAP,
        (Number(options.targetAccuracyPercent) || 60) - densityPenalty
      )
    );
    const inputNoteTime = Number(options.noteTime) || 0;
    // Keep historical drift anchors while retaining authored negative times
    // for input recovery. Distinct pre-song heads must not become one chord.
    const noteTime = Math.max(0, inputNoteTime);
    const lane = Math.max(
      0,
      Math.min(3, Math.floor(Number(options.lane) || 0))
    );
    const style = getBeatBattleHumanTimingProfileStyle(
      profile,
      styleRandomUnit
    );
    const stability = Math.max(
      0.5,
      Math.min(1, Number(style.stability) || 0.85)
    );

    let sigmaMs = getBeatBattleHumanTimingSigmaMs(
      targetAccuracyPercent
    );
    sigmaMs *= getBeatBattleHumanTimingScatterScale(
      targetAccuracyPercent
    );

    // A run has a tiny overall EARLY/LATE condition in addition to the player's
    // persistent profile bias. This stays stable for the entire match.
    const runShiftRangeMs = Math.min(4.5, sigmaMs * 0.14);
    const runShiftMs =
      (styleRandomUnit("run-shift") * 2 - 1) *
      runShiftRangeMs;
    const runSpread = timingVersion >= 3
      ? 1.08 - condition * 0.16
      : 0.94 + styleRandomUnit("run-spread") * 0.12;

    // Human timing tends to drift for a few seconds at a time. Interpolating
    // deterministic anchors makes adjacent notes share the same tendency.
    const driftBucketMs = BEAT_BATTLE_HUMAN_TIMING_DRIFT_BUCKET_MS;
    const driftBucket = Math.floor(noteTime / driftBucketMs);
    const driftPhaseRaw =
      (noteTime - driftBucket * driftBucketMs) /
      driftBucketMs;
    const driftPhase =
      driftPhaseRaw * driftPhaseRaw * (3 - 2 * driftPhaseRaw);
    const driftA =
      styleRandomUnit("drift|" + driftBucket) * 2 - 1;
    const driftB =
      styleRandomUnit("drift|" + (driftBucket + 1)) * 2 - 1;
    const driftUnit =
      driftA + (driftB - driftA) * driftPhase;
    const driftAmplitudeMs =
      sigmaMs * (0.24 + (1 - stability) * 0.28);
    const driftMs = driftUnit * driftAmplitudeMs;

    // A shorter phrase-scale wave sits on top of the slow drift. Human timing
    // often moves in little runs (a few notes slightly early, then a few late)
    // rather than independent left/right noise on every single note.
    const phraseBucketMs = BEAT_BATTLE_HUMAN_TIMING_PHRASE_BUCKET_MS;
    const phraseBucket = Math.floor(noteTime / phraseBucketMs);
    const phrasePhaseRaw =
      (noteTime - phraseBucket * phraseBucketMs) /
      phraseBucketMs;
    const phrasePhase =
      phrasePhaseRaw * phrasePhaseRaw * (3 - 2 * phrasePhaseRaw);
    const phraseA =
      styleRandomUnit("phrase|" + phraseBucket) * 2 - 1;
    const phraseB =
      styleRandomUnit("phrase|" + (phraseBucket + 1)) * 2 - 1;
    const phraseUnit =
      phraseA + (phraseB - phraseA) * phrasePhase;
    const phraseAmplitudeMs =
      sigmaMs * (0.10 + (1 - stability) * 0.13);
    const phraseDriftMs =
      phraseUnit * phraseAmplitudeMs;

    // Focus changes more slowly than timing drift. Bad focus widens the cloud
    // for a short stretch instead of making one isolated note randomly awful.
    const focusBucketMs = BEAT_BATTLE_HUMAN_TIMING_FOCUS_BUCKET_MS;
    const focusBucket = Math.floor(noteTime / focusBucketMs);
    const focusPhaseRaw =
      (noteTime - focusBucket * focusBucketMs) /
      focusBucketMs;
    const focusPhase =
      focusPhaseRaw * focusPhaseRaw * (3 - 2 * focusPhaseRaw);
    const focusA =
      styleRandomUnit("focus|" + focusBucket) * 2 - 1;
    const focusB =
      styleRandomUnit("focus|" + (focusBucket + 1)) * 2 - 1;
    const focusUnit =
      focusA + (focusB - focusA) * focusPhase;
    const focusAmplitude =
      0.07 + (1 - stability) * 0.16;
    const focusScale = Math.max(
      0.78,
      Math.min(1.25, 1 + focusUnit * focusAmplitude)
    );

    // Two independent uniform rolls form a centered triangular distribution.
    // It keeps ordinary presses clustered near the player's current timing,
    // while rare lapses below provide the long human tail / occasional MISS.
    const jitterZ = (
      getBeatBattleHumanTimingJitterUnit(noteRandomUnit("human-jitter-a"), timingVersion) +
      getBeatBattleHumanTimingJitterUnit(noteRandomUnit("human-jitter-b"), timingVersion) -
      1
    ) * BEAT_BATTLE_HUMAN_TIMING_SQRT6;

    const persistentBiasMs = Math.max(
      -20,
      Math.min(20, Number(profile && profile.timingBiasMs) || 0)
    );
    const softenedPersistentBiasMs =
      persistentBiasMs * BEAT_BATTLE_HUMAN_TIMING_PROFILE_BIAS_WEIGHT;
    const laneBiasMs =
      Number(style.laneBiasMs && style.laneBiasMs[lane]) || 0;

    let timingOffsetMs =
      softenedPersistentBiasMs +
      laneBiasMs +
      runShiftMs +
      driftMs +
      phraseDriftMs +
      sigmaMs *
        BEAT_BATTLE_HUMAN_TIMING_JITTER_WEIGHT *
        runSpread *
        focusScale *
        jitterZ;

    // A tiny lapse probability creates believable dropped notes/chokes even for
    // strong players. Poor-focus stretches raise it slightly, producing short
    // clusters instead of perfectly memoryless misses.
    const target01 = targetAccuracyPercent / 100;
    let lapseChance = Math.max(
      0.00002,
      Math.min(0.02, (1 - target01) * 0.05)
    );
    if (focusScale > 1) {
      lapseChance *= 1 + (focusScale - 1) * 1.8;
    }

    const forcedLapse =
      noteRandomUnit("human-lapse") < lapseChance;
    if (forcedLapse) {
      const missWindow = Math.max(
        Number(hitWindows.good) || 90,
        Number(hitWindows.miss) || 166.6667
      );
      const direction =
        noteRandomUnit("human-lapse-direction") < 0.5 ? -1 : 1;
      const overshoot =
        10 +
        noteRandomUnit("human-lapse-distance") *
          Math.min(120, 28 + sigmaMs * 1.7);
      timingOffsetMs =
        direction * (missWindow + overshoot);
    }

    let fullSickEligible = true;
    if (timingVersion >= 3) {
      fullSickEligible = isBeatBattleFullSickEligible(
        profile, options.chartBpm ?? options.bpm, options.rapidRatio, condition
      );
      const count = Math.max(1, Math.floor(Number(options.noteCount) || 1));
      const slipIndex = Math.min(count - 1, Math.floor(
        getBeatBattleHumanTimingJitterUnit(styleRandomUnit("full-sick-slip"), 3) * count
      ));
      const state = options.inputState;
      if (state) {
        if (!Array.isArray(state.lastPlans)) state.lastPlans = [null, null, null, null];
        const cached = state.lastPlans[lane];
        if (Array.isArray(cached) && Math.abs(inputNoteTime - Number(cached[0])) <= BEAT_BATTLE_HUMAN_DUPLICATE_EPSILON_MS) {
          // One stacked input plan, including a dropped input, judges the whole
          // duplicate group. Do not reroll its lapse or slide its time anchor.
          const sharedOffset = Number(cached[0]) + Number(cached[1]) - inputNoteTime;
          const sharedMiss = !!cached[2];
          return {
            rating: sharedMiss ? "MISS" : getBeatBattleHumanTimingRating(sharedOffset),
            willMiss: sharedMiss,
            timingOffsetMs: sharedOffset,
            sigmaMs, focusScale, driftMs, phraseDriftMs, condition, fullSickEligible
          };
        }
      }
      let minimumPressTime = -Infinity;
      let sharesPreviousPress = false;
      if (state && getBeatBattleHumanTimingRating(timingOffsetMs) !== "MISS") {
        if (!Array.isArray(state.lastPressTimes)) state.lastPressTimes = [null, null, null, null];
        if (!Array.isArray(state.lastNoteTimes)) state.lastNoteTimes = [null, null, null, null];
        const lastPress = state.lastPressTimes[lane];
        const lastNote = state.lastNoteTimes[lane];
        if (lastPress !== null && Number.isFinite(Number(lastPress))) {
          if (lastNote !== null && Math.abs(inputNoteTime - Number(lastNote)) <= BEAT_BATTLE_HUMAN_DUPLICATE_EPSILON_MS) {
            timingOffsetMs = Number(lastPress) - inputNoteTime;
            sharesPreviousPress = true;
          } else {
            const skill = Math.max(0, Math.min(1, ((Number(profile && profile.accuracy) || targetAccuracyPercent) - 80) / 20));
            const recoveryMs = (120 - skill * 25 - affinity * 15) *
              (1.04 - condition * 0.06) * (1 + strain * 0.12 * (1 - affinity * 0.35));
            minimumPressTime = Number(lastPress) + recoveryMs;
            timingOffsetMs = Math.max(inputNoteTime + timingOffsetMs, minimumPressTime) - inputNoteTime;
          }
        }
      }
      if (!fullSickEligible && (Math.floor(Number(options.noteOrdinal) || 0)) === slipIndex &&
          getBeatBattleHumanTimingRating(timingOffsetMs) === "SICK") {
        // Apply hesitation after recovery, using a press that respects both
        // constraints. Recovery must not pull an EARLY GOOD back into SICK.
        const distance = (Number(hitWindows.sick) || 45) + 3 +
          getBeatBattleHumanTimingJitterUnit(noteRandomUnit("full-sick-slip-distance"), 3) * 6;
        const direction = timingOffsetMs < 0 && inputNoteTime - distance >= minimumPressTime ? -1 : 1;
        timingOffsetMs = Math.max(direction * distance, minimumPressTime - inputNoteTime);
      }
      if (state && getBeatBattleHumanTimingRating(timingOffsetMs) !== "MISS") {
        state.lastPressTimes[lane] = inputNoteTime + timingOffsetMs;
        // Keep the first head as the duplicate anchor. Sliding the anchor on
        // each near-time duplicate would permit an unlimited sub-ms stream.
        if (!sharesPreviousPress) state.lastNoteTimes[lane] = inputNoteTime;
      }
    }
    const rating = getBeatBattleHumanTimingRating(timingOffsetMs);
    if (timingVersion >= 3 && options.inputState) {
      // Compact [authored time, offset, dropped input] per lane; persisted LIVE
      // can resume in the middle of a stack without rerolling its first head.
      options.inputState.lastPlans[lane] = [inputNoteTime, timingOffsetMs, rating === "MISS"];
    }
    return {
      rating,
      willMiss: rating === "MISS",
      timingOffsetMs,
      sigmaMs,
      focusScale,
      driftMs,
      phraseDriftMs,
      condition,
      fullSickEligible
    };
  }

  function getBeatBattleHumanRunCondition(styleRandomUnit) {
    return getBeatBattleHumanTimingJitterUnit(styleRandomUnit("run-condition"), 3);
  }

  function isBeatBattleFullSickEligible(profile, bpm, rapidRatio, condition) {
    if (!profile || !Number.isFinite(Number(condition)) || Number(condition) < BEAT_BATTLE_FULL_SICK_CONDITION_MIN) return false;
    const tempo = getBeatBattleTempoTraitFactors(Number(bpm) || 140);
    const strong = BEAT_BATTLE_FULL_SICK_AFFINITY_MIN;
    return (Number(rapidRatio) >= 0.25 && Number(profile.rapidAffinity) >= strong) ||
      (tempo.fast >= 0.5 && Number(profile.fastSongAffinity) >= strong) ||
      (tempo.slow >= 0.5 && Number(profile.slowSongAffinity) >= strong);
  }

  function getBeatBattlePopulationHashUnit(key) {
    return (
      (parseInt(replayHashString("population-v1|" + String(key)), 16) >>> 0) /
      0xffffffff
    );
  }

  function clampPopulationNumber(value, min, max) {
    return Math.max(min, Math.min(max, Number(value) || 0));
  }

  function getBeatBattleTempoTraitFactors(bpm) {
    const value = clampPopulationNumber(bpm, 40, 320);
    return {
      fast: clampPopulationNumber((value - 145) / 65, 0, 1),
      slow: clampPopulationNumber((135 - value) / 65, 0, 1)
    };
  }

  // v0.2.35: Run form is separate from the listed season accuracy. Every
  // population player has immutable consistency/upside/downside traits, while
  // a deterministic per-match roll decides whether this particular run is
  // ordinary, unusually hot, or unusually cold.
  function getBeatBattleProfilePerformanceTraits(profile) {
    if (!profile || profile.isHuman) {
      return { stability: 1, upside: 0, downside: 0 };
    }
    const id = String(profile.id || "");
    const readStored = (field, fallbackKey, fallback) => {
      const value = Number(profile[field]);
      if (profile[field] !== null && profile[field] !== undefined && profile[field] !== "" && Number.isFinite(value)) {
        return clampPopulationNumber(value, 0, 1);
      }
      if (!id) return fallback;
      return getBeatBattlePopulationHashUnit("performance-v1|" + id + "|" + fallbackKey);
    };
    return {
      stability: readStored("performanceStability", "stability", 0.75),
      upside: readStored("performanceUpside", "upside", 0.5),
      downside: readStored("performanceDownside", "downside", 0.5)
    };
  }

  function getBeatBattleProfileRunAccuracyDelta(profile, runRandomUnit) {
    if (!profile || profile.isHuman) return 0;
    const randomUnit = typeof runRandomUnit === "function" ? runRandomUnit : (() => 0.5);
    const traits = getBeatBattleProfilePerformanceTraits(profile);
    const instability = 1 - traits.stability;

    // Ordinary form is triangular and centered. Very stable players barely
    // move; volatile players can swing by well over one accuracy point even
    // before a rare hot/cold event.
    const centered = clampPopulationNumber(randomUnit("form-center-a"), 0, 1) +
      clampPopulationNumber(randomUnit("form-center-b"), 0, 1) - 1;
    const ordinaryRange = 0.16 + Math.pow(instability, 1.35) * 1.65;
    let delta = centered * ordinaryRange;

    // Hot streaks and cold slumps are independent profile tendencies. This
    // permits e.g. a rock-solid player who only rarely pops off, as well as a
    // chaotic player who can explode upward or collapse downward.
    const hotChance = Math.min(0.14, 0.012 + traits.upside * 0.055 + instability * 0.025);
    const coldChance = Math.min(0.14, 0.012 + traits.downside * 0.055 + instability * 0.025);
    const event = clampPopulationNumber(randomUnit("form-event"), 0, 1);
    if (event < hotChance) {
      const strength = clampPopulationNumber(randomUnit("form-hot-strength"), 0, 1);
      delta += 0.45 + traits.upside * 0.55 + instability * 0.45 +
        strength * (0.35 + traits.upside * 0.55);
    } else if (event > 1 - coldChance) {
      const strength = clampPopulationNumber(randomUnit("form-cold-strength"), 0, 1);
      delta -= 0.45 + traits.downside * 0.55 + instability * 0.45 +
        strength * (0.35 + traits.downside * 0.55);
    }

    return Math.max(-3.0, Math.min(3.0, delta));
  }

  function applyBeatBattleProfileRunAccuracy(targetAccuracy, profile, runRandomUnit, runFormVersion = 1) {
    const base = clampPopulationNumber(targetAccuracy, 60, BEAT_BATTLE_POPULATION_TARGET_ACC_CAP);
    if (Number(runFormVersion) < 1 || !profile || profile.isHuman) return base;
    return clampPopulationNumber(
      base + getBeatBattleProfileRunAccuracyDelta(profile, runRandomUnit),
      60,
      BEAT_BATTLE_POPULATION_TARGET_ACC_CAP
    );
  }

  function getBeatBattleProfileTargetAccuracy(profile, bpm, isRapid = false, humanTimingVersion = BEAT_BATTLE_HUMAN_TIMING_VERSION) {
    const base = clampPopulationNumber(profile && profile.accuracy, 0, 100);
    if (!profile || profile.isHuman) return base;

    const tempo = getBeatBattleTempoTraitFactors(bpm);
    let adjusted = base;
    adjusted += (Number(profile.fastSongAffinity) || 0) * tempo.fast * BEAT_BATTLE_TEMPO_TRAIT_MAX_ACC_DELTA;
    adjusted += (Number(profile.slowSongAffinity) || 0) * tempo.slow * BEAT_BATTLE_TEMPO_TRAIT_MAX_ACC_DELTA;
    if (isRapid && normalizeBeatBattleHumanTimingVersion(humanTimingVersion) < 3) {
      adjusted += (Number(profile.rapidAffinity) || 0) * BEAT_BATTLE_RAPID_TRAIT_MAX_ACC_DELTA;
    }

    return clampPopulationNumber(adjusted, 60, BEAT_BATTLE_POPULATION_TARGET_ACC_CAP);
  }

  function getBeatBattleProfileAverageTargetAccuracy(profile, bpm, rapidRatio = 0, humanTimingVersion = BEAT_BATTLE_HUMAN_TIMING_VERSION) {
    const base = clampPopulationNumber(profile && profile.accuracy, 0, 100);
    if (!profile || profile.isHuman) return base;
    const tempo = getBeatBattleTempoTraitFactors(bpm);
    let adjusted = base;
    adjusted += (Number(profile.fastSongAffinity) || 0) * tempo.fast * BEAT_BATTLE_TEMPO_TRAIT_MAX_ACC_DELTA;
    adjusted += (Number(profile.slowSongAffinity) || 0) * tempo.slow * BEAT_BATTLE_TEMPO_TRAIT_MAX_ACC_DELTA;
    if (normalizeBeatBattleHumanTimingVersion(humanTimingVersion) < 3) {
      adjusted += (Number(profile.rapidAffinity) || 0) * clampPopulationNumber(rapidRatio, 0, 1) * BEAT_BATTLE_RAPID_TRAIT_MAX_ACC_DELTA;
    } else {
      // Aggregate-only completions have no note stream. Approximate the rapid
      // burden; visible rooms use their actual density/recovery note by note.
      adjusted -= clampPopulationNumber(rapidRatio, 0, 1) * 0.75 *
        (1 - clampPopulationNumber(profile.rapidAffinity, -1, 1) * 0.45);
    }
    return clampPopulationNumber(adjusted, 60, BEAT_BATTLE_POPULATION_TARGET_ACC_CAP);
  }

    return {
      getBeatBattleHumanTimingSigmaMs,
      getBeatBattleHumanTimingScatterScale,
      getBeatBattleHumanTimingProfileStyle,
      getBeatBattleHumanTimingRating,
      normalizeBeatBattleHumanTimingVersion,
      getBeatBattleLiveHumanTimingVersion,
      getBeatBattleHumanTimingJitterUnit,
      getBeatBattleHumanTimingOutcome,
      getBeatBattleHumanRunCondition,
      isBeatBattleFullSickEligible,
      getBeatBattlePopulationHashUnit,
      clampPopulationNumber,
      getBeatBattleTempoTraitFactors,
      getBeatBattleProfilePerformanceTraits,
      getBeatBattleProfileRunAccuracyDelta,
      applyBeatBattleProfileRunAccuracy,
      getBeatBattleProfileTargetAccuracy,
      getBeatBattleProfileAverageTargetAccuracy,
    };
  }

  const modules = globalThis.PKEModules || (globalThis.PKEModules = {});
  modules.createCpuTiming = createCpuTiming;
})();
