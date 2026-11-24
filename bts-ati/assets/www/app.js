(function () {
  "use strict";
  var $ = function (s) { return document.querySelector(s); };
  var view = $("#view"), titleEl = $("#title"), backBtn = $("#back");
  var timer = null;

  // ---------- Stockage local ----------
  var store = { done: {}, fav: {}, quiz: {}, last: null, year: 0, theme: "", box: {}, streak: { last: -1, n: 0, best: 0 }, act: {}, zoom: 1, exams: [], goal: 15 };
  try { var raw = localStorage.getItem("btsati"); if (raw) store = Object.assign(store, JSON.parse(raw)); } catch (e) {}
  function save() { try { localStorage.setItem("btsati", JSON.stringify(store)); } catch (e) {} }
  function today() { var d = new Date(); return Math.floor((d.getTime() - d.getTimezoneOffset() * 60000) / 864e5); }
  function activity(n) {
    var t = today(), s = store.streak;
    if (s.last !== t) { s.n = s.last === t - 1 ? s.n + 1 : 1; s.last = t; s.best = Math.max(s.best || 0, s.n); }
    store.act[t] = (store.act[t] || 0) + (n || 1);
    Object.keys(store.act).forEach(function (k) { if (+k < t - 60) delete store.act[k]; });
    save();
  }
  function streakNow() { var s = store.streak; return s.last >= today() - 1 ? s.n : 0; }

  // ---------- Index ----------
  var ALL = [], BYID = {}, CARDS = [];
  COURS.forEach(function (s) {
    s.chapters.forEach(function (c, i) {
      c.subject = s; c.index = i; c.key = s.id + "/" + c.id;
      c.cards = c.cards || []; c.quiz = c.quiz || [];
      c.exos = (window.EXOS && EXOS[c.key]) || c.exos || [];
      ALL.push(c); BYID[c.key] = c;
      c.cards.forEach(function (x, j) { CARDS.push({ id: c.key + "#" + j, f: x[0], b: x[1], c: c }); });
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
  function subjById(id) { return COURS.filter(function (x) { return x.id === id; })[0]; }

  // ---------- Thème / zoom ----------
  function applyTheme() {
    var t = store.theme || (window.matchMedia && matchMedia("(prefers-color-scheme: dark)").matches ? "dark" : "light");
    document.documentElement.setAttribute("data-theme", t);
    view.style.zoom = store.zoom || 1;
  }
  backBtn.onclick = function () { history.back(); };
  $("#searchbtn").onclick = function () { location.hash = "#/search"; };
  applyTheme();

  function setHeader(t, canBack, tab) {
    titleEl.textContent = t;
    backBtn.style.visibility = canBack ? "visible" : "hidden";
    document.querySelectorAll("#tabs a").forEach(function (a) { a.classList.toggle("on", a.getAttribute("data-tab") === tab); });
    window.scrollTo(0, 0);
  }
  function yearChips() {
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
  function row(href, n, title, sub, cls) {
    return '<a class="row" href="' + href + '"><span class="n' + (cls ? " " + cls : "") + '">' + n + "</span><div><b>" + title + "</b>" + (sub ? "<small>" + sub + "</small>" : "") + "</div><span>›</span></a>";
  }

  // ---------- Accueil ----------
  function dueCards() {
    var t = today();
    return CARDS.filter(function (k) { var b = store.box[k.id]; return b && b.d <= t && inYear(k.c); });
  }
  function home() {
    setHeader("BTS ATI — Cours", false, "home");
    var list = ALL.filter(inYear), d = countDone(list), st = streakNow(), act = store.act[today()] || 0, goal = store.goal || 15;
    var h = '<div class="hero"><h2>Salut 👋 Prêt à réviser ?</h2><p>Tous les cours du BTS ATI, 1ʳᵉ et 2ᵉ année, expliqués simplement. Fonctionne sans Internet.</p>' +
      '<div class="bar"><i style="width:' + pct(d, list.length) + '%"></i></div><p style="margin-top:6px;font-size:13px">' + d + " / " + list.length + " chapitres appris (" + pct(d, list.length) + " %)</p>" +
      '<div class="herostats"><span>🔥 <b>' + st + "</b> jour" + (st > 1 ? "s" : "") + " de suite</span><span>🎯 Objectif du jour : <b>" + Math.min(act, goal) + "/" + goal + "</b></span></div>" +
      '<div class="bar"><i style="width:' + pct(Math.min(act, goal), goal) + '%;background:#34D399"></i></div></div>';
    if (store.last && BYID[store.last]) {
      var l = BYID[store.last];
      h += row("#/c/" + l.key, "▶", esc(l.t), "Reprendre · " + l.subject.emoji + " " + esc(l.subject.name));
    }
    var due = dueCards().length;
    h += '<div class="tiles">' +
      '<a href="#/review"><span>🔁</span><b>Révision du jour</b><small>' + (due ? due + " fiche" + (due > 1 ? "s" : "") + " à revoir" : "Nouvelles fiches") + "</small></a>" +
      '<a href="#/exam"><span>📝</span><b>Examen blanc</b><small>Chrono + note /20</small></a>' +
      '<a href="#/exos"><span>✍️</span><b>Exercices corrigés</b><small>Pas à pas</small></a>' +
      '<a href="#/tools"><span>🧮</span><b>Calculatrices</b><small>Calculs techniques</small></a>' +
      '<a href="#/formules"><span>📋</span><b>Formulaire</b><small>Toutes les formules</small></a>' +
      '<a href="#/challenge"><span>🎯</span><b>Quiz défi</b><small>10 questions</small></a></div>';
    h += '<h2 class="sec">📖 Les cours</h2>' + yearChips() + '<div class="grid">';
    COURS.forEach(function (s) {
      var ch = s.chapters.filter(inYear); if (!ch.length) return;
      var dd = countDone(ch);
      h += '<a class="subj" style="--c:' + s.color + '" href="#/s/' + s.id + '"><span class="em">' + s.emoji + "</span><b>" + esc(s.name) + "</b><small>" + ch.length + " chapitres · " + pct(dd, ch.length) + " %</small>" +
        '<div class="bar"><i style="width:' + pct(dd, ch.length) + "%;background:" + s.color + '"></i></div></a>';
    });
    view.innerHTML = h + "</div>"; bindChips();
  }

  function subject(id) {
    var s = subjById(id); if (!s) return home();
    setHeader(s.emoji + " " + s.name, true, "home");
    var h = '<div class="card"><p style="margin:0">' + s.desc + "</p></div>" + yearChips() + '<div class="list">';
    s.chapters.forEach(function (c, i) {
      if (!inYear(c)) return;
      var q = store.quiz[c.key], dn = store.done[c.key];
      var sub = yearTag(c.y) + (c.quiz.length ? "Quiz : " + (q != null ? q + " %" : "à faire") : "") + (c.exos.length ? " · ✍️ " + c.exos.length + " exo" + (c.exos.length > 1 ? "s" : "") : "");
      h += row("#/c/" + c.key, dn ? "✓" : i + 1, esc(c.t), sub, dn ? "done" : "");
    });
    view.innerHTML = h + "</div>"; bindChips();
  }

  function exoHtml(e, i) {
    return '<div class="exo"><b class="exot">Exercice ' + (i + 1) + (e.t ? " — " + e.t : "") + "</b>" + (e.lvl ? ' <span class="tag">' + "★".repeat(e.lvl) + "</span>" : "") + "<div>" + e.q + '</div><details><summary>Voir la correction</summary><div class="sol">' + e.s + "</div></details></div>";
  }

  function chapter(key) {
    var c = BYID[key]; if (!c) return home();
    store.last = key; save();
    setHeader(c.t, true, "home");
    var s = c.subject, prev = s.chapters[c.index - 1], next = s.chapters[c.index + 1];
    var h = '<div style="display:flex;align-items:center;gap:8px;margin-bottom:10px"><div style="flex:1"><small class="muted">' + s.emoji + " " + esc(s.name) + "</small><br>" + yearTag(c.y) +
      '</div><button class="fav" id="fav">' + (store.fav[key] ? "★" : "☆") + "</button></div>";
    h += '<article class="course">' + c.html + "</article>";
    if (c.exos.length) h += '<h2 class="sec">✍️ Exercices corrigés</h2>' + c.exos.map(exoHtml).join("");
    h += '<div class="btns"><button class="btn ' + (store.done[key] ? "ok" : "alt") + '" id="done">' + (store.done[key] ? "✓ Chapitre appris" : "Marquer comme appris") + "</button>";
    if (c.quiz.length) h += '<a class="btn" href="#/q/' + key + '">🎯 Quiz (' + c.quiz.length + ")</a>";
    if (c.cards.length) h += '<a class="btn alt" href="#/cards/' + key + '">🃏 Fiches (' + c.cards.length + ")</a>";
    h += '</div><div class="btns">';
    if (prev) h += '<a class="btn alt" href="#/c/' + prev.key + '">‹ ' + esc(prev.t) + "</a>";
    if (next) h += '<a class="btn alt" href="#/c/' + next.key + '">' + esc(next.t) + " ›</a>";
    view.innerHTML = h + "</div>";
    $("#fav").onclick = function () { if (store.fav[key]) delete store.fav[key]; else store.fav[key] = 1; save(); this.textContent = store.fav[key] ? "★" : "☆"; };
    $("#done").onclick = function () {
      if (store.done[key]) delete store.done[key]; else { store.done[key] = 1; activity(3); } save();
      this.className = "btn " + (store.done[key] ? "ok" : "alt"); this.textContent = store.done[key] ? "✓ Chapitre appris" : "Marquer comme appris";
    };
  }

  // ---------- Quiz ----------
  function runQuiz(questions, onEnd) {
    var i = 0, score = 0;
    function show() {
      if (i >= questions.length) return end();
      var q = questions[i], order = shuffle(q[1].map(function (_, k) { return k; }));
      var h = '<div class="card"><small class="muted">Question ' + (i + 1) + " / " + questions.length + (q.src ? " · " + esc(q.src) : "") +
        '</small><div class="bar"><i style="width:' + pct(i, questions.length) + '%"></i></div></div><div class="card"><p class="q">' + q[0] + "</p>";
      order.forEach(function (k) { h += '<button class="opt" data-k="' + k + '">' + q[1][k] + "</button>"; });
      view.innerHTML = h + '<div id="ex"></div></div>';
      view.querySelectorAll(".opt").forEach(function (b) {
        b.onclick = function () {
          if (view.querySelector(".opt.good")) return;
          var k = +b.getAttribute("data-k"), ok = k === q[2];
          if (ok) score++;
          activity(1);
          view.querySelectorAll(".opt").forEach(function (o) { if (+o.getAttribute("data-k") === q[2]) o.classList.add("good"); });
          if (!ok) b.classList.add("bad");
          $("#ex").innerHTML = '<div class="box ' + (ok ? "ex" : "warn") + ' expl"><b>' + (ok ? "Bonne réponse ! ✅" : "Pas tout à fait ❌") + "</b><br>" + (q[3] || "") + '</div><div class="btns"><button class="btn" id="nx">' + (i + 1 < questions.length ? "Question suivante ›" : "Voir mon score") + "</button></div>";
          $("#nx").onclick = function () { i++; show(); };
          $("#nx").scrollIntoView({ block: "nearest" });
        };
      });
      window.scrollTo(0, 0);
    }
    function end() {
      var p = pct(score, questions.length);
      if (onEnd) onEnd(p);
      var msg = p >= 80 ? "Excellent ! Tu maîtrises 🏆" : p >= 50 ? "Pas mal ! Relis les points ratés 💪" : "Relis le cours puis recommence, tu vas y arriver 📚";
      view.innerHTML = '<div class="card" style="text-align:center"><div class="big">' + (p >= 80 ? "🏆" : p >= 50 ? "👍" : "📚") + "</div><h2>" + score + " / " + questions.length + " (" + p + " %)</h2><p>" + msg +
        '</p><div class="btns"><button class="btn" id="again">Recommencer</button><button class="btn alt" id="bk">Retour</button></div></div>';
      $("#again").onclick = function () { runQuiz(shuffle(questions), onEnd); };
      $("#bk").onclick = function () { history.back(); };
    }
    show();
  }
  function quiz(key) {
    var c = BYID[key]; if (!c) return home();
    setHeader("Quiz : " + c.t, true, "challenge");
    runQuiz(shuffle(c.quiz), function (p) { store.quiz[key] = Math.max(p, store.quiz[key] || 0); if (p >= 80) store.done[key] = 1; save(); });
  }
  function pool(sid) {
    var out = [];
    ALL.forEach(function (c) {
      if (!inYear(c) || (sid && sid !== "all" && c.subject.id !== sid)) return;
      c.quiz.forEach(function (q) { var x = q.slice(); x.src = c.t; x.key = c.key; out.push(x); });
    });
    return out;
  }
  function challenge(sid) {
    setHeader("Quiz défi", sid != null, "challenge");
    if (sid == null) {
      var h = '<div class="card"><b>🎯 Quiz défi</b><p class="muted" style="margin:4px 0 0">10 questions tirées au hasard, avec correction immédiate. Pour t\'entraîner en conditions d\'examen, essaie plutôt l\'<a href="#/exam">examen blanc</a>.</p></div>' + yearChips() + '<div class="list">';
      h += row("#/challenge/all", "🔀", "Tout mélanger", "Toutes les matières");
      COURS.forEach(function (s) { h += row("#/challenge/" + s.id, s.emoji, esc(s.name), pool(s.id).length + " questions"); });
      view.innerHTML = h + "</div>"; bindChips(); return;
    }
    var p = pool(sid);
    if (!p.length) { view.innerHTML = '<div class="card">Aucune question pour ce filtre.</div>'; return; }
    runQuiz(shuffle(p).slice(0, 10));
  }

  // ---------- Examen blanc ----------
  function exam(sid) {
    setHeader("Examen blanc", sid != null, "challenge");
    if (sid == null) {
      var h = '<div class="card"><b>📝 Examen blanc</b><p class="muted" style="margin:4px 0 0">20 questions, 30 minutes, <b>pas de correction pendant l\'épreuve</b> : comme le jour J. À la fin : ta note sur 20 et la correction détaillée de tes erreurs.</p></div>' + yearChips() + '<div class="list">';
      h += row("#/exam/all", "🔀", "Toutes les matières", "Sujet mixte");
      COURS.forEach(function (s) { var n = pool(s.id).length; if (n >= 10) h += row("#/exam/" + s.id, s.emoji, esc(s.name), Math.min(20, n) + " questions"); });
      h += "</div>";
      if (store.exams.length) {
        h += '<h2 class="sec">📈 Mes derniers examens</h2><div class="list">' + store.exams.slice(-8).reverse().map(function (e) {
          var s = subjById(e.s); return '<div class="row"><span class="n">' + e.note + '</span><div><b>' + (s ? s.emoji + " " + esc(s.name) : "🔀 Mixte") + "</b><small>" + e.date + " · " + e.note + "/20</small></div></div>";
        }).join("") + "</div>";
      }
      view.innerHTML = h; bindChips(); return;
    }
    var qs = shuffle(pool(sid)).slice(0, 20); if (!qs.length) return;
    var answers = [], i = 0, end = Date.now() + qs.length * 90 * 1000;
    function tick() {
      var r = Math.max(0, Math.round((end - Date.now()) / 1000)), el = $("#chrono");
      if (el) { el.textContent = "⏱ " + Math.floor(r / 60) + ":" + ("0" + (r % 60)).slice(-2); el.classList.toggle("late", r < 120); }
      if (r <= 0) finish();
    }
    function show() {
      if (i >= qs.length) return finish();
      var q = qs[i], order = q.order || (q.order = shuffle(q[1].map(function (_, k) { return k; })));
      var h = '<div class="card examhead"><span>Question ' + (i + 1) + " / " + qs.length + '</span><b id="chrono"></b></div><div class="bar" style="margin:-4px 0 12px"><i style="width:' + pct(i, qs.length) + '%"></i></div><div class="card"><p class="q">' + q[0] + "</p>";
      order.forEach(function (k) { h += '<button class="opt" data-k="' + k + '">' + q[1][k] + "</button>"; });
      h += '<div class="btns"><button class="btn alt" id="skip">Passer</button><button class="btn alt" id="stop">Terminer</button></div></div>';
      view.innerHTML = h; tick(); window.scrollTo(0, 0);
      view.querySelectorAll(".opt").forEach(function (b) { b.onclick = function () { answers[i] = +b.getAttribute("data-k"); i++; show(); }; });
      $("#skip").onclick = function () { answers[i] = -1; i++; show(); };
      $("#stop").onclick = function () { if (confirm("Terminer l'examen maintenant ?")) finish(); };
    }
    var finished = false;
    function finish() {
      if (finished) return; finished = true; clearInterval(timer);
      var good = 0; qs.forEach(function (q, k) { if (answers[k] === q[2]) good++; });
      var note = Math.round(20 * good / qs.length * 10) / 10, d = new Date();
      store.exams.push({ s: sid, note: note, date: d.toLocaleDateString("fr-FR") }); store.exams = store.exams.slice(-30); activity(5);
      var h = '<div class="card" style="text-align:center"><div class="big">' + (note >= 14 ? "🏆" : note >= 10 ? "✅" : "📚") + "</div><h2>" + note + " / 20</h2><p>" + good + " bonnes réponses sur " + qs.length + ". " +
        (note >= 14 ? "Très bon niveau !" : note >= 10 ? "C'est la moyenne, continue !" : "Il faut retravailler : regarde la correction ci-dessous.") + "</p></div>";
      var wrong = qs.map(function (q, k) { return [q, answers[k]]; }).filter(function (x) { return x[1] !== x[0][2]; });
      if (wrong.length) {
        h += '<h2 class="sec">🔍 Correction de tes erreurs</h2>';
        wrong.forEach(function (x) {
          var q = x[0];
          h += '<div class="card"><p class="q" style="font-size:15px">' + q[0] + "</p>" + (x[1] >= 0 && x[1] !== undefined ? '<p class="badans">✗ Ta réponse : ' + q[1][x[1]] + "</p>" : '<p class="badans">✗ Pas de réponse</p>') +
            '<p class="goodans">✓ Bonne réponse : ' + q[1][q[2]] + '</p><p class="muted" style="font-size:14px">' + (q[3] || "") + '</p><a href="#/c/' + q.key + '">Revoir le cours : ' + esc(q.src) + " ›</a></div>";
        });
      }
      view.innerHTML = h + '<div class="btns"><a class="btn" href="#/exam">Nouvel examen</a></div>'; window.scrollTo(0, 0);
    }
    show(); timer = setInterval(tick, 1000);
  }

  // ---------- Fiches & révision espacée ----------
  var GAPS = [1, 2, 4, 7, 15, 30];
  function grade(card, ok) {
    var b = store.box[card.id] || { b: 0, d: 0 };
    b.b = ok ? Math.min((b.b || 0) + 1, 5) : 0; b.d = today() + (ok ? GAPS[b.b] : 0);
    store.box[card.id] = b; activity(1);
  }
  function flash(list, label, emptyMsg) {
    var i = 0, flipped = false, known = 0;
    function show() {
      if (i >= list.length) {
        view.innerHTML = '<div class="card" style="text-align:center"><div class="big">🎉</div><h2>Session terminée</h2><p>' + known + " / " + list.length + ' fiches sues. Les fiches ratées reviendront plus tôt, les fiches sues de plus en plus tard : c\'est la répétition espacée.</p><div class="btns"><a class="btn" href="#/cards">Retour aux fiches</a></div></div>';
        return;
      }
      var c = list[i], b = store.box[c.id];
      view.innerHTML = '<p class="muted" style="text-align:center">Fiche ' + (i + 1) + " / " + list.length + " · " + c.c.subject.emoji + " " + esc(c.c.t) + (b ? " · niveau " + b.b + "/5" : " · nouvelle") + "</p>" +
        '<div class="flash' + (flipped ? " back" : "") + '" id="fl"><div>' + (flipped ? "<small class=\"muted\">" + c.f + "</small><br><br><b>" + c.b + "</b>" : "<b>" + c.f + "</b><br><small class=\"muted\">(touche pour voir la réponse)</small>") + "</div></div>" +
        (flipped ? '<div class="btns"><button class="btn no" id="ko">😕 À revoir</button><button class="btn ok" id="okb">🙂 Je savais</button></div>' : '<div class="btns"><button class="btn" id="turn">Voir la réponse</button></div>');
      $("#fl").onclick = function () { flipped = !flipped; show(); };
      if (flipped) {
        $("#ko").onclick = function () { grade(c, false); save(); if (list.indexOf(c, i + 1) < 0 && list.length - i < 40) list.push(c); i++; flipped = false; show(); };
        $("#okb").onclick = function () { grade(c, true); known++; save(); i++; flipped = false; show(); };
      } else $("#turn").onclick = function () { flipped = true; show(); };
    }
    if (list.length) show(); else view.innerHTML = '<div class="card">' + (emptyMsg || "Pas de fiche ici.") + "</div>";
  }
  function review() {
    setHeader("Révision du jour", true, "cards");
    var due = shuffle(dueCards());
    var fresh = shuffle(CARDS.filter(function (k) { return !store.box[k.id] && inYear(k.c); }));
    var learnedFirst = fresh.filter(function (k) { return store.done[k.c.key]; });
    var others = fresh.filter(function (k) { return !store.done[k.c.key]; });
    var list = due.slice(0, 25).concat(learnedFirst.concat(others).slice(0, Math.max(5, 20 - due.length)));
    flash(list, "Révision du jour");
  }
  function cards(key) {
    if (!key) {
      setHeader("Fiches de révision", false, "cards");
      var due = dueCards().length, seen = Object.keys(store.box).length, mast = Object.keys(store.box).filter(function (k) { return store.box[k].b >= 3; }).length;
      var h = '<a class="hero" style="display:block;text-decoration:none" href="#/review"><h2>🔁 Révision du jour</h2><p>' + (due ? due + " fiche" + (due > 1 ? "s" : "") + " à revoir aujourd'hui + des nouvelles." : "Aucune fiche en retard : découvre de nouvelles fiches !") + '</p><p style="margin-top:8px"><b>Commencer ›</b></p></a>';
      h += '<div class="stat"><div><b>' + seen + "</b><small>fiches vues</small></div><div><b>" + mast + "</b><small>fiches maîtrisées</small></div><div><b>" + CARDS.length + "</b><small>fiches au total</small></div></div>";
      h += '<h2 class="sec">Par matière</h2>' + yearChips() + '<div class="list">';
      COURS.forEach(function (s) {
        var n = CARDS.filter(function (k) { return k.c.subject === s && inYear(k.c); }).length;
        if (n) h += row("#/cards/s:" + s.id, s.emoji, esc(s.name), n + " fiches");
      });
      view.innerHTML = h + "</div>"; bindChips(); return;
    }
    var list;
    if (BYID[key]) { list = CARDS.filter(function (k) { return k.c.key === key; }); setHeader("Fiches : " + BYID[key].t, true, "cards"); }
    else { var sid = key.slice(2), s = subjById(sid); list = CARDS.filter(function (k) { return k.c.subject.id === sid && inYear(k.c); }); setHeader(s ? "Fiches : " + s.name : "Fiches", true, "cards"); }
    flash(shuffle(list));
  }

  // ---------- Exercices ----------
  function exos(sid) {
    setHeader("Exercices corrigés", sid != null, "home");
    if (sid == null) {
      var h = '<div class="card"><b>✍️ Exercices corrigés</b><p class="muted" style="margin:4px 0 0">Cherche d\'abord seul(e) avec un brouillon, puis ouvre la correction détaillée étape par étape. ★ = facile, ★★ = moyen, ★★★ = type examen.</p></div><div class="list">';
      COURS.forEach(function (s) {
        var n = 0; s.chapters.forEach(function (c) { n += c.exos.length; });
        if (n) h += row("#/exos/" + s.id, s.emoji, esc(s.name), n + " exercices");
      });
      view.innerHTML = h + "</div>"; return;
    }
    var s = subjById(sid); if (!s) return;
    setHeader("Exercices : " + s.name, true, "home");
    var h2 = "";
    s.chapters.forEach(function (c) {
      if (!c.exos.length) return;
      h2 += '<h2 class="sec"><a href="#/c/' + c.key + '">' + esc(c.t) + " ›</a></h2>" + c.exos.map(exoHtml).join("");
    });
    view.innerHTML = h2;
  }

  // ---------- Formulaire ----------
  var FCACHE = {};
  function formules(sid) {
    setHeader("Formulaire", sid != null, "home");
    if (sid == null) {
      var h = '<div class="card"><b>📋 Formulaire</b><p class="muted" style="margin:4px 0 0">Toutes les formules des cours, regroupées par matière. Idéal juste avant un devoir.</p></div><div class="list">';
      COURS.forEach(function (s) { h += row("#/formules/" + s.id, s.emoji, esc(s.name), ""); });
      view.innerHTML = h + "</div>"; return;
    }
    var s = subjById(sid); if (!s) return;
    setHeader("Formules : " + s.name, true, "home");
    if (!FCACHE[sid]) {
      var out = "", tmp = document.createElement("div");
      s.chapters.forEach(function (c) {
        tmp.innerHTML = c.html;
        var fs = tmp.querySelectorAll(".f"); if (!fs.length) return;
        out += '<h2 class="sec"><a href="#/c/' + c.key + '">' + esc(c.t) + " ›</a></h2>";
        fs.forEach(function (f) { out += f.outerHTML; });
      });
      FCACHE[sid] = out || '<div class="card">Pas de formule dans cette matière.</div>';
    }
    view.innerHTML = '<div class="formules">' + FCACHE[sid] + "</div>";
  }

  // ---------- Calculatrices ----------
  function tools(id) {
    var T = window.TOOLS || [];
    if (!id) {
      setHeader("Calculatrices", false, "tools");
      var h = '<div class="card"><b>🧮 Calculatrices techniques</b><p class="muted" style="margin:4px 0 0">Pour vérifier tes exercices. Chaque outil rappelle la formule utilisée : au BTS, il faut toujours écrire la formule avant le résultat !</p></div><div class="list">';
      T.forEach(function (t) { h += row("#/tools/" + t.id, t.emoji, esc(t.name), esc(t.desc)); });
      view.innerHTML = h + "</div>"; return;
    }
    var t = T.filter(function (x) { return x.id === id; })[0]; if (!t) return tools();
    setHeader(t.name, true, "tools");
    var h = '<div class="card"><p style="margin:0 0 10px">' + t.desc + "</p>";
    t.inputs.forEach(function (inp) {
      h += '<label class="field"><span>' + inp[1] + (inp[3] ? " <small>(" + inp[3] + ")</small>" : "") + '</span><input inputmode="decimal" data-k="' + inp[0] + '" value="' + inp[2] + '"></label>';
    });
    h += '</div><div class="card" id="out"></div>' + (t.formula ? '<div class="f">' + t.formula + "</div>" : "") + (t.note ? '<div class="box tip">' + t.note + "</div>" : "");
    view.innerHTML = h;
    function run() {
      var v = {};
      view.querySelectorAll("input[data-k]").forEach(function (i) { var x = i.value.trim().replace(",", "."); v[i.getAttribute("data-k")] = x === "" ? NaN : +x; });
      var r; try { r = t.calc(v); } catch (e) { r = "Valeurs invalides"; }
      $("#out").innerHTML = typeof r === "string" ? r : '<table class="res">' + r.map(function (x) { return "<tr><td>" + x[0] + "</td><td><b>" + fmt(x[1]) + "</b> " + (x[2] || "") + "</td></tr>"; }).join("") + "</table>";
    }
    view.querySelectorAll("input").forEach(function (i) { i.oninput = run; });
    run();
  }
  function fmt(x) {
    if (typeof x !== "number") return x;
    if (!isFinite(x)) return "—";
    var a = Math.abs(x);
    if (a !== 0 && (a < 0.001 || a >= 1e7)) return x.toExponential(3).replace(".", ",").replace("e", " × 10^");
    return (+x.toPrecision(4)).toLocaleString("fr-FR", { maximumFractionDigits: 4 });
  }
  window.fmtNum = fmt;

  // ---------- Recherche ----------
  var TEXT = null;
  function search() {
    setHeader("Rechercher", true, "");
    if (!TEXT) { TEXT = {}; ALL.forEach(function (c) { TEXT[c.key] = norm(c.t + " " + plain(c.html) + " " + c.exos.map(function (e) { return plain(e.q); }).join(" ")); }); }
    view.innerHTML = '<input class="search" id="sq" placeholder="Ex : dérivée, Grafcet, triphasé, AMDEC…" autocomplete="off"><div id="res" class="list" style="margin-top:12px"></div>';
    var inp = $("#sq");
    try { inp.value = sessionStorage.getItem("sq") || ""; } catch (e) {}
    function go() {
      var q = norm(inp.value.trim()); try { sessionStorage.setItem("sq", inp.value); } catch (e) {}
      if (q.length < 2) { $("#res").innerHTML = '<p class="muted">Tape au moins 2 lettres. La recherche fouille les titres, les cours et les exercices.</p>'; return; }
      var words = q.split(/\s+/), out = [];
      ALL.forEach(function (c) {
        var t = TEXT[c.key]; if (!words.every(function (w) { return t.indexOf(w) >= 0; })) return;
        var p = t.indexOf(words[0]);
        out.push([norm(c.t).indexOf(words[0]) >= 0 ? 0 : 1, c, t.substr(Math.max(0, p - 40), 120)]);
      });
      out.sort(function (a, b) { return a[0] - b[0]; });
      var re = new RegExp(words[0].replace(/[.*+?^${}()|[\]\\]/g, "\\$&"), "g");
      $("#res").innerHTML = out.length ? out.slice(0, 60).map(function (o) {
        return '<a class="row" href="#/c/' + o[1].key + '"><span class="n">' + o[1].subject.emoji + "</span><div><b>" + esc(o[1].t) + "</b><small>…" + esc(o[2]).replace(re, "<mark>$&</mark>") + "…</small></div></a>";
      }).join("") : '<p class="muted">Aucun résultat.</p>';
    }
    inp.oninput = go; go(); inp.focus();
  }

  // ---------- Plus ----------
  function more() {
    setHeader("Plus", false, "more");
    var d = countDone(ALL), qs = Object.keys(store.quiz), avg = qs.length ? Math.round(qs.reduce(function (a, k) { return a + store.quiz[k]; }, 0) / qs.length) : 0;
    var nq = 0, ne = 0; ALL.forEach(function (c) { nq += c.quiz.length; ne += c.exos.length; });
    var h = '<div class="stat"><div><b>' + d + "</b><small>chapitres appris</small></div><div><b>" + qs.length + "</b><small>quiz faits</small></div><div><b>" + avg + " %</b><small>score moyen</small></div>" +
      "<div><b>🔥 " + streakNow() + "</b><small>jours de suite</small></div><div><b>" + (store.streak.best || 0) + "</b><small>record (jours)</small></div><div><b>" + Object.keys(store.box).length + "</b><small>fiches vues</small></div></div>";
    // activité 14 jours
    var t = today(), max = 1, days = [];
    for (var k = 13; k >= 0; k--) { var v = store.act[t - k] || 0; days.push(v); max = Math.max(max, v); }
    h += '<div class="card"><b>Activité des 14 derniers jours</b><div class="spark">' + days.map(function (v) { return '<i style="height:' + Math.max(4, Math.round(60 * v / max)) + 'px" class="' + (v ? "on" : "") + '"></i>'; }).join("") + "</div></div>";
    h += '<p class="muted" style="font-size:13px">Contenu : ' + COURS.length + " matières, " + ALL.length + " chapitres, " + nq + " questions, " + CARDS.length + " fiches, " + ne + " exercices corrigés, " + (window.TOOLS || []).length + " calculatrices.</p>";
    h += '<h2 class="sec">⭐ Mes favoris</h2><div class="list">';
    var favs = Object.keys(store.fav).filter(function (k) { return BYID[k]; });
    h += favs.length ? favs.map(function (k) { return row("#/c/" + k, BYID[k].subject.emoji, esc(BYID[k].t), ""); }).join("") : '<p class="muted">Touche ☆ dans un chapitre pour l\'ajouter ici.</p>';
    h += '</div><h2 class="sec">🔁 À retravailler (quiz &lt; 60 %)</h2><div class="list">';
    var weak = qs.filter(function (k) { return store.quiz[k] < 60 && BYID[k]; });
    h += weak.length ? weak.map(function (k) { return row("#/c/" + k, store.quiz[k] + "%", esc(BYID[k].t), ""); }).join("") : '<p class="muted">Rien pour l\'instant 👍</p>';
    h += '</div><h2 class="sec">⚙️ Réglages</h2><div class="card settings">' +
      '<div><span>Thème</span><span><button class="chip' + (store.theme === "light" ? " on" : "") + '" data-th="light">☀️ Clair</button> <button class="chip' + (store.theme === "dark" ? " on" : "") + '" data-th="dark">🌙 Sombre</button> <button class="chip' + (!store.theme ? " on" : "") + '" data-th="">Auto</button></span></div>' +
      '<div><span>Taille du texte</span><span><button class="chip" id="zm">A−</button> <b>' + Math.round((store.zoom || 1) * 100) + ' %</b> <button class="chip" id="zp">A+</button></span></div>' +
      '<div><span>Objectif quotidien</span><span><button class="chip" id="gm">−</button> <b>' + (store.goal || 15) + ' actions</b> <button class="chip" id="gp">+</button></span></div></div>' +
      '<div class="btns"><button class="btn alt" id="reset">Remettre la progression à zéro</button></div>';
    h += '<div class="card"><small class="muted">BTS ATI Cours v1.1 — application de révision hors ligne. Contenu pédagogique rédigé pour être simple : vérifie toujours le programme, les thèmes et les modalités d\'examen de ta session auprès de ton établissement.</small></div>';
    view.innerHTML = h;
    view.querySelectorAll("[data-th]").forEach(function (b) { b.onclick = function () { store.theme = b.getAttribute("data-th"); save(); applyTheme(); more(); }; });
    $("#zm").onclick = function () { store.zoom = Math.max(0.8, Math.round(((store.zoom || 1) - 0.1) * 10) / 10); save(); applyTheme(); more(); };
    $("#zp").onclick = function () { store.zoom = Math.min(1.5, Math.round(((store.zoom || 1) + 0.1) * 10) / 10); save(); applyTheme(); more(); };
    $("#gm").onclick = function () { store.goal = Math.max(5, (store.goal || 15) - 5); save(); more(); };
    $("#gp").onclick = function () { store.goal = Math.min(100, (store.goal || 15) + 5); save(); more(); };
    $("#reset").onclick = function () { if (confirm("Effacer toute la progression (chapitres, quiz, fiches, examens, série) ?")) { store.done = {}; store.quiz = {}; store.fav = {}; store.box = {}; store.exams = []; store.act = {}; store.streak = { last: -1, n: 0, best: 0 }; store.last = null; save(); more(); } };
  }

  // ---------- Routeur ----------
  function route() {
    clearInterval(timer);
    var h = decodeURIComponent(location.hash.replace(/^#\/?/, "")), p = h.split("/");
    if (p[0] === "s") subject(p[1]);
    else if (p[0] === "c") chapter(p[1] + "/" + p[2]);
    else if (p[0] === "q") quiz(p[1] + "/" + p[2]);
    else if (p[0] === "cards") cards(p[1] ? p.slice(1).join("/") : null);
    else if (p[0] === "review") review();
    else if (p[0] === "challenge") challenge(p[1]);
    else if (p[0] === "exam") exam(p[1]);
    else if (p[0] === "exos") exos(p[1]);
    else if (p[0] === "formules") formules(p[1]);
    else if (p[0] === "tools") tools(p[1]);
    else if (p[0] === "search") search();
    else if (p[0] === "more") more();
    else home();
  }
  window.addEventListener("hashchange", route);
  route();
})();
