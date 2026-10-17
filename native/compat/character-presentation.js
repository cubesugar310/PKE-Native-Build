// Source declarations preserved verbatim. See character-provenance.json.
(()=>{
"use strict";
const W=1280,H=720,playerFieldX=720,fieldWidth=448;
const OPPONENT_SHARED_POSITION_X_DELTA=W-(playerFieldX+fieldWidth*.82)-(16+((playerFieldX-24)-16)*.39);
const OPPONENT_SHARED_POSITION_Y_DELTA=52;
const animationAnchorBoxCache=new WeakMap(),bfStableSavedLayoutKeyCache=new Map();
let savedLayoutStore,currentBFSkin="",detectedBfSkins=new Map(),detectedBfSkinOrder=[];
// Native evaluates a private copy; durable files are owned by SaveService.
function markSavedLayoutDirty(){}
function writeSavedLayoutStore(){return true;}
// V1.14 app.js:13292 medianNumber
function medianNumber(values, fallback = 1) {
    const nums = values
      .map(Number)
      .filter(Number.isFinite)
      .filter(v => v > 0)
      .sort((a, b) => a - b);

    if (!nums.length) return fallback;

    const mid = Math.floor(nums.length / 2);

    return nums.length % 2
      ? nums[mid]
      : (nums[mid - 1] + nums[mid]) / 2;
  }

// V1.14 app.js:13392 getAnimationAnchorBox
function getAnimationAnchorBox(frames) {
    if (!frames || !frames.length) {
      return { width: 1, height: 1 };
    }

    const cached = animationAnchorBoxCache.get(frames);
    if (cached) return cached;

    const width = medianNumber(
      frames.map(f =>
        Number(f.frameWidth) || Number(f.width) || 1
      ),
      1
    );

    const height = medianNumber(
      frames.map(f =>
        Number(f.frameHeight) || Number(f.height) || 1
      ),
      1
    );

    const box = {
      width: Math.max(1, width),
      height: Math.max(1, height)
    };

    animationAnchorBoxCache.set(frames, box);
    return box;
  }

// V1.14 app.js:13492 getPsychCharacterFramePlacement
function getPsychCharacterFramePlacement(
    frame,
    referenceFrames,
    anchorX,
    anchorY,
    renderScale,
    offsetX = 0,
    offsetY = 0,
    out = null
  ) {
    
    
    
    
    
    
    const box = getAnimationAnchorBox(referenceFrames);

    const logicalX =
      anchorX - (box.width * renderScale) / 2;

    const logicalY =
      anchorY - box.height * renderScale;

    
    
    const dx =
      logicalX +
      (
        -(Number(frame.frameX) || 0) -
        Number(offsetX || 0)
      ) * renderScale;

    const dy =
      logicalY +
      (
        -(Number(frame.frameY) || 0) -
        Number(offsetY || 0)
      ) * renderScale;

    const result = out || {};
    result.dx = dx;
    result.dy = dy;
    result.logicalX = logicalX;
    result.logicalY = logicalY;
    result.logicalWidth = box.width * renderScale;
    result.logicalHeight = box.height * renderScale;
    return result;
  }

// V1.14 app.js:13308 stabilizeIsolatedAtlasTrimOutliers
function stabilizeIsolatedAtlasTrimOutliers(frames) {
    if (!Array.isArray(frames) || frames.length < 3) return 0;

    const nearlyEqual = (a, b, tolerance = 0.01) =>
      Number.isFinite(Number(a)) &&
      Number.isFinite(Number(b)) &&
      Math.abs(Number(a) - Number(b)) <= tolerance;

    const samePackedCrop = (a, b) =>
      !!a && !!b &&
      nearlyEqual(a.x, b.x) &&
      nearlyEqual(a.y, b.y) &&
      nearlyEqual(
        a.sourceWidth != null ? a.sourceWidth : a.width,
        b.sourceWidth != null ? b.sourceWidth : b.width
      ) &&
      nearlyEqual(
        a.sourceHeight != null ? a.sourceHeight : a.height,
        b.sourceHeight != null ? b.sourceHeight : b.height
      );

    const sameLogicalCanvas = (a, b) =>
      !!a && !!b &&
      nearlyEqual(a.frameWidth, b.frameWidth) &&
      nearlyEqual(a.frameHeight, b.frameHeight);

    let changedFrames = 0;

    for (let i = 1; i < frames.length - 1; i++) {
      const previous = frames[i - 1];
      const current = frames[i];
      const next = frames[i + 1];
      if (!previous || !current || !next) continue;

      
      
      
      
      if (!samePackedCrop(previous, current)) continue;
      if (!sameLogicalCanvas(previous, current) ||
          !sameLogicalCanvas(previous, next)) continue;

      let changed = false;
      for (const [key, logicalKey] of [
        ['frameX', 'frameWidth'],
        ['frameY', 'frameHeight']
      ]) {
        const prevValue = Number(previous[key]);
        const currentValue = Number(current[key]);
        const nextValue = Number(next[key]);
        const logicalSize = Math.max(1, Number(current[logicalKey]) || 1);
        if (![prevValue, currentValue, nextValue].every(Number.isFinite)) continue;

        const largeJump = Math.max(24, logicalSize * 0.08);
        const returnTolerance = Math.max(8, logicalSize * 0.03);
        if (
          Math.abs(currentValue - prevValue) < largeJump ||
          Math.abs(currentValue - nextValue) < largeJump ||
          Math.abs(nextValue - prevValue) > returnTolerance
        ) continue;

        current[key] = (prevValue + nextValue) / 2;
        changed = true;
      }

      if (changed) changedFrames += 1;
    }

    return changedFrames;
  }

// V1.14 app.js:13379 stabilizeAnimationMapTrimOutliers
function stabilizeAnimationMapTrimOutliers(animationMap) {
    if (!animationMap || typeof animationMap.values !== 'function') return 0;
    let changed = 0;
    const seen = new Set();
    for (const value of animationMap.values()) {
      const frames = Array.isArray(value) ? value : value && value.frames;
      if (!Array.isArray(frames) || seen.has(frames)) continue;
      seen.add(frames);
      changed += stabilizeIsolatedAtlasTrimOutliers(frames);
    }
    return changed;
  }

// V1.14 app.js:29419 clampSharedCharacterScale
function clampSharedCharacterScale(value, fallback = null) {
    const scale = Number(value);
    if (!Number.isFinite(scale) || scale <= 0) return fallback;
    return Math.max(0.3, Math.min(5, scale));
  }

// V1.14 app.js:29437 normalizeSharedCharacterPosition
function normalizeSharedCharacterPosition(value, fallback = null) {
    if (!value || typeof value !== "object") return fallback;
    const x = Number(value.x);
    const y = Number(value.y);
    if (!Number.isFinite(x) || !Number.isFinite(y)) return fallback;
    return { x: Math.round(x), y: Math.round(y) };
  }

// V1.14 app.js:29454 sharedPositionToOpponentOffsets
function sharedPositionToOpponentOffsets(position) {
    const normalized = normalizeSharedCharacterPosition(position, { x: 0, y: 0 });
    return {
      x: OPPONENT_SHARED_POSITION_X_DELTA - normalized.x,
      y: normalized.y + OPPONENT_SHARED_POSITION_Y_DELTA
    };
  }

// V1.14 app.js:29462 opponentOffsetsToSharedPosition
function opponentOffsetsToSharedPosition(x, y) {
    return {
      x: Math.round(
        OPPONENT_SHARED_POSITION_X_DELTA - (Number(x) || 0)
      ),
      y: Math.round(Number(y) || 0) - OPPONENT_SHARED_POSITION_Y_DELTA
    };
  }

// V1.14 app.js:29561 getOrMigrateSharedCharacterScale
function getOrMigrateSharedCharacterScale(key, bfCandidates = [], opponentSaved = null) {
    const sharedKey = String(key || "");
    if (!sharedKey) return 1;
    if (!savedLayoutStore.characterScale || typeof savedLayoutStore.characterScale !== "object") {
      savedLayoutStore.characterScale = Object.create(null);
    }

    const existing = clampSharedCharacterScale(savedLayoutStore.characterScale[sharedKey], null);
    if (existing !== null) return existing;

    
    for (const candidate of bfCandidates || []) {
      if (!candidate || candidate === sharedKey) continue;
      const legacyShared = clampSharedCharacterScale(
        savedLayoutStore.characterScale[candidate],
        null
      );
      if (legacyShared === null) continue;
      savedLayoutStore.characterScale[sharedKey] = legacyShared;
      markSavedLayoutDirty("characterScale", sharedKey);
      writeSavedLayoutStore();
      return legacyShared;
    }

    
    
    
    
    let migrated = null;
    for (const candidate of bfCandidates || []) {
      const profile = savedLayoutStore.bf && savedLayoutStore.bf[candidate];
      migrated = clampSharedCharacterScale(profile && profile.scale, null);
      if (migrated !== null) break;
    }
    if (migrated === null) {
      migrated = clampSharedCharacterScale(opponentSaved && opponentSaved.scale, null);
    }

    if (migrated !== null) {
      savedLayoutStore.characterScale[sharedKey] = migrated;
      markSavedLayoutDirty("characterScale", sharedKey);
      writeSavedLayoutStore();
      return migrated;
    }
    return 1;
  }

// V1.14 app.js:29471 getOrMigrateSharedCharacterPosition
function getOrMigrateSharedCharacterPosition(key, bfCandidates = [], opponentSaved = null) {
    const sharedKey = String(key || "");
    if (!sharedKey) return { x: 0, y: 0 };
    if (!savedLayoutStore.characterPosition || typeof savedLayoutStore.characterPosition !== "object") {
      savedLayoutStore.characterPosition = Object.create(null);
    }

    const existing = normalizeSharedCharacterPosition(
      savedLayoutStore.characterPosition[sharedKey],
      null
    );
    if (existing !== null) return existing;

    
    for (const candidate of bfCandidates || []) {
      if (!candidate || candidate === sharedKey) continue;
      const legacyShared = normalizeSharedCharacterPosition(
        savedLayoutStore.characterPosition[candidate],
        null
      );
      if (legacyShared === null) continue;
      savedLayoutStore.characterPosition[sharedKey] = { ...legacyShared };
      markSavedLayoutDirty("characterPosition", sharedKey);
      writeSavedLayoutStore();
      return { ...legacyShared };
    }

    
    
    
    let migrated = null;
    for (const candidate of bfCandidates || []) {
      const profile = savedLayoutStore.bf && savedLayoutStore.bf[candidate];
      if (!profile || typeof profile !== "object") continue;
      migrated = {
        x: Math.round(Number(profile.offsetX) || 0),
        y: Math.round(Number(profile.offsetY) || 0)
      };
      break;
    }
    if (migrated === null && opponentSaved && typeof opponentSaved === "object") {
      migrated = opponentOffsetsToSharedPosition(
        opponentSaved.offsetX,
        opponentSaved.offsetY
      );
    }

    if (migrated !== null) {
      savedLayoutStore.characterPosition[sharedKey] = migrated;
      markSavedLayoutDirty("characterPosition", sharedKey);
      writeSavedLayoutStore();
      return { ...migrated };
    }
    return { x: 0, y: 0 };
  }

// V1.14 app.js:28917 getLegacyBFSavedLayoutKey
function getLegacyBFSavedLayoutKey(skinId = currentBFSkin) {
    return String(skinId || "boyfriend");
  }

// V1.14 app.js:28925 getLegacyPathBasedBFSavedLayoutKey
function getLegacyPathBasedBFSavedLayoutKey(skinId = currentBFSkin) {
    const legacy = getLegacyBFSavedLayoutKey(skinId);
    const def = detectedBfSkins && detectedBfSkins.get
      ? detectedBfSkins.get(String(skinId || ""))
      : null;

    if (!def) return legacy;

    const type = String(def.type || "sparrow").trim().toLowerCase();
    const directory = String(def.directory || "")
      .replace(/\\/g, "/")
      .replace(/\/+$/g, "")
      .toLowerCase();
    const baseName = cleanAssetBaseName(def.baseName || "").toLowerCase();
    const jsonPath = def.jsonFile ? getFileRelativePath(def.jsonFile).toLowerCase() : "";
    const xmlPath = def.xmlFile ? getFileRelativePath(def.xmlFile).toLowerCase() : "";
    const imagePath = def.imageFile ? getFileRelativePath(def.imageFile).toLowerCase() : "";

    const identity = [
      type,
      directory,
      baseName,
      jsonPath,
      xmlPath,
      imagePath
    ].join("|");

    return identity.replace(/\|+$/g, "") || legacy;
  }

// V1.14 app.js:28955 getStableBFSavedLayoutKey
function getStableBFSavedLayoutKey(skinId = currentBFSkin) {
    const normalizedSkinId = String(skinId || "");
    const legacy = getLegacyBFSavedLayoutKey(normalizedSkinId);
    const def = detectedBfSkins && detectedBfSkins.get
      ? detectedBfSkins.get(normalizedSkinId)
      : null;

    if (def) {
      const psychKey = getCanonicalPsychCharacterKeyFromDefinition(def);
      const stableKey = psychKey || getLegacyPathBasedBFSavedLayoutKey(normalizedSkinId);
      const stableKeyCache =
        typeof bfStableSavedLayoutKeyCache !== "undefined"
          ? bfStableSavedLayoutKeyCache
          : null;
      if (stableKey && normalizedSkinId && stableKeyCache) {
        stableKeyCache.set(normalizedSkinId, stableKey);
      }
      return stableKey || legacy;
    }

    
    
    
    const stableKeyCache =
      typeof bfStableSavedLayoutKeyCache !== "undefined"
        ? bfStableSavedLayoutKeyCache
        : null;
    return stableKeyCache && stableKeyCache.get(normalizedSkinId) || legacy;
  }

// V1.14 app.js:29202 getCurrentBFSavedLayoutCandidates
function getCurrentBFSavedLayoutCandidates(skinId = currentBFSkin) {
    
    
    
    const candidates = [
      getStableBFSavedLayoutKey(skinId),
      getLegacyPathBasedBFSavedLayoutKey(skinId)
    ];
    return candidates.filter((value, index) => value && candidates.indexOf(value) === index);
  }

// V1.14 app.js:29378 getOpponentBFLayoutCandidates
function getOpponentBFLayoutCandidates(jsonFile, xmlFile, imageFile) {
    
    
    
    
    
    
    if (jsonFile) {
      const characterId = cleanAssetBaseName(fileBaseName(jsonFile)).toLowerCase();
      if (characterId) {
        const canonicalKey = "psych:" + characterId;
        const legacyOpponentKey = "opponent:" + characterId;
        for (const skinId of detectedBfSkinOrder) {
          const def = detectedBfSkins.get(skinId);
          if (getPsychCharacterIdFromDefinition(def) === characterId) {
            return [
              canonicalKey,
              ...getCurrentBFSavedLayoutCandidates(skinId),
              legacyOpponentKey
            ].filter((value, index, list) => value && list.indexOf(value) === index);
          }
        }
        
        
        
        return [canonicalKey, legacyOpponentKey];
      }
    }

    
    const sameFile = (a, b) => !!(a && b && (a === b ||
      getFileRelativePath(a).toLowerCase() === getFileRelativePath(b).toLowerCase()));
    for (const skinId of detectedBfSkinOrder) {
      const def = detectedBfSkins.get(skinId);
      if (!def || def.type !== "sparrow" || !sameFile(def.xmlFile, xmlFile) || !sameFile(def.imageFile, imageFile)) continue;
      if (jsonFile && !sameFile(def.jsonFile, jsonFile)) continue;
      return getCurrentBFSavedLayoutCandidates(skinId);
    }
    return [];
  }

// V1.14 app.js:16538 getPsychCharacterIdFromDefinition
function getPsychCharacterIdFromDefinition(definition) {
    const def = definition && typeof definition === "object" ? definition : null;
    if (!def || def.type !== "sparrow") return "";
    const raw = def.jsonFile
      ? fileBaseName(def.jsonFile)
      : (def.characterId || def.baseName || "");
    return cleanAssetBaseName(raw).trim().toLowerCase();
  }

// V1.14 app.js:16547 getCanonicalPsychCharacterKeyFromDefinition
function getCanonicalPsychCharacterKeyFromDefinition(definition) {
    const characterId = getPsychCharacterIdFromDefinition(definition);
    return characterId ? "psych:" + characterId : "";
  }

// V1.14 js/character-utils.js:131 cleanAssetBaseName
function cleanAssetBaseName(value) {
    const raw = String(value || "").replace(/\\/g, "/");
    const last = raw.split("/").pop() || "";
    return last.replace(/\.(png|xml|json)$/i, "");
  }

// V1.14 js/character-utils.js:137 fileBaseName
function fileBaseName(file) {
    return String(file && file.name || "").replace(/\.[^.]+$/, "");
  }

// V1.14 js/character-utils.js:141 getFileRelativePath
function getFileRelativePath(file) {
    return String(
      file && (
        file.pkeRelativePath ||
        file.webkitRelativePath ||
        file.name
      ) || ""
    ).replace(/\\/g, "/");
  }

// V1.14 js/character-utils.js:629 cloneOpponentPoseAdjustmentMap
function cloneOpponentPoseAdjustmentMap(source) {
    const result = Object.create(null);

    if (!source || typeof source !== "object") {
      return result;
    }

    for (const [name, value] of Object.entries(source)) {
      result[name] = {
        x: Math.round(Number(value && value.x) || 0),
        y: Math.round(Number(value && value.y) || 0)
      };
    }

    return result;
  }

// V1.14 js/character-utils.js:646 cloneBFPoseAdjustmentMap
function cloneBFPoseAdjustmentMap(source) {
    const result = Object.create(null);

    if (!source || typeof source !== "object") {
      return result;
    }

    for (const [name, value] of Object.entries(source)) {
      result[name] = {
        x: Math.round(Number(value && value.x) || 0),
        y: Math.round(Number(value && value.y) || 0)
      };
    }

    return result;
  }

// V1.14 js/character-utils.js:663 createEmptySavedLayoutStore
function createEmptySavedLayoutStore() {
    return {
      bf: Object.create(null),
      opponent: Object.create(null),
      gf: Object.create(null),
      characterScale: Object.create(null),
      characterPosition: Object.create(null)
    };
  }

// V1.14 js/character-utils.js:673 cloneSavedLayoutStore
function cloneSavedLayoutStore(store) {
    const next = createEmptySavedLayoutStore();

    if (!store || typeof store !== "object") {
      return next;
    }

    if (store.bf && typeof store.bf === "object") {
      for (const [key, value] of Object.entries(store.bf)) {
        next.bf[key] = {
          scale: Math.max(
            0.3,
            Math.min(5, Number(value && value.scale) || 1)
          ),
          offsetX: Math.round(Number(value && value.offsetX) || 0),
          offsetY: Math.round(Number(value && value.offsetY) || 0),
          flipXOverride:
            typeof (value && value.flipXOverride) === "boolean"
              ? value.flipXOverride
              : null,
          poseAdjustments:
            cloneBFPoseAdjustmentMap(
              value && value.poseAdjustments
            )
        };
      }
    }

    if (store.opponent && typeof store.opponent === "object") {
      for (const [key, value] of Object.entries(store.opponent)) {
        next.opponent[key] = {
          // Keep the layout model when cloning saved profiles. Without this,
          // Psych-JSON enemies were treated as incompatible after reload and
          // their saved scale/position was silently ignored.
          layoutModel:
            value && value.layoutModel
              ? String(value.layoutModel)
              : "",
          scale: Math.max(
            0.3,
            Math.min(5, Number(value && value.scale) || 1)
          ),
          offsetX: Math.round(Number(value && value.offsetX) || 0),
          offsetY: Math.round(Number(value && value.offsetY) || 0),
          flipXOverride:
            typeof (value && value.flipXOverride) === "boolean"
              ? value.flipXOverride
              : null,
          poseAdjustments: cloneOpponentPoseAdjustmentMap(
            value && value.poseAdjustments
          )
        };
        if (typeof (value && value.scaleOverride) === "boolean") {
          next.opponent[key].scaleOverride = value.scaleOverride;
        }
      }
    }

    if (store.gf && typeof store.gf === "object") {
      for (const [key, value] of Object.entries(store.gf)) {
        next.gf[key] = {
          scale: Math.max(
            0.3,
            Math.min(5, Number(value && value.scale) || 1)
          ),
          offsetX: Math.round(Number(value && value.offsetX) || 0),
          offsetY: Math.round(Number(value && value.offsetY) || 0)
        };
      }
    }

    if (store.characterScale && typeof store.characterScale === "object") {
      for (const [key, value] of Object.entries(store.characterScale)) {
        const scale = Number(value);
        if (!key || !Number.isFinite(scale) || scale <= 0) continue;
        next.characterScale[key] = Math.max(0.3, Math.min(5, scale));
      }
    }

    if (store.characterPosition && typeof store.characterPosition === "object") {
      for (const [key, value] of Object.entries(store.characterPosition)) {
        if (!key || !value || typeof value !== "object") continue;
        const x = Number(value.x);
        const y = Number(value.y);
        if (!Number.isFinite(x) || !Number.isFinite(y)) continue;
        next.characterPosition[key] = {
          x: Math.round(x),
          y: Math.round(y)
        };
      }
    }

    return next;
  }

// V1.14 app.js:29099 canonicalizeSavedLayoutAliasesFromIdentityHistory
function canonicalizeSavedLayoutAliasesFromIdentityHistory() {
    if (typeof savedLayoutStore === "undefined" || !savedLayoutStore) return false;
    const history = loadSkinIdentityHistory();
    let changed = false;

    for (const [alias, values] of Object.entries(history || {})) {
      const legacy = String(alias || "");
      if (!/^(?:sparrow|vslice):/i.test(legacy)) continue;
      const owners = Array.from(new Set(
        (Array.isArray(values) ? values : [values])
          .map(value => String(value || ""))
          .filter(Boolean)
      ));
      if (owners.length !== 1) continue;

      const stableKey = owners[0];
      if (!stableKey || stableKey === legacy) continue;

      
      
      
      
      if (savedLayoutStore.bf &&
          !Object.prototype.hasOwnProperty.call(savedLayoutStore.bf, stableKey) &&
          Object.prototype.hasOwnProperty.call(savedLayoutStore.bf, legacy)) {
        const cloned = cloneSavedLayoutStore({
          bf: { [stableKey]: savedLayoutStore.bf[legacy] }
        });
        savedLayoutStore.bf[stableKey] = cloned.bf[stableKey];
        markSavedLayoutDirty("bf", stableKey);
        changed = true;
      }

      if (savedLayoutStore.characterScale &&
          !Object.prototype.hasOwnProperty.call(savedLayoutStore.characterScale, stableKey) &&
          Object.prototype.hasOwnProperty.call(savedLayoutStore.characterScale, legacy)) {
        const scale = clampSharedCharacterScale(savedLayoutStore.characterScale[legacy], null);
        if (scale !== null) {
          savedLayoutStore.characterScale[stableKey] = scale;
          markSavedLayoutDirty("characterScale", stableKey);
          changed = true;
        }
      }

      if (savedLayoutStore.characterPosition &&
          !Object.prototype.hasOwnProperty.call(savedLayoutStore.characterPosition, stableKey) &&
          Object.prototype.hasOwnProperty.call(savedLayoutStore.characterPosition, legacy)) {
        const position = normalizeSharedCharacterPosition(
          savedLayoutStore.characterPosition[legacy],
          null
        );
        if (position !== null) {
          savedLayoutStore.characterPosition[stableKey] = { ...position };
          markSavedLayoutDirty("characterPosition", stableKey);
          changed = true;
        }
      }
    }

    return changed;
  }

 let skinIdentityHistoryCache={};
 function loadSkinIdentityHistory(){return skinIdentityHistoryCache;}
 function initializeProfiles(input,assets){
   savedLayoutStore=cloneSavedLayoutStore(input.layouts);
   skinIdentityHistoryCache=input.identityHistory&&typeof input.identityHistory==='object'&&!Array.isArray(input.identityHistory)?input.identityHistory:{};
   canonicalizeSavedLayoutAliasesFromIdentityHistory();
   detectedBfSkins=new Map();bfStableSavedLayoutKeyCache.clear();
   for(const asset of assets)if(asset.definition)detectedBfSkins.set(String(asset.skinId||''),asset.definition);
   detectedBfSkinOrder=Array.from(detectedBfSkins.keys());
   currentBFSkin=String(assets[0].skinId||'');
 }
 function resolveProfile(input,opponent){
   const candidates=opponent&&input.definition&&input.definition.jsonFile?getOpponentBFLayoutCandidates(input.definition.jsonFile,input.definition.xmlFile,input.definition.imageFile):getCurrentBFSavedLayoutCandidates(String(input.skinId||''));
   const key=candidates[0]||'';
   let saved=opponent?savedLayoutStore.opponent[String(input.id||'')]:candidates.map(k=>savedLayoutStore.bf[k]).find(Boolean);
   const scale=getOrMigrateSharedCharacterScale(key,candidates,opponent?saved:null);
   if(opponent&&saved&&String(saved.layoutModel||'')!=='psych-json-v2')saved=null;
   const position=getOrMigrateSharedCharacterPosition(key,candidates,opponent?saved:null);
   const offset=opponent?sharedPositionToOpponentOffsets(position):position;
   return {key,scale,offsetX:offset.x,offsetY:offset.y,
     flipXOverride:saved&&typeof saved.flipXOverride==='boolean'?saved.flipXOverride:null,
     poses:opponent?cloneOpponentPoseAdjustmentMap(saved&&saved.poseAdjustments):cloneBFPoseAdjustmentMap(saved&&saved.poseAdjustments)};
 }
 globalThis.pkeCharacterProfile=input=>{
   initializeProfiles(input,[input]);return resolveProfile(input,input.role==='opponent');
 };
 globalThis.pkeCharacterProfiles=input=>{
   initializeProfiles(input,[input.player,input.opponent]);
   const player=resolveProfile(input.player,false),opponent=resolveProfile(input.opponent,true);
   // Original applySavedOpponentScaleProfile -> syncLiveSharedCharacterScale.
   // Load-time position/flip/pose behavior remains role-specific.
   if(player.key&&player.key===opponent.key)player.scale=opponent.scale;
   return {player,opponent};
 };
 globalThis.pkeCharacterGeometry=input=>{
   const frames=input.frames,animations=new Map(input.animations.map(a=>[a.name,a.indices.map(i=>frames[i])]));
   stabilizeAnimationMapTrimOutliers(animations);
   return {frames,anchors:Array.from(animations,([name,frames])=>({name,...getAnimationAnchorBox(frames)}))};
 };
 globalThis.pkePsychPlacement=input=>getPsychCharacterFramePlacement(input.frame,input.frames,input.anchorX,input.anchorY,input.scale,input.offsetX,input.offsetY);
})();
