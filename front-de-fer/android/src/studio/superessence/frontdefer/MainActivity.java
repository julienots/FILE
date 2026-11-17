package studio.superessence.frontdefer;

import android.app.Activity;
import android.os.Build;
import android.os.Bundle;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.WindowManager;
import android.webkit.ValueCallback;
import android.webkit.WebSettings;
import android.webkit.WebView;

/** Enveloppe plein écran : le jeu (HTML + WebGL) est chargé depuis les fichiers embarqués, sans réseau. */
public class MainActivity extends Activity {
    private WebView web;

    @Override protected void onCreate(Bundle state) {
        super.onCreate(state);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        if (Build.VERSION.SDK_INT >= 28) {
            WindowManager.LayoutParams lp = getWindow().getAttributes();
            lp.layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
            getWindow().setAttributes(lp);
        }
        web = new WebView(this);
        WebSettings s = web.getSettings();
        s.setJavaScriptEnabled(true);
        s.setDomStorageEnabled(true);
        s.setMediaPlaybackRequiresUserGesture(false);
        web.setBackgroundColor(0xff0c0d0b);
        web.setOverScrollMode(View.OVER_SCROLL_NEVER);
        web.setFocusable(true);
        web.setFocusableInTouchMode(true);
        setContentView(web);
        web.requestFocus();
        immersive();
        web.loadUrl("file:///android_asset/index.html");
    }

    private void immersive() {
        web.setSystemUiVisibility(View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY | View.SYSTEM_UI_FLAG_FULLSCREEN
                | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION);
    }

    @Override public void onWindowFocusChanged(boolean focus) { super.onWindowFocusChanged(focus); if (focus) immersive(); }

    @Override protected void onPause() {
        web.evaluateJavascript("if(window.setPause)setPause(true)", null);
        web.onPause();
        super.onPause();
    }

    @Override protected void onResume() { super.onResume(); web.onResume(); immersive(); }

    @Override public void onBackPressed() {
        web.evaluateJavascript("(function(){if(G.state==='play'){setPause(true);return 1}if(G.state==='pause'||G.state==='end'){toLobby();return 1}return 0})()",
                new ValueCallback<String>() {
                    @Override public void onReceiveValue(String v) { if (!"1".equals(v)) finish(); }
                });
    }

    private static boolean isPad(int src) {
        return (src & InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD || (src & InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK;
    }

    /** Les boutons de la manette vont au jeu : B ne doit pas devenir « retour » et fermer l'appli. */
    @Override public boolean dispatchKeyEvent(KeyEvent e) {
        if (isPad(e.getSource()) && e.getKeyCode() != KeyEvent.KEYCODE_HOME) { web.dispatchKeyEvent(e); return true; }
        return super.dispatchKeyEvent(e);
    }

    @Override public boolean dispatchGenericMotionEvent(MotionEvent e) {
        if (isPad(e.getSource())) { web.dispatchGenericMotionEvent(e); return true; }
        return super.dispatchGenericMotionEvent(e);
    }

    @Override protected void onDestroy() { web.destroy(); super.onDestroy(); }
}
