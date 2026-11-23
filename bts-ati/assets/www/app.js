(function () {
  "use strict";
  var $ = function (s) { return document.querySelector(s); };
  var view = $("#view"), titleEl = $("#title"), backBtn = $("#back");

  // ---------- Stockage local (progression) ----------
  var store = { done: {}, fav: {}, quiz: {}, last: null, year: 0, theme: "" };
  try { var raw = localStorage.getItem("btsati"); if (raw) store = Object.assign(store, JSON.parse(raw)); } catch (e) {}
  function save() { try { localStorage.setItem("btsati", JSON.stringify(store)); } catch (e) {} }

  // ---------- Index des chapitres ----------
  var ALL = [], BYID = {};
  COURS.forEach(function (s) {
    s.chapters.forEach(function (c, i) {
      c.subject = s; c.index = i; c.key = s.id + "/" + c.id;
      c.cards = c.cards || []; c.quiz = c.quiz || [];
      ALL.push(c); BYID[c.key] = c;
    });
  });

  function esc(t) { return String(t).replace(/[&<>"]/g, function (m) { return { "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" }[m]; }); }
  function plain(html) { var d = document.createElement("div"); d.innerHTML = html; return d.textContent || ""; }
  function norm(t) { return t.toLowerCase().normalize("NFD").replace(/[̀-ͯ]/g, ""); }
  function yearTag(y) { return y === 1 ? '<span class="tag y1">1ʳᵉ année</span>' : y === 2 ? '<span class="tag y2">2ᵉ année</span>' : '<span class="tag">1ʳᵉ & 2ᵉ</span>'; }
  function inYear(c) { return !store.year || !c.y || c.y === store.year; }
  function shuffle(a) { a = a.slice(); for (var i = a.length - 1; i > 0; i--) { var j = Math.floor(Math.random() * (i + 1)); var t = a[i]; a[i] = a[j]; a[j] = t; } return a; }
  function pct(n, d) { return d ? Math.round(100 * n / d) : 0; }
  function countDone(list) { return list.filter(function (c) { return store.done[c.key]; }).length; }

  // ---------- Thème ----------
  function applyTheme() {
    var t = store.theme || (window.matchMedia && matchMedia("(prefers-color-scheme: dark)").matches ? "dark" : "light");
    document.documentElement.setAttribute("data-theme", t);
    $("#theme").innerHTML = t === "dark" ? "&#9728;" : "&#9790;";
  }
  $("#theme").onclick = function () {
    store.theme = document.documentElement.getAttribute("data-theme") === "dark" ? "light" : "dark"; save(); applyTheme();
  };
  backBtn.onclick = function () { history.back(); };
  applyTheme();

  function setHeader(t, canBack, tab) {
    titleEl.textContent = t;
    backBtn.style.visibility = canBack ? "visible" : "hidden";
    document.querySelectorAll("#tabs a").forEach(function (a) { a.classList.toggle("on", a.getAttribute("data-tab") === tab); });
    window.scrollTo(0, 0);
  }

  function yearChips(cb) {
    var h = '<div class="chips">';
    [[0, "Tout le BTS"], [1, "1ʳᵉ année"], [2, "2ᵉ année"]].forEach(function (o) {
      h += '<button class="chip' + (store.year === o[0] ? " on" : "") + '" data-y="' + o[0] + '">' + o[1] + "</button>";
    });
    return h + "</div>";
  }
  function bindChips() {
    view.querySelectorAll(".chip[data-y]").forEach(function (b) {
      b.onclick = function () { store.year = +b.getAttribute("data-y"); save(); route(); };
    });
  }

  // ---------- Écrans ----------
  function home() {
    setHeader("BTS ATI — Cours", false, "home");
    var list = ALL.filter(inYear), d = countDone(list);
    var h = '<div class="hero"><h2>Salut 👋 Prêt à réviser ?</h2><p>Tous les cours du BTS ATI (Assistance Technique d\'Ingénieur), 1ʳᵉ et 2ᵉ année, expliqués simplement. Fonctionne sans Internet.</p>' +
      '<div class="bar"><i style="width:' + pct(d, list.length) + '%"></i></div><p style="margin-top:6px;font-size:13px">' + d + " / " + list.length + " chapitres appris (" + pct(d, list.length) + " %)</p></div>";
    if (store.last && BYID[store.last]) {
      var l = BYID[store.last];
      h += '<a class="row" href="#/c/' + l.key + '"><span class="n">▶</span><div><small>Reprendre</small><b>' + esc(l.t) + "</b><small>" + l.subject.emoji + " " + esc(l.subject.name) + "</small></div></a>";
    }
    h += yearChips() + '<div class="grid">';
    COURS.forEach(function (s) {
      var ch = s.chapters.filter(inYear); if (!ch.length) return;
      var dd = countDone(ch);
      h += '<a class="subj" style="--c:' + s.color + '" href="#/s/' + s.id + '"><span class="em">' + s.emoji + "</span><b>" + esc(s.name) + "</b><small>" + ch.length + " chapitres · " + pct(dd, ch.length) + " %</small>" +
        '<div class="bar"><i style="width:' + pct(dd, ch.length) + "%;background:" + s.color + '"></i></div></a>';
    });
    h += "</div>";
    view.innerHTML = h; bindChips();
  }

  function subject(id) {
    var s = COURS.filter(function (x) { return x.id === id; })[0]; if (!s) return home();
    setHeader(s.emoji + " " + s.name, true, "home");
    var h = '<div class="card"><p style="margin:0">' + s.desc + "</p></div>" + yearChips() + '<div class="list">';
    s.chapters.forEach(function (c, i) {
      if (!inYear(c)) return;
      var q = store.quiz[c.key];
      h += '<a class="row" href="#/c/' + c.key + '"><span class="n' + (store.done[c.key] ? " done" : "") + '">' + (store.done[c.key] ? "✓" : i + 1) + "</span><div><b>" + esc(c.t) + "</b>" + yearTag(c.y) +
        (c.quiz.length ? '<small>Quiz : ' + (q != null ? q + " %" : "pas encore fait") + "</small>" : "") + "</div><span>›</span></a>";
    });
    view.innerHTML = h + "</div>"; bindChips();
  }

  function chapter(key) {
    var c = BYID[key]; if (!c) return home();
    store.last = key; save();
    setHeader(c.t, true, "home");
    var s = c.subject, prev = s.chapters[c.index - 1], next = s.chapters[c.index + 1];
    var h = '<div style="display:flex;align-items:center;gap:8px;margin-bottom:10px"><div style="flex:1"><small class="muted">' + s.emoji + " " + esc(s.name) + "</small><br>" + yearTag(c.y) +
      '</div><button class="fav" id="fav">' + (store.fav[key] ? "★" : "☆") + "</button></div>";
    h += '<article class="course">' + c.html + "</article>";
    h += '<div class="btns"><button class="btn ' + (store.done[key] ? "ok" : "alt") + '" id="done">' + (store.done[key] ? "✓ Chapitre appris" : "Marquer comme appris") + "</button>";
    if (c.quiz.length) h += '<a class="btn" href="#/q/' + key + '">🎯 Quiz (' + c.quiz.length + " questions)</a>";
    if (c.cards.length) h += '<a class="btn alt" href="#/cards/' + key + '">🃏 Fiches (' + c.cards.length + ")</a>";
    h += "</div><div class=\"btns\">";
    if (prev) h += '<a class="btn alt" href="#/c/' + prev.key + '">‹ ' + esc(prev.t) + "</a>";
    if (next) h += '<a class="btn alt" href="#/c/' + next.key + '">' + esc(next.t) + " ›</a>";
    view.innerHTML = h + "</div>";
    $("#fav").onclick = function () { if (store.fav[key]) delete store.fav[key]; else store.fav[key] = 1; save(); this.textContent = store.fav[key] ? "★" : "☆"; };
    $("#done").onclick = function () {
      if (store.done[key]) delete store.done[key]; else store.done[key] = 1; save();
      this.className = "btn " + (store.done[key] ? "ok" : "alt"); this.textContent = store.done[key] ? "✓ Chapitre appris" : "Marquer comme appris";
    };
  }

  // ---------- Quiz ----------
  function runQuiz(questions, title, onEnd) {
    var i = 0, score = 0;
    function show() {
      if (i >= questions.length) return end();
      var q = questions[i], order = shuffle(q[1].map(function (_, k) { return k; }));
      var h = '<div class="card"><small class="muted">Question ' + (i + 1) + " / " + questions.length + (q.src ? " · " + esc(q.src) : "") +
        '</small><div class="bar"><i style="width:' + pct(i, questions.length) + '%"></i></div></div><div class="card"><p class="q">' + q[0] + "</p>";
      order.forEach(function (k) { h += '<button class="opt" data-k="' + k + '">' + q[1][k] + "</button>"; });
      h += '<div id="ex"></div></div>';
      view.innerHTML = h;
      view.querySelectorAll(".opt").forEach(function (b) {
        b.onclick = function () {
          if (view.querySelector(".opt.good")) return;
          var k = +b.getAttribute("data-k"), ok = k === q[2];
          if (ok) score++;
          view.querySelectorAll(".opt").forEach(function (o) { if (+o.getAttribute("data-k") === q[2]) o.classList.add("good"); });
          if (!ok) b.classList.add("bad");
          $("#ex").innerHTML = '<div class="box ' + (ok ? "ex" : "warn") + ' expl"><b>' + (ok ? "Bonne réponse ! ✅" : "Pas tout à fait ❌") + "</b><br>" + (q[3] || "") + '</div><div class="btns"><button class="btn" id="nx">' + (i + 1 < questions.length ? "Question suivante ›" : "Voir mon score") + "</button></div>";
          $("#nx").onclick = function () { i++; show(); window.scrollTo(0, 0); };
        };
      });
    }
    function end() {
      var p = pct(score, questions.length);
      if (onEnd) onEnd(p);
      var msg = p >= 80 ? "Excellent ! Tu maîtrises 🏆" : p >= 50 ? "Pas mal ! Relis les points ratés 💪" : "Relis le cours puis recommence, tu vas y arriver 📚";
      view.innerHTML = '<div class="card" style="text-align:center"><div class="big">' + (p >= 80 ? "🏆" : p >= 50 ? "👍" : "📚") + "</div><h2>" + score + " / " + questions.length + " (" + p + " %)</h2><p>" + msg +
        '</p><div class="btns"><button class="btn" id="again">Recommencer</button><button class="btn alt" id="bk">Retour</button></div></div>';
      $("#again").onclick = function () { runQuiz(shuffle(questions), title, onEnd); };
      $("#bk").onclick = function () { history.back(); };
    }
    show();
  }
  function quiz(key) {
    var c = BYID[key]; if (!c) return home();
    setHeader("Quiz : " + c.t, true, "challenge");
    runQuiz(shuffle(c.quiz), c.t, function (p) { store.quiz[key] = Math.max(p, store.quiz[key] || 0); if (p >= 80) store.done[key] = 1; save(); });
  }
  function challenge(sid) {
    setHeader("Quiz défi", sid != null, "challenge");
    if (sid == null) {
      var h = '<div class="card"><b>🎯 Quiz défi</b><p class="muted" style="margin:4px 0 0">10 questions tirées au hasard. Choisis une matière ou mélange tout.</p></div>' + yearChips() + '<div class="list">';
      h += '<a class="row" href="#/challenge/all"><span class="n">🔀</span><div><b>Tout mélanger</b><small>Toutes les matières</small></div></a>';
      COURS.forEach(function (s) { h += '<a class="row" href="#/challenge/' + s.id + '"><span class="n">' + s.emoji + "</span><div><b>" + esc(s.name) + "</b></div></a>"; });
      view.innerHTML = h + "</div>"; bindChips(); return;
    }
    var pool = [];
    ALL.forEach(function (c) {
      if (!inYear(c) || (sid !== "all" && c.subject.id !== sid)) return;
      c.quiz.forEach(function (q) { var x = q.slice(); x.src = c.t; pool.push(x); });
    });
    if (!pool.length) { view.innerHTML = '<div class="card">Aucune question pour ce filtre.</div>'; return; }
    runQuiz(shuffle(pool).slice(0, 10), "Défi");
  }

  // ---------- Fiches (flashcards) ----------
  function cards(key) {
    var list = [], label;
    if (key && BYID[key]) { list = BYID[key].cards.map(function (x) { return [x[0], x[1], BYID[key].t]; }); label = BYID[key].t; }
    else if (key && key.indexOf("s:") === 0) {
      var sid = key.slice(2);
      ALL.forEach(function (c) { if (c.subject.id === sid && inYear(c)) c.cards.forEach(function (x) { list.push([x[0], x[1], c.t]); }); });
      label = "Fiches";
    }
    if (!key) {
      setHeader("Fiches de révision", false, "cards");
      var h = '<div class="card"><b>🃏 Fiches mémo</b><p class="muted" style="margin:4px 0 0">Lis la question, essaie de répondre dans ta tête, puis touche la carte pour voir la réponse. Idéal 10 min par jour !</p></div>' + yearChips() + '<div class="list">';
      COURS.forEach(function (s) {
        var n = 0; s.chapters.forEach(function (c) { if (inYear(c)) n += c.cards.length; });
        if (n) h += '<a class="row" href="#/cards/s:' + s.id + '"><span class="n">' + s.emoji + "</span><div><b>" + esc(s.name) + "</b><small>" + n + " fiches</small></div><span>›</span></a>";
      });
      view.innerHTML = h + "</div>"; bindChips(); return;
    }
    setHeader(label, true, "cards");
    list = shuffle(list);
    var i = 0, flipped = false;
    function show() {
      var c = list[i];
      view.innerHTML = '<p class="muted" style="text-align:center">Fiche ' + (i + 1) + " / " + list.length + " · " + esc(c[2]) + "</p>" +
        '<div class="flash' + (flipped ? " back" : "") + '" id="fl"><div>' + (flipped ? c[1] : "<b>" + c[0] + "</b><br><small class=\"muted\">(touche pour retourner)</small>") + "</div></div>" +
        '<div class="btns"><button class="btn alt" id="pv">‹ Précédente</button><button class="btn" id="nx">Suivante ›</button></div>';
      $("#fl").onclick = function () { flipped = !flipped; show(); };
      $("#pv").onclick = function () { i = (i - 1 + list.length) % list.length; flipped = false; show(); };
      $("#nx").onclick = function () { i = (i + 1) % list.length; flipped = false; show(); };
    }
    if (list.length) show(); else view.innerHTML = '<div class="card">Pas de fiche ici.</div>';
  }

  // ---------- Recherche ----------
  var TEXT = null;
  function search() {
    setHeader("Rechercher", false, "search");
    if (!TEXT) { TEXT = {}; ALL.forEach(function (c) { TEXT[c.key] = norm(c.t + " " + plain(c.html)); }); }
    view.innerHTML = '<input class="search" id="sq" placeholder="Ex : dérivée, Grafcet, triphasé, AMDEC…" autocomplete="off"><div id="res" class="list" style="margin-top:12px"></div>';
    var inp = $("#sq");
    inp.value = sessionStorage.getItem("sq") || "";
    function go() {
      var q = norm(inp.value.trim()); sessionStorage.setItem("sq", inp.value);
      if (q.length < 2) { $("#res").innerHTML = '<p class="muted">Tape au moins 2 lettres. La recherche fouille les titres et tout le texte des cours.</p>'; return; }
      var words = q.split(/\s+/), out = [];
      ALL.forEach(function (c) {
        var t = TEXT[c.key]; if (!words.every(function (w) { return t.indexOf(w) >= 0; })) return;
        var score = norm(c.t).indexOf(words[0]) >= 0 ? 0 : 1;
        var p = t.indexOf(words[0]), snip = t.substr(Math.max(0, p - 40), 120);
        out.push([score, c, snip]);
      });
      out.sort(function (a, b) { return a[0] - b[0]; });
      $("#res").innerHTML = out.length ? out.slice(0, 60).map(function (o) {
        return '<a class="row" href="#/c/' + o[1].key + '"><span class="n">' + o[1].subject.emoji + "</span><div><b>" + esc(o[1].t) + "</b><small>…" + esc(o[2]).replace(new RegExp(words[0].replace(/[.*+?^${}()|[\]\\]/g, "\\$&"), "g"), "<mark>$&</mark>") + "…</small></div></a>";
      }).join("") : '<p class="muted">Aucun résultat.</p>';
    }
    inp.oninput = go; go();
  }

  // ---------- Plus ----------
  function more() {
    setHeader("Plus", false, "more");
    var d = countDone(ALL), qs = Object.keys(store.quiz), avg = qs.length ? Math.round(qs.reduce(function (a, k) { return a + store.quiz[k]; }, 0) / qs.length) : 0;
    var nq = 0, nc = 0; ALL.forEach(function (c) { nq += c.quiz.length; nc += c.cards.length; });
    var h = '<div class="stat"><div><b>' + d + "</b><small>chapitres appris</small></div><div><b>" + qs.length + "</b><small>quiz faits</small></div><div><b>" + avg + ' %</b><small>score moyen</small></div></div>';
    h += '<p class="muted" style="font-size:13px">Contenu : ' + COURS.length + " matières, " + ALL.length + " chapitres, " + nq + " questions de quiz, " + nc + " fiches.</p>";
    h += '<h2 class="sec">⭐ Mes favoris</h2><div class="list">';
    var favs = Object.keys(store.fav).filter(function (k) { return BYID[k]; });
    h += favs.length ? favs.map(function (k) { return '<a class="row" href="#/c/' + k + '"><span class="n">' + BYID[k].subject.emoji + "</span><div><b>" + esc(BYID[k].t) + "</b></div></a>"; }).join("") : '<p class="muted">Touche ☆ dans un chapitre pour l\'ajouter ici.</p>';
    h += '</div><h2 class="sec">🔁 À retravailler (quiz &lt; 60 %)</h2><div class="list">';
    var weak = qs.filter(function (k) { return store.quiz[k] < 60 && BYID[k]; });
    h += weak.length ? weak.map(function (k) { return '<a class="row" href="#/c/' + k + '"><span class="n">' + store.quiz[k] + "%</span><div><b>" + esc(BYID[k].t) + "</b></div></a>"; }).join("") : '<p class="muted">Rien pour l\'instant 👍</p>';
    h += '</div><h2 class="sec">⚙️ Réglages</h2><div class="btns"><button class="btn alt" id="reset">Remettre la progression à zéro</button></div>';
    h += '<div class="card"><small class="muted">BTS ATI Cours v1.0 — application de révision hors ligne. Contenu pédagogique rédigé pour être simple : vérifie toujours le programme et les consignes officielles de ton établissement et du référentiel en vigueur.</small></div>';
    view.innerHTML = h;
    $("#reset").onclick = function () { if (confirm("Effacer toute la progression ?")) { store.done = {}; store.quiz = {}; store.fav = {}; store.last = null; save(); more(); } };
  }

  // ---------- Routeur ----------
  function route() {
    var h = decodeURIComponent(location.hash.replace(/^#\/?/, "")), p = h.split("/");
    if (p[0] === "s") subject(p[1]);
    else if (p[0] === "c") chapter(p[1] + "/" + p[2]);
    else if (p[0] === "q") quiz(p[1] + "/" + p[2]);
    else if (p[0] === "cards") cards(p[1] ? p.slice(1).join("/") : null);
    else if (p[0] === "challenge") challenge(p[1]);
    else if (p[0] === "search") search();
    else if (p[0] === "more") more();
    else home();
  }
  window.addEventListener("hashchange", route);
  route();
})();
