package com.julien.videohub;

import android.Manifest;
import android.app.Activity;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Build;

import java.util.ArrayList;
import java.util.List;

final class Perms {
    private Perms() {}

    static String videoPermission() {
        return Build.VERSION.SDK_INT >= 33 ? "android.permission.READ_MEDIA_VIDEO" : Manifest.permission.READ_EXTERNAL_STORAGE;
    }

    static boolean canReadVideos(Context c) {
        return c.checkSelfPermission(videoPermission()) == PackageManager.PERMISSION_GRANTED;
    }

    static void requestAll(Activity a) {
        List<String> need = new ArrayList<String>();
        if (!canReadVideos(a)) need.add(videoPermission());
        if (Build.VERSION.SDK_INT >= 33
                && a.checkSelfPermission("android.permission.POST_NOTIFICATIONS") != PackageManager.PERMISSION_GRANTED) {
            need.add("android.permission.POST_NOTIFICATIONS");
        }
        if (!need.isEmpty()) a.requestPermissions(need.toArray(new String[0]), 7);
    }
}
