package com.sirbilliams.spongebob;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.ParcelFileDescriptor;
import android.widget.TextView;
import android.widget.Toast;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;

/** Hosts the Canvas launcher and the original CD importer and diagnostic actions. */
public class SetupActivity extends Activity {

    private static final int PICK_ISO = 1;

    private String mPage = "main";
    private LauncherView launcher;
    private boolean importing;
    private final Handler ui = new Handler(Looper.getMainLooper());

    // ------------------------------------------------------------------ lifecycle

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        File base = getExternalFilesDir(null);
        if (base != null) base.mkdirs();
        launcher = new LauncherView(this, new LauncherView.Actions() {
            public void select(int row) { activate(row); }
            public void back() { onBackPressed(); }
        });
        setContentView(launcher);
        showMain();
        launcher.requestFocus();
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

    private void showMain() {
        mPage = "main";
        launcher.page("Welcome aboard", installed() ? "Ready to set sail" : "Your adventure awaits",
            installed() ? "Game files installed.\nChoose Play to begin." : "Import your own CD image\nto begin your adventure.",
            "Play", "Import game", "Settings", "Test", "About", "Quit");
    }

    private boolean hasSave() {
        File[] files = new File(gameDir(), "saved").listFiles();
        if (files != null) for (File f : files) if (f.isFile() && f.length() > 0) return true;
        return false;
    }

    private void showPlay() {
        mPage = "play";
        launcher.page("Set sail", hasSave() ? "Welcome back, captain" : "Adventure on the horizon",
            hasSave() ? "A save was found in saved/.\nResume from the game's menu." : "Start the game and choose\na new adventure.",
            installed() ? "Start game" : "Import game files first", "Back");
    }

    private boolean panelOn() {
        return CompanionPanel.wanted(getSharedPreferences(CompanionPanel.PREFS, MODE_PRIVATE), true);
    }
    private void showSettings() {
        mPage = "settings";
        int mode = Math.floorMod(getSharedPreferences("settings", MODE_PRIVATE).getInt("widescreen", 0), 4);
        launcher.page("Captain's settings", "Make yourself at home", "Choices apply on next launch.\nTouch always controls the cursor.",
            "Widescreen: " + new String[]{"Fill", "Side panels", "Stretch", "Off"}[mode],
            "Bottom-screen panel: " + (panelOn() ? "On" : "Off"),
            "Right-stick cursor: " + (getSharedPreferences("settings", MODE_PRIVATE).getBoolean("pad_cursor", true) ? "On" : "Off"), "Back");
    }
    private void showTest() {
        mPage = "test";
        launcher.page("Engine room", "Tools & diagnostics", "Logs include the current\nand previous run.",
            "Share log", "App version: " + versionText(), "Reset settings", "Crash test", "Reinstall from CD image", "Back");
    }
    private void showAbout() {
        mPage = "about";
        launcher.page("About this voyage", "An independent engine port", "Original launcher shapes.\nBring your own game files.",
            "Version: " + versionText(), "Open-source licences", "Back");
    }
    private void showLicences() {
        final String[] names = {"SDL2-zlib.txt", "gl4es-MIT.txt", "unshield-MIT.txt", "OMT-LGPL-2.1.txt", "HIDAPI-BSD.txt"};
        new android.app.AlertDialog.Builder(this).setTitle("Open-source licences")
            .setItems(names, (dialog, which) -> {
                try (InputStream in = getAssets().open("licenses/" + names[which])) {
                    java.io.ByteArrayOutputStream out = new java.io.ByteArrayOutputStream();
                    byte[] buffer = new byte[4096]; int n;
                    while ((n = in.read(buffer)) > 0) out.write(buffer, 0, n);
                    TextView text = new TextView(this); text.setText(out.toString("UTF-8")); text.setPadding(24,24,24,24);
                    android.widget.ScrollView scroll = new android.widget.ScrollView(this); scroll.addView(text);
                    new android.app.AlertDialog.Builder(this).setTitle(names[which]).setView(scroll).setPositiveButton("Close", null).show();
                } catch (IOException e) { Toast.makeText(this, e.getMessage(), Toast.LENGTH_LONG).show(); }
            }).setNegativeButton("Back", null).show();
    }
    private void activate(int row) {
        if (importing) return;
        switch (mPage) {
        case "main":
            switch(row) { case 0: showPlay(); break; case 1: showImport(); break; case 2: showSettings(); break; case 3: showTest(); break; case 4: showAbout(); break; default: finish(); } break;
        case "play": if(row==1)showMain(); else if(installed())play(false); else showImport(); break;
        case "import": if(row==0)pickIso(); else showMain(); break;
        case "settings":
            if(row==3) {showMain();break;}
            if(row==0) getSharedPreferences("settings", MODE_PRIVATE).edit().putInt("widescreen", Math.floorMod(getSharedPreferences("settings", MODE_PRIVATE).getInt("widescreen",0)+1,4)).apply();
            if(row==1) getSharedPreferences(CompanionPanel.PREFS, MODE_PRIVATE).edit().putBoolean(CompanionPanel.KEY_ON,!panelOn()).apply();
            if(row==2) getSharedPreferences("settings", MODE_PRIVATE).edit().putBoolean("pad_cursor",!getSharedPreferences("settings", MODE_PRIVATE).getBoolean("pad_cursor",true)).apply();
            showSettings(); launcher.focusRow(row); break;
        case "test":
            switch(row) {case 0:shareLog();break;case 1:break;case 2:resetSettings();break;case 3:if(installed())play(true);else showImport();break;case 4:showImport();break;default:showMain();} break;
        case "about": if(row==1)showLicences(); else if(row==2)showMain(); break;
        }
    }
    @Override public void onBackPressed() {
        if(importing)return;
        if(!mPage.equals("main"))showMain(); else super.onBackPressed();
    }

