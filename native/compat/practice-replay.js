// Pure V1.12 replay calculations. See replay-provenance.json. No browser APIs.
globalThis.pkeBuildPracticeReplay = input => {
  const s=input.stats, songName=input.song, chartFormat="custom", chart=input.notes;
  const bpmTimeline=input.bpm, scrollSpeedTimeline=[], characterChangeEvents=[];
  const characterChangeBaseOpponentId=input.opponent, currentOpponentCharacterId=input.opponent;
  const onlineBattlePlayerIsOpponent=input.side, chartScrollSpeed=input.speed, speedInput={value:input.speed};
  const captureBeatBattlePopulationOpponentSnapshot=()=>null;
  const ratingOffsetMs=input.offset, hitWindows={sick:45,good:90,bad:135}, safeFrames=10;
  const sustainsAsOneNote=true, gaugePushEnabled=false, zeroComboBreakMode=false, beatBattleMode=false, beatBattlePracticeMode=true;
  const beatBattleGaugeFullDifference=10000, beatBattleCpuSkill=93, beatBattleCpuRunSeed=0, beatBattleHumanTimingVersion=3, beatBattleRunFormVersion=1;
  const rawScore=s.score, combo=s.combo, maxCombo=s.maxCombo, misses=s.miss, sickHits=s.sick, goodHits=s.good, badHits=s.bad;
  const totalPlayed=input.totalPlayed, weightedHits=s.accuracy/100*totalPlayed;
  const replayRecordingSession=true, playing=false, pausedAt=0, replayLastResultSnapshot=null;
  const replayStartedSongName="", replayStartedSongFolderName=input.songFolder, currentFreeplayFolderName=input.songFolder;
  const replayStartedFormat="", replayStartedChartHash="", replayStartedChartSnapshot=null, replayStartedSettings=null;
  const instFileName=input.inst, voicesFileName=input.voices, replayEvents=input.inputs;
  const getRecordedReplaySource=()=>"player", PKE_ENGINE_VERSION="Beat Battle v0.2.42", PKE_REPLAY_SCHEMA_VERSION=1;

  function replayHashString(input) {
    let hash = 0x811c9dc5;
    const value = String(input || "");
    for (let i = 0; i < value.length; i++) {
      hash ^= value.charCodeAt(i);
      hash = Math.imul(hash, 0x01000193) >>> 0;
    }
    return hash.toString(16).padStart(8, "0");
  }

  function buildReplayChartSignaturePayload() {
    return {
      song: String(songName || ""),
      format: String(chartFormat || ""),
      notes: chart.map(note => ({
        time: Number(note.time) || 0,
        lane: Number(note.lane) || 0,
        length: Math.max(0, Number(note.length) || 0),
        noteType: note.noteType ?? "",
        gfNote: note.gfNote === true,
        gfSection: note.gfSection === true,
        altAnim: note.altAnim === true,
        sectionAltAnim: note.sectionAltAnim === true,
        side: note.side === "opponent" ? "opponent" : "player"
      })),
      bpmTimeline: (bpmTimeline || []).map(entry => ({
        time: Number(entry && entry.time) || 0,
        bpm: Number(entry && entry.bpm) || 0
      })),
      scrollSpeedTimeline: (scrollSpeedTimeline || []).map(entry => ({
        time: Number(entry && entry.time) || 0,
        speed: Number(entry && (entry.speed ?? entry.value ?? entry.multiplier)) || 0
      })),
      characterChangeEvents: characterChangeEvents.map(event => ({
        time: Number(event.time) || 0,
        slot: String(event.slot || ""),
        characterId: String(event.characterId || "")
      }))
    };
  }

  function getReplayChartHash() {
    return "fnv1a32-" + replayHashString(
      JSON.stringify(buildReplayChartSignaturePayload())
    );
  }

  function captureReplayChartSnapshot() {
    return {
      song: String(songName || "Untitled"),
      format: String(chartFormat || "custom"),
      opponentCharacterId: String(characterChangeBaseOpponentId || currentOpponentCharacterId || ""),
      populationOpponent: captureBeatBattlePopulationOpponentSnapshot(),
      gfCharacterId: "", 
      characterChangeEvents: characterChangeEvents.map(event => ({
        time: Number(event.time) || 0,
        slot: String(event.slot || ""),
        characterId: String(event.characterId || "")
      })),
      onlineBattlePlayerIsOpponent: !!onlineBattlePlayerIsOpponent,
      notes: chart.map(note => ({
        time: Number(note.time) || 0,
        lane: Number(note.lane) || 0,
        length: Math.max(0, Number(note.length) || 0),
        noteType: note.noteType ?? "",
        gfNote: note.gfNote === true,
        gfSection: note.gfSection === true,
        altAnim: note.altAnim === true,
        sectionAltAnim: note.sectionAltAnim === true,
        side: note.side === "opponent" ? "opponent" : "player"
      })),
      bpmTimeline: (bpmTimeline || []).map(entry => ({
        time: Number(entry && entry.time) || 0,
        bpm: Number(entry && entry.bpm) || 120
      })),
      baseScrollSpeed:
        chartScrollSpeed !== null
          ? Number(chartScrollSpeed)
          : Number(speedInput.value) || 1.8,
      chartScrollSpeed:
        chartScrollSpeed === null ? null : Number(chartScrollSpeed),
      scrollSpeedTimeline: (scrollSpeedTimeline || []).map(entry => ({
        time: Number(entry && entry.time) || 0,
        toSpeed: Number(entry && (entry.toSpeed ?? entry.speed ?? entry.value)) || 1.8,
        durationMs: Math.max(0, Number(entry && entry.durationMs) || 0)
      }))
    };
  }

  function captureReplayGameplaySettings() {
    return {
      ratingOffset: Number(ratingOffsetMs) || 0,
      sickWindow: Number(hitWindows.sick) || 45,
      goodWindow: Number(hitWindows.good) || 90,
      badWindow: Number(hitWindows.bad) || 135,
      safeFrames: Number(safeFrames) || 10,
      sustainsAsOneNote: !!sustainsAsOneNote,
      gaugePushEnabled: !!gaugePushEnabled,
      zeroComboBreakMode: !!zeroComboBreakMode,
      beatBattleMode: !!beatBattleMode,
      practiceReplay: !!beatBattlePracticeMode,
      beatBattleGaugeFullDifference: Number(beatBattleGaugeFullDifference) || 10000,
      beatBattleCpuSkill: Number.isFinite(Number(beatBattleCpuSkill))
        ? Number(beatBattleCpuSkill)
        : 93,
      beatBattleCpuRunSeed: beatBattleCpuRunSeed >>> 0,
      beatBattleHumanTimingVersion,
      beatBattleRunFormVersion
    };
  }

  function captureReplayResultSnapshot() {
    return {
      score: getScore(),
      combo,
      maxCombo,
      misses,
      accuracy: Number(getAccuracy().toFixed(6)),
      fcStatus: getFCStatus(),
      sick: sickHits,
      good: goodHits,
      bad: badHits,
      totalPlayed,
      beatBattle: beatBattleMode
        ? {
            youScore: getBeatBattleYouScore(),
            cpuScore: getBeatBattleCpuScore(),
            difference: getBeatBattleScoreDifference(),
            cpuAccuracyTarget: Number(beatBattleCpuSkill) || 0,
            cpuAccuracy: Number(getBeatBattleCpuAccuracy().toFixed(6)),
            cpuCombo: beatBattleCpuCombo,
            cpuMaxCombo: beatBattleCpuMaxCombo,
            cpuSick: beatBattleCpuSick,
            cpuGood: beatBattleCpuGood,
            cpuBad: beatBattleCpuBad,
            cpuShit: beatBattleCpuShit,
            cpuMiss: beatBattleCpuMiss
          }
        : null
    };
  }

  function getAccuracy() {
    if (!totalPlayed) return 0;
    return (weightedHits / totalPlayed) * 100;
  }

  function getFCStatus() {
    
    if (totalPlayed === 0 && misses === 0) return "N/A";

    
    if (misses >= 10) return "CLEAR";
    if (misses >= 1) return "SDCB";

    
    if (badHits > 0) return "FC";
    if (goodHits > 0) return "GFC";

    
    return "SFC";
  }

  function getScore() {
    return Math.round(rawScore);
  }

  function buildReplayObject(resultOverride = null) {
    const result = resultOverride || ((
      replayRecordingSession || playing || pausedAt > 0
    )
      ? captureReplayResultSnapshot()
      : (replayLastResultSnapshot || captureReplayResultSnapshot()));

    return {
      format: "PKE_REPLAY",
      schemaVersion: PKE_REPLAY_SCHEMA_VERSION,
      engineVersion: PKE_ENGINE_VERSION,
      createdAt: new Date().toISOString(),
      source: getRecordedReplaySource(),
      song: replayStartedSongName || String(songName || "Untitled"),
      songFolder: replayStartedSongFolderName || String(currentFreeplayFolderName || ""),
      chartFormat: replayStartedFormat || String(chartFormat || "custom"),
      chartHash: replayStartedChartHash || getReplayChartHash(),
      chartSnapshot: replayStartedChartSnapshot
        ? replayStartedChartSnapshot
        : captureReplayChartSnapshot(),
      audio: {
        inst: String(instFileName || ""),
        voices: String(voicesFileName || "")
      },
      settings:
        replayStartedSettings
          ? { ...replayStartedSettings }
          : captureReplayGameplaySettings(),
      result,
      inputs: replayEvents.map(event => ({
        time: Number(event.time),
        lane: Number(event.lane),
        type: event.type === "up" ? "up" : "down"
      }))
    };
  }

  return buildReplayObject();
};
