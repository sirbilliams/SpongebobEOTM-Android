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

    private static final int PICK_ISO = 1, EXPORT_ALL = 2, EXPORT_ONE = 3, IMPORT_ZIP = 4, PICK_ART = 5, PICK_HD = 6;
    static final String ART_FILE = "boxart.jpg";

    private String mPage = "main";
    private LauncherView launcher;
    private LauncherSound sound;
    private LauncherBottom bottom;
    private android.graphics.Bitmap art;
    private SaveFiles.Info lastSave;
    private android.graphics.Bitmap lastThumb;
    private String exportOne;                       // file of the save chosen for "Export one save"
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
        try { sound = new LauncherSound(this); launcher.setSounds(sound); } catch (RuntimeException e) { sound = null; }
        bottom = new LauncherBottom(this, new LauncherView.Actions() {
            public void select(int dial) { turnDial(dial); }
            public void back() { }
        }, new LauncherBottom.Fill() {
            public void fill(LauncherView v) { fillBottom(v); }
        });
        loadArt();
        refreshSave();
        showMain();
        launcher.requestFocus();
    }

    @Override protected void onResume() {
        super.onResume();
        // port: animator duration scale 0 (Settings > Accessibility > Remove animations): a still launcher, no dives.
        float scale = 1f;
        try { scale = android.provider.Settings.Global.getFloat(getContentResolver(), android.provider.Settings.Global.ANIMATOR_DURATION_SCALE, 1f); }
        catch (RuntimeException ignored) { }
        launcher.setReducedMotion(scale == 0f);
        refreshSave();
        if (mPage.equals("play")) showPlay();
        bottom.resume(panelOn());
        bottom.refresh();
    }

    @Override protected void onPause() {
        bottom.pause();
        super.onPause();
    }

    @Override protected void onDestroy() {
        if (sound != null) sound.release();
        super.onDestroy();
    }

    /** The save Resume would load (Save= in settings.cfg, else the newest), for the Play page and the bottom screen. */
    private void refreshSave() {
        lastSave = SaveFiles.last(new File(gameDir(), "saved"), new File(getFilesDir(), "settings.cfg"), new File(gameDir(), "settings.cfg"));
        lastThumb = lastSave != null ? lastSave.bitmap() : null;
        if (lastSave != null) launcher.setCard(lastThumb, "Chapter " + lastSave.chapter(), lastSave.place(), "Saved " + lastSave.date(), lastSave.chapter());
        else launcher.setCard(null, null, null, null, 0);
    }

    private void loadArt() {
        File f = new File(getFilesDir(), ART_FILE);
        art = f.isFile() ? android.graphics.BitmapFactory.decodeFile(f.getPath()) : null;
        launcher.setArt(art);
        if (bottom != null) bottom.refresh();
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
        launcher.page(0, "Welcome aboard", installed() ? "Ready to set sail" : "Your adventure awaits",
            installed() ? "Game files installed.\nChoose Play to begin." : "Import your own CD image\nto begin your adventure.",
            "Play", "Import game", "Saves", "Settings", "Engine room", "About", "Quit");
    }

    private void showPlay() {
        mPage = "play";
        boolean save = lastSave != null;
        launcher.page(1, "Set sail", save ? "Welcome back, captain" : "Adventure on the horizon",
            save ? "Choose Resume in the game's menu to carry on." : "Start the game and choose\na new adventure.",
            installed() ? "Start game" : "Import game files first", "Back");
        launcher.withCard();
    }

    private boolean panelOn() {
        return CompanionPanel.wanted(getSharedPreferences(CompanionPanel.PREFS, MODE_PRIVATE), true);
    }
    private int bottomMode() {
        return getSharedPreferences(CompanionPanel.PREFS, MODE_PRIVATE | MODE_MULTI_PROCESS).getInt(CompanionPanel.KEY_MODE, CompanionView.MODE_PANEL);
    }
    private boolean panelFocusable() {
        return getSharedPreferences(CompanionPanel.PREFS, MODE_PRIVATE | MODE_MULTI_PROCESS).getBoolean(CompanionPanel.KEY_FOCUSABLE, false);
    }
    private static final String[] WIDESCREEN = {"Fill", "Side panels", "Stretch", "Off"};
    private int widescreen() { return Math.floorMod(getSharedPreferences("settings", MODE_PRIVATE).getInt("widescreen", 0), 4); }
    private boolean padCursor() { return getSharedPreferences("settings", MODE_PRIVATE).getBoolean("pad_cursor", true); }
    private int cursor() { return Math.floorMod(getSharedPreferences("settings", MODE_PRIVATE).getInt("cursor", 0), 4); }
    private void showSettings() {
        mPage = "settings";
        launcher.page(2, "Captain's settings", "Make yourself at home", "Choices apply on next launch.\nTouchpad: bottom screen moves the cursor.\nPanel window: only if bottom touch fails.",
            "Widescreen: " + WIDESCREEN[widescreen()],
            "Bottom-screen panel: " + (panelOn() ? "On" : "Off"),
            "Bottom screen: " + (bottomMode() == CompanionView.MODE_PAD ? "Touchpad" : "Panel"),
            "Panel window: " + (panelFocusable() ? "Takes focus" : "Safe (default)"),
            "Right-stick cursor: " + (padCursor() ? "On" : "Off"),
            "Cursor: " + LauncherView.CURSORS[cursor()],
            "Box art: " + (art != null ? "Your picture" : "Pick an image"), "Back");
    }
    private boolean hdOn() { return getSharedPreferences("settings", MODE_PRIVATE).getBoolean("hd_bg", true); }
    /** "3 scenes, 12.4 MB" for the installed HD pack (docs/hd-pack.md), or null without one. */
    private String hdInfo() {
        File data = gameDir();
        if (HdPack.count(data) == 0) return null;
        File[] scenes = new File(data, HdPack.DIR + "/sceneart").listFiles();
        int n = 0;
        if (scenes != null) for (File f : scenes) if (f.isDirectory()) n++;
        long bytes = treeSize(new File(data, HdPack.DIR));
        return n + (n == 1 ? " scene, " : " scenes, ") + (bytes >= 1048576 ? String.format(java.util.Locale.US, "%.1f MB", bytes / 1048576.0) : (bytes / 1024) + " KB");
    }
    private static long treeSize(File f) {
        File[] kids = f.listFiles();
        if (kids == null) return f.length();
        long n = 0;
        for (File k : kids) n += treeSize(k);
        return n;
    }
    private void showTest() {
        mPage = "test";
        launcher.page(3, "Engine room", "Tools & diagnostics", "Logs include the current\nand previous run.",
            "Share log", "Touch test", "App version: " + versionText(), "Reset settings", "Crash test", "Reinstall from CD image", "Tools", "Back");
    }
    private void showTools() {
        mPage = "tools";
        String hd = hdInfo();
        launcher.page(4, "Tools", "Assets & HD pack", "Browse the game's models and scenes.\n" + (hd != null ? "HD pack installed: " + hd + "." : "No HD pack installed.") + "\nHD changes apply on next launch.",
            "Model viewer", "Scene explorer", "HD backgrounds: " + (hdOn() ? "On" : "Off"), "HD pack: Install (.zip)", "HD pack: Remove", "Back");
    }
    private void showAbout() {
        mPage = "about";
        launcher.page(1, "About this voyage", "An independent engine port", "Original launcher shapes.\nBring your own game files.",
            "Version: " + versionText(), "Open-source licences", "Back");
    }
    private void showSaves() {
        mPage = "saves";
        int n = SaveFiles.list(new File(gameDir(), "saved"), false).size();
        launcher.page(2, "Ship's locker", n == 0 ? "No saves on board yet" : n == 1 ? "1 save on board" : n + " saves on board",
            "Export makes one .zip where you choose.\nImport shows every save before anything is replaced.",
            "Export all saves (.zip)", "Export one save", "Import saves (.zip)", "Back");
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
            switch(row) { case 0: showPlay(); break; case 1: showImport(); break; case 2: showSaves(); break; case 3: showSettings(); break; case 4: showTest(); break; case 5: showAbout(); break; default: finish(); } break;
        case "play": if(row==1)showMain(); else if(installed())play(false); else showImport(); break;
        case "import": if(row==0)pickIso(); else showMain(); break;
        case "saves":
            switch(row) { case 0: exportAll(); break; case 1: chooseExport(); break; case 2: pickZip(); break; default: showMain(); } break;
        case "settings":
            if(row==7) {showMain();break;}
            if(row==6) {chooseArt();break;}
            applySetting(row);
            showSettings(); launcher.focusRow(row); bottom.refresh(); break;
        case "test":
            switch(row) {case 0:shareLog();break;case 1:startActivity(new Intent(this, TouchTestActivity.class), onTopScreen());break;case 2:break;case 3:resetSettings();break;case 4:if(installed())play(true);else showImport();break;case 5:showImport();break;case 6:showTools();break;default:showMain();} break;
        case "tools":
            switch(row) {case 0:viewAssets("models");break;case 1:viewAssets("scene");break;case 2:applySetting(6);showTools();launcher.focusRow(2);break;case 3:pickHd();break;case 4:removeHd();break;default:showTest();} break;
        case "about": if(row==1)showLicences(); else if(row==2)showMain(); break;
        }
    }

    // ------------------------------------------------------------------ bottom screen

    private void fillBottom(LauncherView v) {
        if (lastSave != null) v.setCard(lastThumb, "Chapter " + lastSave.chapter(), lastSave.place(), "Saved " + lastSave.date(), lastSave.chapter());
        else v.setCard(null, null, null, null, 0);
        v.setArt(art);
        v.setDials(new String[] {"Widescreen", "Right stick", "Cursor", "Bottom screen"},
            new String[] {WIDESCREEN[widescreen()], padCursor() ? "On" : "Off", LauncherView.CURSORS[cursor()], bottomMode() == CompanionView.MODE_PAD ? "Touchpad" : "Panel"},
            new int[] {widescreen(), padCursor() ? 1 : 0, cursor(), bottomMode() == CompanionView.MODE_PAD ? 1 : 0}, new int[] {4, 2, 4, 2});
    }

    /** A Settings row's preference, cycled (rows 0-6); the bottom screen's dials use the same. */
    private void applySetting(int row) {
        if(row==0) getSharedPreferences("settings", MODE_PRIVATE).edit().putInt("widescreen", Math.floorMod(widescreen()+1,4)).apply();
        if(row==1) getSharedPreferences(CompanionPanel.PREFS, MODE_PRIVATE).edit().putBoolean(CompanionPanel.KEY_ON,!panelOn()).apply();
        if(row==2) getSharedPreferences(CompanionPanel.PREFS, MODE_PRIVATE).edit().putInt(CompanionPanel.KEY_MODE, bottomMode() == CompanionView.MODE_PAD ? CompanionView.MODE_PANEL : CompanionView.MODE_PAD).apply();
        if(row==3) getSharedPreferences(CompanionPanel.PREFS, MODE_PRIVATE).edit().putBoolean(CompanionPanel.KEY_FOCUSABLE, !panelFocusable()).apply();
        if(row==4) getSharedPreferences("settings", MODE_PRIVATE).edit().putBoolean("pad_cursor",!padCursor()).apply();
        // port: same persistent preference cycle as widescreen.
        if(row==5) getSharedPreferences("settings", MODE_PRIVATE).edit().putInt("cursor", Math.floorMod(cursor()+1,4)).apply();
        if(row==6) getSharedPreferences("settings", MODE_PRIVATE).edit().putBoolean("hd_bg", !hdOn()).apply();
        if(row==1) bottom.resume(panelOn());
    }

    /** A bottom-screen dial: the same preference as its Settings row. */
    private void turnDial(int dial) {
        int[] rows = {0, 4, 5, 2};
        if (dial < 0 || dial >= rows.length) return;
        applySetting(rows[dial]);
        if (mPage.equals("settings")) { showSettings(); launcher.focusRow(rows[dial]); }
        bottom.refresh();
    }

    // ------------------------------------------------------------------ box art

    private void chooseArt() {
        if (art == null) { pickArt(); return; }
        new android.app.AlertDialog.Builder(this).setTitle("Box art")
            .setItems(new String[] {"Pick another image", "Remove the picture"}, (d, which) -> {
                if (which == 0) pickArt();
                else { new File(getFilesDir(), ART_FILE).delete(); loadArt(); showSettings(); launcher.focusRow(6); }
            }).setNegativeButton("Back", null).show();
    }
    private void pickArt() {
        Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        i.addCategory(Intent.CATEGORY_OPENABLE);
        i.setType("image/*");
        startActivityForResult(i, PICK_ART);
    }
    /** Keeps a copy of the picked picture (at most 1280 px, JPEG) in the app's own files; it is never bundled. */
    private void storeArt(Uri uri) {
        try {
            android.graphics.BitmapFactory.Options o = new android.graphics.BitmapFactory.Options();
            o.inJustDecodeBounds = true;
            try (InputStream in = getContentResolver().openInputStream(uri)) { android.graphics.BitmapFactory.decodeStream(in, null, o); }
            int sample = 1;
            while (o.outWidth / (sample * 2) >= 1280 || o.outHeight / (sample * 2) >= 1280) sample *= 2;
            android.graphics.BitmapFactory.Options d = new android.graphics.BitmapFactory.Options();
            d.inSampleSize = sample;
            android.graphics.Bitmap b;
            try (InputStream in = getContentResolver().openInputStream(uri)) { b = android.graphics.BitmapFactory.decodeStream(in, null, d); }
            if (b == null) throw new IOException("not a picture");
            File tmp = new File(getFilesDir(), ART_FILE + ".tmp");
            try (java.io.FileOutputStream out = new java.io.FileOutputStream(tmp)) { b.compress(android.graphics.Bitmap.CompressFormat.JPEG, 90, out); }
            if (!tmp.renameTo(new File(getFilesDir(), ART_FILE))) throw new IOException("could not keep the picture");
            loadArt();
        } catch (IOException | SecurityException | OutOfMemoryError e) {
            Toast.makeText(this, "Couldn't use that picture: " + e.getMessage(), Toast.LENGTH_LONG).show();
        }
        showSettings(); launcher.focusRow(6);
    }

    // ------------------------------------------------------------------ saves export / import

    private File savedDir() { return new File(gameDir(), "saved"); }

    private void exportAll() {
        if (SaveFiles.list(savedDir(), false).isEmpty()) { Toast.makeText(this, "No saves to export yet", Toast.LENGTH_SHORT).show(); return; }
        createZip("eotm-saves-" + new java.text.SimpleDateFormat("yyyyMMdd", java.util.Locale.ROOT).format(new java.util.Date()) + ".zip", EXPORT_ALL);
    }
    private void createZip(String name, int code) {
        Intent i = new Intent(Intent.ACTION_CREATE_DOCUMENT);
        i.addCategory(Intent.CATEGORY_OPENABLE);
        i.setType("application/zip");
        i.putExtra(Intent.EXTRA_TITLE, name);
        startActivityForResult(i, code);
    }
    private void chooseExport() {
        final java.util.List<SaveFiles.Info> saves = SaveFiles.list(savedDir(), false);
        if (saves.isEmpty()) { Toast.makeText(this, "No saves to export yet", Toast.LENGTH_SHORT).show(); return; }
        String[] labels = new String[saves.size()];
        for (int k = 0; k < labels.length; k++) {
            SaveFiles.Info s = saves.get(k);
            labels[k] = s.label() + (s.name.length() > 0 ? " - " + s.name : "") + "\n" + s.place() + ", " + s.date();
        }
        new android.app.AlertDialog.Builder(this).setTitle("Export one save")
            .setItems(labels, (d, which) -> { exportOne = saves.get(which).file; createZip("eotm-" + exportOne.replace(".sb2", "") + ".zip", EXPORT_ONE); })
            .setNegativeButton("Back", null).show();
    }
    private void writeZip(final Uri uri, final String only) {
        final java.util.List<SaveFiles.Info> saves = SaveFiles.list(savedDir(), false);
        if (only != null) for (java.util.Iterator<SaveFiles.Info> it = saves.iterator(); it.hasNext(); ) if (!it.next().file.equals(only)) it.remove();
        new Thread(() -> {
            String msg;
            try (java.io.OutputStream out = getContentResolver().openOutputStream(uri, "w")) {
                if (out == null) throw new IOException("can't write there");
                SaveFiles.exportZip(savedDir(), saves, out);
                msg = saves.size() == 1 ? "Exported 1 save" : "Exported " + saves.size() + " saves";
            } catch (IOException | SecurityException e) {
                msg = "Export failed: " + e.getMessage();
            }
            final String m = msg;
            ui.post(() -> Toast.makeText(this, m, Toast.LENGTH_LONG).show());
        }).start();
    }
    private void pickZip() {
        Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        i.addCategory(Intent.CATEGORY_OPENABLE);
        i.setType("*/*");
        i.putExtra(Intent.EXTRA_MIME_TYPES, new String[] {"application/zip", "application/x-zip-compressed", "application/octet-stream"});
        startActivityForResult(i, IMPORT_ZIP);
    }
    /** Reads the .zip, then shows each save (picture, slot, place, date) and what it would replace before writing anything. */
    private void previewZip(Uri uri) {
        java.util.List<SaveFiles.Info> found;
        try (InputStream in = getContentResolver().openInputStream(uri)) {
            if (in == null) throw new IOException("can't open the file");
            found = SaveFiles.readZip(in);
        } catch (IOException | SecurityException e) {
            new android.app.AlertDialog.Builder(this).setTitle("Import failed").setMessage("Couldn't read the .zip: " + e.getMessage()).setPositiveButton("OK", null).show();
            return;
        }
        if (found.isEmpty()) {
            new android.app.AlertDialog.Builder(this).setTitle("No saves found").setMessage("The .zip holds no slotN.sb2 or quick.sb2 save files.").setPositiveButton("OK", null).show();
            return;
        }
        final java.util.List<SaveFiles.Info> saves = found;
        int replaced = 0;
        float dp = getResources().getDisplayMetrics().density;
        android.widget.LinearLayout list = new android.widget.LinearLayout(this);
        list.setOrientation(android.widget.LinearLayout.VERTICAL);
        list.setPadding((int) (16 * dp), (int) (8 * dp), (int) (16 * dp), 0);
        for (SaveFiles.Info s : saves) {
            boolean used = SaveFiles.used(savedDir(), s.file);
            if (used) replaced++;
            android.widget.LinearLayout row = new android.widget.LinearLayout(this);
            row.setPadding(0, (int) (6 * dp), 0, (int) (6 * dp));
            android.widget.ImageView pic = new android.widget.ImageView(this);
            android.graphics.Bitmap b = s.bitmap();
            if (b != null) pic.setImageBitmap(b); else pic.setBackgroundColor(0xff5f8a8c);
            pic.setScaleType(android.widget.ImageView.ScaleType.CENTER_CROP);
            row.addView(pic, new android.widget.LinearLayout.LayoutParams((int) (128 * dp), (int) (96 * dp)));
            TextView text = new TextView(this);
            text.setPadding((int) (12 * dp), 0, 0, 0);
            text.setText(s.label() + (s.name.length() > 0 ? " - " + s.name : "") + "\nChapter " + s.chapter() + ": " + s.place()
                + (s.modified > 0 ? "\n" + s.date() : "") + (used ? "\nReplaces the save already in this slot" : "\nFree slot"));
            row.addView(text);
            list.addView(row);
        }
        android.widget.ScrollView scroll = new android.widget.ScrollView(this);
        scroll.addView(list);
        android.app.AlertDialog.Builder dlg = new android.app.AlertDialog.Builder(this)
            .setTitle(saves.size() == 1 ? "Import 1 save?" : "Import " + saves.size() + " saves?").setView(scroll)
            .setNegativeButton("Cancel", null)
            .setNeutralButton("Into free slots", (d, w) -> applyImport(saves, true))
            .setPositiveButton(replaced > 0 ? "Replace (" + replaced + ")" : "Import", (d, w) -> applyImport(saves, false));
        dlg.show();
    }
    private void applyImport(java.util.List<SaveFiles.Info> saves, boolean freeSlots) {
        try {
            java.util.List<String> done = SaveFiles.importSaves(savedDir(), saves, freeSlots);
            StringBuilder b = new StringBuilder();
            for (String line : done) b.append(line).append('\n');
            new android.app.AlertDialog.Builder(this).setTitle("Saves imported").setMessage(b.toString().trim()).setPositiveButton("OK", null).show();
        } catch (IOException | SecurityException e) {
            new android.app.AlertDialog.Builder(this).setTitle("Import failed").setMessage(e.getMessage()).setPositiveButton("OK", null).show();
        }
        refreshSave(); bottom.refresh();
        showSaves(); launcher.focusRow(2);
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
        bottom.resume(panelOn()); bottom.refresh();
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
        // port: the panel's display choice and the bottom-screen touch diagnosis (docs/bottom-touch.md)
        sb.append("\n\n===== touch-log.txt (bottom-screen touch: devices, displays, first events) =====\n").append(tail(new File(base, TouchDiag.LOG_FILE), 24 * 1024));
        sb.append("\n\n===== display-log.txt (which display is the bottom screen) =====\n").append(tail(new File(base, BottomScreen.LOG_FILE), 8 * 1024));
        Intent i = new Intent(Intent.ACTION_SEND);
        i.setType("text/plain");
        i.putExtra(Intent.EXTRA_SUBJECT, "SpongeBob EOTM log " + versionText());
        i.putExtra(Intent.EXTRA_TEXT, sb.toString());
        startActivity(Intent.createChooser(i, "Share log"));
    }

    // ------------------------------------------------------------------ asset viewer, HD pack (docs/asset-viewer.md, docs/hd-pack.md)

    private void viewAssets(String what) {
        if (!installed()) { showImport(); return; }
        startActivity(new Intent(this, SpongeActivity.class).putExtra(SpongeActivity.EXTRA_ASSET_VIEW, what), onTopScreen());
    }

    private void pickHd() {
        if (!installed()) { showImport(); return; }
        Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        i.addCategory(Intent.CATEGORY_OPENABLE);
        i.setType("application/zip");
        startActivityForResult(i, PICK_HD);
    }

    private void removeHd() {
        if (importing) return;
        boolean had = HdPack.count(gameDir()) > 0;
        boolean ok = HdPack.remove(gameDir());
        showTools(); launcher.focusRow(4);
        launcher.status(!ok ? "Couldn't remove the HD pack." : had ? "HD pack removed." : "No HD pack installed.", false);
    }

    private void installHd(final Uri uri) {
        importing = true;
        showTools();
        launcher.status("Installing the HD pack…", true);
        final File data = gameDir();
        new Thread(() -> {
            long size = -1;
            try (android.database.Cursor c = getContentResolver().query(uri, new String[] {android.provider.OpenableColumns.SIZE}, null, null, null)) {
                if (c != null && c.moveToFirst() && !c.isNull(0)) size = c.getLong(0);
            } catch (RuntimeException e) { /* unknown size */ }
            final int[] last = { -1 };
            final HdPack.Result r = HdPack.install(this, uri, data, size, permille -> {
                if (permille != last[0]) {
                    last[0] = permille;
                    ui.post(() -> launcher.status("Installing the HD pack…\n" + (permille / 10) + "%", true));
                }
            });
            ui.post(() -> {
                importing = false;
                showTools(); launcher.focusRow(3);
                launcher.status(r.error == null ? r.message() : "HD pack failed. The old pack is kept.", false);
                if (r.error != null) new android.app.AlertDialog.Builder(this).setTitle("HD pack failed").setMessage(r.error).setPositiveButton("OK", null).show();
            });
        }).start();
    }

    // ------------------------------------------------------------------ install screen

    private void showImport() {
        mPage = "import";
        launcher.page(1, "Import game", "Bring your own CD image", "Choose a game CD image (.iso).\nAbout 1 GB of space is needed.\nReinstall replaces game files.",
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
        if (resultCode != RESULT_OK || data == null || data.getData() == null) return;
        Uri uri = data.getData();
        if (requestCode == PICK_ISO) importIso(uri);
        else if (requestCode == EXPORT_ALL) writeZip(uri, null);
        else if (requestCode == EXPORT_ONE && exportOne != null) writeZip(uri, exportOne);
        else if (requestCode == IMPORT_ZIP) previewZip(uri);
        else if (requestCode == PICK_ART) storeArt(uri);
        else if (requestCode == PICK_HD) installHd(uri);
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
