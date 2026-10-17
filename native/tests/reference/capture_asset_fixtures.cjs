const fs=require('fs'),path=require('path'),vm=require('vm');
const root=path.resolve(__dirname,'../..'),context=vm.createContext({});
for(const f of [root+'/compat/chart-utils.js',__dirname+'/v112-song-difficulty.js',__dirname+'/v112-catalog.js'])
  vm.runInContext(fs.readFileSync(f,'utf8'),context);
vm.runInContext(`(async()=>{
 const charts=[];
 function add(name,input,fallback='folder'){charts.push({name,input,fallback});}
 add('section ownership and sustain',{song:{song:'Original',bpm:400,speed:1.6,scrollSpeed:9,player2:'characters/DAD.json',notes:[{mustHitSection:true,sectionNotes:[[0,0,200],[65,1,0],[100,4,0],[100,5,0],[200,0,0,'Hurt Note'],[300,1,0,'Alt Animation'],[400,2,0,'GF Sing'],[500,3,0,'Normal Note']]}]}});
 add('absolute converted ownership',{song:{format:'psych_v1_convert',songName:'Converted',bpm:20,notes:[{mustHitSection:false,sectionNotes:[[1000,0,50],[1000,4,0],[1035,1,0],[1205,2,0],[1376,3,0]]}]}});
 add('custom types and speed',{name:'Custom',speed:2.4,opponent:'dad',notes:[{time:100,lane:0,length:250},{time:135,lane:1,side:'enemy'},{time:305,lane:2,owner:'DAD'},{time:340,lane:3,opponent:true,noteType:'Mine'},{time:500,lane:0,noteType:'Hey!'},{time:600,lane:1,noteType:0},{time:700,lane:4},{time:800,lane:2.5}]});
 add('custom bare array',[{time:100,lane:0},{time:200,lane:1,opponent:true}]);
 add('empty chart',{song:{notes:[{sectionNotes:[[0,0,0,'Hurt']]}]}});
 add('dense regular jack',{name:'Jack',notes:Array.from({length:64},(_,i)=>({time:i*65,lane:1}))});
 for(const c of charts){const lightweight=V112Catalog.buildBeatBattleLightweightChartData(c.input,c.fallback);
  if(!lightweight){c.expected=null;continue;}
  const rating=await PKEModules.songDifficulty.analyzeSongDifficulty(lightweight,V112Catalog.getFreeplayChartReferenceSpeed(c.input),{now:()=>0,yieldToBrowser:()=>Promise.resolve()});
  for(const field of ['times','sides','lanes','lengths','rapidFlags','difficultyRegularFlags'])lightweight[field]=Array.from(lightweight[field]);
  c.expected={displayName:V112Catalog.getFreeplaySongDisplayName(c.input,c.fallback),opponentCharacterId:V112Catalog.getFreeplayOpponentCharacterId(c.input),beatBattleLightweightChart:lightweight,beatBattleDifficulty:rating};
 }
 const songs=[{displayName:'ＴＥＳＴ カタカナ',folderName:'wide_name',chartFile:{name:'chart-hard.json'},beatBattleDifficulty:{player:{stars:7},opponent:{stars:1}}},{displayName:'Test 2',folderName:'other-folder',chartFile:{name:'chart.json'},beatBattleDifficulty:{player:{stars:1},opponent:{stars:3}}},{displayName:'ΣΟΣ é',folderName:'accent',chartFile:{name:'data.json'},beatBattleDifficulty:{player:{stars:2},opponent:{stars:2}}}];
 const search=[{name:'compatibility width kana',input:{query:'test ｶﾀｶﾅ',side:'player',stars:0}},{name:'all terms separator case',input:{query:'NAME　ＷＩＤＥ',side:'player',stars:0}},{name:'opponent star',input:{query:'',side:'opponent',stars:1}},{name:'player star',input:{query:'test',side:'player',stars:7}},{name:'greek contextual lower',input:{query:'σος',side:'player',stars:0}},{name:'canonical combining',input:{query:'e\\u0301',side:'player',stars:0}},{name:'no match',input:{query:'absent',side:'player',stars:0}}];
 for(const c of search)c.expected=V112Catalog.search(c.input,songs);
 return {source:'V1.12 exact catalog functions and original async difficulty, Node V8',charts,songs,search};
})()`,context).then(value=>{const dir=root+'/tests/fixtures/assets';fs.mkdirSync(dir,{recursive:true});fs.writeFileSync(dir+'/v112-catalog.json',JSON.stringify(value,null,2)+'\n');console.log(JSON.stringify({charts:value.charts.length,search:value.search.map(c=>({name:c.name,indices:c.expected}))}));}).catch(e=>{console.error(e);process.exitCode=1;});
