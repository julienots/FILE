"use strict";
// ============================================================
//  UNE VIE — simulateur de vie complet
// ============================================================
const Life = {
  L: null,
  busy: false,

  // ---------------- Entrée / création ----------------
  enter() {
    const s = Store.get("life");
    if (s && s.alive) { this.L = s; this.show(); return; }
    this.newLifeMenu();
  },
  newLifeMenu() {
    Screens.show("life");
    $("#life").innerHTML = `
      <div class="topbar"><button class="back" id="lvBack">←</button><div class="ttl"><b>UNE VIE</b><small>Nouvelle vie</small></div></div>
      <div class="scroll"><div class="list">
        <div class="center" style="padding:16px 0 6px;font-size:64px">🍼</div>
        <p class="center muted" style="margin:0 20px 14px">Chaque vie est unique. Naissez quelque part dans le monde, avec une famille, un caractère et un destin à écrire.</p>
        <button class="btn gold block" id="lvRandom">🎲 Vie aléatoire</button>
        <button class="btn block" id="lvCustom">✏️ Vie personnalisée</button>
        <button class="btn block" id="lvCem">🪦 Cimetière (${(Store.get("cemetery", [])).length})</button>
      </div></div>`;
    $("#lvBack").onclick = () => showMenu();
    $("#lvRandom").onclick = () => { Sound.good(); this.create({}); };
    $("#lvCustom").onclick = () => this.customForm();
    $("#lvCem").onclick = () => this.cemetery();
  },
  customForm() {
    const m = Modal.open({ icon: "✏️", title: "Vie personnalisée", html: `
      <div class="field"><label>Sexe</label><div class="seg" id="cfG"><button data-v="M" class="on">Garçon</button><button data-v="F">Fille</button></div></div>
      <div class="field"><label>Prénom</label><input id="cfF" placeholder="Aléatoire"></div>
      <div class="field"><label>Nom</label><input id="cfL" placeholder="Aléatoire"></div>
      <div class="field"><label>Pays</label><select id="cfC">${LD.countries.map((c, i) => `<option value="${i}">${c.flag} ${c.n}</option>`).join("")}</select></div>
      <div class="field"><label>Ville</label><select id="cfV"></select></div>
      <button class="btn gold block" id="cfGo">Naître ! 👶</button>` });
    let g = "M";
    $$("#cfG button", m.el).forEach(b => b.onclick = () => { g = b.dataset.v; $$("#cfG button", m.el).forEach(x => x.classList.toggle("on", x === b)); });
    const fillCities = () => { const c = LD.countries[+$("#cfC", m.el).value]; $("#cfV", m.el).innerHTML = c.cities.map(v => `<option>${v}</option>`).join(""); };
    $("#cfC", m.el).onchange = fillCities; fillCities();
    $("#cfGo", m.el).onclick = () => {
      const o = { g, first: $("#cfF", m.el).value.trim(), last: $("#cfL", m.el).value.trim(), country: +$("#cfC", m.el).value, city: $("#cfV", m.el).value };
      m.close(); Sound.good(); this.create(o);
    };
  },
  person(g, age, last, extra = {}) {
    const first = pick(g === "M" ? LD.namesM : LD.namesF);
    return Object.assign({ id: Math.random().toString(36).slice(2, 9), n: first + (last ? " " + last : ""), g, age, r: rnd(55, 95), alive: true,
      looks: rnd(10, 100), smarts: rnd(10, 100), money: 0, job: null }, extra);
  },
  create(o) {
    const ci = o.country !== undefined ? o.country : (chance(.55) ? 0 : rnd(0, LD.countries.length - 1));
    const C = LD.countries[ci];
    const g = o.g || (chance(.5) ? "M" : "F");
    const last = o.last || pick(LD.lastNames);
    const day = rnd(1, 28), mon = rnd(0, 11);
    const z = LD.zodiac[mon][1] <= day ? LD.zodiac[mon + 1][0] : LD.zodiac[mon][0];
    const L = this.L = {
      v: 1, first: o.first || pick(g === "M" ? LD.namesM : LD.namesF), last, g, age: 0, alive: true, bd: [day, mon], zodiac: z,
      country: ci, city: o.city || pick(C.cities),
      st: { happy: rnd(60, 100), health: rnd(70, 100), smarts: rnd(5, 100), looks: rnd(5, 100) },
      karma: rnd(40, 70), fame: 0, followers: 0, talent: rnd(0, 100), sport: rnd(0, 100), money: 0, debt: 0,
      log: [], school: null, grades: 0, degrees: [], majors: [], uni: null, job: null, jobs: [], fameJob: null, pol: null, retired: false, pension: 0,
      rel: [], assets: [], home: "parents", dis: [], prison: null, record: [], license: false, sexuality: chance(.9) ? "hetero" : "homo",
      stats: { crimes: 0, partners: 0, vacations: 0, parties: 0, donations: 0, prisonYears: 0, kids: 0, gym: 0 },
      gen: o.gen || 1, flags: {}, pregnant: 0, year: 2026 - 0,
    };
    L.year = 2026;
    // parents
    const fa = this.person("M", rnd(20, 45), last, { t: "pere", money: rnd(500, 60000), job: pick(LD.jobs).t });
    const mo = this.person("F", clamp(fa.age + rnd(-6, 4), 18, 50), chance(.7) ? last : pick(LD.lastNames), { t: "mere", money: rnd(500, 60000), job: pick(LD.jobs).t });
    if (chance(.12)) fa.alive = false, fa.r = 0, fa.absent = true;
    if (o.parents) L.rel.push(...o.parents); else L.rel.push(fa, mo);
    if (!o.parents) { const nb = pick([0, 0, 1, 1, 1, 2, 2, 3]); for (let i = 0; i < nb; i++) { const sg = chance(.5) ? "M" : "F"; L.rel.push(this.person(sg, rnd(1, 12), last, { t: sg === "M" ? "frere" : "soeur" })); } }
    if (o.siblings) L.rel.push(...o.siblings);
    if (o.money) L.money = o.money;
    this.say(`Je suis né${g === "F" ? "e" : ""} ${L.first === "" ? "" : ""}le ${day} ${LD.months[mon]} ${L.year} à ${L.city} (${C.flag} ${C.n}). Je suis ${g === "M" ? "un garçon" : "une fille"}.`, "big");
    const par = L.rel.filter(r => r.t === "pere" || r.t === "mere");
    par.forEach(p => this.say(p.absent ? `Mon père est parti avant ma naissance.` : `${p.t === "pere" ? "Mon père" : "Ma mère"}, ${p.n}, a ${p.age} ans (${p.job || "sans emploi"}).`));
    const sib = L.rel.filter(r => r.t === "frere" || r.t === "soeur");
    if (sib.length) this.say(`J'ai ${sib.map(s => (s.t === "frere" ? "un frère, " : "une sœur, ") + s.n.split(" ")[0] + ` (${s.age} ans)`).join(" et ")}.`);
    this.say(`Mon signe astrologique : ${z}.`);
    if (o.heritage) this.say(o.heritage, "good");
    this.save(); this.show();
  },

  // ---------------- Utilitaires ----------------
  C() { return LD.countries[this.L.country]; },
  cur() { return this.C().cur; },
  m(v) { return fmtMoney(v, this.cur()); },
  he() { return this.L.g === "M" ? "" : "e"; },
  say(t, c = "") {
    const L = this.L; let y = L.log[L.log.length - 1];
    if (!y || y.a !== L.age) { y = { a: L.age, l: [] }; L.log.push(y); if (L.log.length > 140) L.log.shift(); }
    y.l.push([t, c]);
    if (this.rendered) this.renderLog();
  },
  add(k, v) { this.L.st[k] = clamp(this.L.st[k] + v); },
  rels(t) { return this.L.rel.filter(r => (Array.isArray(t) ? t.includes(r.t) : r.t === t) && r.alive); },
  partner() { return this.L.rel.find(r => ["partenaire", "fiance", "conjoint"].includes(r.t) && r.alive); },
  adult() { return this.L.age >= this.C().legal; },
  save() { if (this.L) Store.set("life", this.L); },
  hasDeg(d) { return this.L.degrees.some(x => DEG_RANK[x] >= DEG_RANK[d]) || (d === "police" && this.L.degrees.includes("police")) || (d === "militaire" && this.L.degrees.includes("militaire")); },
  avatar() {
    const L = this.L; if (!L.alive) return "🪦"; if (L.prison) return "⛓️";
    const a = L.age, f = L.g === "F";
    return a < 3 ? "👶" : a < 13 ? (f ? "👧" : "👦") : a < 20 ? (f ? "👩" : "🧑") : a < 60 ? (f ? "👩" : "👨") : (f ? "👵" : "👴");
  },
  occupationLabel() {
    const L = this.L;
    if (L.prison) return `Détenu${this.he()} (${L.prison.left} an${L.prison.left > 1 ? "s" : ""} restant${L.prison.left > 1 ? "s" : ""})`;
    if (L.pol) return LD.politics[L.pol.lvl];
    if (L.fameJob) { const f = LD.fameCareers.find(x => x.id === L.fameJob.id); return f.lv[L.fameJob.lvl]; }
    if (L.job) return L.job.t + (L.job.pt ? " (temps partiel)" : "");
    if (L.uni) return `Étudiant${this.he()} · ${LD.majors.find(x => x.id === L.uni.maj).n}`;
    if (L.school) return { creche: "Crèche", maternelle: "École maternelle", primaire: "École primaire", college: "Collège", lycee: "Lycée" }[L.school];
    if (L.retired) return `Retraité${this.he()}`;
    if (L.age < 3) return "Bébé";
    return "Sans emploi";
  },

  // ---------------- Interface ----------------
  show() {
    Screens.show("life");
    const L = this.L;
    $("#life").innerHTML = `
      <div class="lv-head">
        <div class="lv-ava" id="lvAva"></div>
        <div class="lv-id"><b id="lvName"></b><small id="lvSub"></small></div>
        <div style="text-align:right"><div class="money" id="lvMoney"></div><button class="ghost" id="lvMenu" style="padding:6px 10px;margin-top:4px">☰</button></div>
      </div>
      <div class="lv-log" id="lvLog"></div>
      <div class="lv-dock">
        <div class="lv-stats" id="lvStats"></div>
        <div class="lv-actions">
          <button id="aJob"><span class="i">💼</span>Occupation</button>
          <button id="aAssets"><span class="i">🏠</span>Biens</button>
          <button class="age" id="aAge"><span class="i">+</span>Vieillir</button>
          <button id="aRel"><span class="i">❤️</span>Relations</button>
          <button id="aAct"><span class="i">🎯</span>Activités</button>
        </div>
      </div>`;
    this.rendered = true;
    $("#aAge").onclick = () => this.ageUp();
    $("#aJob").onclick = () => this.occupation();
    $("#aAssets").onclick = () => this.assetsMenu();
    $("#aRel").onclick = () => this.relationsMenu();
    $("#aAct").onclick = () => this.activities();
    $("#lvMenu").onclick = () => this.mainMenu();
    $("#lvAva").onclick = () => this.profile();
    this.refresh(); this.renderLog();
  },
  refresh() {
    const L = this.L; if (!this.rendered || Screens.cur !== "life" || !$("#lvName")) return;
    $("#lvAva").textContent = this.avatar();
    $("#lvName").textContent = `${L.first} ${L.last}`;
    $("#lvSub").textContent = `${L.age} an${L.age > 1 ? "s" : ""} · ${this.occupationLabel()} · ${this.C().flag}`;
    const mo = $("#lvMoney"); mo.textContent = this.m(L.money); mo.classList.toggle("neg", L.money < 0);
    const S = [["Bonheur", "happy", "#ffc926", "😊"], ["Santé", "health", "#ff4d5e", "❤️"], ["Intellect", "smarts", "#4aa3ff", "🧠"], ["Beauté", "looks", "#ff6fb5", "✨"]];
    $("#lvStats").innerHTML = S.map(([n, k, c, e]) => `<div class="stat"><span class="nm">${e} ${n}</span><div class="bar"><i style="width:${L.st[k]}%;background:${L.st[k] < 25 ? "#ff4d5e" : c}"></i></div><span class="v">${Math.round(L.st[k])}%</span></div>`).join("");
    $("#aAge .i").textContent = L.alive ? "+" : "🪦";
  },
  renderLog() {
    const el = $("#lvLog"); if (!el) return;
    el.innerHTML = this.L.log.map(y => `<div class="lv-year"><h4>${y.a === 0 ? "Naissance" : `${y.a} an${y.a > 1 ? "s" : ""}`}</h4>${y.l.map(([t, c]) => `<p class="${c}">${esc(t)}</p>`).join("")}</div>`).join("");
    el.scrollTop = el.scrollHeight;
    this.refresh();
  },
  mainMenu() {
    const m = Modal.open({ icon: "☰", title: "Menu", html: `<div class="list" style="padding:0">
      <button class="item" data-a="profile"><div class="ic">🪪</div><div class="tx"><b>Profil complet</b><small>Statistiques, diplômes, casier…</small></div></button>
      <button class="item" data-a="cem"><div class="ic">🪦</div><div class="tx"><b>Cimetière</b><small>Vos vies passées</small></div></button>
      <button class="item" data-a="new"><div class="ic">🍼</div><div class="tx"><b>Nouvelle vie</b><small>Abandonner cette vie</small></div></button>
      <button class="item" data-a="quit"><div class="ic">🏠</div><div class="tx"><b>Menu principal</b><small>La partie est sauvegardée</small></div></button></div>` });
    $$(".item", m.el).forEach(b => b.onclick = async () => {
      m.close(); const a = b.dataset.a;
      if (a === "profile") this.profile();
      if (a === "cem") this.cemetery();
      if (a === "quit") { this.save(); showMenu(); }
      if (a === "new" && await Modal.confirm("Nouvelle vie ?", "Votre vie actuelle sera abandonnée (elle n'ira pas au cimetière).")) { Store.del("life"); this.newLifeMenu(); }
    });
  },
  profile() {
    const L = this.L, C = this.C();
    const kv = (a, b) => `<div class="kv"><span>${a}</span><span>${b}</span></div>`;
    Modal.open({ icon: this.avatar(), title: `${L.first} ${L.last}`, sub: `${L.age} ans · ${C.flag} ${L.city}`, html:
      kv("Naissance", `${L.bd[0]} ${LD.months[L.bd[1]]} ${L.year - L.age}`) + kv("Signe", L.zodiac) + kv("Génération", L.gen) +
      kv("Argent", this.m(L.money)) + (L.debt ? kv("Dette étudiante", this.m(L.debt)) : "") + kv("Occupation", this.occupationLabel()) +
      kv("Diplômes", L.degrees.length ? L.degrees.map(d => d.toUpperCase()).join(", ") : "Aucun") + kv("Karma", L.karma >= 70 ? "Ange 😇" : L.karma >= 45 ? "Correct 🙂" : L.karma >= 25 ? "Douteux 😏" : "Diabolique 😈") +
      kv("Célébrité", `${Math.round(L.fame)}%`) + kv("Abonnés", fmtNum(L.followers)) + kv("Talent artistique", Math.round(L.talent) + "%") + kv("Forme physique", Math.round(L.sport) + "%") +
      kv("Permis de conduire", L.license ? "Oui ✅" : "Non") + kv("Logement", { parents: "Chez mes parents", loyer: "Locataire", proprio: "Propriétaire", sdf: "Sans domicile" }[L.home]) +
      kv("Maladies", L.dis.length ? L.dis.map(d => d.n).join(", ") : "Aucune") + kv("Casier judiciaire", L.record.length ? L.record.join(", ") : "Vierge") +
      kv("Partenaires", L.stats.partners) + kv("Enfants", this.rels("enfant").length) + kv("Vacances", L.stats.vacations) });
  },
  cemetery() {
    const cem = Store.get("cemetery", []);
    Modal.open({ icon: "🪦", title: "Cimetière", sub: `${cem.length} vie(s) terminée(s)`, html: cem.length ? `<div class="list" style="padding:0">${cem.slice().reverse().map(c => `
      <div class="item"><div class="ic">${(LD.ribbons[c.rib] || LD.ribbons.ordinaire)[0]}</div><div class="tx"><b>${esc(c.n)}</b><small>${c.y0}–${c.y1} · ${c.age} ans · ${esc(c.job || "")}<br>« ${(LD.ribbons[c.rib] || LD.ribbons.ordinaire)[1]} » · ${fmtMoney(c.money, c.cur)} · ${esc(c.cause)}</small></div></div>`).join("")}</div>` : `<p class="muted center">Personne n'est encore mort… pour l'instant.</p>` });
  },

  // ---------------- VIEILLIR ----------------
  async ageUp() {
    const L = this.L;
    if (this.busy) return;
    if (!L.alive) return this.deathScreen();
    this.busy = true; Sound.age();
    try {
      L.age++; L.year++;
      this.say(L.age === 1 ? "J'ai soufflé ma première bougie !" : "", "");
      L.log[L.log.length - 1].l.pop();
      this.yearlyFamily();
      this.yearlyMoney();
      this.yearlyBody();
      if (L.prison) await this.prisonYear();
      else { await this.yearlySchool(); await this.yearlyWork(); }
      if (L.alive) await this.randomEvents();
      this.checkDeath();
      this.cleanupYear();
    } catch (e) { console.error(e); }
    this.busy = false;
    this.save(); this.renderLog();
    if (!L.alive) setTimeout(() => this.deathScreen(), 600);
  },
  cleanupYear() {
    const L = this.L;
    const y = L.log[L.log.length - 1];
    if (!y || y.a !== L.age || !y.l.length) this.say(pick(["Une année tranquille.", "Rien de spécial cette année.", "L'année est passée vite.", "Une année comme les autres."]), "");
    L.st.happy = clamp(L.st.happy + (60 - L.st.happy) * 0.08);
    // la réputation de l'âge
    if (L.fame > 0 && !L.fameJob && !L.pol) L.fame = Math.max(0, L.fame - 2);
    L.flags.yearActs = {};
  },
  yearlyFamily() {
    const L = this.L;
    for (const r of L.rel) {
      if (!r.alive) continue;
      r.age++;
      if (r.t === "animal") {
        if (r.age >= r.life) { r.alive = false; this.say(`${r.ic} Mon ${r.kind.toLowerCase()} ${r.n} est mort de vieillesse. 💔`, "bad"); this.add("happy", -15); }
        continue;
      }
      // relation qui s'érode
      r.r = clamp(r.r - rnd(0, 4));
      // décès des proches
      const p = r.age < 50 ? .002 : r.age < 70 ? .012 : r.age < 80 ? .04 : r.age < 90 ? .1 : .25;
      if (chance(p)) {
        r.alive = false;
        const cause = pick(["d'une crise cardiaque", "d'un cancer", "dans un accident de voiture", "paisiblement dans son sommeil", "d'un AVC", "d'une longue maladie", "d'une pneumonie"]);
        this.say(`💔 ${this.relLabel(r)} ${r.n} est décédé${r.g === "F" ? "e" : ""} ${r.age > 75 ? "paisiblement dans son sommeil" : cause}, à ${r.age} ans.`, "bad");
        this.add("happy", r.t === "pere" || r.t === "mere" || r.t === "enfant" || r.t === "conjoint" ? -30 : -12);
        if ((r.t === "pere" || r.t === "mere") && L.age >= 18 && r.money > 0 && chance(.8)) {
          const h = Math.round(r.money * rf(.3, 1) * .8); L.money += h; this.say(`J'ai hérité de ${this.m(h)}.`, "good");
        }
        if (r.t === "conjoint" || r.t === "partenaire" || r.t === "fiance") { r.t = "defunt"; this.say("Je suis désormais veu" + (L.g === "F" ? "ve" : "f") + ".", "bad"); }
      }
      if (r.t === "enfant" && r.age === 18 && !r.left) { r.left = true; this.say(`${r.n.split(" ")[0]} a eu 18 ans et a quitté la maison.`); }
    }
    if (L.pregnant) {
      L.pregnant = 0;
      const p = this.partner();
      const g = chance(.5) ? "M" : "F";
      const kid = this.person(g, 0, L.last, { t: "enfant", r: 100 });
      L.rel.push(kid); L.stats.kids++;
      this.say(`👶 ${L.g === "F" ? "J'ai donné naissance à" : (p ? p.n.split(" ")[0] + " a donné naissance à" : "Mon bébé est né :")} ${g === "M" ? "un petit garçon" : "une petite fille"}, ${kid.n.split(" ")[0]} !`, "good");
      this.add("happy", 20);
    }
    // parents & départ de la maison
    if (L.age === 18 && L.home === "parents") this.say("J'ai 18 ans : je peux désormais quitter le domicile familial (onglet Biens).");
  },
  yearlyMoney() {
    const L = this.L, C = this.C();
    let income = 0, out = 0;
    if (L.job && !L.prison) income += L.job.sal * .78;
    if (L.fameJob && !L.prison) income += this.fameIncome();
    if (L.pol && !L.prison) income += [3000, 30000, 25000, 85000, 90000, 120000, 150000, 180000][L.pol.lvl];
    if (L.retired) income += L.pension;
    if (L.followers > 10000 && !L.prison) income += Math.round(L.followers * .02);
    // logement
    if (L.age >= 18 && !L.prison) {
      if (L.home === "loyer") out += (L.rent || 7000) * C.col;
      if (L.home === "proprio") out += L.assets.filter(a => a.k === "house").reduce((s, a) => s + a.p * .012, 0);
      if (L.home !== "parents") out += 5500 * C.col; // vie courante
    }
    out += L.assets.filter(a => a.k === "car").reduce((s, a) => s + Math.min(a.p * .06, 60000), 0);
    out += this.rels("animal").length * 600;
    out += this.rels("enfant").filter(k => k.age < 18).length * 4500 * C.col;
    // revenus locatifs
    income += L.assets.filter(a => a.rented).reduce((s, a) => s + a.p * .045, 0);
    // dette étudiante
    if (L.debt > 0 && (L.job || L.fameJob)) { const pay = Math.min(L.debt, Math.max(1500, L.debt * .15)); L.debt -= pay; out += pay; }
    else if (L.debt > 0) L.debt = Math.round(L.debt * 1.02);
    L.money += Math.round(income - out);
    // dépréciation & appréciation des biens
    for (const a of L.assets) { if (a.k === "car") a.p = Math.round(a.p * .87); else a.p = Math.round(a.p * rf(.98, 1.06)); }
    if (L.money < -20000 && !L.flags.bankrupt) { L.flags.bankrupt = 1; this.say("Je croule sous les dettes… Les huissiers me harcèlent.", "bad"); this.add("happy", -15); }
    if (L.money >= 0) L.flags.bankrupt = 0;
  },
  yearlyBody() {
    const L = this.L, a = L.age;
    if (a < 18) this.add("smarts", rnd(0, 3));
    if (a > 30) this.add("looks", -rf(0, a > 55 ? 2.5 : 1));
    const base = clamp(95 - Math.max(0, a - 35) * 0.9 + (L.sport - 50) * .1);
    L.st.health = clamp(L.st.health + (base - L.st.health) * .18);
    L.sport = clamp(L.sport - 2);
    for (const d of L.dis) {
      this.add("health", -d.sev * rf(.35, .7)); if (d.mood) this.add("happy", -8);
      if (!d.incurable && chance(d.heal)) { this.say(`J'ai guéri de : ${d.n}.`, "good"); d.gone = 1; }
    }
    L.dis = L.dis.filter(d => !d.gone);
    // nouvelle maladie
    const risk = .07 + (a > 50 ? .06 : 0) + (a > 70 ? .08 : 0) + (L.st.health < 40 ? .05 : 0);
    if (chance(risk)) {
      const pool = LD.diseases.filter(d => (!d.minAge || a >= d.minAge) && (!d.maxAge || a <= d.maxAge) && !L.dis.some(x => x.n === d.n));
      if (pool.length) { const d = pick(pool); L.dis.push(Object.assign({}, d)); this.say(`🤒 On m'a diagnostiqué : ${d.n}.`, "bad"); this.add("happy", -5); }
    }
  },
  checkDeath() {
    const L = this.L, a = L.age;
    let p = a < 1 ? .002 : a < 40 ? .0008 : a < 60 ? .003 : a < 70 ? .012 : a < 80 ? .035 : a < 90 ? .09 : a < 100 ? .22 : .45;
    if (L.st.health < 20) p += .12; if (L.st.health < 8) p += .3;
    p *= L.dis.some(d => d.deadly) ? 2.2 : 1;
    if (L.st.health <= 0 || chance(p)) {
      const cause = L.st.health <= 5 ? (L.dis.length ? `des suites de : ${L.dis[0].n}` : "de problèmes de santé") : a > 80 ? "paisiblement dans son sommeil" : pick(["d'une crise cardiaque", "d'un AVC", "dans un accident de la route", L.dis.length ? `des suites de : ${L.dis[0].n}` : "d'une rupture d'anévrisme"]);
      this.die(cause);
    }
  },
  die(cause) {
    const L = this.L; if (!L.alive) return;
    L.alive = false; L.cause = cause;
    this.say(`🪦 Je suis mort${this.he()} ${cause}, à l'âge de ${L.age} ans.`, "big");
    Sound.bad();
    // ruban
    L.rib = this.ribbon();
    const cem = Store.get("cemetery", []);
    cem.push({ n: `${L.first} ${L.last}`, age: L.age, y0: L.year - L.age, y1: L.year, money: L.money, cur: this.cur(), job: this.occupationLabel(), cause, rib: L.rib });
    if (cem.length > 200) cem.shift();
    Store.set("cemetery", cem);
  },
  ribbon() {
    const L = this.L, s = L.st, sc = {};
    sc.riche = L.money > 5e6 ? 10 : L.money > 1e6 ? 6 : 0;
    sc.celebre = L.fame > 80 ? 10 : L.fame > 50 ? 6 : 0;
    sc.criminel = L.stats.crimes > 8 ? 8 : L.stats.crimes > 4 ? 5 : 0;
    sc.prison = L.stats.prisonYears > 10 ? 9 : 0;
    sc.genie = s.smarts > 92 ? 7 : 0; sc.beau = s.looks > 92 ? 6 : 0;
    sc.heureux = s.happy > 85 ? 5 : 0; sc.triste = s.happy < 20 ? 6 : 0;
    sc.sportif = L.sport > 85 ? 5 : 0; sc.romantique = L.stats.partners > 8 ? 7 : 0;
    sc.famille = this.rels("enfant").length + L.rel.filter(r => r.t === "enfant").length / 2 > 4 ? 7 : 0;
    sc.genereux = L.stats.donations > 50000 || L.karma > 90 ? 7 : 0;
    sc.vieux = L.age >= 100 ? 11 : 0; sc.jeune = L.age < 25 ? 9 : 0;
    sc.politique = L.pol && L.pol.lvl >= 5 ? 12 : 0; sc.diplome = L.degrees.includes("doctorat") ? 5 : 0;
    sc.fetard = L.stats.parties > 25 ? 6 : 0;
    const best = Object.entries(sc).sort((a, b) => b[1] - a[1])[0];
    return best[1] > 0 ? best[0] : "ordinaire";
  },
  deathScreen() {
    const L = this.L; const [ic, nm, ds] = LD.ribbons[L.rib] || LD.ribbons.ordinaire;
    const heirs = L.rel.filter(r => r.t === "enfant" && r.alive);
    const assetsV = L.assets.reduce((s, a) => s + a.p, 0);
    const m = Modal.open({ centered: true, closable: false, html: `<div class="death">
      <div class="tomb">🪦</div><h2>${esc(L.first)} ${esc(L.last)}</h2><div class="muted">${L.year - L.age} – ${L.year} · ${L.age} ans</div>
      <p class="muted">Mort${this.he()} ${esc(L.cause)}</p>
      <div class="ribbon">${ic} ${nm}</div><p style="font-size:13px" class="muted">${ds}</p>
      <div class="kv"><span>Fortune</span><span>${this.m(L.money)}</span></div><div class="kv"><span>Patrimoine</span><span>${this.m(assetsV)}</span></div>
      <div class="kv"><span>Dernière occupation</span><span>${esc(this.occupationLabel())}</span></div><div class="kv"><span>Enfants</span><span>${heirs.length}</span></div>
      <div class="choices" style="margin-top:14px">
        ${heirs.map((h, i) => `<button class="btn gold" data-h="${i}">👪 Continuer en tant que ${esc(h.n.split(" ")[0])} (${h.age} ans)</button>`).join("")}
        <button class="btn" id="dNew">🍼 Nouvelle vie</button><button class="btn" id="dMenu">🏠 Menu principal</button></div></div>` });
    $$("[data-h]", m.el).forEach(b => b.onclick = () => { m.close(); this.inherit(heirs[+b.dataset.h], assetsV); });
    $("#dNew", m.el).onclick = () => { m.close(); Store.del("life"); this.newLifeMenu(); };
    $("#dMenu", m.el).onclick = () => { m.close(); Store.del("life"); showMenu(); };
  },
  inherit(h, assetsV) {
    const P = this.L;
    const heirs = P.rel.filter(r => r.t === "enfant" && r.alive);
    const total = Math.max(0, P.money + assetsV - P.debt) * .8;
    const share = Math.round(total / heirs.length);
    const parent = { id: "p", n: `${P.first} ${P.last}`, g: P.g, age: P.age, r: 0, alive: false, t: P.g === "M" ? "pere" : "mere" };
    const other = P.rel.find(r => r.t === "conjoint" && r.alive);
    const parents = [parent]; if (other) parents.push(Object.assign({}, other, { t: other.g === "M" ? "pere" : "mere" }));
    const sibs = heirs.filter(x => x !== h).map(x => Object.assign({}, x, { t: x.g === "M" ? "frere" : "soeur" }));
    const [first, ...rest] = h.n.split(" ");
    this.create({ g: h.g, first, last: rest.join(" ") || P.last, country: P.country, city: P.city, parents, siblings: sibs, gen: P.gen + 1,
      money: share, heritage: `Génération ${P.gen + 1}. J'hérite de ${fmtMoney(share, LD.countries[P.country].cur)} de mon parent ${P.first}.` });
    // rattraper l'âge
    const L = this.L; L.age = h.age; L.year = P.year; L.st.smarts = h.smarts || L.st.smarts; L.st.looks = h.looks || L.st.looks;
    L.log = [{ a: L.age, l: [[`Je reprends ma vie à ${L.age} ans, après la mort de mon parent ${P.first}.`, "big"], [L.log[0].l.slice(-1)[0][0], "good"]] }];
    if (L.age >= 18) { L.home = "loyer"; L.rent = 7000; } else this.autoSchool();
    this.save(); this.show();
  },

  // ---------------- École ----------------
  autoSchool() {
    const L = this.L, a = L.age;
    const st = a < 3 ? (a >= 1 ? "creche" : null) : a < 6 ? "maternelle" : a < 11 ? "primaire" : a < 15 ? "college" : a < 18 ? "lycee" : null;
    if (st && L.school !== st && !L.flags.dropout) {
      const names = { creche: "la crèche", maternelle: "l'école maternelle", primaire: "l'école primaire", college: "le collège", lycee: "le lycée" };
      L.school = st; if (st === "primaire" || st === "college" || st === "lycee") this.say(`🎒 J'ai fait ma rentrée ${st === "lycee" ? "au" : st === "college" ? "au" : "à"} ${names[st].replace(/^l[ea] |^l'/, "")}.`.replace("à école", "à l'école").replace("au le", "au"));
      if (!L.grades) L.grades = clamp(L.st.smarts + rnd(-15, 15));
    }
  },
  async yearlySchool() {
    const L = this.L;
    if (L.age < 18 && !L.flags.dropout) {
      this.autoSchool();
      L.grades = clamp(L.grades + (L.st.smarts - L.grades) * .2 + rnd(-6, 6));
      if (L.age === 15) { if (L.grades >= 40) { L.degrees.push("brevet"); this.say("📜 J'ai obtenu le brevet des collèges !", "good"); } else this.say("J'ai raté le brevet.", "bad"); }
      return;
    }
    if (L.age === 18 && L.school === "lycee") {
      L.school = null;
      const p = L.grades / 100 * .7 + L.st.smarts / 100 * .4;
      if (chance(clamp(p * 100, 10, 98) / 100)) {
        const mention = L.grades > 85 ? " avec mention très bien 🏅" : L.grades > 70 ? " avec mention bien" : L.grades > 58 ? " avec mention assez bien" : "";
        L.degrees.push("bac"); this.say(`🎓 J'ai obtenu mon baccalauréat${mention} !`, "good"); this.add("happy", 12);
        const go = await Modal.choice("Après le bac", "Que voulez-vous faire maintenant ?", [
          { label: "🎓 Faire des études supérieures", value: "uni" }, { label: "💼 Chercher un travail", value: "work" }, { label: "🌴 Prendre une année sabbatique", value: "gap" }], "🎓");
        if (go === "uni") await this.chooseUni(); else if (go === "gap") { this.say("Je prends une année sabbatique pour réfléchir."); this.add("happy", 10); }
        else this.say("J'ai décidé d'entrer dans la vie active.");
      } else { this.say("J'ai raté mon baccalauréat… 😞", "bad"); this.add("happy", -15); L.flags.retakeBac = 1; }
      return;
    }
    if (L.uni) {
      const U = L.uni, M = LD.majors.find(x => x.id === U.maj);
      L.grades = clamp(L.grades + (L.st.smarts - L.grades) * .2 + rnd(-5, 5));
      if (L.grades < 25 && chance(.4)) { this.say(`J'ai redoublé mon année de ${M.n} à cause de mes mauvaises notes.`, "bad"); U.fails = (U.fails || 0) + 1; if (U.fails >= 2) { this.say("J'ai été exclu" + this.he() + " de l'université.", "bad"); L.uni = null; } return; }
      U.year++;
      const cost = M.cost * this.C().col;
      if (U.paidBy === "parents") { } else if (U.paidBy === "bourse") { } else if (U.paidBy === "pret" || L.money < cost) L.debt += cost; else L.money -= cost;
      if (U.year >= M.y) {
        L.degrees.push(M.deg); L.majors.push(M.id); L.uni = null;
        this.say(`🎓 Diplômé${this.he()} ! J'ai terminé mes études de ${M.n} (${M.deg.toUpperCase()}).`, "good"); this.add("happy", 15); this.add("smarts", 5);
      } else this.say(`J'ai validé ma ${U.year}${U.year === 1 ? "re" : "e"} année de ${M.n}.`);
    }
  },
  async chooseUni() {
    const L = this.L;
    if (!L.degrees.includes("bac")) return Modal.alert("Impossible", "Il faut le baccalauréat pour entrer dans l'enseignement supérieur.", "🎓");
    const opts = LD.majors.map(M => ({ label: `${M.n}`, sub: `${M.y} an${M.y > 1 ? "s" : ""} · ${M.deg.toUpperCase()} · ${M.cost ? this.m(M.cost * this.C().col) + "/an" : "gratuit"}${L.st.smarts < M.intel - 15 ? " · niveau insuffisant ?" : ""}`, value: M.id }));
    const id = await Modal.choice("Études supérieures", "Choisissez votre filière :", opts, "🎓", true);
    if (!id) return;
    const M = LD.majors.find(x => x.id === id);
    const p = clamp(50 + (L.st.smarts - M.intel) * 2 + (L.grades - 50) * .6, 5, 98) / 100;
    if (!chance(p)) { this.say(`Ma candidature en ${M.n} a été refusée. 😔`, "bad"); this.add("happy", -8); this.save(); this.renderLog(); return; }
    let paid = "self";
    if (M.cost > 0) {
      const par = this.rels(["pere", "mere"]);
      const opts2 = [{ label: "💳 Prêt étudiant", sub: "À rembourser une fois en poste", value: "pret" }, { label: "💶 Payer moi-même", sub: this.m(L.money) + " disponibles", value: "self" }];
      if (par.length) opts2.unshift({ label: "👪 Demander à mes parents", value: "parents" });
      if (L.grades > 80) opts2.unshift({ label: "🏅 Demander une bourse au mérite", value: "bourse" });
      paid = await Modal.choice("Financement", `Comment financer ${this.m(M.cost * this.C().col)} par an ?`, opts2, "💶") || "pret";
      if (paid === "parents") { const rich = par.reduce((s, p) => s + p.money, 0); if (rich < M.cost * 2 || chance(.25)) { this.say("Mes parents ne peuvent pas payer mes études. Je prends un prêt étudiant."); paid = "pret"; } else this.say("Mes parents vont financer mes études. Merci ! 🙏", "good"); }
      if (paid === "bourse") { if (chance(.6)) this.say("J'ai obtenu une bourse au mérite ! 🏅", "good"); else { this.say("Bourse refusée. Je prends un prêt étudiant."); paid = "pret"; } }
    }
    L.uni = { maj: id, year: 0, paidBy: paid }; L.job = null;
    this.say(`🏫 Je suis admis${this.he()} en ${M.n} !`, "good"); this.add("happy", 10);
    this.save(); this.renderLog();
  },

  // ---------------- Travail ----------------
  async yearlyWork() {
    const L = this.L;
    if (L.job) {
      L.job.yrs++;
      L.job.perf = clamp(L.job.perf + rnd(-8, 6) + (L.st.smarts - 50) / 25);
      if (L.job.perf < 15 && chance(.5)) { this.say(`🔥 J'ai été licencié${this.he()} de mon poste de ${L.job.t} pour mauvaises performances.`, "bad"); this.add("happy", -20); L.jobs.push(L.job.t); L.job = null; }
      else if (L.job.car && L.job.lvl < L.job.car.length - 1 && L.job.perf > 70 && L.job.yrs >= 2 && chance(.35)) this.promote();
      else if (L.job && chance(.25)) { const r = Math.round(L.job.sal * rf(.01, .04)); L.job.sal += r; }
      if (L.job && L.job.pt && L.age >= 18 && chance(.3)) this.say(`Je travaille toujours à temps partiel comme ${L.job.t}.`);
    }
    if (L.fameJob) this.fameYear();
    if (L.pol) await this.polYear();
    const C = this.C();
    if (!L.retired && L.age >= C.retire && (L.job || L.age >= C.retire + 3)) {
      if (L.job) {
        const ok = await Modal.confirm("Retraite", `Vous avez ${L.age} ans. Partir à la retraite ?`, "Prendre ma retraite", "Continuer à travailler", "🏖️");
        if (ok) this.retire();
      }
    }
  },
  promote() {
    const J = this.L.job; J.lvl++; J.t = J.car[J.lvl]; J.sal = Math.round(J.sal * rf(1.2, 1.45)); J.perf = 55;
    this.say(`📈 J'ai été promu${this.he()} : ${J.t} ! Nouveau salaire : ${this.m(J.sal)}/an.`, "good"); this.add("happy", 12); Sound.good();
  },
  retire() {
    const L = this.L;
    L.pension = Math.round((L.job ? L.job.sal : 15000) * .55); L.retired = true;
    if (L.job) L.jobs.push(L.job.t); L.job = null;
    this.say(`🏖️ J'ai pris ma retraite ! Pension : ${this.m(L.pension)}/an.`, "good"); this.add("happy", 15);
  },
  jobOffers() {
    const L = this.L;
    if (!L.flags.offers || L.flags.offersAge !== L.age) {
      let pool = LD.jobs.filter(j => L.age < 18 ? j.pt : !j.pt || chance(.3));
      L.flags.offers = shuffle(pool.map((j, i) => LD.jobs.indexOf(j))).slice(0, L.age < 18 ? 5 : 14);
      L.flags.offersAge = L.age;
    }
    return L.flags.offers.map(i => LD.jobs[i]);
  },
  reqOk(j) {
    const L = this.L; const why = [];
    if (j.deg && !this.hasDeg(j.deg)) why.push("diplôme : " + j.deg.toUpperCase());
    if (j.maj && !L.majors.includes(j.maj)) why.push("filière : " + (LD.majors.find(m => m.id === j.maj) || {}).n);
    if (j.drive && !L.license) why.push("permis de conduire");
    if (j.intel && L.st.smarts < j.intel - 10) why.push("intelligence");
    if (j.fit && L.sport < j.fit - 20) why.push("forme physique");
    if (L.record.length && ["Sécurité", "Justice", "Finance", "Politique", "Éducation"].includes(j.cat)) why.push("casier judiciaire");
    return why;
  },
  async apply(j) {
    const L = this.L;
    if (L.prison) return;
    const why = this.reqOk(j);
    if (why.length) return Modal.alert("Candidature impossible", "Il vous manque : " + why.join(", ") + ".", "📋");
    const Q = pick([
      ["Pourquoi voulez-vous ce poste ?", [["Je suis passionné par ce métier.", 1], ["Pour l'argent, clairement.", -1], ["Je cherche à progresser et apprendre.", 1], ["Ma mère m'a obligé.", -2]]],
      ["Quel est votre plus grand défaut ?", [["Je suis trop perfectionniste.", 0], ["Je n'en ai aucun.", -1], ["J'ai du mal à déléguer, mais je travaille dessus.", 1], ["Je dors beaucoup.", -2]]],
      ["Où vous voyez-vous dans 5 ans ?", [["À votre place !", -1], ["Responsable d'une équipe ici.", 1], ["Aucune idée.", -1], ["En train de maîtriser parfaitement ce poste.", 1]]],
      ["Comment gérez-vous la pression ?", [["Je m'organise et je priorise.", 1], ["Je crie un bon coup.", -2], ["Je fais du sport pour décompresser.", 1], ["Je ne la gère pas.", -2]]],
      ["Parlez-moi de vous.", [["Je suis rigoureux et motivé.", 1], ["J'aime les chats et les jeux vidéo.", 0], ["Il n'y a pas grand-chose à dire.", -1], ["Je suis le meilleur candidat, point.", 0]]],
    ]);
    const ans = await Modal.choice("Entretien d'embauche", `<b>${j.t}</b><br>Le recruteur vous demande : « ${Q[0]} »`, shuffle(Q[1].slice()).map(a => ({ label: a[0], value: a })), "🤝");
    if (!ans) return;
    const p = clamp(45 + ans[1] * 18 + (L.st.smarts - 50) * .4 + (L.st.looks - 50) * .25 + (L.karma - 50) * .1 - L.jobs.length, 5, 95) / 100;
    if (chance(p)) {
      if (L.job) L.jobs.push(L.job.t);
      if (L.fameJob) L.fameJob = null;
      const sal = Math.round(j.sal * this.C().col * rf(.92, 1.1));
      L.job = { t: j.car ? j.car[0] : j.t, base: j.t, sal, lvl: 0, car: j.car || null, perf: 55, yrs: 0, cat: j.cat, pt: j.pt || 0 };
      L.retired = false;
      this.say(`💼 J'ai été embauché${this.he()} comme ${L.job.t} ! Salaire : ${this.m(sal)}/an.`, "good"); this.add("happy", 10); Sound.good();
    } else { this.say(`Ma candidature pour le poste de ${j.t} a été refusée.`, "bad"); this.add("happy", -4); Sound.bad(); }
    this.save(); this.renderLog();
  },

  // ---------------- Occupation (menu) ----------------
  occupation() {
    const L = this.L; if (!L.alive) return;
    if (L.prison) return this.prisonMenu();
    const items = [];
    const it = (ic, t, s, a, dis) => items.push(`<button class="item ${dis ? "dis" : ""}" data-a="${a}"><div class="ic">${ic}</div><div class="tx"><b>${t}</b>${s ? `<small>${s}</small>` : ""}</div></button>`);
    if (L.age < 4) { Modal.alert("Trop jeune", "Vous êtes encore un bébé ! Profitez-en. 🍼", "👶"); return; }
    if (L.school && L.age < 18) {
      items.push(`<div class="sec">${this.occupationLabel()} · notes ${Math.round(L.grades)}/100</div>`);
      it("📚", "Étudier plus dur", "Améliore vos notes", "study"); it("🙋", "Parler au professeur", "", "teacher");
      it("🤝", "Se faire des amis", "", "friends"); it("⚽", "Activités extrascolaires", "Club de sport, théâtre, musique…", "clubs");
      it("🏃", "Sécher les cours", "", "skip"); if (L.age >= 16) it("🚪", "Abandonner l'école", "Déconseillé…", "dropout");
    }
    if (L.uni) {
      const M = LD.majors.find(x => x.id === L.uni.maj);
      items.push(`<div class="sec">${M.n} · année ${L.uni.year + 1}/${M.y} · notes ${Math.round(L.grades)}</div>`);
      it("📚", "Étudier plus dur", "", "study"); it("🎉", "Faire la fête étudiante", "", "party"); it("🚪", "Abandonner les études", "", "quituni");
    }
    if (L.job) {
      items.push(`<div class="sec">${L.job.t} · ${this.m(L.job.sal)}/an · performance ${Math.round(L.job.perf)}%</div>`);
      it("💪", "Travailler plus dur", "", "hard"); it("💸", "Demander une augmentation", "", "raise");
      if (L.job.car && L.job.lvl < L.job.car.length - 1) it("📈", "Demander une promotion", `Prochain poste : ${L.job.car[L.job.lvl + 1]}`, "promo");
      it("☕", "Socialiser avec les collègues", "", "cowork"); it("😴", "Tirer au flanc", "", "slack"); it("🚪", "Démissionner", "", "quit");
    }
    if (L.fameJob) {
      const F = LD.fameCareers.find(x => x.id === L.fameJob.id);
      items.push(`<div class="sec">${F.t} · ${F.lv[L.fameJob.lvl]} · progression ${Math.round(L.fameJob.prog)}%</div>`);
      it(F.ic, "Travailler mon art", "Fait progresser votre carrière", "art"); it("📣", "Faire de la promotion", "Gagner en célébrité", "promoF"); it("🚪", "Arrêter cette carrière", "", "quitF");
    }
    if (L.pol) {
      items.push(`<div class="sec">${LD.politics[L.pol.lvl]} · soutien ${Math.round(L.pol.sup)}%</div>`);
      it("🗣️", "Faire campagne / meetings", "", "campaign"); it("📜", "Proposer une loi", "", "law");
      if (L.pol.lvl < LD.politics.length - 1) it("🗳️", `Se présenter : ${LD.politics[L.pol.lvl + 1]}`, "Élection", "run"); it("🚪", "Quitter la politique", "", "quitP");
    }
    if (L.age >= 14) {
      items.push(`<div class="sec">Emploi</div>`);
      it("📋", "Offres d'emploi", L.age < 18 ? "Petits boulots" : "Postes disponibles cette année", "offers");
      if (L.age >= 16) it("🌟", "Carrières spéciales", "Acteur, musicien, footballeur, écrivain, influenceur", "fame");
      if (L.age >= 18) it("🏛️", "Politique", L.pol ? "" : "Se présenter au conseil municipal", "politics");
      if (L.age >= 18 && !L.uni) it("🎓", "Reprendre des études", L.degrees.includes("bac") ? "Université et grandes écoles" : (L.flags.retakeBac || L.flags.dropout ? "Repasser le bac en candidat libre" : ""), "uni");
      if (L.age >= 50 && !L.retired) it("🏖️", "Prendre ma retraite", "", "retire");
    }
    const m = Modal.open({ icon: "💼", title: "Occupation", sub: this.occupationLabel(), html: `<div class="list" style="padding:0">${items.join("")}</div>` });
    $$(".item", m.el).forEach(b => b.onclick = () => { m.close(); this.occAction(b.dataset.a); });
  },
  once(k) { const f = this.L.flags.yearActs = this.L.flags.yearActs || {}; if (f[k]) { toast("Déjà fait cette année"); return false; } f[k] = 1; return true; },
  async occAction(a) {
    const L = this.L;
    const done = () => { this.save(); this.renderLog(); };
    switch (a) {
      case "study": if (!this.once("study")) return; L.grades = clamp(L.grades + rnd(3, 9)); this.add("smarts", rnd(1, 3)); this.add("happy", -2); this.say("J'ai étudié plus dur."); break;
      case "teacher": { if (!this.once("teacher")) return; const r = rnd(0, 2); this.say(["Mon professeur m'a encouragé" + this.he() + " à continuer comme ça.", "Mon professeur m'a conseillé de mieux m'organiser.", "Le prof m'a ignoré" + this.he() + "…"][r]); if (r === 0) L.grades = clamp(L.grades + 3); break; }
      case "friends": { if (!this.once("friends")) return; if (chance(.6 + (L.st.looks - 50) / 200)) { const f = this.person(chance(.5) ? "M" : "F", L.age + rnd(-1, 1), pick(LD.lastNames), { t: "ami", r: rnd(50, 80) }); L.rel.push(f); this.say(`Je me suis fait un nouvel ami : ${f.n} ! 🤝`, "good"); this.add("happy", 5); } else this.say("Personne n'a voulu traîner avec moi. 😕", "bad"); break; }
      case "clubs": {
        const c = await Modal.choice("Activités extrascolaires", "", [{ label: "⚽ Football", value: "foot" }, { label: "🏀 Basket", value: "basket" }, { label: "🎭 Théâtre", value: "theatre" }, { label: "🎸 Musique", value: "musique" }, { label: "♟️ Échecs", value: "echecs" }, { label: "🏊 Natation", value: "natation" }], "⚽", true);
        if (!c || !this.once("club")) return;
        if (["foot", "basket", "natation"].includes(c)) { L.sport = clamp(L.sport + rnd(4, 10)); this.add("health", 3); }
        if (c === "theatre") { L.talent = clamp(L.talent + rnd(3, 8)); this.add("looks", 1); }
        if (c === "musique") L.talent = clamp(L.talent + rnd(4, 10));
        if (c === "echecs") this.add("smarts", rnd(2, 4));
        this.add("happy", 4); this.say(`Je me suis inscrit${this.he()} à l'activité : ${c}.`); break;
      }
      case "skip": if (!this.once("skip")) return; L.grades = clamp(L.grades - rnd(3, 8)); this.add("happy", 5); this.say("J'ai séché les cours avec des amis. 😎"); if (chance(.3)) { this.say("Mes parents l'ont appris… Gros savon.", "bad"); this.rels(["pere", "mere"]).forEach(p => p.r = clamp(p.r - 10)); } break;
      case "dropout": if (await Modal.confirm("Abandonner l'école ?", "Sans diplôme, trouver un bon emploi sera difficile.")) { L.school = null; L.flags.dropout = 1; this.say("J'ai abandonné l'école.", "bad"); } break;
      case "party": if (!this.once("party")) return; this.add("happy", 8); L.grades = clamp(L.grades - 3); L.stats.parties++; this.say("Soirée étudiante mémorable ! 🍻"); break;
      case "quituni": if (await Modal.confirm("Abandonner ?", "Vous quitterez vos études sans diplôme.")) { this.say("J'ai abandonné mes études.", "bad"); L.uni = null; } break;
      case "hard": if (!this.once("hard")) return; L.job.perf = clamp(L.job.perf + rnd(5, 12)); this.add("happy", -3); this.add("health", -1); this.say("J'ai travaillé plus dur que jamais."); break;
      case "slack": if (!this.once("slack")) return; L.job.perf = clamp(L.job.perf - rnd(5, 12)); this.add("happy", 4); this.say("J'ai passé l'année à éviter le travail. 😴"); break;
      case "cowork": { if (!this.once("cowork")) return; const c = this.person(chance(.5) ? "M" : "F", L.age + rnd(-8, 8), pick(LD.lastNames), { t: "collegue", r: rnd(30, 70) }); L.rel.push(c); this.say(`J'ai sympathisé avec ${c.n}, un${c.g === "F" ? "e" : ""} collègue.`); this.add("happy", 3); break; }
      case "raise": {
        if (!this.once("raise")) return;
        if (L.job.perf > 60 && chance(.5)) { const r = Math.round(L.job.sal * rf(.04, .12)); L.job.sal += r; this.say(`💸 Augmentation obtenue : +${this.m(r)}/an !`, "good"); Sound.coin(); }
        else { this.say("Mon patron a refusé mon augmentation.", "bad"); if (chance(.15)) { this.say("Il l'a même très mal pris… Je suis viré" + this.he() + " !", "bad"); L.jobs.push(L.job.t); L.job = null; } }
        break;
      }
      case "promo": if (!this.once("promo")) return; if (L.job.perf > 65 && L.job.yrs >= 1 && chance(.55)) this.promote(); else this.say("Promotion refusée : il faut encore faire mes preuves.", "bad"); break;
      case "quit": if (await Modal.confirm("Démissionner ?", `Quitter votre poste de ${L.job.t} ?`)) { this.say(`J'ai démissionné de mon poste de ${L.job.t}.`); L.jobs.push(L.job.t); L.job = null; } break;
      case "offers": return this.offersMenu();
      case "fame": return this.fameMenu();
      case "politics": return this.politicsStart();
      case "uni":
        if (L.degrees.includes("bac")) return this.chooseUni();
        if (!this.once("bac")) return;
        if (chance(clamp(L.st.smarts * .8 + 10) / 100)) { L.degrees.push("bac"); L.flags.dropout = 0; this.say("🎓 J'ai obtenu le bac en candidat libre !", "good"); } else this.say("J'ai encore échoué au bac…", "bad");
        break;
      case "retire": if (await Modal.confirm("Retraite", "Prendre votre retraite maintenant ?")) this.retire(); break;
      case "art": if (!this.once("art")) return; this.fameWork(); break;
      case "promoF": if (!this.once("promoF")) return; L.fame = clamp(L.fame + rnd(2, 8)); L.followers += rnd(500, 5000) * (1 + L.fameJob.lvl); this.say("J'ai fait la promotion de mon travail dans les médias. 📣"); break;
      case "quitF": if (await Modal.confirm("Arrêter ?", "Mettre fin à votre carrière ?")) { this.say("J'ai mis fin à ma carrière artistique/sportive."); L.fameJob = null; } break;
      case "campaign": if (!this.once("camp")) return; L.pol.sup = clamp(L.pol.sup + rnd(3, 10)); L.money -= 2000 * (L.pol.lvl + 1); this.say("J'ai enchaîné les meetings et serré des milliers de mains. 🗣️"); break;
      case "law": {
        if (!this.once("law")) return;
        const law = pick(["la gratuité des transports", "la baisse des impôts", "la protection des animaux", "la semaine de 4 jours", "la légalisation du cannabis", "l'interdiction des portables à l'école", "un revenu universel", "le vote à 16 ans"]);
        if (chance(.45 + L.pol.lvl * .05)) { this.say(`📜 Ma proposition sur ${law} a été adoptée !`, "good"); L.pol.sup = clamp(L.pol.sup + rnd(2, 8)); L.fame = clamp(L.fame + 3); }
        else { this.say(`Ma proposition sur ${law} a été rejetée.`, "bad"); L.pol.sup = clamp(L.pol.sup - rnd(1, 5)); }
        break;
      }
      case "run": return this.runElection(L.pol.lvl + 1);
      case "quitP": if (await Modal.confirm("Quitter la politique ?", "")) { this.say("J'ai quitté la vie politique."); L.pol = null; } break;
    }
    done();
  },
  offersMenu() {
    const L = this.L;
    const offers = this.jobOffers();
    const html = offers.map((j, i) => { const why = this.reqOk(j); return `<button class="item ${why.length ? "dis" : ""}" data-i="${i}"><div class="ic">💼</div><div class="tx"><b>${j.car ? j.car[0] : j.t}</b><small>${j.cat} · ${why.length ? "❌ " + why.join(", ") : "✅ Profil compatible"}</small></div><div class="rt">${this.m(j.sal * this.C().col)}<br>/an</div></button>`; }).join("");
    const m = Modal.open({ icon: "📋", title: "Offres d'emploi", sub: "Nouvelles offres chaque année", html: `<div class="list" style="padding:0">${html}</div>` });
    $$(".item", m.el).forEach(b => b.onclick = () => { m.close(); this.apply(offers[+b.dataset.i]); });
  },
  // carrières célèbres
  fameMenu() {
    const L = this.L;
    const m = Modal.open({ icon: "🌟", title: "Carrières spéciales", html: `<div class="list" style="padding:0">${LD.fameCareers.map((F, i) => `<button class="item" data-i="${i}"><div class="ic">${F.ic}</div><div class="tx"><b>${F.t}</b><small>Audition / premiers pas. Compte : ${F.st === "beaute" ? "apparence" : F.st === "talent" ? "talent" : F.st === "sport" ? "forme" : "intelligence"}</small></div></button>`).join("")}</div>` });
    $$(".item", m.el).forEach(b => b.onclick = async () => {
      m.close(); const F = LD.fameCareers[+b.dataset.i];
      if (!this.once("audition")) return;
      const v = F.st === "beaute" ? L.st.looks : F.st === "talent" ? L.talent : F.st === "sport" ? L.sport : L.st.smarts;
      if (F.id === "foot" && L.age > 26) { this.say("Trop âgé" + this.he() + " pour devenir footballeur pro."); this.save(); this.renderLog(); return; }
      if (chance(clamp(v * .8 + 10) / 100)) {
        if (L.job) { L.jobs.push(L.job.t); L.job = null; }
        L.fameJob = { id: F.id, lvl: 0, prog: 10 }; this.say(`${F.ic} Ma carrière de ${F.t.toLowerCase()} commence : ${F.lv[0]} !`, "good"); this.add("happy", 10);
      } else this.say(`Mon audition pour devenir ${F.t.toLowerCase()} a échoué.`, "bad");
      this.save(); this.renderLog();
    });
  },
  fameWork() {
    const L = this.L, F = LD.fameCareers.find(x => x.id === L.fameJob.id);
    const v = F.st === "beaute" ? L.st.looks : F.st === "talent" ? L.talent : F.st === "sport" ? L.sport : L.st.smarts;
    L.fameJob.prog += rnd(5, 15) + v / 10;
    if (F.st === "talent") L.talent = clamp(L.talent + 2); if (F.st === "sport") L.sport = clamp(L.sport + 3);
    this.say(`J'ai travaillé dur sur ma carrière de ${F.t.toLowerCase()}.`);
  },
  fameYear() {
    const L = this.L, F = LD.fameCareers.find(x => x.id === L.fameJob.id), J = L.fameJob;
    J.prog += rnd(0, 10);
    if (J.prog >= 100 && J.lvl < F.lv.length - 1) {
      J.lvl++; J.prog = 0; L.fame = clamp(L.fame + 12);
      L.followers += Math.round(Math.pow(10, J.lvl + 2) * rf(1, 5));
      this.say(`🌟 Ma carrière décolle : je suis désormais ${F.lv[J.lvl]} !`, "good"); this.add("happy", 15); Sound.good();
      if (F.id === "foot" && J.lvl >= 4) this.say("Je suis sélectionné" + this.he() + " en équipe nationale ! 🏆", "good");
    }
    if (F.id === "foot" && L.age > 34 && chance(.4)) { this.say("⚽ J'ai pris ma retraite sportive.", ""); L.fameJob = null; return; }
    if (chance(.2)) { const ev = pick(["J'ai fait la une des magazines people.", "Une rumeur sur ma vie privée a fait le buzz.", "Les paparazzis me suivent partout.", "J'ai reçu un prix pour mon travail ! 🏆"]); this.say(ev); }
    L.fame = clamp(L.fame + J.lvl);
  },
  fameIncome() { const J = this.L.fameJob; return [4000, 15000, 45000, 150000, 900000, 5000000][J.lvl] * rf(.7, 1.4); },
  // politique
  async politicsStart() {
    const L = this.L;
    if (L.pol) return toast("Vous êtes déjà en politique");
    if (L.record.length) return Modal.alert("Impossible", "Avec un casier judiciaire, aucun parti ne veut de vous.", "🏛️");
    const party = await Modal.choice("Choisir un parti", "Sous quelle étiquette vous présenter ?", [{ label: "🌹 Gauche", value: "Gauche" }, { label: "🕊️ Centre", value: "Centre" }, { label: "🔵 Droite", value: "Droite" }, { label: "🌿 Écologistes", value: "Écologiste" }, { label: "⚪ Sans étiquette", value: "Indépendant" }], "🏛️", true);
    if (!party) return;
    L.pol = { lvl: -1, sup: 30 + rnd(0, 20), party };
    await this.runElection(0);
  },
  async runElection(lvl) {
    const L = this.L;
    if (!this.once("election")) return;
    const cost = [500, 3000, 8000, 20000, 30000, 60000, 100000, 500000][lvl];
    const spend = await Modal.choice(`Élection : ${LD.politics[lvl]}`, `Une campagne coûte au moins ${this.m(cost)}. Combien investir ?`, [
      { label: `Campagne modeste (${this.m(cost)})`, value: 1 }, { label: `Grande campagne (${this.m(cost * 3)})`, value: 3 }, { label: `Campagne massive (${this.m(cost * 8)})`, value: 8 }], "🗳️", true);
    if (!spend) { L.flags.yearActs.election = 0; if (L.pol && L.pol.lvl < 0) L.pol = null; return; }
    L.money -= cost * spend;
    const p = clamp(25 + (L.pol.sup - 40) * .8 + (L.st.smarts - 50) * .25 + (L.st.looks - 50) * .2 + (L.karma - 50) * .2 + spend * 4 + L.fame * .2 - lvl * 4, 4, 92) / 100;
    if (chance(p)) {
      L.pol.lvl = lvl; L.pol.sup = 50 + rnd(0, 15); L.pol.term = 0;
      if (L.job) { L.jobs.push(L.job.t); L.job = null; }
      this.say(`🗳️ Victoire ! Je suis élu${this.he()} ${LD.politics[lvl]} (${L.pol.party}) avec ${Math.round(50 + p * 20)} % des voix !`, "good"); this.add("happy", 20); L.fame = clamp(L.fame + lvl * 6); Sound.good();
      if (lvl === LD.politics.length - 1) this.say("🇫🇷 Je suis à la tête du pays ! Une page d'histoire s'écrit.", "big");
    } else {
      this.say(`Défaite à l'élection : ${LD.politics[lvl]}. 😞`, "bad"); this.add("happy", -12);
      if (L.pol.lvl < 0) L.pol = null;
    }
    this.save(); this.renderLog();
  },
  async polYear() {
    const L = this.L, P = L.pol;
    P.term = (P.term || 0) + 1; P.sup = clamp(P.sup + rnd(-8, 5));
    if (chance(.12)) {
      const r = await Modal.choice("Proposition douteuse", "Un homme d'affaires vous propose une enveloppe de 50 000 en échange d'un marché public.", [{ label: "💼 Accepter le pot-de-vin", value: 1 }, { label: "🙅 Refuser et le dénoncer", value: 0 }], "💰");
      if (r) { L.money += 50000; L.karma -= 10; if (chance(.35)) { this.say("Le scandale de corruption a éclaté ! J'ai dû démissionner.", "bad"); L.pol = null; await this.trial("Corruption", [1, 5]); return; } this.say("J'ai discrètement empoché l'enveloppe…"); }
      else { L.karma += 5; P.sup = clamp(P.sup + 8); this.say("J'ai refusé un pot-de-vin et dénoncé le corrupteur. 👏", "good"); }
    }
    if (P.term >= 5) {
      P.term = 0;
      if (chance(clamp(P.sup, 10, 90) / 100)) { this.say(`🗳️ J'ai été réélu${this.he()} ${LD.politics[P.lvl]} !`, "good"); P.sup = clamp(P.sup + 5); }
      else { this.say(`J'ai perdu ma réélection comme ${LD.politics[P.lvl]}.`, "bad"); L.pol = null; this.add("happy", -15); }
    }
  },

  // ---------------- Prison & justice ----------------
  async trial(crime, range) {
    const L = this.L;
    let years = rnd(range[0], range[1]);
    const lawyer = await Modal.choice("Procès", `Vous êtes jugé${this.he()} pour : ${crime}. Qui vous défend ?`, [
      { label: "⚖️ Avocat commis d'office", sub: "Gratuit", value: 0 }, { label: "👔 Bon avocat", sub: this.m(15000), value: 1 }, { label: "🦈 Ténor du barreau", sub: this.m(150000), value: 2 }], "⚖️");
    L.money -= [0, 15000, 150000][lawyer || 0];
    const acquit = [.1, .3, .55][lawyer || 0];
    L.record.push(crime);
    if (chance(acquit)) { this.say(`⚖️ J'ai été acquitté${this.he()} pour ${crime.toLowerCase()} ! 🙌`, "good"); return; }
    if (years <= 0) { const amende = rnd(200, 3000); L.money -= amende; this.say(`⚖️ Condamné${this.he()} à ${this.m(amende)} d'amende pour ${crime.toLowerCase()}.`, "bad"); return; }
    L.prison = { left: years, total: years, crime };
    if (L.job) { L.jobs.push(L.job.t); L.job = null; } L.uni = null; L.fameJob = null; L.pol = null;
    this.say(`⛓️ Condamné${this.he()} à ${years} an${years > 1 ? "s" : ""} de prison pour ${crime.toLowerCase()}.`, "bad"); this.add("happy", -30); Sound.bad();
  },
  async prisonYear() {
    const L = this.L;
    L.prison.left--; L.stats.prisonYears++;
    if (L.prison.left <= 0) { this.say("🔓 J'ai purgé ma peine. Je suis enfin libre !", "good"); L.prison = null; this.add("happy", 25); if (L.home !== "proprio") L.home = L.age >= 18 ? "loyer" : "parents"; return; }
    this.say(`Encore ${L.prison.left} an${L.prison.left > 1 ? "s" : ""} derrière les barreaux.`);
    if (chance(.25)) { const ev = pick(["Mon codétenu ronfle comme un tracteur.", "Un gardien m'a confisqué mes biscuits.", "J'ai reçu une lettre de ma famille. 💌", "Bagarre dans la cour de promenade.", "On m'a proposé de rejoindre un gang."]); this.say(ev); }
  },
  prisonMenu() {
    const L = this.L;
    const m = Modal.open({ icon: "⛓️", title: "Prison", sub: `${L.prison.crime} · ${L.prison.left} an(s) restant(s)`, html: `<div class="list" style="padding:0">
      <button class="item" data-a="gym"><div class="ic">🏋️</div><div class="tx"><b>Musculation dans la cour</b></div></button>
      <button class="item" data-a="lib"><div class="ic">📖</div><div class="tx"><b>Bibliothèque de la prison</b></div></button>
      <button class="item" data-a="good"><div class="ic">😇</div><div class="tx"><b>Bonne conduite</b><small>Augmente les chances de libération anticipée</small></div></button>
      <button class="item" data-a="parole"><div class="ic">📝</div><div class="tx"><b>Demander une libération conditionnelle</b></div></button>
      <button class="item" data-a="riot"><div class="ic">🔥</div><div class="tx"><b>Lancer une émeute</b></div></button>
      <button class="item" data-a="escape"><div class="ic">🕳️</div><div class="tx"><b>Tenter une évasion</b><small>Très risqué</small></div></button></div>` });
    $$(".item", m.el).forEach(b => b.onclick = async () => {
      m.close(); const a = b.dataset.a;
      if (!this.once("p" + a)) return;
      if (a === "gym") { L.sport = clamp(L.sport + 6); this.add("health", 3); this.say("Séance de musculation intense. 💪"); }
      if (a === "lib") { this.add("smarts", 4); this.say("J'ai dévoré des livres à la bibliothèque. 📖"); }
      if (a === "good") { L.flags.goodConduct = (L.flags.goodConduct || 0) + 1; L.karma += 2; this.say("Je me tiens à carreau. Les gardiens l'ont remarqué."); }
      if (a === "parole") {
        if (L.prison.left <= L.prison.total / 2 && chance(.3 + (L.flags.goodConduct || 0) * .12)) { this.say("📝 Libération conditionnelle accordée ! 🔓", "good"); L.prison = null; this.add("happy", 20); }
        else this.say("Libération conditionnelle refusée.", "bad");
      }
      if (a === "riot") { if (chance(.3)) { this.say("🔥 L'émeute a mis la prison sens dessus dessous !"); this.add("happy", 5); } else { L.prison.left += rnd(1, 3); this.say("L'émeute a échoué. Ma peine a été allongée.", "bad"); } if (chance(.1)) { this.add("health", -30); this.say("J'ai été gravement blessé" + this.he() + " pendant l'émeute.", "bad"); } }
      if (a === "escape") {
        const ok = await this.escapeGame();
        if (ok) { this.say("🏃 Je me suis évadé" + this.he() + " ! Je suis désormais en cavale.", "good"); L.prison = null; L.flags.fugitive = 1; L.record.push("Évasion"); }
        else { L.prison.left += rnd(2, 5); this.say("Évasion ratée… Les gardiens m'ont rattrapé" + this.he() + ". Peine prolongée.", "bad"); }
      }
      this.save(); this.renderLog();
    });
  },
  escapeGame() {
    // mini-jeu : trouver la bonne sortie dans 3 couloirs successifs
    return new Promise(async res => {
      for (let i = 0; i < 3; i++) {
        const c = await Modal.choice(`Évasion — étape ${i + 1}/3`, ["Vous êtes sorti de votre cellule. Quel couloir prendre ?", "Une ronde approche ! Où vous cacher ?", "Le mur d'enceinte ! Comment le franchir ?"][i],
          [["⬅️ Gauche", "➡️ Droite", "⬆️ Tout droit"], ["🧺 Dans la buanderie", "🍳 Dans les cuisines", "🚽 Dans les toilettes"], ["🪢 Corde de draps", "🚚 Camion de livraison", "🧗 Escalade"]][i].map((l, k) => ({ label: l, value: k })), "🕳️");
        if (c !== rnd(0, 2) && chance(.55)) return res(false);
      }
      res(true);
    });
  },

  // ---------------- Événements aléatoires ----------------
  async randomEvents() {
    const L = this.L;
    const pool = Life.EVENTS.filter(e => { try { return e.c(L, this); } catch (x) { return false; } });
    const n = L.age < 3 ? 1 : rnd(1, 2);
    const used = new Set();
    for (let i = 0; i < n && pool.length; i++) {
      const tot = pool.reduce((s, e) => s + (e.w || 1), 0); let r = Math.random() * tot, ev = pool[0];
      for (const e of pool) { r -= e.w || 1; if (r <= 0) { ev = e; break; } }
      if (used.has(ev.id) || (ev.once && L.flags["ev_" + ev.id])) continue;
      used.add(ev.id); if (ev.once) L.flags["ev_" + ev.id] = 1;
      if (chance(ev.p || .5)) await ev.run(L, this);
      if (!L.alive) return;
    }
  },

  // ---------------- Biens ----------------
  assetsMenu() {
    const L = this.L; if (!L.alive) return;
    const own = L.assets.map((a, i) => `<button class="item" data-i="${i}"><div class="ic">${a.ic}</div><div class="tx"><b>${a.n}</b><small>${a.k === "house" ? (a.rented ? "Mise en location" : "Résidence") : "Véhicule"} · état ${Math.round(a.cond)}%</small></div><div class="rt">${this.m(a.p)}</div></button>`).join("");
    const home = { parents: "Chez mes parents", loyer: `Locataire (${this.m((L.rent || 7000) * this.C().col)}/an)`, proprio: "Propriétaire", sdf: "Sans domicile" }[L.home];
    const m = Modal.open({ icon: "🏠", title: "Biens", sub: `Logement : ${home}`, html: `<div class="list" style="padding:0">
      ${own || `<p class="muted center">Vous ne possédez rien pour l'instant.</p>`}
      <div class="sec">Acheter</div>
      <button class="item" data-a="immo"><div class="ic">🏘️</div><div class="tx"><b>Agence immobilière</b><small>Maisons, appartements, châteaux…</small></div></button>
      <button class="item" data-a="auto"><div class="ic">🚗</div><div class="tx"><b>Concession & luxe</b><small>Voitures, motos, bateaux, jets</small></div></button>
      ${L.age >= 18 && L.home === "parents" ? `<button class="item" data-a="move"><div class="ic">📦</div><div class="tx"><b>Quitter le domicile familial</b><small>Louer un appartement</small></div></button>` : ""}
      ${L.age >= 18 && L.home !== "parents" && L.home !== "proprio" && this.rels(["pere", "mere"]).length ? `<button class="item" data-a="back"><div class="ic">🏡</div><div class="tx"><b>Retourner chez mes parents</b></div></button>` : ""}
      ${L.age >= 18 ? `<button class="item" data-a="invest"><div class="ic">📈</div><div class="tx"><b>Investir en Bourse</b><small>Risque et rendement</small></div></button>` : ""}
      ${L.age >= 18 ? `<button class="item" data-a="loan"><div class="ic">🏦</div><div class="tx"><b>Banque : emprunter</b></div></button>` : ""}</div>` });
    $$(".item", m.el).forEach(b => b.onclick = () => { m.close(); if (b.dataset.i !== undefined) this.assetAction(+b.dataset.i); else this.assetBuy(b.dataset.a); });
  },
  async assetBuy(a) {
    const L = this.L, C = this.C();
    if (L.age < 18 && a !== "move") return Modal.alert("Trop jeune", "Il faut être majeur pour acheter des biens.", "🔞");
    if (a === "immo" || a === "auto") {
      const list = a === "immo" ? LD.houses : LD.cars;
      const opts = list.map((h, i) => { const p = Math.round(h.p * (a === "immo" ? C.col : 1) * rf(.9, 1.1)); return { label: `${h.ic} ${h.n}`, sub: this.m(p) + (L.money < p ? (a === "immo" && L.money >= p * .2 ? " · crédit possible" : " · trop cher") : ""), value: [i, p] }; });
      const r = await Modal.choice(a === "immo" ? "Agence immobilière" : "Concession", `Votre budget : ${this.m(L.money)}`, opts, a === "immo" ? "🏘️" : "🚗", true);
      if (!r) return;
      const [i, p] = r, h = list[i];
      if (a === "auto" && (h.ic === "🚗" || h.ic === "🏎️" || h.ic === "🚙" || h.ic === "🚘") && !L.license) toast("Sans permis, vous ne pourrez pas la conduire…");
      if (L.money >= p) { L.money -= p; }
      else if (a === "immo" && L.money >= p * .2 && (L.job || L.fameJob) && await Modal.confirm("Crédit immobilier", `Apport de ${this.m(p * .2)}, le reste emprunté (${this.m(p * .8)}) sera déduit progressivement.`)) { L.money -= p * .2; L.debt += p * .8 * 1.25; }
      else return Modal.alert("Fonds insuffisants", "Vous n'avez pas assez d'argent.", "💸");
      L.assets.push({ k: a === "immo" ? "house" : "car", n: h.n, ic: h.ic, p, cond: 100, y: L.age });
      if (a === "immo" && L.home !== "proprio") { L.home = "proprio"; }
      this.say(`🔑 J'ai acheté : ${h.n} pour ${this.m(p)} !`, "good"); this.add("happy", 10); Sound.coin();
    }
    if (a === "move") {
      const opts = LD.houses.filter(h => h.rent).slice(0, 6).map(h => ({ label: `${h.ic} Louer : ${h.n}`, sub: this.m(h.rent * C.col) + "/an", value: h.rent }));
      const r = await Modal.choice("Location", "", opts, "📦", true); if (!r) return;
      L.home = "loyer"; L.rent = r; this.say("📦 J'ai quitté le nid familial et emménagé dans mon propre logement !", "good"); this.add("happy", 12);
    }
    if (a === "back") { L.home = "parents"; this.say("Je suis retourné" + this.he() + " vivre chez mes parents."); this.add("happy", -5); }
    if (a === "invest") {
      if (L.money < 100) return Modal.alert("Fonds insuffisants", "Il faut au moins un peu d'argent pour investir.", "📈");
      const amt = await Modal.slider("Bourse", "Montant à investir (résultat immédiat sur l'année) :", 1, Math.max(1, Math.floor(L.money / 100)), Math.max(1, Math.floor(L.money / 1000)), v => this.m(v * 100), "Investir", "📈");
      if (!amt || !this.once("invest")) return;
      const v = amt * 100, r = rf(-.35, .45);
      L.money += Math.round(v * r);
      this.say(r >= 0 ? `📈 Mon investissement de ${this.m(v)} a rapporté ${this.m(v * r)} !` : `📉 J'ai perdu ${this.m(-v * r)} en Bourse…`, r >= 0 ? "good" : "bad");
    }
    if (a === "loan") {
      const max = Math.round(((L.job ? L.job.sal : 0) + (L.fameJob ? 50000 : 0)) * 2);
      if (max <= 0) return Modal.alert("Refusé", "Sans revenus, la banque refuse de vous prêter de l'argent.", "🏦");
      const amt = await Modal.slider("Emprunt", `Montant maximal : ${this.m(max)} (remboursé avec 20 % d'intérêts)`, 1, Math.floor(max / 1000), Math.floor(max / 2000), v => this.m(v * 1000), "Emprunter", "🏦");
      if (!amt) return; L.money += amt * 1000; L.debt += amt * 1200; this.say(`🏦 J'ai emprunté ${this.m(amt * 1000)}.`);
    }
    this.save(); this.renderLog();
  },
  async assetAction(i) {
    const L = this.L, a = L.assets[i];
    const opts = [{ label: `💰 Vendre (${this.m(a.p * .95)})`, value: "sell" }, { label: "🔧 Entretenir / rénover", sub: this.m(a.p * .03), value: "fix" }];
    if (a.k === "house") opts.push({ label: a.rented ? "🏠 Arrêter la location" : "🗝️ Mettre en location", value: "rent" });
    if (a.k === "car") opts.push({ label: "🛣️ Faire un tour", value: "drive" });
    const r = await Modal.choice(a.n, `Valeur estimée : ${this.m(a.p)}`, opts, a.ic, true);
    if (r === "sell") { L.money += Math.round(a.p * .95); L.assets.splice(i, 1); this.say(`J'ai vendu : ${a.n} pour ${this.m(a.p * .95)}.`); if (a.k === "house" && !L.assets.some(x => x.k === "house")) { L.home = "loyer"; L.rent = 7000; } Sound.coin(); }
    if (r === "fix") { L.money -= Math.round(a.p * .03); a.cond = 100; a.p = Math.round(a.p * 1.04); this.say(`J'ai rénové : ${a.n}.`); }
    if (r === "rent") { a.rented = !a.rented; this.say(a.rented ? `J'ai mis en location : ${a.n}.` : `J'ai repris : ${a.n}.`); }
    if (r === "drive") {
      if (!L.license) { if (chance(.4)) { this.say("🚓 Contrôlé" + this.he() + " sans permis ! Grosse amende.", "bad"); L.money -= 3750; } else this.say("J'ai conduit sans permis… et je ne me suis pas fait prendre."); }
      else if (chance(.04)) { this.say("💥 Accident de voiture ! Je suis blessé" + this.he() + ".", "bad"); this.add("health", -rnd(10, 40)); a.cond = 20; a.p = Math.round(a.p * .5); }
      else { this.say("J'ai fait une belle balade en voiture. 🛣️"); this.add("happy", 5); }
    }
    this.save(); this.renderLog();
  },

  // ---------------- Relations ----------------
  relLabel(r) {
    const f = r.g === "F";
    return { pere: "Père", mere: "Mère", frere: "Frère", soeur: "Sœur", ami: f ? "Amie" : "Ami", partenaire: f ? "Petite amie" : "Petit ami", fiance: f ? "Fiancée" : "Fiancé",
      conjoint: f ? "Épouse" : "Époux", enfant: f ? "Fille" : "Fils", ex: "Ex", animal: r.kind || "Animal", collegue: f ? "Collègue" : "Collègue", defunt: f ? "Défunte épouse" : "Défunt époux",
      demifrere: "Demi-frère", ennemi: "Ennemi" }[r.t] || r.t;
  },
  relationsMenu() {
    const L = this.L; if (!L.alive) return;
    const order = ["partenaire", "fiance", "conjoint", "pere", "mere", "frere", "soeur", "enfant", "ami", "collegue", "animal", "ex"];
    const list = L.rel.filter(r => r.alive && order.includes(r.t)).sort((a, b) => order.indexOf(a.t) - order.indexOf(b.t));
    const html = list.map(r => `<button class="item" data-id="${r.id}"><div class="ic">${r.t === "animal" ? r.ic : r.g === "F" ? (r.age < 13 ? "👧" : r.age > 60 ? "👵" : "👩") : (r.age < 13 ? "👦" : r.age > 60 ? "👴" : "👨")}</div>
      <div class="tx"><b>${esc(r.n)}</b><small>${this.relLabel(r)} · ${r.age} an${r.age > 1 ? "s" : ""}</small><div class="bar" style="margin-top:5px;height:6px"><i style="width:${r.r}%;background:${r.r > 60 ? "#35d07f" : r.r > 30 ? "#ffc926" : "#ff4d5e"}"></i></div></div></button>`).join("");
    const m = Modal.open({ icon: "❤️", title: "Relations", sub: `${list.length} personne(s)`, html: `<div class="list" style="padding:0">${html || `<p class="muted center">Vous êtes seul${this.he()} au monde.</p>`}</div>` });
    $$(".item", m.el).forEach(b => b.onclick = () => { m.close(); this.relActions(L.rel.find(r => r.id === b.dataset.id)); });
  },
  relActions(r) {
    const L = this.L;
    const A = [];
    const add = (ic, t, a) => A.push({ label: `${ic} ${t}`, value: a });
    if (r.t === "animal") { add("🦴", "Jouer avec", "play"); add("🐾", "Promener / câliner", "walk"); add("🩺", "Chez le vétérinaire", "vet"); add("💔", "Abandonner", "abandon"); }
    else {
      add("⏳", "Passer du temps ensemble", "time"); add("💬", "Discuter", "talk"); add("😊", "Complimenter", "compliment"); add("🎁", "Offrir un cadeau", "gift");
      if (["pere", "mere"].includes(r.t) && L.age >= 5) add("💶", "Demander de l'argent", "money");
      if (["partenaire", "fiance", "conjoint"].includes(r.t)) { add("💋", "Sortie en amoureux", "date"); if (r.t === "partenaire" && L.age >= 18) add("💍", "Demander en mariage", "propose"); if (r.t === "fiance") add("💒", "Organiser le mariage", "wed"); if (L.age >= 18 && L.age <= 50) add("👶", "Essayer d'avoir un enfant", "baby"); add(r.t === "conjoint" ? "📄" : "💔", r.t === "conjoint" ? "Divorcer" : "Rompre", "breakup"); }
      if (r.t === "enfant" && r.age >= 18) add("💶", "Demander de l'argent", "money");
      if (r.t === "enfant" && r.age < 18) add("📚", "Aider aux devoirs", "homework");
      if (["ami", "collegue"].includes(r.t) && L.age >= 15 && !this.partner()) add("💘", "Proposer un rendez-vous", "ask");
      add("😜", "Faire une blague", "prank"); add("😤", "Se disputer", "argue"); add("🤬", "Insulter", "insult");
      if (L.age >= 14) add("👊", "Agresser", "attack");
      if (["ami", "collegue", "ex"].includes(r.t)) add("🚫", "Couper les ponts", "unfriend");
    }
    Modal.choice(r.n, `${this.relLabel(r)} · ${r.age} ans · relation ${Math.round(r.r)}%${r.job ? "<br>" + esc(r.job) : ""}`, A, r.t === "animal" ? r.ic : "❤️", true).then(a => { if (a) this.relDo(r, a); });
  },
  async relDo(r, a) {
    const L = this.L, nm = r.n.split(" ")[0];
    if (!this.once(r.id + a)) return;
    const up = (v, t, c = "") => { r.r = clamp(r.r + v); if (t) this.say(t, c); };
    switch (a) {
      case "play": case "walk": up(8, `J'ai passé un bon moment avec ${nm}. ${r.ic}`); this.add("happy", 5); break;
      case "vet": L.money -= 150; up(5, `J'ai emmené ${nm} chez le vétérinaire.`); r.life = (r.life || 10) + (chance(.3) ? 1 : 0); break;
      case "abandon": r.alive = false; L.karma -= 10; this.say(`J'ai abandonné ${nm}… 😢`, "bad"); break;
      case "time": up(rnd(4, 12), `J'ai passé du temps avec ${nm}.`); this.add("happy", 3); break;
      case "talk": { const topics = ["la météo", "le sens de la vie", "le foot", "la politique", "leurs souvenirs d'enfance", "les séries du moment", "l'avenir"]; up(rnd(2, 8), `J'ai discuté de ${pick(topics)} avec ${nm}.`); break; }
      case "compliment": up(rnd(3, 9), `J'ai complimenté ${nm}, ${r.g === "F" ? "elle" : "il"} a souri.`); break;
      case "gift": { const g = await Modal.choice("Cadeau", "", [{ label: "🌹 Fleurs (30)", value: [30, 6] }, { label: "🍫 Chocolats (20)", value: [20, 5] }, { label: "📱 Smartphone (900)", value: [900, 15] }, { label: "⌚ Montre de luxe (8 000)", value: [8000, 25] }, { label: "🚗 Une voiture (25 000)", value: [25000, 40] }], "🎁", true); if (!g) { L.flags.yearActs[r.id + a] = 0; return; } if (L.money < g[0]) return Modal.alert("Pas assez d'argent", "", "💸"); L.money -= g[0]; up(g[1], `J'ai offert un cadeau à ${nm}. ${r.g === "F" ? "Elle" : "Il"} est ravi${r.g === "F" ? "e" : ""} !`, "good"); break; }
      case "money": {
        const rich = r.money || 0;
        if (r.r > 50 && rich > 200 && chance(.55)) { const v = Math.round(Math.min(rich * rf(.01, .1), L.age < 18 ? 200 : 20000)); r.money -= v; L.money += v; this.say(`${nm} m'a donné ${this.m(v)}. 💶`, "good"); Sound.coin(); }
        else up(-5, `${nm} a refusé de me donner de l'argent.`, "bad"); break;
      }
      case "homework": up(6, `J'ai aidé ${nm} à faire ses devoirs.`); break;
      case "date": L.money -= 80; up(rnd(5, 12), `Soirée romantique avec ${nm}. 💕`, "good"); this.add("happy", 6); break;
      case "propose": {
        const ring = await Modal.choice("Bague", "Quelle bague ?", [{ label: "💍 Bague simple (500)", value: [500, 0] }, { label: "💍 Bague en or (3 000)", value: [3000, .1] }, { label: "💎 Diamant (15 000)", value: [15000, .2] }], "💍", true);
        if (!ring) { L.flags.yearActs[r.id + a] = 0; return; }
        L.money -= ring[0];
        if (chance(clamp(r.r / 100 + ring[1] - .15, .05, .95))) { r.t = "fiance"; this.say(`💍 ${nm} a dit OUI ! Nous sommes fiancés !`, "good"); this.add("happy", 20); Sound.good(); }
        else { up(-15, `💔 ${nm} a refusé ma demande en mariage…`, "bad"); this.add("happy", -15); }
        break;
      }
      case "wed": {
        const w = await Modal.choice("Mariage", "Quel mariage ?", [{ label: "🏛️ À la mairie, en petit comité (1 500)", value: 1500 }, { label: "💒 Mariage classique (15 000)", value: 15000 }, { label: "🏰 Mariage de rêve dans un château (80 000)", value: 80000 }], "💒", true);
        if (!w) { L.flags.yearActs[r.id + a] = 0; return; }
        L.money -= w; r.t = "conjoint"; r.r = clamp(r.r + 15); this.add("happy", 25);
        this.say(`💒 Je me suis marié${this.he()} avec ${r.n} ! Plus beau jour de ma vie.`, "good"); Sound.good(); break;
      }
      case "baby": {
        if (L.pregnant) { this.say("Un bébé est déjà en route ! 🤰"); break; }
        const sameSex = r.g === L.g;
        if (sameSex) { const ad = await Modal.confirm("Adoption", "Voulez-vous adopter un enfant ? (frais : 8 000)"); if (ad) { L.money -= 8000; const g = chance(.5) ? "M" : "F"; const k = this.person(g, rnd(0, 6), L.last, { t: "enfant", r: 80 }); L.rel.push(k); L.stats.kids++; this.say(`👶 Nous avons adopté ${k.n.split(" ")[0]} (${k.age} ans) !`, "good"); this.add("happy", 15); } break; }
        const fert = L.g === "F" ? (L.age < 35 ? .55 : L.age < 42 ? .3 : .08) : (r.age < 35 ? .55 : r.age < 42 ? .3 : .08);
        if (chance(fert)) { L.pregnant = 1; this.say(`🤰 ${L.g === "F" ? "Je suis enceinte" : nm + " est enceinte"} ! Le bébé arrive l'année prochaine.`, "good"); this.add("happy", 10); }
        else this.say("Pas de bébé pour l'instant…"); break;
      }
      case "breakup": {
        if (!await Modal.confirm(r.t === "conjoint" ? "Divorcer ?" : "Rompre ?", `Mettre fin à votre relation avec ${nm} ?`)) { L.flags.yearActs[r.id + a] = 0; return; }
        if (r.t === "conjoint") { const half = Math.round(Math.max(0, L.money) * .4); L.money -= half + 5000; this.say(`📄 Divorce prononcé avec ${nm}. Il m'a coûté ${this.m(half + 5000)}.`, "bad"); }
        else this.say(`💔 J'ai rompu avec ${nm}.`, "bad");
        r.t = "ex"; r.r = 10; this.add("happy", -10); break;
      }
      case "ask": {
        if (chance(clamp((r.r + L.st.looks) / 200, .05, .9))) { r.t = "partenaire"; L.stats.partners++; this.say(`💘 ${nm} a accepté de sortir avec moi !`, "good"); this.add("happy", 12); }
        else up(-10, `${nm} m'a mis un râteau. 😬`, "bad"); break;
      }
      case "prank": if (chance(.5)) up(5, `${nm} a ri de ma blague. 😂`); else up(-8, `${nm} n'a pas du tout apprécié ma blague.`, "bad"); break;
      case "argue": up(-rnd(8, 18), `Grosse dispute avec ${nm}.`, "bad"); this.add("happy", -4); break;
      case "insult": up(-rnd(15, 30), `J'ai insulté ${nm}. ${r.g === "F" ? "Elle" : "Il"} est furieux${r.g === "F" ? "e" : ""}.`, "bad"); L.karma -= 3; break;
      case "attack": {
        L.karma -= 8; up(-50, `👊 J'ai agressé ${nm} !`, "bad");
        if (chance(.4)) { this.add("health", -rnd(5, 25)); this.say(`${nm} s'est défendu${r.g === "F" ? "e" : ""} et m'a blessé${this.he()}.`, "bad"); }
        if (L.age >= 13 && chance(.35)) { L.stats.crimes++; await this.trial("Coups et blessures", [0, 3]); }
        break;
      }
      case "unfriend": L.rel = L.rel.filter(x => x !== r); this.say(`J'ai coupé les ponts avec ${nm}.`); break;
    }
    this.save(); this.renderLog();
  },

  // ---------------- Activités ----------------
  activities() {
    const L = this.L; if (!L.alive) return;
    if (L.prison) return this.prisonMenu();
    const a = L.age;
    const S = [];
    const it = (ic, t, s, id, ok = true) => { if (ok) S.push(`<button class="item" data-a="${id}"><div class="ic">${ic}</div><div class="tx"><b>${t}</b>${s ? `<small>${s}</small>` : ""}</div></button>`); };
    const sec = t => S.push(`<div class="sec">${t}</div>`);
    if (a < 4) { it("🧸", "Jouer avec mes jouets", "", "toys"); it("😭", "Faire un caprice", "", "tantrum"); it("🍼", "Réclamer un câlin", "", "hug"); }
    else {
      sec("Corps & esprit");
      it("🏋️", "Salle de sport", "Santé, apparence, forme", "gym", a >= 12); it("🏃", "Jogging", "", "jog"); it("📖", "Bibliothèque", "Intelligence", "lib");
      it("🧘", "Méditation & yoga", "Bonheur", "medit", a >= 10); it("🥗", "Faire un régime", "", "diet", a >= 14);
      sec("Santé");
      it("🩺", "Médecin", L.dis.length ? `Soigner : ${L.dis.map(d => d.n).join(", ")}` : "Bilan de santé", "doctor");
      it("🛋️", "Psychologue", "", "psy", a >= 10); it("💉", "Chirurgie esthétique", "Apparence (risqué)", "surgery", a >= 18); it("🌿", "Médecine alternative", "", "alt", a >= 16);
      sec("Loisirs");
      it("✈️", "Vacances", "", "vacation", a >= 18); it("🪩", "Boîte de nuit", "", "club", a >= 18); it("🎟️", "Concert / cinéma / théâtre", "", "show", a >= 8);
      it("🎰", "Casino", "Blackjack & roulette", "casino", a >= 18); it("🎫", "Loterie", "Ticket : 2", "lotto", a >= 18);
      it("🐾", "Refuge & animalerie", "Adopter un animal", "pet", a >= 8); it("📱", "Réseaux sociaux", `${fmtNum(L.followers)} abonnés`, "social", a >= 13);
      sec("Amour");
      it("💘", "Rencontres", "Trouver l'amour", "dating", a >= 14); it("📲", "Appli de rencontre", "", "app", a >= 18);
      sec("Démarches");
      it("🚗", "Permis de conduire", L.license ? "Obtenu ✅" : "Passer l'examen", "license", a >= this.C().drive && !L.license);
      it("🌍", "Émigrer", "Changer de pays", "emigrate", a >= 18); it("✏️", "Changer de nom", "", "rename", a >= 18);
      it("💝", "Faire un don", "Karma", "donate", a >= 12); it("🎓", "Cours du soir", "Intelligence", "night", a >= 18);
      sec("Crimes"); LD.crimes.forEach(c => it(c.ic, c.n, "", "crime:" + c.id, a >= c.min && (!c.job || L.job)));
    }
    const m = Modal.open({ icon: "🎯", title: "Activités", html: `<div class="list" style="padding:0">${S.join("")}</div>` });
    $$(".item", m.el).forEach(b => b.onclick = () => { m.close(); this.doActivity(b.dataset.a); });
  },
  async doActivity(id) {
    const L = this.L, C = this.C();
    const done = () => { this.save(); this.renderLog(); };
    if (id.startsWith("crime:")) { await this.crime(LD.crimes.find(c => c.id === id.slice(6))); return done(); }
    const noRepeat = ["gym", "jog", "lib", "medit", "diet", "psy", "show", "club", "night", "toys", "tantrum", "hug", "alt"];
    if (noRepeat.includes(id) && !this.once(id)) return;
    switch (id) {
      case "toys": this.add("happy", 6); this.say("J'ai joué avec mes jouets. 🧸"); break;
      case "tantrum": this.add("happy", 2); this.rels(["pere", "mere"]).forEach(p => p.r = clamp(p.r - 4)); this.say("J'ai fait un énorme caprice au supermarché. 😭"); break;
      case "hug": this.add("happy", 8); this.rels(["pere", "mere"]).forEach(p => p.r = clamp(p.r + 4)); this.say("Câlin avec mes parents. 🤗"); break;
      case "gym": L.sport = clamp(L.sport + rnd(4, 9)); this.add("health", rnd(2, 5)); this.add("looks", rnd(1, 3)); this.add("happy", 2); L.stats.gym++; this.say("J'ai soulevé de la fonte à la salle. 🏋️"); if (chance(.03)) { this.add("health", -10); this.say("Je me suis fait une élongation.", "bad"); } break;
      case "jog": L.sport = clamp(L.sport + rnd(3, 6)); this.add("health", rnd(1, 4)); this.add("happy", 2); this.say("Petit footing matinal. 🏃"); break;
      case "lib": this.add("smarts", rnd(2, 5)); this.say(`J'ai lu ${pick(["un roman de Victor Hugo", "un livre d'histoire", "une BD", "un traité de philosophie", "un manga", "un livre de développement personnel", "un polar"])}. 📖`); break;
      case "medit": this.add("happy", rnd(4, 9)); this.add("health", 1); this.say("Séance de méditation : je me sens apaisé" + this.he() + ". 🧘"); break;
      case "diet": { const d = await Modal.choice("Régime", "", [{ label: "🥗 Végétarien", value: 1 }, { label: "🥩 Protéiné", value: 2 }, { label: "🍩 Junk food", value: 3 }, { label: "🍽️ Jeûne intermittent", value: 4 }], "🥗", true); if (!d) { L.flags.yearActs.diet = 0; return; } if (d === 3) { this.add("health", -5); this.add("happy", 6); this.say("J'ai mangé n'importe quoi toute l'année. 🍩"); } else { this.add("health", rnd(2, 6)); this.add("looks", 2); this.say("J'ai suivi un régime sain."); } break; }
      case "doctor": {
        if (!L.dis.length) { L.money -= 25; this.say("Bilan de santé : tout va bien. 🩺"); this.add("health", 2); break; }
        const opts = L.dis.map((d, i) => ({ label: `💊 Soigner : ${d.n}`, sub: d.incurable ? "Incurable" : this.m(d.cure * C.col), value: i }));
        const i = await Modal.choice("Médecin", "Quel problème traiter ?", opts, "🩺", true); if (i === null) return;
        const d = L.dis[i]; if (d.incurable) return Modal.alert("Incurable", "Les médecins ne peuvent rien faire, hélas.", "😔");
        const cost = d.cure * C.col; if (L.money < cost && L.age >= 18) return Modal.alert("Trop cher", "Vous n'avez pas les moyens de ce traitement.", "💸");
        if (L.age >= 18) L.money -= cost;
        if (chance(d.deadly ? .55 : .85)) { L.dis.splice(i, 1); this.add("health", 10); this.say(`✅ Traitement réussi : je suis guéri${this.he()} (${d.n}) !`, "good"); } else this.say(`Le traitement contre ${d.n} n'a pas fonctionné.`, "bad");
        break;
      }
      case "psy": L.money -= L.age >= 18 ? 70 : 0; this.add("happy", rnd(5, 12)); L.dis = L.dis.filter(d => !(d.mood && chance(.35))); this.say("Séance chez le psy : ça fait du bien de parler. 🛋️"); break;
      case "alt": L.money -= 60; if (chance(.15)) { this.add("health", 5); this.say("Étonnamment, l'acupuncture m'a fait du bien."); } else this.say("La médecine alternative n'a rien changé du tout."); break;
      case "surgery": {
        const s = await Modal.choice("Chirurgie esthétique", "", [{ label: "👃 Rhinoplastie (5 000)", value: 5000 }, { label: "💉 Liposuccion (7 000)", value: 7000 }, { label: "😶 Lifting du visage (12 000)", value: 12000 }, { label: "🦷 Facettes dentaires (8 000)", value: 8000 }], "💉", true);
        if (!s) return; if (L.money < s) return Modal.alert("Trop cher", "", "💸"); L.money -= s;
        if (chance(.85)) { this.add("looks", rnd(8, 20)); this.say("💉 Opération réussie : je suis rayonnant" + this.he() + " !", "good"); }
        else { this.add("looks", -rnd(10, 25)); this.add("health", -10); this.say("L'opération a mal tourné… Résultat catastrophique. 😱", "bad"); }
        break;
      }
      case "vacation": {
        const opts = LD.vacations.map((v, i) => ({ label: `✈️ ${v.n}`, sub: this.m(v.p), value: i }));
        const i = await Modal.choice("Vacances", `Budget : ${this.m(L.money)}`, opts, "✈️", true); if (i === null) return;
        const v = LD.vacations[i]; if (L.money < v.p) return Modal.alert("Trop cher", "", "💸");
        if (!this.once("vac")) return;
        L.money -= v.p; this.add("happy", v.h); this.add("health", 3); L.stats.vacations++;
        const p = this.partner(); if (p) p.r = clamp(p.r + 10);
        this.say(`🌴 Vacances : ${v.n}${p ? " avec " + p.n.split(" ")[0] : ""}. Inoubliable !`, "good");
        if (chance(.08)) { const ev = pick(["J'ai rencontré une célébrité à l'hôtel !", "Ma valise a été perdue par la compagnie aérienne.", "J'ai attrapé une intoxication alimentaire. 🤢", "J'ai pris un coup de soleil monumental."]); this.say(ev); }
        break;
      }
      case "club": {
        L.stats.parties++; L.money -= 60; this.add("happy", rnd(4, 10));
        const r = rnd(1, 10);
        if (r <= 3 && !this.partner()) { const g = (L.sexuality === "hetero") === (L.g === "M") ? "F" : "M"; const p = this.person(g, L.age + rnd(-4, 4), pick(LD.lastNames), { t: "partenaire", r: rnd(40, 70) }); if (await Modal.confirm("Coup de foudre", `${p.n} (${p.age} ans) vous drague sur la piste de danse. Sortir avec ?`, "Oui 😍", "Non")) { L.rel.push(p); L.stats.partners++; this.say(`💃 J'ai rencontré ${p.n} en boîte. On sort ensemble !`, "good"); } }
        else if (r === 4) { this.say("Un videur m'a refoulé" + this.he() + " à l'entrée. 😤", "bad"); }
        else if (r === 5) { this.add("health", -5); this.say("J'ai beaucoup trop bu… Lendemain difficile. 🥴", "bad"); }
        else this.say("Nuit endiablée en boîte de nuit ! 🪩");
        break;
      }
      case "show": { L.money -= 40; this.add("happy", rnd(4, 9)); this.say(`J'ai assisté à ${pick(["un concert de rock", "un film d'action au cinéma", "une pièce de Molière", "un spectacle d'humour", "un opéra", "un match de foot au stade", "un festival d'été"])}. 🎟️`); break; }
      case "casino": return this.casino();
      case "lotto": {
        if (L.money < 2) return; L.money -= 2;
        const r = Math.random();
        if (r < .000005) { const w = rnd(15, 120) * 1e6; L.money += w; L.fame = clamp(L.fame + 20); this.say(`🎉🎉 JACKPOT ! J'ai gagné ${this.m(w)} à la loterie !!!`, "big"); Sound.good(); }
        else if (r < .002) { const w = rnd(1000, 50000); L.money += w; this.say(`🎫 Ticket gagnant : ${this.m(w)} !`, "good"); Sound.coin(); }
        else if (r < .12) { const w = rnd(2, 40); L.money += w; this.say(`🎫 J'ai gagné ${this.m(w)} à la loterie.`); }
        else this.say("🎫 Ticket perdant.");
        break;
      }
      case "pet": {
        const opts = LD.pets.map((p, i) => ({ label: `${p.ic} ${p.n}`, sub: this.m(p.p), value: i }));
        const i = await Modal.choice("Animalerie & refuge", "", opts, "🐾", true); if (i === null) return;
        const P = LD.pets[i]; if (L.money < P.p && L.age >= 18) return Modal.alert("Trop cher", "", "💸");
        if (L.age < 18) { const par = this.rels(["pere", "mere"]); if (!par.length || chance(.5)) { this.say("Mes parents ont refusé que j'aie un animal. 😢", "bad"); break; } }
        else L.money -= P.p;
        const name = await Modal.input("Nom de l'animal", `Comment s'appelle votre ${P.n.toLowerCase()} ?`, pick(LD.petNames), P.ic) || pick(LD.petNames);
        L.rel.push({ id: Math.random().toString(36).slice(2, 9), t: "animal", kind: P.n, ic: P.ic, n: name, g: "M", age: rnd(0, 3), life: rnd(P.life[0], P.life[1]), r: 80, alive: true });
        this.say(`${P.ic} J'ai adopté ${name}, mon ${P.n.toLowerCase()} !`, "good"); this.add("happy", 10); break;
      }
      case "social": {
        const s = await Modal.choice("Réseaux sociaux", `${fmtNum(L.followers)} abonnés`, [{ label: "🤳 Poster un selfie", value: "selfie" }, { label: "🎥 Publier une vidéo", value: "video" }, { label: "💬 Clash avec une célébrité", value: "clash" }, { label: "🤝 Partenariat payant", value: "sponso", disabled: L.followers < 10000 }, { label: "🗑️ Supprimer mes comptes", value: "del" }], "📱", true);
        if (!s) return;
        if (s === "del") { L.followers = 0; this.say("J'ai supprimé tous mes réseaux sociaux. Quelle paix !"); this.add("happy", 5); break; }
        if (!this.once("soc" + s)) return;
        const base = (L.st.looks + L.fame * 2 + L.talent) / 3;
        if (s === "sponso") { const v = Math.round(L.followers * rf(.005, .03)); L.money += v; this.say(`🤝 Un partenariat m'a rapporté ${this.m(v)}.`, "good"); break; }
        let gain = Math.round(Math.pow(base / 10, 2.4) * rf(2, 30) + L.followers * rf(0, .15));
        if (s === "clash") { if (chance(.4)) { gain *= 6; L.fame = clamp(L.fame + 5); this.say("💥 Mon clash est devenu viral !", "good"); } else { gain = -Math.round(L.followers * .1); this.say("Mon clash s'est retourné contre moi… bad buzz.", "bad"); } }
        else if (chance(.04)) { gain *= 40; L.fame = clamp(L.fame + 8); this.say(`🚀 Ma ${s === "video" ? "vidéo" : "photo"} a fait le buzz : des millions de vues !`, "good"); }
        L.followers = Math.max(0, L.followers + gain);
        this.say(`${s === "video" ? "🎥 J'ai publié une vidéo" : "🤳 J'ai posté un selfie"} : ${gain >= 0 ? "+" : ""}${fmtNum(gain)} abonnés.`);
        if (L.followers > 1e5) L.fame = clamp(Math.max(L.fame, Math.log10(L.followers) * 10 - 30));
        break;
      }
      case "dating": case "app": {
        if (!this.once("dating")) return;
        if (this.partner()) { if (!await Modal.confirm("Infidélité ?", "Vous êtes déjà en couple. Chercher quelqu'un d'autre ?")) return; L.karma -= 6; }
        const g = (L.sexuality === "hetero") === (L.g === "M") ? "F" : "M";
        const cands = [0, 1, 2].map(() => this.person(g, clamp(L.age + rnd(-6, 6), 14, 99), pick(LD.lastNames), { t: "partenaire", r: rnd(40, 70), job: L.age >= 18 ? pick(LD.jobs).t : null, money: rnd(0, 50000) }));
        const i = await Modal.choice(id === "app" ? "Appli de rencontre" : "Rencontres", "Qui vous intéresse ?", cands.map((c, k) => ({ label: `${c.g === "F" ? "👩" : "👨"} ${c.n}, ${c.age} ans`, sub: `Apparence ${c.looks}% · Intelligence ${c.smarts}%${c.job ? " · " + c.job : ""}`, value: k })), "💘", true);
        if (i === null) return;
        const c = cands[i];
        if (chance(clamp(.5 + (L.st.looks - c.looks) / 150 + (L.money > 1e6 ? .2 : 0), .1, .9))) {
          const cur = this.partner(); if (cur) { if (chance(.5)) { this.say(`${cur.n.split(" ")[0]} a découvert ma liaison et m'a quitté${this.he()} ! 💔`, "bad"); cur.t = "ex"; cur.r = 5; } }
          L.rel.push(c); L.stats.partners++; this.say(`💘 Premier rendez-vous réussi avec ${c.n} ! Nous sortons ensemble.`, "good"); this.add("happy", 10);
        } else { this.say(`${c.n} ne m'a jamais répondu. 😶`, "bad"); this.add("happy", -3); }
        break;
      }
      case "license": {
        if (!this.once("license")) return;
        L.money -= 1500 * C.col;
        if (chance(clamp(.45 + L.st.smarts / 250))) { L.license = true; this.say("🚗 J'ai obtenu mon permis de conduire !", "good"); this.add("happy", 10); }
        else this.say(`J'ai raté l'examen du permis : ${pick(["j'ai grillé un feu rouge", "j'ai calé trois fois", "j'ai oublié le clignotant", "j'ai failli renverser un piéton"])}.`, "bad");
        break;
      }
      case "emigrate": {
        const i = await Modal.choice("Émigrer", "Vers quel pays ?", LD.countries.map((c, k) => ({ label: `${c.flag} ${c.n}`, value: k, disabled: k === L.country })), "🌍", true);
        if (i === null) return; if (L.money < 3000) return Modal.alert("Pas assez d'argent", "Un déménagement à l'étranger coûte au moins 3 000.", "💸");
        if (L.record.length && chance(.6)) { this.say("Mon visa a été refusé à cause de mon casier judiciaire.", "bad"); break; }
        L.money -= 3000; L.country = i; L.city = pick(LD.countries[i].cities); if (L.job) { L.jobs.push(L.job.t); L.job = null; }
        L.home = "loyer"; L.rent = 7000; L.flags.offers = null;
        this.say(`🌍 J'ai émigré : nouvelle vie à ${L.city} (${LD.countries[i].flag} ${LD.countries[i].n}) !`, "good"); this.add("happy", 8); break;
      }
      case "rename": { const n = await Modal.input("Changer de nom", "Nouveau prénom :", L.first, "✏️"); if (n) { L.money -= 200; this.say(`J'ai changé de prénom : ${L.first} → ${n}.`); L.first = n; } break; }
      case "donate": {
        if (L.money < 10) return Modal.alert("Pas assez d'argent", "", "💸");
        const v = await Modal.slider("Faire un don", "Montant :", 1, Math.max(1, Math.floor(L.money / 10)), Math.max(1, Math.floor(L.money / 100)), x => this.m(x * 10), "Donner", "💝");
        if (!v) return; L.money -= v * 10; L.stats.donations += v * 10; L.karma = clamp(L.karma + Math.min(15, Math.log10(v * 10) * 3)); this.add("happy", 5);
        this.say(`💝 J'ai donné ${this.m(v * 10)} à ${pick(["une association caritative", "la recherche contre le cancer", "un refuge animalier", "une ONG humanitaire", "les Restos du Cœur"])}.`, "good"); break;
      }
      case "night": L.money -= 500; this.add("smarts", rnd(3, 7)); this.say("J'ai suivi des cours du soir. 🎓"); break;
    }
    done();
  },
  async crime(c) {
    const L = this.L;
    if (!this.once("crime" + c.id)) return;
    if (!await Modal.confirm(c.n, "Êtes-vous sûr de vouloir commettre ce délit ?", "Passer à l'acte", "Renoncer", c.ic)) { L.flags.yearActs["crime" + c.id] = 0; return; }
    L.karma = clamp(L.karma + c.kar); L.stats.crimes++;
    let risk = c.risk - (L.st.smarts - 50) / 400 + (L.flags.fugitive ? .1 : 0);
    if (c.id === "braquage") { const plan = await Modal.choice("Plan du braquage", "Quelle approche ?", [{ label: "🔫 Entrée en force", value: .05 }, { label: "🕳️ Creuser un tunnel", value: -.15 }, { label: "🧑‍💻 Pirater le coffre", value: L.st.smarts > 70 ? -.2 : .1 }], "🏦"); risk += plan || 0; }
    if (chance(risk)) {
      this.say(`🚓 Pris${L.g === "F" ? "e" : ""} en flagrant délit : ${c.n.toLowerCase()} !`, "bad"); Sound.bad();
      await this.trial(c.n, c.prison);
    } else {
      const g = rnd(c.gain[0], c.gain[1]);
      L.money += g; if (c.fun) this.add("happy", c.fun);
      this.say(`${c.ic} ${c.n} réussi${g ? ` : j'ai empoché ${this.m(g)}` : ""}. 😈`, g ? "good" : ""); if (g) Sound.coin();
    }
  },
  casino() {
    const L = this.L;
    const m = Modal.open({ icon: "🎰", title: "Casino", sub: `Solde : ${this.m(L.money)}`, html: `<div class="choices">
      <button class="btn" id="cBJ">🃏 Blackjack</button><button class="btn" id="cRo">🎡 Roulette</button><button class="btn" id="cSl">🎰 Machine à sous</button></div>` });
    const bet = async () => { if (L.money < 10) { Modal.alert("Fauché", "Pas assez d'argent pour miser.", "💸"); return null; } const b = await Modal.slider("Mise", "Combien miser ?", 1, Math.max(1, Math.min(1000, Math.floor(L.money / 10))), 1, v => this.m(v * 10), "Miser", "🪙"); return b ? b * 10 : null; };
    $("#cSl", m.el).onclick = async () => {
      const b = await bet(); if (!b) return; L.money -= b;
      const sy = ["🍒", "🍋", "🔔", "⭐", "💎", "7️⃣"]; const r = [pick(sy), pick(sy), pick(sy)];
      let win = 0; if (r[0] === r[1] && r[1] === r[2]) win = b * (r[0] === "7️⃣" ? 50 : r[0] === "💎" ? 25 : 10); else if (r[0] === r[1] || r[1] === r[2]) win = b * 2;
      L.money += win; await Modal.alert(win ? "Gagné !" : "Perdu", `<div class="center" style="font-size:46px">${r.join(" ")}</div>${win ? `Vous gagnez ${this.m(win)} !` : "La machine a tout avalé."}`, "🎰");
      this.say(win ? `🎰 Jackpot à la machine à sous : +${this.m(win - b)} !` : `🎰 Perdu ${this.m(b)} à la machine à sous.`, win ? "good" : "bad"); this.save(); this.renderLog();
    };
    $("#cRo", m.el).onclick = async () => {
      const b = await bet(); if (!b) return;
      const c = await Modal.choice("Roulette", "Sur quoi miser ?", [{ label: "🔴 Rouge (x2)", value: "r" }, { label: "⚫ Noir (x2)", value: "n" }, { label: "🔢 Pair (x2)", value: "p" }, { label: "🎯 Numéro 17 (x36)", value: "17" }], "🎡");
      if (!c) return; L.money -= b;
      const n = rnd(0, 36), red = [1, 3, 5, 7, 9, 12, 14, 16, 18, 19, 21, 23, 25, 27, 30, 32, 34, 36].includes(n);
      const win = (c === "r" && red) || (c === "n" && !red && n) || (c === "p" && n && n % 2 === 0) ? b * 2 : c === "17" && n === 17 ? b * 36 : 0;
      L.money += win; await Modal.alert(win ? "Gagné !" : "Perdu", `La bille s'arrête sur le <b>${n}</b> ${n === 0 ? "🟢" : red ? "🔴" : "⚫"}.<br>${win ? "Vous gagnez " + this.m(win) + " !" : "Perdu !"}`, "🎡");
      this.say(win ? `🎡 Roulette : +${this.m(win - b)} !` : `🎡 Roulette : -${this.m(b)}.`, win ? "good" : "bad"); this.save(); this.renderLog();
    };
    $("#cBJ", m.el).onclick = async () => { const b = await bet(); if (!b) return; m.close(); this.blackjack(b); };
  },
  blackjack(b) {
    const L = this.L; L.money -= b;
    const suits = ["♠", "♥", "♦", "♣"], vals = ["A", "2", "3", "4", "5", "6", "7", "8", "9", "10", "V", "D", "R"];
    const deck = shuffle(suits.flatMap(s => vals.map(v => [v, s])));
    const score = h => { let t = 0, a = 0; for (const [v] of h) { if (v === "A") { a++; t += 11; } else t += ["V", "D", "R"].includes(v) ? 10 : +v; } while (t > 21 && a--) t -= 10; return t; };
    const P = [deck.pop(), deck.pop()], D = [deck.pop(), deck.pop()];
    const card = (c, hide) => hide ? `<div class="pcard back"></div>` : `<div class="pcard ${c[1] === "♥" || c[1] === "♦" ? "r" : ""}">${c[0]}${c[1]}</div>`;
    const m = Modal.open({ icon: "🃏", title: "Blackjack", sub: `Mise : ${this.m(b)}`, closable: false, html: `<div id="bj"></div>` });
    const render = (end, msg) => {
      $("#bj", m.el).innerHTML = `<div class="muted center">Croupier ${end ? "(" + score(D) + ")" : ""}</div><div class="cards">${D.map((c, i) => card(c, !end && i === 1)).join("")}</div>
        <div class="muted center">Vous (${score(P)})</div><div class="cards">${P.map(c => card(c)).join("")}</div>
        ${msg ? `<p class="center"><b>${msg}</b></p><button class="btn gold block" id="bjOk">OK</button>` : `<div class="row"><button class="btn green" id="bjH">Carte</button><button class="btn red" id="bjS">Rester</button></div>`}`;
      if (msg) $("#bjOk", m.el).onclick = () => { m.close(); this.save(); this.renderLog(); };
      else { $("#bjH", m.el).onclick = hit; $("#bjS", m.el).onclick = stand; }
    };
    const finish = (win, msg) => { L.money += win; this.say(win > b ? `🃏 Blackjack : +${this.m(win - b)} !` : win === b ? "🃏 Blackjack : égalité." : `🃏 Blackjack : -${this.m(b)}.`, win > b ? "good" : win === b ? "" : "bad"); render(true, msg); win > b ? Sound.coin() : Sound.bad(); };
    const hit = () => { P.push(deck.pop()); if (score(P) > 21) finish(0, "Vous avez dépassé 21 ! Perdu."); else render(); };
    const stand = () => { while (score(D) < 17) D.push(deck.pop()); const p = score(P), d = score(D); if (d > 21 || p > d) finish(b * 2, `Gagné ! +${this.m(b)}`); else if (p === d) finish(b, "Égalité, mise rendue."); else finish(0, "Le croupier gagne."); };
    if (score(P) === 21) finish(Math.round(b * 2.5), "BLACKJACK ! 🎉"); else render();
  },
};

// ============ Événements aléatoires ============
// c : condition, w : poids, p : probabilité de déclenchement, once : une seule fois par vie
Life.EVENTS = [
  { id: "firstword", c: L => L.age === 1, p: 1, run: (L, G) => G.say(`J'ai prononcé mon premier mot : « ${pick(["Maman", "Papa", "Non", "Gâteau", "Chat", "Encore"])} ». 🗣️`) },
  { id: "walk", c: L => L.age === 1, p: 1, run: (L, G) => G.say("J'ai fait mes premiers pas ! 👣", "good") },
  { id: "babysick", c: L => L.age < 4, run: (L, G) => { G.say(pick(["J'ai fait une otite. 🤒", "J'ai mis un Lego dans ma bouche… Frayeur aux urgences.", "Je suis tombé" + G.he() + " de la table à langer.", "J'ai eu une grosse fièvre."]), "bad"); G.add("health", -5); } },
  { id: "babyhappy", c: L => L.age < 5, run: (L, G) => { G.say(pick(["J'ai découvert la neige ! ❄️", "J'ai reçu un doudou que j'adore. 🧸", "Mes parents m'ont emmené" + G.he() + " au zoo. 🦁", "J'ai fait mon premier château de sable. 🏖️", "J'ai appris à faire du vélo ! 🚲"]), "good"); G.add("happy", 5); } },
  { id: "bully", c: L => L.age >= 7 && L.age <= 16 && L.school, w: 1.2, run: async (L, G) => {
    const b = G.person(chance(.6) ? "M" : "F", L.age, pick(LD.lastNames)).n;
    const r = await Modal.choice("Harcèlement", `${b} se moque de vous et vous bouscule dans la cour de récréation.`, [
      { label: "👊 Le frapper", value: 0 }, { label: "🗣️ Lui répondre du tac au tac", value: 1 }, { label: "🧑‍🏫 Prévenir un adulte", value: 2 }, { label: "😶 Ignorer", value: 3 }], "😠");
    if (r === 0) { if (chance(.5)) { G.say(`J'ai mis une raclée à ${b}. Plus personne ne m'embête.`, "good"); G.add("happy", 6); } else { G.say(`${b} m'a frappé${G.he()} plus fort. J'ai un œil au beurre noir.`, "bad"); G.add("health", -8); } if (chance(.4)) { G.say("J'ai été collé" + G.he() + " pour bagarre."); L.grades = clamp(L.grades - 5); } }
    if (r === 1) { if (chance(.5)) { G.say(`Ma répartie a ridiculisé ${b} devant tout le monde. 😎`, "good"); G.add("happy", 6); } else G.say(`${b} a continué à se moquer de moi.`, "bad"); }
    if (r === 2) { G.say(`J'ai prévenu un surveillant : ${b} a été puni${chance(.5) ? "" : "… mais m'a traité" + G.he() + " de balance"}.`); }
    if (r === 3) { G.say(`J'ai ignoré ${b}.`); G.add("happy", -4); }
  } },
  { id: "crush", c: L => L.age >= 11 && L.age <= 17 && !Life.partner(), run: async (L, G) => {
    const g = (L.sexuality === "hetero") === (L.g === "M") ? "F" : "M";
    const p = G.person(g, L.age, pick(LD.lastNames), { t: "partenaire", r: 60 });
    const r = await Modal.confirm("Coup de cœur", `Vous avez le béguin pour ${p.n}. Lui ${g === "F" ? "(elle)" : ""} déclarer votre flamme ?`, "Oui 💌", "Non", "💘");
    if (r) { if (chance(clamp(L.st.looks / 120 + .15, .1, .9))) { L.rel.push(p); L.stats.partners++; G.say(`💌 ${p.n} ressent la même chose ! On sort ensemble.`, "good"); G.add("happy", 12); } else { G.say(`${p.n} m'a gentiment dit non… 💔`, "bad"); G.add("happy", -10); } }
  } },
  { id: "exam", c: L => L.school && L.age >= 8, run: async (L, G) => {
    const r = await Modal.choice("Contrôle surprise", "Contrôle surprise en mathématiques ! Que faites-vous ?", [{ label: "🧠 Faire de mon mieux", value: 0 }, { label: "👀 Copier sur mon voisin", value: 1 }, { label: "🤒 Simuler un malaise", value: 2 }], "📝");
    if (r === 0) { const ok = chance(L.st.smarts / 100); G.say(ok ? "J'ai eu une excellente note au contrôle ! 🌟" : "J'ai eu une mauvaise note au contrôle.", ok ? "good" : "bad"); L.grades = clamp(L.grades + (ok ? 4 : -4)); }
    if (r === 1) { if (chance(.6)) { G.say("J'ai copié… et ça a marché. 😏"); L.grades = clamp(L.grades + 3); } else { G.say("Pris" + (L.g === "F" ? "e" : "") + " en train de copier ! Zéro et convocation des parents.", "bad"); L.grades = clamp(L.grades - 8); } L.karma -= 2; }
    if (r === 2) { G.say("L'infirmière n'a pas cru à mon malaise.", "bad"); }
  } },
  { id: "teentrouble", c: L => L.age >= 14 && L.age <= 19, run: async (L, G) => {
    const r = await Modal.choice("Soirée", "Des amis vous proposent une cigarette et de l'alcool à une soirée.", [{ label: "🙅 Refuser poliment", value: 0 }, { label: "🍺 Accepter", value: 1 }, { label: "🏃 Partir de la soirée", value: 2 }], "🎉");
    if (r === 1) { G.add("happy", 5); G.add("health", -4); G.say("J'ai fait la fête… un peu trop. 🥴"); if (chance(.3)) { G.say("Mes parents m'ont surpris" + G.he() + " en rentrant à 4 h du matin.", "bad"); G.rels(["pere", "mere"]).forEach(p => p.r = clamp(p.r - 10)); } }
    else G.say(r === 0 ? "J'ai refusé l'alcool et la cigarette à une soirée." : "J'ai quitté une soirée qui tournait mal.");
  } },
  { id: "parentsdivorce", once: 1, c: L => L.age >= 3 && L.age <= 17 && Life.rels("pere").length && Life.rels("mere").length, w: .4, run: (L, G) => { G.say("💔 Mes parents ont divorcé. Je vis désormais une semaine sur deux chez chacun.", "bad"); G.add("happy", -15); } },
  { id: "found", c: L => L.age >= 6, w: .6, run: async (L, G) => {
    const v = rnd(20, 500);
    const r = await Modal.choice("Portefeuille trouvé", `Vous trouvez un portefeuille contenant ${G.m(v)} dans la rue.`, [{ label: "👮 Le rapporter au commissariat", value: 0 }, { label: "💰 Garder l'argent", value: 1 }], "👛");
    if (r) { L.money += v; L.karma -= 5; G.say(`J'ai gardé ${G.m(v)} trouvés dans un portefeuille.`); } else { L.karma += 6; G.say("J'ai rapporté un portefeuille perdu. Son propriétaire m'a chaleureusement remercié" + G.he() + ". 😇", "good"); if (chance(.3)) { L.money += 50; G.say("Il m'a même donné une petite récompense."); } }
  } },
  { id: "accident", c: L => L.age >= 5, w: .5, run: (L, G) => { const a = pick(["Je me suis cassé le bras en tombant à vélo.", "Je me suis fait mordre par un chien.", "Je suis tombé" + G.he() + " dans les escaliers.", "Je me suis brûlé" + G.he() + " en cuisinant.", "Je me suis tordu la cheville en jouant au foot."]); G.add("health", -rnd(5, 15)); G.say(`🤕 ${a}`, "bad"); } },
  { id: "promoevent", c: L => L.job && L.age >= 18, run: async (L, G) => {
    const r = await Modal.choice("Au travail", pick(["Votre patron vous demande de rester tard ce soir pour finir un dossier urgent.", "Un collègue s'attribue le mérite de votre travail en réunion.", "On vous propose de diriger un nouveau projet ambitieux."]), [{ label: "👍 Accepter / laisser couler", value: 0 }, { label: "✋ Refuser / protester", value: 1 }], "💼");
    if (r === 0) { L.job.perf = clamp(L.job.perf + 8); G.add("happy", -3); G.say("J'ai pris sur moi au travail. Mon patron a apprécié."); }
    else { if (chance(.5)) { G.add("happy", 4); G.say("J'ai su dire non au travail, et c'est passé."); } else { L.job.perf = clamp(L.job.perf - 8); G.say("Mon refus a été mal perçu par ma hiérarchie.", "bad"); } }
  } },
  { id: "lottoGift", c: L => L.age >= 18, w: .3, run: (L, G) => { const v = rnd(100, 3000); L.money += v; G.say(pick([`J'ai reçu un remboursement des impôts de ${G.m(v)}. 💶`, `J'ai gagné ${G.m(v)} à un jeu-concours !`, `J'ai vendu de vieux objets en ligne pour ${G.m(v)}.`]), "good"); } },
  { id: "fine", c: L => L.age >= 18, w: .4, run: (L, G) => { const v = rnd(35, 750); L.money -= v; G.say(pick([`Amende pour excès de vitesse : ${G.m(v)}. 🚓`, `Mon lave-linge a rendu l'âme : réparation ${G.m(v)}.`, `Fuite d'eau chez moi : ${G.m(v)} de travaux.`, `Contravention de stationnement : ${G.m(v)}.`]), "bad"); } },
  { id: "proposal", c: L => { const p = Life.partner(); return p && p.t === "partenaire" && p.r > 70 && L.age >= 22; }, w: .5, run: async (L, G) => {
    const p = G.partner();
    const r = await Modal.confirm("Demande en mariage", `${p.n} pose un genou à terre et vous demande en mariage ! 💍`, "Oui ! 💍", "Non…", "💍");
    if (r) { p.t = "fiance"; G.say(`💍 J'ai accepté la demande en mariage de ${p.n} !`, "good"); G.add("happy", 20); } else { p.r = clamp(p.r - 30); G.say(`J'ai refusé la demande de ${p.n}. Ambiance glaciale…`, "bad"); if (chance(.4)) { p.t = "ex"; G.say(`${p.n.split(" ")[0]} m'a quitté${G.he()}.`, "bad"); } }
  } },
  { id: "breakup", c: L => { const p = Life.partner(); return p && p.r < 30; }, w: 1.5, p: .6, run: (L, G) => { const p = G.partner(); if (p.t === "conjoint") { G.say(`📄 ${p.n} a demandé le divorce.`, "bad"); L.money -= Math.round(Math.max(0, L.money) * .3); } else G.say(`💔 ${p.n} m'a quitté${G.he()}.`, "bad"); p.t = "ex"; G.add("happy", -15); } },
  { id: "kidsEvent", c: L => Life.rels("enfant").some(k => k.age >= 4 && k.age < 18), run: (L, G) => { const k = pick(G.rels("enfant").filter(k => k.age >= 4 && k.age < 18)); const n = k.n.split(" ")[0]; const e = pick([[`${n} a eu d'excellentes notes à l'école. Je suis fier${G.he()} ! 🌟`, 1], [`${n} s'est fait renvoyer de cours.`, -1], [`${n} a gagné une compétition sportive ! 🏅`, 1], [`${n} a fait une grosse bêtise à la maison.`, -1], [`${n} m'a fait un dessin magnifique. 🎨`, 1]]); G.say(e[0], e[1] > 0 ? "good" : "bad"); G.add("happy", e[1] * 4); } },
  { id: "inherit", c: L => L.age >= 25, w: .15, once: 1, run: (L, G) => { const v = rnd(5000, 250000); L.money += v; G.say(`📜 Un grand-oncle dont j'ignorais l'existence m'a légué ${G.m(v)} !`, "good"); } },
  { id: "scam", c: L => L.age >= 25 && L.money > 2000, w: .4, run: async (L, G) => {
    const r = await Modal.choice("Courriel suspect", "Un « prince » vous promet 2 millions si vous lui avancez 1 500 pour les frais de transfert.", [{ label: "🗑️ Supprimer le courriel", value: 0 }, { label: "💸 Envoyer l'argent", value: 1 }], "📧");
    if (r) { L.money -= 1500; G.say("J'ai envoyé 1 500 à un « prince »… Évidemment, c'était une arnaque. 🤦", "bad"); } else G.say("J'ai déjoué une arnaque par courriel.");
  } },
  { id: "dog", c: L => L.age >= 8, w: .3, run: async (L, G) => { const r = await Modal.confirm("Chien errant", "Un chien errant vous suit jusqu'à chez vous. L'adopter ?", "L'adopter 🐶", "Non", "🐕"); if (r) { const n = pick(LD.petNames); L.rel.push({ id: Math.random().toString(36).slice(2, 9), t: "animal", kind: "Chien", ic: "🐕", n, g: "M", age: rnd(1, 5), life: rnd(10, 15), r: 70, alive: true }); G.say(`🐕 J'ai adopté un chien errant que j'ai appelé ${n}.`, "good"); G.add("happy", 8); } } },
  { id: "pandemic", once: 1, c: L => L.age >= 5, w: .15, run: (L, G) => { G.say("🦠 Une pandémie mondiale a éclaté. Confinement général pendant des mois.", "bad"); G.add("happy", -12); if (chance(.3)) { G.add("health", -15); G.say("J'ai attrapé le virus… et j'en suis sorti" + G.he() + " épuisé" + G.he() + ".", "bad"); } } },
  { id: "famousMeet", c: L => L.age >= 12, w: .2, run: (L, G) => { G.say(`🤩 J'ai croisé ${pick(["un célèbre footballeur", "une chanteuse célèbre", "un acteur de cinéma", "un youtubeur connu", "un ancien ministre"])} dans la rue et j'ai eu un selfie !`, "good"); G.add("happy", 6); } },
  { id: "midlife", c: L => L.age >= 40 && L.age <= 55, w: .4, once: 1, run: async (L, G) => { const r = await Modal.choice("Crise de la quarantaine", "Vous vous sentez vieillir… Que faire ?", [{ label: "🏍️ Acheter une grosse moto", value: 0 }, { label: "🧘 Partir en retraite spirituelle", value: 1 }, { label: "💇 Changer de look radicalement", value: 2 }, { label: "🤷 Accepter le temps qui passe", value: 3 }], "🫠"); const t = ["J'ai acheté une grosse moto. 🏍️", "Retraite spirituelle en Inde : révélation !", "Nouveau look : on ne me reconnaît plus !", "J'accepte de vieillir avec sérénité."][r]; if (r === 0) { L.money -= 18000; L.assets.push({ k: "car", n: "Grosse cylindrée", ic: "🏍️", p: 18000, cond: 100 }); } if (r === 1) L.money -= 3000; if (r === 2) G.add("looks", 6); G.add("happy", 10); G.say(t); } },
  { id: "grandkid", c: L => Life.rels("enfant").some(k => k.age >= 24), w: .3, run: (L, G) => { const k = pick(G.rels("enfant").filter(k => k.age >= 24)); G.say(`👶 ${k.n.split(" ")[0]} m'a annoncé que je vais être grand-${L.g === "F" ? "mère" : "père"} !`, "good"); G.add("happy", 15); } },
  { id: "oldfall", c: L => L.age >= 70, run: (L, G) => { G.say(pick(["Je suis tombé" + G.he() + " dans ma salle de bain.", "J'ai oublié où j'avais garé ma voiture… pendant 3 heures.", "J'ai du mal à monter les escaliers."]), "bad"); G.add("health", -6); } },
  { id: "oldjoy", c: L => L.age >= 65, run: (L, G) => { G.say(pick(["J'ai appris à jouer au bridge. 🃏", "Je passe mes journées au jardin. 🌻", "J'ai fait un voyage avec le club du troisième âge.", "Mes petits-enfants sont venus me voir. 🥰"]), "good"); G.add("happy", 6); } },
  { id: "talent", c: L => L.age >= 6 && L.age <= 20, run: (L, G) => { const t = pick([["J'ai découvert que j'avais l'oreille absolue. 🎵", "talent"], ["J'ai gagné le cross de l'école ! 🏃", "sport"], ["J'ai gagné un concours de dessin. 🎨", "talent"], ["J'ai été repéré" + G.he() + " par un club de sport.", "sport"]]); L[t[1]] = clamp(L[t[1]] + 10); G.say(t[0], "good"); } },
  { id: "robbed", c: L => L.age >= 16, w: .35, run: (L, G) => { const v = Math.min(Math.max(0, L.money), rnd(50, 2000)); L.money -= v; G.say(`🦹 Je me suis fait cambrioler${v ? ` : ${G.m(v)} envolés` : ""}.`, "bad"); G.add("happy", -6); } },
  { id: "jury", c: L => L.age >= 23 && !L.record.length, w: .2, once: 1, run: (L, G) => G.say("⚖️ J'ai été tiré" + G.he() + " au sort pour être juré d'assises. Expérience marquante.") },
  { id: "fugitive", c: L => L.flags.fugitive, w: 3, p: .35, run: async (L, G) => { G.say("🚓 La police m'a retrouvé" + G.he() + " après ma cavale !", "bad"); L.flags.fugitive = 0; L.prison = { left: rnd(3, 8), total: 8, crime: "Évasion" }; if (L.job) L.job = null; } },
  { id: "lovers", c: L => { const p = Life.partner(); return p && p.r > 60; }, run: (L, G) => { const p = G.partner(); G.say(pick([`${p.n.split(" ")[0]} m'a préparé un dîner surprise. 🥰`, `${p.n.split(" ")[0]} et moi avons fêté notre anniversaire de couple.`, `Week-end en amoureux avec ${p.n.split(" ")[0]}.`]), "good"); G.add("happy", 6); p.r = clamp(p.r + 3); } },
  { id: "cheat", c: L => { const p = Life.partner(); return p && p.r < 50; }, w: .5, run: async (L, G) => { const p = G.partner(); const r = await Modal.choice("Trahison", `Vous découvrez que ${p.n} vous trompe !`, [{ label: "💔 Rompre immédiatement", value: 0 }, { label: "🙏 Pardonner", value: 1 }, { label: "🔥 Se venger en brûlant ses affaires", value: 2 }], "😡"); if (r === 0) { p.t = "ex"; G.say(`J'ai quitté ${p.n} après sa trahison.`, "bad"); } if (r === 1) { p.r = clamp(p.r + 10); G.say(`J'ai pardonné à ${p.n}.`); } if (r === 2) { p.t = "ex"; L.karma -= 5; G.say(`J'ai brûlé les affaires de ${p.n} dans le jardin. 🔥`, "bad"); } G.add("happy", -12); } },
  { id: "drunkDriver", c: L => L.age >= 18 && L.license, w: .3, run: async (L, G) => { const r = await Modal.choice("Soirée arrosée", "Vous avez bu à une soirée. Comment rentrer ?", [{ label: "🚕 Prendre un taxi", value: 0 }, { label: "🚗 Conduire quand même", value: 1 }, { label: "🛋️ Dormir sur place", value: 2 }], "🍷"); if (r === 1) { if (chance(.3)) { G.say("🚓 Contrôle d'alcoolémie positif. Permis suspendu !", "bad"); L.license = false; L.money -= 4500; L.record.push("Conduite en état d'ivresse"); } else G.say("J'ai conduit en ayant bu. Je m'en suis tiré" + G.he() + "… cette fois."); L.karma -= 3; } else G.say(r === 0 ? "J'ai pris un taxi après une soirée." : "J'ai dormi sur le canapé d'un ami."); } },
  { id: "volunteer", c: L => L.age >= 14, w: .3, run: async (L, G) => { if (await Modal.confirm("Bénévolat", "Une association cherche des bénévoles pour distribuer des repas. Participer ?", "Oui 🙌", "Non", "🤲")) { L.karma = clamp(L.karma + 6); G.add("happy", 6); G.say("J'ai fait du bénévolat cette année. 🤲", "good"); } } },
  { id: "dream", c: L => L.age >= 4 && L.age <= 10, run: (L, G) => G.say(`Quand je serai grand${G.he()}, je veux être ${pick(["astronaute", "pompier", "vétérinaire", "footballeur", "chanteuse", "président", "pilote", "dinosaure"])} !`) },
  { id: "siblingFight", c: L => L.age < 18 && Life.rels(["frere", "soeur"]).length, run: (L, G) => { const s = pick(G.rels(["frere", "soeur"])); s.r = clamp(s.r - 6); G.say(`Je me suis disputé${G.he()} avec ${s.n.split(" ")[0]} pour ${pick(["la télécommande", "le dernier yaourt", "la place à l'avant de la voiture", "un jeu vidéo"])}.`); } },
  { id: "pocket", c: L => L.age >= 6 && L.age < 18, run: (L, G) => { const v = rnd(5, 60); L.money += v; G.say(`J'ai reçu ${G.m(v)} ${pick(["d'argent de poche", "pour mon anniversaire", "de la petite souris", "de ma grand-mère"])}. 💰`); } },
  { id: "disease_mental", c: L => L.st.happy < 25 && !L.dis.some(d => d.n === "Dépression"), w: 1, run: (L, G) => { L.dis.push(Object.assign({}, LD.diseases.find(d => d.n === "Dépression"))); G.say("Je suis tombé" + G.he() + " en dépression. Un psy pourrait m'aider.", "bad"); } },
  { id: "promoTax", c: L => L.money > 1e6, w: .4, run: (L, G) => { const v = Math.round(L.money * rf(.01, .04)); L.money -= v; G.say(`Le fisc m'a réclamé ${G.m(v)} d'impôt sur la fortune. 🧾`, "bad"); } },
  { id: "military", once: 1, c: L => L.age === 18 && L.country === 0, w: 1, p: .4, run: (L, G) => G.say("J'ai participé à la Journée défense et citoyenneté. 🇫🇷") },
];
