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

/** Réglages : adresse du hub, identifiants, jeton des rendus, synchro automatique de Movies/Cuttinx. */
public class SettingsActivity extends Activity {
    private EditText url, user, pass, token, folder;
    private CheckBox sync;
    private TextView status;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Prefs p = new Prefs(this);
        LinearLayout col = Ui.column(this);

        col.addView(Ui.label(this, "Adresse du hub"));
        url = Ui.field(this, p.url(), "https://mon-hub.exemple.com", InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_URI);
        col.addView(url);
        col.addView(Ui.hint(this, "La valeur de PUBLIC_BASE_URL du serveur."));

        col.addView(Ui.label(this, "Identifiant (HUB_USERNAME)"));
        user = Ui.field(this, p.user(), "admin", InputType.TYPE_CLASS_TEXT);
        col.addView(user);
        col.addView(Ui.label(this, "Mot de passe (HUB_PASSWORD)"));
        pass = Ui.field(this, p.pass(), "", InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_PASSWORD);
        col.addView(pass);

        col.addView(Ui.label(this, "Envoi automatique des rendus CUTTINX"));
        sync = new CheckBox(this);
        sync.setText("Envoyer au hub chaque nouveau MP4 exporté par CUTTINX");
        sync.setChecked(p.syncEnabled());
        col.addView(sync);
        col.addView(Ui.label(this, "Jeton d'envoi (RENDER_UPLOAD_TOKEN)"));
        token = Ui.field(this, p.token(), "", InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_PASSWORD);
        col.addView(token);
        col.addView(Ui.label(this, "Dossier des rendus"));
        folder = Ui.field(this, p.folder(), "Cuttinx", InputType.TYPE_CLASS_TEXT);
        col.addView(folder);
        col.addView(Ui.hint(this, "Nom du dossier de la galerie où CUTTINX exporte (Movies/Cuttinx). "
                + "Seuls les fichiers créés après l'activation sont envoyés ; les brouillons 480p sont ignorés par le hub."));

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
        new Prefs(this).save(u, user.getText().toString(), pass.getText().toString(), token.getText().toString(),
                sync.isChecked(), folder.getText().toString());
        RenderSyncJob.schedule(this);
        status.setText("Test de connexion…");
        new Thread(new Runnable() {
            @Override
            public void run() {
                String msg;
                try {
                    HubClient.Result r = new HubClient(SettingsActivity.this).get("/api/state");
                    if (r.ok()) msg = "✅ Connecté au hub.";
                    else if (r.code == 401) msg = "❌ Identifiant ou mot de passe refusé.";
                    else msg = "❌ Réponse " + r.code + " : " + UploadService.shorten(r.body);
                } catch (Exception e) {
                    msg = "❌ Hub injoignable : " + e.getMessage();
                }
                Prefs p = new Prefs(SettingsActivity.this);
                if (p.syncEnabled() && p.token().isEmpty()) msg += "\n⚠️ Jeton d'envoi vide : les rendus ne seront pas envoyés.";
                if (p.syncEnabled() && !Perms.canReadVideos(SettingsActivity.this)) msg += "\n⚠️ Autorisez l'accès aux vidéos pour l'envoi des rendus.";
                final String m = msg;
                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        status.setText(m);
                        if (m.startsWith("✅")) Toast.makeText(SettingsActivity.this, "Réglages enregistrés", Toast.LENGTH_SHORT).show();
                    }
                });
            }
        }).start();
    }
}
