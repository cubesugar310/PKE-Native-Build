const fs=require('fs'),path=require('path'),vm=require('vm');
const nativeRoot=path.resolve(__dirname,'../..'),ctx=vm.createContext({});
for(const file of ['cpu-timing.js','title-system.js','population-social-sessions.js','boss-battle.js'])
 vm.runInContext(fs.readFileSync(nativeRoot+'/compat/'+file,'utf8'),ctx);
vm.runInContext(fs.readFileSync(__dirname+'/v112-song-difficulty.js','utf8'),ctx);
vm.runInContext(fs.readFileSync(__dirname+'/v112-population-async.js','utf8'),ctx);
vm.runInContext(`(async()=>{
 let randomState=123456789;
 Math.random=function(){let t=randomState=(randomState+0x6D2B79F5)>>>0;t=Math.imul(t^(t>>>15),t|1);t^=t+Math.imul(t^(t>>>7),t|61);return ((t^(t>>>14))>>>0)/4294967296;};
 const songs=[];
 for(let kind=0;kind<3;kind++){
   const times=Array.from({length:16},(_,i)=>kind===0?i*500:kind===1?Math.floor(i/4)*600:i*65);
   const sides=Array.from({length:16},(_,i)=>kind===0?i%2:0);
   const lanes=Array.from({length:16},(_,i)=>kind===2?1:i%4);
   const data={times,sides,lanes,rapidFlags:Array(16).fill(kind===2?1:0),durationMs:12000,baseBpm:140,
     playerNoteCount:sides.filter(s=>s===0).length,opponentNoteCount:sides.filter(s=>s===1).length,
     playerRapidRatio:kind===2?1:0,opponentRapidRatio:0,runtimeSongName:'Synthetic '+kind};
   const rating=await PKEModules.songDifficulty.analyzeSongDifficulty(data,1,{now:()=>0,yieldToBrowser:()=>Promise.resolve()});
   songs.push({folderName:'fixture-'+kind,displayName:'Synthetic '+kind,chartFile:{name:'chart.json'},instFile:{name:'Inst.ogg'},
     opponentCharacterId:'fixture-dad',beatBattleDifficulty:rating,beatBattleLightweightChart:data});
 }
 const input={now:1791989999000,songs,skins:['bf','dad']};
 const native={now:input.now,bootAt:input.now,dirty:false,songs:JSON.parse(JSON.stringify(songs)),skins:input.skins,bannedSkins:new Set(),playerName:'YOU',playerSkin:'',records:pkeCompatRecords(input.now),history:[]};
 const HostDate=Date;native.Date=class extends HostDate{constructor(...args){super(...(args.length?args:[native.now]));}static now(){return native.now;}};
 native.performance={now:()=>native.now-native.bootAt};
 const logic=createReferencePopulationLogic(native);
 await logic.initialize();
 for(let i=1;i<=6;i++){native.now=input.now+i*5000;await logic.tick(native.now);}
 let h=0x811c9dc5;const text=JSON.stringify(native.records);
 for(let i=0;i<text.length;i++){h^=text.charCodeAt(i);h=Math.imul(h,0x01000193)>>>0;}
 const history=logic.history();
 return {source:'V1.12 original async calculations with synthetic authored charts',input,elapsed:30000,
   expected:{summary:logic.summary(),recordsHash:h.toString(16).padStart(8,'0'),randomState,historyCount:history.length,
     liveCompletions:history.filter(m=>m.source==='live').length,
     roomSamples:native.records.background.liveMatches.slice(0,3)}};
})()`,ctx).then(value=>{
 fs.writeFileSync(nativeRoot+'/tests/fixtures/population/v112-catalog.json',JSON.stringify(value,null,2)+'\n');
 console.log(JSON.stringify({summary:value.expected.summary,historyCount:value.expected.historyCount,liveCompletions:value.expected.liveCompletions}));
}).catch(e=>{console.error(e);process.exitCode=1;});
