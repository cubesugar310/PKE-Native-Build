const fs=require('fs'),vm=require('vm');
const path=require('path');
const nativeRoot=path.resolve(__dirname,'../..');
const ctx=vm.createContext({});
for(const f of ['cpu-timing.js','title-system.js','population-social-sessions.js','boss-battle.js'])
 vm.runInContext(fs.readFileSync(nativeRoot+'/compat/'+f,'utf8'),ctx,{filename:'reference/'+f});
// Pure difficulty helpers are independent of the population fixture's empty
// catalog. The reference's original async analyzer is captured separately.
vm.runInContext(fs.readFileSync(nativeRoot+'/compat/song-difficulty.js','utf8'),ctx);

vm.runInContext(fs.readFileSync(__dirname+'/v112-population-async.js','utf8'),ctx);
const result=vm.runInContext(`(async()=>{
 let randomState=123456789;
 Math.random=function(){let t=randomState=(randomState+0x6D2B79F5)>>>0;t=Math.imul(t^(t>>>15),t|1);t^=t+Math.imul(t^(t>>>7),t|61);return ((t^(t>>>14))>>>0)/4294967296;};
 const start=1791989999000;
 const native={now:start,bootAt:start,dirty:false,songs:[],skins:[],bannedSkins:new Set(),playerName:'YOU',playerSkin:'',records:pkeCompatRecords(start),history:[]};
 const HostDate=Date;
 native.Date=class extends HostDate{constructor(...args){super(...(args.length?args:[native.now]));}static now(){return native.now;}};
 native.performance={now:()=>native.now-native.bootAt};
 const logic=createReferencePopulationLogic(native);
 function fingerprint(value){let h=0x811c9dc5;const s=JSON.stringify(value);for(let i=0;i<s.length;i++){h^=s.charCodeAt(i);h=Math.imul(h,0x01000193)>>>0;}return h.toString(16).padStart(8,'0');}
 const ids=['00001','04242','10000','10001','14999'];
 function snapshot(){return JSON.parse(JSON.stringify({summary:logic.summary(),profiles:logic.query(ids),recordsHash:fingerprint(native.records),randomState,historyCount:logic.history().length}));}
 await logic.initialize();
 const transitions=[];
 for(let i=1;i<=14999&&transitions.length<6;i++){const id=String(i).padStart(5,'0');native.now=start;const from=logic.query([id])[0].baseStatus;native.now=start+60000;const to=logic.query([id])[0].baseStatus;if(from!==to){transitions.push({id,from,to});if(!ids.includes(id))ids.push(id);}}
 native.now=start;
 const startup=snapshot();
 for(let i=1;i<=12;i++){native.now=start+i*5000;await logic.tick(native.now);}
 const oneMinute=snapshot();
 const presence=[];
 for(const elapsed of [0,480000,1800000]){native.now=start+elapsed;presence.push({elapsed,players:logic.query(['00001','00002','00003','00004','00005','04242','10001','14999']).map(p=>({id:p.profile.id,baseStatus:p.baseStatus}))});}
 return {source:'V1.12 embedded original async calculations, Native IO/catalog boundary only',referenceZipSha256:'29294463b238e10815c017ac2496567de24b91f4f1d6326f770d81f1125fac3c',clock:start,seed:123456789,transitions,startup,oneMinute,presence};
})()`,ctx);
result.then(value=>{const dir=nativeRoot+'/tests/fixtures/population';fs.mkdirSync(dir,{recursive:true});fs.writeFileSync(dir+'/v112-population.json',JSON.stringify(value,null,2)+'\n');console.log(JSON.stringify({startup:value.startup.summary,minute:value.oneMinute.summary,transitions:value.transitions,presence:value.presence}));}).catch(e=>{console.error(e);process.exitCode=1;});
