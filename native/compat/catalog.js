// Native boundaries: no file APIs, DOM objects or application timers.
(() => {
  'use strict';
  const logic=PKENativeCatalog;
  let searchIndex=[],characterFiles=[];
  globalThis.pkeCatalogSetCharacterFiles=input=>{characterFiles=input.files;return characterFiles.length;};
  globalThis.pkeCatalogCharacterPlan=input=>{
    const json=JSON.parse(input.text),file=characterFiles[input.key];
    if(!json||!Array.isArray(json.animations)||!json.animations.some(anim=>{
      const key=String(anim?.anim||'').toLowerCase();
      return key==='idle'||key.startsWith('sing')||key==='danceleft'||key==='danceright';
    }))return null;
    const imageBase=logic.cleanAssetBaseName(json.image||'');if(!imageBase)return null;
    const directory=logic.getFileDirectoryPath(file);
    const xml=logic.characterFile(directory,imageBase,['.xml'],characterFiles);
    if(!xml)return null;
    const id=logic.cleanAssetBaseName(logic.fileBaseName(file)).toLowerCase();
    return {id,sourceSkinId:logic.buildDetectedBFSkinId('sparrow',directory,id),
      displayName:logic.getDetectedCharacterDisplayNameFromJson(json,file,imageBase),
      xml:xml.key,imageBase,directory};
  };
  globalThis.pkeCatalogCharacterImage=input=>{
    const xmlBase=logic.cleanAssetBaseName(input.xmlImagePath);
    return ((xmlBase&&logic.characterFile(input.directory,xmlBase,['.png','.webp'],characterFiles))||
      logic.characterFile(input.directory,input.imageBase,['.png','.webp'],characterFiles))?.key??-1;
  };
  globalThis.pkeCatalogPreferCharacter=input=>logic.preferSparrowDefinition(
    {jsonFile:characterFiles[input.candidate]},
    input.current>=0?{jsonFile:characterFiles[input.current]}:null);
  globalThis.pkeCatalogChart=input=>{
    const json=JSON.parse(input.text);
    const lightweight=logic.buildBeatBattleLightweightChartData(json,input.fallback);
    if(!lightweight)return null;
    const beatBattleDifficulty=PKEModules.songDifficulty.analyzeSongDifficulty(
      lightweight,logic.getFreeplayChartReferenceSpeed(json));
    for(const field of ['times','sides','lanes','lengths','rapidFlags','difficultyRegularFlags'])
      lightweight[field]=Array.from(lightweight[field]);
    return {displayName:logic.getFreeplaySongDisplayName(json,input.fallback),
      opponentCharacterId:logic.getFreeplayOpponentCharacterId(json),
      beatBattleLightweightChart:lightweight,beatBattleDifficulty};
  };
  globalThis.pkeCatalogFilePlan=input=>({
    charts:logic.getFreeplayChartCandidates(input.files,input.folder).map(f=>f.key),
    inst:logic.chooseFreeplayAudioFile(input.files,'inst')?.key??-1,
    voices:logic.chooseFreeplayAudioFile(input.files,'voices')?.key??-1
  });
  globalThis.pkeCatalogSort=input=>input.songs.slice().sort((a,b)=>
    a.displayName.localeCompare(b.displayName,undefined,{numeric:true,sensitivity:'base'})).map(s=>s.key);
  globalThis.pkeCatalogIcon=input=>logic.icon({opponentCharacterId:input.opponent,files:input.files},
    characterFiles.map(file=>({...file,key:input.files.length+file.key})))?.key??-1;
  globalThis.pkeCatalogSetSearch=input=>{
    searchIndex=input.songs.map(song=>({
      text:logic.normalizeBeatBattlePracticeSearch([song.displayName,song.folderName,song.chartFile?.name].filter(Boolean).join(' ')),
      player:Math.floor(Number(song.beatBattleDifficulty?.player?.stars)),
      opponent:Math.floor(Number(song.beatBattleDifficulty?.opponent?.stars))
    }));
    return searchIndex.length;
  };
  globalThis.pkeCatalogSearch=input=>{
    const terms=logic.normalizeBeatBattlePracticeSearch(input.query).split(' ').filter(Boolean);
    const selectedStars=input.stars?Math.floor(Number(input.stars)):0;
    const side=input.side==='opponent'?'opponent':'player',result=[];
    searchIndex.forEach((song,index)=>{
      const stars=song[side]>=1&&song[side]<=7?song[side]:0;
      if(selectedStars&&stars!==selectedStars)return;
      if(terms.every(term=>song.text.includes(term)))result.push(index);
    });
    return result;
  };
})();
