// LOST KINGDOM — the only Java code of the game: hides the Android system bars (true full screen).
// The game itself is native (NativeActivity + liblostkingdom.so). Hiding the navigation bar must be done on the
// UI thread; the native game thread cannot do it (Android throws CalledFromWrongThreadException), which is why
// the bar stayed visible up to 1.6.2.
package com.superessence.lostkingdom;

import android.app.NativeActivity;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.provider.OpenableColumns;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;
import android.os.Build;
import android.os.Bundle;
import android.view.View;
import android.view.Window;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;

public class GameActivity extends NativeActivity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        hideSystemBars();
    }

    @Override
    protected void onResume() {
        super.onResume();
        hideSystemBars();
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) hideSystemBars();
    }

    // ---- mods (1.7.0): called from native code (any thread). Opens the system file picker for a .zip.
    private static final int PICK_MOD = 4242;

    public void pickModZip() {
        runOnUiThread(() -> {
            Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            i.addCategory(Intent.CATEGORY_OPENABLE);
            i.setType("*/*");
            i.putExtra(Intent.EXTRA_MIME_TYPES, new String[] {"application/zip", "application/x-zip-compressed", "application/octet-stream"});
            try { startActivityForResult(i, PICK_MOD); } catch (Exception e) { /* no picker available */ }
        });
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode != PICK_MOD || resultCode != RESULT_OK || data == null || data.getData() == null) return;
        final Uri uri = data.getData();
        new Thread(() -> importMod(uri)).start();   // unzipping can take a moment: never on the UI thread
    }

    // Unpacks a mod zip into <files>/mods/<name>/. Data only: entries that would escape the folder ("..",
    // absolute paths) are refused, and the total size is capped. Writes <files>/mods/.imported when done.
    private void importMod(Uri uri) {
        File modsDir = new File(getFilesDir(), "mods");
        modsDir.mkdirs();
        String name = displayName(uri);
        if (name.toLowerCase().endsWith(".zip")) name = name.substring(0, name.length() - 4);
        name = name.replaceAll("[^A-Za-z0-9_\\-]", "_");
        if (name.isEmpty()) name = "mod";
        File dest = new File(modsDir, name);
        String status = "ok " + name;
        long total = 0;
        try (InputStream in = getContentResolver().openInputStream(uri); ZipInputStream zip = new ZipInputStream(in)) {
            byte[] buf = new byte[65536];
            String destPath = dest.getCanonicalPath() + File.separator;
            ZipEntry e;
            while ((e = zip.getNextEntry()) != null) {
                File out = new File(dest, e.getName());
                if (!out.getCanonicalPath().startsWith(destPath)) { status = "refused " + name; break; }
                if (e.isDirectory()) { out.mkdirs(); continue; }
                out.getParentFile().mkdirs();
                try (FileOutputStream fo = new FileOutputStream(out)) {
                    int n;
                    while ((n = zip.read(buf)) > 0) {
                        total += n;
                        if (total > 256L * 1024 * 1024) throw new java.io.IOException("mod too large");
                        fo.write(buf, 0, n);
                    }
                }
            }
        } catch (Exception ex) {
            status = "error " + name;
        }
        try (FileOutputStream fo = new FileOutputStream(new File(modsDir, ".imported"))) {
            fo.write(status.getBytes("UTF-8"));
        } catch (Exception ignored) { }
    }

    private String displayName(Uri uri) {
        try (Cursor c = getContentResolver().query(uri, null, null, null, null)) {
            if (c != null && c.moveToFirst()) {
                int idx = c.getColumnIndex(OpenableColumns.DISPLAY_NAME);
                if (idx >= 0) return c.getString(idx);
            }
        } catch (Exception ignored) { }
        return "mod";
    }

    @SuppressWarnings("deprecation")
    private void hideSystemBars() {
        Window w = getWindow();
        if (w == null) return;
        if (Build.VERSION.SDK_INT >= 28) {
            // draw under the camera notch too (landscape game)
            WindowManager.LayoutParams lp = w.getAttributes();
            lp.layoutInDisplayCutoutMode = Build.VERSION.SDK_INT >= 30
                ? WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS
                : WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
            w.setAttributes(lp);
        }
        if (Build.VERSION.SDK_INT >= 30) {
            w.setDecorFitsSystemWindows(false);
            WindowInsetsController c = w.getInsetsController();
            if (c != null) {
                c.hide(WindowInsets.Type.statusBars() | WindowInsets.Type.navigationBars());
                // a swipe from the edge shows the bars for a moment, then they hide again
                c.setSystemBarsBehavior(WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
            }
        } else {
            w.getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY | View.SYSTEM_UI_FLAG_FULLSCREEN | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_LAYOUT_STABLE | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN);
        }
    }
}
