"use strict";
// ============================================================
//  EMPIRE MONDIAL — grande stratégie par provinces
//  Carte : 3 300 provinces réelles, relief, villes, fleuves.
// ============================================================
const MONTHS_S = ["Janv.", "Févr.", "Mars", "Avr.", "Mai", "Juin", "Juil.", "Août", "Sept.", "Oct.", "Nov.", "Déc."];
const TERRAIN = [{ n: "Plaine", ic: "🌾", d: 1 }, { n: "Collines", ic: "⛰️", d: 1.3 }, { n: "Montagnes", ic: "🏔️", d: 1.7 }];
const BUILDINGS = {
  base: { n: "Base militaire", ic: "🪖", cost: 3, max: 3, d: "Recrutement -30 % / niveau, +10 % défense, +5 000 soldats de réserve" },
  fort: { n: "Fortifications", ic: "🏰", cost: 4, max: 3, d: "+30 % de défense par niveau" },
  usine: { n: "Zone industrielle", ic: "🏭", cost: 5, max: 3, d: "+15 % de PIB de la province par niveau" },
  port: { n: "Port militaire", ic: "⚓", cost: 4, max: 1, d: "Permet les débarquements navals depuis cette province" },
  aero: { n: "Base aérienne", ic: "✈️", cost: 6, max: 1, d: "+20 % d'attaque dans un rayon, frappes aériennes possibles" },
  univ: { n: "Université", ic: "🎓", cost: 4, max: 2, d: "+recherche, +stabilité" },
  hopital: { n: "Hôpital", ic: "🏥", cost: 3, max: 2, d: "+stabilité et +popularité" },
  silo: { n: "Silo nucléaire", ic: "☢️", cost: 25, max: 1, d: "Permet les frappes nucléaires (technologie requise)", tech: ["nuc", 1] },
  radar: { n: "Bouclier antimissile", ic: "📡", cost: 12, max: 1, d: "Protège la province et ses voisines des frappes", tech: ["def", 3] },
};
const TECHS = {
  arm: { n: "Armement", ic: "🔫", d: "+10 % d'attaque par niveau", max: 6 },
  def: { n: "Défense", ic: "🛡️", d: "+10 % de défense par niveau", max: 6 },
  eco: { n: "Économie", ic: "📈", d: "+8 % de revenus par niveau", max: 6 },
  nav: { n: "Marine", ic: "🚢", d: "+portée des débarquements, -pénalité navale", max: 5 },
  air: { n: "Aviation", ic: "🛩️", d: "+efficacité des frappes aériennes", max: 5 },
  esp: { n: "Renseignement", ic: "🕵️", d: "+réussite de l'espionnage", max: 5 },
  med: { n: "Médecine", ic: "⚕️", d: "+croissance, +stabilité", max: 5 },
  nuc: { n: "Nucléaire", ic: "☢️", d: "Débloque les silos nucléaires (niv. 1), dissuasion", max: 3 },
  prop: { n: "Propagande", ic: "📺", d: "+popularité, -révoltes", max: 4 },
};
const BLOCS = {
  otan: "USA CAN GBR FRA DEU ITA ESP PRT NLD BEL LUX NOR DNK ISL POL CZE SVK HUN ROU BGR GRC TUR EST LVA LTU SVN HRV ALB MNE MKD FIN SWE".split(" "),
  otsc: "RUS BLR KAZ KGZ TJK ARM".split(" "),
  ue: "FRA DEU ITA ESP PRT NLD BEL LUX AUT IRL DNK SWE FIN POL CZE SVK HUN ROU BGR GRC EST LVA LTU SVN HRV CYP MLT".split(" "),
  ligue: "EGY SAU ARE QAT KWT OMN BHR JOR IRQ SYR LBN YEM MAR DZA TUN LBY SDN MRT SOM DJI COM PSE".split(" "),
};
const RIVALS = [["IND", "PAK"], ["ISR", "IRN"], ["KOR", "PRK"], ["USA", "RUS"], ["CHN", "TWN"], ["ARM", "AZE"], ["GRC", "TUR"], ["USA", "IRN"], ["USA", "PRK"], ["SAU", "IRN"], ["MAR", "DZA"], ["RUS", "UKR"], ["CHN", "IND"], ["ETH", "ERI"], ["SRB", "XKX"], ["VEN", "GUY"], ["CHN", "JPN"], ["USA", "CUB"], ["ISR", "LBN"], ["ISR", "SYR"], ["SDN", "SSD"], ["COL", "VEN"], ["JPN", "PRK"]];
const FRIENDS = [["USA", "ISR"], ["USA", "JPN"], ["USA", "KOR"], ["USA", "AUS"], ["RUS", "CHN"], ["CHN", "PRK"], ["RUS", "IRN"], ["FRA", "DEU"], ["USA", "GBR"], ["AUS", "NZL"], ["RUS", "SYR"], ["PAK", "CHN"], ["IND", "RUS"], ["BRA", "ARG"], ["SAU", "ARE"]];

