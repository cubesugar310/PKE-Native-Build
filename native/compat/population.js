// Native owns clocks, files, scenes, catalogs, and the worker's lifetime.
// This boundary exchanges compact summaries; checkpoints are explicit requests.
"use strict";

let pkePopulationWorld=null;
const PKE_NATIVE_POPULATION_SCHEMA="pke-native-population-v1";
function pkeCompatCopy(value){return JSON.parse(JSON.stringify(value));}
function pkeCompatTime(value){
  if(!Number.isSafeInteger(value)||value<0)throw Error("Invalid Native population time");
  return value;
}
function pkeCompatObject(value){return !!value&&typeof value==='object'&&!Array.isArray(value);}
function pkeCompatRecords(now){
  return {seasonId:2,seasonLabel:'SEASON 2',seasonStartedAt:now,version:12,
    h2h:{},skinById:{},skinMoodById:{},developmentById:{},leagueById:{},
    titleState:{peakRpById:{},equippedById:{},earnedById:{}},
    player:{accuracy:93,accuracySum:0,accuracyMatches:0,recentMatches:[],onlineSongRangeTier:'beginner'},
    background:{liveMatches:[],serial:0,lastTick:now,elasticLiveRoomCap:1200,
      bossMatches:[],privateMatches:[],practiceById:{},spectatorsById:{},spectatorScanCursor:0},
    lastOpponentId:'',seasonResetNoticePending:true};
}
function pkeCompatValidateCheckpoint(value){
  if(!pkeCompatObject(value)||value.schema!==PKE_NATIVE_POPULATION_SCHEMA||value.version!==1)
    throw Error("Invalid Native population checkpoint schema");
  pkeCompatTime(value.now);pkeCompatTime(value.bootAt);
  const records=value.records,bg=records&&records.background;
  if(!pkeCompatObject(records)||records.seasonId!==2||records.version!==12||!pkeCompatObject(bg))
    throw Error("Invalid Native population records");
  for(const key of ['h2h','skinById','skinMoodById','developmentById','leagueById','titleState','player'])
    if(!pkeCompatObject(records[key]))throw Error("Invalid population map: "+key);
  for(const key of ['peakRpById','equippedById','earnedById'])
    if(!pkeCompatObject(records.titleState[key]))throw Error("Invalid population title map");
  if(!Number.isSafeInteger(bg.lastTick)||bg.lastTick<0||bg.lastTick>value.now||
     !Number.isSafeInteger(bg.serial)||bg.serial<0||!Array.isArray(value.history))
    throw Error("Invalid population checkpoint clock or history");
  for(const key of ['liveMatches','bossMatches','privateMatches'])
    if(!Array.isArray(bg[key])||bg[key].length>15000)throw Error("Invalid population room list");
  for(const key of ['practiceById','spectatorsById'])
    if(!pkeCompatObject(bg[key]))throw Error("Invalid population activity map");
  return value;
}
function pkeCompatMakeWorld(options,checkpoint=null){
  if(!pkeCompatObject(options))throw Error("Missing Native population options");
  const now=pkeCompatTime(checkpoint?checkpoint.now:options.now);
  const native={now,bootAt:checkpoint?checkpoint.bootAt:now,dirty:false,
    songs:pkeCompatCopy(Array.isArray(options.songs)?options.songs:[]),
    skins:Array.isArray(options.skins)?options.skins.map(String):[],
    bannedSkins:new Set(Array.isArray(options.bannedSkins)?options.bannedSkins.map(String):[]),
    playerName:String(options.playerName||'YOU'),playerSkin:String(options.playerSkin||''),
    records:checkpoint?pkeCompatCopy(checkpoint.records):pkeCompatRecords(now),
    history:checkpoint?pkeCompatCopy(checkpoint.history):[]};
  const HostDate=globalThis.Date;
  native.Date=class extends HostDate{
    constructor(...args){super(...(args.length?args:[native.now]));}
    static now(){return native.now;}
  };
  native.performance=Object.freeze({now:()=>native.now-native.bootAt});
  const logic=pkeCreatePopulationLogic(native);
  logic.initialize(!!checkpoint);
  return {native,logic,options:pkeCompatCopy(options)};
}
function pkeCompatWorld(){if(!pkePopulationWorld)throw Error("Population is not initialized");return pkePopulationWorld;}

function pkePopulationInitialize(options){
  const next=pkeCompatMakeWorld(options);
  pkePopulationWorld=next;
  return next.logic.summary();
}
function pkePopulationAdvanceTo(input){
  const world=pkeCompatWorld(),native=world.native,target=pkeCompatTime(input&&input.now);
  if(target<=native.now)return world.logic.summary();
  // Keep the reference five-second tick order, including every elapsed bucket.
  // Native PopulationRuntime will divide long catch-up into bounded calls.
  while(native.records.background.lastTick+5000<=target){
    native.now=native.records.background.lastTick+5000;
    world.logic.tick(native.now);
  }
  native.now=target;
  return world.logic.summary();
}
function pkePopulationSummary(){return pkeCompatWorld().logic.summary();}
function pkePopulationQuery(input){
  const ids=input&&input.ids;
  if(!Array.isArray(ids)||ids.length>128||ids.some(id=>id!=='YOU'&&
      (!/^\d{5}$/.test(id)||Number(id)<1||Number(id)>14999)))
    throw Error("Population queries require at most 128 valid player ids");
  return pkeCompatWorld().logic.query(ids);
}
function pkePopulationSerialize(){
  const {native,logic}=pkeCompatWorld();
  return pkeCompatCopy({schema:PKE_NATIVE_POPULATION_SCHEMA,version:1,
    now:native.now,bootAt:native.bootAt,records:native.records,history:logic.history()});
}
function pkePopulationRestore(input){
  const value=pkeCompatValidateCheckpoint(input);
  const options=pkePopulationWorld?pkePopulationWorld.options:{now:value.now,songs:[],skins:[]};
  const next=pkeCompatMakeWorld(options,value);
  pkePopulationWorld=next;
  return next.logic.summary();
}
function pkeHalloweenState(input){
  const now=pkeCompatTime(input&&input.now);
  const state=getBeatBattleHalloweenEventWindow(now);
  const override=input.override==='force-on'?'force-on':input.override==='force-off'?'force-off':'auto';
  return {...state,active:override==='force-on'||(override==='auto'&&state.active),
    override,teamTarget:getBeatBattleBossTeamTarget('halloween')};
}
function pkeSongDifficulty(input){
  return PKEModules.songDifficulty.analyzeSongDifficulty(input&&input.chart,input&&input.scrollSpeed);
}
function pkeTitleAvailability(input){return PkeBeatBattleTitles.getAvailableTitles(input.earned,input.stats);}
