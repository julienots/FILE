/* Égide — YouTube sans pub (m.youtube.com). Injecté au tout début de chaque page. */
(function () {
  if (window.__egideYT) return;
  window.__egideYT = true;

  // 1. Retire les publicités des réponses du lecteur avant que YouTube ne les lise.
  var AD_KEYS = ['adPlacements', 'playerAds', 'adSlots', 'adBreakHeartbeatParams', 'adSignalsInfo'];
  function strip(o, depth) {
    if (!o || typeof o !== 'object' || depth > 6) return o;
    for (var i = 0; i < AD_KEYS.length; i++) if (AD_KEYS[i] in o) { try { delete o[AD_KEYS[i]]; } catch (e) {} }
    if (o.playerResponse) strip(o.playerResponse, depth + 1);
    if (Array.isArray(o)) for (var j = 0; j < o.length && j < 8; j++) strip(o[j], depth + 1);
    return o;
  }
  var origParse = JSON.parse;
  JSON.parse = function () { return strip(origParse.apply(this, arguments), 0); };
  if (window.Response && Response.prototype.json) {
    var origJson = Response.prototype.json;
    Response.prototype.json = function () { return origJson.apply(this, arguments).then(function (d) { return strip(d, 0); }); };
  }
  ['ytInitialPlayerResponse', 'ytInitialData'].forEach(function (name) {
    var v;
    try {
      Object.defineProperty(window, name, {
        configurable: true,
        get: function () { return v; },
        set: function (n) { v = strip(n, 0); }
      });
    } catch (e) {}
  });

  // 2. Masque les emplacements publicitaires restants.
  var css = [
    'ytm-promoted-sparkles-web-renderer', 'ytm-promoted-video-renderer', 'ytm-companion-ad-renderer',
    'ytm-ad-slot-renderer', 'ad-slot-renderer', 'ytd-ad-slot-renderer', 'ytm-promoted-sparkles-text-search-renderer',
    'ytm-display-ad-renderer', 'ytm-statement-banner-renderer', 'ytm-mealbar-promo-renderer',
    'ytm-paid-content-overlay-renderer', '.ytp-ad-overlay-container', '.ytp-ad-image-overlay',
    '.ytp-ad-text-overlay', 'ytm-item-section-renderer:has(ytm-ad-slot-renderer)',
    'ytm-rich-item-renderer:has(ad-slot-renderer)', 'ytm-companion-slot', '#player-ads', '.masthead-ad',
    'ytm-engagement-panel-section-list-renderer[target-id="engagement-panel-ads"]'
  ].join(',') + '{display:none!important}';
  function addStyle() {
    if (document.getElementById('egide-yt-css')) return;
    var s = document.createElement('style');
    s.id = 'egide-yt-css';
    s.textContent = css;
    (document.head || document.documentElement).appendChild(s);
  }

  // 3. Si une pub vidéo passe quand même : son coupé, avance rapide et clic sur « Passer ».
  var skipped = 0;
  function tick() {
    addStyle();
    var player = document.querySelector('.html5-video-player');
    var adShowing = player && (player.classList.contains('ad-showing') || player.classList.contains('ad-interrupting'));
    var btn = document.querySelector('.ytp-ad-skip-button, .ytp-ad-skip-button-modern, .ytp-skip-ad-button, .ytp-ad-skip-button-slot button');
    if (btn) { btn.click(); }
    if (adShowing) {
      var v = player.querySelector('video');
      if (v) {
        v.muted = true;
        if (isFinite(v.duration) && v.duration > 0) { v.currentTime = Math.max(v.duration - 0.1, 0); }
        v.playbackRate = 16;
        if (v.paused) { try { v.play(); } catch (e) {} }
      }
      if (!window.__egideCounted) {
        window.__egideCounted = true;
        skipped++;
        try { EgideBridge.adSkipped(); } catch (e) {}
      }
    } else if (window.__egideCounted) {
      window.__egideCounted = false;
      var v2 = player && player.querySelector('video');
      if (v2) { v2.playbackRate = 1; v2.muted = false; }
    }
  }
  setInterval(tick, 250);
  document.addEventListener('DOMContentLoaded', tick);
})();
