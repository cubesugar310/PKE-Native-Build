// Frozen original async calculations; Native boundaries are described in compat/README.md.
function pkeCompatRecords(now){
  return {seasonId:2,seasonLabel:'SEASON 2',seasonStartedAt:now,version:12,
    h2h:{},skinById:{},skinMoodById:{},developmentById:{},leagueById:{},
    titleState:{peakRpById:{},equippedById:{},earnedById:{}},
    player:{accuracy:93,accuracySum:0,accuracyMatches:0,recentMatches:[],onlineSongRangeTier:'beginner'},
    background:{liveMatches:[],serial:0,lastTick:now,elasticLiveRoomCap:1200,
      bossMatches:[],privateMatches:[],practiceById:{},spectatorsById:{},spectatorScanCursor:0},
    lastOpponentId:'',seasonResetNoticePending:true};
}
function createReferencePopulationLogic(native){
const Date=native.Date,performance=native.performance;const console={debug(){},warn(){}};
const {analyzeSongDifficulty, normalizeSongRange, getSongRangeLimit,
    getSongRangeLabel, promoteAutomaticSongRange, filterSongsByRange, formatSongDifficultyInfo} = PKEModules.songDifficulty;

function cleanAssetBaseName(value) {
    const raw = String(value || "").replace(/\\/g, "/");
    const last = raw.split("/").pop() || "";
    return last.replace(/\.(png|xml|json)$/i, "");
  }

function fileBaseName(file) {
    return String(file && file.name || "").replace(/\.[^.]+$/, "");
  }

function getLastPathSegment(path) {
    const normalized = String(path || "").replace(/\\/g, "/").replace(/\/+$/, "");
    if (!normalized) return "";
    const parts = normalized.split("/").filter(Boolean);
    return parts.length ? parts[parts.length - 1] : "";
  }

let onlineBattleQueueActive = false;

let beatBattleOnlineSongPools = {beginner:[], standard:[], all:[]};

let beatBattleSongByFolderLookup = new Map();

const PKE_HALLOWEEN_JST_OFFSET_MS = 9 * 60 * 60 * 1000;

function getBeatBattleHalloweenEventWindow(nowMs = Date.now()) {
    const instant = Number.isFinite(Number(nowMs)) ? Number(nowMs) : Date.now();
    const jstDate = new Date(instant + PKE_HALLOWEEN_JST_OFFSET_MS);
    const year = jstDate.getUTCFullYear();
    const startMs = Date.UTC(year, 9, 14, 15, 0, 0, 0);
    const endMs = Date.UTC(year, 9, 31, 15, 0, 0, 0);
    const active = instant >= startMs && instant < endMs;
    return {
      active,
      year,
      startMs,
      endMs,
      remainingMs: active ? Math.max(0, endMs - instant) : 0
    };
  }

function getBeatBattleBossTeamTarget(eventKind) {
    return String(eventKind || "weekly") === "halloween" ? 255000 : 276000;
  }

let beatBattleSpectatorMode = false;

let beatBattleSpectatorPlayerProfile = null;

let beatBattleSpectatorOpponentProfile = null;

let beatBattleSpectatorResultHandled = false;

let beatBattleSpectatorBackgroundMatchId = "";

let beatBattlePopulationReplayMode = false;

const BEAT_BATTLE_BACKGROUND_TICK_MS = 5000;

const BEAT_BATTLE_LIVE_CAP_NORMAL = 1200;

const BEAT_BATTLE_LIVE_CAP_BUSY = 2000;

const BEAT_BATTLE_LIVE_CAP_PEAK = 2800;

const BEAT_BATTLE_LIVE_WAIT_BUSY = 2000;

const BEAT_BATTLE_LIVE_WAIT_PEAK = 5000;

const BEAT_BATTLE_LIVE_DEMAND_BUSY = 3200;

const BEAT_BATTLE_LIVE_DEMAND_PEAK = 6000;

const BEAT_BATTLE_BACKGROUND_BURST_MATCHES = 32;

let beatBattlePlayerLiveRoomActive = false;

let beatBattlePlayerRoomQueueReserved = false;

let beatBattleLocalAudienceRoom = null;

let beatBattleViewerRooms = new Map();

let beatBattleViewerIdsByRoom = new Map();

let beatBattleViewerParticipantIds = new Set();

const beatBattleViewerReservedParticipantIds = new Set();

const beatBattleViewerReleasedParticipantIds = new Set();

let beatBattleViewerIndexRevision = 0;

const beatBattlePopulationPresenceCache = new Map();

const BEAT_BATTLE_SPECTATOR_SCAN_SIZE = 1000;

const BEAT_BATTLE_BACKGROUND_SLICE_SIZE_PLAYING = 24;

const BEAT_BATTLE_BACKGROUND_SLICE_SIZE_IDLE = 24;

let beatBattleBackgroundProfilesReady = false;

const BEAT_BATTLE_LIVE_NOTE_SIM_VERSION = 5;

const BEAT_BATTLE_HUMAN_TIMING_VERSION = 3;

const BEAT_BATTLE_RUN_FORM_VERSION = 1;

const BEAT_BATTLE_LIVE_NOTE_YIELD_BUDGET_PLAYING = 1600;

const BEAT_BATTLE_LIVE_NOTE_YIELD_BUDGET_IDLE = 1024;

const BEAT_BATTLE_RAPID_TRAIT_MAX_ACC_DELTA = 2.0;

const BEAT_BATTLE_TEMPO_TRAIT_MAX_ACC_DELTA = 1.4;

const BEAT_BATTLE_POPULATION_TARGET_ACC_CAP = 99.8;

const BEAT_BATTLE_FULL_SICK_CONDITION_MIN = 0.98;

const BEAT_BATTLE_FULL_SICK_AFFINITY_MIN = 0.75;

const BEAT_BATTLE_HUMAN_DENSITY_WINDOW_MS = 1000;

const BEAT_BATTLE_HUMAN_STRAIN_DECAY_MS = 4000;

const BEAT_BATTLE_HUMAN_DUPLICATE_EPSILON_MS = 0.01;

const BEAT_BATTLE_HUMAN_TIMING_DRIFT_BUCKET_MS = 2400;

const BEAT_BATTLE_HUMAN_TIMING_PHRASE_BUCKET_MS = 950;

const BEAT_BATTLE_HUMAN_TIMING_FOCUS_BUCKET_MS = 6500;

const BEAT_BATTLE_HUMAN_TIMING_SQRT6 = Math.sqrt(6);

const BEAT_BATTLE_HUMAN_TIMING_PROFILE_BIAS_WEIGHT = 0.55;

const BEAT_BATTLE_HUMAN_TIMING_JITTER_WEIGHT = 1.05;

const BEAT_BATTLE_HUMAN_TIMING_SIGMA_TABLE = [
    [99.8, 20.176],
    [99.5, 21.397],
    [99.0, 22.964],
    [98.0, 25.617],
    [97.0, 28.108],
    [95.0, 33.234],
    [93.0, 38.730],
    [90.0, 45.134],
    [87.0, 51.063],
    [84.0, 57.206],
    [80.0, 66.000],
    [75.0, 78.000],
    [70.0, 91.000],
    [60.0, 118.000]
  ];

const beatBattleHumanTimingProfileStyleCache = new Map();

const BEAT_BATTLE_STARTUP_RECOVERY_MS = 15000;

const BEAT_BATTLE_STARTUP_SLICE_SIZE = 12;

const BEAT_BATTLE_STARTUP_NOTE_YIELD_BUDGET = 192;

const BEAT_BATTLE_STARTUP_MAX_NOTES_PER_MATCH = 48;

const BEAT_BATTLE_BACKGROUND_MAX_NOTES_PER_MATCH = 384;

const BEAT_BATTLE_STARTUP_AGGREGATE_BURST = 48;

const beatBattleRuntimeStartedAt =
    typeof performance !== "undefined" && typeof performance.now === "function"
      ? performance.now()
      : Date.now();

const beatBattleLiveRandomContextCache = new WeakMap();

const beatBattleLiveNoteArrayOwnership = new WeakMap();

const beatBattleLiveNoteNormalizedSideStates = new WeakSet();

const BEAT_BATTLE_PLAYER_ID = "YOU";

const BEAT_BATTLE_SLEEP_PROBABILITY = 0.12;

const BEAT_BATTLE_REST_PROBABILITY = 0.08;

const BEAT_BATTLE_SLEEP_BUCKET_MS = 30 * 60 * 1000;

const BEAT_BATTLE_REST_BUCKET_MS = 8 * 60 * 1000;

const BEAT_BATTLE_BACKGROUND_BOSS_MATCH_TARGET= 500;

const BEAT_BATTLE_BACKGROUND_PRIVATE_MATCH_TARGET= 200;

let beatBattleRankingCacheDirty = true;

let beatBattleRankingRevision = 0;

let beatBattlePlayerRankCache = { rank: 0, builtAt: 0, rp: null, accuracy: null };

const beatBattlePopulationSocial=globalThis.PKEPopulationSocial;

let beatBattlePopulationSocialLookupCache = null;

function invalidateBeatBattlePlayerRankCache() {
    beatBattlePlayerRankCache.builtAt = 0;
    beatBattleRankingRevision++;
  }

function updateBeatBattleLiveRoomCountLabel(){}

function getBeatBattleSongPoolForRange(range) {
    const normalized = normalizeSongRange(range);
    if (!discoveredFreeplaySongs.length) return [];
    const cached = beatBattleOnlineSongPools[normalized];
    const allPool = beatBattleOnlineSongPools.all;
    const poolsReady = Array.isArray(allPool) && allPool.length === discoveredFreeplaySongs.length;
    if (poolsReady && Array.isArray(cached)) return cached;
    return filterSongsByRange(discoveredFreeplaySongs, normalized);
  }

function chooseBeatBattleSongForRange(range, unit = Math.random()) {
    const pool = getBeatBattleSongPoolForRange(range);
    if (!pool.length) return null;
    const safeUnit = Math.max(0, Math.min(0.999999999, Number(unit) || 0));
    return pool[Math.floor(safeUnit * pool.length)] || null;
  }

function getBeatBattleSongGlobalSlot(song) {
    if (!song || !discoveredFreeplaySongs.length) return 0;
    const index = discoveredFreeplaySongs.indexOf(song);
    return index >= 0 ? index : 0;
  }

function getBeatBattleBackgroundMatchSong(match) {
    if (!match || !discoveredFreeplaySongs.length) return null;
    const wantedFolder = String(match.songFolder || "").trim().toLowerCase();
    if (wantedFolder) {
      const exact = beatBattleSongByFolderLookup.get(wantedFolder);
      if (exact) return exact;
    }
    const slot = Math.abs(Math.floor(Number(match.songSlot) || 0));
    return discoveredFreeplaySongs[slot % discoveredFreeplaySongs.length] || null;
  }

function getBeatBattleSongStars(song) {
    const rating = song && song.beatBattleDifficulty;
    const direct = Math.floor(Number(rating && rating.maxStars));
    if (direct >= 1 && direct <= 7) return direct;
    const player = Math.floor(Number(rating && rating.player && rating.player.stars));
    const opponent = Math.floor(Number(rating && rating.opponent && rating.opponent.stars));
    const stars = Math.max(player || 0, opponent || 0);
    return stars >= 1 && stars <= 7 ? stars : 0;
  }

function getBeatBattleBossSongDescriptors() {
    return discoveredFreeplaySongs.map((song, index) => ({
      id: String(song.folderName || song.displayName || index),
      name: String(song.displayName || song.folderName || "Song"),
      stars: getBeatBattleSongStars(song),
      eligible: !!(song && song.chartFile && song.instFile && song.opponentCharacterId),
      opponentCharacterId: String(song.opponentCharacterId || ""),
      songIndex: index
    }));
  }

function findBeatBattleDiscoveredSongByFolder(folderName) {
    const wanted = String(folderName || "").trim().toLowerCase();
    if (!wanted) return null;
    return discoveredFreeplaySongs.find(song =>
      String(song && song.folderName || "").trim().toLowerCase() === wanted
    ) || null;
  }

function getBeatBattleSpectatorSeed(key, salt) {
    const hex = replayHashString(
      "spectator-live-v1|" + String(key || "match") + "|" + String(salt || "seed")
    );
    const value = parseInt(hex, 16) >>> 0;
    return value || 1;
  }

const hitWindows = {
    sick: 45,
    good: 90,
    bad: 135,
    miss: 180
  };

const {
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
  } = PKEModules.createCpuTiming({
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
  });

let instBuffer = null;

let currentFreeplayFolderName = "";

let discoveredFreeplaySongs=native.songs;

let held = [false, false, false, false];

const BEAT_BATTLE_LEGACY_POPULATION_SIZE = 10000;

const BEAT_BATTLE_NEW_REGISTRATION_COUNT = 4999;

const BEAT_BATTLE_POPULATION_SIZE = BEAT_BATTLE_LEGACY_POPULATION_SIZE + BEAT_BATTLE_NEW_REGISTRATION_COUNT;

const BEAT_BATTLE_REGISTERED_TOTAL = BEAT_BATTLE_POPULATION_SIZE + 1;

const BEAT_BATTLE_POPULATION_SKILL_CAP = 99.5;

const BEAT_BATTLE_POPULATION_MIN_POTENTIAL_GAIN = 0.35;

const BEAT_BATTLE_POPULATION_MAX_POTENTIAL_GAIN = 2.0;

const BEAT_BATTLE_POPULATION_MIN_GROWTH_CURVE_MATCHES = 180;

const BEAT_BATTLE_POPULATION_MAX_GROWTH_CURVE_MATCHES = 360;

const BEAT_BATTLE_PLAYER_START_RP = 1500;

const BEAT_BATTLE_PLAYER_START_ACCURACY = 93;

let beatBattleBackgroundComputePopulationRevision = 1;

function getBeatBattlePopulationRevision() {
    return beatBattleBackgroundComputePopulationRevision;
  }

function markBeatBattlePopulationDerivedStateDirty() {
    beatBattleBackgroundComputePopulationRevision++;
    return beatBattleBackgroundComputePopulationRevision;
  }

function requestBeatBattleBackgroundCompute(){throw Error("Native uses the original in-context compute fallback");}

const BEAT_BATTLE_BEGINNER_OPPONENT_MAX_ACCURACY = 95.0;

const BEAT_BATTLE_RP_K_FACTOR = 50;

const BEAT_BATTLE_RP_MAX_DELTA = 50;

const BEAT_BATTLE_RP_DRAW_BONUS = 12;

let beatBattlePopulationOpponent = null;

let beatBattlePopulationMatchLockedForCurrentChart = false;

let beatBattlePopulationRecords=native.records;

let beatBattleBackgroundMatchmakingPool = null;

let beatBattlePriorityLiveRefillDepth = 0;

const beatBattlePopulationBaseProfileCache = new Array(BEAT_BATTLE_POPULATION_SIZE);

function getBeatBattleScoreWindowEndMs() {
    const seconds = instBuffer && Number(instBuffer.duration);
    return Number.isFinite(seconds) && seconds >= 0 ? seconds * 1000 : Infinity;
  }

function isBeatBattleNoteScoreEligible(noteOrTime) {
    const time = Number(noteOrTime && typeof noteOrTime === "object" ? noteOrTime.time : noteOrTime);
    if (!Number.isFinite(time) || time < 0) return false;
    const end = getBeatBattleScoreWindowEndMs();
    return !Number.isFinite(end) || time < end;
  }

function isBeatBattleChartNoteScoreEligible(chartData, noteTime) {
    const current = getCurrentBeatBattleLightweightChartData();
    if (!chartData || !current || chartData !== current) return true;
    return isBeatBattleNoteScoreEligible(noteTime);
  }

const beatBattleScoreableChartCountsCache = new WeakMap();

function getBeatBattleScoreableChartNoteCounts(chartData) {
    const fallback = {
      player: Math.max(0, Number(chartData && chartData.playerNoteCount) || 0),
      opponent: Math.max(0, Number(chartData && chartData.opponentNoteCount) || 0)
    };
    const current = getCurrentBeatBattleLightweightChartData();
    if (!chartData || !chartData.times || !current || chartData !== current) return fallback;
     
     
     
    const times = chartData.times;
    const sides = chartData.sides;
    const end = getBeatBattleScoreWindowEndMs();
    const cached = beatBattleScoreableChartCountsCache.get(chartData);
    if (cached && cached.times === times && cached.sides === sides &&
        cached.length === times.length && cached.end === end) {
      return { player: cached.player, opponent: cached.opponent };
    }
    let player = 0, opponent = 0;
    for (let i = 0; i < times.length; i++) {
      const time = Number(times[i]);
      if (!Number.isFinite(time) || time < 0 || time >= end) continue;
      if (sides[i] === 0) player++; else opponent++;
    }
    beatBattleScoreableChartCountsCache.set(chartData,
      { times, sides, length: times.length, end, player, opponent });
    return { player, opponent };
  }

function buildBeatBattleHumanChartContext(data) {
    const iterator = iterateBeatBattleHumanChartContext(data);
    let step = iterator.next();
    while (!step.done) step = iterator.next();
    return step.value;
  }

function* iterateBeatBattleHumanChartContext(data) {
    const count = data && data.times ? data.times.length : 0;
    const density = new Float32Array(count);
    const strain = new Float32Array(count);
    const ordinals = new Uint32Array(count);
    const sideCounts = [0, 0];
    const rapidRatios = [0, 0];
    const bySide = [[], []];
    const uniqueBySide = [[], []];
    const lastLaneTime = [[null, null, null, null], [null, null, null, null]];
    const lastLaneOrdinal = [[0, 0, 0, 0], [0, 0, 0, 0]];
     
     
    const unsorted = [];
    let work = 0;
    for (let i = 0; i < count; i++) {
      unsorted.push(i);
      if ((++work & 15) === 0) yield;
    }
    const indices = yield* iterateBeatBattleStableSort(unsorted,
      (a, b) => Number(data.times[a]) - Number(data.times[b]));
    for (const i of indices) {
      const side = data.sides[i] === 0 ? 0 : 1;
      const lane = Math.max(0, Math.min(3, Number(data.lanes[i]) || 0));
      const time = Number(data.times[i]);
      bySide[side].push(i);
      const previous = lastLaneTime[side][lane];
      if (previous === null || Math.abs(time - previous) > BEAT_BATTLE_HUMAN_DUPLICATE_EPSILON_MS) {
        ordinals[i] = sideCounts[side]++;
        lastLaneOrdinal[side][lane] = ordinals[i];
        uniqueBySide[side].push(i);
        lastLaneTime[side][lane] = time;
      } else {
        ordinals[i] = lastLaneOrdinal[side][lane];
      }
      if ((++work & 15) === 0) yield;
    }
    for (let side = 0; side < 2; side++) {
      const unique = uniqueBySide[side];
      let left = 0, right = 0, fatigue = 0, previousTime = null, previousLoad = 0;
      for (const i of bySide[side]) {
        const time = Number(data.times[i]);
        const halfWindow = BEAT_BATTLE_HUMAN_DENSITY_WINDOW_MS / 2;
        while (left < unique.length && Number(data.times[unique[left]]) < time - halfWindow) {
          left++;
          if ((++work & 15) === 0) yield;
        }
        while (right < unique.length && Number(data.times[unique[right]]) <= time + halfWindow) {
          right++;
          if ((++work & 15) === 0) yield;
        }
        const notesPerSecond = (right - left) * 1000 / BEAT_BATTLE_HUMAN_DENSITY_WINDOW_MS;
        const load = Math.max(0, Math.min(4, (notesPerSecond - 8) / 8));
        if (previousTime !== null) {
          const elapsed = Math.max(0, time - previousTime);
          const decay = Math.exp(-elapsed / BEAT_BATTLE_HUMAN_STRAIN_DECAY_MS);
           
           
          const activeLoad = elapsed <= BEAT_BATTLE_HUMAN_DENSITY_WINDOW_MS ? previousLoad : 0;
          fatigue = fatigue * decay + activeLoad * (1 - decay);
        }
        density[i] = load;
        strain[i] = fatigue;
        previousTime = time;
        previousLoad = load;
        if ((++work & 15) === 0) yield;
      }
      let rapidCount = 0;
      for (const i of unique) {
        if ((data.rapidFlags && data.rapidFlags[i]) || density[i] > 0) rapidCount++;
        if ((++work & 15) === 0) yield;
      }
      rapidRatios[side] = rapidCount / Math.max(1, sideCounts[side]);
    }
    return { density, strain, ordinals, sideCounts, rapidRatios };
  }

async function prepareBeatBattleBackgroundHumanChartContext(data, humanTimingVersion) {
    if (!data || humanTimingVersion < 3 || data.humanContext) return;
    const context = await runBeatBattleBackgroundProfileSearch(iterateBeatBattleHumanChartContext(data));
     
    if (!data.humanContext) data.humanContext = context;
  }

const BEAT_BATTLE_POPULATION_HISTORY_KEEP = 10;

const beatBattlePopulationHistoryMemory = new Map();

function isBeatBattleCorruptZeroScoreHistoryRecord(raw) {
    if (!raw || typeof raw !== "object") return false;
    const source = String(raw.source || "");
    if (source !== "live" && source !== "aggregate-fallback") return false;
    const scoreA = Number(raw.scoreA);
    const scoreB = Number(raw.scoreB);
     
     
     
     
     
    return Number.isFinite(scoreA) && Number.isFinite(scoreB) && scoreA === 0 && scoreB === 0;
  }

function normalizeBeatBattlePopulationHistoryRecord(raw) {
    if (!raw || typeof raw !== "object") return null;
    if (isBeatBattleCorruptZeroScoreHistoryRecord(raw)) return null;
    const aId = String(raw.aId || "");
    const bId = String(raw.bId || "");
    const matchId = String(raw.matchId || raw.id || "");
    if (!aId || !bId || !matchId || aId === bId) return null;
    return {
      matchId,
      aId,
      bId,
      aCharacterId: String(raw.aCharacterId || "").trim(),
      bCharacterId: String(raw.bCharacterId || "").trim(),
      endedAt: Math.max(1, Math.floor(Number(raw.endedAt) || Date.now())),
      songSlot: Number.isFinite(Number(raw.songSlot)) ? Math.floor(Number(raw.songSlot)) : null,
      songFolder: String(raw.songFolder || ""),
      songName: String(raw.songName || "Song"),
      scoreA: Math.max(0, Math.min(100000, Math.round(Number(raw.scoreA) || 0))),
      scoreB: Math.max(0, Math.min(100000, Math.round(Number(raw.scoreB) || 0))),
      accuracyA: Math.max(0, Math.min(100, Number(raw.accuracyA) || 0)),
      accuracyB: Math.max(0, Math.min(100, Number(raw.accuracyB) || 0)),
      deltaA: Math.round(Number(raw.deltaA) || 0),
      deltaB: Math.round(Number(raw.deltaB) || 0),
      source: String(raw.source || "aggregate"),
      replayable: !!raw.replayable,
      simulationVersion: Math.max(0, Math.floor(Number(raw.simulationVersion) || 0)),
      humanTimingVersion: getBeatBattleLiveHumanTimingVersion(raw),
      runFormVersion: getBeatBattleMatchRunFormVersion(raw),
      youReplayId: String(raw.youReplayId || "")
    };
  }

function cacheBeatBattlePopulationHistoryRecord(record) {
    const normalized = normalizeBeatBattlePopulationHistoryRecord(record);
    if (!normalized) return null;
    for (const playerId of [normalized.aId, normalized.bId]) {
      const current = Array.isArray(beatBattlePopulationHistoryMemory.get(playerId))
        ? beatBattlePopulationHistoryMemory.get(playerId).slice()
        : [];
      const filtered = current.filter(item => String(item && item.matchId || "") !== normalized.matchId);
      filtered.push(normalized);
      filtered.sort((a, b) => Number(b.endedAt) - Number(a.endedAt));
      beatBattlePopulationHistoryMemory.set(playerId, filtered.slice(0, BEAT_BATTLE_POPULATION_HISTORY_KEEP));
    }
    return normalized;
  }

function queueBeatBattlePopulationHistoryRecord(record){native.dirty=true;return !!cacheBeatBattlePopulationHistoryRecord(record);}

function getBeatBattlePopulationHistorySongFields(match) {
    const song = getBeatBattleBackgroundMatchSong(match);
    return {
      songFolder: String(song && song.folderName || ""),
      songName: String(song && (song.displayName || song.folderName) || "Song")
    };
  }

let replayMode = false;

function replayHashString(input) {
    let hash = 0x811c9dc5;
    const value = String(input || "");
    for (let i = 0; i < value.length; i++) {
      hash ^= value.charCodeAt(i);
      hash = Math.imul(hash, 0x01000193) >>> 0;
    }
    return hash.toString(16).padStart(8, "0");
  }

function appendReplayHashUint32(hash, value) {
    for (let i = 0; i < value.length; i++) {
      hash ^= value.charCodeAt(i);
      hash = Math.imul(hash, 0x01000193) >>> 0;
    }
    return hash >>> 0;
  }

function createBeatBattleRandomPrefixContext(notePrefix, stylePrefix) {
    return {
      notePrefix, stylePrefix,
      noteHash: appendReplayHashUint32(0x811c9dc5, notePrefix),
      styleHash: appendReplayHashUint32(0x811c9dc5, stylePrefix),
      styleUnits: new Map()
    };
  }

let currentBFSkin=native.playerSkin;

const detectedBfSkins = new Map();

let detectedBfSkinOrder = [];

let detectedBfSkinNameIndex = null;

let beatBattlePopulationSkinAvailability={candidates:native.skins,aliases:new Set(native.skins.map(id=>cleanAssetBaseName(id).toLowerCase())),rawCandidateIds:native.skins};

function getBeatBattleFixedSkinId(profile, candidateIds = null) {
    if (!profile) return "";

    if (profile.id === BEAT_BATTLE_PLAYER_ID) {
      return String(currentBFSkin || profile.characterId || "").trim();
    }

    const allowRecordedReplaySkin = !!(replayMode || beatBattlePopulationReplayMode);
    if (profile.matchCharacterId &&
        (allowRecordedReplaySkin || !isPsychCharacterIdBanned(profile.matchCharacterId))) {
      return String(profile.matchCharacterId).trim();
    }

    const records = loadBeatBattlePopulationRecords();
    const saved = String(records.skinById[profile.id] || profile.characterId || "").trim();
    const available = beatBattlePopulationSkinAvailability;
    if (saved && !isPsychCharacterIdBanned(saved) &&
        (!available || available.aliases.has(cleanAssetBaseName(saved).toLowerCase()))) return saved;

    const candidates = available ? available.candidates :
      (Array.isArray(candidateIds) ? candidateIds : collectRandomOpponentCandidateIds());
    if (!candidates.length) return "";

    const seed = Number(profile.skinSeed) >>> 0;
    const nextSkin = String(candidates[seed % candidates.length] || "").trim();
    if (saved && available && /^\d{5}$/.test(String(profile.id)) &&
        Number(profile.id) >= 1 && Number(profile.id) <= BEAT_BATTLE_POPULATION_SIZE) {
      records.skinById[profile.id] = nextSkin;
      profile.characterId = nextSkin;
      invalidateBeatBattleRankingCache();
    }
    return nextSkin;
  }

function isPsychCharacterIdBanned(id){return native.bannedSkins.has(cleanAssetBaseName(id).toLowerCase());}

function findDetectedBFSkinForPsychCharacter(characterId) {
    const wanted = cleanAssetBaseName(characterId).toLowerCase();
    if (!wanted) return "";

    
    
    
    
    if (!detectedBfSkinNameIndex || detectedBfSkinNameIndex.order !== detectedBfSkinOrder ||
        detectedBfSkinNameIndex.count !== detectedBfSkinOrder.length || detectedBfSkinNameIndex.size !== detectedBfSkins.size) {
      const names = new Map();
      for (const id of detectedBfSkinOrder) {
        const def = detectedBfSkins.get(id);
        if (!def) continue;
        const aliases = [
          def.baseName,
          def.displayName,
          getLastPathSegment(def.directory),
          def.jsonFile ? fileBaseName(def.jsonFile) : "",
          def.xmlFile ? fileBaseName(def.xmlFile) : "",
          def.imageFile ? fileBaseName(def.imageFile) : ""
        ];
        for (const value of aliases) {
          const alias = cleanAssetBaseName(value).toLowerCase();
          if (alias && !names.has(alias)) names.set(alias, id);
        }
      }
      detectedBfSkinNameIndex = {
        order: detectedBfSkinOrder, count: detectedBfSkinOrder.length, size: detectedBfSkins.size, names
      };
    }
    return detectedBfSkinNameIndex.names.get(wanted) || "";
  }

const BEAT_BATTLE_NAME_JP_SHORT = [
    "あかね", "ひより", "みなと", "こよい", "つばさ", "しの", "あおい", "れん",
    "まひろ", "ゆの", "せな", "ひなた", "かえで", "すい", "りつ", "あさひ",
    "かなで", "なずな", "しき", "こはる", "ふう", "まお", "さえ", "ゆら",
    "あんず", "しずく", "すみれ", "いおり", "かな", "みお", "なな", "れい",
    "りん", "そよ", "あむ", "のの", "きり", "うた", "るか", "とわ",
    "いろは", "つむぎ", "かのん", "ほのか", "ましろ", "ちなつ", "りお", "ねね"
  ];

const BEAT_BATTLE_NAME_JP_NOUN = [
    "こんぺいとう", "しらたま", "もなか", "あられ", "しおこんぶ", "ココア", "ミルクティー", "ソーダ",
    "プリン", "わたあめ", "おにぎり", "きなこパン", "クリームパン", "ゼリー", "レモネード", "あんみつ",
    "マグカップ", "ホチキス", "ふせん", "毛布", "まくら", "消しゴム", "えんぴつ", "時計",
    "充電器", "リモコン", "輪っか", "ビー玉", "ガラス", "余白", "白紙", "机",
    "らっこ", "くらげ", "かもめ", "うずら", "すずめ", "あひる", "こぐま", "やもり",
    "しろねこ", "こぎつね", "ぺんぎん", "たぬき", "ハムスター", "ふくろう", "あざらし", "ひつじ"
  ];

const BEAT_BATTLE_NAME_LATIN = [
    "Velo", "Neru", "Kivi", "Melo", "Raze", "Lino", "Noct", "Aru",
    "Mira", "Coda", "Lime", "Reef", "Faye", "Vivi", "Nero", "Sena",
    "Kite", "Lyn", "Riku", "Moca", "Sui", "Ame", "Reno", "Nagi",
    "Aster", "Frost", "Orbit", "Pixel", "Luca", "Hina", "Mio", "Rin",
    "Sora", "Kano", "Nemu", "Yoru", "Kuro", "Runa", "Ciel", "Noa",
    "Rito", "Aqua", "Luna", "Echo", "Mint", "Mugi", "Rui", "Haru"
  ];

const BEAT_BATTLE_NAME_PHRASE = [
    "今日は当たる", "あと一勝", "たぶんいける", "先に寝ます", "判定ください", "右見てます",
    "一旦休憩", "今の見た？", "フルコンしたい", "手が冷たい", "最後までやる", "ミスじゃない",
    "音ズレ？", "集中します", "次こそ勝つ", "譜面見えない", "ノーツどこ", "もう一曲だけ",
    "精度ほしい", "RPください", "観戦中", "今日はむり", "たぶん左", "ちょっと待って",
    "今のなし", "先に謝る", "寝る前ラスト", "指が動かない", "Hardください", "ゴースト見ます",
    "3キー派", "コンボどこ", "判定迷子", "最後だけ集中", "ここから本気", "一回だけやる"
  ];

const BEAT_BATTLE_NAME_CLAN = [
    "ARC", "VLT", "CRN", "FLC", "NXT", "QED", "R0", "LUX", "KSM", "VTX", "FRM", "ZEP"
  ];

const BEAT_BATTLE_NAME_WEIRD = [
    "?", "??", "…", "_", "0", "00", "404", "1/4", "2.0", "無題", "仮", "null", "nan", "x", "z", "A", "B"
  ];

const BEAT_BATTLE_NAME_JP_DECOR = ["", "", "", "さん", "くん", "ちゃん", "☆", "♪", "!", "？", "_", "2", "3"];

const BEAT_BATTLE_NAME_LATIN_DECOR = ["", "", "_", ".", "-", "_", ".", "JP", "ch", "TV", "X", "☆", "♪"];

function chooseBeatBattlePopulationNameItem(pool, id, key) {
    if (!Array.isArray(pool) || !pool.length) return "player";
    const unit = getBeatBattlePopulationHashUnit("human-name-v2|" + id + "|" + key);
    return pool[Math.floor(unit * pool.length) % pool.length];
  }

function createBeatBattlePopulationName(id) {
    const style = getBeatBattlePopulationHashUnit("human-name-v2|" + id + "|style");
    const shortJP = () => chooseBeatBattlePopulationNameItem(BEAT_BATTLE_NAME_JP_SHORT, id, "jp");
    const nounJP = () => chooseBeatBattlePopulationNameItem(BEAT_BATTLE_NAME_JP_NOUN, id, "noun");
    const latin = (key = "latin") => chooseBeatBattlePopulationNameItem(BEAT_BATTLE_NAME_LATIN, id, key);
    const smallNumber = (key, max = 99) => String(1 + Math.floor(getBeatBattlePopulationHashUnit("human-name-v2|" + id + "|" + key) * max));

    
    if (style < 0.27) {
      const base = shortJP();
      const decorate = getBeatBattlePopulationHashUnit("human-name-v2|" + id + "|jp-decor-roll");
      if (decorate < 0.20) {
        return base + chooseBeatBattlePopulationNameItem(BEAT_BATTLE_NAME_JP_DECOR, id, "jp-decor");
      }
      return base;
    }

    
    if (style < 0.44) {
      const base = nounJP();
      const roll = getBeatBattlePopulationHashUnit("human-name-v2|" + id + "|noun-decor-roll");
      if (roll < 0.11) return base + "さん";
      if (roll < 0.18) return base + "くん";
      if (roll < 0.23) return base + "☆";
      return base;
    }

    
    if (style < 0.60) {
      return latin();
    }

    
    if (style < 0.75) {
      const base = latin();
      const decor = chooseBeatBattlePopulationNameItem(BEAT_BATTLE_NAME_LATIN_DECOR, id, "latin-decor");
      const numRoll = getBeatBattlePopulationHashUnit("human-name-v2|" + id + "|latin-num-roll");
      if (decor === "." || decor === "_" || decor === "-") {
        return base + decor + (numRoll < 0.70 ? smallNumber("latin-num", 999) : latin("latin-alt").toLowerCase());
      }
      if (decor === "JP" || decor === "ch" || decor === "TV" || decor === "X") return base + decor;
      if (decor === "☆" || decor === "♪") return base + decor;
      return numRoll < 0.32 ? base + smallNumber("plain-num", 99) : base;
    }

    
    if (style < 0.87) {
      return chooseBeatBattlePopulationNameItem(BEAT_BATTLE_NAME_PHRASE, id, "phrase");
    }

    
    if (style < 0.95) {
      const clan = chooseBeatBattlePopulationNameItem(BEAT_BATTLE_NAME_CLAN, id, "clan");
      const mixedRoll = getBeatBattlePopulationHashUnit("human-name-v2|" + id + "|mixed-roll");
      if (mixedRoll < 0.45) return clan + "/" + latin("mixed-latin");
      if (mixedRoll < 0.75) return clan + "_" + shortJP();
      return clan + "." + latin("mixed-latin-dot").toLowerCase();
    }

    
    
    const weird = chooseBeatBattlePopulationNameItem(BEAT_BATTLE_NAME_WEIRD, id, "weird");
    const weirdRoll = getBeatBattlePopulationHashUnit("human-name-v2|" + id + "|weird-num-roll");
    if ((weird === "A" || weird === "B" || weird === "x" || weird === "z") && weirdRoll < 0.35) {
      return weird + smallNumber("weird-num", 9);
    }
    return weird;
  }

function getBeatBattlePopulationSkillCeiling(playerId, baseAccuracy) {
    const base = clampPopulationNumber(baseAccuracy, 60, BEAT_BATTLE_POPULATION_SKILL_CAP);
    const potentialUnit = (
      getBeatBattlePopulationHashUnit(String(playerId || "") + "|potential1") +
      getBeatBattlePopulationHashUnit(String(playerId || "") + "|potential2") +
      getBeatBattlePopulationHashUnit(String(playerId || "") + "|potential3")
    ) / 3;
    const gain = BEAT_BATTLE_POPULATION_MIN_POTENTIAL_GAIN +
      potentialUnit * (BEAT_BATTLE_POPULATION_MAX_POTENTIAL_GAIN - BEAT_BATTLE_POPULATION_MIN_POTENTIAL_GAIN);
    return Math.round(Math.min(BEAT_BATTLE_POPULATION_SKILL_CAP, base + gain) * 10) / 10;
  }

function getBeatBattlePopulationGrowthCurveMatches(playerId) {
    const unit = getBeatBattlePopulationHashUnit(String(playerId || "") + "|learning-speed");
    return BEAT_BATTLE_POPULATION_MIN_GROWTH_CURVE_MATCHES +
      unit * (BEAT_BATTLE_POPULATION_MAX_GROWTH_CURVE_MATCHES - BEAT_BATTLE_POPULATION_MIN_GROWTH_CURVE_MATCHES);
  }

function getBeatBattlePopulationDevelopedAccuracy(profile, developmentMatches) {
    if (!profile) return BEAT_BATTLE_PLAYER_START_ACCURACY;
    const base = clampPopulationNumber(
      Number(profile.baseAccuracy ?? profile.accuracy) || BEAT_BATTLE_PLAYER_START_ACCURACY,
      60,
      BEAT_BATTLE_POPULATION_SKILL_CAP
    );
    const ceiling = clampPopulationNumber(
      Number(profile.skillCeiling) || getBeatBattlePopulationSkillCeiling(profile.id, base),
      base,
      BEAT_BATTLE_POPULATION_SKILL_CAP
    );
    const matches = Math.max(0, Math.floor(Number(developmentMatches) || 0));
    if (!matches || ceiling <= base) return Math.round(base * 10) / 10;
    const curve = Math.max(1, getBeatBattlePopulationGrowthCurveMatches(profile.id));
    const progress = 1 - Math.exp(-matches / curve);
    return Math.round(Math.min(ceiling, base + (ceiling - base) * progress) * 10) / 10;
  }

function getBeatBattlePopulationTraitAffinity(playerId, key) {
    const roll = getBeatBattlePopulationHashUnit(
      "playstyle-v1|" + String(playerId || "") + "|" + String(key || "trait")
    );

    
    
    if (roll < 0.16) return -1.0;
    if (roll < 0.36) return -0.5;
    if (roll < 0.64) return 0.0;
    if (roll < 0.84) return 0.5;
    return 1.0;
  }

function getBeatBattlePopulationTraitLabels(profile) {
    if (!profile) return [];
    const labels = [];
    const rapid = Number(profile.rapidAffinity) || 0;
    const fast = Number(profile.fastSongAffinity) || 0;
    const slow = Number(profile.slowSongAffinity) || 0;

    if (rapid >= 0.75) labels.push("連打得意");
    else if (rapid <= -0.75) labels.push("連打苦手");
    if (fast >= 0.75) labels.push("速い曲得意");
    else if (fast <= -0.75) labels.push("速い曲苦手");
    if (slow >= 0.75) labels.push("遅い曲得意");
    else if (slow <= -0.75) labels.push("遅い曲苦手");

    if (!labels.length) labels.push("安定型");
    return labels;
  }

function getCurrentBeatBattleLightweightChartData() {
    const folder = String(currentFreeplayFolderName || "");
    if (!folder || !Array.isArray(discoveredFreeplaySongs)) return null;
    const song = discoveredFreeplaySongs.find(entry => String(entry && entry.folderName || "") === folder);
    return song && song.beatBattleLightweightChart ? song.beatBattleLightweightChart : null;
  }

function getBeatBattlePopulationBasePresenceStatus(playerId, now = Date.now()) {
    const id = String(playerId || "").trim();
    if (!id) return "ACTIVE";
    if (id === BEAT_BATTLE_PLAYER_ID) return "YOU";
    const time = Math.max(0, Number(now) || Date.now());
    let cached = beatBattlePopulationPresenceCache.get(id);
    if (cached && time >= cached.from && time < cached.until) return cached.status;
    if (!cached) {
      cached = {
        sleepPhase: getBeatBattlePopulationHashUnit("presence-v1|" + id + "|sleep|phase") * BEAT_BATTLE_SLEEP_BUCKET_MS,
        restPhase: getBeatBattlePopulationHashUnit("presence-v1|" + id + "|rest|phase") * BEAT_BATTLE_REST_BUCKET_MS
      };
      
      if (/^\d{5}$/.test(id) && Number(id) >= 1 && Number(id) <= BEAT_BATTLE_POPULATION_SIZE) {
        beatBattlePopulationPresenceCache.set(id, cached);
      }
    }
    const sleepBucket = Math.floor((time + cached.sleepPhase) / BEAT_BATTLE_SLEEP_BUCKET_MS);
    const restBucket = Math.floor((time + cached.restPhase) / BEAT_BATTLE_REST_BUCKET_MS);
    const sleepRoll = getBeatBattlePopulationHashUnit(
      "presence-v1|" + id + "|sleep|" + sleepBucket
    );
    const restRoll = sleepRoll < BEAT_BATTLE_SLEEP_PROBABILITY ? 1 : getBeatBattlePopulationHashUnit(
      "presence-v1|" + id + "|rest|" + restBucket
    );
    cached.status = sleepRoll < BEAT_BATTLE_SLEEP_PROBABILITY ? "SLEEP"
      : restRoll < BEAT_BATTLE_REST_PROBABILITY ? "REST" : "ACTIVE";
    cached.from = time;
    
    
    cached.until = Math.min((sleepBucket + 1) * BEAT_BATTLE_SLEEP_BUCKET_MS - cached.sleepPhase,
      (restBucket + 1) * BEAT_BATTLE_REST_BUCKET_MS - cached.restPhase);
    return cached.status;
  }

function normalizeBeatBattlePopulationSocialSessions(bg, now = Date.now()) {
    if (!bg || typeof bg !== "object") return;
    const eventState = PKEBossBattle && typeof PKEBossBattle.getEventState === "function"
      ? PKEBossBattle.getEventState(now)
      : { active: false, weekKey: "" };
    if (beatBattlePopulationSocial && typeof beatBattlePopulationSocial.normalize === "function") {
      beatBattlePopulationSocial.normalize(bg, now, eventState);
    } else {
      if (!Array.isArray(bg.bossMatches)) bg.bossMatches = [];
      if (!Array.isArray(bg.privateMatches)) bg.privateMatches = [];
      if (!eventState.active) bg.bossMatches = [];
    }
  }

function getBeatBattlePopulationSocialSessionStatus(playerId, now = Date.now()) {
    if (!beatBattlePopulationSocial || typeof beatBattlePopulationSocial.getStatus !== "function") return "";
    const records = loadBeatBattlePopulationRecords();
    const bg = records.background;
    const boss = Array.isArray(bg.bossMatches) ? bg.bossMatches : [];
    const privateRooms = Array.isArray(bg.privateMatches) ? bg.privateMatches : [];
    let cache = beatBattlePopulationSocialLookupCache;
    if (!cache || cache.background !== bg || cache.boss !== boss || cache.privateRooms !== privateRooms ||
        cache.bossLength !== boss.length || cache.privateLength !== privateRooms.length) {
      const bossById = new Map(), privateById = new Map();
      const add = (index, id, match) => {
        const key = String(id || '');
        let matches = index.get(key);
        if (!matches) index.set(key, matches = []);
        matches.push(match);
      };
      for (const match of boss) {
        for (const id of match && Array.isArray(match.playerIds) ? match.playerIds : []) add(bossById, id, match);
      }
      for (const match of privateRooms) {
        if (match) { add(privateById, match.aId, match); add(privateById, match.bId, match); }
      }
      cache = beatBattlePopulationSocialLookupCache = {
        background: bg, boss, privateRooms, bossLength: boss.length, privateLength: privateRooms.length,
        bossById, privateById, eventAt: null, eventState: null
      };
    }
    if (cache.eventAt !== now || !cache.eventState) {
      cache.eventState = PKEBossBattle.getEventState(now);
      cache.eventAt = now;
    }
    const id = String(playerId || '');
    
    
    return beatBattlePopulationSocial.getStatus({
      bossMatches: cache.bossById.get(id) || [],
      privateMatches: cache.privateById.get(id) || []
    }, playerId, now, cache.eventState);
  }

function getBeatBattlePopulationPresenceStatus(playerId, now = Date.now()) {
    
    
    const socialStatus = getBeatBattlePopulationSocialSessionStatus(playerId, now);
    if (socialStatus) return socialStatus;
    const base = getBeatBattlePopulationBasePresenceStatus(playerId, now);
    if (base !== "ACTIVE") return base;
    if (getBeatBattlePopulationPracticeSession(playerId, now)) return "PRACTICE";
    return getBeatBattlePopulationWatchedMatch(playerId, now) ? "WATCHING" : base;
  }

function getBeatBattlePopulationPracticeSession(playerId, now = Date.now(), participantIds = beatBattleViewerParticipantIds) {
    const id = String(playerId || "");
    const records = loadBeatBattlePopulationRecords();
    const session = records.background.practiceById[id];
    if (!session || !Number.isFinite(Number(session.endsAt)) || Number(session.endsAt) <= now ||
        Number(session.startedAt) > now || participantIds.has(id) ||
        getBeatBattlePopulationBasePresenceStatus(id, now) !== "ACTIVE") return null;
    return session;
  }

function normalizeBeatBattlePopulationPractice(bg, now = Date.now()) {
    if (!bg.practiceById || typeof bg.practiceById !== "object" || Array.isArray(bg.practiceById)) bg.practiceById = {};
    for (const [id, session] of Object.entries(bg.practiceById)) {
      if (!/^\d{5}$/.test(id) || Number(id) < 1 || Number(id) > BEAT_BATTLE_POPULATION_SIZE ||
          !session || typeof session !== "object" || Array.isArray(session) ||
          !Number.isFinite(Number(session.startedAt)) || !Number.isFinite(Number(session.endsAt)) ||
          Number(session.startedAt) > now || Number(session.endsAt) <= now || Number(session.endsAt) > now + 3600000) {
        delete bg.practiceById[id];
        continue;
      }
      session.songName = String(session.songName || "Song").slice(0, 160);
      session.songFolder = String(session.songFolder || "").slice(0, 200);
    }
  }

function beginBeatBattlePopulationPractice(playerId, now = Date.now()) {
    const id = String(playerId);
    if (!discoveredFreeplaySongs.length || beatBattleViewerParticipantIds.has(id) ||
        getBeatBattlePopulationPresenceStatus(id, now) !== "ACTIVE") return false;
    const roll = getBeatBattlePopulationHashUnit("practice-v1|" + id + "|song|" + Math.floor(now / 5000));
    const song = discoveredFreeplaySongs[Math.min(discoveredFreeplaySongs.length - 1, Math.floor(roll * discoveredFreeplaySongs.length))];
    const duration = Math.max(10000, Math.min(3600000, Number(song.beatBattleLightweightChart && song.beatBattleLightweightChart.durationMs) || 120000));
    loadBeatBattlePopulationRecords().background.practiceById[id] = {
      songFolder:String(song.folderName || ""), songName:String(song.displayName || "Song"), startedAt:now, endsAt:now + duration
    };
    return true;
  }

function isBeatBattlePopulationAvailableForMatch(playerId, now = Date.now()) {
    const id = String(playerId || "");
    if (beatBattleViewerReservedParticipantIds.has(id)) return false;
    if (beatBattleViewerParticipantIds.has(id) && !beatBattleViewerReleasedParticipantIds.has(id)) return false;
    if (beatBattlePlayerLiveRoomActive && !beatBattleSpectatorMode && !replayMode &&
        beatBattlePopulationOpponent && String(beatBattlePopulationOpponent.id) === String(playerId)) return false;
    if (beatBattleSpectatorMode && !beatBattleSpectatorResultHandled && !beatBattlePopulationReplayMode && !replayMode &&
        [beatBattleSpectatorPlayerProfile, beatBattleSpectatorOpponentProfile].some(profile => profile && String(profile.id) === String(playerId))) return false;
    return getBeatBattlePopulationPresenceStatus(playerId, now) === "ACTIVE";
  }

function* iterateBeatBattleAvailablePopulationProfiles(excludeIds = null, maxTries = 240, options = null) {
    const exclude = excludeIds instanceof Set ? excludeIds : new Set(excludeIds || []);
    const tries = Math.max(20, Math.floor(Number(maxTries) || 240));
    const availabilityNow = options && Number.isFinite(Number(options.availabilityNow))
      ? Number(options.availabilityNow)
      : Date.now();
    const requiredSongRange = options && options.requiredSongRange
      ? normalizeSongRange(options.requiredSongRange)
      : "";
    const candidatePool = options && Array.isArray(options.candidatePool)
      ? options.candidatePool
      : null;
    const availableIds = options && options.availableIds instanceof Set
      ? options.availableIds
      : null;
    const allowHigherSongRange = !!(options && options.allowHigherSongRange);
    const rawMaxAccuracy = options ? options.maxAccuracy : undefined;
    const maxAccuracy = rawMaxAccuracy !== null && rawMaxAccuracy !== undefined &&
      Number.isFinite(Number(rawMaxAccuracy))
      ? clampPopulationNumber(rawMaxAccuracy, 0, 100)
      : null;

    const candidateMatchesRules = candidate => {
      if (!candidate || exclude.has(candidate.id)) return false;
      if (availableIds ? !availableIds.has(candidate.id) : !isBeatBattlePopulationAvailableForMatch(candidate.id, availabilityNow)) return false;
      if (requiredSongRange) {
        const candidateSongRange = candidate._beatBattleMatchRange || getBeatBattlePopulationOnlineSongRange(candidate);
        const canUseHigherStandardOpponent = allowHigherSongRange &&
          requiredSongRange === "standard" && candidateSongRange === "all";
        if (candidateSongRange !== requiredSongRange && !canUseHigherStandardOpponent) return false;
      }
      if (maxAccuracy !== null && Number(candidate.accuracy) > maxAccuracy) return false;
      return true;
    };

    const sourceSize = candidatePool ? candidatePool.length : BEAT_BATTLE_POPULATION_SIZE;
    if (!sourceSize) return null;
    for (let i = 0; i < tries; i++) {
      const candidateIndex = Math.floor(Math.random() * sourceSize);
      const candidate = candidatePool
        ? candidatePool[candidateIndex]
        : getBeatBattleEffectivePopulationProfileByIndex(candidateIndex);
      yield;
      if (!candidateMatchesRules(candidate)) continue;
      return candidate;
    }

    
    
    const startIndex = Math.floor(Math.random() * sourceSize);
    for (let offset = 0; offset < sourceSize; offset++) {
      const candidateIndex = (startIndex + offset) % sourceSize;
      const candidate = candidatePool
        ? candidatePool[candidateIndex]
        : getBeatBattleEffectivePopulationProfileByIndex(candidateIndex);
      yield;
      if (!candidateMatchesRules(candidate)) continue;
      return candidate;
    }

    return null;
  }

async function runBeatBattleBackgroundProfileSearch(iterator) {
    let deadline = getBeatBattleBackgroundWorkDeadline();
    for (;;) {
      const step = iterator.next();
      if (step.done) return step.value;
      if (performance.now() >= deadline) {
        await yieldBeatBattleBackgroundWork();
        deadline = getBeatBattleBackgroundWorkDeadline();
      }
    }
  }

async function getBeatBattleBackgroundAvailableProfile(excludeIds = null, maxTries = 240, options = null) {
    return runBeatBattleBackgroundProfileSearch(iterateBeatBattleAvailablePopulationProfiles(excludeIds, maxTries, options));
  }

async function prepareBeatBattleBackgroundPopulationProfiles() {
    if (beatBattleBackgroundProfilesReady) return;
    let deadline = getBeatBattleBackgroundWorkDeadline();
    const now = Date.now();
    for (let i = 0; i < BEAT_BATTLE_POPULATION_SIZE; i++) {
      createBeatBattlePopulationProfile(i);
      getBeatBattlePopulationBasePresenceStatus(String(i + 1).padStart(5, "0"), now);
      if (performance.now() >= deadline) {
        await yieldBeatBattleBackgroundWork();
        deadline = getBeatBattleBackgroundWorkDeadline();
      }
    }
    beatBattleBackgroundProfilesReady = true;
  }

function createBeatBattlePopulationProfile(index) {
    const safeIndex = Math.max(0, Math.min(BEAT_BATTLE_POPULATION_SIZE - 1, Math.floor(Number(index) || 0)));
    const cached = beatBattlePopulationBaseProfileCache[safeIndex];
    if (cached) return { ...cached };

    const numericId = safeIndex + 1;
    const id = String(numericId).padStart(5, "0");

    const name = createBeatBattlePopulationName(id);

    
    
    const skillUnit = (
      getBeatBattlePopulationHashUnit(id + "|skill1") +
      getBeatBattlePopulationHashUnit(id + "|skill2") +
      getBeatBattlePopulationHashUnit(id + "|skill3") +
      getBeatBattlePopulationHashUnit(id + "|skill4")
    ) / 4;

    
    
    
    const baseAccuracy = Math.round((84 + skillUnit * 15.2) * 10) / 10;
    const potentialUnit = (
      getBeatBattlePopulationHashUnit(id + "|potential1") +
      getBeatBattlePopulationHashUnit(id + "|potential2") +
      getBeatBattlePopulationHashUnit(id + "|potential3")
    ) / 3;
    const skillCeiling = Math.round(Math.min(
      99.5,
      baseAccuracy + 0.35 + potentialUnit * 1.65
    ) * 10) / 10;
    const accuracy = baseAccuracy;
    const isNewRegistration = numericId > BEAT_BATTLE_LEGACY_POPULATION_SIZE;
    const rpNoise = (getBeatBattlePopulationHashUnit(id + "|rpNoise") - 0.5) * 220;
    
    
    
    
    const rp = isNewRegistration
      ? BEAT_BATTLE_PLAYER_START_RP
      : Math.round(clampPopulationNumber(850 + skillUnit * 2050 + rpNoise, 800, 2950));
    const timingBiasMs = Math.round((getBeatBattlePopulationHashUnit(id + "|timingBias") - 0.5) * 24);
    const rapidAffinity = getBeatBattlePopulationTraitAffinity(id, "rapid");
    const fastSongAffinity = getBeatBattlePopulationTraitAffinity(id, "fast-song");
    const slowSongAffinity = getBeatBattlePopulationTraitAffinity(id, "slow-song");
    const performanceTraits = getBeatBattleProfilePerformanceTraits({ id });
    const performanceStability = performanceTraits.stability;
    const performanceUpside = performanceTraits.upside;
    const performanceDownside = performanceTraits.downside;
    const matches = isNewRegistration
      ? 0
      : 50 + Math.floor(getBeatBattlePopulationHashUnit(id + "|matches") * 950);
    const winRate = clampPopulationNumber(
      0.30 + skillUnit * 0.38 + (getBeatBattlePopulationHashUnit(id + "|winNoise") - 0.5) * 0.10,
      0.22,
      0.78
    );
    const baseWins = isNewRegistration ? 0 : Math.round(matches * winRate);
    const baseLosses = isNewRegistration ? 0 : Math.max(0, matches - baseWins);
    const skinSeed = parseInt(replayHashString("population-skin-v1|" + id), 16) >>> 0;

    const profile = {
      id,
      numericId,
      name,
      baseAccuracy,
      skillCeiling,
      accuracy,
      rp,
      timingBiasMs,
      rapidAffinity,
      fastSongAffinity,
      slowSongAffinity,
      performanceStability,
      performanceUpside,
      performanceDownside,
      playStyleTraits: [],
      baseWins,
      baseLosses,
      skinSeed,
      characterId: ""
    };
    profile.playStyleTraits = getBeatBattlePopulationTraitLabels(profile);
    beatBattlePopulationBaseProfileCache[safeIndex] = profile;
    return { ...profile };
  }

function queueBeatBattlePopulationRankingJournalSnapshot(){native.dirty=true;}

function loadBeatBattlePopulationRecords(){return beatBattlePopulationRecords;}

function collectBeatBattlePopulationSkinCandidateIds(){return native.skins.filter(id=>!isPsychCharacterIdBanned(id));}

function getBeatBattlePopulationSkinChangeInterval(playerId, turn, purpose = "natural") {
    const variety = getBeatBattlePopulationHashUnit("skin-mood-v1|" + playerId + "|variety");
    const jitter = getBeatBattlePopulationHashUnit("skin-mood-v1|" + playerId + "|" + purpose + "-interval|" + turn);
    const minutes = (purpose === "cooldown" ? 20 : 15) + (1 - variety) * 20 + jitter * 10;
    return Math.round(minutes * 60000);
  }

function getBeatBattlePopulationSkinMood(playerId, now = Date.now()) {
    const id = String(playerId || "");
    if (!/^\d{5}$/.test(id) || Number(id) < 1 || Number(id) > BEAT_BATTLE_POPULATION_SIZE) return null;
    const records = loadBeatBattlePopulationRecords();
    let state = records.skinMoodById[id];
    if (!state || typeof state !== "object" || Array.isArray(state)) {
      state = records.skinMoodById[id] = {};
    }
    state.lossStreak = Math.max(0, Math.min(1000000, Math.floor(Number(state.lossStreak) || 0)));
    state.turn = Math.max(0, Math.min(1000000000, Math.floor(Number(state.turn) || 0)));
    state.pendingChange = state.pendingChange === "losses" ? "losses" : "";
    if (!Number.isFinite(Number(state.nextNaturalAt)) || Number(state.nextNaturalAt) <= 0 || Number(state.nextNaturalAt) > now + 86400000) {
      state.nextNaturalAt = now + getBeatBattlePopulationSkinChangeInterval(id, state.turn);
    }
    state.cooldownUntil = Math.max(0, Math.min(now + 3600000, Number(state.cooldownUntil) || 0));
    return state;
  }

function recordBeatBattlePopulationSkinMoodResult(profile, actual, now = Date.now()) {
    const state = getBeatBattlePopulationSkinMood(profile && profile.id, now);
    if (!state) return;
    state.turn++;
    if (actual !== 0) {
      state.lossStreak = 0;
      state.pendingChange = "";
      return;
    }
    state.lossStreak++;
    if (state.lossStreak < 3 || state.pendingChange || now < state.cooldownUntil) return;
    const variety = getBeatBattlePopulationHashUnit("skin-mood-v1|" + profile.id + "|variety");
    const chance = Math.min(0.85, (0.18 + (state.lossStreak - 3) * 0.13) * (0.75 + variety * 0.5));
    const roll = getBeatBattlePopulationHashUnit("skin-mood-v1|" + profile.id + "|loss|" + state.turn);
    if (roll < chance) state.pendingChange = "losses";
  }

function maybeChangeBeatBattlePopulationSkin(profileOrId, candidateIds = null, now = Date.now()) {
    const id = String(profileOrId && typeof profileOrId === "object" ? profileOrId.id : profileOrId || "");
    if (!/^\d{5}$/.test(id) || Number(id) < 1 || Number(id) > BEAT_BATTLE_POPULATION_SIZE) return false;
    const candidates = Array.isArray(candidateIds) ? candidateIds : collectBeatBattlePopulationSkinCandidateIds();
    if (candidates.length < 2) return false;
    if (!replayMode && !beatBattlePopulationReplayMode && beatBattlePopulationMatchLockedForCurrentChart &&
        beatBattlePopulationOpponent && String(beatBattlePopulationOpponent.id) === id) return false;
    if (!replayMode && !beatBattlePopulationReplayMode && (beatBattleSpectatorMode || onlineBattleQueueActive) &&
        [beatBattleSpectatorPlayerProfile, beatBattleSpectatorOpponentProfile].some(profile => profile && String(profile.id) === id)) return false;
    if (getBeatBattlePopulationPresenceStatus(id, now) !== "ACTIVE") return false;
    const state = getBeatBattlePopulationSkinMood(id, now);
    if (now < state.cooldownUntil || (!state.pendingChange && now < state.nextNaturalAt)) return false;
    const records = loadBeatBattlePopulationRecords();
     
     
    if ((records.background.liveMatches || []).some(match => match && (String(match.aId) === id || String(match.bId) === id))) return false;
    const profile = typeof profileOrId === "object" ? profileOrId : getBeatBattleEffectivePopulationProfileById(id);
    if (!profile) return false;
    const current = getBeatBattleFixedSkinId(profile, candidates).toLowerCase();
    const currentDetectedSkin = findDetectedBFSkinForPsychCharacter(current);
    const alternatives = [...new Set(candidates.map(value => String(value || "").trim().toLowerCase()))]
      .filter(value => value && value !== current && (!currentDetectedSkin || findDetectedBFSkinForPsychCharacter(value) !== currentDetectedSkin));
    if (!alternatives.length) return false;
    state.turn++;
    if (!state.pendingChange) {
      state.nextNaturalAt = now + getBeatBattlePopulationSkinChangeInterval(id, state.turn);
      const variety = getBeatBattlePopulationHashUnit("skin-mood-v1|" + id + "|variety");
      const roll = getBeatBattlePopulationHashUnit("skin-mood-v1|" + id + "|natural|" + state.turn);
      if (roll >= 0.10 + variety * 0.30) return false;
    }
    const choice = getBeatBattlePopulationHashUnit("skin-mood-v1|" + id + "|choice|" + state.turn);
    const nextSkin = alternatives[Math.min(alternatives.length - 1, Math.floor(choice * alternatives.length))];
    records.skinById[id] = nextSkin;
    profile.characterId = nextSkin;
    state.pendingChange = "";
    state.cooldownUntil = now + getBeatBattlePopulationSkinChangeInterval(id, state.turn, "cooldown");
    state.nextNaturalAt = Math.max(state.cooldownUntil, now + getBeatBattlePopulationSkinChangeInterval(id, state.turn));
    invalidateBeatBattleRankingCache();
     
     
    return true;
  }

function getBeatBattlePopulationSocialSongDuration(song) {
    return Math.max(10000, Math.min(3600000,
      Number(song && song.beatBattleLightweightChart && song.beatBattleLightweightChart.durationMs) || 120000
    ));
  }

function* iterateBeatBattlePopulationSocialPlayerIds(count, usedIds, now = Date.now()) {
    const used = usedIds instanceof Set ? usedIds : new Set();
    const picked = [];
    const tryId = id => {
      if (!id || used.has(id) || picked.includes(id) || !isBeatBattlePopulationAvailableForMatch(id, now)) return false;
      picked.push(id);
      return true;
    };
    const randomTries = Math.max(120, count * 180);
    for (let i = 0; i < randomTries && picked.length < count; i++) {
      tryId(String(1 + Math.floor(Math.random() * BEAT_BATTLE_POPULATION_SIZE)).padStart(5, "0"));
      yield;
    }
    if (picked.length < count) {
      const start = Math.floor(Math.random() * BEAT_BATTLE_POPULATION_SIZE);
      for (let offset = 0; offset < BEAT_BATTLE_POPULATION_SIZE && picked.length < count; offset++) {
        tryId(String(((start + offset) % BEAT_BATTLE_POPULATION_SIZE) + 1).padStart(5, "0"));
        yield;
      }
    }
    return picked;
  }

async function pickBeatBattlePopulationSocialPlayerIds(count, usedIds, now = Date.now()) {
    return runBeatBattleBackgroundProfileSearch(iterateBeatBattlePopulationSocialPlayerIds(count, usedIds, now));
  }

async function updateBeatBattlePopulationSocialSessions(records, now = Date.now()) {
    if (!records || !records.background) return;
    const bg = records.background;
    normalizeBeatBattlePopulationSocialSessions(bg, now);

    const used = new Set();
    let deadline = getBeatBattleBackgroundWorkDeadline();
    for (const match of bg.liveMatches || []) {
      if (performance.now() >= deadline) {
        await yieldBeatBattleBackgroundWork();
        deadline = getBeatBattleBackgroundWorkDeadline();
      }
      if (!match) continue;
      used.add(String(match.aId || ""));
      used.add(String(match.bId || ""));
    }
    for (const match of bg.bossMatches || []) {
      for (const id of match && Array.isArray(match.playerIds) ? match.playerIds : []) used.add(String(id));
      if (performance.now() >= deadline) {
        await yieldBeatBattleBackgroundWork();
        deadline = getBeatBattleBackgroundWorkDeadline();
      }
    }
    for (const match of bg.privateMatches || []) {
      if (!match) continue;
      used.add(String(match.aId || ""));
      used.add(String(match.bId || ""));
      if (performance.now() >= deadline) {
        await yieldBeatBattleBackgroundWork();
        deadline = getBeatBattleBackgroundWorkDeadline();
      }
    }

    const eventState = PKEBossBattle.getEventState(now);
    if (eventState.active) {
      const weekly = PKEBossBattle.selectWeeklySongs(getBeatBattleBossSongDescriptors(), now);
      if (weekly && weekly.ok && Array.isArray(weekly.songs) && weekly.songs.length) {
        while (bg.bossMatches.length < BEAT_BATTLE_BACKGROUND_BOSS_MATCH_TARGET) {
          const playerIds = await pickBeatBattlePopulationSocialPlayerIds(3, used, now);
          if (playerIds.length < 3) break;
          if (!playerIds.every(id => isBeatBattlePopulationAvailableForMatch(id, now))) continue;
          const songDescriptor = weekly.songs[Math.min(weekly.songs.length - 1, Math.floor(Math.random() * weekly.songs.length))];
          const song = findBeatBattleDiscoveredSongByFolder(songDescriptor && songDescriptor.id);
          const duration = getBeatBattlePopulationSocialSongDuration(song);
          const backfill = Math.min(12000, Math.floor(duration * 0.1));
          const startedAt = now - Math.floor(Math.random() * (backfill + 1));
          const serial = Math.max(0, Math.floor(Number(bg.socialSerial) || 0)) + 1;
          bg.socialSerial = serial;
          bg.bossMatches.push({
            id: "boss-pop-" + serial + "-" + now,
            playerIds,
            weekKey: String(eventState.weekKey || ""),
            songFolder: String(songDescriptor && songDescriptor.id || ""),
            songName: String(songDescriptor && songDescriptor.name || "Song"),
            songStars: Math.floor(Number(songDescriptor && songDescriptor.stars) || 0),
            startedAt,
            endsAt: startedAt + duration
          });
          playerIds.forEach(id => used.add(id));
          if (performance.now() >= deadline) {
            await yieldBeatBattleBackgroundWork();
            deadline = getBeatBattleBackgroundWorkDeadline();
          }
        }
      }
    }

    const privateSongs = (discoveredFreeplaySongs || []).filter(song => song && song.folderName && song.chartFile && song.instFile);
    if (privateSongs.length) {
      while (bg.privateMatches.length < BEAT_BATTLE_BACKGROUND_PRIVATE_MATCH_TARGET) {
        const playerIds = await pickBeatBattlePopulationSocialPlayerIds(2, used, now);
        if (playerIds.length < 2) break;
        if (!playerIds.every(id => isBeatBattlePopulationAvailableForMatch(id, now))) continue;
        const song = privateSongs[Math.min(privateSongs.length - 1, Math.floor(Math.random() * privateSongs.length))];
        const duration = getBeatBattlePopulationSocialSongDuration(song);
        const backfill = Math.min(12000, Math.floor(duration * 0.1));
        const startedAt = now - Math.floor(Math.random() * (backfill + 1));
        const serial = Math.max(0, Math.floor(Number(bg.socialSerial) || 0)) + 1;
        bg.socialSerial = serial;
        bg.privateMatches.push({
          id: "private-pop-" + serial + "-" + now,
          aId: playerIds[0],
          bId: playerIds[1],
          songFolder: String(song.folderName || ""),
          songName: String(song.displayName || song.folderName || "Song"),
          songStars: getBeatBattleSongStars(song) || 0,
          startedAt,
          endsAt: startedAt + duration
        });
        playerIds.forEach(id => used.add(id));
        if (performance.now() >= deadline) {
          await yieldBeatBattleBackgroundWork();
          deadline = getBeatBattleBackgroundWorkDeadline();
        }
      }
    }
  }

function normalizeBeatBattlePopulationSpectators(bg, now = Date.now()) {
    if (!bg.spectatorsById || typeof bg.spectatorsById !== "object" || Array.isArray(bg.spectatorsById)) bg.spectatorsById = {};
    for (const [id, state] of Object.entries(bg.spectatorsById)) {
      if (!/^\d{5}$/.test(id) || Number(id) < 1 || Number(id) > BEAT_BATTLE_POPULATION_SIZE ||
          !state || typeof state !== "object" || Array.isArray(state)) {
        delete bg.spectatorsById[id];
        continue;
      }
      state.matchId = typeof state.matchId === "string" ? state.matchId : "";
      state.nextActionAt = Number.isFinite(Number(state.nextActionAt))
        ? Math.max(now - 1, Math.min(now + 120000, Number(state.nextActionAt))) : now;
      state.turn = Math.max(0, Math.min(1000000, Math.floor(Number(state.turn) || 0)));
    }
    bg.spectatorScanCursor = Math.max(0, Math.floor(Number(bg.spectatorScanCursor) || 0)) % BEAT_BATTLE_POPULATION_SIZE;
  }

function reserveBeatBattleViewerParticipants(match) {
    if (!match) return;
    for (const rawId of [match.aId, match.bId]) {
      const id = String(rawId || "");
      if (!id) continue;
      beatBattleViewerReleasedParticipantIds.delete(id);
      beatBattleViewerReservedParticipantIds.add(id);
    }
    beatBattleViewerIndexRevision++;
  }

function releaseBeatBattleViewerParticipants(match) {
    if (!match) return;
    for (const rawId of [match.aId, match.bId]) {
      const id = String(rawId || "");
      if (!id) continue;
      beatBattleViewerReservedParticipantIds.delete(id);
      beatBattleViewerReleasedParticipantIds.add(id);
    }
    beatBattleViewerIndexRevision++;
  }

function* iterateBeatBattleViewerIndex(now = Date.now()) {
    const records = beatBattlePopulationRecords;
    const revision = beatBattleViewerIndexRevision;
    const rooms = new Map();
    const idsByRoom = new Map();
    const participantIds = new Set();
    const invalidStates = [];
    for (const match of records && records.background.liveMatches || []) {
      if (match) {
        participantIds.add(String(match.aId));
        participantIds.add(String(match.bId));
        if (Number(match.endsAt) > now) rooms.set(String(match.id), match);
      }
      yield;
      if (revision !== beatBattleViewerIndexRevision) return false;
    }
    if (records && beatBattlePlayerLiveRoomActive && beatBattlePopulationOpponent && !beatBattleSpectatorMode && !replayMode) {
      participantIds.add(BEAT_BATTLE_PLAYER_ID);
      participantIds.add(String(beatBattlePopulationOpponent.id));
      if (beatBattleLocalAudienceRoom) rooms.set(beatBattleLocalAudienceRoom.id, beatBattleLocalAudienceRoom);
    }
    if (records && beatBattleSpectatorMode && !beatBattleSpectatorResultHandled && !beatBattlePopulationReplayMode && !replayMode) {
      for (const profile of [beatBattleSpectatorPlayerProfile, beatBattleSpectatorOpponentProfile]) {
        if (profile) participantIds.add(String(profile.id));
      }
      if (beatBattleLocalAudienceRoom && beatBattleLocalAudienceRoom.kind === "spectator") {
        rooms.set(beatBattleLocalAudienceRoom.id, beatBattleLocalAudienceRoom);
      }
    }
    const states = records && records.background.spectatorsById || {};
    for (const id of Object.keys(states)) {
      const state = states[id];
      if (state && state.matchId) {
        const room = rooms.get(state.matchId);
        if (!room || participantIds.has(id) || getBeatBattlePopulationBasePresenceStatus(id, now) !== "ACTIVE" ||
            getBeatBattlePopulationPracticeSession(id, now, participantIds)) {
          invalidStates.push(state);
        } else {
          let viewers = idsByRoom.get(state.matchId);
          if (!viewers) idsByRoom.set(state.matchId, viewers = new Set());
          viewers.add(id);
        }
      }
      yield;
      if (revision !== beatBattleViewerIndexRevision) return false;
    }
     
     
    if (revision !== beatBattleViewerIndexRevision || records !== beatBattlePopulationRecords) return false;
    for (const state of invalidStates) {
      state.matchId = "";
      state.nextActionAt = now + 15000;
    }
    beatBattleViewerRooms = rooms;
    beatBattleViewerIdsByRoom = idsByRoom;
    beatBattleViewerParticipantIds = participantIds;
    beatBattleViewerReservedParticipantIds.clear();
    beatBattleViewerReleasedParticipantIds.clear();
    beatBattleViewerIndexRevision++;
    return true;
  }

function rebuildBeatBattleViewerIndex(now = Date.now()) {
    const iterator = iterateBeatBattleViewerIndex(now);
    let step = iterator.next();
    while (!step.done) step = iterator.next();
    return step.value;
  }

async function rebuildBeatBattleViewerIndexInBackground(now = Date.now()) {
    for (;;) {
      const iterator = iterateBeatBattleViewerIndex(now);
      let deadline = getBeatBattleBackgroundWorkDeadline();
      for (;;) {
        const step = iterator.next();
        if (step.done) {
          if (step.value) return;
          break;
        }
        if (performance.now() >= deadline) {
          await yieldBeatBattleBackgroundWork();
          deadline = getBeatBattleBackgroundWorkDeadline();
        }
      }
      await yieldBeatBattleBackgroundWork();
    }
  }

function getBeatBattlePopulationWatchedMatch(playerId, now = Date.now()) {
    const records = loadBeatBattlePopulationRecords();
    const state = records.background.spectatorsById[String(playerId || "")];
    const room = state && beatBattleViewerRooms.get(state.matchId);
    if (!room || (room !== beatBattleLocalAudienceRoom && Number(room.endsAt) <= now)) return null;
    return room;
  }

function setBeatBattlePopulationWatchedMatch(playerId, room, now = Date.now()) {
    const id = String(playerId);
    if (room && getBeatBattlePopulationPracticeSession(id, now)) return;
    const states = loadBeatBattlePopulationRecords().background.spectatorsById;
    const state = states[id] || {matchId: "", nextActionAt: now, turn: 0};
    const oldViewers = beatBattleViewerIdsByRoom.get(state.matchId);
    if (oldViewers) {
      oldViewers.delete(id);
      if (!oldViewers.size) beatBattleViewerIdsByRoom.delete(state.matchId);
    }
    state.turn++;
    state.matchId = room ? String(room.id) : "";
    const dwell = getBeatBattlePopulationHashUnit("spectator-v1|" + id + "|" + state.turn + "|dwell|" + Math.floor(now / 5000));
    state.nextActionAt = now + (room ? 20000 + Math.floor(dwell * 70000) : 10000 + Math.floor(dwell * 45000));
    states[id] = state;
    beatBattleViewerIndexRevision++;
    if (room) {
      let viewers = beatBattleViewerIdsByRoom.get(state.matchId);
      if (!viewers) beatBattleViewerIdsByRoom.set(state.matchId, viewers = new Set());
      viewers.add(id);
    }
  }

function releaseBeatBattleLiveViewers(matchId, now = Date.now()) {
    const id = String(matchId || "");
    beatBattleViewerIndexRevision++;
    const viewers = beatBattleViewerIdsByRoom.get(id);
    if (viewers) {

      for (const playerId of [...viewers]) setBeatBattlePopulationWatchedMatch(playerId, null, now);
    }
    beatBattleViewerIdsByRoom.delete(id);
    beatBattleViewerRooms.delete(id);
  }

function chooseBeatBattleSpectatorRoom(playerId, rooms, previousId = "", now = Date.now(), turn = 0) {
    if (!rooms.length) return null;
    const unit = getBeatBattlePopulationHashUnit("spectator-v1|" + playerId + "|" + turn + "|room|" + Math.floor(now / 5000));
    const start = Math.min(rooms.length - 1, Math.floor(unit * rooms.length));
    for (let offset = 0; offset < rooms.length; offset++) {
      const room = rooms[(start + offset) % rooms.length];
      if (String(room.id) === String(previousId) || String(room.aId) === String(playerId) || String(room.bId) === String(playerId)) continue;

      if (beatBattleViewerRooms.get(String(room.id)) !== room) continue;
      if (room !== beatBattleLocalAudienceRoom && Number(room.endsAt) <= now) continue;
      return room;
    }
    return null;
  }

async function updateBeatBattlePopulationSpectators(records, now = Date.now()) {


    await rebuildBeatBattleViewerIndexInBackground(now);
    let workDeadline = getBeatBattleBackgroundWorkDeadline();
    const bg = records.background;
    const rooms = [...beatBattleViewerRooms.values()];
    const skinCandidates = collectBeatBattlePopulationSkinCandidateIds();
    let processed = 0;
    for (const id of Object.keys(bg.practiceById)) {
      if (!getBeatBattlePopulationPracticeSession(id, now)) delete bg.practiceById[id];
      if (++processed % 128 === 0 || performance.now() >= workDeadline) {
        await yieldBeatBattleBackgroundWork();
        workDeadline = getBeatBattleBackgroundWorkDeadline();
      }
    }
    for (const [id, state] of Object.entries(bg.spectatorsById)) {
      if (state.nextActionAt <= now) {
        if (!state.matchId) {
          delete bg.spectatorsById[id];
        } else {
          const choice = getBeatBattlePopulationHashUnit("spectator-v1|" + id + "|" + state.turn + "|action|" + Math.floor(now / 5000));
          if (choice < 0.4) {
            setBeatBattlePopulationWatchedMatch(id, chooseBeatBattleSpectatorRoom(id, rooms, state.matchId, now, state.turn), now);
          } else if (choice < 0.72) {


            setBeatBattlePopulationWatchedMatch(id, null, now);
          } else {
            setBeatBattlePopulationWatchedMatch(id, beatBattleViewerRooms.get(state.matchId), now);
          }
        }
      }
      if (++processed % 128 === 0 || performance.now() >= workDeadline) {
        await yieldBeatBattleBackgroundWork();
        workDeadline = getBeatBattleBackgroundWorkDeadline();
      }
    }

    let spectatorProposals = null;
    const spectatorProposalBuildStartedAt = performance.now();
    const spectatorProposalPayload = {
            now,
            cursor: bg.spectatorScanCursor,
            scanSize: BEAT_BATTLE_SPECTATOR_SCAN_SIZE,
            populationSize: BEAT_BATTLE_POPULATION_SIZE,
            participantIds: Array.from(beatBattleViewerParticipantIds),
            spectatorIds: Object.keys(bg.spectatorsById),
            practiceIds: Object.keys(bg.practiceById),
            rooms: rooms.map(room => ({
              id: String(room && room.id || ""),
              aId: String(room && room.aId || ""),
              bId: String(room && room.bId || ""),
              endsAt: Number(room && room.endsAt) || 0,
              local: room === beatBattleLocalAudienceRoom
            })),
            sleepBucketMs: BEAT_BATTLE_SLEEP_BUCKET_MS,
            restBucketMs: BEAT_BATTLE_REST_BUCKET_MS,
            sleepProbability: BEAT_BATTLE_SLEEP_PROBABILITY,
            restProbability: BEAT_BATTLE_REST_PROBABILITY
          };
    recordBeatBattlePersistentSpectatorProposalTiming(performance.now() - spectatorProposalBuildStartedAt, null);
    try {
      const proposalResponse = await requestBeatBattleBackgroundCompute(
        "spectator-proposals", [], getBeatBattlePopulationRevision(), {
          timeoutMs: 5000, softTimeout: true, payload: spectatorProposalPayload
        }
      );
      if (proposalResponse && Array.isArray(proposalResponse.result)) spectatorProposals = proposalResponse.result;
    } catch (error) {
      spectatorProposals = null;
    }

    if (spectatorProposals) {
      const spectatorProposalApplyStartedAt = performance.now();
      for (const proposal of spectatorProposals) {
        const id = String(proposal && proposal.id || "");
        if (!id || beatBattleViewerParticipantIds.has(id) || bg.spectatorsById[id] || bg.practiceById[id] || getBeatBattlePopulationBasePresenceStatus(id, now) !== "ACTIVE") continue;
        maybeChangeBeatBattlePopulationSkin(id, skinCandidates, now);
        if (proposal.action === "join") {
          const room = beatBattleViewerRooms.get(String(proposal.roomId || ""));
          if (room && String(room.aId) !== id && String(room.bId) !== id && (room === beatBattleLocalAudienceRoom || Number(room.endsAt) > now)) {
            setBeatBattlePopulationWatchedMatch(id, room, now);
          }
        } else if (proposal.action === "practice") {
          beginBeatBattlePopulationPractice(id, now);
        }
        if (++processed % 128 === 0 || performance.now() >= workDeadline) {
          await yieldBeatBattleBackgroundWork();
          workDeadline = getBeatBattleBackgroundWorkDeadline();
        }
      }
      recordBeatBattlePersistentSpectatorProposalTiming(null, performance.now() - spectatorProposalApplyStartedAt);
    } else {
      for (let offset = 0; offset < BEAT_BATTLE_SPECTATOR_SCAN_SIZE; offset++) {
        const index = (bg.spectatorScanCursor + offset) % BEAT_BATTLE_POPULATION_SIZE;
        const id = String(index + 1).padStart(5, "0");
        const state = bg.spectatorsById[id];
        if (!beatBattleViewerParticipantIds.has(id) && !state && !bg.practiceById[id] && getBeatBattlePopulationBasePresenceStatus(id, now) === "ACTIVE") {
          maybeChangeBeatBattlePopulationSkin(id, skinCandidates, now);
          const roll = getBeatBattlePopulationHashUnit("spectator-v1|" + id + "|join|" + Math.floor(now / 5000));
          if (roll < 0.38) {
            const room = chooseBeatBattleSpectatorRoom(id, rooms, "", now);
            if (room) setBeatBattlePopulationWatchedMatch(id, room, now);
          } else if (roll < 0.46) {
            beginBeatBattlePopulationPractice(id, now);
          }
        }
        if (++processed % 128 === 0 || performance.now() >= workDeadline) {
          await yieldBeatBattleBackgroundWork();
          workDeadline = getBeatBattleBackgroundWorkDeadline();
        }
      }
    }
    bg.spectatorScanCursor = (bg.spectatorScanCursor + BEAT_BATTLE_SPECTATOR_SCAN_SIZE) % BEAT_BATTLE_POPULATION_SIZE;
    refreshBeatBattleViewerCountLabels();
  }

function refreshBeatBattleViewerCountLabels(){}

function saveBeatBattlePopulationRecordsFromBackground(){native.dirty=true;}

function ensureBeatBattleTitleState(records = null) {
    const target = records && typeof records === "object" ? records : loadBeatBattlePopulationRecords();
    if (!target.titleState || typeof target.titleState !== "object" || Array.isArray(target.titleState)) target.titleState = {};
    if (!target.titleState.peakRpById || typeof target.titleState.peakRpById !== "object" || Array.isArray(target.titleState.peakRpById)) target.titleState.peakRpById = {};
    if (!target.titleState.equippedById || typeof target.titleState.equippedById !== "object" || Array.isArray(target.titleState.equippedById)) target.titleState.equippedById = {};
    if (!target.titleState.earnedById || typeof target.titleState.earnedById !== "object" || Array.isArray(target.titleState.earnedById)) target.titleState.earnedById = {};
    return target.titleState;
  }

function getBeatBattleTitlePeakRp(profile) {
    if (!profile || !PkeBeatBattleTitles) return 0;
    const state = ensureBeatBattleTitleState();
    const id = String(profile.id || "");
    return PkeBeatBattleTitles.getPeakRp(state.peakRpById[id], profile.rp);
  }

function recordBeatBattleTitlePeakRp(profile) {
    if (!profile || !PkeBeatBattleTitles) return 0;
    const state = ensureBeatBattleTitleState();
    const id = String(profile.id || "");
    if (!id) return 0;
    const next = PkeBeatBattleTitles.getPeakRp(state.peakRpById[id], profile.rp);
    const previous = Math.max(0, Math.round(Number(state.peakRpById[id]) || 0));
    if (next > previous) state.peakRpById[id] = next;
    return next;
  }

function getBeatBattleTitleStats(profile, rank = 0) {
    if (!profile) return { peakRp: 0, wins: 0, matches: 0, accuracy: 0, rank: 0 };
    const wins = Math.max(0, Math.floor(Number(profile.baseWins) || 0));
    const losses = Math.max(0, Math.floor(Number(profile.baseLosses) || 0));
    const draws = Math.max(0, Math.floor(Number(profile.draws) || 0));
    const matches = Math.max(wins + losses + draws, Math.floor(Number(profile.dynamicMatches) || 0));
    return {
      peakRp: getBeatBattleTitlePeakRp(profile),
      wins,
      matches,
      accuracy: Math.max(0, Math.min(100, Number(profile.accuracy) || 0)),
      rank: Math.max(0, Math.floor(Number(rank) || 0))
    };
  }

function recordBeatBattlePermanentTitleUnlocks(profile) {
    if (!profile || !PkeBeatBattleTitles) return [];
    const id = String(profile.id || "");
    if (!id) return [];
    recordBeatBattleTitlePeakRp(profile);
    const state = ensureBeatBattleTitleState();
    const previous = Array.isArray(state.earnedById[id]) ? state.earnedById[id] : [];
    const next = PkeBeatBattleTitles.mergeEarnedTitleIds(previous, getBeatBattleTitleStats(profile));
    state.earnedById[id] = next;
    return next;
  }

function isBeatBattleBackgroundGameplayActive(){return false;}

function getBeatBattleBackgroundWorkDeadline(){return Infinity;}

function yieldBeatBattleBackgroundWork(){}

function getBeatBattlePopulationIndexFromId(id) {
    const numeric = Math.floor(Number(String(id || "").replace(/\D/g, ""))) || 0;
    if (numeric < 1 || numeric > BEAT_BATTLE_POPULATION_SIZE) return -1;
    return numeric - 1;
  }

function getBeatBattlePopulationLeagueEntry(profile, create = false) {
    if (!profile) return null;
    const records = loadBeatBattlePopulationRecords();
    let entry = records.leagueById[profile.id];
    if ((!entry || typeof entry !== "object") && create) {
      const initialRp = Math.max(0, Math.round(Number(profile.rp) || 0));
      entry = {
        rp: initialRp,
        wins: 0,
        losses: 0,
        draws: 0,
        matches: 0,
        onlineSongRangeTier: promoteAutomaticSongRange("beginner", 0, initialRp)
      };
      records.leagueById[profile.id] = entry;
    }
    return entry && typeof entry === "object" ? entry : null;
  }

function getBeatBattlePopulationDevelopmentMatches(profile) {
    if (!profile || getBeatBattlePopulationIndexFromId(profile.id) < 0) return 0;
    const records = loadBeatBattlePopulationRecords();
    return Math.max(0, Math.floor(Number(records.developmentById[profile.id]) || 0));
  }

function recordBeatBattlePopulationDevelopmentMatch(profile) {
    if (!profile || getBeatBattlePopulationIndexFromId(profile.id) < 0) return 0;
    const records = loadBeatBattlePopulationRecords();
    const next = getBeatBattlePopulationDevelopmentMatches(profile) + 1;
    records.developmentById[profile.id] = next;
    return next;
  }

function getBeatBattlePopulationOnlineSongRange(profile, persist = false) {
    if (!profile) return "beginner";
    const entry = getBeatBattlePopulationLeagueEntry(profile, false);
    const previous = normalizeSongRange(entry && entry.onlineSongRangeTier);
    const matches = entry
      ? Math.max(0, Math.floor(Number(entry.matches) || 0))
      : Math.max(0, Math.floor(Number(profile.dynamicMatches) || 0));
    const rp = entry
      ? Math.max(0, Number(entry.rp) || Number(profile.rp) || 0)
      : Math.max(0, Number(profile.rp) || 0);
    const promoted = promoteAutomaticSongRange(previous, matches, rp);
    if (persist && entry && entry.onlineSongRangeTier !== promoted) {
      entry.onlineSongRangeTier = promoted;
    }
    return promoted;
  }

function applyBeatBattlePopulationLeagueState(profile) {
    if (!profile) return null;
    const historicalWins = Math.max(0, Math.floor(Number(profile.baseWins) || 0));
    const historicalLosses = Math.max(0, Math.floor(Number(profile.baseLosses) || 0));
    const entry = getBeatBattlePopulationLeagueEntry(profile, false);

    profile.careerWins = historicalWins;
    profile.careerLosses = historicalLosses;
    profile.baseWins = entry ? Math.max(0, Math.floor(Number(entry.wins) || 0)) : 0;
    profile.baseLosses = entry ? Math.max(0, Math.floor(Number(entry.losses) || 0)) : 0;
    profile.draws = entry ? Math.max(0, Math.floor(Number(entry.draws) || 0)) : 0;
    profile.dynamicMatches = entry ? Math.max(0, Math.floor(Number(entry.matches) || 0)) : 0;
    if (entry) {
      profile.rp = Math.max(0, Math.round(Number(entry.rp) || Number(profile.rp) || 0));
    }



    if (getBeatBattlePopulationIndexFromId(profile.id) >= 0) {
      const developmentMatches = getBeatBattlePopulationDevelopmentMatches(profile);
      profile.baseAccuracy = clampPopulationNumber(
        Number(profile.baseAccuracy ?? profile.accuracy) || BEAT_BATTLE_PLAYER_START_ACCURACY,
        60,
        BEAT_BATTLE_POPULATION_SKILL_CAP
      );
      profile.skillCeiling = clampPopulationNumber(
        Number(profile.skillCeiling) || getBeatBattlePopulationSkillCeiling(profile.id, profile.baseAccuracy),
        profile.baseAccuracy,
        BEAT_BATTLE_POPULATION_SKILL_CAP
      );
      profile.developmentMatches = developmentMatches;
      profile.accuracy = getBeatBattlePopulationDevelopedAccuracy(profile, developmentMatches);
    }
    return profile;
  }

function getBeatBattlePlayerDisplayName(){return native.playerName;}

function getBeatBattlePlayerProfile() {
    const records = loadBeatBattlePopulationRecords();
    const profile = {
      id: BEAT_BATTLE_PLAYER_ID,
      numericId: 0,
      name: getBeatBattlePlayerDisplayName(),
      accuracy: Number.isFinite(Number(records.player.accuracy)) &&
        (Number(records.player.accuracy) > 0 || records.player.accuracy === 0)
        ? clampPopulationNumber(records.player.accuracy, 0, 100)
        : BEAT_BATTLE_PLAYER_START_ACCURACY,
      rp: BEAT_BATTLE_PLAYER_START_RP,
      timingBiasMs: 0,
      baseWins: 0,
      baseLosses: 0,
      skinSeed: 0,
      characterId: String(currentBFSkin || ""),
      isHuman: true
    };
    return applyBeatBattlePopulationLeagueState(profile);
  }

function* iterateBeatBattlePopulationOpponentsNear(
    targetRp, targetAccuracy, excludeIds = null, sampleCount = 220, options = null
  ) {
    const exclude = excludeIds instanceof Set ? excludeIds : new Set(excludeIds || []);
    const wantedRp = Math.max(0, Number(targetRp) || BEAT_BATTLE_PLAYER_START_RP);
    const rawTargetAccuracy = Number(targetAccuracy);
    const hasTargetAccuracy = Number.isFinite(rawTargetAccuracy) &&
      (rawTargetAccuracy > 0 || targetAccuracy === 0);
    const wantedAcc = hasTargetAccuracy
      ? clampPopulationNumber(rawTargetAccuracy, 0, 100)
      : BEAT_BATTLE_PLAYER_START_ACCURACY;
    const rpOnly = !!(options && options.rpOnly);
    const accuracyPrimary = !rpOnly && (!options || options.accuracyPrimary !== false);
    const rawMaxAccuracy = options ? options.maxAccuracy : undefined;
    const maxAccuracy = rawMaxAccuracy !== null && rawMaxAccuracy !== undefined &&
      Number.isFinite(Number(rawMaxAccuracy))
      ? clampPopulationNumber(rawMaxAccuracy, 0, 100)
      : null;
    const requiredSongRange = options && options.requiredSongRange
      ? normalizeSongRange(options.requiredSongRange)
      : "";
    const allowHigherSongRange = !!(options && options.allowHigherSongRange);
    const availabilityNow = options && Number.isFinite(Number(options.availabilityNow))
      ? Number(options.availabilityNow)
      : Date.now();
    const shortlist = [];
    const count = Math.max(80, Math.floor(Number(sampleCount) || 220));
    const fullScan = count >= BEAT_BATTLE_POPULATION_SIZE;
    const iterations = fullScan ? BEAT_BATTLE_POPULATION_SIZE : count;

    for (let i = 0; i < iterations; i++) {
      const candidateIndex = fullScan
        ? i
        : Math.floor(Math.random() * BEAT_BATTLE_POPULATION_SIZE);
      const candidate = getBeatBattleEffectivePopulationProfileByIndex(candidateIndex);
      yield;
      if (!candidate || exclude.has(candidate.id)) continue;
      if (!isBeatBattlePopulationAvailableForMatch(candidate.id, availabilityNow)) continue;
      if (requiredSongRange) {
        const candidateSongRange = getBeatBattlePopulationOnlineSongRange(candidate);
        const canUseHigherStandardOpponent = allowHigherSongRange &&
          requiredSongRange === "standard" && candidateSongRange === "all";
        if (candidateSongRange !== requiredSongRange && !canUseHigherStandardOpponent) continue;
      }
      if (maxAccuracy !== null && Number(candidate.accuracy) > maxAccuracy) continue;

      const rpDiff = Math.abs((Number(candidate.rp) || 0) - wantedRp);
      const accDiff = Math.abs((Number(candidate.accuracy) || 0) - wantedAcc);
      shortlist.push({ candidate, rpDiff, accDiff, jitter: Math.random() });
    }

    if (!shortlist.length) return null;

    let pool = shortlist;

    if (rpOnly) {



      pool.sort((a, b) => {
        const scoreA = a.rpDiff + a.jitter * 12;
        const scoreB = b.rpDiff + b.jitter * 12;
        return scoreA - scoreB;
      });
    } else if (accuracyPrimary) {




      const accuracyBands = [0.25, 0.5, 0.75, 1.0, 1.5, 2.5, 4.0, Infinity];
      for (const band of accuracyBands) {
        const withinBand = shortlist.filter(entry => entry.accDiff <= band);
        if (withinBand.length > 0 || band === Infinity) {
          pool = withinBand;
          break;
        }
      }

      pool.sort((a, b) => {
        const scoreA = a.rpDiff + a.accDiff * 80 + a.jitter * 24;
        const scoreB = b.rpDiff + b.accDiff * 80 + b.jitter * 24;
        return scoreA - scoreB;
      });
    } else {


      pool.sort((a, b) => {
        const scoreA = a.rpDiff + a.accDiff * 42 + a.jitter * 35;
        const scoreB = b.rpDiff + b.accDiff * 42 + b.jitter * 35;
        return scoreA - scoreB;
      });
    }

    const poolSize = Math.min(5, pool.length);
    return pool[Math.floor(Math.random() * poolSize)].candidate;
  }

async function collectBeatBattleBackgroundMatchmakingPool() {
    if (Array.isArray(beatBattleBackgroundMatchmakingPool) &&
        beatBattleBackgroundMatchmakingPool.length === BEAT_BATTLE_POPULATION_SIZE) {
      return beatBattleBackgroundMatchmakingPool;
    }
    const pool = new Array(BEAT_BATTLE_POPULATION_SIZE);
    let deadline = getBeatBattleBackgroundWorkDeadline();
    for (let i = 0; i < BEAT_BATTLE_POPULATION_SIZE; i++) {
      const profile = getBeatBattleEffectivePopulationProfileByIndex(i);
      if (profile) profile._beatBattleMatchRange = getBeatBattlePopulationOnlineSongRange(profile);
      pool[i] = profile;
      if (performance.now() >= deadline) {
        await yieldBeatBattleBackgroundWork();
        deadline = getBeatBattleBackgroundWorkDeadline();
      }
    }
    beatBattleBackgroundMatchmakingPool = pool;
    return pool;
  }

async function collectBeatBattleBackgroundAvailableIds(candidatePool, availabilityNow = Date.now()) {
    const pool = Array.isArray(candidatePool) ? candidatePool : [];
    const availableIds = new Set();
    let deadline = getBeatBattleBackgroundWorkDeadline();
    const now = Number.isFinite(Number(availabilityNow)) ? Number(availabilityNow) : Date.now();
    for (let i = 0; i < pool.length; i++) {
      const candidate = pool[i];
      if (candidate && isBeatBattlePopulationAvailableForMatch(candidate.id, now)) {
        availableIds.add(candidate.id);
      }
      if (performance.now() >= deadline) {
        await yieldBeatBattleBackgroundWork();
        deadline = getBeatBattleBackgroundWorkDeadline();
      }
    }
    return availableIds;
  }

async function collectBeatBattleBackgroundAvailableIdsWithWorker(candidatePool, availabilityNow = Date.now()) {
    return collectBeatBattleBackgroundAvailableIds(candidatePool, availabilityNow);
  }

function refreshBeatBattleBackgroundMatchmakingPoolProfile(playerId) {
    if (!Array.isArray(beatBattleBackgroundMatchmakingPool)) return;
    const index = getBeatBattlePopulationIndexFromId(playerId);
    if (index < 0 || index >= beatBattleBackgroundMatchmakingPool.length) return;
    const profile = getBeatBattleEffectivePopulationProfileByIndex(index);
    if (profile) profile._beatBattleMatchRange = getBeatBattlePopulationOnlineSongRange(profile);
    beatBattleBackgroundMatchmakingPool[index] = profile;
  }

function* iterateBeatBattleStableSort(list, compare) {


    if (list.length < 2) return list;
    let source = list;
    let target = new Array(list.length);
    for (let width = 1; width < list.length; width *= 2) {
      for (let start = 0; start < list.length; start += width * 2) {
        const middle = Math.min(start + width, list.length);
        const end = Math.min(start + width * 2, list.length);
        let left = start, right = middle;
        for (let out = start; out < end; out++) {
          target[out] = left < middle &&
            (right >= end || !(compare(source[left], source[right]) > 0))
            ? source[left++] : source[right++];
          if ((out & 7) === 7) yield;
        }
      }
      const previous = source;
      source = target;
      target = previous;
    }
    return source;
  }

async function sortBeatBattleBackgroundProfiles(list, compare) {
    return runBeatBattleBackgroundProfileSearch(iterateBeatBattleStableSort(list, compare));
  }

async function buildBeatBattleBackgroundMatchmakingIndex(candidatePool) {
    const byAccuracy = { beginner: [], standard: [], all: [] };
    const byRp = { beginner: [], standard: [], all: [] };
    const pool = Array.isArray(candidatePool) ? candidatePool : [];


    const admitted = pool.slice();
    let deadline = getBeatBattleBackgroundWorkDeadline();
    for (const candidate of admitted) {
      if (performance.now() >= deadline) {
        await yieldBeatBattleBackgroundWork();
        deadline = getBeatBattleBackgroundWorkDeadline();
      }
      if (!candidate) continue;
      const range = candidate._beatBattleMatchRange || getBeatBattlePopulationOnlineSongRange(candidate);
      if (!byAccuracy[range]) continue;
      byAccuracy[range].push(candidate);
      byRp[range].push(candidate);
    }
    for (const range of ["beginner", "standard", "all"]) {
      byAccuracy[range] = await sortBeatBattleBackgroundProfiles(byAccuracy[range], (a, b) =>
        (Number(a.accuracy) - Number(b.accuracy)) ||
        (Number(a.rp) - Number(b.rp)) ||
        String(a.id).localeCompare(String(b.id))
      );
      byRp[range] = await sortBeatBattleBackgroundProfiles(byRp[range], (a, b) =>
        (Number(a.rp) - Number(b.rp)) ||
        (Number(a.accuracy) - Number(b.accuracy)) ||
        String(a.id).localeCompare(String(b.id))
      );
    }
    return { byAccuracy, byRp, candidatePool: pool };
  }

function buildBeatBattleBackgroundMatchmakingIndexWithWorker(pool){return buildBeatBattleBackgroundMatchmakingIndex(pool);}

function lowerBoundBeatBattleBackground(list, value, selector) {
    let lo = 0;
    let hi = list.length;
    while (lo < hi) {
      const mid = (lo + hi) >> 1;
      if (selector(list[mid]) < value) lo = mid + 1;
      else hi = mid;
    }
    return lo;
  }

function getBeatBattleBackgroundMatchRanges(requiredSongRange, allowHigherSongRange) {
    if (!requiredSongRange) return ["beginner", "standard", "all"];
    if (allowHigherSongRange && requiredSongRange === "standard") return ["standard", "all"];
    return [requiredSongRange];
  }

function pushBeatBattleBackgroundTopCandidate(top, entry, limit = 5) {
    let insertAt = top.length;
    for (let i = 0; i < top.length; i++) {
      if (entry.score < top[i].score) {
        insertAt = i;
        break;
      }
    }
    top.splice(insertAt, 0, entry);
    if (top.length > limit) top.length = limit;
  }

async function findBeatBattleBackgroundOpponentNearFromIndex(
    candidateIndex, targetRp, targetAccuracy, excludeIds, options = null
  ) {
    if (!candidateIndex || !candidateIndex.byAccuracy || !candidateIndex.byRp) return null;
    const exclude = excludeIds instanceof Set ? excludeIds : new Set(excludeIds || []);
    const wantedRp = Math.max(0, Number(targetRp) || BEAT_BATTLE_PLAYER_START_RP);
    const rawTargetAccuracy = Number(targetAccuracy);
    const hasTargetAccuracy = Number.isFinite(rawTargetAccuracy) &&
      (rawTargetAccuracy > 0 || targetAccuracy === 0);
    const wantedAcc = hasTargetAccuracy
      ? clampPopulationNumber(rawTargetAccuracy, 0, 100)
      : BEAT_BATTLE_PLAYER_START_ACCURACY;
    const rpOnly = !!(options && options.rpOnly);
    const accuracyPrimary = !rpOnly && (!options || options.accuracyPrimary !== false);
    const rawMaxAccuracy = options ? options.maxAccuracy : undefined;
    const maxAccuracy = rawMaxAccuracy !== null && rawMaxAccuracy !== undefined &&
      Number.isFinite(Number(rawMaxAccuracy))
      ? clampPopulationNumber(rawMaxAccuracy, 0, 100)
      : null;
    const requiredSongRange = options && options.requiredSongRange
      ? normalizeSongRange(options.requiredSongRange)
      : "";
    const allowHigherSongRange = !!(options && options.allowHigherSongRange);
    const availableIds = options && options.availableIds instanceof Set
      ? options.availableIds
      : null;
    const availabilityNow = options && Number.isFinite(Number(options.availabilityNow))
      ? Number(options.availabilityNow)
      : Date.now();
    const ranges = getBeatBattleBackgroundMatchRanges(requiredSongRange, allowHigherSongRange);
    let deadline = getBeatBattleBackgroundWorkDeadline();
    const isEligible = candidate => {
      if (!candidate || exclude.has(candidate.id)) return false;
      if (availableIds ? !availableIds.has(candidate.id) : !isBeatBattlePopulationAvailableForMatch(candidate.id, availabilityNow)) return false;
      if (maxAccuracy !== null && Number(candidate.accuracy) > maxAccuracy) return false;
      return true;
    };

    if (accuracyPrimary) {
      const accuracyBands = [0.25, 0.5, 0.75, 1.0, 1.5, 2.5, 4.0, Infinity];
      for (const band of accuracyBands) {
        const top = [];
        const minAcc = band === Infinity ? -Infinity : wantedAcc - band;
        const maxAcc = band === Infinity ? Infinity : wantedAcc + band;
        for (const range of ranges) {
          const list = candidateIndex.byAccuracy[range] || [];
          const start = band === Infinity
            ? 0
            : lowerBoundBeatBattleBackground(list, minAcc, candidate => Number(candidate.accuracy) || 0);
          const end = band === Infinity
            ? list.length
            : lowerBoundBeatBattleBackground(list, maxAcc + Number.EPSILON, candidate => Number(candidate.accuracy) || 0);
          for (let i = start; i < end; i++) {
            if (performance.now() >= deadline) {
              await yieldBeatBattleBackgroundWork();
              deadline = getBeatBattleBackgroundWorkDeadline();
            }
            const candidate = list[i];
            if (!isEligible(candidate)) continue;
            const rpDiff = Math.abs((Number(candidate.rp) || 0) - wantedRp);
            const accDiff = Math.abs((Number(candidate.accuracy) || 0) - wantedAcc);
            pushBeatBattleBackgroundTopCandidate(top, {
              candidate,
              score: rpDiff + accDiff * 80 + Math.random() * 24
            });
          }
        }
        if (top.length) return top[Math.floor(Math.random() * top.length)].candidate;
      }
      return null;
    }

    if (rpOnly) {
      const top = [];
      for (const range of ranges) {
        const list = candidateIndex.byRp[range] || [];
        let right = lowerBoundBeatBattleBackground(list, wantedRp, candidate => Number(candidate.rp) || 0);
        let left = right - 1;
        while (left >= 0 || right < list.length) {
          if (performance.now() >= deadline) {
            await yieldBeatBattleBackgroundWork();
            deadline = getBeatBattleBackgroundWorkDeadline();
          }
          const leftDiff = left >= 0
            ? Math.abs((Number(list[left].rp) || 0) - wantedRp)
            : Infinity;
          const rightDiff = right < list.length
            ? Math.abs((Number(list[right].rp) || 0) - wantedRp)
            : Infinity;
          const useLeft = leftDiff <= rightDiff;
          const nextDiff = useLeft ? leftDiff : rightDiff;
          if (top.length >= 5 && nextDiff > top[top.length - 1].score) break;
          const candidate = useLeft ? list[left--] : list[right++];
          if (!isEligible(candidate)) continue;
          pushBeatBattleBackgroundTopCandidate(top, {
            candidate,
            score: nextDiff + Math.random() * 12
          });
        }
      }
      return top.length ? top[Math.floor(Math.random() * top.length)].candidate : null;
    }

    return findBeatBattleBackgroundOpponentNearFromPool(
      candidateIndex.candidatePool || [], targetRp, targetAccuracy, exclude, options);
  }

async function findBeatBattleBackgroundOpponentNearFromPool(
    candidatePool, targetRp, targetAccuracy, excludeIds, options = null
  ) {
    const pool = Array.isArray(candidatePool) ? candidatePool : [];
    if (!pool.length) return null;
    const exclude = excludeIds instanceof Set ? excludeIds : new Set(excludeIds || []);
    const wantedRp = Math.max(0, Number(targetRp) || BEAT_BATTLE_PLAYER_START_RP);
    const rawTargetAccuracy = Number(targetAccuracy);
    const hasTargetAccuracy = Number.isFinite(rawTargetAccuracy) &&
      (rawTargetAccuracy > 0 || targetAccuracy === 0);
    const wantedAcc = hasTargetAccuracy
      ? clampPopulationNumber(rawTargetAccuracy, 0, 100)
      : BEAT_BATTLE_PLAYER_START_ACCURACY;
    const rpOnly = !!(options && options.rpOnly);
    const accuracyPrimary = !rpOnly && (!options || options.accuracyPrimary !== false);
    const rawMaxAccuracy = options ? options.maxAccuracy : undefined;
    const maxAccuracy = rawMaxAccuracy !== null && rawMaxAccuracy !== undefined &&
      Number.isFinite(Number(rawMaxAccuracy))
      ? clampPopulationNumber(rawMaxAccuracy, 0, 100)
      : null;
    const requiredSongRange = options && options.requiredSongRange
      ? normalizeSongRange(options.requiredSongRange)
      : "";
    const allowHigherSongRange = !!(options && options.allowHigherSongRange);
    const availableIds = options && options.availableIds instanceof Set
      ? options.availableIds
      : null;
    const availabilityNow = options && Number.isFinite(Number(options.availabilityNow))
      ? Number(options.availabilityNow)
      : Date.now();
    const accuracyBands = [0.25, 0.5, 0.75, 1.0, 1.5, 2.5, 4.0, Infinity];
    let bestBandIndex = accuracyBands.length - 1;
    let top = [];
    let deadline = getBeatBattleBackgroundWorkDeadline();

    for (let i = 0; i < pool.length; i++) {
      if (performance.now() >= deadline) {
        await yieldBeatBattleBackgroundWork();
        deadline = getBeatBattleBackgroundWorkDeadline();
      }
      const candidate = pool[i];
      if (!candidate || exclude.has(candidate.id) ||
          (availableIds ? !availableIds.has(candidate.id) : !isBeatBattlePopulationAvailableForMatch(candidate.id, availabilityNow))) {
        continue;
      }
      if (requiredSongRange) {
        const candidateSongRange = candidate._beatBattleMatchRange || getBeatBattlePopulationOnlineSongRange(candidate);
        const canUseHigherStandardOpponent = allowHigherSongRange &&
          requiredSongRange === "standard" && candidateSongRange === "all";
        if (candidateSongRange !== requiredSongRange && !canUseHigherStandardOpponent) {
          continue;
        }
      }
      if (maxAccuracy !== null && Number(candidate.accuracy) > maxAccuracy) continue;

      const rpDiff = Math.abs((Number(candidate.rp) || 0) - wantedRp);
      const accDiff = Math.abs((Number(candidate.accuracy) || 0) - wantedAcc);
      const jitter = Math.random();

      if (rpOnly) {
        pushBeatBattleBackgroundTopCandidate(top, {
          candidate,
          score: rpDiff + jitter * 12
        });
      } else if (accuracyPrimary) {
        let bandIndex = accuracyBands.length - 1;
        for (let b = 0; b < accuracyBands.length; b++) {
          if (accDiff <= accuracyBands[b]) {
            bandIndex = b;
            break;
          }
        }
        if (bandIndex < bestBandIndex) {
          bestBandIndex = bandIndex;
          top = [];
        }
        if (bandIndex === bestBandIndex) {
          pushBeatBattleBackgroundTopCandidate(top, {
            candidate,
            score: rpDiff + accDiff * 80 + jitter * 24
          });
        }
      } else {
        pushBeatBattleBackgroundTopCandidate(top, {
          candidate,
          score: rpDiff + accDiff * 42 + jitter * 35
        });
      }

    }

    return top.length ? top[Math.floor(Math.random() * top.length)].candidate : null;
  }

async function findBeatBattleBackgroundOpponentNear(targetRp, targetAccuracy, excludeIds, sampleCount, options = null) {
    if (options && options.candidateIndex) {
      return findBeatBattleBackgroundOpponentNearFromIndex(
        options.candidateIndex, targetRp, targetAccuracy, excludeIds, options);
    }
    if (options && Array.isArray(options.candidatePool)) {
      return findBeatBattleBackgroundOpponentNearFromPool(
        options.candidatePool, targetRp, targetAccuracy, excludeIds, options);
    }
    return runBeatBattleBackgroundProfileSearch(
      iterateBeatBattlePopulationOpponentsNear(targetRp, targetAccuracy, excludeIds, sampleCount, options));
  }

function getBeatBattleEffectivePopulationProfileByIndex(index) {
    return applyBeatBattlePopulationLeagueState(createBeatBattlePopulationProfile(index));
  }

function getBeatBattleEffectivePopulationProfileById(id) {
    const index = getBeatBattlePopulationIndexFromId(id);
    if (index < 0) return null;
    const profile = getBeatBattleEffectivePopulationProfileByIndex(index);
    const records = loadBeatBattlePopulationRecords();
    const savedSkin = String(records.skinById[profile.id] || "").trim();
    if (savedSkin) profile.characterId = savedSkin;
    return profile;
  }

function getBeatBattleBackgroundSimulationProfileById(id) {
    const index = getBeatBattlePopulationIndexFromId(id);
    if (index >= 0 && Array.isArray(beatBattleBackgroundMatchmakingPool) &&
        beatBattleBackgroundMatchmakingPool.length === BEAT_BATTLE_POPULATION_SIZE) {
      const cached = beatBattleBackgroundMatchmakingPool[index];
      if (cached && String(cached.id) === String(id)) return cached;
    }
    return getBeatBattleEffectivePopulationProfileById(id);
  }

function applyBeatBattlePopulationLeagueResult(profileA, profileB, scoreA, scoreB) {
    if (!profileA || !profileB || profileA.id === profileB.id) return false;

    const freshA = getBeatBattleEffectivePopulationProfileById(profileA.id) || profileA;
    const freshB = getBeatBattleEffectivePopulationProfileById(profileB.id) || profileB;
    const entryA = getBeatBattlePopulationLeagueEntry(freshA, true);
    const entryB = getBeatBattlePopulationLeagueEntry(freshB, true);

    recordBeatBattleTitlePeakRp(freshA);
    recordBeatBattleTitlePeakRp(freshB);
    const rpA = Math.max(0, Number(entryA.rp) || Number(freshA.rp) || 0);
    const rpB = Math.max(0, Number(entryB.rp) || Number(freshB.rp) || 0);
    const expectedA = 1 / (1 + Math.pow(10, (rpB - rpA) / 420));
    const expectedB = 1 - expectedA;

    let actualA = 0.5;
    let actualB = 0.5;
    if (Number(scoreA) > Number(scoreB)) { actualA = 1; actualB = 0; }
    else if (Number(scoreA) < Number(scoreB)) { actualA = 0; actualB = 1; }




    const isDraw = Number(scoreA) === Number(scoreB);
    const k = BEAT_BATTLE_RP_K_FACTOR;
    const deltaA = isDraw
      ? BEAT_BATTLE_RP_DRAW_BONUS
      : Math.max(
          -BEAT_BATTLE_RP_MAX_DELTA,
          Math.min(BEAT_BATTLE_RP_MAX_DELTA, Math.round(k * (actualA - expectedA)))
        );
    const deltaB = isDraw
      ? BEAT_BATTLE_RP_DRAW_BONUS
      : Math.max(
          -BEAT_BATTLE_RP_MAX_DELTA,
          Math.min(BEAT_BATTLE_RP_MAX_DELTA, Math.round(k * (actualB - expectedB)))
        );

    entryA.rp = Math.max(100, Math.round(rpA + deltaA));
    entryB.rp = Math.max(100, Math.round(rpB + deltaB));
    recordBeatBattleTitlePeakRp({ id: freshA.id, rp: entryA.rp });
    recordBeatBattleTitlePeakRp({ id: freshB.id, rp: entryB.rp });
    entryA.matches = Math.max(0, Math.floor(Number(entryA.matches) || 0)) + 1;
    entryB.matches = Math.max(0, Math.floor(Number(entryB.matches) || 0)) + 1;



    recordBeatBattlePopulationDevelopmentMatch(freshA);
    recordBeatBattlePopulationDevelopmentMatch(freshB);



    entryA.onlineSongRangeTier = promoteAutomaticSongRange(
      entryA.onlineSongRangeTier, entryA.matches, entryA.rp);
    entryB.onlineSongRangeTier = promoteAutomaticSongRange(
      entryB.onlineSongRangeTier, entryB.matches, entryB.rp);

    if (actualA === 1) {
      entryA.wins = Math.max(0, Math.floor(Number(entryA.wins) || 0)) + 1;
      entryB.losses = Math.max(0, Math.floor(Number(entryB.losses) || 0)) + 1;
    } else if (actualB === 1) {
      entryB.wins = Math.max(0, Math.floor(Number(entryB.wins) || 0)) + 1;
      entryA.losses = Math.max(0, Math.floor(Number(entryA.losses) || 0)) + 1;
    } else {
      entryA.draws = Math.max(0, Math.floor(Number(entryA.draws) || 0)) + 1;
      entryB.draws = Math.max(0, Math.floor(Number(entryB.draws) || 0)) + 1;
    }

    recordBeatBattlePermanentTitleUnlocks({
      ...freshA,
      rp: entryA.rp,
      baseWins: entryA.wins,
      baseLosses: entryA.losses,
      draws: entryA.draws,
      dynamicMatches: entryA.matches
    });
    recordBeatBattlePermanentTitleUnlocks({
      ...freshB,
      rp: entryB.rp,
      baseWins: entryB.wins,
      baseLosses: entryB.losses,
      draws: entryB.draws,
      dynamicMatches: entryB.matches
    });

    recordBeatBattlePopulationSkinMoodResult(freshA, actualA);
    recordBeatBattlePopulationSkinMoodResult(freshB, actualB);
    queueBeatBattlePopulationRankingJournalSnapshot([freshA.id, freshB.id]);
    refreshBeatBattleBackgroundMatchmakingPoolProfile(freshA.id);
    refreshBeatBattleBackgroundMatchmakingPoolProfile(freshB.id);
    invalidateBeatBattleRankingCache();

    return {
      changed: true,
      deltaA,
      deltaB,
      rpA: entryA.rp,
      rpB: entryB.rp,
      actualA,
      actualB
    };
  }

function getBeatBattleBackgroundResultScores(match) {
    const a = getBeatBattleEffectivePopulationProfileById(match && match.aId);
    const b = getBeatBattleEffectivePopulationProfileById(match && match.bId);
    if (!a || !b) return { a, b, scoreA: 0, scoreB: 0 };

    const key = String(match && match.id || (a.id + "-" + b.id));

    const song = getBeatBattleBackgroundMatchSong(match);
    const chartData = song && song.beatBattleLightweightChart;
    const bpm = chartData && Number(chartData.baseBpm) > 0
      ? Number(chartData.baseBpm)
      : (70 + getBeatBattlePopulationHashUnit("background|" + key + "|pseudo-bpm") * 170);
    const rapidRatioA = chartData
      ? clampPopulationNumber(chartData.playerRapidRatio, 0, 1)
      : (0.04 + getBeatBattlePopulationHashUnit("background|" + key + "|pseudo-rapid-a") * 0.42);
    const rapidRatioB = chartData
      ? clampPopulationNumber(chartData.opponentRapidRatio, 0, 1)
      : (0.04 + getBeatBattlePopulationHashUnit("background|" + key + "|pseudo-rapid-b") * 0.42);

    const basePerfA = getBeatBattleProfileAverageTargetAccuracy(
      a, bpm, rapidRatioA, getBeatBattleLiveHumanTimingVersion(match)
    );
    const basePerfB = getBeatBattleProfileAverageTargetAccuracy(
      b, bpm, rapidRatioB, getBeatBattleLiveHumanTimingVersion(match)
    );
    let perfA;
    let perfB;
    if (getBeatBattleMatchRunFormVersion(match) >= 1) {
      perfA = getBeatBattleLiveRunAccuracyTarget(match, a, "a", chartData, basePerfA);
      perfB = getBeatBattleLiveRunAccuracyTarget(match, b, "b", chartData, basePerfB);
    } else {


      const noiseA =
        (getBeatBattlePopulationHashUnit("background|" + key + "|a1") +
         getBeatBattlePopulationHashUnit("background|" + key + "|a2") - 1) * 4.3;
      const noiseB =
        (getBeatBattlePopulationHashUnit("background|" + key + "|b1") +
         getBeatBattlePopulationHashUnit("background|" + key + "|b2") - 1) * 4.3;
      perfA = clampPopulationNumber(basePerfA + noiseA, 60, BEAT_BATTLE_POPULATION_TARGET_ACC_CAP);
      perfB = clampPopulationNumber(basePerfB + noiseB, 60, BEAT_BATTLE_POPULATION_TARGET_ACC_CAP);
    }
    const scoreA = Math.round(perfA * 1000);
    const scoreB = Math.round(perfB * 1000);
    return { a, b, scoreA, scoreB };
  }

function getBeatBattleBackgroundResultScoresWithWorker(match){return getBeatBattleBackgroundResultScores(match);}

function createBeatBattleLiveNoteSideState() {
    return {
      rawScore: 0,
      totalPlayed: 0,
      weightedHits: 0,
      sick: 0,
      good: 0,
      bad: 0,
      miss: 0,
      combo: 0,
      maxCombo: 0,
      lastPressTimes: [null, null, null, null],
      lastNoteTimes: [null, null, null, null]
    };
  }

function normalizeBeatBattleLiveNoteSideState(value) {
    const state = value && typeof value === "object" ? value : createBeatBattleLiveNoteSideState();
    let owned = beatBattleLiveNoteArrayOwnership.get(state);
    if (!owned) {
      owned = { planTuples: [null, null, null, null] };
      beatBattleLiveNoteArrayOwnership.set(state, owned);
    }
    for (const key of ["rawScore", "totalPlayed", "weightedHits", "sick", "good", "bad", "miss", "combo", "maxCombo"]) {
      if (!Number.isFinite(Number(state[key]))) state[key] = 0;
    }
    for (const key of ["lastPressTimes", "lastNoteTimes"]) {
      const saved = Array.isArray(state[key]) ? state[key] : [];
      const output = owned[key] === saved && saved.length === 4 ? saved : new Array(4);
      for (let lane = 0; lane < 4; lane++) {
        const item = saved[lane];
        output[lane] = item !== null && item !== undefined && Number.isFinite(Number(item)) ? Number(item) : null;
      }
      state[key] = output;
      owned[key] = output;
    }
    if (Array.isArray(state.lastPlans)) {
      const saved = state.lastPlans;
      const output = owned.lastPlans === saved && saved.length === 4 ? saved : new Array(4);
      for (let lane = 0; lane < 4; lane++) {
        const plan = saved[lane];
        if (Array.isArray(plan) && plan.length === 3 && plan[0] !== null && plan[1] !== null &&
          Number.isFinite(Number(plan[0])) && Number.isFinite(Number(plan[1]))) {
          const time = Number(plan[0]), offset = Number(plan[1]), miss = !!plan[2];
          if (owned.lastPlans === saved && owned.planTuples[lane] === plan) {
            plan[0] = time; plan[1] = offset; plan[2] = miss;
            output[lane] = plan;
          } else output[lane] = [time, offset, miss];
        } else output[lane] = null;
        owned.planTuples[lane] = output[lane];
      }
      state.lastPlans = output;
      owned.lastPlans = output;
    }
    return state;
  }

function getBeatBattleLiveNoteNormalizedScore(state, noteCount) {
    const maxRaw = Math.max(1, Math.floor(Number(noteCount) || 0) * 350);
    return Math.round(
      Math.max(0, Math.min(100000, (Math.max(0, Number(state && state.rawScore) || 0) / maxRaw) * 100000))
    );
  }

function getBeatBattleLiveRandomContext(match, profile, sideKey, songData) {
    const runtimeSongName = String(songData && songData.runtimeSongName || "Song");
    const profileId = String(profile && profile.id || (sideKey === "a" ? "" : "manual"));
    const cacheKey = String(sideKey || "a") + "|" + profileId + "|" + runtimeSongName;
    let perMatch = null;
    if (match && typeof match === "object") {
      perMatch = beatBattleLiveRandomContextCache.get(match);
      if (!perMatch) {
        perMatch = new Map();
        beatBattleLiveRandomContextCache.set(match, perMatch);
      }
      if (perMatch.has(cacheKey)) return perMatch.get(cacheKey);
    }
    const seed = getBeatBattleSpectatorSeed(String(match && match.id || "match"), sideKey === "a" ? "bf" : "dad");
    const context = createBeatBattleRandomPrefixContext(
      sideKey === "a"
        ? "beat-battle-spectator-player|" + String(seed >>> 0) + "|" + profileId + "|" + runtimeSongName + "|"
        : "beat-battle-cpu|" + String(seed >>> 0) + "|" + runtimeSongName + "|",
      sideKey === "a"
        ? "beat-battle-spectator-player-style|" + String(seed >>> 0) + "|" + profileId + "|" + runtimeSongName + "|"
        : "beat-battle-cpu-style|" + String(seed >>> 0) + "|" + runtimeSongName + "|" + profileId + "|"
    );
    context.seed = seed;
    if (perMatch) perMatch.set(cacheKey, context);
    return context;
  }

function getBeatBattleLiveNoteStyleRandomUnit(match, profile, sideKey, songData, salt) {
    const context = getBeatBattleLiveRandomContext(match, profile, sideKey, songData);
    const key = String(salt || "");
    if (context.styleUnits.has(key)) return context.styleUnits.get(key);
    const value = appendReplayHashUint32(context.styleHash, key) / 0xffffffff;
    context.styleUnits.set(key, value);
    return value;
  }

function getBeatBattleMatchRunFormVersion(match) {
    return match && Number(match.runFormVersion) >= 1 ? BEAT_BATTLE_RUN_FORM_VERSION : 0;
  }

function getBeatBattleLiveRunAccuracyTarget(match, profile, sideKey, songData, baseTarget) {
    return applyBeatBattleProfileRunAccuracy(
      baseTarget,
      profile,
      salt => getBeatBattleLiveNoteStyleRandomUnit(
        match, profile, sideKey, songData, "run-form|" + String(salt || "")
      ),
      getBeatBattleMatchRunFormVersion(match)
    );
  }

function getBeatBattleLiveNoteOutcome(match, profile, sideKey, songData, noteIndex, noteTime, lane, styleUnitCache = null, targetAccuracyOverride = null, inputState = null) {
    const isRapid = !!(
      songData &&
      songData.rapidFlags &&
      songData.rapidFlags[noteIndex]
    );
    const hasTargetAccuracyOverride =
      targetAccuracyOverride !== null &&
      targetAccuracyOverride !== undefined &&
      Number.isFinite(Number(targetAccuracyOverride));
    const targetAccuracyPercent = hasTargetAccuracyOverride
      ? Number(targetAccuracyOverride)
      : getBeatBattleProfileTargetAccuracy(
          profile,
          Number(songData && songData.baseBpm) || 140,
          isRapid,
          getBeatBattleLiveHumanTimingVersion(match)
        );
    const humanTimingVersion = getBeatBattleLiveHumanTimingVersion(match);
    if (humanTimingVersion >= 3 && songData && !songData.humanContext) {
      songData.humanContext = buildBeatBattleHumanChartContext(songData);
    }
    const humanContext = humanTimingVersion >= 3 && songData && songData.humanContext;
    const side = sideKey === "a" ? 0 : 1;
    const randomContext = getBeatBattleLiveRandomContext(match, profile, sideKey, songData);
    const noteHash = appendReplayHashUint32(randomContext.noteHash,
      String(noteIndex) + "|" + String(noteTime) + "|" + String(lane) + "|");
    const outcome = getBeatBattleHumanTimingOutcome({
      profile,
      humanTimingVersion,
      targetAccuracyPercent,
      noteTime: Number(noteTime),
      lane: Number(lane),
      isRapid,
      bpm: Number(songData && songData.baseBpm) || 140,
      density: humanContext ? humanContext.density[noteIndex] : 0,
      strain: humanContext ? humanContext.strain[noteIndex] : 0,
      noteOrdinal: humanContext ? humanContext.ordinals[noteIndex] : 0,
      noteCount: humanContext ? humanContext.sideCounts[side] : 0,
      rapidRatio: humanContext ? humanContext.rapidRatios[side] : 0,
      inputState,
      noteRandomUnit: salt => appendReplayHashUint32(noteHash, String(salt || "")) / 0xffffffff,
      styleRandomUnit: salt => {
        const cacheKey = String(salt || "");
        if (styleUnitCache instanceof Map && styleUnitCache.has(cacheKey)) {
          return styleUnitCache.get(cacheKey);
        }
        const value = appendReplayHashUint32(randomContext.styleHash, cacheKey) / 0xffffffff;
        if (styleUnitCache instanceof Map) {


          if (styleUnitCache === randomContext.styleUnits && styleUnitCache.size >= 64) {

            let oldest = styleUnitCache.keys().next().value;
            for (const key of styleUnitCache.keys()) {
              if (key.startsWith("drift|") || key.startsWith("phrase|") || key.startsWith("focus|")) {
                oldest = key;
                break;
              }
            }
            styleUnitCache.delete(oldest);
          }
          styleUnitCache.set(cacheKey, value);
        }
        return value;
      }
    });
    return outcome;
  }

function chooseBeatBattleLiveNoteRating(match, profile, sideKey, songData, noteIndex, noteTime, lane, styleUnitCache = null, targetAccuracyOverride = null, inputState = null) {
    return getBeatBattleLiveNoteOutcome(
      match, profile, sideKey, songData, noteIndex, noteTime, lane,
      styleUnitCache, targetAccuracyOverride, inputState
    ).rating;
  }

function applyBeatBattleLiveNoteRating(state, rating, scoreEligible = true) {
    state.totalPlayed++;
    if (rating === "MISS") {
      state.miss++;
      state.combo = 0;
      return;
    }

    let points = 100;
    let weight = 0.40;
    if (rating === "SICK") {
      state.sick++;
      points = 350;
      weight = 1.0;
    } else if (rating === "GOOD") {
      state.good++;
      points = 200;
      weight = 0.75;
    } else {
      state.bad++;
    }

    if (scoreEligible) state.rawScore += points;
    state.weightedHits += weight;
    state.combo++;
    state.maxCombo = Math.max(state.maxCombo, state.combo);
  }

function ensureBeatBattleBackgroundLiveNoteSim(match, resolvedSong = null) {
    if (!match) return null;
    const song = resolvedSong || getBeatBattleBackgroundMatchSong(match);
    const chartData = song && song.beatBattleLightweightChart;
    if (!song || !chartData || !chartData.times || !chartData.times.length) return null;

    const songKey = String(song.folderName || "") + "|" + String(song.chartFile && song.chartFile.name || "");
    const humanTimingVersion = getBeatBattleLiveHumanTimingVersion(match);
    let sim = match.noteSim;
    if (
      !sim ||
      typeof sim !== "object" ||
      Number(sim.version) !== BEAT_BATTLE_LIVE_NOTE_SIM_VERSION ||
      normalizeBeatBattleHumanTimingVersion(sim.humanTimingVersion, 1) !== humanTimingVersion ||
      String(sim.songKey || "") !== songKey
    ) {
      sim = {
        version: BEAT_BATTLE_LIVE_NOTE_SIM_VERSION,
        humanTimingVersion,
        songKey,
        cursor: 0,
        processedThroughMs: 0,
        a: createBeatBattleLiveNoteSideState(),
        b: createBeatBattleLiveNoteSideState(),
        scoreA: 0,
        scoreB: 0,
        accuracyA: 0,
        accuracyB: 0
      };
      match.noteSim = sim;
      beatBattleLiveNoteNormalizedSideStates.add(sim.a);
      beatBattleLiveNoteNormalizedSideStates.add(sim.b);
    } else {
      sim.cursor = Math.max(0, Math.min(chartData.times.length, Math.floor(Number(sim.cursor) || 0)));
      if (!beatBattleLiveNoteNormalizedSideStates.has(sim.a)) {
        sim.a = normalizeBeatBattleLiveNoteSideState(sim.a);
        beatBattleLiveNoteNormalizedSideStates.add(sim.a);
      }
      if (!beatBattleLiveNoteNormalizedSideStates.has(sim.b)) {
        sim.b = normalizeBeatBattleLiveNoteSideState(sim.b);
        beatBattleLiveNoteNormalizedSideStates.add(sim.b);
      }
    }

    const startedAt = Number(match.startedAt) || Date.now();
    match.startedAt = startedAt;


    match.endsAt = startedAt + Math.max(1000, Number(chartData.durationMs) || 120000);

    return { song, chartData, sim };
  }

function updateBeatBattleLiveNoteSimTotals(sim, chartData, targetSongTimeMs) {
    sim.processedThroughMs = Math.max(Number(sim.processedThroughMs) || 0, targetSongTimeMs);
    const scoreCounts = getBeatBattleScoreableChartNoteCounts(chartData);
    sim.scoreA = getBeatBattleLiveNoteNormalizedScore(sim.a, scoreCounts.player);
    sim.scoreB = getBeatBattleLiveNoteNormalizedScore(sim.b, scoreCounts.opponent);
    sim.accuracyA = sim.a.totalPlayed > 0 ? (sim.a.weightedHits / sim.a.totalPlayed) * 100 : 0;
    sim.accuracyB = sim.b.totalPlayed > 0 ? (sim.b.weightedHits / sim.b.totalPlayed) * 100 : 0;
  }

function advanceBeatBattleBackgroundLiveNoteSim(match, now = Date.now(), maxNotes = Infinity, deadline = Infinity, resolvedSong = null) {
    const context = ensureBeatBattleBackgroundLiveNoteSim(match, resolvedSong);
    if (!context) return 0;
    const { chartData, sim } = context;
    const profileA = getBeatBattleBackgroundSimulationProfileById(match.aId);
    const profileB = getBeatBattleBackgroundSimulationProfileById(match.bId);
    if (!profileA || !profileB) return 0;

    const targetSongTimeMs = Math.max(
      0,
      Math.min(Number(chartData.durationMs) || 0, Number(now) - Number(match.startedAt || now))
    );
    const safeMaxNotes = Number.isFinite(Number(maxNotes))
      ? Math.max(1, Math.floor(Number(maxNotes)))
      : Infinity;

    if (sim.cursor >= chartData.times.length || Number(chartData.times[sim.cursor]) > targetSongTimeMs) {
      updateBeatBattleLiveNoteSimTotals(sim, chartData, targetSongTimeMs);
      return 0;
    }



    const baseBpm = Number(chartData.baseBpm) || 140;
    const humanTimingVersion = getBeatBattleLiveHumanTimingVersion(match);
    const targetAccuracy = {
      a: [
        getBeatBattleLiveRunAccuracyTarget(
          match, profileA, "a", chartData,
          getBeatBattleProfileTargetAccuracy(profileA, baseBpm, false, humanTimingVersion)
        ),
        getBeatBattleLiveRunAccuracyTarget(
          match, profileA, "a", chartData,
          getBeatBattleProfileTargetAccuracy(profileA, baseBpm, true, humanTimingVersion)
        )
      ],
      b: [
        getBeatBattleLiveRunAccuracyTarget(
          match, profileB, "b", chartData,
          getBeatBattleProfileTargetAccuracy(profileB, baseBpm, false, humanTimingVersion)
        ),
        getBeatBattleLiveRunAccuracyTarget(
          match, profileB, "b", chartData,
          getBeatBattleProfileTargetAccuracy(profileB, baseBpm, true, humanTimingVersion)
        )
      ]
    };


    const styleUnitCacheA = getBeatBattleLiveRandomContext(match, profileA, "a", chartData).styleUnits;
    const styleUnitCacheB = getBeatBattleLiveRandomContext(match, profileB, "b", chartData).styleUnits;

    let cursor = Math.max(0, Math.floor(Number(sim.cursor) || 0));
    let processed = 0;
    while (
      cursor < chartData.times.length &&
      Number(chartData.times[cursor]) <= targetSongTimeMs &&
      processed < safeMaxNotes &&
      (processed === 0 || performance.now() < deadline)
    ) {
      const sideKey = chartData.sides[cursor] === 0 ? "a" : "b";
      const profile = sideKey === "a" ? profileA : profileB;
      const state = sideKey === "a" ? sim.a : sim.b;
      const isRapid = !!(chartData.rapidFlags && chartData.rapidFlags[cursor]);
      const rating = chooseBeatBattleLiveNoteRating(
        match,
        profile,
        sideKey,
        chartData,
        cursor,
        Number(chartData.times[cursor]),
        Number(chartData.lanes[cursor]),
        sideKey === "a" ? styleUnitCacheA : styleUnitCacheB,
        targetAccuracy[sideKey][isRapid ? 1 : 0],
        state
      );
      applyBeatBattleLiveNoteRating(
        state, rating,
        isBeatBattleChartNoteScoreEligible(chartData, Number(chartData.times[cursor]))
      );
      cursor++;
      processed++;
    }

    sim.cursor = cursor;
    updateBeatBattleLiveNoteSimTotals(sim, chartData, targetSongTimeMs);
    return processed;
  }

async function finishBeatBattleBackgroundLiveNoteSim(match, deadline = getBeatBattleBackgroundWorkDeadline()) {
    const context = ensureBeatBattleBackgroundLiveNoteSim(match);
    if (!context) return;
    const { song, chartData, sim } = context;


    while (sim.cursor < chartData.times.length && Number(chartData.times[sim.cursor]) <= Number(chartData.durationMs)) {
      const before = sim.cursor;
      advanceBeatBattleBackgroundLiveNoteSim(match, match.endsAt, BEAT_BATTLE_BACKGROUND_MAX_NOTES_PER_MATCH,
        deadline, song);
      if (sim.cursor === before) break;
      if (sim.cursor < chartData.times.length) {
        await yieldBeatBattleBackgroundWork();
        deadline = getBeatBattleBackgroundWorkDeadline();
      }
    }
  }

function getBeatBattleBackgroundLiveNoteScores(match, finish = false) {
    const context = ensureBeatBattleBackgroundLiveNoteSim(match);
    if (!context) return null;
    if (finish) {
      advanceBeatBattleBackgroundLiveNoteSim(match, Number(match.endsAt) || Date.now());
    }
    const sim = match.noteSim;
    if (!sim) return null;
    return {
      scoreA: Math.max(0, Math.min(100000, Math.round(Number(sim.scoreA) || 0))),
      scoreB: Math.max(0, Math.min(100000, Math.round(Number(sim.scoreB) || 0))),
      accuracyA: Number(sim.accuracyA) || 0,
      accuracyB: Number(sim.accuracyB) || 0,
      cursor: Math.max(0, Math.floor(Number(sim.cursor) || 0)),
      totalNotes: context.chartData.times.length
    };
  }

let beatBattlePersistentLiveGeneration = 0;

let beatBattlePersistentLiveTrusted = false;

let beatBattlePersistentLiveNeedsFullSync = true;

const beatBattlePersistentLiveSyncedMatchIds = new Set();

const beatBattlePersistentLiveProfileFingerprints = new Map();

const beatBattlePersistentLiveSongFingerprints = new Map();

const beatBattlePersistentLiveDirtyMatchIds = new Set();

const beatBattlePersistentLiveRemovedMatchIds = new Set();

const beatBattlePersistentLiveDirtyProfileIds = new Set();

const beatBattlePersistentLiveDirtySongKeys = new Set();

const beatBattlePersistentLiveStats = {
    matchCount: 0, profileCount: 0, songCount: 0,
    lastSyncUpsertMatches: 0, lastSyncUpsertProfiles: 0, lastSyncUpsertSongs: 0,
    lastSyncRemoveMatches: 0, lastSyncEstimatedBytes: 0, lastTickEstimatedBytes: 0,
    lastTickDeltaCount: 0, exportCount: 0, resyncCount: 0, restartCount: 0, fallbackCount: 0,
    lastSyncBuildMs: 0, lastDeltaApplyMs: 0, lastSpectatorProposalBuildMs: 0, lastSpectatorProposalApplyMs: 0
  };

function recordBeatBattlePersistentSpectatorProposalTiming(){}

function invalidateBeatBattlePersistentLiveMirror(reason = "invalidated") {
    if (beatBattlePersistentLiveGeneration || beatBattlePersistentLiveTrusted) beatBattlePersistentLiveStats.fallbackCount++;
    beatBattlePersistentLiveGeneration = 0;
    beatBattlePersistentLiveTrusted = false;
    beatBattlePersistentLiveNeedsFullSync = true;
    beatBattlePersistentLiveSyncedMatchIds.clear();
    beatBattlePersistentLiveProfileFingerprints.clear();
    beatBattlePersistentLiveSongFingerprints.clear();
    beatBattlePersistentLiveDirtyMatchIds.clear();
    beatBattlePersistentLiveRemovedMatchIds.clear();
    beatBattlePersistentLiveDirtyProfileIds.clear();
    beatBattlePersistentLiveDirtySongKeys.clear();
    beatBattlePersistentLiveStats.matchCount = 0;
    beatBattlePersistentLiveStats.profileCount = 0;
    beatBattlePersistentLiveStats.songCount = 0;
    return String(reason || "invalidated");
  }

function markBeatBattlePersistentLiveMatchDirty(){}

function markBeatBattlePersistentLiveMatchRemoved(){}

function applyBeatBattlePersistentLiveExportedMatch(match, exported) {
    if (!match || !exported || String(match.id || "") !== String(exported.id || "")) return false;
    if (!exported.noteSim || typeof exported.noteSim !== "object") return false;
    match.noteSim = exported.noteSim;
    match.endsAt = Number(exported.endsAt) || match.endsAt;
    if (match.noteSim.a) beatBattleLiveNoteNormalizedSideStates.add(match.noteSim.a);
    if (match.noteSim.b) beatBattleLiveNoteNormalizedSideStates.add(match.noteSim.b);
    if (match.noteSimSummary) {
      match.noteSimSummary.cursor = Math.max(0, Math.floor(Number(match.noteSim.cursor) || 0));
      match.noteSimSummary.processedThroughMs = Number(match.noteSim.processedThroughMs) || 0;
      match.noteSimSummary.scoreA = Number(match.noteSim.scoreA) || 0;
      match.noteSimSummary.scoreB = Number(match.noteSim.scoreB) || 0;
      match.noteSimSummary.accuracyA = Number(match.noteSim.accuracyA) || 0;
      match.noteSimSummary.accuracyB = Number(match.noteSim.accuracyB) || 0;
    }
    return true;
  }

async function exportBeatBattlePersistentLiveMatches(matchIds, finish = false) {
    if (!beatBattlePersistentLiveTrusted || !beatBattlePersistentLiveGeneration) return null;
    const ids = Array.from(new Set((Array.isArray(matchIds) ? matchIds : []).map(id => String(id || "")).filter(Boolean)));
    if (!ids.length) return { generation: beatBattlePersistentLiveGeneration, matches: [] };
    const response = await requestBeatBattleBackgroundCompute(
      "live-export", [], getBeatBattlePopulationRevision(), {
        timeoutMs: 15000, softTimeout: true,
        payload: { generation: beatBattlePersistentLiveGeneration, matchIds: ids, finish: !!finish }
      }
    );
    const result = response && response.result;
    if (!result || Number(result.generation) !== beatBattlePersistentLiveGeneration || !Array.isArray(result.matches)) {
      throw new Error("Invalid persistent LIVE export result");
    }
    beatBattlePersistentLiveStats.exportCount += result.matches.length;
    return result;
  }

function advanceBeatBattleBackgroundLiveNoteBatchWithWorker(){return {usedWorker:false,processed:0};}

function invalidateBeatBattleRankingCache() {
    beatBattleRankingCacheDirty = true;
    markBeatBattlePopulationDerivedStateDirty();
    invalidateBeatBattlePlayerRankCache();
  }

async function createBeatBattleBackgroundLiveMatch(
    usedIds = null, candidateIds = null, matchmakingPool = null, matchmakingAvailableIds = undefined,
    matchmakingIndex = null, logicalNow = Date.now()
  ) {
    const records = loadBeatBattlePopulationRecords();
    const matchNow = Number.isFinite(Number(logicalNow)) ? Number(logicalNow) : Date.now();
    const used = usedIds instanceof Set ? usedIds : new Set();
    const populationPool = Array.isArray(matchmakingPool)
      ? matchmakingPool
      : await collectBeatBattleBackgroundMatchmakingPool();
    const availableIds = matchmakingAvailableIds instanceof Set
      ? matchmakingAvailableIds
      : (matchmakingAvailableIds === null
          ? null
          : await collectBeatBattleBackgroundAvailableIdsWithWorker(populationPool, matchNow));
    const populationIndex = matchmakingIndex || await buildBeatBattleBackgroundMatchmakingIndexWithWorker(populationPool);

    let a = await getBeatBattleBackgroundAvailableProfile(used, 96, {
      candidatePool: populationPool,
      availableIds,
      availabilityNow: matchNow
    });
    let b = null;
    const aRange = a ? getBeatBattlePopulationOnlineSongRange(a) : "beginner";
    const beginnerProtection = aRange === "beginner";
    if (a) used.add(a.id);

    if (a) {
      b = await findBeatBattleBackgroundOpponentNear(
        a.rp,
        a.accuracy,
        new Set([...used, a.id]),
        BEAT_BATTLE_POPULATION_SIZE,
        {
          accuracyPrimary: !beginnerProtection,
          rpOnly: beginnerProtection,
          candidateIndex: populationIndex,
          candidatePool: populationPool,
          availableIds,
          availabilityNow: matchNow,
          maxAccuracy: beginnerProtection ? BEAT_BATTLE_BEGINNER_OPPONENT_MAX_ACCURACY : null,
          requiredSongRange: aRange,
          allowHigherSongRange: !beginnerProtection && aRange === "standard"
        }
      );
    }
    if (!b) {
      b = await getBeatBattleBackgroundAvailableProfile(
        new Set([...used, a && a.id].filter(Boolean)),
        96,
        {
          candidatePool: populationPool,
          availableIds,
          availabilityNow: matchNow,
          maxAccuracy: beginnerProtection ? BEAT_BATTLE_BEGINNER_OPPONENT_MAX_ACCURACY : null,
          requiredSongRange: aRange,
          allowHigherSongRange: !beginnerProtection && aRange === "standard"
        }
      );
    }
    if (!a || !b || a.id === b.id) return null;


    if (!isBeatBattlePopulationAvailableForMatch(a.id, matchNow) || !isBeatBattlePopulationAvailableForMatch(b.id, matchNow)) return null;
    used.add(b.id);

    records.background.serial = Math.max(0, Math.floor(Number(records.background.serial) || 0)) + 1;
    const now = matchNow;
    const skinCandidates = Array.isArray(candidateIds) ? candidateIds : collectBeatBattlePopulationSkinCandidateIds();
    maybeChangeBeatBattlePopulationSkin(a, skinCandidates, now);
    maybeChangeBeatBattlePopulationSkin(b, skinCandidates, now);
    const song = chooseBeatBattleSongForRange(aRange, Math.random());
    const songSlot = getBeatBattleSongGlobalSlot(song);
    const chartData = song && song.beatBattleLightweightChart;
    const duration = chartData && Number(chartData.durationMs) > 0
      ? Number(chartData.durationMs)
      : (70000 + Math.floor(Math.random() * 90001));
    const backfill = Math.min(18000, Math.max(0, Math.floor(duration * 0.12)));
    const startedAt = now - Math.floor(Math.random() * (backfill + 1));

    return {
      id: "live-" + records.background.serial + "-" + now,
      aId: a.id,
      bId: b.id,
      aCharacterId: getBeatBattleFixedSkinId(a, skinCandidates),
      bCharacterId: getBeatBattleFixedSkinId(b, skinCandidates),
      startedAt,
      endsAt: startedAt + duration,
      songSlot,
      songFolder: String(song && song.folderName || ""),
      songStars: typeof getBeatBattleSongStars === "function" ? (getBeatBattleSongStars(song) || undefined) : undefined,
      songRange: aRange,
      humanTimingVersion: BEAT_BATTLE_HUMAN_TIMING_VERSION,
      runFormVersion: BEAT_BATTLE_RUN_FORM_VERSION,
      noteSim: null
    };
  }

function finalizeBeatBattleBackgroundLiveMatch(match, forcedScoreA = null, forcedScoreB = null, reason = "background live", finishNoteSim = true, removeFromLive = true) {
    if (!match) return false;
    const records = loadBeatBattlePopulationRecords();
    const live = records.background.liveMatches || [];
    if (removeFromLive) {
      let index = live.indexOf(match);
      if (index < 0) index = live.findIndex(room => room && String(room.id) === String(match.id));
      if (index < 0) return false;
      live.splice(index, 1);
      markBeatBattlePersistentLiveMatchRemoved(match.id);
    }





    let liveScores = null;
    if (finishNoteSim) {
      liveScores = getBeatBattleBackgroundLiveNoteScores(match, true);
    } else {
      const partialScores = getBeatBattleBackgroundLiveNoteScores(match, false);
      if (partialScores && partialScores.cursor >= partialScores.totalNotes) {
        liveScores = partialScores;
      }
    }
    releaseBeatBattleViewerParticipants(match);
    releaseBeatBattleLiveViewers(match.id);
    const result = getBeatBattleBackgroundResultScores(match);
    if (!result.a || !result.b) return false;




    const hasForcedScoreA =
      forcedScoreA !== null && forcedScoreA !== undefined && forcedScoreA !== "" &&
      Number.isFinite(Number(forcedScoreA));
    const hasForcedScoreB =
      forcedScoreB !== null && forcedScoreB !== undefined && forcedScoreB !== "" &&
      Number.isFinite(Number(forcedScoreB));
    const scoreA = hasForcedScoreA
      ? Number(forcedScoreA)
      : (liveScores ? liveScores.scoreA : result.scoreA);
    const scoreB = hasForcedScoreB
      ? Number(forcedScoreB)
      : (liveScores ? liveScores.scoreB : result.scoreB);
    const leagueChange = applyBeatBattlePopulationLeagueResult(result.a, result.b, scoreA, scoreB);
    const songFields = getBeatBattlePopulationHistorySongFields(match);
    queueBeatBattlePopulationHistoryRecord({
      matchId: String(match.id || ""),
      aId: result.a.id,
      bId: result.b.id,
      aCharacterId: String(match.aCharacterId || result.a.characterId || ""),
      bCharacterId: String(match.bCharacterId || result.b.characterId || ""),
      endedAt: Math.max(1, Math.floor(Number(match.endsAt) || Date.now())),
      songSlot: Number(match.songSlot),
      songFolder: songFields.songFolder,
      songName: songFields.songName,
      scoreA,
      scoreB,
      accuracyA: liveScores ? liveScores.accuracyA : scoreA / 1000,
      accuracyB: liveScores ? liveScores.accuracyB : scoreB / 1000,
      deltaA: leagueChange && leagueChange.deltaA,
      deltaB: leagueChange && leagueChange.deltaB,
      source: liveScores ? "live" : "aggregate-fallback",
      replayable: !!(liveScores && liveScores.cursor >= liveScores.totalNotes),
      simulationVersion: liveScores ? BEAT_BATTLE_LIVE_NOTE_SIM_VERSION : 0,
      humanTimingVersion: getBeatBattleLiveHumanTimingVersion(match),
      runFormVersion: getBeatBattleMatchRunFormVersion(match)
    });
    invalidateBeatBattleRankingCache();
    console.debug(
      "[Background Match Result]",
      reason,
      match.id,
      result.a.id,
      scoreA,
      "-",
      scoreB,
      result.b.id,
      liveScores ? ("note-sim " + liveScores.cursor + "/" + liveScores.totalNotes) : "aggregate-fallback"
    );
    return true;
  }

async function simulateBeatBattleBackgroundCompletedMatch(
    serialSalt = "", matchmakingPool = null, matchmakingAvailableIds = undefined, logicalNow = Date.now()
  ) {
    const matchNow = Number.isFinite(Number(logicalNow)) ? Number(logicalNow) : Date.now();
    const populationPool = Array.isArray(matchmakingPool)
      ? matchmakingPool
      : await collectBeatBattleBackgroundMatchmakingPool();
    const availableIds = matchmakingAvailableIds instanceof Set
      ? matchmakingAvailableIds
      : (matchmakingAvailableIds === null
          ? null
          : await collectBeatBattleBackgroundAvailableIdsWithWorker(populationPool, matchNow));
    const a = await getBeatBattleBackgroundAvailableProfile(null, 72, {
      candidatePool: populationPool,
      availableIds,
      availabilityNow: matchNow
    });
    if (!a) return false;
    const aRange = getBeatBattlePopulationOnlineSongRange(a);
    const beginnerProtection = aRange === "beginner";
    let b = await findBeatBattleBackgroundOpponentNear(
      a.rp,
      a.accuracy,
      new Set([a.id]),
      BEAT_BATTLE_POPULATION_SIZE,
      {
        accuracyPrimary: !beginnerProtection,
        rpOnly: beginnerProtection,
        candidatePool: populationPool,
        availableIds,
        availabilityNow: matchNow,
        maxAccuracy: beginnerProtection ? BEAT_BATTLE_BEGINNER_OPPONENT_MAX_ACCURACY : null,
        requiredSongRange: aRange,
        allowHigherSongRange: !beginnerProtection && aRange === "standard"
      }
    );
    if (!b) {
      b = await getBeatBattleBackgroundAvailableProfile(
        new Set([a.id]),
        72,
        {
          candidatePool: populationPool,
          availableIds,
          availabilityNow: matchNow,
          maxAccuracy: beginnerProtection ? BEAT_BATTLE_BEGINNER_OPPONENT_MAX_ACCURACY : null,
          requiredSongRange: aRange,
          allowHigherSongRange: !beginnerProtection && aRange === "standard"
        }
      );
    }
    if (!b || a.id === b.id) return false;
    if (!isBeatBattlePopulationAvailableForMatch(a.id, matchNow) || !isBeatBattlePopulationAvailableForMatch(b.id, matchNow)) return false;

    const pseudoId = "sim-" + String(serialSalt) + "-" + a.id + "-" + b.id + "-" + Math.floor(Math.random() * 1e9);
    const songUnit = getBeatBattlePopulationHashUnit("background-song|" + pseudoId);
    const song = chooseBeatBattleSongForRange(aRange, songUnit);
    const pseudo = {
      id: pseudoId,
      aId: a.id,
      bId: b.id,
      humanTimingVersion: BEAT_BATTLE_HUMAN_TIMING_VERSION,
      runFormVersion: BEAT_BATTLE_RUN_FORM_VERSION,


      
      songSlot: getBeatBattleSongGlobalSlot(song),
      songFolder: String(song && song.folderName || ""),
      songRange: aRange
    };
    const result = await getBeatBattleBackgroundResultScoresWithWorker(pseudo);
    if (!result.a || !result.b) return false;
    const leagueChange = applyBeatBattlePopulationLeagueResult(result.a, result.b, result.scoreA, result.scoreB);
    const songFields = getBeatBattlePopulationHistorySongFields(pseudo);
    queueBeatBattlePopulationHistoryRecord({
      matchId: pseudoId,
      aId: result.a.id,
      bId: result.b.id,
      endedAt: matchNow,
      songSlot: Number(pseudo.songSlot),
      songFolder: songFields.songFolder,
      songName: songFields.songName,
      scoreA: result.scoreA,
      scoreB: result.scoreB,
      accuracyA: result.scoreA / 1000,
      accuracyB: result.scoreB / 1000,
      deltaA: leagueChange && leagueChange.deltaA,
      deltaB: leagueChange && leagueChange.deltaB,
      source: "aggregate",
      replayable: false,
      simulationVersion: 0,
      humanTimingVersion: BEAT_BATTLE_HUMAN_TIMING_VERSION,
      runFormVersion: BEAT_BATTLE_RUN_FORM_VERSION
    });
    return leagueChange;
  }

function isBeatBattleStartupRecoveryActive() {
    const now =
      typeof performance !== "undefined" && typeof performance.now === "function"
        ? performance.now()
        : Date.now();
    return now - beatBattleRuntimeStartedAt < BEAT_BATTLE_STARTUP_RECOVERY_MS;
  }

async function refillBeatBattleBackgroundLiveRoomsPriority(records, now = Date.now()) {
    const bg = records && records.background;
    if (!bg) return 0;
    const liveTarget = getBeatBattleBackgroundLiveTarget(records);
    if ((bg.liveMatches || []).length >= liveTarget) { updateBeatBattleLiveRoomCountLabel(records); return 0; }
    beatBattlePriorityLiveRefillDepth++;
    try {
      const pool = await collectBeatBattleBackgroundMatchmakingPool();
      const availableIds = await collectBeatBattleBackgroundAvailableIdsWithWorker(pool, now);
      const index = await buildBeatBattleBackgroundMatchmakingIndexWithWorker(pool);
      const used = new Set();
      if (beatBattlePlayerLiveRoomActive && beatBattlePopulationOpponent && !beatBattleSpectatorMode) used.add(String(beatBattlePopulationOpponent.id));
      for (const match of bg.liveMatches || []) if (match) { used.add(String(match.aId || "")); used.add(String(match.bId || "")); }
      const skins = collectBeatBattlePopulationSkinCandidateIds();
      let created = 0, deadline = getBeatBattleBackgroundWorkDeadline();
      while ((bg.liveMatches || []).length < liveTarget) {
        const match = await createBeatBattleBackgroundLiveMatch(used, skins, pool, availableIds, index, now);
        if (!match) break;
        bg.liveMatches.push(match); markBeatBattlePersistentLiveMatchDirty(match); reserveBeatBattleViewerParticipants(match); created++;
        if (performance.now() >= deadline) { await yieldBeatBattleBackgroundWork(); deadline = getBeatBattleBackgroundWorkDeadline(); }
      }
      updateBeatBattleLiveRoomCountLabel(records);
      return created;
    } finally { beatBattlePriorityLiveRefillDepth--; }
  }

async function tickBeatBattleBackgroundPopulation(initial = false, logicalNow = null) {
      const records = loadBeatBattlePopulationRecords();
      const bg = records.background;
      await prepareBeatBattleBackgroundPopulationProfiles();
      const now = Number.isFinite(Number(logicalNow)) ? Number(logicalNow) : Date.now();
      const previousTick = Math.max(0, Number(bg.lastTick) || now);
      const elapsedBuckets = Math.max(1, Math.floor((now - previousTick) / BEAT_BATTLE_BACKGROUND_TICK_MS));
      const startupRecovery = initial || isBeatBattleStartupRecoveryActive();
      const gameplayActive = isBeatBattleBackgroundGameplayActive();
      const sliceSize = startupRecovery
        ? BEAT_BATTLE_STARTUP_SLICE_SIZE
        : (gameplayActive
            ? BEAT_BATTLE_BACKGROUND_SLICE_SIZE_PLAYING
            : BEAT_BATTLE_BACKGROUND_SLICE_SIZE_IDLE);
      const noteYieldBudget = startupRecovery
        ? BEAT_BATTLE_STARTUP_NOTE_YIELD_BUDGET
        : (gameplayActive
            ? BEAT_BATTLE_LIVE_NOTE_YIELD_BUDGET_PLAYING
            : BEAT_BATTLE_LIVE_NOTE_YIELD_BUDGET_IDLE);
      const maxNotesPerMatch = startupRecovery
        ? BEAT_BATTLE_STARTUP_MAX_NOTES_PER_MATCH
        : BEAT_BATTLE_BACKGROUND_MAX_NOTES_PER_MATCH;

      await refillBeatBattleBackgroundLiveRoomsPriority(records, now);

      
      
      
      
      let workDeadline = getBeatBattleBackgroundWorkDeadline();
      const liveMatchesForAdvance = (bg.liveMatches || []).filter(Boolean);
      const liveWorkerAdvance = await advanceBeatBattleBackgroundLiveNoteBatchWithWorker(
        liveMatchesForAdvance, now, maxNotesPerMatch
      );
      if (!liveWorkerAdvance.usedWorker) {
        let notesSinceYield = 0;
        let matchesSinceYield = 0;
        for (const match of liveMatchesForAdvance) {
          const song = getBeatBattleBackgroundMatchSong(match);
          await prepareBeatBattleBackgroundHumanChartContext(song && song.beatBattleLightweightChart,
            getBeatBattleLiveHumanTimingVersion(match));
          notesSinceYield += advanceBeatBattleBackgroundLiveNoteSim(match, now, maxNotesPerMatch, workDeadline, song);
          matchesSinceYield++;
          if (notesSinceYield >= noteYieldBudget || matchesSinceYield >= sliceSize || performance.now() >= workDeadline) {
            notesSinceYield = 0;
            matchesSinceYield = 0;
            await yieldBeatBattleBackgroundWork();
            workDeadline = getBeatBattleBackgroundWorkDeadline();
          }
        }
      } else {
        workDeadline = getBeatBattleBackgroundWorkDeadline();
      }

      
      
      const expiredMatches = (bg.liveMatches || []).filter(match =>
        match &&
        Number(match.endsAt) <= now &&
        !(
          beatBattleSpectatorBackgroundMatchId &&
          String(match.id) === String(beatBattleSpectatorBackgroundMatchId) &&
          (beatBattleSpectatorMode || onlineBattleQueueActive)
        )
      );

      const persistentExportedIds = new Set();
      if (liveWorkerAdvance.usedWorker && beatBattlePersistentLiveTrusted && expiredMatches.length) {
        try {
          const exported = await exportBeatBattlePersistentLiveMatches(
            expiredMatches.map(match => String(match && match.id || "")).filter(Boolean), true
          );
          const byId = new Map(expiredMatches.map(match => [String(match && match.id || ""), match]));
          for (const item of exported && exported.matches || []) {
            const match = byId.get(String(item && item.id || ""));
            if (match && applyBeatBattlePersistentLiveExportedMatch(match, item)) persistentExportedIds.add(String(match.id));
          }
        } catch (error) {
          invalidateBeatBattlePersistentLiveMirror(error);
        }
      }

      
      
      
      
      for (let batchStart = 0; batchStart < expiredMatches.length; batchStart += sliceSize) {
        const batchEnd = Math.min(expiredMatches.length, batchStart + sliceSize);
        const ready = [];
        for (let i = batchStart; i < batchEnd; i++) {
          const match = expiredMatches[i];
          const matchId = String(match && match.id || "");
          if (!matchId) continue;
          if (!startupRecovery && !persistentExportedIds.has(matchId)) await finishBeatBattleBackgroundLiveNoteSim(match, workDeadline);
          if (String(beatBattleSpectatorBackgroundMatchId) === matchId &&
              (beatBattleSpectatorMode || onlineBattleQueueActive)) continue;
          ready.push(match);
        }

        if (ready.length) {
          const readyIds = new Set(ready.map(match => String(match.id)));
          const removedIds = new Set();
          bg.liveMatches = (bg.liveMatches || []).filter(match => {
            if (!match) return true;
            const id = String(match.id || "");
            if (readyIds.has(id)) {
              removedIds.add(id);
              return false;
            }
            return true;
          });
          for (const match of ready) {
            const matchId = String(match.id || "");
            if (!removedIds.delete(matchId)) continue;
            finalizeBeatBattleBackgroundLiveMatch(
              match,
              null,
              null,
              startupRecovery ? "startup-expired" : "timer",
              !startupRecovery,
              false
            );
            if (performance.now() >= workDeadline) {
              await yieldBeatBattleBackgroundWork();
              workDeadline = getBeatBattleBackgroundWorkDeadline();
            }
          }
        }
        if (batchEnd < expiredMatches.length) {
          await yieldBeatBattleBackgroundWork();
          workDeadline = getBeatBattleBackgroundWorkDeadline();
        }
      }

      
      
      await updateBeatBattlePopulationSocialSessions(records, now);

      
      
      
      const backgroundMatchmakingPool = await collectBeatBattleBackgroundMatchmakingPool();
      
      
      
      
      
      const backgroundMatchmakingAvailableIds = await collectBeatBattleBackgroundAvailableIdsWithWorker(
        backgroundMatchmakingPool, now);

      
      
      
      const burst = initial
        ? Math.min(BEAT_BATTLE_STARTUP_AGGREGATE_BURST, elapsedBuckets * BEAT_BATTLE_BACKGROUND_BURST_MATCHES)
        : Math.min(96, elapsedBuckets * BEAT_BATTLE_BACKGROUND_BURST_MATCHES);
      for (let i = 0; i < burst; i++) {
        await simulateBeatBattleBackgroundCompletedMatch(
          String(now) + "|" + i,
          backgroundMatchmakingPool,
          backgroundMatchmakingAvailableIds,
          now
        );
        if (gameplayActive || (i + 1) % sliceSize === 0 || performance.now() >= workDeadline) {
          await yieldBeatBattleBackgroundWork();
          workDeadline = getBeatBattleBackgroundWorkDeadline();
        }
      }

      
      
      
      
      
      
      await refillBeatBattleBackgroundLiveRoomsPriority(records, now);

      await updateBeatBattlePopulationSpectators(records, now);
      bg.lastTick = now;
      saveBeatBattlePopulationRecordsFromBackground(false);
      invalidateBeatBattleRankingCache();

      
      
      updateBeatBattleLiveRoomCountLabel(records);
  }

function getBeatBattlePopulationOnlineCount() {
    const bucket = Math.floor(Date.now() / 15000);
    const unit = getBeatBattlePopulationHashUnit("online|" + bucket);
    return 2800 + Math.floor(unit * 4701);
  }

function getBeatBattleRoomCapForOccupancy(liveRooms) {
    const rooms = Math.max(0, Math.floor(Number(liveRooms) || 0));
    if (rooms <= BEAT_BATTLE_LIVE_CAP_NORMAL) return BEAT_BATTLE_LIVE_CAP_NORMAL;
    if (rooms <= BEAT_BATTLE_LIVE_CAP_BUSY) return BEAT_BATTLE_LIVE_CAP_BUSY;
    if (rooms <= BEAT_BATTLE_LIVE_CAP_PEAK) return BEAT_BATTLE_LIVE_CAP_PEAK;
    
    
    return rooms;
  }

function getBeatBattleHeldRoomCap(records, targetCap, liveRooms) {
    const data = records || loadBeatBattlePopulationRecords();
    if (!data.background || typeof data.background !== "object") data.background = {};
    const bg = data.background;
    const target = Math.max(BEAT_BATTLE_LIVE_CAP_NORMAL, Math.floor(Number(targetCap) || BEAT_BATTLE_LIVE_CAP_NORMAL));
    const rooms = Math.max(0, Math.floor(Number(liveRooms) || 0));
    const occupancyCap = getBeatBattleRoomCapForOccupancy(rooms);
    let held = Math.max(0, Math.floor(Number(bg.elasticLiveRoomCap) || 0));

    
    
    if (!held) held = Math.max(target, occupancyCap);

    
    
    if (target > held) held = target;
    if (occupancyCap > held) held = occupancyCap;

    
    
    
    if (target < held && rooms <= target) held = target;

    bg.elasticLiveRoomCap = held;
    return held;
  }

function getBeatBattleRoomDemandSnapshot(records = null) {
    const data = records || loadBeatBattlePopulationRecords();
    const liveMatches = data && data.background && Array.isArray(data.background.liveMatches)
      ? data.background.liveMatches.length
      : 0;
    const online = getBeatBattlePopulationOnlineCount();
    const bucket = Math.floor(Date.now() / 15000);
    
    
    
    const pressure = getBeatBattlePopulationHashUnit("room-demand-v1|" + bucket);
    const demandRate = 0.46 + pressure * 0.40;
    const demand = Math.max(0, Math.min(online, Math.round(online * demandRate)));
    const occupiedPlayers = liveMatches * 2 + (beatBattlePlayerLiveRoomActive ? 2 : 0);
    const waiting = Math.max(0, demand - occupiedPlayers);

    let targetCap = BEAT_BATTLE_LIVE_CAP_NORMAL;
    if (waiting >= BEAT_BATTLE_LIVE_WAIT_PEAK || demand >= BEAT_BATTLE_LIVE_DEMAND_PEAK) {
      targetCap = BEAT_BATTLE_LIVE_CAP_PEAK;
    } else if (waiting >= BEAT_BATTLE_LIVE_WAIT_BUSY || demand >= BEAT_BATTLE_LIVE_DEMAND_BUSY) {
      targetCap = BEAT_BATTLE_LIVE_CAP_BUSY;
    }

    const totalRooms = liveMatches + (beatBattlePlayerLiveRoomActive ? 1 : 0);
    const cap = getBeatBattleHeldRoomCap(data, targetCap, totalRooms);
    
    
    const displayCap = cap;
    return { online, demand, waiting, cap, targetCap, displayCap, liveMatches, totalRooms, occupiedPlayers };
  }

function getBeatBattleBackgroundLiveTarget(records = null) {
    const snapshot = getBeatBattleRoomDemandSnapshot(records);
    const reservedRooms = (beatBattlePlayerLiveRoomActive || beatBattlePlayerRoomQueueReserved) ? 1 : 0;
    
    
    
    return Math.max(0, snapshot.targetCap - reservedRooms);
  }

function collectRandomOpponentCandidateIds(){return native.skins.filter(id=>!isPsychCharacterIdBanned(id)).slice().sort((a,b)=>String(a).localeCompare(String(b)));}

return {
 async initialize(restoring=false){
   normalizeBeatBattlePopulationSocialSessions(native.records.background,native.now);
   normalizeBeatBattlePopulationPractice(native.records.background,native.now);
   normalizeBeatBattlePopulationSpectators(native.records.background,native.now);
   for(const entry of native.history)cacheBeatBattlePopulationHistoryRecord(entry);
   for(const song of native.songs)beatBattleSongByFolderLookup.set(String(song.folderName||'').trim().toLowerCase(),song);
   beatBattleOnlineSongPools={beginner:filterSongsByRange(native.songs,'beginner'),standard:filterSongsByRange(native.songs,'standard'),all:filterSongsByRange(native.songs,'all')};
   rebuildBeatBattleViewerIndex(native.now);
   await prepareBeatBattleBackgroundPopulationProfiles();
   if(!restoring)await tickBeatBattleBackgroundPopulation(true,native.now);
 },
 async tick(now){await tickBeatBattleBackgroundPopulation(false,now);},
 summary(){const s=getBeatBattleRoomDemandSnapshot(native.records);return {
   registered:BEAT_BATTLE_REGISTERED_TOTAL,cpuPlayers:BEAT_BATTLE_POPULATION_SIZE,
   now:native.now,online:s.online,live:s.totalRooms,liveCap:s.displayCap,waiting:s.waiting,
   localPlayerState:'YOU',lastTick:native.records.background.lastTick
 };},
 query(ids){return ids.map(id=>({profile:getBeatBattleEffectivePopulationProfileById(id),
   baseStatus:getBeatBattlePopulationBasePresenceStatus(id,native.now),
   status:getBeatBattlePopulationPresenceStatus(id,native.now)}));},
 history(){const entries=new Map();for(const matches of beatBattlePopulationHistoryMemory.values())for(const m of matches)entries.set(m.matchId,m);
   return [...entries.values()].sort((a,b)=>a.endedAt-b.endedAt||String(a.matchId).localeCompare(String(b.matchId)));}
};

}
