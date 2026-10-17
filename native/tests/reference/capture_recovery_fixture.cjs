// Capture the original async tick once at the latest coalesced target, as the
// browser's request/drain queue does after a long suspended interval.
const fs=require('fs'),vm=require('vm'),path=require('path');
const root=path.resolve(__dirname,'../..'),ctx=vm.createContext({});
for(const f of ['cpu-timing.js','title-system.js','population-social-sessions.js','boss-battle.js','song-difficulty.js'])
 vm.runInContext(fs.readFileSync(path.join(root,'compat',f),'utf8'),ctx);
vm.runInContext(fs.readFileSync(path.join(__dirname,'v112-population-async.js'),'utf8'),ctx);
vm.runInContext(`(async()=>{
 let randomState=123456789;
 Math.random=function(){let t=randomState=(randomState+0x6D2B79F5)>>>0;t=Math.imul(t^(t>>>15),t|1);t^=t+Math.imul(t^(t>>>7),t|61);return ((t^(t>>>14))>>>0)/4294967296;};
 const start=1791989999000,gapMs=21600000;
 const native={now:start,bootAt:start,dirty:false,songs:[],skins:[],bannedSkins:new Set(),playerName:'YOU',playerSkin:'',records:pkeCompatRecords(start),history:[]};
 const HostDate=Date;native.Date=class extends HostDate{constructor(...args){super(...(args.length?args:[native.now]));}static now(){return native.now;}};
 native.performance={now:()=>native.now-native.bootAt};
 const logic=createReferencePopulationLogic(native);await logic.initialize();
 native.now=start+gapMs;await logic.tick(native.now);
 const summary=logic.summary(),profiles=logic.query(['00001','04242','14999']);
 let hash=0x811c9dc5;const text=JSON.stringify(native.records);
 for(let i=0;i<text.length;i++){hash^=text.charCodeAt(i);hash=Math.imul(hash,0x01000193)>>>0;}
 return {source:'V1.12 original async single-drain recovery',clock:start,gapMs,
   expected:{summary,profiles,recordsHash:hash.toString(16).padStart(8,'0'),randomState,historyCount:logic.history().length}};
})()`,ctx).then(value=>{
 fs.writeFileSync(path.join(root,'tests/fixtures/population/v112-recovery.json'),JSON.stringify(value,null,2)+'\n');
 console.log(JSON.stringify({gapMs:value.gapMs,...value.expected.summary,recordsHash:value.expected.recordsHash}));
}).catch(error=>{console.error(error);process.exitCode=1;});
