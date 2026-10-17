(function(root,factory){
  const api=factory();
  if(typeof module==='object'&&module.exports) module.exports=api;
  if(root) root.PKEPopulationSocial=api;
})(typeof globalThis!=='undefined'?globalThis:this,function(){
  'use strict';

  function validPopulationId(value){
    const id=String(value||'');
    return /^\d{5}$/.test(id)&&Number(id)>=1&&Number(id)<=15000;
  }

  function normalizeSongFields(session){
    session.songFolder=String(session.songFolder||'').slice(0,200);
    session.songName=String(session.songName||'Song').slice(0,160);
    const stars=Math.floor(Number(session.songStars)||0);
    session.songStars=stars>=1&&stars<=7?stars:0;
  }

  function validTimes(session,now){
    const start=Number(session&&session.startedAt);
    const end=Number(session&&session.endsAt);
    return Number.isFinite(start)&&Number.isFinite(end)&&start<=now&&end>now&&end<=now+3600000;
  }

  function normalize(bg,now=Date.now(),eventState={active:false,weekKey:''}){
    if(!bg||typeof bg!=='object') return bg;
    if(!Array.isArray(bg.bossMatches)) bg.bossMatches=[];
    if(!Array.isArray(bg.privateMatches)) bg.privateMatches=[];
    const active=!!(eventState&&eventState.active);
    const weekKey=String(eventState&&eventState.weekKey||'');
    const used=new Set();

    bg.bossMatches=bg.bossMatches.filter(match=>{
      if(!active||!match||typeof match!=='object'||Array.isArray(match)||!validTimes(match,now)) return false;
      if(String(match.weekKey||'')!==weekKey) return false;
      const ids=Array.isArray(match.playerIds)?match.playerIds.map(String):[];
      if(ids.length!==3||new Set(ids).size!==3||ids.some(id=>!validPopulationId(id)||used.has(id))) return false;
      match.playerIds=ids;
      match.id=String(match.id||'').slice(0,180);
      match.weekKey=weekKey;
      normalizeSongFields(match);
      ids.forEach(id=>used.add(id));
      return true;
    });

    bg.privateMatches=bg.privateMatches.filter(match=>{
      if(!match||typeof match!=='object'||Array.isArray(match)||!validTimes(match,now)) return false;
      const aId=String(match.aId||''), bId=String(match.bId||'');
      if(!validPopulationId(aId)||!validPopulationId(bId)||aId===bId||used.has(aId)||used.has(bId)) return false;
      match.aId=aId;
      match.bId=bId;
      match.id=String(match.id||'').slice(0,180);
      normalizeSongFields(match);
      used.add(aId); used.add(bId);
      return true;
    });
    return bg;
  }

  function getBossMatch(bg,playerId,now=Date.now(),eventState={active:false,weekKey:''}){
    const id=String(playerId||'');
    if(!validPopulationId(id)||!(eventState&&eventState.active)) return null;
    const weekKey=String(eventState.weekKey||'');
    return (bg&&Array.isArray(bg.bossMatches)?bg.bossMatches:[]).find(match=>
      match&&String(match.weekKey||'')===weekKey&&validTimes(match,now)&&Array.isArray(match.playerIds)&&match.playerIds.map(String).includes(id)
    )||null;
  }

  function getPrivateMatch(bg,playerId,now=Date.now()){
    const id=String(playerId||'');
    if(!validPopulationId(id)) return null;
    return (bg&&Array.isArray(bg.privateMatches)?bg.privateMatches:[]).find(match=>
      match&&validTimes(match,now)&&(String(match.aId||'')===id||String(match.bId||'')===id)
    )||null;
  }

  function getStatus(bg,playerId,now=Date.now(),eventState={active:false,weekKey:''}){
    if(getBossMatch(bg,playerId,now,eventState)) return 'BOSS BATTLE';
    if(getPrivateMatch(bg,playerId,now)) return 'PRIVATE MATCH';
    return '';
  }

  function getPrivateOpponentId(match,playerId){
    if(!match) return '';
    const id=String(playerId||'');
    const aId=String(match.aId||''), bId=String(match.bId||'');
    if(id===aId) return bId;
    if(id===bId) return aId;
    return '';
  }

  function formatPrivateOpponentLabel(match,playerId,profileLookup){
    const opponentId=getPrivateOpponentId(match,playerId);
    if(!opponentId) return '';
    const profile=typeof profileLookup==='function'?profileLookup(opponentId):null;
    const name=String(profile&&profile.name||'Player').replace(/[\t\r\n]+/g,' ').trim()||'Player';
    return name+'#'+opponentId;
  }

  return { normalize,getBossMatch,getPrivateMatch,getStatus,getPrivateOpponentId,formatPrivateOpponentLabel };
});
