package com.julien.videohub;

import android.app.Activity;
import android.os.Bundle;
import android.text.InputType;
import android.view.View;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import android.widget.Toast;

/** Réglages : relais Supabase (déjà pré-rempli), dossier des rendus CUTTINX, envoi automatique. */
public class SettingsActivity extends Activity {
    private EditText url, key, folder;
    private CheckBox sync;
    private TextView status;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Prefs p = new Prefs(this);
        LinearLayout col = Ui.column(this);

        col.addView(Ui.hint(this, "Normalement rien à changer : l'appli est déjà reliée à votre hub. "
                + "Claude passe toutes les heures pour monter, écrire la légende et publier."));
        col.addView(Ui.label(this, "Envoi automatique des rendus CUTTINX"));
        sync = new CheckBox(this);
        sync.setText("Envoyer au hub chaque nouveau MP4 exporté par CUTTINX");
        sync.setChecked(p.syncEnabled());
        col.addView(sync);
        col.addView(Ui.label(this, "Dossier des rendus"));
        folder = Ui.field(this, p.folder(), "Cuttinx", InputType.TYPE_CLASS_TEXT);
        col.addView(folder);

        col.addView(Ui.label(this, "Adresse du relais (Supabase)"));
        url = Ui.field(this, p.url(), "https://xxxx.supabase.co", InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_URI);
        col.addView(url);
        col.addView(Ui.label(this, "Clé publique du relais"));
        key = Ui.field(this, p.key(), "", InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_PASSWORD);
        col.addView(key);

        Button save = new Button(this);
        save.setText("Enregistrer et tester");
        save.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                saveAndTest();
            }
        });
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(-1, -2);
        lp.topMargin = Ui.dp(this, 20);
        col.addView(save, lp);

        status = new TextView(this);
        status.setPadding(0, Ui.dp(this, 12), 0, 0);
        col.addView(status);

        ScrollView sv = new ScrollView(this);
        sv.addView(col);
        setContentView(sv);
        Perms.requestAll(this);
    }

    private void saveAndTest() {
        String u = url.getText().toString().trim();
        if (!u.startsWith("http")) {
            Toast.makeText(this, "L'adresse doit commencer par https://", Toast.LENGTH_LONG).show();
            return;
        }
        new Prefs(this).save(u, key.getText().toString(), sync.isChecked(), folder.getText().toString());
        RenderSyncJob.schedule(this);
        status.setText("Test de connexion…");
        new Thread(new Runnable() {
            @Override
            public void run() {
                String msg;
                try {
                    new Relay(SettingsActivity.this).ping();
                    msg = "✅ Connecté au hub.";
                } catch (Exception e) {
                    msg = "❌ " + e.getMessage();
                }
                if (new Prefs(SettingsActivity.this).syncEnabled() && !Perms.canReadVideos(SettingsActivity.this)) {
                    msg += "\n⚠️ Autorisez l'accès aux vidéos (Tout autoriser) pour l'envoi des rendus.";
                }
                final String m = msg;
                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        status.setText(m);
                    }
                });
            }
        }).start();
    }
}
