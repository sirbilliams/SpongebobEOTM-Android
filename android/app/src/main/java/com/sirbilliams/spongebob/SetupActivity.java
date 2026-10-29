package com.sirbilliams.spongebob;

import android.app.Activity;
import android.content.Intent;
import android.database.Cursor;
import android.graphics.Color;
import android.graphics.Typeface;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.ParcelFileDescriptor;
import android.provider.OpenableColumns;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.TextView;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;

/**
 * First screen. If the game is installed (files/game/sb2.exe readable) it shows the launcher: Play and a
 * Test page (share omt-log.txt, crash test, reinstall). Otherwise it lets the player pick the game's CD
 * image (.iso), which is installed like the CD's setup does (GameImporter) into files/game, which the app
 * owns. Files copied in over USB can be visible to the app but not readable on some Android builds;
 * files the app writes itself always are.
 */
public class SetupActivity extends Activity {

    private static final int PICK_ISO = 1;

    private String mPage = "main";
    private TextView message;
    private Button choose;
    private ProgressBar progress;
    private final Handler ui = new Handler(Looper.getMainLooper());

    // ------------------------------------------------------------------ lifecycle

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        File base = getExternalFilesDir(null);
        if (base != null) base.mkdirs();
        if (installed()) showMain();
        else showImport();
    }

    private File gameDir() {
        File base = getExternalFilesDir(null);
        return new File(base != null ? base : getFilesDir(), "game");
    }

    /** True if the game's exe is there and readable. */
    private boolean installed() {
        return canRead(new File(gameDir(), "sb2.exe"));
    }

    private static boolean canRead(File f) {
        try (InputStream in = new FileInputStream(f)) {
            return in.read() >= 0;
        } catch (IOException | SecurityException e) {
            return false;
        }
    }

    private void play(boolean crashTest) {
        Intent i = new Intent(this, SpongeActivity.class);
        if (crashTest) i.putExtra(SpongeActivity.EXTRA_CRASH_TEST, true);
        startActivity(i, onTopScreen());
    }

    /** port: the game always opens on the default (top) display, never a TV / cast display. */
    static Bundle onTopScreen() {
        if (android.os.Build.VERSION.SDK_INT < 26) return null;
        try {
            return android.app.ActivityOptions.makeBasic()
                    .setLaunchDisplayId(android.view.Display.DEFAULT_DISPLAY).toBundle();
        } catch (RuntimeException e) {
            return null;
        }
    }

    // ------------------------------------------------------------------ launcher

    private LinearLayout page(String heading) {
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setGravity(Gravity.CENTER);
        root.setBackgroundColor(Color.rgb(14, 20, 40));
        int pad = dp(32);
        root.setPadding(pad, pad, pad, pad);
        TextView title = new TextView(this);
        title.setText(heading);
        title.setTextColor(Color.rgb(255, 210, 90));
        title.setTextSize(26);
        title.setTypeface(Typeface.DEFAULT_BOLD);
        title.setGravity(Gravity.CENTER);
        title.setPadding(0, 0, 0, dp(20));
        root.addView(title, wrap());
        return root;
    }

    private Button button(LinearLayout root, String text, View.OnClickListener l) {
        Button b = new Button(this);
        b.setText(text);
        b.setTextSize(18);
        b.setOnClickListener(l);
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(dp(360), ViewGroup.LayoutParams.WRAP_CONTENT);
        lp.bottomMargin = dp(8);
        root.addView(b, lp);
        return b;
    }

    private void showMain() {
        mPage = "main";
        LinearLayout root = page("SpongeBob EOTM");
        Button play = button(root, "Play", v -> play(false));
        final Button panel = button(root, panelLabel(), null);
        panel.setOnClickListener(v -> { setPanelOn(!panelOn()); panel.setText(panelLabel()); });
        button(root, "Test", v -> showTest());
        button(root, "Quit", v -> finish());
        setContentView(root);
        play.requestFocus();                                // so the Thor's A button works without touching the screen
    }

    // port: the bottom-screen companion panel (docs/companion.md). On by default; the choice is remembered.
    private boolean panelOn() {
        return CompanionPanel.wanted(getSharedPreferences(CompanionPanel.PREFS, MODE_PRIVATE), true);
    }

    private void setPanelOn(boolean on) {
        getSharedPreferences(CompanionPanel.PREFS, MODE_PRIVATE).edit().putBoolean(CompanionPanel.KEY_ON, on).apply();
    }

    private String panelLabel() {
        return "Bottom screen panel: " + (panelOn() ? "ON" : "OFF");
    }

    private void showTest() {
        mPage = "test";
        LinearLayout root = page("Test");
        Button share = button(root, "Share log (omt-log.txt)", v -> shareLog());
        button(root, "Crash test (writes a backtrace to the log)", v -> play(true));
        button(root, "Reinstall from the CD image", v -> showImport());
        button(root, "Back", v -> showMain());
        setContentView(root);
        share.requestFocus();
    }

    @Override
    public void onBackPressed() {
        if (installed() && !mPage.equals("main")) showMain();
        else super.onBackPressed();
    }

    /** Sends the last run's omt-log.txt (written by the engine next to the game folder) as text. */
    private void shareLog() {
        File f = new File(getExternalFilesDir(null), "omt-log.txt");
        String text;
        try (FileInputStream in = new FileInputStream(f)) {
            long len = f.length(), max = 200 * 1024;
            if (len > max) in.skip(len - max);              // the end of a long log matters most
            byte[] b = new byte[(int) Math.min(len, max)];
            int n = 0, r;
            while (n < b.length && (r = in.read(b, n, b.length - n)) > 0) n += r;
            text = new String(b, 0, n, "UTF-8");
        } catch (IOException e) {
            text = "No log yet (" + e.getMessage() + "). Run Play first.";
        }
        Intent i = new Intent(Intent.ACTION_SEND);
        i.setType("text/plain");
        i.putExtra(Intent.EXTRA_SUBJECT, "SpongeBob EOTM log");
        i.putExtra(Intent.EXTRA_TEXT, text);
        startActivity(Intent.createChooser(i, "Share log"));
    }

    // ------------------------------------------------------------------ install screen

    private void showImport() {
        mPage = "import";
        LinearLayout root = page("SpongeBob EOTM");
        message = new TextView(this);
        message.setTextColor(Color.rgb(220, 225, 235));
        message.setTextSize(17);
        message.setGravity(Gravity.CENTER);
        message.setPadding(0, 0, 0, dp(24));
        message.setText("Game files needed.\n\nChoose the game's CD image (.iso): the game is installed "
                + "from it, about 1 GB, like the CD's own setup does.");
        root.addView(message, wrap());

        progress = new ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal);
        progress.setMax(1000);
        progress.setVisibility(View.GONE);
        LinearLayout.LayoutParams pl = new LinearLayout.LayoutParams(dp(420), ViewGroup.LayoutParams.WRAP_CONTENT);
        pl.bottomMargin = dp(20);
        root.addView(progress, pl);

        choose = new Button(this);
        choose.setText("Choose the game's .iso…");
        choose.setTextSize(18);
        choose.setOnClickListener(v -> pickIso());
        root.addView(choose, wrap());

        setContentView(root);
        choose.requestFocus();
    }

    private int dp(int v) { return (int) (v * getResources().getDisplayMetrics().density + 0.5f); }

    private static LinearLayout.LayoutParams wrap() {
        return new LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
    }

    private void pickIso() {
        Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        i.addCategory(Intent.CATEGORY_OPENABLE);
        i.setType("*/*");
        i.putExtra(Intent.EXTRA_MIME_TYPES, new String[] {
                "application/x-iso9660-image", "application/x-cd-image", "application/octet-stream" });
        startActivityForResult(i, PICK_ISO);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == PICK_ISO && resultCode == RESULT_OK && data != null && data.getData() != null)
            importIso(data.getData());
    }

    // ------------------------------------------------------------------ import from the CD image

    private void importIso(final Uri uri) {
        choose.setEnabled(false);
        progress.setVisibility(View.VISIBLE);
        progress.setProgress(0);
        message.setText("Installing from the CD image…");

        new Thread(() -> {
            String error;
            File base = getExternalFilesDir(null);
            File tmp = new File(base, "game.import");
            File cab = new File(base, "game.import-cab");
            File dest = gameDir();
            try (ParcelFileDescriptor pfd = getContentResolver().openFileDescriptor(uri, "r")) {
                deleteTree(tmp);
                deleteTree(cab);
                if (pfd == null) throw new IOException("can't open the file");
                final int[] last = { -1 };
                error = GameImporter.importIso(pfd.getFd(), tmp.getPath(), cab.getPath(), (stage, done, total) -> {
                    int p = total > 0 ? (int) (stage * 500 + done * 500 / total) : stage * 500;
                    if (p != last[0]) {
                        last[0] = p;
                        final String what = stage == 0 ? "Copying the installer off the CD image…" : "Unpacking the game files…";
                        ui.post(() -> { progress.setProgress(p); message.setText(what); });
                    }
                    return false;
                });
                if (error == null) {
                    if (!canRead(new File(tmp, "sb2.exe")) || !canRead(new File(tmp, "sb2.omt"))) {
                        error = "The CD image was unpacked but the game files are missing (sb2.exe).";
                    } else {
                        deleteTree(dest);
                        if (!tmp.renameTo(dest)) throw new IOException("could not move files into place");
                    }
                }
            } catch (IOException | SecurityException e) {
                error = "Couldn't read the CD image: " + e.getMessage()
                        + "\nIf it is on a cloud drive, copy it to the device first.";
            } finally {
                deleteTree(tmp);
                deleteTree(cab);
            }
            final String err = error;
            ui.post(() -> {
                if (err == null) {
                    showMain();
                } else {
                    progress.setVisibility(View.GONE);
                    message.setText(err + "\n\nChoose another file to try again.");
                    choose.setEnabled(true);
                    choose.requestFocus();
                }
            });
        }).start();
    }

    private long querySize(Uri uri) {
        try (Cursor c = getContentResolver().query(uri, new String[] { OpenableColumns.SIZE }, null, null, null)) {
            if (c != null && c.moveToFirst() && !c.isNull(0)) return c.getLong(0);
        } catch (RuntimeException ignored) { }
        return -1;
    }

    private static void deleteTree(File f) {
        if (f == null || !f.exists()) return;
        File[] kids = f.listFiles();
        if (kids != null) for (File k : kids) deleteTree(k);
        //noinspection ResultOfMethodCallIgnored
        f.delete();
    }
}
