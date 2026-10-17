// Browser-independent V1.12 catalog calculations. See catalog-provenance.json.
(()=>{
"use strict";
  const {normalizeNoteTypeName,isBeatBattleDamageOnlyNoteType,getSectionSongObject,isSectionChart,getSectionSideMappingMode}=PKEModules.chartUtils;
  const BEAT_BATTLE_RAPID_NOTE_MIN_GAP_MS=35,BEAT_BATTLE_RAPID_NOTE_MAX_GAP_MS=170;
  const FREEPLAY_AUDIO_EXTENSIONS=new Set([".ogg",".mp3",".wav",".m4a",".aac",".flac",".webm"]);
  let selectedFolderFiles=[];

  // V1.12 app:getFreeplayChartCandidates 31283-31357 SHA-256 b260b02db97f6a3f46a58f1bb8678ef6e9bb4a43731fa7c367da2f1c8c62a073
  function getFreeplayChartCandidates(files, songFolderName = "") {
    const jsonFiles = files.filter(file =>
      String(file.name || "").toLowerCase().endsWith(".json")
    );

    if (!jsonFiles.length) return [];

    
    
    
    
    const folderBase = String(songFolderName || "")
      .trim()
      .toLowerCase();
    const ordered = [];
    const seen = new Set();

    const add = file => {
      if (!file || seen.has(file)) return;
      seen.add(file);
      ordered.push(file);
    };

    if (folderBase) {
      const exactHardName = folderBase + "-hard.json";
      add(jsonFiles.find(file =>
        String(file.name || "").toLowerCase() === exactHardName
      ));
    }

    const hardCharts = jsonFiles
      .filter(file => /-hard\.json$/i.test(String(file.name || "")))
      .slice()
      .sort((a, b) =>
        String(a.name || "").localeCompare(String(b.name || ""), undefined, {
          numeric: true,
          sensitivity: "base"
        })
      );

    if (hardCharts.length > 1 && folderBase) {
      const normalizedFolder = folderBase.replace(/[^a-z0-9]+/g, "");
      add(hardCharts.find(file => {
        const base = String(file.name || "")
          .toLowerCase()
          .replace(/-hard\.json$/, "")
          .replace(/[^a-z0-9]+/g, "");
        return base === normalizedFolder;
      }));
    }

    for (const file of hardCharts) add(file);

    add(jsonFiles.find(file =>
      String(file.name || "").toLowerCase() === "chart.json"
    ));
    add(jsonFiles.find(file =>
      String(file.name || "").toLowerCase() === "data.json"
    ));
    add(jsonFiles.find(file =>
      String(file.name || "").toLowerCase() === "song.json"
    ));

    const remaining = jsonFiles
      .slice()
      .sort((a, b) =>
        String(a.name || "").localeCompare(String(b.name || ""), undefined, {
          numeric: true,
          sensitivity: "base"
        })
      );
    for (const file of remaining) add(file);

    return ordered;
  }

  // V1.12 app:chooseFreeplayAudioFile 31406-31415 SHA-256 aab72378654c1c8b0fe4117503f2062f09b1d10b08189ffca0015b55d1b0c9c8
  function chooseFreeplayAudioFile(files, prefix) {
    const lowerPrefix = String(prefix || "").toLowerCase();

    return files.find(file => {
      const name = String(file.name || "").toLowerCase();
      const ext = getFileExtension(name);
      return name.startsWith(lowerPrefix) &&
        FREEPLAY_AUDIO_EXTENSIONS.has(ext);
    }) || null;
  }

  // V1.12 app:getFreeplaySongDisplayName 31417-31432 SHA-256 515f6b5aeda374f1138a4b39955f8816bf0de2fb41c1534f04233bae4f06c256
  function getFreeplaySongDisplayName(json, fallback) {
    try {
      const song = getSectionSongObject(json);

      const candidate =
        (song && typeof song.song === "string" && song.song) ||
        (song && typeof song.songName === "string" && song.songName) ||
        (json && typeof json.songName === "string" && json.songName) ||
        (json && typeof json.name === "string" && json.name) ||
        fallback;

      return String(candidate || fallback || "Untitled");
    } catch (_) {
      return String(fallback || "Untitled");
    }
  }

  // V1.12 app:getFreeplayChartReferenceSpeed 31434-31444 SHA-256 baabc617f034d00468503d5338eaedbe1b481c610f9a37fd4f6c1ef6180670fd
  function getFreeplayChartReferenceSpeed(json) {
    const song = isSectionChart(json) ? getSectionSongObject(json) : null;
    
    
    const candidates = song ? [song.speed, song.scrollSpeed, json.scrollSpeed] : [json.scrollSpeed, json.speed];
    for (const raw of candidates) {
      const speed = Number(raw);
      if (Number.isFinite(speed)) return Math.max(0.1, speed);
    }
    return null;
  }

  // V1.12 app:isBeatBattleDifficultyRegularNoteType 31452-31459 SHA-256 2415a9790540e8e923223393142040a079a3f1b8b2cb98cc20b9df366f5af8f6
  function isBeatBattleDifficultyRegularNoteType(value) {
    if (value === undefined || value === null) return true;
    if (typeof value === "number") return value === 0;
    const normalized = normalizeNoteTypeName(value);
    return normalized === "" || normalized === "0" ||
      normalized === "normal" || normalized === "normal note" ||
      normalized === "default" || normalized === "default note";
  }

  // V1.12 app:buildBeatBattleLightweightChartData 31461-31615 SHA-256 c4da8ddf673b4ce94c329b901732b818333bd20da68354e68de67a3ef1ee8568
  function buildBeatBattleLightweightChartData(chartJson, fallbackName = "Song") {
    const compact = [];
    let runtimeSongName = String(fallbackName || "Song");
    const baseBpm = getBeatBattleChartBaseBpm(chartJson);

    if (isSectionChart(chartJson)) {
      const song = getSectionSongObject(chartJson);
      const sideMapping = getSectionSideMappingMode(chartJson, song);
      runtimeSongName = String(
        (song && (song.song || song.songName)) ||
        fallbackName ||
        "Section Chart"
      );

      for (const section of (song && song.notes) || []) {
        if (!section || !Array.isArray(section.sectionNotes)) continue;
        const mustHitSection = !!section.mustHitSection;

        for (const rawNote of section.sectionNotes) {
          if (!Array.isArray(rawNote) || rawNote.length < 2) continue;
          const time = Number(rawNote[0]);
          const rawData = Number(rawNote[1]);
          const length = Math.max(0, Number(rawNote[2] || 0));
          if (!Number.isFinite(time) || !Number.isFinite(rawData) || rawData < 0) continue;

          const noteData = Math.floor(rawData);
          const lane = ((noteData % 4) + 4) % 4;
          let playerNote;
          if (sideMapping.id === "absolute-note-half") {
            playerNote = noteData < 4;
          } else {
            playerNote = mustHitSection;
            if (noteData > 3) playerNote = !playerNote;
          }
          const noteType = rawNote.length >= 4 ? rawNote[3] : "";

          
          
          
          
          if (isBeatBattleDamageOnlyNoteType(noteType)) {
            continue;
          }

          compact.push([
            time,
            playerNote ? 0 : 1,
            lane,
            length,
            isBeatBattleDifficultyRegularNoteType(noteType) ? 1 : 0
          ]);
        }
      }
    } else {
      const arr = Array.isArray(chartJson)
        ? chartJson
        : (chartJson && Array.isArray(chartJson.notes) ? chartJson.notes : []);
      runtimeSongName = String(
        (chartJson && (chartJson.songName || chartJson.name)) ||
        fallbackName ||
        "Custom Chart"
      );

      for (const raw of arr) {
        if (!raw || typeof raw !== "object") continue;
        const time = Number(raw.time);
        const lane = Number(raw.lane);
        if (!Number.isFinite(time) || !Number.isInteger(lane) || lane < 0 || lane > 3) continue;
        const rawSide = String(raw.side ?? raw.owner ?? "").toLowerCase();
        const opponent = rawSide === "opponent" || rawSide === "enemy" || rawSide === "dad" || raw.opponent === true;
        const noteType = raw.noteType ?? "";

        if (isBeatBattleDamageOnlyNoteType(noteType)) {
          continue;
        }

        compact.push([
          time,
          opponent ? 1 : 0,
          lane,
          Math.max(0, Number(raw.length || 0)),
          isBeatBattleDifficultyRegularNoteType(noteType) ? 1 : 0
        ]);
      }
    }

    compact.sort((a, b) => a[0] - b[0]);
    if (!compact.length) return null;

    const times = new Float64Array(compact.length);
    const sides = new Uint8Array(compact.length);
    const lanes = new Uint8Array(compact.length);
    const lengths = new Float64Array(compact.length);
    const rapidFlags = new Uint8Array(compact.length);
    const difficultyRegularFlags = new Uint8Array(compact.length);
    let playerNoteCount = 0;
    let opponentNoteCount = 0;
    let playerRapidCount = 0;
    let opponentRapidCount = 0;
    let lastNoteEndMs = 0;

    for (let i = 0; i < compact.length; i++) {
      const [time, side, lane, length, difficultyRegular] = compact[i];
      times[i] = time;
      sides[i] = side;
      lanes[i] = lane;
      lengths[i] = length;
      difficultyRegularFlags[i] = difficultyRegular ? 1 : 0;
      if (side === 0) playerNoteCount++;
      else opponentNoteCount++;
      lastNoteEndMs = Math.max(lastNoteEndMs, time + Math.max(0, length));
    }

    
    
    const previousIndexBySide = [-1, -1];
    for (let i = 0; i < times.length; i++) {
      const side = sides[i] === 0 ? 0 : 1;
      const previousIndex = previousIndexBySide[side];
      if (previousIndex >= 0) {
        const gap = Number(times[i]) - Number(times[previousIndex]);
        if (gap >= BEAT_BATTLE_RAPID_NOTE_MIN_GAP_MS && gap <= BEAT_BATTLE_RAPID_NOTE_MAX_GAP_MS) {
          if (!rapidFlags[previousIndex]) {
            rapidFlags[previousIndex] = 1;
            if (side === 0) playerRapidCount++;
            else opponentRapidCount++;
          }
          if (!rapidFlags[i]) {
            rapidFlags[i] = 1;
            if (side === 0) playerRapidCount++;
            else opponentRapidCount++;
          }
        }
      }
      previousIndexBySide[side] = i;
    }

    return {
      times,
      sides,
      lanes,
      lengths,
      rapidFlags,
      difficultyRegularFlags,
      baseBpm,
      playerNoteCount,
      opponentNoteCount,
      playerRapidRatio: playerRapidCount / Math.max(1, playerNoteCount),
      opponentRapidRatio: opponentRapidCount / Math.max(1, opponentNoteCount),
      
      
      durationMs: Math.max(30000, Math.ceil(lastNoteEndMs + 1500)),
      runtimeSongName
    };
  }

  // V1.12 app:getFreeplayOpponentCharacterId 31710-31721 SHA-256 4347328607c18eebc2827442c9f21a51c7c2e892d399599d2a10717d91a08ee4
  function getFreeplayOpponentCharacterId(chartJson) {
    try {
      const song = getSectionSongObject(chartJson);
      const raw =
        (song && (song.player2 ?? song.opponent ?? song.dad)) ??
        (chartJson && (chartJson.player2 ?? chartJson.opponent ?? chartJson.dad)) ??
        "";
      return cleanAssetBaseName(String(raw || "").trim()).toLowerCase();
    } catch (_) {
      return "";
    }
  }

  // V1.12 app:findFreeplayOpponentIconFile 31723-31754 SHA-256 1a76128a6893ce4484273ddde61fd1078bc97701b58f52f8b734ef1e707f8845
  function findFreeplayOpponentIconFile(songEntry) {
    const characterId = cleanAssetBaseName(String(
      (songEntry && songEntry.opponentCharacterId) || ""
    ).trim()).toLowerCase();
    if (!characterId) return null;

    const wantedBase = "icon-" + characterId;
    const songFiles = Array.from(
      (songEntry && songEntry.files) || []
    );
    const characterFiles = Array.from(selectedFolderFiles || []);

    
    for (const pool of [songFiles, characterFiles]) {
      const matches = pool.filter(file => {
        const name = String(file && file.name || "").toLowerCase();
        if (!name.endsWith(".png") && !name.endsWith(".webp")) {
          return false;
        }
        return fileBaseName(file).toLowerCase() === wantedBase;
      });

      if (matches.length) {
        matches.sort((a, b) =>
          getFileRelativePath(a).length - getFileRelativePath(b).length
        );
        return matches[0];
      }
    }

    return null;
  }

  // V1.12 app:getFileExtension 31257-31261 SHA-256 ce86d12f1a312d849bd0ae6dc86458251ae56f8c046ed77d41daddf275eacdb8
  function getFileExtension(name) {
    const value = String(name || "");
    const index = value.lastIndexOf(".");
    return index >= 0 ? value.slice(index).toLowerCase() : "";
  }

  // V1.12 app:getBeatBattleChartBaseBpm 19349-19359 SHA-256 8e78d2ab31e3c7ee533b3cc28f428c976de9dfae0073ddf0e49ef2d2bfdfc575
  function getBeatBattleChartBaseBpm(chartJson) {
    try {
      const song = isSectionChart(chartJson) ? getSectionSongObject(chartJson) : chartJson;
      const raw = Number(
        (song && (song.bpm ?? song.songBpm)) ??
        (chartJson && (chartJson.bpm ?? chartJson.songBpm))
      );
      if (Number.isFinite(raw) && raw > 0) return clampPopulationNumber(raw, 40, 320);
    } catch (_) {}
    return 140;
  }

  // V1.12 app:normalizeBeatBattlePracticeSearch 8167-8173 SHA-256 3300d720368547dae68ae12d75e7fb0b94eae644e7753a16bcb2568188ee5025
  function normalizeBeatBattlePracticeSearch(value) {
    return String(value || "")
      .normalize("NFKC")
      .toLocaleLowerCase("ja-JP")
      .replace(/[\s_\-]+/g, " ")
      .trim();
  }

  // V1.12 character:cleanAssetBaseName 131-135 SHA-256 8abb43183eaaaf8afbac8654ec68be24cce3d30a59b142c07e71707641a4fa7f
  function cleanAssetBaseName(value) {
    const raw = String(value || "").replace(/\\/g, "/");
    const last = raw.split("/").pop() || "";
    return last.replace(/\.(png|xml|json)$/i, "");
  }

  // V1.12 character:fileBaseName 137-139 SHA-256 fb99504b26c4f4af44f0bf7f5ac8176b1e4ccc18e96c5f3568bd22be0750a007
  function fileBaseName(file) {
    return String(file && file.name || "").replace(/\.[^.]+$/, "");
  }

  // V1.12 character:getFileRelativePath 141-149 SHA-256 fdbe49a2424138a62ee30006c61cd1b56cdc59a685fae5e0dc0e57ddbaa09cce
  function getFileRelativePath(file) {
    return String(
      file && (
        file.pkeRelativePath ||
        file.webkitRelativePath ||
        file.name
      ) || ""
    ).replace(/\\/g, "/");
  }

  // V1.12 cpu:clampPopulationNumber 418-420 SHA-256 9ec0cc36bc5eb238e95c26a9f636a8c18e18cdac12b15029641cb887adddc04c
  function clampPopulationNumber(value, min, max) {
    return Math.max(min, Math.min(max, Number(value) || 0));
  }

  // V1.12 app:findFolderFileInDirectory 16519-16535 SHA-256 cac946cdb953980831d92c2a2a4fa217b79aca78ae7f8a366a0d4566ebfc4335
  function findFolderFileInDirectory(directory, baseName, extensions) {
    const wantedDir = String(directory || "").toLowerCase();
    const wantedBase = cleanAssetBaseName(baseName).toLowerCase();
    if (!wantedBase) return null;

    const extSet = new Set(extensions.map(ext => String(ext).toLowerCase()));
    const matches = selectedFolderFiles.filter(file => {
      if (getFileDirectoryPath(file) !== wantedDir) return false;
      const lowerName = String(file && file.name || "").toLowerCase();
      const ext = lowerName.includes(".")
        ? "." + lowerName.split(".").pop()
        : "";
      return extSet.has(ext) && fileBaseName(file).toLowerCase() === wantedBase;
    });

    return matches[0] || null;
  }

  // V1.12 app:findFolderFile 17709-17734 SHA-256 ee77c81309bc6a99f835b6d6f8dc5ac2f918e19ab74c61e0736fe3d2f56388b2
  function findFolderFile(baseName, extensions) {
    const wanted = cleanAssetBaseName(baseName).toLowerCase();
    if (!wanted) return null;

    const extSet = new Set(extensions.map(ext => ext.toLowerCase()));
    const matches = selectedFolderFiles.filter(file => {
      const lowerName = String(file.name || "").toLowerCase();
      const ext = lowerName.includes(".")
        ? "." + lowerName.split(".").pop()
        : "";
      return extSet.has(ext) && fileBaseName(file).toLowerCase() === wanted;
    });

    if (!matches.length) return null;

    
    matches.sort((a, b) => {
      const ap = String(a.pkeRelativePath || a.webkitRelativePath || a.name).toLowerCase();
      const bp = String(b.pkeRelativePath || b.webkitRelativePath || b.name).toLowerCase();
      const ac = ap.includes("/characters/") ? 0 : 1;
      const bc = bp.includes("/characters/") ? 0 : 1;
      return ac - bc || ap.length - bp.length;
    });

    return matches[0];
  }

  // V1.12 app:getSparrowSourcePriority 17459-17469 SHA-256 1512be55c6ef240f486d9e930cf2212b0e957422c40828c767e9af8369e64b65
  function getSparrowSourcePriority(definition) {
      const path = getFileRelativePath(definition && definition.jsonFile)
        .replace(/\\/g, "/")
        .toLowerCase();
      const directoryParts = path.split("/").slice(0, -1).filter(Boolean);
      return {
        inCharactersDirectory: directoryParts.includes("characters"),
        pathLength: path.length,
        path
      };
    }

  // V1.12 app:preferSparrowDefinition 17470-17481 SHA-256 2b4c30c5d3cf0937b35ef9105d52be0fb6797adf64fef0e05fed61cc7d02d3f9
  function preferSparrowDefinition(candidate, current) {
      if (!current) return true;
      const a = getSparrowSourcePriority(candidate);
      const b = getSparrowSourcePriority(current);
      if (a.inCharactersDirectory !== b.inCharactersDirectory) {
        return a.inCharactersDirectory;
      }
      if (a.pathLength !== b.pathLength) {
        return a.pathLength < b.pathLength;
      }
      return a.path.localeCompare(b.path) < 0;
    }

  // V1.12 character:getFileDirectoryPath 151-155 SHA-256 e9ca44504cd72026dc503e2fc67bd38fe4c61eb264b543ec31f479593d9a78bb
  function getFileDirectoryPath(file) {
    const path = getFileRelativePath(file);
    const slash = path.lastIndexOf("/");
    return slash >= 0 ? path.slice(0, slash).toLowerCase() : "";
  }

  // V1.12 character:prettifyDetectedBFSkinName 164-184 SHA-256 793cf5fa3d2baf54a645271c6912fec5e44ea79b2c7e6bfb5aea4721ace3ed50
  function prettifyDetectedBFSkinName(rawName, type = "sparrow") {
    const base = cleanAssetBaseName(rawName) || "skin";
    const lower = base.toLowerCase();

    if (lower === "bf" || lower === "boyfriend") {
      return "Boyfriend";
    }
    if (lower === "bfghost") {
      return type === "vslice"
        ? "Ghost Boyfriend (V-Slice)"
        : "Ghost Boyfriend";
    }

    const pretty = base
      .replace(/[-_]+/g, " ")
      .replace(/\b\w/g, c => c.toUpperCase());

    return type === "vslice"
      ? pretty + " (V-Slice)"
      : pretty;
  }

  // V1.12 character:getDetectedCharacterDisplayNameFromJson 204-276 SHA-256 f04a27deff1e6c05eb955e53cd7210e5dca82a46547daf03654ace19d96d09b9
  function getDetectedCharacterDisplayNameFromJson(characterJson, jsonFile = null, fallbackBaseName = "") {
    const data = characterJson && typeof characterJson === "object"
      ? characterJson
      : null;

    const nestedMeta = data && data.metadata && typeof data.metadata === "object"
      ? data.metadata
      : (data && data.meta && typeof data.meta === "object" ? data.meta : null);

    const directCandidates = data ? [
      data.displayName,
      data.display_name,
      data.characterName,
      data.character_name,
      data.character,
      data.name,
      nestedMeta && nestedMeta.displayName,
      nestedMeta && nestedMeta.display_name,
      nestedMeta && nestedMeta.characterName,
      nestedMeta && nestedMeta.character_name,
      nestedMeta && nestedMeta.name
    ] : [];

    for (const value of directCandidates) {
      if (typeof value === "string" && value.trim()) {
        return value.trim();
      }
    }

    // Psych character JSON usually has no dedicated display-name field.
    // v0.1.59: healthicon is an icon lookup id, not the character identity.
    // Many BF variants intentionally share healthicon: "bf", so using it here
    // collapses unrelated skins to the display name "Boyfriend". Prefer the
    // character sprite id first, then the character JSON filename.
    let imageBase = "";
    if (data) {
      imageBase = cleanAssetBaseName(data.image || data.imagePath || data.texture || "");
      if (
        imageBase &&
        !/^(bf|boyfriend|character|animation|spritemap1)$/i.test(imageBase)
      ) {
        return prettifyDetectedBFSkinName(imageBase, "sparrow");
      }
    }

    // If the JSON itself has a meaningful character-specific filename, use it.
    if (jsonFile) {
      const jsonBase = fileBaseName(jsonFile);
      if (
        jsonBase &&
        !/^(character|animation|spritemap1)$/i.test(jsonBase)
      ) {
        return prettifyDetectedBFSkinName(jsonBase, "sparrow");
      }
    }

    // A generic sprite id such as "bf" is still a better identity fallback
    // than an unrelated folder name when the JSON filename is unavailable.
    if (imageBase) {
      return prettifyDetectedBFSkinName(imageBase, "sparrow");
    }

    // healthicon remains supported only as a last compatibility fallback.
    if (data) {
      const healthIcon = cleanAssetBaseName(data.healthicon || data.healthIcon || "");
      if (healthIcon && !/^icon[-_]?face$/i.test(healthIcon)) {
        return prettifyDetectedBFSkinName(healthIcon, "sparrow");
      }
    }

    // Final compatibility fallback only. Folder names no longer take priority.
    return prettifyDetectedBFSkinName(fallbackBaseName || "Character", "sparrow");
  }

// Direct V1.14 character-utils.js import; see source-port-provenance.json.
function getLastPathSegment(path) {
    const normalized = String(path || "").replace(/\\/g, "/").replace(/\/+$/, "");
    if (!normalized) return "";
    const parts = normalized.split("/").filter(Boolean);
    return parts.length ? parts[parts.length - 1] : "";
  }

function buildDetectedBFSkinId(type, directory, baseName = "") {
    const folderSlug = cleanAssetBaseName(getLastPathSegment(directory));
    const baseSlug = cleanAssetBaseName(baseName);
    const slug = (folderSlug || baseSlug || "skin").toLowerCase();
    const prefix = type === "vslice" ? "vslice" : "sparrow";
    return prefix + ":" + slug;
  }

globalThis.PKENativeCatalog=Object.freeze({getLastPathSegment,buildDetectedBFSkinId,getFreeplayChartCandidates,chooseFreeplayAudioFile,getFreeplaySongDisplayName,getFreeplayChartReferenceSpeed,isBeatBattleDifficultyRegularNoteType,buildBeatBattleLightweightChartData,getFreeplayOpponentCharacterId,findFreeplayOpponentIconFile,getFileExtension,getBeatBattleChartBaseBpm,normalizeBeatBattlePracticeSearch,cleanAssetBaseName,fileBaseName,getFileRelativePath,clampPopulationNumber,findFolderFileInDirectory,findFolderFile,getSparrowSourcePriority,preferSparrowDefinition,getFileDirectoryPath,prettifyDetectedBFSkinName,getDetectedCharacterDisplayNameFromJson,icon(song,characters){selectedFolderFiles=characters;return findFreeplayOpponentIconFile(song);},characterFile(directory,base,extensions,files){selectedFolderFiles=files;return findFolderFileInDirectory(directory,base,extensions)||findFolderFile(base,extensions);}});
})();
