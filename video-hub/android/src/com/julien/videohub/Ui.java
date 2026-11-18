package com.julien.videohub;

import android.content.Context;
import android.graphics.Typeface;
import android.text.InputType;
import android.view.View;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.RadioButton;
import android.widget.RadioGroup;
import android.widget.TextView;

/** Petits utilitaires pour construire les écrans en code (pas de dépendance AndroidX). */
final class Ui {
    private Ui() {}

    static int dp(Context c, int v) {
        return Math.round(v * c.getResources().getDisplayMetrics().density);
    }

    static LinearLayout column(Context c) {
        LinearLayout l = new LinearLayout(c);
        l.setOrientation(LinearLayout.VERTICAL);
        int pad = dp(c, 20);
        l.setPadding(pad, pad, pad, pad);
        return l;
    }

    static TextView label(Context c, String text) {
        TextView t = new TextView(c);
        t.setText(text);
        t.setTypeface(Typeface.DEFAULT_BOLD);
        t.setPadding(0, dp(c, 14), 0, dp(c, 4));
        return t;
    }

    static TextView hint(Context c, String text) {
        TextView t = new TextView(c);
        t.setText(text);
        t.setTextSize(13);
        t.setAlpha(0.7f);
        return t;
    }

    static EditText field(Context c, String value, String hint, int type) {
        EditText e = new EditText(c);
        e.setSingleLine(type != (InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_MULTI_LINE));
        e.setInputType(type);
        e.setHint(hint);
        e.setText(value);
        return e;
    }

    static RadioGroup radios(Context c, String[] labels, int checked) {
        RadioGroup g = new RadioGroup(c);
        for (int i = 0; i < labels.length; i++) {
            RadioButton b = new RadioButton(c);
            b.setId(View.generateViewId());
            b.setText(labels[i]);
            b.setTag(i);
            g.addView(b);
            if (i == checked) b.setChecked(true);
        }
        return g;
    }

    static int checkedIndex(RadioGroup g) {
        View v = g.findViewById(g.getCheckedRadioButtonId());
        return v == null ? 0 : (Integer) v.getTag();
    }
}
