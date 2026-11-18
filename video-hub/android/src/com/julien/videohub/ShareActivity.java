package com.julien.videohub;

import android.app.Activity;
import android.content.ClipData;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.text.InputType;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.RadioGroup;
import android.widget.ScrollView;
import android.widget.TextView;
import android.widget.Toast;

import java.util.ArrayList;

/** « Partager → Hub vidéo » depuis la galerie : catégorie, format, publication, puis envoi en arrière-plan. */
public class ShareActivity extends Activity {
    private final ArrayList<Uri> uris = new ArrayList<Uri>();

    @Override
    @SuppressWarnings("deprecation")
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (!new Prefs(this).configured()) {
            Toast.makeText(this, "Configurez d'abord le relais (Réglages).", Toast.LENGTH_LONG).show();
            startActivity(new Intent(this, SettingsActivity.class));
            finish();
            return;
        }
        Intent in = getIntent();
        if (Intent.ACTION_SEND.equals(in.getAction())) {
            Uri u = in.getParcelableExtra(Intent.EXTRA_STREAM);
            if (u != null) uris.add(u);
        } else if (Intent.ACTION_SEND_MULTIPLE.equals(in.getAction())) {
            ArrayList<Uri> list = in.getParcelableArrayListExtra(Intent.EXTRA_STREAM);
            if (list != null) uris.addAll(list);
        }
        if (uris.isEmpty()) {
            Toast.makeText(this, "Aucune vidéo reçue.", Toast.LENGTH_LONG).show();
            finish();
            return;
        }
        setTitle(uris.size() == 1 ? "Envoyer 1 vidéo au hub" : "Envoyer " + uris.size() + " vidéos au hub");

        LinearLayout col = Ui.column(this);
        col.addView(Ui.label(this, "Catégorie"));
        final RadioGroup cat = Ui.radios(this, new String[]{"💪 Performance (street workout)", "🎥 Vidéo normale (vlog, divers)"}, 0);
        col.addView(cat);

        final TextView aspectLabel = Ui.label(this, "Format");
        final RadioGroup aspect = Ui.radios(this, new String[]{"Vertical 9:16 (Reels)", "Horizontal 16:9"}, 0);
        aspectLabel.setVisibility(View.GONE);
        aspect.setVisibility(View.GONE);
        col.addView(aspectLabel);
        col.addView(aspect);
        cat.setOnCheckedChangeListener(new RadioGroup.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(RadioGroup g, int id) {
                int vis = Ui.checkedIndex(g) == 1 ? View.VISIBLE : View.GONE;
                aspectLabel.setVisibility(vis);
                aspect.setVisibility(vis);
            }
        });

        col.addView(Ui.label(this, "Publication"));
        final RadioGroup mode = Ui.radios(this, new String[]{"⚡ Dès que c'est prêt", "🗓️ Heure de forte audience"}, 0);
        col.addView(mode);

        col.addView(Ui.label(this, "Notes (facultatif)"));
        final EditText notes = Ui.field(this, "", "Ex. : ralenti sur la 2e rep",
                InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_MULTI_LINE);
        col.addView(notes);

        Button send = new Button(this);
        send.setText("Envoyer dans la file");
        send.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                boolean perf = Ui.checkedIndex(cat) == 0;
                Intent svc = new Intent(ShareActivity.this, UploadService.class)
                        .putExtra(UploadService.EXTRA_CATEGORY, perf ? "performance" : "normal")
                        .putExtra(UploadService.EXTRA_ASPECT, perf || Ui.checkedIndex(aspect) == 0 ? "9:16" : "16:9")
                        .putExtra(UploadService.EXTRA_MODE, Ui.checkedIndex(mode) == 0 ? "now" : "scheduled")
                        .putExtra(UploadService.EXTRA_NOTES, notes.getText().toString())
                        .addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
                ClipData clip = ClipData.newRawUri("videos", uris.get(0));
                for (int i = 1; i < uris.size(); i++) clip.addItem(new ClipData.Item(uris.get(i)));
                svc.setClipData(clip);
                startForegroundService(svc);
                Toast.makeText(ShareActivity.this, "Envoi en cours (voir la notification)", Toast.LENGTH_SHORT).show();
                finish();
            }
        });
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(-1, -2);
        lp.topMargin = Ui.dp(this, 16);
        col.addView(send, lp);

        ScrollView sv = new ScrollView(this);
        sv.addView(col);
        setContentView(sv);
    }
}
