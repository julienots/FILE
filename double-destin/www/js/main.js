"use strict";
// ============ Démarrage : studio → logo → menu ============
(function boot() {
  const word = $("#studioWord");
  const txt = "SuperEssence.";
  word.innerHTML = [...txt].map((ch, i) => `<span class="${i < 5 ? "w" : "y"}" style="animation-delay:${0.25 + i * 0.12}s">${ch}</span>`).join("");
  let skipped = false;
  const t1 = setTimeout(() => { word.classList.add("shine"); $(".studio-sub").classList.add("on"); }, 2000);
  const t2 = setTimeout(goTitle, 4300);
  $("#splash").addEventListener("click", () => { Sound.init(); goTitle(); });
  setTimeout(() => Sound.studio(), 300);

  function goTitle() {
    if (skipped) return; skipped = true; clearTimeout(t1); clearTimeout(t2);
    $("#splash").classList.add("out");
    setTimeout(() => {
      $("#bigLogo").innerHTML = logoHTML(210, 52);
      Screens.show("title");
    }, 650);
  }
  $("#title").addEventListener("click", () => { Sound.good(); showMenu(); });
})();

function showMenu() {
  $("#smallLogo").innerHTML = logoHTML(110, 32, true);
  const lifeSave = Store.get("life"), empSave = Store.get("empire");
  $$(".resume").forEach(e => e.remove());
  if (lifeSave && lifeSave.alive) $("#goLife").insertAdjacentHTML("beforeend", `<span class="resume">Reprendre · ${esc(lifeSave.first)}, ${lifeSave.age} ans</span>`);
  if (empSave && !empSave.over) $("#goEmpire").insertAdjacentHTML("beforeend", `<span class="resume">Reprendre · ${esc(empSave.natName || "")}</span>`);
  Screens.show("menu");
}

$("#goLife").onclick = () => { Sound.click(); Life.enter(); };
$("#goEmpire").onclick = () => { Sound.click(); Empire.enter(); };
$("#btnSettings").onclick = () => {
  const m = Modal.open({ icon: "⚙️", title: "Réglages", html: `
    <div class="list" style="padding:0">
      <button class="item" id="sSound"><div class="ic">🔊</div><div class="tx"><b>Sons</b></div><div class="rt">${Settings.sound ? "Activés" : "Coupés"}</div></button>
      <button class="item" id="sVib"><div class="ic">📳</div><div class="tx"><b>Vibrations</b></div><div class="rt">${Settings.vibrate ? "Activées" : "Coupées"}</div></button>
      <button class="item" id="sResetL"><div class="ic">🧬</div><div class="tx"><b>Effacer la vie en cours</b><small>Le cimetière est conservé</small></div></button>
      <button class="item" id="sResetE"><div class="ic">🌍</div><div class="tx"><b>Effacer la partie Empire</b></div></button>
    </div>` });
  $("#sSound").onclick = () => { Settings.sound = !Settings.sound; saveSettings(); m.close(); $("#btnSettings").click(); };
  $("#sVib").onclick = () => { Settings.vibrate = !Settings.vibrate; saveSettings(); m.close(); $("#btnSettings").click(); };
  $("#sResetL").onclick = async () => { if (await Modal.confirm("Effacer ?", "La vie en cours sera perdue.")) { Store.del("life"); toast("Vie effacée"); showMenu(); } };
  $("#sResetE").onclick = async () => { if (await Modal.confirm("Effacer ?", "La partie Empire sera perdue.")) { Store.del("empire"); toast("Partie effacée"); showMenu(); } };
};
$("#btnCredits").onclick = () => Modal.open({ icon: "ℹ️", title: "Crédits", html: `
  <div class="center" style="margin:6px 0 14px">${logoHTML(90, 24, false)}</div>
  <p><b>Double Destin</b> — un jeu <b>SuperEssence Studio</b>.</p>
  <p class="muted" style="font-size:13px">Cartographie : Natural Earth (domaine public). Altitudes : Terrarium / Mapzen, AWS Open Data. Noms de pays : world-countries (ODbL). Polices Fredoka et Nunito (SIL Open Font License).</p>
  <p class="muted" style="font-size:13px">Toute ressemblance avec des personnes existantes dans « Une Vie » est fortuite. Les dirigeants et événements d'« Empire Mondial » sont fictifs.</p>` });

BackHandlers.push(() => {
  if (Screens.cur === "life" || Screens.cur === "empire") { if (Screens.cur === "empire") Empire.save(); showMenu(); return true; }
  return false;
});
window.onAppPause = () => { try { if (Life.L) Life.save(); if (Empire.E) Empire.save(); } catch (e) {} };
