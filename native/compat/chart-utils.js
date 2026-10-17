/* PKE Beat Battle v0.1.98 - chart-utils */
(() => {
  "use strict";

  function normalizeNoteTypeName(value) {
    return String(value ?? "")
      .trim()
      .toLowerCase()
      .replace(/[_\-]+/g, " ")
      .replace(/\s+/g, " ");
  }

  function getNormalizedNoteType(
    noteOrType
  ) {
    const value =
      noteOrType &&
      typeof noteOrType === "object"
        ? noteOrType.noteType
        : noteOrType;

    return normalizeNoteTypeName(value);
  }

  // Beat Battle gameplay excludes only note types whose sole purpose is to
  // punish the player for pressing them. Harmless routing/animation note types
  // (Alt Animation, GF Sing, Hey!, No Animation, custom visual notes, etc.)
  // stay in the chart. Psych's built-in `Hurt Note` is the main case; the
  // conservative aliases below cover common custom names without treating every
  // explicit noteType as harmful. Arbitrary Lua-only behavior cannot be inferred
  // from chart JSON when the noteType name itself gives no damage hint.
  function isBeatBattleDamageOnlyNoteType(noteOrType) {
    const normalized =
      getNormalizedNoteType(noteOrType);

    if (!normalized) return false;

    if (
      normalized === "hurt" ||
      normalized === "hurt note" ||
      normalized === "damage" ||
      normalized === "damage note" ||
      normalized === "mine" ||
      normalized === "mine note" ||
      normalized === "poison" ||
      normalized === "poison note" ||
      normalized === "hazard" ||
      normalized === "hazard note" ||
      normalized === "death note" ||
      normalized === "kill note"
    ) {
      return true;
    }

    // Custom types such as `Black Hurt Note` / `Fire Damage Note` are still
    // recognisable from their type name. Keep the heuristic intentionally
    // narrow so names like Alt Note / GF Sing are never removed.
    return (
      /(?:^|\s)hurt(?:\s|$)/.test(normalized) ||
      /(?:^|\s)damage(?:\s|$)/.test(normalized)
    );
  }

  function isAltAnimationNote(
    noteOrType
  ) {
    // Beat Battle v0.1.7: Alt Animation notes/section altAnim are disabled.
    // Keep noteType metadata intact for replay/chart compatibility, but every
    // gameplay animation uses the normal directional sing/miss animation.
    return false;
  }

  function isWhiteAndBlackDuetNote(
    noteOrType
  ) {
    return (
      getNormalizedNoteType(noteOrType) ===
      "white and black duet"
    );
  }

  function isDadNoteV2(
    noteOrType
  ) {
    const normalized =
      getNormalizedNoteType(noteOrType);

    return (
      normalized === "dad notev2" ||
      normalized === "dad note v2"
    );
  }

  function isGFSingNote(noteOrType) {
    // Beat Battle v0.1.7: GF Sing note routing is disabled. GF may still be
    // visible/dance normally, but chart notes never replace BF/Dad with GF.
    return false;
  }

  function sanitizeBpm(value, fallback = 120) {
    const n = Number(value);
    return Number.isFinite(n) && n > 0
      ? Math.max(1, n)
      : Math.max(1, Number(fallback) || 120);
  }

  function sanitizeScrollSpeed(value, fallback = 1.8) {
    const n = Number(value);
    return Number.isFinite(n) && n > 0
      ? Math.max(0.05, n)
      : Math.max(0.05, Number(fallback) || 1.8);
  }

  function getScrollSpeedFromBuiltTimeline(songTime, timeline, baseSpeed) {
    const t = Math.max(0, Number(songTime) || 0);
    const base = sanitizeScrollSpeed(baseSpeed, 1.8);

    if (!timeline || !timeline.length) {
      return base;
    }

    let latest = null;

    for (const change of timeline) {
      if (change.time > t) break;
      latest = change;
    }

    if (!latest) return base;

    if (
      latest.durationMs > 0 &&
      t < latest.time + latest.durationMs
    ) {
      const p = Math.max(
        0,
        Math.min(
          1,
          (t - latest.time) / latest.durationMs
        )
      );

      return (
        latest.fromSpeed +
        (latest.toSpeed - latest.fromSpeed) * p
      );
    }

    return latest.toSpeed;
  }

  function getSectionSongObject(json) {
    return json && typeof json.song === "object" ? json.song : json;
  }

  function sectionHasRuntimeContent(section) {
    if (!section || typeof section !== "object") return false;

    if (
      Array.isArray(section.sectionNotes) &&
      section.sectionNotes.length > 0
    ) {
      return true;
    }

    if (
      section.changeBPM === true &&
      Number.isFinite(Number(section.bpm)) &&
      Number(section.bpm) > 0
    ) {
      return true;
    }

    const wantsScrollChange =
      section.changeScrollSpeed === true ||
      section.changeSpeed === true;

    const scrollValue = Number(
      section.scrollSpeed ??
      section.speed
    );

    if (
      wantsScrollChange &&
      Number.isFinite(scrollValue) &&
      scrollValue > 0
    ) {
      return true;
    }

    if (
      (Array.isArray(section.events) && section.events.length > 0) ||
      (Array.isArray(section.eventObjects) && section.eventObjects.length > 0)
    ) {
      return true;
    }

    return false;
  }

  function trimTrailingEmptySections(song) {
    if (!song || !Array.isArray(song.notes) || !song.notes.length) {
      return {
        before: 0,
        after: 0,
        trimmed: 0
      };
    }

    const sections = song.notes;
    const before = sections.length;
    let lastMeaningfulIndex = -1;

    for (let i = sections.length - 1; i >= 0; i--) {
      if (sectionHasRuntimeContent(sections[i])) {
        lastMeaningfulIndex = i;
        break;
      }
    }

    // If the entire chart is empty, leave it untouched so the normal
    // "no valid notes" error path can describe the problem accurately.
    if (lastMeaningfulIndex < 0) {
      return {
        before,
        after: before,
        trimmed: 0
      };
    }

    const after = lastMeaningfulIndex + 1;

    if (after < before) {
      sections.length = after;
    }

    return {
      before,
      after,
      trimmed: Math.max(0, before - after)
    };
  }

  function isSectionChart(json) {
    const song = getSectionSongObject(json);

    return !!(
      song &&
      Array.isArray(song.notes) &&
      song.notes.some(section =>
        section &&
        typeof section === "object" &&
        Array.isArray(section.sectionNotes)
      )
    );
  }

  function detectSectionChartFlavor(json) {
    const song = getSectionSongObject(json);
    if (!song) return "Legacy FNF";

    let hasFourthNoteValue = false;
    let hasPsychNoteType = false;

    for (const section of song.notes || []) {
      const sectionNotes =
        section && Array.isArray(section.sectionNotes)
          ? section.sectionNotes
          : [];

      for (const rawNote of sectionNotes) {
        if (!Array.isArray(rawNote)) continue;

        if (rawNote.length >= 4) {
          hasFourthNoteValue = true;

          const noteType = rawNote[3];
          if (
            typeof noteType === "string" ||
            (typeof noteType === "number" && noteType !== 0)
          ) {
            hasPsychNoteType = true;
          }
        }
      }
    }

    const psychMetadata =
      Array.isArray(json && json.events) ||
      Array.isArray(song.events) ||
      typeof song.gfVersion === "string" ||
      typeof song.stage === "string" ||
      typeof song.arrowSkin === "string" ||
      typeof song.splashSkin === "string" ||
      hasPsychNoteType;

    if (psychMetadata) return "Psych Engine";

    if (
      typeof song.needsVoices === "boolean" ||
      typeof song.validScore === "boolean" ||
      typeof song.player1 === "string" ||
      typeof song.player2 === "string" ||
      !hasFourthNoteValue
    ) {
      return "Kade Engine / Legacy FNF";
    }

    return "Legacy FNF";
  }

  function getSectionSideMappingMode(json, song) {
    const declaredFormat = String(
      (song && song.format) ??
      (json && json.format) ??
      ""
    ).trim().toLowerCase();

    // Psych v1 converted charts use the absolute noteData half as ownership:
    //   0..3 = player/BF
    //   4..7 = opponent
    // mustHitSection still describes the active section/camera, but must NOT
    // be applied a second time to flip note ownership.
    if (
      declaredFormat === "psych_v1_convert" ||
      declaredFormat.includes("psych_v1_convert") ||
      declaredFormat === "nmv2" ||
      declaredFormat.includes("nmv2")
    ) {
      return {
        id: "absolute-note-half",
        label:
          declaredFormat.includes("nmv2")
            ? "NMV2 absolute lanes (0-3 BF / 4-7 opponent)"
            : "Psych v1 converted (0-3 BF / 4-7 opponent)"
      };
    }

    // Legacy Psych/Kade/FNF section chart behavior:
    // noteData is relative to mustHitSection, so >3 swaps ownership.
    return {
      id: "legacy-section-relative",
      label: "Legacy section-relative (mustHitSection + noteData flip)"
    };
  }

  function collectKadeBpmBeatEvents(json, song) {
    const raw =
      Array.isArray(song && song.eventObjects)
        ? song.eventObjects
        : (
            Array.isArray(json && json.eventObjects)
              ? json.eventObjects
              : []
          );

    const out = [];

    for (const event of raw) {
      if (!event || typeof event !== "object") continue;

      const type = String(event.type || "").trim().toLowerCase();

      if (type !== "bpm change") continue;

      const beat = Number(event.position);
      const bpm = Number(event.value);

      if (
        !Number.isFinite(beat) ||
        beat < 0 ||
        !Number.isFinite(bpm) ||
        bpm <= 0
      ) {
        continue;
      }

      out.push({
        beat,
        bpm,
        source:
          "Kade eventObjects: " +
          String(event.name || event.type || "BPM Change")
      });
    }

    return out;
  }

  function collectSectionBpmTimeEvents(song, baseBpm) {
    const out = [];
    let computedStart = 0;
    let currentBpm = sanitizeBpm(baseBpm, 120);

    for (const section of song.notes || []) {
      if (!section || typeof section !== "object") continue;

      const explicitStart = Number(section.startTime);
      const startTime =
        Number.isFinite(explicitStart) && explicitStart >= 0
          ? explicitStart
          : computedStart;

      if (
        section.changeBPM === true &&
        Number.isFinite(Number(section.bpm)) &&
        Number(section.bpm) > 0
      ) {
        currentBpm = Number(section.bpm);

        out.push({
          time: startTime,
          bpm: currentBpm,
          source: "section.changeBPM"
        });
      }

      const explicitEnd = Number(section.endTime);

      if (
        Number.isFinite(explicitEnd) &&
        explicitEnd >= startTime
      ) {
        computedStart = explicitEnd;
      } else {
        const steps = Math.max(
          1,
          Number(section.lengthInSteps) || 16
        );

        computedStart =
          startTime +
          steps * (60000 / currentBpm) / 4;
      }
    }

    return out;
  }

  function normalizePsychCharacterChangeSlot(value) {
    const raw = String(value ?? '').trim().toLowerCase();
    if (raw === '0' || raw === 'bf' || raw === 'boyfriend' || raw === 'player') return 'player';
    if (raw === '1' || raw === 'dad' || raw === 'opponent' || raw === 'enemy') return 'opponent';
    if (raw === '2' || raw === 'gf' || raw === 'girlfriend') return 'gf';
    return 'player';
  }

  function collectPsychChangeCharacterEvents(json, song) {
    const groups = [];
    if (Array.isArray(json && json.events)) groups.push(...json.events);
    if (Array.isArray(song && song.events) && song.events !== json.events) groups.push(...song.events);

    const out = [];
    let order = 0;
    for (const group of groups) {
      if (!Array.isArray(group) || group.length < 2) continue;
      const time = Number(group[0]);
      const list = group[1];
      if (!Number.isFinite(time) || !Array.isArray(list)) continue;

      for (const event of list) {
        if (!Array.isArray(event) || event.length < 1) continue;
        if (String(event[0] || '').trim().toLowerCase() !== 'change character') continue;
        const characterId = String(event[2] ?? '').trim();
        if (!characterId) continue;
        out.push({
          time: Math.max(0, time),
          slot: normalizePsychCharacterChangeSlot(event[1]),
          characterId,
          order: order++
        });
      }
    }
    out.sort((a,b) => a.time - b.time || a.order - b.order);
    return out;
  }

  function collectPsychScrollSpeedEvents(json, song, baseSpeed) {
    const groups = [];

    if (Array.isArray(json && json.events)) {
      groups.push(...json.events);
    }

    if (
      Array.isArray(song && song.events) &&
      song.events !== json.events
    ) {
      groups.push(...song.events);
    }

    const out = [];

    for (const group of groups) {
      if (!Array.isArray(group) || group.length < 2) continue;

      const time = Number(group[0]);
      const list = group[1];

      if (!Number.isFinite(time) || !Array.isArray(list)) continue;

      for (const rawEvent of list) {
        if (!Array.isArray(rawEvent) || rawEvent.length < 1) continue;

        const name =
          String(rawEvent[0] || "")
            .trim()
            .toLowerCase();

        if (
          name !== "change scroll speed" &&
          name !== "scroll speed change"
        ) {
          continue;
        }

        const multiplier = Number(rawEvent[1]);
        const durationSec = Number(rawEvent[2]);

        if (
          !Number.isFinite(multiplier) ||
          multiplier <= 0
        ) {
          continue;
        }

        out.push({
          time,
          toSpeed:
            sanitizeScrollSpeed(baseSpeed, 1.8) *
            multiplier,
          durationMs:
            Number.isFinite(durationSec) && durationSec > 0
              ? durationSec * 1000
              : 0,
          source: "Psych Change Scroll Speed"
        });
      }
    }

    return out;
  }

  const modules = globalThis.PKEModules || (globalThis.PKEModules = {});
  modules.chartUtils = Object.freeze({
    normalizeNoteTypeName,
    getNormalizedNoteType,
    isBeatBattleDamageOnlyNoteType,
    isAltAnimationNote,
    isWhiteAndBlackDuetNote,
    isDadNoteV2,
    isGFSingNote,
    sanitizeBpm,
    sanitizeScrollSpeed,
    getScrollSpeedFromBuiltTimeline,
    getSectionSongObject,
    sectionHasRuntimeContent,
    trimTrailingEmptySections,
    isSectionChart,
    detectSectionChartFlavor,
    getSectionSideMappingMode,
    collectKadeBpmBeatEvents,
    collectSectionBpmTimeEvents,
    normalizePsychCharacterChangeSlot,
    collectPsychChangeCharacterEvents,
    collectPsychScrollSpeedEvents,
  });
})();