const Empire = {
  E: null, W: null, view: { s: .03, x: 0, y: 0 }, mapMode: "pol", sel: -1, pending: null,

  // ---------------- Préparation de la géométrie ----------------
  prepare() {
    if (this.W) return;
    const W = this.W = window.WORLD;
    // décodage des arcs
    W.A = W.arcs.map(a => { const o = new Float32Array(a.length); let x = 0, y = 0; for (let i = 0; i < a.length; i += 2) { x += a[i]; y += a[i + 1]; o[i] = x; o[i + 1] = y; } return o; });
    const arcSides = W.A.map(() => []);
    W.provs.forEach((p, i) => {
      const path = new Path2D(); let x0 = 1e9, y0 = 1e9, x1 = -1e9, y1 = -1e9;
      for (const poly of p.g) for (const ring of poly) {
        let first = true;
        for (const ai of ring) {
          const k = ai < 0 ? ~ai : ai, A = W.A[k], n = A.length / 2;
          if (!arcSides[k].includes(i)) arcSides[k].push(i);
          for (let j = 0; j < n; j++) {
            const jj = ai < 0 ? n - 1 - j : j, x = A[jj * 2], y = A[jj * 2 + 1];
            if (first) { path.moveTo(x, y); first = false; } else if (j > 0) path.lineTo(x, y);
            if (x < x0) x0 = x; if (x > x1) x1 = x; if (y < y0) y0 = y; if (y > y1) y1 = y;
          }
        }
        path.closePath();
      }
      p.path = path; p.bb = [x0, y0, x1, y1];
    });
    W.arcSides = arcSides;
    // toutes les frontières de provinces
    W.provBorder = new Path2D();
    W.A.forEach((A, k) => { if (arcSides[k].length < 2) return; W.provBorder.moveTo(A[0], A[1]); for (let j = 2; j < A.length; j += 2) W.provBorder.lineTo(A[j], A[j + 1]); });
    W.coastPath = new Path2D();
    W.A.forEach((A, k) => { if (arcSides[k].length !== 1) return; W.coastPath.moveTo(A[0], A[1]); for (let j = 2; j < A.length; j += 2) W.coastPath.lineTo(A[j], A[j + 1]); });
    // villes par province
    W.cityOf = {}; W.cities.forEach((c, i) => { (W.cityOf[c[6]] = W.cityOf[c[6]] || []).push(i); });
    W.provs.forEach((p, i) => { const cs = W.cityOf[i]; p.city = cs ? W.cities[cs[0]][0] : ""; });
    // relief
    W.relief = [0, 1, 2, 3].map(i => { const im = new Image(); im.src = `img/relief_${i}.jpg`; im.onload = () => this.draw(); return im; });
    W.reliefSmall = new Image(); W.reliefSmall.src = "img/relief_small.jpg"; W.reliefSmall.onload = () => this.draw();
    // couleurs des nations
    W.nations.forEach((n, i) => {
      let h = 0; for (const ch of n.code) h = (h * 31 + ch.charCodeAt(0)) % 360;
      h = (h * 7 + i * 47) % 360;
      n.col = `hsl(${h},${55 + (i % 3) * 10}%,${42 + (i % 4) * 6}%)`;
      n.flag = n.a2 ? String.fromCodePoint(...[...n.a2.toUpperCase()].map(c => 0x1F1E6 + c.charCodeAt(0) - 65)) : "🏳️";
    });
  },

  // ---------------- Nouvelle partie ----------------
  enter() {
    Screens.show("empire");
    $("#empire").innerHTML = `<div class="center" style="margin:auto;color:#9aa0b4"><div style="font-size:50px">🌍</div>Chargement de la carte…</div>`;
    setTimeout(() => {
      this.prepare();
      const s = Store.get("empire");
      if (s && !s.over) { this.load(s); this.buildUI(); this.centerOn(this.E.me); }
      else this.pickCountry();
    }, 50);
  },
  newState(me, opts) {
    const W = this.W, NP = W.provs.length, NN = W.nations.length;
    const E = {
      v: 1, me, turn: 0, startY: 2026, diff: opts.diff, regime: opts.regime, leader: opts.leader, natName: W.nations[me].n,
      own: new Int16Array(NP), troops: new Float32Array(NP), unrest: new Float32Array(NP), bld: W.provs.map(() => ({})), gdpM: new Float32Array(NP).fill(1), popM: new Float32Array(NP).fill(1),
      nat: [], rel: [], wars: [], allies: [], pacts: [], trade: [], vassal: new Int16Array(NN).fill(-1),
      news: [], approval: 62, nextElection: 48, over: false, won: false, stats: { conquered: 0, battles: 0, lost: 0, nukes: 0 }, warStart: {},
    };
    W.provs.forEach((p, i) => { E.own[i] = p.o; });
    for (let n = 0; n < NN; n++) {
      const N = W.nations[n];
      const army = Math.max(2, N.pop * 1.1 + N.gdp * .045);
      const tech = {}; for (const t in TECHS) tech[t] = 0;
      const lvl = N.gdp > 2000 ? 2 : N.gdp > 400 ? 1 : 0; tech.arm = tech.def = tech.eco = lvl; tech.nav = lvl; tech.air = lvl;
      if (["USA", "RUS", "CHN", "FRA", "GBR", "IND", "PAK", "ISR", "PRK"].includes(N.code)) tech.nuc = 1;
      E.nat.push({ alive: true, gold: N.gdp * .03, tax: 30, stab: 65 + rnd(-10, 15), tech, rp: 0, research: null, aggr: rf(.2, 1), mobil: 0, cap: N.capP, exhaustion: 0 });
      // troupes réparties
      const ps = W.provs.map((p, i) => i).filter(i => E.own[i] === n);
      const tot = ps.reduce((s, i) => s + W.provs[i].pop + .05, 0);
      for (const i of ps) E.troops[i] = army * (W.provs[i].pop + .05) / tot * .8 + (i === N.capP ? army * .2 : 0);
      if (N.gdp > 50) { E.bld[N.capP].base = 1; E.bld[N.capP].fort = 1; if (N.gdp > 300) E.bld[N.capP].aero = 1; }
      if (tech.nuc) E.bld[N.capP].silo = 1;
    }
    // relations initiales
    E.rel = []; for (let a = 0; a < NN; a++) { E.rel.push(new Int8Array(NN)); }
    const idx = {}; W.nations.forEach((n, i) => idx[n.code] = i);
    for (let a = 0; a < NN; a++) for (let b = a + 1; b < NN; b++) {
      let r = rnd(-10, 25);
      const A = W.nations[a], B = W.nations[b];
      if (A.sub && A.sub === B.sub) r += 15; else if (A.reg && A.reg === B.reg) r += 8;
      for (const k in BLOCS) if (BLOCS[k].includes(A.code) && BLOCS[k].includes(B.code)) r += 35;
      if ((BLOCS.otan.includes(A.code) && BLOCS.otsc.includes(B.code)) || (BLOCS.otsc.includes(A.code) && BLOCS.otan.includes(B.code))) r -= 35;
      E.rel[a][b] = E.rel[b][a] = clamp(r, -100, 100);
    }
    const setRel = (a, b, v) => { if (idx[a] !== undefined && idx[b] !== undefined) E.rel[idx[a]][idx[b]] = E.rel[idx[b]][idx[a]] = v; };
    RIVALS.forEach(([a, b]) => setRel(a, b, -rnd(55, 85)));
    FRIENDS.forEach(([a, b]) => setRel(a, b, rnd(60, 85)));
    // alliances de départ
    const bloc = list => { const ids = list.map(c => idx[c]).filter(x => x !== undefined); for (let i = 0; i < ids.length; i++) for (let j = i + 1; j < ids.length; j++) E.allies.push([ids[i], ids[j]]); };
    if (opts.blocs) { bloc(BLOCS.otan); bloc(BLOCS.otsc); }
    if (opts.realWars && idx.RUS !== undefined && idx.UKR !== undefined) {
      E.wars.push([idx.RUS, idx.UKR]); E.warStart[idx.RUS + "-" + idx.UKR] = 0;
      // armées déjà mobilisées
      for (let i = 0; i < NP; i++) { if (E.own[i] === idx.UKR) E.troops[i] *= 9; if (E.own[i] === idx.RUS) E.troops[i] *= 3; }
    }
    E.nat[me].gold = Math.max(E.nat[me].gold, 2);
    return E;
  },
  pickCountry() {
    const W = this.W;
    this.E = null;
    const strong = W.nations.map((n, i) => ({ n, i })).sort((a, b) => b.n.gdp - a.n.gdp);
    $("#empire").innerHTML = `
      <div class="topbar"><button class="back" id="emBack">←</button><div class="ttl"><b>EMPIRE MONDIAL</b><small>Choisissez votre pays — touchez la carte ou la liste</small></div></div>
      <div class="map-wrap" id="map" style="flex:1.2"><canvas id="mapC"></canvas>
        <div class="map-ctrl"><button id="zIn">＋</button><button id="zOut">－</button></div></div>
      <div style="flex:1;display:flex;flex-direction:column;min-height:0;background:var(--bg2)">
        <div style="padding:8px 12px"><input id="natSearch" placeholder="🔎 Rechercher un pays…" style="width:100%;padding:11px;border-radius:12px;border:1px solid var(--line);background:var(--card);color:#fff"></div>
        <div class="scroll"><div class="list pick-list" id="natList" style="padding-top:0"></div></div></div>`;
    $("#emBack").onclick = () => showMenu();
    const renderList = q => {
      q = (q || "").toLowerCase().normalize("NFD").replace(/[̀-ͯ]/g, "");
      $("#natList").innerHTML = strong.filter(o => !q || o.n.n.toLowerCase().normalize("NFD").replace(/[̀-ͯ]/g, "").includes(q)).slice(0, 80).map(o => `
        <button class="item" data-i="${o.i}"><div class="ic">${o.n.flag}</div><div class="tx"><b>${esc(o.n.n)}</b><small>${fmtNum(o.n.pop * 1e6)} hab. · PIB ${fmtNum(o.n.gdp)} Md$ · ${W.provs.filter(p => p.o === o.i).length} provinces</small></div><div class="rt">${o.n.gdp > 1500 ? "⭐⭐⭐" : o.n.gdp > 300 ? "⭐⭐" : "⭐"}</div></button>`).join("");
      $$("#natList .item").forEach(b => b.onclick = () => this.confirmCountry(+b.dataset.i));
    };
    $("#natSearch").oninput = e => renderList(e.target.value);
    renderList("");
    this.initMap(true);
    this.view.s = 0; this.fitWorld();
  },
  confirmCountry(n) {
    const W = this.W, N = W.nations[n];
    this.sel = W.nations[n].capP; this.centerOn(n, true); this.draw();
    const m = Modal.open({ icon: N.flag, title: N.n, sub: `Capitale : ${N.capN || W.provs[N.capP].n}`, html: `
      <div class="grid2" style="margin-bottom:10px"><div class="tile"><small>Population</small><b>${fmtNum(N.pop * 1e6)}</b></div><div class="tile"><small>PIB</small><b>${fmtNum(N.gdp)} Md$</b></div>
      <div class="tile"><small>Provinces</small><b>${W.provs.filter(p => p.o === n).length}</b></div><div class="tile"><small>Difficulté</small><b>${N.gdp > 1500 ? "Facile" : N.gdp > 200 ? "Moyenne" : N.gdp > 20 ? "Difficile" : "Extrême"}</b></div></div>
      <div class="field"><label>Votre nom de président(e)</label><input id="ldr" value="${esc(pick(LD.namesF.concat(LD.namesM)) + " " + pick(LD.lastNames))}" maxlength="30"></div>
      <div class="field"><label>Régime</label><div class="seg" id="reg"><button data-v="demo" class="on">🗳️ Démocratie</button><button data-v="auto">🎖️ Autoritaire</button></div></div>
      <div class="field"><label>Agressivité de l'IA</label><div class="seg" id="dif"><button data-v="0.6">Paisible</button><button data-v="1" class="on">Normale</button><button data-v="1.6">Belliqueuse</button></div></div>
      <div class="field"><label>Monde de départ</label><div class="seg" id="real"><button data-v="1" class="on">Réaliste (OTAN, conflits)</button><button data-v="0">Bac à sable</button></div></div>
      <p class="muted" style="font-size:12.5px">🗳️ Démocratie : élections tous les 4 ans, stabilité élevée. 🎖️ Autoritaire : pas d'élections, armée moins chère, risque de coup d'État et de révoltes.</p>
      <button class="btn gold block" id="go">Prendre le pouvoir 🏛️</button>` });
    const seg = id => { let v = $(`#${id} .on`, m.el).dataset.v; $$(`#${id} button`, m.el).forEach(b => b.onclick = () => { $$(`#${id} button`, m.el).forEach(x => x.classList.toggle("on", x === b)); v = b.dataset.v; }); return () => v; };
    const reg = seg("reg"), dif = seg("dif"), real = seg("real");
    $("#go", m.el).onclick = () => {
      const opts = { regime: reg(), diff: +dif(), leader: $("#ldr", m.el).value.trim() || "Le Président", blocs: real() === "1", realWars: real() === "1" };
      m.close(); Sound.good();
      this.E = this.newState(n, opts); this.sel = -1;
      this.news(`${N.flag} ${opts.leader} prête serment et devient ${opts.regime === "demo" ? "président(e)" : "chef suprême"} de ${N.n}.`, "me");
      if (opts.realWars) this.news("⚔️ La guerre fait toujours rage entre la Russie et l'Ukraine.", "war");
      this.buildUI(); this.centerOn(n); this.save();
      Modal.alert("Bienvenue au pouvoir", `<b>${esc(opts.leader)}</b>, vous dirigez désormais <b>${esc(N.n)}</b>.<br><br>👆 Touchez une province pour la gérer : recruter, construire des bases, déplacer vos troupes ou attaquer.<br>🤝 Négociez, formez des alliances ou déclarez la guerre.<br>▶️ Chaque tour = 1 mois.<br><br>Objectif : dominer le monde… ou simplement survivre au prochain mandat.`, "🏛️", "C'est parti !");
    };
  },
  load(s) {
    const W = this.W;
    const E = this.E = s;
    E.own = Int16Array.from(s.own); E.troops = Float32Array.from(s.troops); E.unrest = Float32Array.from(s.unrest);
    E.gdpM = Float32Array.from(s.gdpM); E.popM = Float32Array.from(s.popM); E.vassal = Int16Array.from(s.vassal);
    E.rel = s.rel.map(r => Int8Array.from(r));
  },
  save() {
    const E = this.E; if (!E) return;
    const o = Object.assign({}, E, { own: Array.from(E.own), troops: Array.from(E.troops, v => Math.round(v * 10) / 10), unrest: Array.from(E.unrest, v => Math.round(v)), gdpM: Array.from(E.gdpM, v => Math.round(v * 100) / 100), popM: Array.from(E.popM, v => Math.round(v * 100) / 100), vassal: Array.from(E.vassal), rel: E.rel.map(r => Array.from(r)), news: E.news.slice(-80) });
    if (!Store.set("empire", o)) toast("⚠️ Sauvegarde impossible (stockage plein)");
  },

  // ---------------- Interface principale ----------------
  buildUI() {
    $("#empire").innerHTML = `
      <div class="em-top"><div class="em-flag" id="emFlag"></div><div class="ttl" style="flex:1;min-width:0"><b id="emName" style="font-family:Fredoka;font-size:16px;display:block;white-space:nowrap;overflow:hidden;text-overflow:ellipsis"></b><small id="emDate" class="muted" style="font-size:12px"></small></div>
        <button class="ghost" id="emMenu" style="padding:8px 12px">☰</button></div>
      <div class="em-res" id="emRes"></div>
      <div class="map-wrap" id="map"><canvas id="mapC"></canvas>
        <div class="map-legend" id="mapLegend"></div>
        <div class="map-ctrl"><button id="zIn">＋</button><button id="zOut">－</button><button id="zHome">🏠</button><button id="zMode">🗺️</button></div>
        <div id="pendBar" style="display:none;position:absolute;top:8px;left:8px;right:8px;z-index:3;background:rgba(255,201,38,.95);color:#1a1300;border-radius:14px;padding:10px 12px;font-weight:700;font-size:14px;display:flex;align-items:center;gap:8px"></div></div>
      <div class="em-dock">
        <button id="dNat"><span class="i">🏛️</span>Nation</button><button id="dDip"><span class="i">🤝</span>Diplomatie</button>
        <button class="next" id="dNext"><span class="i">▶</span>Tour suivant</button>
        <button id="dMil"><span class="i">⚔️</span>Armée</button><button id="dMore"><span class="i">📰</span>Actus & +</button></div>`;
    $("#pendBar").style.display = "none";
    this.initMap(false);
    $("#dNext").onclick = () => this.nextTurn();
    $("#dNat").onclick = () => this.nationPanel();
    $("#dDip").onclick = () => this.diplomacyPanel();
    $("#dMil").onclick = () => this.militaryPanel();
    $("#dMore").onclick = () => this.morePanel();
    $("#emMenu").onclick = () => this.gameMenu();
    $("#zHome").onclick = () => this.centerOn(this.E.me);
    $("#zMode").onclick = () => {
      const modes = ["pol", "dip", "troops", "relief"], names = { pol: "Politique", dip: "Diplomatique", troops: "Militaire", relief: "Relief" };
      this.mapMode = modes[(modes.indexOf(this.mapMode) + 1) % modes.length]; toast("Carte : " + names[this.mapMode]); this.draw();
    };
    this.refreshTop();
  },
  refreshTop() {
    const E = this.E, W = this.W, me = E.me, N = W.nations[me], S = E.nat[me];
    if (!$("#emFlag")) return;
    $("#emFlag").textContent = N.flag; $("#emFlag").style.background = "var(--gold)";
    $("#emName").textContent = `${N.n} · ${E.leader}`;
    $("#emDate").textContent = `${MONTHS_S[E.turn % 12]} ${E.startY + Math.floor(E.turn / 12)} · ${E.regime === "demo" ? `Élections dans ${E.nextElection} mois` : "Régime autoritaire"}`;
    const ec = this.economy(me);
    const T = this.totalTroops(me);
    const wars = E.wars.filter(w => w.includes(me)).length;
    $("#emRes").innerHTML = `
      <div class="res">💰 <span class="${S.gold < 0 ? "neg" : ""}">${S.gold.toFixed(1)} Md$</span> <span class="${ec.net < 0 ? "neg" : "pos"}">${ec.net >= 0 ? "+" : ""}${ec.net.toFixed(1)}</span></div>
      <div class="res">🪖 ${fmtNum(T * 1000)}</div>
      <div class="res">👥 ${fmtNum(this.popOf(me) * 1e6)}</div>
      <div class="res">📈 ${fmtNum(this.gdpOf(me))} Md$</div>
      <div class="res">${E.approval > 50 ? "😀" : E.approval > 30 ? "😐" : "😡"} ${Math.round(E.approval)} %</div>
      <div class="res">⚖️ Stab. ${Math.round(S.stab)}</div>
      <div class="res">🗺️ ${this.provsOf(me).length} prov.</div>
      ${wars ? `<div class="res" style="color:var(--red)">⚔️ ${wars} guerre${wars > 1 ? "s" : ""}</div>` : ""}`;
    const sp = this.sel >= 0 ? W.provs[this.sel] : null;
    $("#mapLegend").innerHTML = sp ? `${W.nations[E.own[this.sel]].flag} <b style="color:#fff">${esc(sp.n)}</b> · ${TERRAIN[sp.t].ic} · 🪖 ${fmtNum(E.troops[this.sel] * 1000)}` : `Touchez une province`;
  },

  // ---------------- Carte (canvas) ----------------
  initMap(picking) {
    const wrap = $("#map"), cv = $("#mapC");
    this.picking = picking;
    const resize = () => { const r = wrap.getBoundingClientRect(); this.dpr = Math.min(2, window.devicePixelRatio || 1); cv.width = r.width * this.dpr; cv.height = r.height * this.dpr; cv.style.width = r.width + "px"; cv.style.height = r.height + "px"; this.cw = r.width; this.ch = r.height; this.clampView(); this.draw(); };
    this.ctx = cv.getContext("2d");
    window.onresize = resize; resize();
    // gestes : glisser, pincer, toucher
    const pts = new Map(); let start = null, moved = false, lastDist = 0, lastMid = null;
    const pos = e => { const r = cv.getBoundingClientRect(); return [e.clientX - r.left, e.clientY - r.top]; };
    cv.onpointerdown = e => { cv.setPointerCapture(e.pointerId); pts.set(e.pointerId, pos(e)); moved = false; start = pos(e); if (pts.size === 2) { const [a, b] = [...pts.values()]; lastDist = Math.hypot(a[0] - b[0], a[1] - b[1]); lastMid = [(a[0] + b[0]) / 2, (a[1] + b[1]) / 2]; } };
    cv.onpointermove = e => {
      if (!pts.has(e.pointerId)) return;
      const prev = pts.get(e.pointerId), p = pos(e); pts.set(e.pointerId, p);
      if (pts.size === 1) { const dx = p[0] - prev[0], dy = p[1] - prev[1]; if (Math.hypot(p[0] - start[0], p[1] - start[1]) > 6) moved = true; this.view.x += dx; this.view.y += dy; }
      else if (pts.size === 2) {
        moved = true; const [a, b] = [...pts.values()]; const d = Math.hypot(a[0] - b[0], a[1] - b[1]); const mid = [(a[0] + b[0]) / 2, (a[1] + b[1]) / 2];
        this.zoomAt(mid[0], mid[1], d / lastDist); this.view.x += mid[0] - lastMid[0]; this.view.y += mid[1] - lastMid[1]; lastDist = d; lastMid = mid;
      }
      this.clampView(); this.requestDraw();
    };
    const up = e => { if (pts.size === 1 && !moved) this.tap(...pos(e)); pts.delete(e.pointerId); if (pts.size === 1) { start = [...pts.values()][0]; } };
    cv.onpointerup = up; cv.onpointercancel = e => pts.delete(e.pointerId);
    cv.onwheel = e => { e.preventDefault(); const p = pos(e); this.zoomAt(p[0], p[1], e.deltaY < 0 ? 1.2 : 1 / 1.2); this.clampView(); this.requestDraw(); };
    $("#zIn").onclick = () => { this.zoomAt(this.cw / 2, this.ch / 2, 1.6); this.clampView(); this.draw(); };
    $("#zOut").onclick = () => { this.zoomAt(this.cw / 2, this.ch / 2, 1 / 1.6); this.clampView(); this.draw(); };
  },
  zoomAt(x, y, f) {
    const v = this.view, ns = clamp(v.s * f, this.minS(), 1.2);
    f = ns / v.s; v.x = x - (x - v.x) * f; v.y = y - (y - v.y) * f; v.s = ns;
  },
  minS() { return Math.max(this.cw / this.W.W, this.ch / this.W.H); },
  fitWorld() { const v = this.view; v.s = this.minS(); v.x = (this.cw - this.W.W * v.s) / 2; v.y = (this.ch - this.W.H * v.s) / 2; this.clampView(); this.draw(); },
  clampView() {
    const v = this.view, W = this.W; if (!this.cw) return;
    v.s = Math.max(v.s, this.minS());
    const mw = W.W * v.s, mh = W.H * v.s;
    v.x = mw < this.cw ? (this.cw - mw) / 2 : clamp(v.x, this.cw - mw, 0);
    v.y = mh < this.ch ? (this.ch - mh) / 2 : clamp(v.y, this.ch - mh, 0);
  },
  centerOn(n, quick) {
    const W = this.W, E = this.E;
    const ps = E ? this.provsOf(n) : W.provs.map((p, i) => i).filter(i => W.provs[i].o === n);
    const cap = E ? E.nat[n].cap : W.nations[n].capP;
    const comp = this.component(ps, cap);
    let x0 = 1e9, y0 = 1e9, x1 = -1e9, y1 = -1e9;
    for (const i of comp) { const b = W.provs[i].bb; x0 = Math.min(x0, b[0]); y0 = Math.min(y0, b[1]); x1 = Math.max(x1, b[2]); y1 = Math.max(y1, b[3]); }
    if (x0 > x1) return;
    const pad = 1.6, s = clamp(Math.min(this.cw / ((x1 - x0) * pad + 400), this.ch / ((y1 - y0) * pad + 400)), this.minS(), .3);
    this.view.s = s; this.view.x = this.cw / 2 - (x0 + x1) / 2 * s; this.view.y = this.ch / 2 - (y0 + y1) / 2 * s;
    this.clampView(); this.draw();
  },
  component(ps, start) {
    const set = new Set(ps); if (!set.has(start)) start = ps[0];
    if (start === undefined) return [];
    const out = [start], seen = new Set([start]);
    for (let k = 0; k < out.length; k++) for (const j of this.W.provs[out[k]].nb) if (set.has(j) && !seen.has(j)) { seen.add(j); out.push(j); }
    return out;
  },
  requestDraw() { if (this.raf) return; this.raf = requestAnimationFrame(() => { this.raf = 0; this.draw(); }); },
  toWorld(x, y) { const v = this.view; return [(x - v.x) / v.s, (y - v.y) / v.s]; },
  provAt(x, y) {
    const W = this.W, [wx, wy] = this.toWorld(x, y);
    const c = this.hitCtx || (this.hitCtx = document.createElement("canvas").getContext("2d"));
    let best = -1, bestA = 1e18;
    for (let i = 0; i < W.provs.length; i++) {
      const b = W.provs[i].bb; if (wx < b[0] || wx > b[2] || wy < b[1] || wy > b[3]) continue;
      if (c.isPointInPath(W.provs[i].path, wx, wy)) { const a = (b[2] - b[0]) * (b[3] - b[1]); if (a < bestA) { bestA = a; best = i; } }
    }
    if (best < 0) { // tolérance pour les petites îles
      let bd = 1e18; const tol = 25 / this.view.s;
      for (let i = 0; i < W.provs.length; i++) { const p = W.provs[i]; const d = Math.hypot(p.cx - wx, p.cy - wy); if (d < tol && d < bd) { bd = d; best = i; } }
    }
    return best;
  },
  ownerColor(n) {
    const E = this.E, W = this.W;
    if (!E) return W.nations[n].col;
    if (n === E.me) return "#ffc926";
    if (this.mapMode === "dip") {
      if (this.atWar(n, E.me)) return "#ff2a3c";
      if (this.isAlly(n, E.me)) return "#2f8cff";
      if (E.vassal[n] === E.me) return "#b884ff";
      const r = E.rel[E.me][n]; return r > 40 ? "#4cc38a" : r > 0 ? "#9bb57a" : r > -40 ? "#c9a05a" : "#d0643f";
    }
    const ov = E.vassal[n]; if (ov >= 0) return ov === E.me ? "#d9b13a" : W.nations[ov].col;
    return W.nations[n].col;
  },
  draw() {
    const ctx = this.ctx, W = this.W, E = this.E; if (!ctx || !W) return;
    const v = this.view, d = this.dpr, s = v.s;
    ctx.setTransform(d, 0, 0, d, 0, 0);
    ctx.fillStyle = "#0b2542"; ctx.fillRect(0, 0, this.cw, this.ch);
    ctx.setTransform(d * s, 0, 0, d * s, d * v.x, d * v.y);
    // relief
    const tw = W.W / 2, th = W.H / 2;
    const useSmall = s < .045 || !W.relief.every(im => im.complete && im.naturalWidth);
    if (useSmall && W.reliefSmall.complete && W.reliefSmall.naturalWidth) ctx.drawImage(W.reliefSmall, 0, 0, W.W, W.H);
    else if (!useSmall) W.relief.forEach((im, i) => ctx.drawImage(im, (i % 2) * tw, Math.floor(i / 2) * th, tw + 1 / s, th + 1 / s));
    // visible
    const vx0 = -v.x / s, vy0 = -v.y / s, vx1 = vx0 + this.cw / s, vy1 = vy0 + this.ch / s;
    const vis = []; for (let i = 0; i < W.provs.length; i++) { const b = W.provs[i].bb; if (b[2] < vx0 || b[0] > vx1 || b[3] < vy0 || b[1] > vy1) continue; vis.push(i); }
    const own = E ? E.own : null;
    if (this.mapMode !== "relief") {
      ctx.globalAlpha = .5;
      // regrouper par couleur
      const byCol = new Map();
      for (const i of vis) {
        let col;
        if (this.mapMode === "troops" && E) { const t = E.troops[i]; col = t > 200 ? "#ff1f3d" : t > 60 ? "#ff6a3d" : t > 20 ? "#ffa53d" : t > 5 ? "#ffd93d" : t > 1 ? "#d8e07a" : "#9fb59a"; }
        else col = this.ownerColor(own ? own[i] : W.provs[i].o);
        (byCol.get(col) || byCol.set(col, []).get(col)).push(i);
      }
      for (const [col, list] of byCol) { ctx.fillStyle = col; for (const i of list) ctx.fill(W.provs[i].path); }
      ctx.globalAlpha = 1;
    }
    // frontières des provinces
    if (s > .045) { ctx.strokeStyle = "rgba(20,20,20,.35)"; ctx.lineWidth = .6 / s; ctx.stroke(W.provBorder); }
    // frontières nationales
    if (!this.natBorder || this.natBorderDirty) this.buildNatBorder();
    ctx.strokeStyle = "rgba(15,10,10,.85)"; ctx.lineWidth = 1.6 / s; ctx.stroke(this.natBorder);
    ctx.strokeStyle = "rgba(10,30,60,.6)"; ctx.lineWidth = .9 / s; ctx.stroke(W.coastPath);
    // mon pays : liseré doré
    if (E && this.myBorder) { ctx.strokeStyle = "#fff1b8"; ctx.lineWidth = 2.2 / s; ctx.stroke(this.myBorder); }
    // fronts de guerre
    if (E && this.warBorder) { ctx.strokeStyle = "#ff2a3c"; ctx.lineWidth = 2.6 / s; ctx.setLineDash([6 / s, 4 / s]); ctx.stroke(this.warBorder); ctx.setLineDash([]); }
    // cibles possibles
    if (this.pending && this.pending.targets) {
      ctx.lineWidth = 2.5 / s; ctx.strokeStyle = this.pending.kind === "move" ? "#35d07f" : "#ff4d5e";
      ctx.fillStyle = this.pending.kind === "move" ? "rgba(53,208,127,.25)" : "rgba(255,77,94,.28)";
      for (const i of this.pending.targets) { ctx.fill(W.provs[i].path); ctx.stroke(W.provs[i].path); }
    }
    if (this.sel >= 0) { ctx.lineWidth = 3 / s; ctx.strokeStyle = "#fff"; ctx.stroke(W.provs[this.sel].path); ctx.lineWidth = 1.2 / s; ctx.strokeStyle = "#000"; ctx.stroke(W.provs[this.sel].path); }
    // ---- étiquettes (en pixels écran)
    ctx.setTransform(d, 0, 0, d, 0, 0);
    ctx.textAlign = "center"; ctx.textBaseline = "middle";
    const sx = x => x * s + v.x, sy = y => y * s + v.y;
    // villes
    const cityLimit = s < .02 ? -1 : s < .035 ? 1 : s < .06 ? 3 : s < .1 ? 4 : s < .18 ? 6 : s < .35 ? 7 : 9;
    if (cityLimit >= 0) {
      ctx.font = "600 11px Nunito, sans-serif";
      for (const c of W.cities) {
        if (c[5] > cityLimit && !(c[4] && s > .02)) continue;
        const x = sx(c[1]), y = sy(c[2]); if (x < -40 || y < -20 || x > this.cw + 40 || y > this.ch + 20) continue;
        ctx.fillStyle = c[4] ? "#fff" : "#f2f2f2"; ctx.strokeStyle = "#000"; ctx.lineWidth = 1;
        ctx.beginPath(); ctx.arc(x, y, c[4] ? 3.4 : 2.2, 0, 7); ctx.fill(); ctx.stroke();
        if (c[4]) { ctx.font = "700 12px Nunito, sans-serif"; } else ctx.font = "600 10.5px Nunito, sans-serif";
        ctx.lineWidth = 3; ctx.strokeStyle = "rgba(0,0,0,.7)"; ctx.strokeText((c[4] ? "★ " : "") + c[0], x, y - 9); ctx.fillStyle = "#fff"; ctx.fillText((c[4] ? "★ " : "") + c[0], x, y - 9);
      }
    }
    // noms des provinces (zoom fort)
    if (s > .12) {
      ctx.font = "italic 600 10px Nunito, sans-serif"; ctx.fillStyle = "rgba(255,255,230,.85)";
      for (const i of vis) { const p = W.provs[i]; const w = (p.bb[2] - p.bb[0]) * s; if (w < 60) continue; ctx.strokeStyle = "rgba(0,0,0,.6)"; ctx.lineWidth = 2.5; ctx.strokeText(p.n, sx(p.cx), sy(p.cy) + 12); ctx.fillText(p.n, sx(p.cx), sy(p.cy) + 12); }
    }
    // troupes & bâtiments
    if (E && s > .05) {
      ctx.font = "700 10.5px Fredoka, sans-serif";
      for (const i of vis) {
        const p = W.provs[i], w = (p.bb[2] - p.bb[0]) * s, o = E.own[i], rel = o === E.me || this.atWar(o, E.me);
        if (i !== this.sel && (w < (rel ? 34 : 70) || (!rel && s < .1))) continue;
        const x = sx(p.cx), y = sy(p.cy), t = E.troops[i];
        if (t >= 1) {
          const txt = fmtNum(t * 1000); const tw2 = ctx.measureText(txt).width + 18;
          const mine = E.own[i] === E.me, war = this.atWar(E.own[i], E.me);
          ctx.fillStyle = mine ? "rgba(40,30,0,.85)" : war ? "rgba(90,0,10,.85)" : "rgba(10,15,25,.75)";
          ctx.beginPath(); if (ctx.roundRect) ctx.roundRect(x - tw2 / 2, y - 8, tw2, 16, 8); else ctx.rect(x - tw2 / 2, y - 8, tw2, 16); ctx.fill();
          ctx.fillStyle = mine ? "#ffd84d" : war ? "#ff9aa5" : "#e6e9f2"; ctx.fillText("🪖" + txt, x, y + 1);
        }
        if (s > .09) { const b = E.bld[i]; const ics = Object.keys(b).filter(k => b[k]).map(k => BUILDINGS[k].ic).join(""); if (ics) { ctx.font = "11px sans-serif"; ctx.fillText(ics, x, y + 17); ctx.font = "700 10.5px Fredoka, sans-serif"; } }
      }
    }
    // noms des nations
    if (s < .14) {
      if (!this.natLabels || this.natBorderDirty === "labels") this.buildLabels();
      for (const L of this.natLabels) {
        const fs = clamp(L.size * s, 0, 30); if (fs < 9) continue;
        const x = sx(L.x), y = sy(L.y); if (x < -100 || x > this.cw + 100 || y < -30 || y > this.ch + 30) continue;
        ctx.font = `700 ${fs}px Fredoka, sans-serif`; ctx.lineWidth = Math.max(2, fs / 5); ctx.strokeStyle = "rgba(0,0,0,.55)";
        const t = L.name.toUpperCase(); ctx.strokeText(t, x, y); ctx.fillStyle = L.me ? "#fff3c4" : "rgba(255,255,255,.93)"; ctx.fillText(t, x, y);
      }
    }
  },
  buildNatBorder() {
    const W = this.W, E = this.E;
    const own = E ? E.own : W.provs.map(p => p.o);
    const nb = new Path2D(), my = new Path2D(), war = new Path2D();
    W.A.forEach((A, k) => {
      const sd = W.arcSides[k];
      if (sd.length < 2) { if (E && own[sd[0]] === E.me) { my.moveTo(A[0], A[1]); for (let j = 2; j < A.length; j += 2) my.lineTo(A[j], A[j + 1]); } return; }
      const a = own[sd[0]], b = own[sd[1]];
      if (a === b) return;
      const tgt = [nb]; if (E && (a === E.me || b === E.me)) { tgt.push(my); if (this.atWar(a, b)) tgt.push(war); }
      else if (E && this.atWar(a, b)) tgt.push(war);
      for (const P of tgt) { P.moveTo(A[0], A[1]); for (let j = 2; j < A.length; j += 2) P.lineTo(A[j], A[j + 1]); }
    });
    this.natBorder = nb; this.myBorder = E ? my : null; this.warBorder = E ? war : null; this.natBorderDirty = "labels";
  },
  buildLabels() {
    const W = this.W, E = this.E, own = E ? E.own : W.provs.map(p => p.o);
    const by = {}; own.forEach((o, i) => (by[o] = by[o] || []).push(i));
    const out = [];
    for (const n in by) {
      const cap = E ? E.nat[n].cap : W.nations[n].capP;
      // composante principale = la plus grande
      const left = new Set(by[n]); let best = null, bestA = 0;
      while (left.size) {
        const st = left.values().next().value; const comp = this.component([...left], st);
        comp.forEach(i => left.delete(i));
        const a = comp.reduce((s, i) => s + W.provs[i].a + 1, 0); if (a > bestA) { bestA = a; best = comp; }
      }
      let x = 0, y = 0, tot = 0, x0 = 1e9, x1 = -1e9;
      for (const i of best) { const p = W.provs[i], w = p.a + 1; x += p.cx * w; y += p.cy * w; tot += w; x0 = Math.min(x0, p.bb[0]); x1 = Math.max(x1, p.bb[2]); }
      const name = W.nations[n].n;
      out.push({ x: x / tot, y: y / tot, size: Math.min((x1 - x0) / (name.length * .75), Math.sqrt(bestA) * 9), name, me: E && +n === E.me });
    }
    this.natLabels = out; this.natBorderDirty = false;
  },
  dirty() { this.natBorderDirty = true; this.natLabels = null; },

  // ---------------- Calculs ----------------
  provsOf(n) { const o = []; const own = this.E.own; for (let i = 0; i < own.length; i++) if (own[i] === n) o.push(i); return o; },
  totalTroops(n) { let t = 0; const own = this.E.own; for (let i = 0; i < own.length; i++) if (own[i] === n) t += this.E.troops[i]; return t; },
  gdpOf(n) { let t = 0; const E = this.E; for (let i = 0; i < E.own.length; i++) if (E.own[i] === n) t += this.W.provs[i].gdp * E.gdpM[i] * (1 + .15 * (E.bld[i].usine || 0)); return t; },
  popOf(n) { let t = 0; const E = this.E; for (let i = 0; i < E.own.length; i++) if (E.own[i] === n) t += this.W.provs[i].pop * E.popM[i]; return t; },
  atWar(a, b) { if (a === b || !this.E) return false; return this.E.wars.some(w => (w[0] === a && w[1] === b) || (w[0] === b && w[1] === a)); },
  isAlly(a, b) { return a !== b && this.E.allies.some(w => (w[0] === a && w[1] === b) || (w[0] === b && w[1] === a)); },
  hasPact(a, b) { return this.E.pacts.some(w => (w[0] === a && w[1] === b) || (w[0] === b && w[1] === a)); },
  hasTrade(a, b) { return this.E.trade.some(w => (w[0] === a && w[1] === b) || (w[0] === b && w[1] === a)); },
  enemiesOf(a) { return this.E.wars.filter(w => w.includes(a)).map(w => w[0] === a ? w[1] : w[0]); },
  alliesOf(a) { return this.E.allies.filter(w => w.includes(a)).map(w => w[0] === a ? w[1] : w[0]).filter(n => this.E.nat[n].alive); },
  power(n) { const t = this.E.nat[n].tech; return this.totalTroops(n) * (1 + .1 * t.arm) + this.gdpOf(n) * .02; },
  economy(n) {
    const S = this.E.nat[n], gdp = this.gdpOf(n);
    const trade = this.E.trade.filter(w => w.includes(n)).length;
    const income = gdp * S.tax / 100 / 12 * (.55 + S.stab / 220) * (1 + .08 * S.tech.eco) * (1 + .015 * trade) * (this.E.nat[n].sanct ? .9 : 1);
    const troops = this.totalTroops(n);
    const army = troops * .012 * (n === this.E.me && this.E.regime === "auto" ? .8 : 1);
    const admin = income * .72; // services publics
    let bld = 0; for (const i of this.provsOf(n)) for (const k in this.E.bld[i]) bld += (this.E.bld[i][k] || 0) * BUILDINGS[k].cost * .01;
    const vassalTax = this.E.vassal[n] >= 0 ? income * .15 : 0;
    let tribute = 0; for (let v = 0; v < this.E.vassal.length; v++) if (this.E.vassal[v] === n && this.E.nat[v].alive) tribute += this.gdpOf(v) * .3 / 100 / 12;
    const net = income - army - admin - bld - vassalTax + tribute;
    return { income, army, admin, bld, net, gdp, tribute, vassalTax };
  },
  defense(i) {
    const E = this.E, W = this.W, p = W.provs[i], n = E.own[i], S = E.nat[n], b = E.bld[i];
    const militia = p.pop * E.popM[i] * 1.2 + .5;
    return (E.troops[i] + militia) * TERRAIN[p.t].d * (1 + .3 * (b.fort || 0)) * (1 + .1 * (b.base || 0)) * (1 + .1 * S.tech.def) * (i === S.cap ? 1.25 : 1) * (S.stab < 30 ? .85 : 1);
  },
  reachNaval(n, from) {
    // provinces côtières ennemies atteignables depuis un port
    const E = this.E, W = this.W, R = 1400 + E.nat[n].tech.nav * 1200;
    const p = W.provs[from]; if (!p.c || !E.bld[from].port) return [];
    const out = [];
    for (let j = 0; j < W.provs.length; j++) { if (!W.provs[j].c || E.own[j] === n) continue; const q = W.provs[j]; if (Math.hypot(q.cx - p.cx, q.cy - p.cy) < R) out.push(j); }
    return out;
  },
  airCover(n, target) {
    const E = this.E, W = this.W, q = W.provs[target], R = 1500 + E.nat[n].tech.air * 500;
    for (let i = 0; i < E.own.length; i++) if (E.own[i] === n && E.bld[i].aero && Math.hypot(W.provs[i].cx - q.cx, W.provs[i].cy - q.cy) < R) return true;
    return false;
  },

  // ---------------- Interaction ----------------
  tap(x, y) {
    const i = this.provAt(x, y);
    if (!this.E) { if (i >= 0) this.confirmCountry(this.W.provs[i].o); return; }
    if (this.pending) {
      if (i >= 0 && this.pending.targets.includes(i)) this.pendingDone(i);
      else toast(this.pending.kind === "move" ? "Choisissez une province en vert" : "Choisissez une province en rouge");
      return;
    }
    if (i < 0) { this.sel = -1; this.draw(); this.refreshTop(); return; }
    this.sel = i; this.draw(); this.refreshTop(); Sound.click();
    this.provincePanel(i);
  },
  setPending(p) {
    this.pending = p;
    const bar = $("#pendBar");
    if (!p) { bar.style.display = "none"; this.draw(); return; }
    bar.style.display = "flex"; bar.innerHTML = `<span style="flex:1">${p.label}</span><button class="btn" style="padding:6px 12px;background:#1a1300;color:#fff" id="pendX">Annuler</button>`;
    $("#pendX").onclick = () => this.setPending(null);
    this.draw();
  },
  async pendingDone(t) {
    const p = this.pending; this.setPending(null);
    if (p.kind === "move") await this.moveTroops(p.from, t);
    else await this.attack(p.from, t, p.naval);
  },
  provincePanel(i) {
    const E = this.E, W = this.W, p = W.provs[i], o = E.own[i], N = W.nations[o], me = E.me, mine = o === me;
    const b = E.bld[i];
    const cities = (W.cityOf[i] || []).slice(0, 4).map(c => W.cities[c][0] + (W.cities[c][4] ? " ★" : "")).join(", ");
    const orig = W.nations[p.o];
    const html = `
      <div class="grid2">
        <div class="tile"><small>Propriétaire</small><b style="font-size:15px">${N.flag} ${esc(N.n)}</b></div>
        <div class="tile"><small>Terrain</small><b style="font-size:15px">${TERRAIN[p.t].ic} ${TERRAIN[p.t].n}</b><small>${p.el} m · défense ×${TERRAIN[p.t].d}</small></div>
        <div class="tile"><small>Population</small><b>${fmtNum(p.pop * E.popM[i] * 1e6)}</b></div>
        <div class="tile"><small>PIB</small><b>${(p.gdp * E.gdpM[i] * (1 + .15 * (b.usine || 0))).toFixed(1)} Md$</b></div>
        <div class="tile"><small>Garnison</small><b>🪖 ${fmtNum(E.troops[i] * 1000)}</b></div>
        <div class="tile"><small>Défense effective</small><b>🛡️ ${fmtNum(this.defense(i) * 1000)}</b></div>
      </div>
      <p class="muted" style="font-size:13px;margin:10px 0">${p.c ? "🌊 Côtière · " : ""}${cities ? "🏙️ " + esc(cities) : "Aucune grande ville"}${p.o !== o ? `<br>📜 Province historique de ${orig.flag} ${esc(orig.n)}` : ""}${E.unrest[i] > 20 ? `<br>🔥 Agitation : ${Math.round(E.unrest[i])} %` : ""}</p>
      <div class="sec" style="padding:4px 0">Infrastructures</div>
      <div style="margin:4px 0 12px">${Object.keys(BUILDINGS).filter(k => b[k]).map(k => `<span class="pill y" style="margin:2px">${BUILDINGS[k].ic} ${BUILDINGS[k].n}${BUILDINGS[k].max > 1 ? " " + b[k] : ""}</span>`).join("") || `<span class="muted">Aucune</span>`}</div>
      <div class="choices" id="pAct"></div>`;
    const m = Modal.open({ icon: N.flag, title: p.n, sub: `${N.n}${i === E.nat[o].cap ? " · ★ Capitale" : ""}`, html });
    const acts = [];
    const A = (ic, label, sub, fn, cls = "") => acts.push({ ic, label, sub, fn, cls });
    if (mine) {
      A("🪖", "Recruter des troupes", `Coût ${(this.recruitCost(i) * 10).toFixed(2)} Md$ pour 10 000 soldats`, () => this.recruit(i), "gold");
      if (E.troops[i] >= 1) A("➡️", "Déplacer des troupes", "Vers une province voisine que vous contrôlez", () => this.startMove(i));
      const tg = this.attackTargets(i);
      if (tg.land.length) A("⚔️", "Attaquer une province voisine", `${tg.land.length} cible(s) ennemie(s)`, () => this.startAttack(i, false), "red");
      if (tg.sea.length) A("🚢", "Débarquement naval", `${tg.sea.length} côte(s) ennemie(s) à portée`, () => this.startAttack(i, true), "red");
      A("🏗️", "Construire", "Bases, forts, usines, ports, aéroports…", () => this.buildMenu(i));
      if (E.unrest[i] > 20) A("👮", "Réprimer l'agitation", "Coût 0,5 Md$, -popularité", () => { if (this.spend(.5)) { E.unrest[i] = Math.max(0, E.unrest[i] - 40); E.approval = clamp(E.approval - 2); toast("Ordre rétabli"); } });
      if (i !== E.nat[me].cap) A("★", "Déplacer la capitale ici", "Coût 15 Md$, -10 stabilité", async () => { if (await Modal.confirm("Nouvelle capitale", `Faire de ${p.n} la capitale ?`) && this.spend(15)) { E.nat[me].cap = i; E.nat[me].stab -= 10; this.news(`★ ${W.nations[me].n} déplace sa capitale à ${p.city || p.n}.`, "me"); } });
      if (p.o !== me && E.nat[p.o].alive === false || (p.o !== me && !E.nat[p.o].alive)) A("🏳️", "Libérer cette province", "Recrée la nation historique, qui sera votre vassale", () => this.liberate(i));
    } else {
      const atWar = this.atWar(o, me);
      if (atWar) {
        const from = W.provs[i].nb.filter(j => E.own[j] === me && E.troops[j] >= 1).sort((a, c) => E.troops[c] - E.troops[a]);
        if (from.length) A("⚔️", "Attaquer cette province", `Depuis ${W.provs[from[0]].n} (${fmtNum(E.troops[from[0]] * 1000)} soldats)`, () => this.attack(from[0], i, false), "red");
        const navFrom = this.provsOf(me).filter(j => E.bld[j].port && E.troops[j] >= 1 && this.reachNaval(me, j).includes(i));
        if (navFrom.length && p.c) A("🚢", "Débarquement naval", `Depuis ${W.provs[navFrom[0]].n}`, () => this.attack(navFrom[0], i, true), "red");
        if (this.airCover(me, i)) A("✈️", "Frappe aérienne", "Coût 1 Md$ — détruit une partie de la garnison", () => this.airStrike(i));
      }
      const silo = this.provsOf(me).some(j => E.bld[j].silo);
      if (silo && E.nat[me].tech.nuc >= 1) A("☢️", "Frappe nucléaire", "Dévastateur. Le monde ne vous le pardonnera pas.", () => this.nuke(i), "red");
      A("🤝", `Diplomatie avec ${N.n}`, `Relations : ${E.rel[me][o]}`, () => this.nationActions(o));
      A("🕵️", "Opération secrète ici", "Sabotage, révolte, espionnage…", () => this.spyMenu(o, i));
    }
    $("#pAct", m.el).innerHTML = acts.map((a, k) => `<button class="btn ${a.cls}" data-k="${k}"><div>${a.ic} ${a.label}${a.sub ? `<small>${a.sub}</small>` : ""}</div></button>`).join("");
    $$("#pAct .btn", m.el).forEach(b => b.onclick = async () => { m.close(); await acts[+b.dataset.k].fn(); this.after(); });
  },
  after() { this.dirty(); this.draw(); this.refreshTop(); this.save(); },
  spend(v) { const S = this.E.nat[this.E.me]; if (S.gold < v) { Modal.alert("Trésor insuffisant", `Il faut ${v.toFixed(1)} Md$. Vous avez ${S.gold.toFixed(1)} Md$.`, "💸"); return false; } S.gold -= v; return true; },
  recruitCost(i) { const E = this.E; return .12 * (1 - .3 * (E.bld[i].base || 0)) * (E.regime === "auto" ? .85 : 1); },
  async recruit(i) {
    const E = this.E, S = E.nat[E.me], c = this.recruitCost(i);
    const max = Math.floor(Math.max(0, S.gold) / (c * 10));
    if (max < 1) return Modal.alert("Trésor insuffisant", "Impossible de recruter : caisses vides.", "💸");
    const pop = this.popOf(E.me), cap = Math.max(5, pop * 25);
    const v = await Modal.slider("Recrutement", `${this.W.provs[i].n} — par tranches de 10 000 soldats. (${(c * 10).toFixed(2)} Md$ / tranche)`, 1, Math.min(max, 200), Math.min(max, 5), x => `${fmtNum(x * 10000)} soldats · ${(x * c * 10).toFixed(1)} Md$`, "Recruter", "🪖");
    if (!v) return;
    S.gold -= v * c * 10; E.troops[i] += v * 10;
    const tot = this.totalTroops(E.me);
    if (tot > cap) { E.approval = clamp(E.approval - 4); toast("Mobilisation massive : la population grogne"); }
    E.approval = clamp(E.approval - v * .05);
    Sound.click(); toast(`+${fmtNum(v * 10000)} soldats à ${this.W.provs[i].n}`);
  },
  startMove(i) {
    const E = this.E, W = this.W;
    const t = W.provs[i].nb.filter(j => E.own[j] === E.me || (this.isAlly(E.own[j], E.me) && E.vassal[E.own[j]] === E.me));
    // déplacement maritime entre provinces portuaires
    if (E.bld[i].port) this.provsOf(E.me).forEach(j => { if (j !== i && E.bld[j].port && !t.includes(j)) t.push(j); });
    if (!t.length) return Modal.alert("Aucune destination", "Aucune province voisine sous votre contrôle (les ports permettent de déplacer par la mer entre deux ports).", "➡️");
    this.setPending({ kind: "move", from: i, targets: t, label: `➡️ Déplacer depuis ${W.provs[i].n} : touchez une province verte` });
  },
  attackTargets(i) {
    const E = this.E, W = this.W, me = E.me;
    if (E.troops[i] < 1) return { land: [], sea: [] };
    const land = W.provs[i].nb.filter(j => this.atWar(E.own[j], me));
    const sea = this.reachNaval(me, i).filter(j => this.atWar(E.own[j], me));
    return { land, sea };
  },
  startAttack(i, naval) {
    const tg = this.attackTargets(i);
    this.setPending({ kind: "attack", from: i, naval, targets: naval ? tg.sea : tg.land, label: `${naval ? "🚢 Débarquement" : "⚔️ Attaque"} depuis ${this.W.provs[i].n} : touchez une province rouge` });
  },
  async moveTroops(a, b) {
    const E = this.E, max = Math.floor(E.troops[a]);
    if (max < 1) return;
    const v = await Modal.slider("Déplacer des troupes", `${this.W.provs[a].n} → ${this.W.provs[b].n}`, 1, max, Math.max(1, Math.floor(max * .7)), x => fmtNum(x * 1000) + " soldats", "Déplacer", "➡️");
    if (!v) return;
    E.troops[a] -= v; E.troops[b] += v; this.after(); toast("Troupes déplacées");
  },
  // ---------------- Combat ----------------
  battle(att, from, to, n, naval, silent) {
    const E = this.E, W = this.W, S = E.nat[att], def = E.own[to];
    const A = n * (1 + .1 * S.tech.arm) * (this.airCover(att, to) ? 1.2 : 1) * (naval ? .65 + .05 * S.tech.nav : 1) * rf(.85, 1.15) * (S.stab < 25 ? .85 : 1);
    const D = this.defense(to) * rf(.85, 1.15);
    E.stats.battles += att === E.me || def === E.me ? 1 : 0;
    const win = A > D;
    let la, ld;
    if (win) { la = n * Math.min(.9, D / A * .55); ld = E.troops[to]; }
    else { la = n * Math.min(.95, .35 + D / A * .25); ld = E.troops[to] * Math.min(.8, A / D * .45); }
    E.troops[from] -= n; E.troops[to] = Math.max(0, E.troops[to] - ld);
    const survivors = Math.max(0, n - la);
    if (win) {
      // la garnison survivante se replie
      const back = W.provs[to].nb.filter(j => E.own[j] === def); if (back.length) E.troops[pick(back)] += ld * .25;
      this.conquer(att, to, survivors);
    } else { E.troops[from] += survivors; }
    E.nat[att].exhaustion += la * .01; E.nat[def].exhaustion += ld * .01;
    return { win, A, D, la, ld, survivors };
  },
  conquer(att, to, troops) {
    const E = this.E, W = this.W, def = E.own[to];
    E.own[to] = att; E.troops[to] = troops; (E.fresh = E.fresh || {})[to] = E.turn; E.unrest[to] = W.provs[to].o === att ? 0 : 55; E.gdpM[to] *= .85;
    if (E.bld[to].silo && att !== W.provs[to].o) E.bld[to].silo = 0;
    if (att === E.me) E.stats.conquered++;
    if (def === E.me) E.stats.lost++;
    // capitale tombée
    if (E.nat[def].cap === to) {
      const rest = this.provsOf(def);
      if (rest.length) {
        E.nat[def].cap = rest.sort((a, b) => W.provs[b].pop - W.provs[a].pop)[0]; E.nat[def].stab -= 25;
        this.news(`🏚️ ${W.nations[def].flag} La capitale de ${W.nations[def].n} est tombée aux mains de ${W.nations[att].n} ! Le gouvernement fuit à ${W.provs[E.nat[def].cap].n}.`, "war");
      }
    }
    if (!this.provsOf(def).length) this.eliminate(def, att);
    this.dirty();
  },
  eliminate(n, by) {
    const E = this.E, W = this.W;
    E.nat[n].alive = false;
    E.wars = E.wars.filter(w => !w.includes(n)); E.allies = E.allies.filter(w => !w.includes(n)); E.pacts = E.pacts.filter(w => !w.includes(n)); E.trade = E.trade.filter(w => !w.includes(n));
    for (let v = 0; v < E.vassal.length; v++) if (E.vassal[v] === n) E.vassal[v] = -1;
    E.vassal[n] = -1;
    this.news(`💀 ${W.nations[n].flag} ${W.nations[n].n} a été entièrement annexé${by >= 0 ? " par " + W.nations[by].n : ""} !`, by === E.me ? "me" : "war");
    if (n === E.me) this.gameOver("conquis", `${W.nations[n].n} a été rayé de la carte.`);
  },
  async attack(from, to, naval) {
    const E = this.E, W = this.W, me = E.me, max = Math.floor(E.troops[from]);
    if (max < 1) return Modal.alert("Pas de troupes", "Cette province n'a pas assez de soldats.", "🪖");
    const est = this.defense(to);
    const v = await Modal.slider(naval ? "Débarquement naval" : "Attaque", `${W.provs[from].n} → ${W.provs[to].n}<br>Défense estimée : <b>~${fmtNum(est * 1000)}</b> (${TERRAIN[W.provs[to].t].n}${E.bld[to].fort ? ", fortifiée" : ""})${naval ? "<br>⚠️ Pénalité de débarquement" : ""}${this.airCover(me, to) ? "<br>✈️ Couverture aérienne : +20 %" : ""}`, 1, max, max, x => `${fmtNum(x * 1000)} soldats`, "À l'assaut !", "⚔️");
    if (!v) return;
    const r = this.battle(me, from, to, v, naval);
    Sound.boom(); vibrate(60);
    await Modal.alert(r.win ? "Victoire !" : "Défaite…", `<div class="battle"><div class="boom">${r.win ? "🏴" : "💥"}</div>
      <div class="vs"><div>Vous<b>${fmtNum(v * 1000)}</b></div><div>⚔️</div><div>${W.nations[r.win ? me : E.own[to]].flag}<b>${fmtNum(est * 1000)}</b></div></div>
      <p>${r.win ? `${W.provs[to].n} est conquise !` : `L'assaut sur ${W.provs[to].n} a été repoussé.`}<br>Pertes : <b style="color:var(--red)">${fmtNum(r.la * 1000)}</b> · Pertes ennemies : <b style="color:var(--green)">${fmtNum(r.ld * 1000)}</b></p></div>`, r.win ? "🎖️" : "💀");
    if (r.win) { this.news(`🎖️ Nos troupes ont pris ${W.provs[to].n} !`, "me"); E.approval = clamp(E.approval + 1); this.checkVictory(); }
    else { this.news(`💥 Échec de notre offensive sur ${W.provs[to].n}.`, "war"); E.approval = clamp(E.approval - 1.5); }
    this.sel = to;
  },
  async airStrike(i) {
    const E = this.E, W = this.W; if (!this.spend(1)) return;
    const k = .1 + .04 * E.nat[E.me].tech.air + rf(0, .1);
    const loss = E.troops[i] * k; E.troops[i] -= loss;
    if (E.bld[i].fort && chance(.3)) E.bld[i].fort--;
    Sound.boom(); this.news(`✈️ Frappe aérienne sur ${W.provs[i].n} : ${fmtNum(loss * 1000)} soldats ennemis neutralisés.`, "me");
    toast(`✈️ ${fmtNum(loss * 1000)} soldats ennemis neutralisés`);
  },
  async nuke(i) {
    const E = this.E, W = this.W, me = E.me, o = E.own[i];
    const prot = W.provs[i].nb.concat([i]).some(j => E.bld[j].radar && E.own[j] === o);
    if (!await Modal.confirm("☢️ FRAPPE NUCLÉAIRE", `Lancer une ogive sur ${W.provs[i].n} (${W.nations[o].n}) ? Des millions de morts. Toutes les nations vous condamneront.`, "LANCER", "Annuler", "☢️")) return;
    if (!this.spend(5)) return;
    E.stats.nukes++;
    Sound.boom(); vibrate(400);
    if (prot && chance(.6)) { this.news(`📡 Le bouclier antimissile de ${W.nations[o].n} a intercepté notre missile !`, "war"); }
    else {
      E.troops[i] *= .05; E.popM[i] *= .5; E.gdpM[i] *= .3; E.bld[i] = {};
      this.news(`☢️ ${W.nations[me].n} a frappé ${W.provs[i].n} d'une arme nucléaire. Le monde est sous le choc.`, "war");
      await Modal.alert("☢️", `${W.provs[i].n} a été rasée. Un champignon atomique s'élève au-dessus de la région.`, "☢️");
    }
    for (let n = 0; n < W.nations.length; n++) if (n !== me) E.rel[me][n] = E.rel[n][me] = clamp(E.rel[me][n] - 45, -100, 100);
    if (!this.atWar(me, o)) this.declareWar(me, o, true);
    // riposte
    if (E.nat[o].tech.nuc && this.provsOf(o).some(j => E.bld[j].silo) && chance(.7)) {
      const tg = pick(this.provsOf(me).sort((a, b) => W.provs[b].pop - W.provs[a].pop).slice(0, 5));
      E.popM[tg] *= .5; E.troops[tg] *= .05; E.gdpM[tg] *= .3; E.bld[tg] = {};
      this.news(`☢️ RIPOSTE : ${W.nations[o].n} a frappé ${W.provs[tg].n} !`, "war");
      await Modal.alert("☢️ Riposte nucléaire", `${W.nations[o].n} a riposté sur ${W.provs[tg].n}.`, "☢️");
      E.approval = clamp(E.approval - 25); E.nat[me].stab -= 20;
    }
    E.approval = clamp(E.approval - 10);
  },
  buildMenu(i) {
    const E = this.E, b = E.bld[i], S = E.nat[E.me];
    const opts = Object.entries(BUILDINGS).map(([k, B]) => {
      const lvl = b[k] || 0, cost = B.cost * (1 + lvl * .5);
      const lock = B.tech && S.tech[B.tech[0]] < B.tech[1];
      const port = k === "port" && !this.W.provs[i].c;
      return { label: `${B.ic} ${B.n}${B.max > 1 ? ` (${lvl}/${B.max})` : lvl ? " ✅" : ""}`, sub: lock ? `🔒 ${TECHS[B.tech[0]].n} niveau ${B.tech[1]} requis` : port ? "Province non côtière" : lvl >= B.max ? "Niveau maximal" : `${cost.toFixed(1)} Md$ · ${B.d}`, value: k, disabled: lock || port || lvl >= B.max };
    });
    Modal.choice("Construire", `${this.W.provs[i].n} · Trésor : ${S.gold.toFixed(1)} Md$`, opts, "🏗️", true).then(k => {
      if (!k) return; const B = BUILDINGS[k], cost = B.cost * (1 + (b[k] || 0) * .5);
      if (!this.spend(cost)) return;
      b[k] = (b[k] || 0) + 1; Sound.good(); toast(`${B.ic} ${B.n} construit à ${this.W.provs[i].n}`);
      if (k === "silo") { this.news(`☢️ ${this.W.nations[E.me].n} déploie un silo nucléaire. Inquiétude internationale.`, "me"); for (let n = 0; n < this.W.nations.length; n++) if (n !== E.me) E.rel[E.me][n] = E.rel[n][E.me] = clamp(E.rel[E.me][n] - 5, -100, 100); }
      this.after();
    });
  },
  async liberate(i) {
    const E = this.E, W = this.W, n = W.provs[i].o;
    const ps = this.provsOf(E.me).filter(j => W.provs[j].o === n);
    if (!await Modal.confirm("Libérer", `Rendre ${ps.length} province(s) à ${W.nations[n].n}, qui deviendra votre vassal ?`)) return;
    ps.forEach(j => { E.own[j] = n; E.unrest[j] = 0; });
    E.nat[n].alive = true; E.nat[n].cap = ps.includes(W.nations[n].capP) ? W.nations[n].capP : ps[0]; E.nat[n].gold = 1; E.nat[n].stab = 60;
    E.vassal[n] = E.me; E.allies.push([E.me, n]); E.rel[E.me][n] = E.rel[n][E.me] = 80;
    this.news(`🏳️ ${W.nations[n].flag} ${W.nations[n].n} renaît sous la protection de ${W.nations[E.me].n}.`, "me");
  },

  // ---------------- Diplomatie ----------------
  declareWar(a, b, silent) {
    const E = this.E, W = this.W;
    if (this.atWar(a, b) || a === b) return;
    E.wars.push([a, b]); E.warStart[a + "-" + b] = E.turn;
    E.allies = E.allies.filter(w => !(w.includes(a) && w.includes(b))); E.pacts = E.pacts.filter(w => !(w.includes(a) && w.includes(b)));
    E.trade = E.trade.filter(w => !(w.includes(a) && w.includes(b)));
    E.rel[a][b] = E.rel[b][a] = clamp(Math.min(E.rel[a][b], 0) - 40, -100, 100);
    this.news(`⚔️ ${W.nations[a].flag} ${W.nations[a].n} déclare la guerre à ${W.nations[b].flag} ${W.nations[b].n} !`, a === E.me || b === E.me ? "me war" : "war");
    // les alliés du défenseur entrent en guerre
    for (const al of this.alliesOf(b)) if (al !== a && !this.atWar(al, a) && !this.isAlly(al, a) && (al === E.me ? false : chance(.75))) { E.wars.push([al, a]); E.warStart[al + "-" + a] = E.turn; this.news(`🛡️ ${W.nations[al].flag} ${W.nations[al].n} honore son alliance et entre en guerre contre ${W.nations[a].n}.`, "war"); }
    // le joueur est appelé à l'aide
    if (this.alliesOf(b).includes(E.me) && a !== E.me && !this.isAlly(E.me, a)) E.callToArms = (E.callToArms || []).concat([[b, a]]);
    // les vassaux suivent
    for (let v = 0; v < E.vassal.length; v++) { if (E.vassal[v] === a && v !== b && !this.atWar(v, b)) E.wars.push([v, b]); if (E.vassal[v] === b && v !== a && !this.atWar(v, a)) E.wars.push([v, a]); }
    // monde
    for (let n = 0; n < W.nations.length; n++) if (n !== a && n !== b) E.rel[n][a] = clamp(E.rel[n][a] - (this.isAlly(n, b) ? 25 : 4), -100, 100);
    this.dirty();
  },
  makePeace(a, b) {
    const E = this.E, W = this.W;
    E.wars = E.wars.filter(w => !(w.includes(a) && w.includes(b)));
    E.rel[a][b] = E.rel[b][a] = clamp(E.rel[a][b] + 25, -100, 100);
    E.pacts.push([a, b]); E.truce = E.truce || {}; E.truce[Math.min(a, b) + "-" + Math.max(a, b)] = E.turn + 24;
    E.nat[a].exhaustion *= .5; E.nat[b].exhaustion *= .5;
    this.news(`🕊️ Paix signée entre ${W.nations[a].flag} ${W.nations[a].n} et ${W.nations[b].flag} ${W.nations[b].n}.`, a === E.me || b === E.me ? "me dip" : "dip");
    this.dirty();
  },
  aiAccepts(ai, what, extra = 0) {
    const E = this.E, me = E.me, r = E.rel[ai][me];
    const pr = this.power(me) / Math.max(1, this.power(ai));
    switch (what) {
      case "trade": return r + extra > 0;
      case "pact": return r + extra > 15 || pr > 3;
      case "alliance": return r + extra > 55 && !this.enemiesOf(me).some(x => this.isAlly(x, ai));
      case "vassal": return pr > 6 && r + extra > -10 || (pr > 3 && r > 50);
      case "annex": return pr > 12 && r > 70;
      case "cede": return pr > 4 && r > -30 && chance(.6);
      case "peace": {
        const myLoss = this.warScore(ai, me);
        return myLoss > 2 || E.nat[ai].exhaustion > 30 || chance(.15) || pr > 4;
      }
      case "capitulate": { const own = this.provsOf(ai).length, orig = this.W.provs.filter(p => p.o === ai).length; return own < orig * .45 && (pr > 1.5 || E.nat[ai].exhaustion > 25); }
    }
    return false;
  },
  warScore(loser, winner) {
    // nombre de provinces historiques de "loser" tenues par "winner"
    const E = this.E, W = this.W; let s = 0;
    for (let i = 0; i < E.own.length; i++) if (E.own[i] === winner && W.provs[i].o === loser) s++;
    return s;
  },
  diplomacyPanel() {
    const E = this.E, W = this.W, me = E.me;
    const list = W.nations.map((n, i) => i).filter(i => i !== me && E.nat[i].alive)
      .sort((a, b) => (this.atWar(b, me) - this.atWar(a, me)) || (this.isAlly(b, me) - this.isAlly(a, me)) || this.power(b) - this.power(a));
    const row = i => { const n = W.nations[i], r = E.rel[me][i]; const tags = []; if (this.atWar(i, me)) tags.push(`<span class="pill r">GUERRE</span>`); if (this.isAlly(i, me)) tags.push(`<span class="pill b">Allié</span>`); if (E.vassal[i] === me) tags.push(`<span class="pill y">Vassal</span>`); if (this.hasTrade(i, me)) tags.push(`<span class="pill g">Commerce</span>`); if (this.hasPact(i, me)) tags.push(`<span class="pill">Non-agression</span>`);
      return `<button class="item" data-i="${i}"><div class="ic">${n.flag}</div><div class="tx"><b>${esc(n.n)}</b><small>${tags.join(" ")} Puissance ${fmtNum(this.power(i))}</small></div><div class="rt" style="color:${r > 30 ? "var(--green)" : r < -30 ? "var(--red)" : "var(--mute)"}">${r > 0 ? "+" : ""}${r}</div></button>`; };
    const m = Modal.open({ icon: "🤝", title: "Diplomatie", sub: `${list.length} nations`, html: `<input id="dSearch" placeholder="🔎 Rechercher…" style="width:100%;padding:10px;border-radius:12px;border:1px solid var(--line);background:var(--card);color:#fff;margin-bottom:8px"><div class="list" style="padding:0" id="dList"></div>` });
    const render = q => { q = (q || "").toLowerCase(); $("#dList", m.el).innerHTML = list.filter(i => !q || W.nations[i].n.toLowerCase().includes(q)).slice(0, 60).map(row).join(""); $$("#dList .item", m.el).forEach(b => b.onclick = () => { m.close(); this.nationActions(+b.dataset.i); }); };
    $("#dSearch", m.el).oninput = e => render(e.target.value); render("");
  },
  nationActions(n) {
    const E = this.E, W = this.W, me = E.me, N = W.nations[n], S = E.nat[n], r = E.rel[me][n];
    const war = this.atWar(me, n), ally = this.isAlly(me, n), trade = this.hasTrade(me, n), pact = this.hasPact(me, n);
    const truce = E.truce && E.truce[Math.min(me, n) + "-" + Math.max(me, n)] > E.turn;
    const opts = [];
    const O = (label, sub, value, cls) => opts.push({ label, sub, value, cls });
    if (war) {
      O("🕊️ Proposer la paix", "Statu quo : chacun garde ce qu'il occupe", "peace");
      O("🏳️ Exiger la capitulation", "Annexion totale ou vassalisation", "capitulate", "gold");
      O("📍 Voir le front", "", "front");
    } else {
      O("💐 Améliorer les relations", "0,5 Md$ · +10 relations", "improve");
      O("💶 Aide financière", "Envoyer de l'argent", "aid");
      if (!trade) O("📦 Accord commercial", "+1,5 % de revenus pour les deux", "trade"); else O("📦 Rompre l'accord commercial", "", "untrade");
      if (!pact && !ally) O("📜 Pacte de non-agression", "", "pact");
      if (!ally) O("🛡️ Proposer une alliance", "Défense mutuelle", "alliance"); else O("💔 Rompre l'alliance", "", "unally");
      if (E.vassal[n] !== me) O("👑 Exiger la vassalisation", "Il vous verse un tribut", "vassal");
      if (E.vassal[n] === me) O("🏛️ Annexer le vassal", "Intégration pacifique (relations > 60)", "annexv", "gold");
      else O("🤲 Proposer une union / annexion pacifique", "Nations faibles et très amies", "annex");
      O("🗺️ Exiger une province frontalière", "Ultimatum", "cede");
      O("🚫 Sanctions économiques", "-10 % de revenus pour lui, -relations", "sanct");
      O("😤 Insulter publiquement", "", "insult");
      O("🕵️ Opérations secrètes", "", "spy");
      O(truce ? "⚔️ Déclarer la guerre (trêve en cours !)" : "⚔️ DÉCLARER LA GUERRE", truce ? "Rompre une trêve fera chuter votre stabilité" : `Ses alliés : ${this.alliesOf(n).map(a => W.nations[a].flag).join(" ") || "aucun"}`, "war", "red");
    }
    const info = `${N.flag} <b>${esc(N.n)}</b> — relations <b>${r}</b><br>Armée ${fmtNum(this.totalTroops(n) * 1000)} · PIB ${fmtNum(this.gdpOf(n))} Md$ · ${this.provsOf(n).length} provinces · stabilité ${Math.round(S.stab)}<br>Guerres : ${this.enemiesOf(n).map(x => W.nations[x].flag).join(" ") || "aucune"} · Alliés : ${this.alliesOf(n).map(x => W.nations[x].flag).join(" ") || "aucun"}${E.vassal[n] >= 0 ? `<br>Vassal de ${W.nations[E.vassal[n]].n}` : ""}`;
    Modal.choice(N.n, info, opts, N.flag, true).then(a => a && this.dipDo(n, a));
  },
  async dipDo(n, a) {
    const E = this.E, W = this.W, me = E.me, N = W.nations[n], M = W.nations[me];
    const rel = v => { E.rel[me][n] = E.rel[n][me] = clamp(E.rel[me][n] + v, -100, 100); };
    const no = t => { Sound.bad(); Modal.alert("Refus", `${N.flag} ${N.n} ${t || "refuse votre proposition."}`, "🙅"); };
    const yes = t => { Sound.good(); this.news(t, "me dip"); toast("✅ " + N.n + " accepte !"); };
    switch (a) {
      case "improve": if (!this.spend(.5)) return; rel(rnd(6, 12)); toast(`Relations avec ${N.n} : ${E.rel[me][n]}`); break;
      case "aid": { const S = E.nat[me]; if (S.gold < 1) return Modal.alert("Trésor vide", "", "💸"); const v = await Modal.slider("Aide financière", `Montant à envoyer à ${N.n}`, 1, Math.floor(S.gold), 1, x => x + " Md$", "Envoyer", "💶"); if (!v) return; S.gold -= v; E.nat[n].gold += v; rel(Math.min(40, v * 300 / Math.max(10, this.gdpOf(n)) + 3)); yes(`💶 ${M.n} envoie ${v} Md$ d'aide à ${N.n}.`); break; }
      case "trade": if (this.aiAccepts(n, "trade")) { E.trade.push([me, n]); rel(5); yes(`📦 Accord commercial signé entre ${M.n} et ${N.n}.`); } else no(); break;
      case "untrade": E.trade = E.trade.filter(w => !(w.includes(me) && w.includes(n))); rel(-10); toast("Accord rompu"); break;
      case "pact": if (this.aiAccepts(n, "pact")) { E.pacts.push([me, n]); rel(5); yes(`📜 Pacte de non-agression entre ${M.n} et ${N.n}.`); } else no(); break;
      case "alliance": if (this.aiAccepts(n, "alliance")) { E.allies.push([me, n]); rel(10); yes(`🛡️ ${M.n} et ${N.n} forment une alliance militaire !`); } else no("refuse : relations insuffisantes (> 55 requis) ou conflit d'intérêts."); break;
      case "unally": E.allies = E.allies.filter(w => !(w.includes(me) && w.includes(n))); rel(-30); E.nat[me].stab -= 3; this.news(`💔 ${M.n} rompt son alliance avec ${N.n}.`, "me dip"); break;
      case "vassal": if (this.aiAccepts(n, "vassal")) { E.vassal[n] = me; E.allies.push([me, n]); rel(-10); yes(`👑 ${N.n} devient vassal de ${M.n}.`); } else { rel(-15); no("rejette votre ultimatum avec mépris."); } break;
      case "annexv": if (E.rel[n][me] > 60 || chance(.25)) { this.provsOf(n).forEach(i => { E.own[i] = me; E.unrest[i] = 15; }); this.eliminate(n, me); E.nat[n].alive = false; yes(`🏛️ ${N.n} est intégré pacifiquement à ${M.n}.`); this.checkVictory(); } else no("refuse l'annexion (relations > 60 requises)."); break;
      case "annex": if (this.aiAccepts(n, "annex")) { this.provsOf(n).forEach(i => { E.own[i] = me; E.unrest[i] = 20; }); this.eliminate(n, me); yes(`🤲 ${N.n} rejoint pacifiquement ${M.n} !`); this.checkVictory(); } else no("ne souhaite pas renoncer à sa souveraineté."); break;
      case "cede": {
        const border = this.provsOf(n).filter(i => W.provs[i].nb.some(j => E.own[j] === me) && i !== E.nat[n].cap);
        if (!border.length) return Modal.alert("Impossible", "Aucune province frontalière à réclamer.", "🗺️");
        const opts = border.slice(0, 20).map(i => ({ label: W.provs[i].n, sub: `${fmtNum(W.provs[i].pop * 1e6)} hab.`, value: i + 1 }));
        const pi = await Modal.choice("Ultimatum", "Quelle province exiger ?", opts, "🗺️", true); if (!pi) return;
        if (this.aiAccepts(n, "cede")) { E.own[pi - 1] = me; E.unrest[pi - 1] = 40; rel(-25); yes(`🗺️ ${N.n} cède ${W.provs[pi - 1].n} à ${M.n} sous la pression.`); }
        else { rel(-20); no("rejette l'ultimatum !"); if (chance(.3)) this.declareWar(n, me); }
        break;
      }
      case "sanct": E.nat[n].sanct = 12; rel(-20); this.news(`🚫 ${M.n} impose des sanctions économiques à ${N.n}.`, "me dip"); break;
      case "insult": rel(-25); E.approval = clamp(E.approval + (E.rel[me][n] < -30 ? 2 : -1)); this.news(`😤 ${E.leader} insulte publiquement les dirigeants de ${N.n}.`, "me dip"); break;
      case "spy": return this.spyMenu(n, -1);
      case "war": {
        const truce = E.truce && E.truce[Math.min(me, n) + "-" + Math.max(me, n)] > E.turn;
        const all = this.alliesOf(n);
        if (!await Modal.confirm("Déclarer la guerre ?", `Entrer en guerre contre ${N.n}${all.length ? ` et probablement ses alliés : ${all.map(x => W.nations[x].n).join(", ")}` : ""} ?`, "Guerre !", "Annuler", "⚔️")) return;
        this.declareWar(me, n);
        E.approval = clamp(E.approval - (E.rel[me][n] < -40 ? 0 : 6)); if (truce || this.hasPact(me, n)) { E.nat[me].stab -= 12; E.approval = clamp(E.approval - 8); }
        // appel aux alliés
        for (const al of this.alliesOf(me)) if (!this.atWar(al, n) && al !== n && (E.rel[al][n] < 10 || E.vassal[al] === me) && chance(.6)) { E.wars.push([al, n]); this.news(`🛡️ ${W.nations[al].flag} ${W.nations[al].n} rejoint la guerre à nos côtés !`, "me war"); }
        Sound.boom(); break;
      }
      case "peace": if (this.aiAccepts(n, "peace")) { this.makePeace(me, n); Sound.good(); toast("🕊️ Paix signée"); } else no("refuse de signer la paix."); break;
      case "capitulate": {
        if (!this.aiAccepts(n, "capitulate")) return no("refuse de capituler : il lui reste trop de forces.");
        const c = await Modal.choice("Capitulation", `${N.n} accepte de capituler. Vos conditions ?`, [{ label: "🏛️ Annexion totale", value: "annex" }, { label: "👑 Vassalisation (garde son territoire restant)", value: "vassal" }], "🏳️");
        this.makePeace(me, n);
        if (c === "annex") { this.provsOf(n).forEach(i => { E.own[i] = me; E.unrest[i] = 60; }); this.eliminate(n, me); }
        else { E.vassal[n] = me; E.allies.push([me, n]); yes(`👑 ${N.n} capitule et devient vassal de ${M.n}.`); }
        this.checkVictory(); break;
      }
      case "front": { const fr = this.provsOf(n).find(i => W.provs[i].nb.some(j => E.own[j] === me)); if (fr !== undefined) { this.sel = fr; const p = W.provs[fr]; this.view.s = Math.max(this.view.s, .08); this.view.x = this.cw / 2 - p.cx * this.view.s; this.view.y = this.ch / 2 - p.cy * this.view.s; this.clampView(); } else toast("Pas de front terrestre : utilisez la marine"); break; }
    }
    this.after();
  },
  spyMenu(n, prov) {
    const E = this.E, W = this.W, me = E.me, N = W.nations[n], lv = E.nat[me].tech.esp, def = E.nat[n].tech.esp;
    const p = op => clamp(.45 + .1 * lv - .08 * def - op, .05, .9);
    const ops = [
      { label: "🔎 Espionner (révèle armée & plans)", sub: `0,3 Md$ · ${Math.round(p(-.3) * 100)} %`, value: ["intel", .3, -.3] },
      { label: "💣 Saboter l'économie", sub: `2 Md$ · ${Math.round(p(0) * 100)} %`, value: ["sabot", 2, 0] },
      { label: "🔥 Attiser une révolte" + (prov >= 0 ? ` (${W.provs[prov].n})` : ""), sub: `3 Md$ · ${Math.round(p(.05) * 100)} %`, value: ["revolt", 3, .05] },
      { label: "🧪 Voler une technologie", sub: `2,5 Md$ · ${Math.round(p(.1) * 100)} %`, value: ["tech", 2.5, .1] },
      { label: "📺 Campagne de désinformation", sub: `1 Md$ · ${Math.round(p(-.1) * 100)} % · -stabilité`, value: ["propa", 1, -.1] },
      { label: "🎖️ Fomenter un coup d'État", sub: `15 Md$ · ${Math.round(p(.3) * 100)} % · régime ami`, value: ["coup", 15, .3] },
      { label: "🎯 Assassiner un général", sub: `4 Md$ · ${Math.round(p(.15) * 100)} % · -10 % troupes`, value: ["gen", 4, .15] },
    ];
    Modal.choice("Opérations secrètes", `Cible : ${N.flag} ${N.n} · Renseignement ${lv} vs ${def}`, ops, "🕵️", true).then(async v => {
      if (!v) return; const [k, cost, d] = v; if (!this.spend(cost)) return;
      const ok = chance(p(d));
      if (!ok) {
        const caught = chance(.5);
        if (caught) { E.rel[me][n] = E.rel[n][me] = clamp(E.rel[me][n] - 30, -100, 100); this.news(`🕵️ Scandale : des agents de ${W.nations[me].n} arrêtés en ${N.n} !`, "me dip"); }
        Sound.bad(); return Modal.alert("Échec", caught ? "L'opération a échoué et nos agents ont été capturés. Incident diplomatique !" : "L'opération a échoué, mais nos agents se sont exfiltrés.", "🕵️");
      }
      Sound.good();
      if (k === "intel") Modal.alert("Rapport secret", `${N.n} : ${fmtNum(this.totalTroops(n) * 1000)} soldats, trésor ${E.nat[n].gold.toFixed(1)} Md$, stabilité ${Math.round(E.nat[n].stab)}, technologie militaire ${E.nat[n].tech.arm}.<br>Ennemis : ${this.enemiesOf(n).map(x => W.nations[x].n).join(", ") || "aucun"}<br>Opinion de nous : ${E.rel[n][me]}<br>${E.rel[n][me] < -40 && this.power(n) > this.power(me) ? "⚠️ Ils envisagent une attaque contre nous !" : "Aucune menace imminente détectée."}`, "🔎");
      if (k === "sabot") { E.nat[n].gold -= Math.max(1, this.gdpOf(n) * .01); this.provsOf(n).forEach(i => E.gdpM[i] *= .97); toast("💣 Économie sabotée"); }
      if (k === "revolt") { const t = prov >= 0 ? prov : pick(this.provsOf(n)); E.unrest[t] = 100; E.nat[n].stab -= 8; toast(`🔥 ${W.provs[t].n} s'embrase`); }
      if (k === "tech") { const ks = Object.keys(TECHS).filter(t => E.nat[n].tech[t] > E.nat[me].tech[t]); if (ks.length) { const t = pick(ks); E.nat[me].tech[t]++; toast(`🧪 Technologie volée : ${TECHS[t].n} ${E.nat[me].tech[t]}`); } else toast("Rien d'intéressant à voler"); }
      if (k === "propa") { E.nat[n].stab -= 12; toast("📺 Désinformation diffusée"); }
      if (k === "gen") { this.provsOf(n).forEach(i => E.troops[i] *= .9); toast("🎯 Le général ennemi a été éliminé"); }
      if (k === "coup") { E.rel[n][me] = E.rel[me][n] = 70; E.nat[n].stab = 40; E.wars = E.wars.filter(w => !(w.includes(n) && w.includes(me))); this.news(`🎖️ Coup d'État en ${N.n} ! Le nouveau régime se rapproche de ${W.nations[me].n}.`, "me dip"); }
      this.after();
    });
  },

  // ---------------- Panneaux ----------------
  nationPanel() {
    const E = this.E, W = this.W, me = E.me, S = E.nat[me], ec = this.economy(me), N = W.nations[me];
    const m = Modal.open({ icon: N.flag, title: N.n, sub: `${E.leader} · ${E.regime === "demo" ? "Démocratie" : "Régime autoritaire"}`, html: `
      <div class="grid2">
        <div class="tile"><small>Trésor</small><b>${S.gold.toFixed(1)} Md$</b></div><div class="tile"><small>Solde mensuel</small><b style="color:${ec.net < 0 ? "var(--red)" : "var(--green)"}">${ec.net >= 0 ? "+" : ""}${ec.net.toFixed(2)}</b></div>
        <div class="tile"><small>PIB annuel</small><b>${fmtNum(ec.gdp)} Md$</b></div><div class="tile"><small>Population</small><b>${fmtNum(this.popOf(me) * 1e6)}</b></div>
        <div class="tile"><small>Popularité</small><b>${Math.round(E.approval)} %</b></div><div class="tile"><small>Stabilité</small><b>${Math.round(S.stab)}</b></div></div>
      <div class="sec" style="padding:12px 0 4px">Budget mensuel</div>
      <div class="kv"><span>Recettes fiscales</span><span style="color:var(--green)">+${ec.income.toFixed(2)}</span></div>
      ${ec.tribute ? `<div class="kv"><span>Tributs des vassaux</span><span style="color:var(--green)">+${ec.tribute.toFixed(2)}</span></div>` : ""}
      <div class="kv"><span>Armée</span><span style="color:var(--red)">-${ec.army.toFixed(2)}</span></div>
      <div class="kv"><span>Administration</span><span style="color:var(--red)">-${ec.admin.toFixed(2)}</span></div>
      <div class="kv"><span>Entretien des infrastructures</span><span style="color:var(--red)">-${ec.bld.toFixed(2)}</span></div>
      ${ec.vassalTax ? `<div class="kv"><span>Tribut versé</span><span style="color:var(--red)">-${ec.vassalTax.toFixed(2)}</span></div>` : ""}
      <div class="field" style="margin-top:14px"><label>Taux d'imposition : <span id="taxV">${S.tax}</span> %</label><input type="range" id="tax" min="5" max="70" value="${S.tax}"></div>
      <p class="muted" style="font-size:12.5px">Plus d'impôts = plus de revenus, mais la popularité baisse au-delà de 35 %.</p>
      <div class="choices">
        <button class="btn" id="nDec">📜 Décrets présidentiels</button>
        <button class="btn" id="nRes">🔬 Recherche (${S.research ? TECHS[S.research].n : "aucune"})</button>
        <button class="btn" id="nReg">🏛️ Changer de régime</button>
        <button class="btn" id="nLoan">🏦 Emprunter sur les marchés</button></div>` });
    $("#tax", m.el).oninput = e => { S.tax = +e.target.value; $("#taxV", m.el).textContent = S.tax; this.refreshTop(); };
    $("#nDec", m.el).onclick = () => { m.close(); this.decrees(); };
    $("#nRes", m.el).onclick = () => { m.close(); this.researchPanel(); };
    $("#nLoan", m.el).onclick = async () => { m.close(); const max = Math.max(1, Math.floor(this.gdpOf(me) * .1)); const v = await Modal.slider("Emprunt", "Montant (remboursé sur 24 mois à 8 %)", 1, max, Math.ceil(max / 4), x => x + " Md$", "Emprunter", "🏦"); if (v) { S.gold += v; E.debt = (E.debt || 0) + v * 1.08; toast(`+${v} Md$`); this.after(); } };
    $("#nReg", m.el).onclick = async () => {
      m.close();
      const to = E.regime === "demo" ? "auto" : "demo";
      if (!await Modal.confirm("Changer de régime", to === "auto" ? "Suspendre la constitution et instaurer un régime autoritaire ? (popularité -25, stabilité -20, relations avec les démocraties en baisse)" : "Rétablir la démocratie ? (stabilité -10, élections dans 12 mois)", "Confirmer", "Annuler", "🏛️")) return;
      E.regime = to;
      if (to === "auto") { E.approval = clamp(E.approval - 25); S.stab -= 20; BLOCS.otan.forEach(c => { const i = W.nations.findIndex(n => n.code === c); if (i >= 0 && i !== me) E.rel[me][i] = E.rel[i][me] = clamp(E.rel[me][i] - 20, -100, 100); }); this.news(`🎖️ ${E.leader} suspend la constitution de ${N.n}.`, "me"); }
      else { S.stab -= 10; E.nextElection = 12; E.approval = clamp(E.approval + 10); this.news(`🗳️ ${N.n} renoue avec la démocratie.`, "me"); }
      this.after();
    };
  },
  decrees() {
    const E = this.E, S = E.nat[E.me];
    const D = [
      ["🎉", "Grande fête nationale", 2, "+8 popularité", () => { E.approval = clamp(E.approval + 8); }],
      ["📺", "Campagne de propagande", 1.5, "+5 popularité, +3 stabilité", () => { E.approval = clamp(E.approval + 5 + S.tech.prop); S.stab += 3; }],
      ["🏗️", "Plan d'investissement massif", 20, "+5 % de PIB dans toutes les provinces", () => { this.provsOf(E.me).forEach(i => E.gdpM[i] *= 1.05); E.approval = clamp(E.approval + 3); }],
      ["🪖", "Conscription générale", 5, "+5 % de la population sous les drapeaux, -15 popularité", () => { const cap = E.nat[E.me].cap; const add = this.popOf(E.me) * 50 * .05; E.troops[cap] += add; E.approval = clamp(E.approval - 15); toast(`+${fmtNum(add * 1000)} soldats à la capitale`); }],
      ["🚨", "État d'urgence", 1, "Réprime toute agitation, -10 popularité", () => { this.provsOf(E.me).forEach(i => E.unrest[i] = 0); E.approval = clamp(E.approval - 10); S.stab += 10; }],
      ["⚖️", "Amnistie générale", .5, "+6 popularité, -4 stabilité", () => { E.approval = clamp(E.approval + 6); S.stab -= 4; }],
      ["🏥", "Réforme de la santé", 8, "+10 popularité, +5 stabilité", () => { E.approval = clamp(E.approval + 10); S.stab += 5; }],
      ["🎓", "Réforme de l'éducation", 8, "+80 points de recherche", () => { S.rp += 80; }],
      ["🚀", "Programme spatial", 40, "+15 relations avec tous, +prestige", () => { for (let n = 0; n < this.W.nations.length; n++) if (n !== E.me) E.rel[E.me][n] = E.rel[n][E.me] = clamp(E.rel[E.me][n] + 6, -100, 100); E.approval = clamp(E.approval + 8); this.news(`🚀 ${this.W.nations[E.me].n} lance un programme spatial ambitieux !`, "me"); }],
      ["🏅", "Organiser les Jeux olympiques", 30, "+12 popularité, +10 relations", () => { E.approval = clamp(E.approval + 12); for (let n = 0; n < this.W.nations.length; n++) if (n !== E.me) E.rel[E.me][n] = E.rel[n][E.me] = clamp(E.rel[E.me][n] + 4, -100, 100); this.news(`🏅 ${this.W.nations[E.me].n} accueillera les Jeux olympiques !`, "me"); }],
      ["💸", "Baisse d'impôts exceptionnelle", 0, "Impôts -5 %, +7 popularité", () => { S.tax = Math.max(5, S.tax - 5); E.approval = clamp(E.approval + 7); }],
      ["🌿", "Transition écologique", 10, "+5 popularité, +5 relations avec l'Europe", () => { E.approval = clamp(E.approval + 5); }],
    ];
    Modal.choice("Décrets présidentiels", `Trésor : ${S.gold.toFixed(1)} Md$ · un décret par mois`, D.map((d, i) => ({ label: `${d[0]} ${d[1]}`, sub: `${d[2] ? d[2] + " Md$ · " : ""}${d[3]}`, value: i + 1, disabled: E.lastDecree === E.turn })), "📜", true).then(i => {
      if (!i) return; const d = D[i - 1]; if (!this.spend(d[2])) return;
      d[4](); E.lastDecree = E.turn; Sound.good(); toast(`${d[0]} Décret signé`); this.after();
    });
  },
  researchPanel() {
    const E = this.E, S = E.nat[E.me], rpm = this.researchRate(E.me);
    const cost = (t) => Math.round(60 * Math.pow(S.tech[t] + 1, 1.6));
    const m = Modal.open({ icon: "🔬", title: "Recherche", sub: `${rpm.toFixed(1)} points / mois · ${Math.round(S.rp)} accumulés`, html: Object.entries(TECHS).map(([k, T]) => `
      <div class="tech"><div style="display:flex;align-items:center;gap:8px"><b style="flex:1">${T.ic} ${T.n} ${S.tech[k]}/${T.max}</b>${S.research === k ? `<span class="pill b">En cours</span>` : S.tech[k] < T.max ? `<button class="btn" style="padding:6px 10px;font-size:13px" data-k="${k}">Rechercher (${cost(k)})</button>` : `<span class="pill g">Max</span>`}</div>
      <div class="lv">${Array.from({ length: T.max }, (_, i) => `<i class="${i < S.tech[k] ? "on" : ""}"></i>`).join("")}</div><small class="muted">${T.d}</small></div>`).join("") });
    $$("[data-k]", m.el).forEach(b => b.onclick = () => { S.research = b.dataset.k; m.close(); toast(`🔬 Recherche : ${TECHS[S.research].n}`); this.save(); });
  },
  researchRate(n) {
    const E = this.E; let u = 0; for (const i of this.provsOf(n)) u += E.bld[i].univ || 0;
    return 1 + Math.sqrt(Math.max(0, this.gdpOf(n))) * .3 * (1 + .15 * u) * (1 + .05 * E.nat[n].tech.med);
  },
  militaryPanel() {
    const E = this.E, W = this.W, me = E.me;
    const ps = this.provsOf(me).sort((a, b) => E.troops[b] - E.troops[a]);
    const enemies = this.enemiesOf(me);
    const front = ps.filter(i => W.provs[i].nb.some(j => this.atWar(E.own[j], me)));
    const m = Modal.open({ icon: "⚔️", title: "Forces armées", sub: `${fmtNum(this.totalTroops(me) * 1000)} soldats · armement ${E.nat[me].tech.arm} · défense ${E.nat[me].tech.def}`, html: `
      ${enemies.length ? `<div class="sec" style="padding:4px 0">En guerre contre</div><div style="margin-bottom:8px">${enemies.map(x => `<span class="pill r" style="margin:2px">${W.nations[x].flag} ${esc(W.nations[x].n)}</span>`).join("")}</div>` : `<p class="muted">Aucune guerre en cours.</p>`}
      <div class="choices" style="margin-bottom:10px">
        ${front.length ? `<button class="btn red" id="mAuto">⚡ Offensive générale sur tous les fronts</button><button class="btn" id="mReinf">🛡️ Renforcer le front (déplace 50 % des réserves)</button>` : ""}
        <button class="btn gold" id="mRecr">🪖 Recruter à la capitale</button></div>
      <div class="sec" style="padding:4px 0">Garnisons (${ps.length} provinces)</div>
      <div class="list" style="padding:0">${ps.slice(0, 40).map(i => `<button class="item" data-i="${i}"><div class="ic">${front.includes(i) ? "🔥" : TERRAIN[W.provs[i].t].ic}</div><div class="tx"><b>${esc(W.provs[i].n)}</b><small>${Object.keys(E.bld[i]).filter(k => E.bld[i][k]).map(k => BUILDINGS[k].ic).join(" ")}${front.includes(i) ? " · front" : ""}</small></div><div class="rt">🪖 ${fmtNum(E.troops[i] * 1000)}</div></button>`).join("")}</div>` });
    $$(".item", m.el).forEach(b => b.onclick = () => { m.close(); const i = +b.dataset.i, p = W.provs[i]; this.sel = i; this.view.s = Math.max(this.view.s, .07); this.view.x = this.cw / 2 - p.cx * this.view.s; this.view.y = this.ch / 2 - p.cy * this.view.s; this.clampView(); this.draw(); this.provincePanel(i); });
    $("#mRecr", m.el).onclick = () => { m.close(); this.recruit(E.nat[me].cap).then(() => this.after()); };
    if (front.length) {
      $("#mReinf", m.el).onclick = () => { m.close(); this.reinforce(me, .5); this.after(); toast("🛡️ Réserves envoyées au front"); };
      $("#mAuto", m.el).onclick = async () => {
        m.close();
        if (!await Modal.confirm("Offensive générale", "Toutes les provinces du front attaqueront la cible la plus faible si le rapport de force est favorable (×1,2).", "Lancer", "Annuler", "⚡")) return;
        let won = 0, lost = 0, la = 0;
        for (const i of front) {
          if (E.own[i] !== me || E.troops[i] < 2) continue;
          const tg = W.provs[i].nb.filter(j => this.atWar(E.own[j], me)).sort((a, b) => this.defense(a) - this.defense(b))[0];
          if (tg === undefined) continue;
          const n = E.troops[i] * .85; if (n * (1 + .1 * E.nat[me].tech.arm) < this.defense(tg) * 1.2) continue;
          const r = this.battle(me, i, tg, n, false); r.win ? won++ : lost++; la += r.la;
        }
        Sound.boom();
        this.news(`⚡ Offensive générale : ${won} province(s) conquise(s), ${lost} assaut(s) repoussé(s).`, "me");
        Modal.alert("Bilan de l'offensive", `✅ ${won} province(s) conquise(s)<br>❌ ${lost} échec(s)<br>☠️ ${fmtNum(la * 1000)} soldats perdus`, "⚡");
        E.approval = clamp(E.approval + won * .5 - lost);
        this.checkVictory(); this.after();
      };
    }
  },
  reinforce(n, frac) {
    const E = this.E, W = this.W;
    const ps = this.provsOf(n);
    const isFront = i => W.provs[i].nb.some(j => this.atWar(E.own[j], n));
    const front = ps.filter(isFront); if (!front.length) return;
    // propagation : chaque province intérieure envoie une partie vers le voisin le plus proche du front (BFS)
    const dist = new Map(); const q = [...front]; front.forEach(i => dist.set(i, 0));
    for (let k = 0; k < q.length; k++) for (const j of W.provs[q[k]].nb) if (E.own[j] === n && !dist.has(j)) { dist.set(j, dist.get(q[k]) + 1); q.push(j); }
    for (const i of ps) {
      if (!dist.has(i) || dist.get(i) === 0) continue;
      const nxt = W.provs[i].nb.filter(j => dist.has(j) && dist.get(j) < dist.get(i));
      if (!nxt.length) continue;
      const keep = i === E.nat[n].cap ? .4 : 0, mv = E.troops[i] * frac * (1 - keep);
      E.troops[i] -= mv; E.troops[pick(nxt)] += mv;
    }
  },
  morePanel() {
    const E = this.E;
    const m = Modal.open({ icon: "📰", title: "Actualités mondiales", html: `
      <div class="row" style="margin-bottom:8px"><button class="btn" id="mRank">🏆 Classement</button><button class="btn" id="mRes">🔬 Recherche</button><button class="btn" id="mStat">📊 Bilan</button></div>
      <div>${E.news.slice().reverse().slice(0, 70).map(n => `<div class="news ${n.c || ""}"><small>${MONTHS_S[n.t % 12]} ${E.startY + Math.floor(n.t / 12)}</small>${esc(n.m)}</div>`).join("")}</div>` });
    $("#mRank", m.el).onclick = () => { m.close(); this.ranking(); };
    $("#mRes", m.el).onclick = () => { m.close(); this.researchPanel(); };
    $("#mStat", m.el).onclick = () => { m.close(); this.statsPanel(); };
  },
  ranking() {
    const E = this.E, W = this.W;
    const l = W.nations.map((n, i) => i).filter(i => E.nat[i].alive).sort((a, b) => this.power(b) - this.power(a)).slice(0, 40);
    Modal.open({ icon: "🏆", title: "Puissances mondiales", html: `<div class="list" style="padding:0">${l.map((i, k) => `<div class="item" style="${i === E.me ? "border:1px solid var(--gold)" : ""}"><div class="ic">${k + 1}</div><div class="tx"><b>${W.nations[i].flag} ${esc(W.nations[i].n)}</b><small>${this.provsOf(i).length} provinces · PIB ${fmtNum(this.gdpOf(i))} Md$</small></div><div class="rt">🪖 ${fmtNum(this.totalTroops(i) * 1000)}</div></div>`).join("")}</div>` });
  },
  statsPanel() {
    const E = this.E, W = this.W, me = E.me;
    const share = this.provsOf(me).length / W.provs.length * 100;
    const gdpShare = this.gdpOf(me) / W.nations.reduce((s, n, i) => s + (E.nat[i].alive ? this.gdpOf(i) : 0), 0) * 100;
    Modal.open({ icon: "📊", title: "Bilan du mandat", html: `
      <div class="kv"><span>Mois au pouvoir</span><span>${E.turn}</span></div><div class="kv"><span>Provinces conquises</span><span>${E.stats.conquered}</span></div>
      <div class="kv"><span>Provinces perdues</span><span>${E.stats.lost}</span></div><div class="kv"><span>Batailles</span><span>${E.stats.battles}</span></div>
      <div class="kv"><span>Frappes nucléaires</span><span>${E.stats.nukes}</span></div><div class="kv"><span>Part du territoire mondial</span><span>${share.toFixed(1)} %</span></div>
      <div class="kv"><span>Part du PIB mondial</span><span>${gdpShare.toFixed(1)} %</span></div>
      <div class="kv"><span>Vassaux</span><span>${Array.from(E.vassal).filter(v => v === me).length}</span></div>
      <p class="muted" style="font-size:13px">🏆 Victoire : contrôler 35 % du PIB mondial ou 30 % des provinces de la planète (vassaux compris).</p>` });
  },
  gameMenu() {
    const m = Modal.open({ icon: "☰", title: "Menu", html: `<div class="list" style="padding:0">
      <button class="item" data-a="save"><div class="ic">💾</div><div class="tx"><b>Sauvegarder et quitter</b></div></button>
      <button class="item" data-a="new"><div class="ic">🌍</div><div class="tx"><b>Nouvelle partie</b><small>Abandonner la partie en cours</small></div></button>
      <button class="item" data-a="help"><div class="ic">❓</div><div class="tx"><b>Comment jouer</b></div></button></div>` });
    $$(".item", m.el).forEach(b => b.onclick = async () => {
      m.close(); const a = b.dataset.a;
      if (a === "save") { this.save(); showMenu(); }
      if (a === "new" && await Modal.confirm("Nouvelle partie ?", "La partie actuelle sera perdue.")) { Store.del("empire"); this.E = null; this.pickCountry(); }
      if (a === "help") Modal.alert("Comment jouer", `
        <b>Carte</b> : glissez pour vous déplacer, pincez pour zoomer. 🗺️ change le mode (politique, diplomatique, militaire, relief).<br><br>
        <b>Provinces</b> : touchez une province pour recruter, construire (bases, forts, usines, ports, aéroports, universités, silos…), déplacer vos troupes ou attaquer.<br><br>
        <b>Combat</b> : l'attaque compare vos soldats à la défense (garnison + milice, relief, fortifications, technologie). Les montagnes sont redoutables. Les ports permettent les débarquements, les bases aériennes donnent +20 % d'attaque.<br><br>
        <b>Économie</b> : impôts, entretien de l'armée, usines et commerce. Un trésor négatif fait chuter la stabilité.<br><br>
        <b>Politique</b> : en démocratie, une popularité sous 40 % aux élections = défaite. En dictature, gare aux coups d'État si la stabilité s'effondre.<br><br>
        <b>Victoire</b> : 35 % du PIB mondial ou 30 % des provinces.`, "❓");
    });
  },
  news(m, c = "") { this.E.news.push({ t: this.E.turn, m, c }); if (this.E.news.length > 150) this.E.news.shift(); },

  // ---------------- TOUR SUIVANT ----------------
  async nextTurn() {
    const E = this.E, W = this.W, me = E.me;
    if (this.turnBusy || E.over) return; this.turnBusy = true;
    if (this.pending) this.setPending(null);
    Sound.age();
    try {
      E.turn++; E.fresh = {};
      const newsBefore = E.news.length;
      // économie et état de toutes les nations
      for (let n = 0; n < W.nations.length; n++) {
        const S = E.nat[n]; if (!S.alive) continue;
        const ec = this.economy(n); S.gold += ec.net;
        if (S.gold < 0) { S.stab -= 1.5; if (n === me) E.approval = clamp(E.approval - 1); }
        if (S.sanct) S.sanct--;
        // recherche
        S.rp += this.researchRate(n);
        if (n !== me && !S.research) { const ks = Object.keys(TECHS).filter(k => S.tech[k] < TECHS[k].max); S.research = pick(ks); }
        if (S.research) { const c = 60 * Math.pow(S.tech[S.research] + 1, 1.6); if (S.rp >= c) { S.rp -= c; S.tech[S.research]++; if (n === me) { this.news(`🔬 Percée scientifique : ${TECHS[S.research].n} niveau ${S.tech[S.research]} !`, "me good"); toast(`🔬 ${TECHS[S.research].n} ${S.tech[S.research]} !`); } if (S.tech[S.research] >= TECHS[S.research].max || n !== me) S.research = null; } }
        // stabilité
        const target = 60 + 5 * S.tech.prop - (this.enemiesOf(n).length ? 8 : 0) - S.exhaustion * .3 + (S.gold > 0 ? 5 : -10);
        S.stab += (target - S.stab) * .06;
        S.stab = clamp(S.stab, 0, 100);
        S.exhaustion = Math.max(0, S.exhaustion * .97 - .1);
      }
      // dette du joueur
      if (E.debt > 0) { const pay = Math.min(E.debt, E.debt / 24 + .1); E.nat[me].gold -= pay; E.debt -= pay; }
      // provinces : croissance, agitation, révoltes
      for (let i = 0; i < E.own.length; i++) {
        const o = E.own[i];
        E.popM[i] *= 1.0006; E.gdpM[i] = Math.min(2.5, E.gdpM[i] * (1.0015 + E.nat[o].tech.eco * .0003));
        if (E.unrest[i] > 0) {
          E.unrest[i] = Math.max(0, E.unrest[i] - 1.5 - (E.bld[i].hopital || 0) + (E.nat[o].stab < 30 ? 2 : 0) - E.troops[i] * .05);
          if (E.unrest[i] > 70 && chance(.08)) await this.revolt(i);
        }
      }
      // joueur : popularité
      const S = E.nat[me];
      let appT = 55 - Math.max(0, S.tax - 35) * .9 + Math.max(0, 25 - S.tax) * .3 + (S.gold > 0 ? 3 : -8) - this.enemiesOf(me).length * 3 + S.tech.prop * 2;
      for (const i of this.provsOf(me)) appT += (E.bld[i].hopital || 0) * .3;
      E.approval = clamp(E.approval + (appT - E.approval) * .05 + rf(-1, 1));
      // IA
      this.aiTurn();
      // appels à l'aide
      if (E.callToArms && E.callToArms.length) {
        for (const [ally, agg] of E.callToArms) {
          if (!E.nat[ally].alive || !E.nat[agg].alive || this.atWar(me, agg)) continue;
          const r = await Modal.choice("Appel aux armes !", `${W.nations[ally].flag} ${W.nations[ally].n}, votre allié, est attaqué par ${W.nations[agg].flag} ${W.nations[agg].n}. Honorer l'alliance ?`, [{ label: "⚔️ Entrer en guerre", value: 1, cls: "red" }, { label: "🙈 Rester neutre (l'alliance sera rompue)", value: 2 }], "🛡️");
          if (r === 1) { E.wars.push([me, agg]); this.news(`🛡️ ${W.nations[me].n} entre en guerre aux côtés de ${W.nations[ally].n}.`, "me war"); E.rel[me][agg] = E.rel[agg][me] = -60; }
          else { E.allies = E.allies.filter(w => !(w.includes(me) && w.includes(ally))); E.rel[me][ally] = E.rel[ally][me] = clamp(E.rel[me][ally] - 40, -100, 100); this.news(`🙈 ${W.nations[me].n} abandonne son allié ${W.nations[ally].n}.`, "me"); }
        }
        E.callToArms = [];
      }
      // événement aléatoire
      if (chance(.3)) await this.randomEvent();
      // élections / coups d'État
      if (E.regime === "demo") { E.nextElection--; if (E.nextElection <= 0) await this.election(); }
      else if (S.stab < 20 && chance(.12)) { await this.gameOver("coup", "L'armée a renversé votre régime lors d'un coup d'État nocturne."); }
      if (S.stab <= 3 && chance(.25)) await this.gameOver("revolution", "Une révolution populaire a renversé votre gouvernement.");
      if (!E.over) this.checkVictory();
      // résumé
      const fresh = E.news.slice(newsBefore).filter(n => n.c && n.c.includes("me") && !n.c.includes("good"));
      if (fresh.length && !E.over) toast(fresh[fresh.length - 1].m.slice(0, 90), 3000);
    } catch (e) { console.error(e); }
    this.turnBusy = false;
    this.dirty(); this.draw(); this.refreshTop(); this.save();
  },
  async revolt(i) {
    const E = this.E, W = this.W, o = E.own[i], orig = W.provs[i].o;
    const rebels = W.provs[i].pop * E.popM[i] * 2 + 3;
    if (rebels * rf(.5, 1.5) > E.troops[i] * TERRAIN[W.provs[i].t].d) {
      if (orig !== o && (!E.nat[orig].alive || this.atWar(orig, o) || chance(.5))) {
        // indépendance / retour à la nation d'origine
        const wasDead = !E.nat[orig].alive;
        E.own[i] = orig; E.troops[i] = rebels * .5; E.unrest[i] = 0;
        if (wasDead) { E.nat[orig].alive = true; E.nat[orig].cap = i; E.nat[orig].gold = 1; E.nat[orig].stab = 50; E.wars.push([orig, o]); }
        this.news(`🔥 Révolte victorieuse à ${W.provs[i].n} : la province ${wasDead ? "proclame la renaissance de" : "rejoint"} ${W.nations[orig].n} !`, o === E.me ? "me war" : "war");
        if (o === E.me) await Modal.alert("Révolte !", `${W.provs[i].n} s'est soulevée et a échappé à votre contrôle.`, "🔥");
        this.dirty();
      } else { E.troops[i] *= .6; E.gdpM[i] *= .9; this.news(`🔥 Émeutes violentes à ${W.provs[i].n}.`, o === E.me ? "me war" : ""); }
    } else { E.unrest[i] *= .5; E.troops[i] *= .9; if (o === E.me) this.news(`👮 Une révolte a été écrasée à ${W.provs[i].n}.`, "me"); }
  },
  aiTurn() {
    const E = this.E, W = this.W, me = E.me, NN = W.nations.length;
    for (let n = 0; n < NN; n++) {
      const S = E.nat[n]; if (!S.alive || n === me) continue;
      const enemies = this.enemiesOf(n);
      const ps = this.provsOf(n);
      // recrutement
      const ec = this.economy(n);
      if (S.gold > 0 && (enemies.length || ec.net > 0)) {
        const budget = S.gold * (enemies.length ? .5 : .12);
        const add = budget / .12; if (add > .3) { const cap = S.cap; const front = ps.filter(i => W.provs[i].nb.some(j => this.atWar(E.own[j], n))); const t = front.length ? pick(front) : cap; E.troops[t] += add; S.gold -= budget; }
      }
      // construction occasionnelle
      if (S.gold > 8 && chance(.05)) { const i = pick(ps); const k = pick(["fort", "usine", "base", "univ", "hopital"]); if ((E.bld[i][k] || 0) < BUILDINGS[k].max) { E.bld[i][k] = (E.bld[i][k] || 0) + 1; S.gold -= BUILDINGS[k].cost; } }
      if (S.tax < 25) S.tax += 1; if (S.gold < 0 && S.tax < 45) S.tax += 2;
      // guerre : renforcer + attaquer
      if (enemies.length) {
        this.reinforce(n, .25);
        let budgetAtt = 1 + Math.floor(ps.length / 25);
        for (const i of shuffle(ps.slice())) {
          if (budgetAtt <= 0) break;
          if (E.own[i] !== n || E.troops[i] < 1 || (E.fresh && E.fresh[i] === E.turn)) continue;
          const tgs = W.provs[i].nb.filter(j => this.atWar(E.own[j], n));
          if (!tgs.length) continue;
          const tg = tgs.sort((a, b) => this.defense(a) - this.defense(b))[0];
          const force = E.troops[i] * .65 * (1 + .1 * S.tech.arm);
          if (force > this.defense(tg) * (1.5 / E.diff) && chance(.4)) {
            budgetAtt--;
            const victim = E.own[tg];
            const r = this.battle(n, i, tg, E.troops[i] * .65, false);
            if (victim === me) this.news(`${r.win ? "🚨" : "🛡️"} ${W.nations[n].flag} ${W.nations[n].n} attaque ${W.provs[tg].n} : ${r.win ? "province perdue !" : "assaut repoussé !"}`, "me war");
            else if (r.win && chance(.15)) this.news(`⚔️ ${W.nations[n].n} s'empare de ${W.provs[tg].n} (${W.nations[victim].n}).`, "war");
          }
        }
        // débarquements si pas de front terrestre
        const hasFront = ps.some(i => W.provs[i].nb.some(j => this.atWar(E.own[j], n)));
        if (!hasFront && chance(.15 * E.diff)) {
          const ports = ps.filter(i => W.provs[i].c && E.troops[i] > 5);
          for (const i of ports.slice(0, 3)) {
            const R = 1400 + S.tech.nav * 1200, p = W.provs[i];
            const tg = W.provs.map((q, j) => j).filter(j => W.provs[j].c && this.atWar(E.own[j], n) && Math.hypot(W.provs[j].cx - p.cx, W.provs[j].cy - p.cy) < R)[0];
            if (tg !== undefined && E.troops[i] * .7 * .7 > this.defense(tg) * 1.2) { const victim = E.own[tg]; const r = this.battle(n, i, tg, E.troops[i] * .7, true); if (victim === me) this.news(`🚢 Débarquement de ${W.nations[n].n} à ${W.provs[tg].n} : ${r.win ? "la province est tombée !" : "repoussé !"}`, "me war"); break; }
          }
        }
        // paix
        for (const e of enemies) {
          if (e === me) continue;
          const dur = E.turn - (E.warStart[n + "-" + e] ?? E.warStart[e + "-" + n] ?? 0);
          if (dur > 10 && chance(.04 + S.exhaustion * .002)) this.makePeace(n, e);
        }
        // l'IA propose la paix au joueur
        if (enemies.includes(me) && (S.exhaustion > 35 || this.warScore(n, me) > 3) && chance(.06)) E.peaceOffers = (E.peaceOffers || []).concat([n]);
        // capitulation des petites nations face aux IA
        for (const e of enemies) if (e !== me && this.provsOf(n).length <= 1 && chance(.15)) { const own = this.provsOf(n); own.forEach(i => { E.own[i] = e; E.unrest[i] = 50; }); if (own.length) this.eliminate(n, e); break; }
      } else if (chance(.0025 * E.diff * (1 + S.aggr))) {
        // déclaration de guerre opportuniste
        const nbs = new Set(); for (const i of ps) for (const j of W.provs[i].nb) if (E.own[j] !== n) nbs.add(E.own[j]);
        const cands = [...nbs].filter(o => E.nat[o].alive && E.vassal[o] !== n && E.vassal[n] !== o && !this.isAlly(n, o) && !this.hasPact(n, o) && E.rel[n][o] < -15 && this.power(n) > this.power(o) * (o === me ? 1.6 : 1.3) && !(E.truce && E.truce[Math.min(n, o) + "-" + Math.max(n, o)] > E.turn));
        if (cands.length) {
          const t = cands.sort((a, b) => E.rel[n][a] - E.rel[n][b])[0];
          this.declareWar(n, t);
          if (t === me) setTimeout(() => Modal.alert("⚔️ GUERRE !", `${W.nations[n].flag} <b>${W.nations[n].n}</b> vient de nous déclarer la guerre ! Mobilisez vos troupes aux frontières.`, "🚨"), 300);
        }
      }
      // diplomatie IA
      if (chance(.02)) {
        const o = rnd(0, NN - 1);
        if (o !== n && E.nat[o].alive && !this.atWar(n, o)) {
          const r = E.rel[n][o];
          if (r > 60 && !this.isAlly(n, o) && o !== me && chance(.3)) { E.allies.push([n, o]); if (chance(.3)) this.news(`🤝 ${W.nations[n].n} et ${W.nations[o].n} signent une alliance.`, "dip"); }
          else if (r > 10 && !this.hasTrade(n, o) && o !== me) E.trade.push([n, o]);
        }
      }
      // l'IA propose des accords au joueur
      if (chance(.006) && E.rel[n][me] > 40 && !this.atWar(n, me)) E.offers = (E.offers || []).concat([[n, this.isAlly(n, me) ? "trade" : E.rel[n][me] > 60 ? "alliance" : "trade"]]);
      // dérive des relations
      if (chance(.1)) { const o = rnd(0, NN - 1); if (o !== n) E.rel[n][o] = E.rel[o][n] = clamp(E.rel[n][o] + rnd(-3, 3), -100, 100); }
      // vassaux qui se rebellent
      if (E.vassal[n] >= 0 && E.rel[n][E.vassal[n]] < -20 && chance(.03)) { const ov = E.vassal[n]; E.vassal[n] = -1; E.allies = E.allies.filter(w => !(w.includes(n) && w.includes(ov))); this.declareWar(n, ov); this.news(`✊ ${W.nations[n].n} se soulève contre son suzerain ${W.nations[ov].n} !`, ov === me ? "me war" : "war"); }
    }
    // propositions au joueur (traitées après le tour)
    this.pendingOffers();
  },
  async pendingOffers() {
    const E = this.E, W = this.W, me = E.me;
    const po = E.peaceOffers || []; E.peaceOffers = [];
    for (const n of [...new Set(po)]) {
      if (!this.atWar(n, me)) continue;
      const r = await Modal.choice("Proposition de paix", `${W.nations[n].flag} ${W.nations[n].n} propose de mettre fin à la guerre. Chacun conserve les territoires occupés.`, [{ label: "🕊️ Accepter la paix", value: 1, cls: "gold" }, { label: "⚔️ Refuser et continuer", value: 2 }], "🕊️");
      if (r === 1) this.makePeace(me, n);
    }
    const of = E.offers || []; E.offers = [];
    for (const [n, k] of of.slice(0, 2)) {
      if (!E.nat[n].alive || this.atWar(n, me)) continue;
      if (k === "alliance" && this.isAlly(n, me)) continue; if (k === "trade" && this.hasTrade(n, me)) continue;
      const ok = await Modal.confirm(k === "alliance" ? "Proposition d'alliance" : "Proposition commerciale", `${W.nations[n].flag} ${W.nations[n].n} propose ${k === "alliance" ? "une alliance militaire défensive" : "un accord commercial"}.`, "Accepter", "Refuser", k === "alliance" ? "🛡️" : "📦");
      if (ok) { (k === "alliance" ? E.allies : E.trade).push([me, n]); E.rel[me][n] = E.rel[n][me] = clamp(E.rel[me][n] + 8, -100, 100); this.news(`${k === "alliance" ? "🛡️ Alliance" : "📦 Accord commercial"} entre ${W.nations[me].n} et ${W.nations[n].n}.`, "me dip"); }
      else E.rel[me][n] = E.rel[n][me] = clamp(E.rel[me][n] - 5, -100, 100);
    }
    this.dirty(); this.draw(); this.refreshTop();
  },
  async election() {
    const E = this.E, W = this.W;
    E.nextElection = 48;
    const score = E.approval + rf(-6, 6);
    const r = await Modal.choice("🗳️ Élection présidentielle", `Les citoyens de ${W.nations[E.me].n} votent. Sondages : <b>${Math.round(E.approval)} %</b> d'opinions favorables.`, [
      { label: "🗳️ Laisser le peuple décider", value: "fair", cls: "gold" }, { label: "🗃️ Truquer l'élection", sub: "Coût 5 Md$ · risque de scandale", value: "rig" }, { label: "🎖️ Annuler l'élection (coup de force)", sub: "Devenir un régime autoritaire", value: "cancel" }], "🗳️");
    if (r === "rig") {
      E.nat[E.me].gold -= 5;
      if (chance(.35)) { this.news("🗃️ Scandale : la fraude électorale est révélée !", "me war"); E.approval = clamp(E.approval - 20); E.nat[E.me].stab -= 25; if (chance(.5)) return this.gameOver("fraude", "La fraude a été révélée : vous êtes destitué(e) par le Parlement."); }
      else { this.news(`🗳️ ${E.leader} est réélu(e) avec 54 % des voix.`, "me"); }
      return;
    }
    if (r === "cancel") { E.regime = "auto"; E.approval = clamp(E.approval - 25); E.nat[E.me].stab -= 25; this.news(`🎖️ ${E.leader} annule l'élection et s'arroge les pleins pouvoirs.`, "me war"); return; }
    if (score >= 45) { this.news(`🗳️ ${E.leader} est réélu(e) avec ${Math.round(50 + (score - 45) / 2)} % des voix !`, "me good"); E.approval = clamp(E.approval + 5); await Modal.alert("Réélu(e) !", `Le peuple vous renouvelle sa confiance pour 4 ans.`, "🎉"); }
    else await this.gameOver("election", `Vous avez perdu l'élection présidentielle avec ${Math.round(Math.max(20, score))} % des voix.`);
  },
  checkVictory() {
    const E = this.E, W = this.W, me = E.me; if (E.won || E.over) return;
    let gMe = 0, gAll = 0, pMe = 0;
    for (let n = 0; n < W.nations.length; n++) { if (!E.nat[n].alive) continue; const g = this.gdpOf(n); gAll += g; if (n === me || E.vassal[n] === me) { gMe += g; pMe += this.provsOf(n).length; } }
    if (gMe / gAll >= .35 || pMe / W.provs.length >= .3) {
      E.won = true; Sound.good();
      this.news(`🏆 ${W.nations[me].n} est devenue la première hyperpuissance mondiale !`, "me good");
      Modal.alert("🏆 VICTOIRE", `<div class="center"><div style="font-size:70px">👑</div><b>${esc(E.leader)}</b> a fait de <b>${esc(W.nations[me].n)}</b> la puissance dominante de la planète !<br><br>${(gMe / gAll * 100).toFixed(1)} % du PIB mondial · ${(pMe / W.provs.length * 100).toFixed(1)} % des provinces.<br><br>Vous pouvez continuer à jouer.</div>`, "🏆", "Continuer");
    }
  },
  async gameOver(kind, msg) {
    const E = this.E; if (E.over) return; E.over = true; Sound.bad();
    this.save();
    const m = Modal.open({ centered: true, closable: false, html: `<div class="death"><div class="tomb">${kind === "conquis" ? "🏴" : kind === "election" ? "🗳️" : "⚰️"}</div><h2>Fin du mandat</h2>
      <p>${esc(msg)}</p><div class="kv"><span>Durée au pouvoir</span><span>${Math.floor(E.turn / 12)} ans et ${E.turn % 12} mois</span></div>
      <div class="kv"><span>Provinces conquises</span><span>${E.stats.conquered}</span></div><div class="kv"><span>Provinces contrôlées</span><span>${this.provsOf(E.me).length}</span></div>
      <div class="choices" style="margin-top:14px"><button class="btn gold" id="goNew">🌍 Nouvelle partie</button><button class="btn" id="goMenu">🏠 Menu principal</button></div></div>` });
    $("#goNew", m.el).onclick = () => { m.close(); Store.del("empire"); this.E = null; this.pickCountry(); };
    $("#goMenu", m.el).onclick = () => { m.close(); Store.del("empire"); this.E = null; showMenu(); };
  },
  async randomEvent() {
    const E = this.E, W = this.W, me = E.me, S = E.nat[me], N = W.nations[me];
    const myP = this.provsOf(me); if (!myP.length) return;
    const prov = pick(myP), pn = W.provs[prov].n;
    const ev = pick([
      { t: "Catastrophe naturelle", ic: "🌪️", d: `Un ${pick(["séisme", "ouragan", "cyclone", "incendie géant", "tsunami", "glissement de terrain"])} frappe ${pn}.`, o: [["Envoyer une aide massive (3 Md$)", () => { if (this.spend(3)) { E.approval = clamp(E.approval + 6); } }], ["Aide minimale", () => { E.gdpM[prov] *= .9; E.approval = clamp(E.approval - 6); E.unrest[prov] += 20; }]] },
      { t: "Grève générale", ic: "✊", d: "Les syndicats paralysent le pays et réclament des hausses de salaires.", o: [["Céder aux revendications (-4 Md$)", () => { if (this.spend(4)) E.approval = clamp(E.approval + 5); }], ["Tenir bon", () => { E.approval = clamp(E.approval - 5); S.gold -= this.gdpOf(me) * .002; }], ["Envoyer la police", () => { E.approval = clamp(E.approval - 10); S.stab += 3; }]] },
      { t: "Découverte de pétrole", ic: "🛢️", d: `Un immense gisement est découvert à ${pn} !`, o: [["Exploiter immédiatement", () => { E.gdpM[prov] *= 1.25; S.gold += 3; }], ["Fonds souverain pour l'avenir", () => { S.gold += 8; }]] },
      { t: "Scandale politique", ic: "📰", d: "Un ministre est accusé de corruption par la presse.", o: [["Le limoger", () => { E.approval = clamp(E.approval + 2); S.stab -= 2; }], ["Le défendre", () => { E.approval = clamp(E.approval - 8); }], ["Censurer la presse", () => { E.approval = clamp(E.approval - 3); S.stab -= 5; }]] },
      { t: "Pandémie", ic: "🦠", d: "Un nouveau virus se propage rapidement.", o: [["Confinement strict", () => { this.provsOf(me).forEach(i => E.gdpM[i] *= .96); E.approval = clamp(E.approval - 4); }], ["Campagne de vaccination (5 Md$)", () => { if (this.spend(5)) E.approval = clamp(E.approval + 4); }], ["Ne rien faire", () => { this.provsOf(me).forEach(i => E.popM[i] *= .995); E.approval = clamp(E.approval - 10); }]] },
      { t: "Crise financière", ic: "📉", d: "Les marchés s'effondrent, les banques vacillent.", o: [["Sauver les banques (8 Md$)", () => { if (this.spend(8)) S.stab += 4; }], ["Laisser faire le marché", () => { this.provsOf(me).forEach(i => E.gdpM[i] *= .95); E.approval = clamp(E.approval - 6); }]] },
      { t: "Attentat", ic: "💣", d: `Un attentat frappe ${pn}. La nation est sous le choc.`, o: [["Union nationale", () => { E.approval = clamp(E.approval + 6); }], ["Lois sécuritaires d'exception", () => { S.stab += 6; E.approval = clamp(E.approval - 3); }]] },
      { t: "Mouvement séparatiste", ic: "🏴", d: `${pn} réclame son indépendance.`, o: [["Accorder plus d'autonomie", () => { E.unrest[prov] = 0; E.gdpM[prov] *= .95; }], ["Réprimer", () => { E.unrest[prov] = Math.max(E.unrest[prov], 50); S.stab += 2; }], ["Organiser un référendum", () => { if (chance(.65)) { E.unrest[prov] = 0; E.approval = clamp(E.approval + 5); this.news(`🗳️ ${pn} vote pour rester dans ${N.n}.`, "me good"); } else { const neighbors = W.provs[prov].nb.filter(j => E.own[j] !== me); E.own[prov] = neighbors.length ? E.own[neighbors[0]] : W.provs[prov].o === me ? E.own[prov] : W.provs[prov].o; this.news(`🗳️ ${pn} vote pour quitter ${N.n}.`, "me war"); this.dirty(); } }]] },
      { t: "Visite d'un chef d'État", ic: "✈️", d: () => `Le dirigeant de ${W.nations[E.evN].n} vous rend visite.`, pre: () => { E.evN = pick(W.nations.map((x, i) => i).filter(i => i !== me && E.nat[i].alive)); }, o: [["Accueil fastueux (1 Md$)", () => { if (this.spend(1)) E.rel[me][E.evN] = E.rel[E.evN][me] = clamp(E.rel[me][E.evN] + 20, -100, 100); }], ["Accueil protocolaire", () => { E.rel[me][E.evN] = E.rel[E.evN][me] = clamp(E.rel[me][E.evN] + 6, -100, 100); }], ["Le snober", () => { E.rel[me][E.evN] = E.rel[E.evN][me] = clamp(E.rel[me][E.evN] - 15, -100, 100); E.approval = clamp(E.approval + 1); }]] },
      { t: "Percée technologique", ic: "💡", d: "Nos chercheurs font une découverte majeure.", o: [["Application militaire", () => { S.tech.arm = Math.min(TECHS.arm.max, S.tech.arm + 1); }], ["Application civile", () => { S.tech.eco = Math.min(TECHS.eco.max, S.tech.eco + 1); }]] },
      { t: "Réfugiés", ic: "🧳", d: "Des centaines de milliers de réfugiés se pressent à nos frontières.", o: [["Les accueillir", () => { E.popM[prov] *= 1.02; E.approval = clamp(E.approval - 3); for (let n = 0; n < W.nations.length; n++) if (n !== me && chance(.3)) E.rel[me][n] = E.rel[n][me] = clamp(E.rel[me][n] + 3, -100, 100); }], ["Fermer les frontières", () => { E.approval = clamp(E.approval + 2); }]] },
      { t: "Victoire sportive", ic: "🏆", d: "Notre équipe nationale remporte la Coupe du monde !", o: [["Jour férié national !", () => { E.approval = clamp(E.approval + 10); }]] },
      { t: "Tentative de coup d'État", ic: "🎖️", d: "Des généraux complotent contre vous.", c: () => S.stab < 50, o: [["Purger l'armée", () => { this.provsOf(me).forEach(i => E.troops[i] *= .92); S.stab += 8; }], ["Acheter leur loyauté (4 Md$)", () => { if (this.spend(4)) S.stab += 4; }], ["Ignorer", () => { if (chance(.3)) this.gameOver("coup", "Les généraux vous ont renversé(e)."); }]] },
      { t: "Boom économique", ic: "📈", d: "La croissance s'emballe !", o: [["Profiter de l'embellie", () => { this.provsOf(me).forEach(i => E.gdpM[i] *= 1.04); E.approval = clamp(E.approval + 4); }]] },
    ].filter(e => !e.c || e.c()));
    if (ev.pre) ev.pre();
    const d = typeof ev.d === "function" ? ev.d() : ev.d;
    const k = await Modal.choice(ev.t, d, ev.o.map((o, i) => ({ label: o[0], value: i + 1 })), ev.ic);
    if (k) ev.o[k - 1][1]();
    this.news(`${ev.ic} ${ev.t} : ${d}`, "me");
  },
};
