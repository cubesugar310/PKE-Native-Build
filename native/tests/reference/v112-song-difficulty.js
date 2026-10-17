/* PKE Beat Battle v0.1.98 - song difficulty and random-song ranges */
(() => {
  "use strict";

  const LABELS = ["", "EASY", "NORMAL", "HARD", "VERY HARD", "INSANE", "EXTREME", "IMPOSSIBLE"];
  const DUPLICATE_EPSILON_MS = 0.01;
  // Initial heuristics, not claims of a universally calibrated FNF rating.
  // A short hard section can still push a chart out of the beginner/standard
  // pools, but a single tiny spike is no longer enough by itself to award
  // EXTREME/IMPOSSIBLE. High stars need duration, repetition, or a longer jack.
  const TYPICAL_NPS_LIMITS = [2, 4, 6, 8, 11, 15];
  const PEAK_THREE_SECOND_LIMITS = [2.5, 4.5, 6.5, 9, 12, 16];
  const PEAK_ONE_SECOND_LIMITS = [3, 5, 8, 11, 15, 20];
  const JACK_NPS_LIMITS = [3, 4, 5, 6.5, 8, 11];

  function ratingForLoad(value, limits) {
    for (let i = 0; i < limits.length; i++) if (value <= limits[i]) return i + 1;
    return 7;
  }

  // Consumes the already-sorted, authored HEAD stream used by LIVE. Sustain
  // children are never expanded. Only scalar summaries survive discovery.
  function* iterateSongDifficulty(data, scrollSpeed) {
    if (!data || !data.times || !data.sides || !data.lanes ||
        data.times.length !== data.sides.length || data.times.length !== data.lanes.length) return null;
    const bySide = [[], []];
    const lastLaneTime = [[-Infinity, -Infinity, -Infinity, -Infinity], [-Infinity, -Infinity, -Infinity, -Infinity]];
    const regularFlags = data.difficultyRegularFlags;
    const hasRegularFlags = !!regularFlags && regularFlags.length === data.times.length;
    let work = 0;
    for (let i = 0; i < data.times.length; i++) {
      if (hasRegularFlags && regularFlags[i] !== 1) {
        if (++work % 64 === 0) yield;
        continue;
      }
      const time = Number(data.times[i]);
      const lane = Number(data.lanes[i]);
      const side = data.sides[i] === 1 ? 1 : 0;
      if (Number.isFinite(time) && Number.isInteger(lane) && lane >= 0 && lane < 4 &&
          time - lastLaneTime[side][lane] > DUPLICATE_EPSILON_MS) {
        bySide[side].push(i);
        lastLaneTime[side][lane] = time;
      }
      if (++work % 64 === 0) yield;
    }

    const summaries = [];
    for (const indices of bySide) {
      const bins = new Map();
      const lastTimes = [-Infinity, -Infinity, -Infinity, -Infinity];
      const lastGaps = [Infinity, Infinity, Infinity, Infinity];
      const jackRunLengths = [0, 0, 0, 0];
      let leftOne = 0, leftThree = 0, peakNps = 0, peakThreeSecondNps = 0;
      let peakOneSecondSpanMs = 0, peakThreeSecondSpanMs = 0;
      let severeBurstCount = 0, impossibleBurstCount = 0;
      let nextSevereBurstStart = -Infinity, nextImpossibleBurstStart = -Infinity;
      let groupTime = -Infinity, chord = 0, maxChord = 0, maxJackNps = 0;
      let maxJackRun = 0, jackRunCount = 0;
      const firstTime = indices.length ? Number(data.times[indices[0]]) : 0;
      for (let j = 0; j < indices.length; j++) {
        const index = indices[j], time = Number(data.times[index]), lane = Number(data.lanes[index]);
        while (time - Number(data.times[indices[leftOne]]) >= 1000) {
          leftOne++;
          if (++work % 64 === 0) yield;
        }
        while (time - Number(data.times[indices[leftThree]]) >= 3000) {
          leftThree++;
          if (++work % 64 === 0) yield;
        }
        const oneCount = j - leftOne + 1;
        const oneStart = Number(data.times[indices[leftOne]]);
        if (oneCount > peakNps) {
          peakNps = oneCount;
          peakOneSecondSpanMs = Math.max(0, time - oneStart);
        }
        const threeCount = j - leftThree + 1;
        const threeNps = threeCount / 3;
        const threeStart = Number(data.times[indices[leftThree]]);
        if (threeNps > peakThreeSecondNps) {
          peakThreeSecondNps = threeNps;
          peakThreeSecondSpanMs = Math.max(0, time - threeStart);
        }
        // Count separate one-second danger bursts instead of counting every
        // overlapping sliding window. ★6 needs 16+ heads/s, ★7 needs 21+.
        if (oneCount >= 16 && oneStart >= nextSevereBurstStart) {
          severeBurstCount++;
          nextSevereBurstStart = oneStart + 1000;
        }
        if (oneCount >= 21 && oneStart >= nextImpossibleBurstStart) {
          impossibleBurstCount++;
          nextImpossibleBurstStart = oneStart + 1000;
        }
        const bin = Math.floor((time - firstTime) / 3000);
        bins.set(bin, (bins.get(bin) || 0) + 1);
        if (time - groupTime > DUPLICATE_EPSILON_MS) {
          groupTime = time;
          chord = 0;
        }
        maxChord = Math.max(maxChord, ++chord);
        const gap = time - lastTimes[lane];
        if (gap > DUPLICATE_EPSILON_MS && gap <= 400) {
          jackRunLengths[lane] = Math.max(1, jackRunLengths[lane]) + 1;
        } else {
          jackRunLengths[lane] = 1;
        }
        maxJackRun = Math.max(maxJackRun, jackRunLengths[lane]);
        if (jackRunLengths[lane] === 5) jackRunCount++;
        // Keep the raw jack speed metric, but high stars below also require a
        // real run length/repetition. A three-hit blip can no longer make an
        // otherwise easy chart EXTREME by itself.
        if (gap > DUPLICATE_EPSILON_MS && gap <= 400 && lastGaps[lane] <= 400) {
          maxJackNps = Math.max(maxJackNps, 1000 / Math.max(gap, lastGaps[lane]));
        }
        lastTimes[lane] = time;
        lastGaps[lane] = gap;
        if (++work % 64 === 0) yield;
      }

      // Nonempty three-second bins describe active phrases rather than intro,
      // outro, or alternating turns. Peak sliding windows cover bin boundaries.
      const loads = Array.from(bins.values(), count => count / 3).sort((a, b) => a - b);
      const typicalNps = loads.length ? loads[Math.ceil(loads.length * 0.75) - 1] : 0;
      const chordFloor = maxChord >= 4 ? 4 : maxChord >= 3 ? 3 : maxChord >= 2 ? 2 : 1;
      const typicalStars = ratingForLoad(typicalNps, TYPICAL_NPS_LIMITS);
      const rawThreeSecondStars = ratingForLoad(peakThreeSecondNps, PEAK_THREE_SECOND_LIMITS);
      let threeSecondStars = rawThreeSecondStars;
      // A huge sub-second cluster can make the old 3 s average look extreme.
      // Require the peak window to actually span part of a phrase before it
      // can award ★6/★7 on its own.
      if (threeSecondStars >= 6 && peakThreeSecondSpanMs < 1500 && typicalStars < 5) threeSecondStars = 5;
      if (threeSecondStars >= 7 && peakThreeSecondSpanMs < 2000 && typicalStars < 6) threeSecondStars = 6;

      const rawOneSecondStars = ratingForLoad(peakNps, PEAK_ONE_SECOND_LIMITS);
      let oneSecondStars = rawOneSecondStars;
      if (oneSecondStars >= 6) {
        let cap = 5;
        if (typicalStars >= 5 || threeSecondStars >= 5 || severeBurstCount >= 2) cap = 6;
        if (typicalStars >= 6 || threeSecondStars >= 6 || impossibleBurstCount >= 2) cap = 7;
        // A single truly enormous 21+ head burst is still harder than ★5,
        // but it needs repetition/sustained support before becoming ★7.
        if (rawOneSecondStars >= 7 && peakNps >= 21) cap = Math.max(cap, 6);
        oneSecondStars = Math.min(oneSecondStars, cap);
      }

      const rawJackStars = ratingForLoad(maxJackNps, JACK_NPS_LIMITS);
      let jackStars = rawJackStars;
      if (maxJackRun < 5) {
        jackStars = Math.min(jackStars, 4);
      } else if (jackStars >= 6) {
        let cap = 5;
        if (maxJackRun >= 8 || jackRunCount >= 2 || typicalStars >= 5 || threeSecondStars >= 5) cap = 6;
        if (maxJackRun >= 12 || jackRunCount >= 3 || typicalStars >= 6 || threeSecondStars >= 6) cap = 7;
        jackStars = Math.min(jackStars, cap);
      }

      const stars = Math.max(chordFloor, typicalStars, threeSecondStars, oneSecondStars, jackStars);
      const spanMs = indices.length ? Math.max(1000, Number(data.times[indices[indices.length - 1]]) - firstTime + 1000) : 1000;
      summaries.push({stars, label:LABELS[stars], noteCount:indices.length,
        averageNps:indices.length * 1000 / spanMs, typicalNps, peakNps, peakThreeSecondNps,
        peakOneSecondSpanMs, peakThreeSecondSpanMs, severeBurstCount, maxChord, maxJackNps, maxJackRun, jackRunCount});
      yield;
    }
    const speed = Number(scrollSpeed);
    return {version:3, player:summaries[0], opponent:summaries[1],
      maxStars:Math.max(summaries[0].stars, summaries[1].stars),
      scrollSpeed:Number.isFinite(speed) && speed > 0 ? speed : null};
  }

  async function analyzeSongDifficulty(data, scrollSpeed, options = {}) {
    const isCurrent = options.isCurrent || (() => true);
    const now = options.now || (() => globalThis.performance?.now?.() ?? Date.now());
    const yieldToBrowser = options.yieldToBrowser || (() => new Promise(resolve => setTimeout(resolve, 0)));
    const budgetMs = Number.isFinite(options.budgetMs) && options.budgetMs > 0 ? options.budgetMs : 4;
    const iterator = iterateSongDifficulty(data, scrollSpeed);
    let deadline = now() + budgetMs, steps = 0;
    while (isCurrent()) {
      const next = iterator.next();
      if (next.done) return isCurrent() ? next.value : null;
      if (++steps >= 8 || now() >= deadline) {
        await yieldToBrowser();
        steps = 0;
        deadline = now() + budgetMs;
      }
    }
    iterator.return();
    return null;
  }

  function normalizeSongRange(value) {
    return value === "all" ? "all" : value === "standard" ? "standard" : "beginner";
  }

  function getSongRangeLimit(value) {
    const range = normalizeSongRange(value);
    return range === "all" ? 7 : range === "standard" ? 5 : 3;
  }

  function getSongRangeLabel(value) {
    const range = normalizeSongRange(value);
    return range === "all" ? "全曲（★1〜★7）" : range === "standard" ? "標準（★1〜★5）" : "初心者保護（★1〜★3）";
  }

  const SONG_RANGE_LEVELS = Object.freeze({beginner:0, standard:1, all:2});

  // Online ranges are progression gates, not player-selectable preferences.
  // Promotion is one-way so an experienced player cannot intentionally drop
  // back into beginner protection by losing RP or starting a new season.
  function promoteAutomaticSongRange(currentValue, matches, rp) {
    const current = normalizeSongRange(currentValue);
    const safeMatches = Math.max(0, Math.floor(Number(matches) || 0));
    const safeRp = Math.max(0, Number(rp) || 0);
    let target = safeMatches >= 20 || safeRp >= 1800 ? "standard" : "beginner";
    if (safeRp >= 2900) target = "all";
    return SONG_RANGE_LEVELS[target] > SONG_RANGE_LEVELS[current] ? target : current;
  }

  function filterSongsByRange(songs, value) {
    const range = normalizeSongRange(value), limit = getSongRangeLimit(range);
    return Array.from(songs || []).filter(song => {
      if (!song) return false;
      if (range === "all") return true;
      const rating = song.beatBattleDifficulty;
      return [rating?.player?.stars, rating?.opponent?.stars].every(stars =>
        Number.isInteger(stars) && stars >= 1 && stars <= limit);
    });
  }

  function formatSongDifficultyInfo(rating) {
    if (!rating?.player || !rating?.opponent) return "";
    const player = rating.player, opponent = rating.opponent;
    return "BF ★" + player.stars + " " + LABELS[player.stars] +
      " / DAD ★" + opponent.stars + " " + LABELS[opponent.stars] +
      " • Notes " + player.noteCount + " / " + opponent.noteCount +
      " • Speed " + (rating.scrollSpeed === null ? "—" : Number(rating.scrollSpeed).toFixed(2));
  }

  const modules = globalThis.PKEModules || (globalThis.PKEModules = {});
  modules.songDifficulty = Object.freeze({analyzeSongDifficulty, normalizeSongRange,
    getSongRangeLimit, getSongRangeLabel, promoteAutomaticSongRange, filterSongsByRange, formatSongDifficultyInfo});
})();
