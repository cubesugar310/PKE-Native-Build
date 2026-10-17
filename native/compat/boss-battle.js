(function(root,factory){
  const api=factory();
  if(typeof module==='object'&&module.exports) module.exports=api;
  if(root) root.PKEBossBattle=api;
})(typeof globalThis!=='undefined'?globalThis:this,function(){
  'use strict';

  const PLAYER_MAX=100000;
  const TEAM_MAX=300000;
  const BOSS_TARGET=276000;
  const ACE_TARGET=92000;
  const RULE_TEAM='TEAM';
  const RULE_ACE='ACE';
  const JST_OFFSET_MS=9*60*60*1000;
  const DAY_MS=24*60*60*1000;

  function clamp(v,a,b){ return Math.max(a,Math.min(b,v)); }
  function clone(value){ return value==null?value:JSON.parse(JSON.stringify(value)); }
  function pad2(n){ return String(n).padStart(2,'0'); }

  function getJstParts(ts=Date.now()){
    const d=new Date(Number(ts)+JST_OFFSET_MS);
    return {
      year:d.getUTCFullYear(), month:d.getUTCMonth()+1, day:d.getUTCDate(),
      hour:d.getUTCHours(), minute:d.getUTCMinutes(), second:d.getUTCSeconds(),
      millisecond:d.getUTCMilliseconds(), weekday:d.getUTCDay()
    };
  }

  function getJstDayStartUtcMs(ts){
    const p=getJstParts(ts);
    return Date.UTC(p.year,p.month-1,p.day)-JST_OFFSET_MS;
  }

  function getMondayStart(ts=Date.now()){
    const p=getJstParts(ts);
    const dayStart=getJstDayStartUtcMs(ts);
    const daysSinceMonday=(p.weekday+6)%7;
    return dayStart-daysSinceMonday*DAY_MS;
  }

  function getWeekKey(ts=Date.now()){
    const monday=getJstParts(getMondayStart(ts));
    return monday.year+'-'+pad2(monday.month)+'-'+pad2(monday.day);
  }

  function getEventState(ts=Date.now()){
    const now=Number(ts);
    const monday=getMondayStart(now);
    const start=monday+4*DAY_MS;
    const end=monday+7*DAY_MS;
    const active=now>=start&&now<end;
    const nextStart=active?start:(now<start?start:start+7*DAY_MS);
    return {active,weekKey:getWeekKey(now),startMs:start,endMs:end,nextStartMs:nextStart,remainingMs:Math.max(0,(active?end:nextStart)-now)};
  }

  function hashString(text){
    let h=2166136261>>>0;
    for(const ch of String(text||'')){
      h^=ch.charCodeAt(0);
      h=Math.imul(h,16777619)>>>0;
    }
    return h>>>0;
  }

  function makeSeededRandom(seed){
    let state=(seed>>>0)||0x6d2b79f5;
    return function(){
      state=(state+0x6D2B79F5)>>>0;
      let t=state;
      t=Math.imul(t^(t>>>15),t|1);
      t^=t+Math.imul(t^(t>>>7),t|61);
      return ((t^(t>>>14))>>>0)/4294967296;
    };
  }

  function songStableKey(song){
    return String(song&&song.id||'')+'\u0000'+String(song&&song.name||'');
  }

  function selectWeeklySongs(songs,ts=Date.now()){
    const eligible=(Array.isArray(songs)?songs:[])
      .filter(s=>s&&s.eligible!==false&&(Number(s.stars)===6||Number(s.stars)===7))
      .sort((a,b)=>songStableKey(a).localeCompare(songStableKey(b)));
    const unique=[];
    const seen=new Set();
    for(const song of eligible){
      const key=songStableKey(song);
      if(seen.has(key)) continue;
      seen.add(key);
      unique.push(song);
    }
    const weekKey=getWeekKey(ts);
    if(unique.length<3) return {ok:false,reason:'not-enough-eligible-songs',weekKey,songs:[]};
    const rand=makeSeededRandom(hashString('pke-boss-week-v1|'+weekKey));
    const pool=unique.slice();
    for(let i=pool.length-1;i>0;i--){
      const j=Math.floor(rand()*(i+1));
      const tmp=pool[i]; pool[i]=pool[j]; pool[j]=tmp;
    }
    return {ok:true,weekKey,songs:pool.slice(0,3).map(clone)};
  }

  function chooseBattleSong(weekly,randomFn=Math.random){
    const list=Array.isArray(weekly)?weekly:(weekly&&Array.isArray(weekly.songs)?weekly.songs:[]);
    if(!list.length) return null;
    const r=clamp(Number(randomFn())||0,0,0.999999999999);
    return clone(list[Math.floor(r*list.length)]);
  }

  function normalizePlayerScore(raw,maxRaw){
    const max=Number(maxRaw);
    if(!(max>0)) return 0;
    return Math.round(clamp((Number(raw)||0)/max,0,1)*PLAYER_MAX);
  }
  function getTeamScore(scores){
    return Math.round((Array.isArray(scores)?scores:[]).reduce((sum,v)=>sum+clamp(Math.round(Number(v)||0),0,PLAYER_MAX),0));
  }
  function normalizeRule(rule){ return rule===RULE_ACE?RULE_ACE:RULE_TEAM; }
  function chooseBattleRule(randomFn=Math.random){
    const r=clamp(Number(randomFn())||0,0,0.999999999999);
    return r<0.5?RULE_TEAM:RULE_ACE;
  }
  function getBossTarget(rule=RULE_TEAM){
    return normalizeRule(rule)===RULE_ACE?ACE_TARGET:BOSS_TARGET;
  }
  function capAllyScoreForRule(score,rule=RULE_TEAM){
    // Allies always keep their real Online-equivalent score. ACE_TARGET is a
    // victory threshold and Boss target, not a per-player score ceiling.
    void rule;
    return clamp(Math.round(Number(score)||0),0,PLAYER_MAX);
  }
  function getBossScore(progress,complete=false,rule=RULE_TEAM){
    const target=getBossTarget(rule);
    if(complete) return target;
    return Math.round(clamp(Number(progress)||0,0,1)*target);
  }
  function isVictory(teamScore){ return Math.round(Number(teamScore)||0)>=BOSS_TARGET; }
  function isRuleVictory(rule,scores){
    const safe=(Array.isArray(scores)?scores:[]).map(v=>clamp(Math.round(Number(v)||0),0,PLAYER_MAX));
    if(normalizeRule(rule)===RULE_ACE) return safe.some(v=>v>=ACE_TARGET);
    return getTeamScore(safe)>=BOSS_TARGET;
  }

  function normalizeParticipant(p){
    if(!p||typeof p!=='object') return null;
    const id=String(p.id||'').trim();
    if(!id) return null;
    return clone({...p,id});
  }
  function createParty(host){
    const h=normalizeParticipant(host);
    return {host:h,friends:[],wilds:[],participants:[h,null,null]};
  }
  function setPartyFriends(party,friends){
    const base=createParty(party&&party.host);
    const seen=new Set(base.host?[base.host.id]:[]);
    const kept=[];
    for(const item of Array.isArray(friends)?friends:[]){
      const p=normalizeParticipant(item);
      if(!p||seen.has(p.id)||kept.length>=2) continue;
      seen.add(p.id); kept.push(p);
    }
    base.friends=kept;
    base.participants=[base.host,kept[0]||null,kept[1]||null];
    return base;
  }
  function planWildFill(party,candidates,randomFn=Math.random){
    const base=setPartyFriends(party,party&&party.friends);
    const need=3-base.participants.filter(Boolean).length;
    const used=new Set(base.participants.filter(Boolean).map(p=>p.id));
    const pool=[];
    for(const item of Array.isArray(candidates)?candidates:[]){
      const p=normalizeParticipant(item);
      if(!p||used.has(p.id)||pool.some(x=>x.id===p.id)) continue;
      pool.push(p);
    }
    const wilds=[];
    while(wilds.length<need&&pool.length){
      const r=clamp(Number(randomFn())||0,0,0.999999999999);
      const index=Math.floor(r*pool.length);
      const picked=pool.splice(index,1)[0];
      if(picked) wilds.push(picked);
    }
    const participants=base.participants.slice();
    for(const wild of wilds){
      const slot=participants.findIndex(v=>!v);
      if(slot>=0) participants[slot]=wild;
    }
    return {...base,wilds,participants};
  }
  function restorePartyAfterMatch(party){
    return setPartyFriends(createParty(party&&party.host),party&&party.friends);
  }

  return {
    BOSS_TARGET,ACE_TARGET,RULE_TEAM,RULE_ACE,PLAYER_MAX,TEAM_MAX,
    getJstParts,getEventState,getWeekKey,selectWeeklySongs,chooseBattleSong,
    normalizePlayerScore,getTeamScore,chooseBattleRule,getBossTarget,capAllyScoreForRule,getBossScore,isVictory,isRuleVictory,
    createParty,setPartyFriends,planWildFill,restorePartyAfterMatch
  };
});
