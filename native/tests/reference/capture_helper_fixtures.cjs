const fs=require('fs'),vm=require('vm');
const path=require('path'),nativeRoot=path.resolve(__dirname,'../..'),ctx=vm.createContext({});
for(const file of [__dirname+'/v112-song-difficulty.js',nativeRoot+'/compat/title-system.js'])vm.runInContext(fs.readFileSync(file,'utf8'),ctx);
const result=vm.runInContext(`(async()=>{
 const cases=[];
 function add(name,times,sides,lanes,scrollSpeed=1,flags=null){const chart={times,sides,lanes};if(flags)chart.difficultyRegularFlags=flags;cases.push({name,input:{chart,scrollSpeed}});}
 add('empty',[],[],[]);
 add('alternating sparse',Array.from({length:40},(_,i)=>i*500),Array.from({length:40},(_,i)=>i%2),Array.from({length:40},(_,i)=>i%4),1.25);
 add('duplicates and non-regular heads',[0,0,0.005,0.02,500,1000,1500],[0,0,0,0,0,1,1],[0,0,0,0,1,2,3],2,[1,1,1,1,0,1,1]);
 add('repeated four-note chords',Array.from({length:64},(_,i)=>Math.floor(i/4)*600),Array(64).fill(0),Array.from({length:64},(_,i)=>i%4),1.4);
 add('sustained fast jack',Array.from({length:64},(_,i)=>i*65),Array(64).fill(0),Array(64).fill(1),2);
 add('single very dense burst',[...Array.from({length:28},(_,i)=>i*22),...Array.from({length:20},(_,i)=>2000+i*1000)],Array(48).fill(1),Array.from({length:48},(_,i)=>i%4),1);
 for(const item of cases)item.expected=await PKEModules.songDifficulty.analyzeSongDifficulty(item.input.chart,item.input.scrollSpeed,{now:()=>0,yieldToBrowser:()=>Promise.resolve()});
 const titleInputs=[{stats:{rank:1},earned:['win-1','rp-2000']},{stats:{rank:20},earned:['match-100','acc-99']},{stats:{rank:0},earned:['unknown','win-10']}];
 const titles=titleInputs.map(input=>({input,expected:PkeBeatBattleTitles.getAvailableTitles(input.earned,input.stats)}));
 return {cases,titles};
})()`,ctx);
result.then(r=>{const dir=nativeRoot+'/tests/fixtures/population';fs.writeFileSync(dir+'/v112-difficulty.json',JSON.stringify(r.cases,null,2)+'\n');fs.writeFileSync(dir+'/v112-titles.json',JSON.stringify(r.titles,null,2)+'\n');console.log(JSON.stringify({difficulty:r.cases.map(x=>({name:x.name,stars:x.expected.maxStars})),titles:r.titles.map(x=>x.expected.map(t=>t.id))}));}).catch(e=>{console.error(e);process.exitCode=1;});
