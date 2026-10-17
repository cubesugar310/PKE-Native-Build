(function(root, factory) {
  const api = factory();
  if (typeof module === 'object' && module.exports) module.exports = api;
  if (root) root.PkeBeatBattleTitles = api;
})(typeof globalThis !== 'undefined' ? globalThis : this, function() {
  'use strict';

  const freezeList = items => Object.freeze(items.map(item => Object.freeze(item)));

  const RP_TITLES = freezeList([
    { id: 'rp-2000', minRp: 2000, name: 'ここからが本番', category: 'rp' },
    { id: 'rp-2500', minRp: 2500, name: '上位勢', category: 'rp' },
    { id: 'rp-2750', minRp: 2750, name: '強者の領域', category: 'rp' },
    { id: 'rp-2900', minRp: 2900, name: '頂点は近い', category: 'rp' },
    { id: 'rp-3000', minRp: 3000, name: '3000の壁を越えて', category: 'rp' },
    { id: 'rp-3100', minRp: 3100, name: '最上位', category: 'rp' },
    { id: 'rp-3200', minRp: 3200, name: '限界の先へ', category: 'rp' }
  ]);

  const WIN_TITLES = freezeList([
    { id: 'win-1', minWins: 1, name: '初勝利！', category: 'wins' },
    { id: 'win-10', minWins: 10, name: '勝ち慣れてきた', category: 'wins' },
    { id: 'win-50', minWins: 50, name: '歴戦のビートバトラー', category: 'wins' },
    { id: 'win-100', minWins: 100, name: '百勝', category: 'wins' },
    { id: 'win-200', minWins: 200, name: '勝利を重ねる者', category: 'wins' }
  ]);

  const MATCH_TITLES = freezeList([
    { id: 'match-1', minMatches: 1, name: 'はじめの一歩', category: 'matches' },
    { id: 'match-50', minMatches: 50, name: '常連', category: 'matches' },
    { id: 'match-100', minMatches: 100, name: '百戦錬磨', category: 'matches' },
    { id: 'match-250', minMatches: 250, name: 'やり込み勢', category: 'matches' }
  ]);

  const ACC_TITLES = freezeList([
    { id: 'acc-95', minAccuracy: 95, name: '精度派', category: 'accuracy' },
    { id: 'acc-98', minAccuracy: 98, name: '精度がすべて', category: 'accuracy' },
    { id: 'acc-99', minAccuracy: 99, name: 'ほぼ完璧', category: 'accuracy' }
  ]);

  const RANK_TITLES = freezeList([
    { id: 'rank-1000', maxRank: 1000, name: 'TOP1000', category: 'rank', dynamic: true },
    { id: 'rank-100', maxRank: 100, name: 'ランカー', category: 'rank', dynamic: true },
    { id: 'rank-10', maxRank: 10, name: '十傑', category: 'rank', dynamic: true },
    { id: 'rank-1', maxRank: 1, name: '頂点', category: 'rank', dynamic: true }
  ]);

  const ALL_TITLES = Object.freeze([
    ...RP_TITLES,
    ...WIN_TITLES,
    ...MATCH_TITLES,
    ...ACC_TITLES,
    ...RANK_TITLES,
  ]);
  const PERMANENT_TITLES = Object.freeze(ALL_TITLES.filter(item => !item.dynamic));
  const BY_ID = new Map(ALL_TITLES.map(item => [item.id, item]));

  function normalizeNumber(value, fallback = 0) {
    const number = Number(value);
    return Number.isFinite(number) ? number : fallback;
  }

  function normalizeRp(value) {
    return Math.max(0, Math.round(normalizeNumber(value, 0)));
  }

  function normalizeCount(value) {
    return Math.max(0, Math.floor(normalizeNumber(value, 0)));
  }

  function normalizeAccuracy(value) {
    return Math.max(0, Math.min(100, normalizeNumber(value, 0)));
  }

  function normalizeRank(value) {
    const rank = Math.floor(normalizeNumber(value, 0));
    return rank > 0 ? rank : 0;
  }

  function getPeakRp(savedPeakRp, currentRp) {
    return Math.max(normalizeRp(savedPeakRp), normalizeRp(currentRp));
  }

  function getUnlockedTitles(peakRp) {
    const rp = normalizeRp(peakRp);
    return RP_TITLES.filter(item => rp >= item.minRp);
  }

  function getTitleById(id) {
    return BY_ID.get(String(id || '')) || null;
  }

  function permanentTitleSatisfied(title, stats) {
    const source = stats && typeof stats === 'object' ? stats : {};
    if (title.category === 'rp') return normalizeRp(source.peakRp) >= title.minRp;
    if (title.category === 'wins') return normalizeCount(source.wins) >= title.minWins;
    if (title.category === 'matches') return normalizeCount(source.matches) >= title.minMatches;
    if (title.category === 'accuracy') return normalizeAccuracy(source.accuracy) >= title.minAccuracy;
    return false;
  }

  function mergeEarnedTitleIds(existingIds, stats) {
    const earned = new Set(Array.isArray(existingIds) ? existingIds.map(id => String(id || '')) : []);
    for (const title of PERMANENT_TITLES) {
      if (permanentTitleSatisfied(title, stats)) earned.add(title.id);
    }
    return ALL_TITLES.filter(title => !title.dynamic && earned.has(title.id)).map(title => title.id);
  }

  function getAvailableTitles(earnedIds, stats) {
    const earned = new Set(Array.isArray(earnedIds) ? earnedIds.map(id => String(id || '')) : []);
    const output = PERMANENT_TITLES.filter(title => earned.has(title.id));
    const rank = normalizeRank(stats && stats.rank);
    if (rank > 0) {
      for (const title of RANK_TITLES) {
        if (rank <= title.maxRank) output.push(title);
      }
    }
    return output;
  }

  function normalizeEquippedTitleId(id, peakRp) {
    const wanted = String(id || '');
    if (!wanted) return '';
    const title = getTitleById(wanted);
    return title && title.category === 'rp' && normalizeRp(peakRp) >= title.minRp ? title.id : '';
  }

  function normalizeEquippedTitleIdFromAvailable(id, availableTitles) {
    const wanted = String(id || '');
    if (!wanted) return '';
    return Array.isArray(availableTitles) && availableTitles.some(title => title && title.id === wanted) ? wanted : '';
  }

  function getRequirementLabel(title) {
    if (!title) return '';
    if (title.category === 'rp') return title.minRp + ' RP';
    if (title.category === 'wins') return title.minWins + '勝';
    if (title.category === 'matches') return title.minMatches + '戦';
    if (title.category === 'accuracy') return 'ACC ' + title.minAccuracy + '%';
    if (title.category === 'rank') return title.maxRank === 1 ? '世界1位' : 'TOP ' + title.maxRank;
    return '';
  }

  function hashUnit(value) {
    const text = String(value || '');
    let hash = 2166136261 >>> 0;
    for (let i = 0; i < text.length; i++) {
      hash ^= text.charCodeAt(i);
      hash = Math.imul(hash, 16777619) >>> 0;
    }
    return hash / 0x100000000;
  }

  function choosePopulationTitleId(playerId, unlockedOrAvailable) {
    const available = Array.isArray(unlockedOrAvailable)
      ? unlockedOrAvailable.filter(title => title && getTitleById(title.id))
      : getUnlockedTitles(unlockedOrAvailable);
    if (!available.length) return '';
    const unit = hashUnit('pke-title-v1|' + String(playerId || ''));
    const index = Math.min(available.length - 1, Math.floor(unit * available.length));
    return available[index].id;
  }

  return Object.freeze({
    RP_TITLES,
    WIN_TITLES,
    MATCH_TITLES,
    ACC_TITLES,
    RANK_TITLES,
    ALL_TITLES,
    getPeakRp,
    getUnlockedTitles,
    getTitleById,
    mergeEarnedTitleIds,
    getAvailableTitles,
    normalizeEquippedTitleId,
    normalizeEquippedTitleIdFromAvailable,
    getRequirementLabel,
    choosePopulationTitleId
  });
});