    /** versionName + git commit the build embedded (res/values git_commit, from build.gradle). */
    private String versionText() {
        String name = "?";
        try {
            name = getPackageManager().getPackageInfo(getPackageName(), 0).versionName;
        } catch (Exception ignored) { }
        return name + " (" + getString(R.string.git_commit) + ")";
    }

    /** Back to defaults for the launcher's options (widescreen, cursor, panel). The game's saves are not touched. */
    private void resetSettings() {
        getSharedPreferences("settings", MODE_PRIVATE).edit().clear().apply();
        getSharedPreferences(CompanionPanel.PREFS, MODE_PRIVATE).edit().clear().apply();
        showTest();
        Toast.makeText(this, "Settings reset (saves kept)", Toast.LENGTH_SHORT).show();
    }

    /** The end of a log file as text (the end of a long log matters most). */
    private static String tail(File f, long max) {
        try (FileInputStream in = new FileInputStream(f)) {
            long len = f.length();
            if (len > max) in.skip(len - max);
            byte[] b = new byte[(int) Math.min(len, max)];
            int n = 0, r;
            while (n < b.length && (r = in.read(b, n, b.length - n)) > 0) n += r;
            return new String(b, 0, n, "UTF-8");
        } catch (IOException e) {
            return "(none: " + e.getMessage() + ")";
        }
    }

    /** Sends device + app info and the current and previous run's logs as text (no FileProvider needed). */
    private void shareLog() {
        File base = getExternalFilesDir(null);
        StringBuilder sb = new StringBuilder();
        sb.append("SpongeBob EOTM ").append(versionText()).append('\n');
        sb.append(Build.MANUFACTURER).append(' ').append(Build.MODEL).append(", Android ")
          .append(Build.VERSION.RELEASE).append(" (API ").append(Build.VERSION.SDK_INT).append("), ")
          .append(Build.SUPPORTED_ABIS.length > 0 ? Build.SUPPORTED_ABIS[0] : "?").append('\n');
        sb.append("\n===== omt-log.txt (last run) =====\n").append(tail(new File(base, "omt-log.txt"), 120 * 1024));
        sb.append("\n\n===== omt-log.prev.txt (run before) =====\n").append(tail(new File(base, "omt-log.prev.txt"), 60 * 1024));
        Intent i = new Intent(Intent.ACTION_SEND);
        i.setType("text/plain");
        i.putExtra(Intent.EXTRA_SUBJECT, "SpongeBob EOTM log " + versionText());
        i.putExtra(Intent.EXTRA_TEXT, sb.toString());
        startActivity(Intent.createChooser(i, "Share log"));
    }

    // ------------------------------------------------------------------ install screen

    private void showImport() {
        mPage = "import";
        launcher.page("Import game", "Bring your own CD image", "Choose a game CD image (.iso).\nAbout 1 GB of space is needed.\nReinstall replaces game files.",
            "Choose CD image (.iso)", "Back");
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
        importing = true;
        showImport();
        launcher.status("Installing from the CD image…", true);

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
                        ui.post(() -> launcher.status(what + "\n" + (p / 10) + "%", true));
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
                importing = false;
                launcher.status("", false);
                if (err == null) {
                    showMain();
                } else {
                    launcher.status("Import failed. Choose another CD image.", false);
                    new android.app.AlertDialog.Builder(this).setTitle("Import failed").setMessage(err).setPositiveButton("OK", null).show();
                }
            });
        }).start();
    }

    private static void deleteTree(File f) {
        if (f == null || !f.exists()) return;
        File[] kids = f.listFiles();
        if (kids != null) for (File k : kids) deleteTree(k);
        //noinspection ResultOfMethodCallIgnored
        f.delete();
    }
}
