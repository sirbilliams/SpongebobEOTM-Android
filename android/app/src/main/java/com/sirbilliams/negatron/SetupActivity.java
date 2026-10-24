package com.sirbilliams.negatron;

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

import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.FilterInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Locale;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

/**
 * First screen. If the game files are readable it starts the engine straight away; otherwise it lets
 * the user pick the game's CD image (.iso), which is installed like the CD's setup does, or a zip of
 * the game's Program_Executable_Files folder, and puts the files into files/game, which the app owns. Files copied in over USB can be visible to
 * the app but not readable on some Android builds; files the app writes itself always are.
 */
public class SetupActivity extends Activity {

    private static final int PICK_ZIP = 1;
    private static final int PICK_BOXART = 2;
    private static final int PICK_BACKGROUND = 3;
    private static final int EXPORT_ALL = 4, EXPORT_SLOT = 5, IMPORT_SAVES = 6;
    private static final int PICK_HD = 7;
    private static final String[] GAME_DIRS = { "omt", "gam", "grn", "ase", "png", "dat", "save" };

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

        if (findReadableData() != null) {
            buildChooser();
            return;
        }
        buildUi(base);
    }

    /** Directory holding omt/level1.omt that we can actually read, or null. */
    private File findReadableData() {
        File base = getExternalFilesDir(null);
        if (base == null) return null;
        File[] candidates = { new File(base, "game"), base, new File(base, "Program_Executable_Files") };
        for (File dir : candidates) {
            if (canRead(new File(new File(dir, "omt"), "level1.omt"))) return dir;
        }
        return null;
    }

    private static boolean canRead(File f) {
        try (InputStream in = new FileInputStream(f)) {
            return in.read() >= 0;
        } catch (IOException | SecurityException e) {
            return false;
        }
    }

    private void startGame() {
        buildChooser();
    }

    private void launch(String lib) {
        launch(lib, null, null);
    }

    /** Starts the engine: lib "negatron" (the game) or "main" (the model viewer); testPlay = a level
     *  to play straight away (NEG_TEST_PLAY); start = where the game starts (NEG_START: menu, new,
     *  continue, load:<slot>). */
    private void launch(String lib, String testPlay, String start) {
        hideBottom();
        Intent i = new Intent(this, NegatronActivity.class);
        i.putExtra(NegatronActivity.EXTRA_LIB, lib);
        if (testPlay != null) i.putExtra(NegatronActivity.EXTRA_TEST_PLAY, testPlay);
        if (start != null) i.putExtra(NegatronActivity.EXTRA_START, start);
        startActivity(i, onTopScreen());
        finish();
    }

    /** port: the Asset Viewer: the game exploring a level, or its model viewer when gam is null
     *  (NEG_ASSET_VIEW=<gam> / models); the launcher comes back on the Asset Viewer's page afterwards. */
    private void launchAssetView(String gam) {
        hideBottom();
        Intent i = new Intent(this, NegatronActivity.class);
        i.putExtra(NegatronActivity.EXTRA_LIB, "negatron");
        i.putExtra(NegatronActivity.EXTRA_ASSET_VIEW, gam != null ? gam : "models");
        i.putExtra(EXTRA_RETURN, gam != null ? "explore:" + gam : "models");
        startActivity(i, onTopScreen());
        finish();
    }

    /** port: the game always opens on the default (top) display, never a TV / cast display, so
     *  screen mirroring sends the game view to the TV (docs/controls.md "Casting / TV"). */
    static Bundle onTopScreen() {
        if (android.os.Build.VERSION.SDK_INT < 26) return null;
        try {
            return android.app.ActivityOptions.makeBasic()
                    .setLaunchDisplayId(android.view.Display.DEFAULT_DISPLAY).toBundle();
        } catch (RuntimeException e) {
            return null;
        }
    }

    // ------------------------------------------------------------------ port: the launcher menu

    /** Intent extra: show the launcher even when the original menu is chosen. */
    static final String EXTRA_LAUNCHER = "launcher";
    /** Intent extra: the game has just ended (NegatronActivity); the launcher shows its main menu. */
    static final String EXTRA_FROM_GAME = "from_game";
    /** Intent extra, passed through the game: the Asset Viewer page to come back to ("models", "explore:<gam>"). */
    static final String EXTRA_RETURN = "return_to";
    /** The player's box art picture, copied into the app's private files (never shipped in the repo). */
    static final String BOX_ART = "boxart.img";

    /** The player's menu background picture (the launcher's main screen), likewise. */
    static final String BACKGROUND = "background.img";

    private File boxArtFile() { return new File(getFilesDir(), BOX_ART); }

    private File backgroundFile() { return new File(getFilesDir(), BACKGROUND); }

    private void pickPicture(int request) {
        Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        i.addCategory(Intent.CATEGORY_OPENABLE);
        i.setType("image/*");
        try {
            startActivityForResult(i, request);
        } catch (android.content.ActivityNotFoundException e) {
            // no picker on this device
        }
    }

    /** Decodes a picture at most about 2048 pixels on its long side (null if it isn't one). */
    private static android.graphics.Bitmap decodeScaled(File f) {
        if (!f.isFile()) return null;
        android.graphics.BitmapFactory.Options o = new android.graphics.BitmapFactory.Options();
        o.inJustDecodeBounds = true;
        android.graphics.BitmapFactory.decodeFile(f.getPath(), o);
        if (o.outWidth <= 0 || o.outHeight <= 0) return null;
        int sample = 1;
        while (Math.max(o.outWidth, o.outHeight) / sample > 2048) sample *= 2;
        o = new android.graphics.BitmapFactory.Options();
        o.inSampleSize = sample;
        try {
            return android.graphics.BitmapFactory.decodeFile(f.getPath(), o);
        } catch (OutOfMemoryError e) {
            return null;
        }
    }

    /** Settings -> Box art / Menu background: copies the chosen picture into the app's private
     *  files (name) and shows it. */
    private void setPicture(Uri uri, String name) {
        File tmp = new File(getFilesDir(), name + ".tmp");
        try (InputStream in = getContentResolver().openInputStream(uri);
             OutputStream out = new FileOutputStream(tmp)) {
            if (in == null) throw new IOException("can't open the picture");
            byte[] buf = new byte[64 * 1024];
            int n;
            while ((n = in.read(buf)) > 0) out.write(buf, 0, n);
        } catch (IOException | SecurityException e) {
            tmp.delete();
            return;
        }
        android.graphics.Bitmap b = decodeScaled(tmp);
        if (b == null) {                                    // not a picture: keep the old one
            tmp.delete();
            return;
        }
        File f = new File(getFilesDir(), name);
        f.delete();
        if (!tmp.renameTo(f)) {
            tmp.delete();
            return;
        }
        if (mMenu != null) {
            if (name.equals(BOX_ART)) mMenu.boxArt = b;
            else mMenu.background = b;
            if (mMenu.page() != null && mMenu.page().id.equals("settings")) mMenu.replace(settingsPage());
            else mMenu.changed();
        }
    }

    /** The game's own pictures for the launcher, saved by the game (NEG_LAUNCHER_ART; LauncherArt.h). */
    static final String ART_DIR = "launcher";

    /** Picture key (LauncherModel.pics) -> the file the game saves for it. */
    private static final String[][] PICS = {
        { "new", "museum.png" }, { "load", "loadsave.png" }, { "saves", "menu.png" },
        { "settings", "options.png" }, { "test", "inventory.png" }, { "quit", "credits.png" } };

    /** The game's pictures (small: the info window shows them at most 640 x 480). */
    private void loadPictures() {
        File dir = new File(getExternalFilesDir(null), ART_DIR);
        mMenu.pics.clear();
        if (!LauncherView.GAME_PICTURES) {                  // the options show the drawings only
            android.graphics.Bitmap pda = android.graphics.BitmapFactory.decodeFile(new File(dir, "menu.png").getPath());
            mMenu.art = pda;                                // the bottom screen's fallback
            return;
        }
        for (String[] k : PICS) putPicture(k[0], new File(dir, k[1]));
        // Continue: the autosave's level (its loading picture), else the general loading screen
        LauncherPages.Slot auto = LauncherPages.slots(saveDir())[0];
        String gam = auto.gam.toLowerCase(Locale.ROOT);
        if (gam.endsWith(".gam")) gam = gam.substring(0, gam.length() - 4);
        if (!auto.present || gam.length() == 0 || !putPicture("continue", new File(dir, "level-" + gam + ".png")))
            putPicture("continue", new File(dir, "loading.png"));
        mMenu.art = mMenu.pics.get("saves");                        // the PDA: the bottom screen's fallback
    }

    private boolean putPicture(String key, File f) {
        android.graphics.Bitmap b = f.isFile() ? android.graphics.BitmapFactory.decodeFile(f.getPath()) : null;
        if (b != null) mMenu.pics.put(key, b);
        return b != null;
    }

    private LauncherModel mMenu;
    private LauncherView mTop;
    private LauncherPresentation mBottom;
    private boolean mTestShown;
    private int mTouchItem = -1;

    private final Runnable mTick = new Runnable() {
        @Override
        public void run() {
            if (mTop == null) return;
            mTop.tick();
            if (mBottom != null && mBottom.view != null) mBottom.view.tick();
            ui.postDelayed(this, 50);
        }
    };

    private final Runnable mRevealTest = new Runnable() {
        @Override
        public void run() { revealTest(); }
    };

    private android.content.SharedPreferences prefs() {
        // the game (its own process) changes some of them too: read the file afresh
        return getSharedPreferences(CompanionPanel.PREFS, MODE_PRIVATE | MODE_MULTI_PROCESS);
    }

    /** Game files present: the launcher menu, or straight into the game's own title menu. */
    private void buildChooser() {
        // port: always the launcher (Bill removed the "original menu" style; the game's own Main Menu
        // is still its PDA, opened with Esc / Start in play).
        mMenu = new LauncherModel();
        loadPictures();
        mMenu.boxArt = decodeScaled(boxArtFile());
        mMenu.background = decodeScaled(backgroundFile());
        mMenu.reset(mainPage());
        String ret = getIntent() != null ? getIntent().getStringExtra(EXTRA_RETURN) : null;
        if (ret != null) {                                  // back from the Asset Viewer: its page again
            mMenu.page().sel = Math.max(0, mMenu.page().items.indexOf(findItem(mMenu.page(), "assets")));
            mMenu.push(LauncherPages.assets());
            if (ret.startsWith("explore:")) {
                mMenu.page().sel = 0;
                mMenu.push(LauncherPages.explore(saveDir(), levelFiles(), ret.substring(8)));
            } else mMenu.page().sel = 1;
        }
        mTop = new LauncherView(this, mMenu, false);
        mTop.setFocusable(true);
        mTop.setFocusableInTouchMode(true);
        mTop.setOnTouchListener(new View.OnTouchListener() {
            @Override
            public boolean onTouch(View v, android.view.MotionEvent e) { return touch(e); }
        });
        setContentView(mTop);
        mTop.requestFocus();
        showBottom();
        ui.removeCallbacks(mTick);
        ui.post(mTick);
    }

    private static LauncherModel.Item findItem(LauncherModel.Page p, String id) {
        for (LauncherModel.Item i : p.items) if (i.id.equals(id)) return i;
        return null;
    }

    private File saveDir() {
        File data = findReadableData();
        return new File(data != null ? data : getExternalFilesDir(null), "save");
    }

    private LauncherModel.Page mainPage() {
        return LauncherPages.main(LauncherPages.slots(saveDir())[0], mTestShown);
    }

    private LauncherModel.Page settingsPage() {
        android.content.SharedPreferences p = prefs();
        boolean second = CompanionPanel.findDisplay(this, displays()) != null;
        return hdItems(LauncherPages.settings(p.getBoolean(PREF_WIDESCREEN, true), p.getBoolean(PREF_BILLS_UI, true),
                p.getString(PREF_ENERGY, "a"), p.getString(PREF_HOLO, "a"), CompanionPanel.wanted(p, second), second,
                boxArtFile().isFile(),
                backgroundFile().isFile()));
    }

    // ------------------------------------------------------------------ port: the HD texture pack (HdPack)

    private int mHdProgress = -1;       // permille while a pack is being installed, else -1
    private int mHdCount = -1;          // pictures installed (cached; -1 = count again)

    /** The settings page's HD pack items: what is installed, the install's progress. */
    private LauncherModel.Page hdItems(LauncherModel.Page p) {
        if (mHdCount < 0 && mHdProgress < 0) mHdCount = HdPack.count(findReadableData());
        for (LauncherModel.Item i : p.items) {
            if (i.id.equals("hd")) {
                i.progress = mHdProgress >= 0 ? mHdProgress / 1000f : -1;
                i.value = mHdProgress >= 0 ? (mHdProgress / 10) + "%" : mHdCount > 0 ? mHdCount + " PICS" : "NONE";
            } else if (i.id.equals("hd_remove")) i.enabled = mHdProgress < 0 && mHdCount > 0;
        }
        return p;
    }

    private void hdRefresh() {
        LauncherModel.Page p = mMenu != null ? mMenu.page() : null;
        if (p != null && p.id.equals("settings")) mMenu.replace(settingsPage());
    }

    private void installHd(final Uri uri) {
        final File data = findReadableData();
        if (data == null || mHdProgress >= 0) return;
        mHdProgress = 0;
        hdRefresh();
        final long total = querySize(uri);
        new Thread(() -> {
            final HdPack.Result r = HdPack.install(this, uri, data, total, pm -> ui.post(() -> {
                if (mHdProgress >= 0) { mHdProgress = pm; hdRefresh(); }
            }));
            ui.post(() -> {
                mHdProgress = -1;
                mHdCount = -1;
                hdRefresh();
                toast(r.message());
            });
        }).start();
    }

    private android.hardware.display.DisplayManager displays() {
        return (android.hardware.display.DisplayManager) getSystemService(DISPLAY_SERVICE);
    }

    /** The hidden test item (Select / Y on the pad, or a long press on the title). */
    private void revealTest() {
        if (mMenu == null || mTestShown) return;
        mTestShown = true;
        if (mMenu.depth() == 1) mMenu.replace(mainPage());
        else mMenu.changed();
    }

    private void choose() {
        LauncherModel.Item it = mMenu.selected();
        if (it == null || !it.enabled || it.hint) return;
        String id = it.id;
        android.content.SharedPreferences p = prefs();
        if (id.equals("continue")) launch("negatron", null, "continue");
        else if (id.equals("new")) mMenu.push(LauncherPages.newGame());
        else if (id.equals("new_yes")) launch("negatron", null, "new");
        else if (id.equals("load")) mMenu.push(LauncherPages.load(LauncherPages.slots(saveDir())));
        else if (id.startsWith("slot:")) launch("negatron", null, "load:" + id.substring(5));
        else if (id.equals("settings")) mMenu.push(settingsPage());
        else if (id.equals("test")) mMenu.push(LauncherPages.test());
        else if (id.equals("quit")) finish();
        else if (id.equals("wide")) {
            p.edit().putBoolean(PREF_WIDESCREEN, !p.getBoolean(PREF_WIDESCREEN, true)).commit();
            mMenu.replace(settingsPage());
        } else if (id.equals("energy")) {
            p.edit().putString(PREF_ENERGY, LauncherPages.energyStep(p.getString(PREF_ENERGY, "a"), mStepDir)).commit();
            mMenu.replace(settingsPage());
        } else if (id.equals("holo")) {
            p.edit().putString(PREF_HOLO, LauncherPages.holoStep(p.getString(PREF_HOLO, "a"), mStepDir)).commit();
            mMenu.replace(settingsPage());
        } else if (id.equals("ui")) {
            p.edit().putBoolean(PREF_BILLS_UI, !p.getBoolean(PREF_BILLS_UI, true)).commit();
            mMenu.replace(settingsPage());
        } else if (id.equals("panel")) {
            boolean second = CompanionPanel.findDisplay(this, displays()) != null;
            p.edit().putBoolean(CompanionPanel.KEY_ON, !CompanionPanel.wanted(p, second)).commit();
            mMenu.replace(settingsPage());
            showBottom();
        } else if (id.equals("saves")) mMenu.push(LauncherPages.saves());
        else if (id.equals("export_all")) {
            String date = new java.text.SimpleDateFormat("yyyy-MM-dd", Locale.ROOT).format(new java.util.Date());
            createDocument("application/zip", "negatron-saves-" + date + ".zip", EXPORT_ALL);
        } else if (id.equals("import")) {
            Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            i.addCategory(Intent.CATEGORY_OPENABLE);
            i.setType("*/*");
            try {
                startActivityForResult(i, IMPORT_SAVES);
            } catch (android.content.ActivityNotFoundException e) {
                toast("No file picker on this device.");
            }
        } else if (id.equals("import_yes")) importSaves();
        else if (id.startsWith("to:")) {
            int n = Integer.parseInt(id.substring(3));
            LauncherPages.Incoming f = mIncoming.get(0);
            f.name = LauncherPages.fileName(n);
            f.info.number = n;
            f.replaces = new File(saveDir(), f.name).isFile();
            mMenu.pop();
            mMenu.push(LauncherPages.importPage(mIncoming));
        } else if (id.equals("file")) {
            // a file of the import: its details are on show
        } else if (id.equals("hd")) {
            if (mHdProgress >= 0) return;
            Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            i.addCategory(Intent.CATEGORY_OPENABLE);
            i.setType("*/*");
            i.putExtra(Intent.EXTRA_MIME_TYPES, new String[] { "application/zip", "application/x-zip-compressed",
                    "application/octet-stream" });
            try {
                startActivityForResult(i, PICK_HD);
            } catch (android.content.ActivityNotFoundException e) {
                toast("No file picker on this device.");
            }
        } else if (id.equals("hd_remove")) {
            HdPack.remove(findReadableData());
            mHdCount = -1;
            hdRefresh();
            toast("HD texture pack removed.");
        } else if (id.equals("boxart")) pickPicture(PICK_BOXART);
        else if (id.equals("bg")) pickPicture(PICK_BACKGROUND);
        else if (id.equals("bg_clear")) {
            backgroundFile().delete();
            mMenu.background = null;
            mMenu.replace(settingsPage());
        } else if (id.equals("boxart_clear")) {
            boxArtFile().delete();
            mMenu.boxArt = null;
            mMenu.replace(settingsPage());
        } else if (id.equals("levels")) mMenu.push(LauncherPages.levels(levelFiles()));
        else if (id.startsWith("level:")) launch("negatron", id.substring(6), null);
        else if (id.equals("viewer")) launch("main");
        else if (id.equals("assets")) mMenu.push(LauncherPages.assets());           // port: the Asset Viewer
        else if (id.equals("explore")) mMenu.push(LauncherPages.explore(saveDir(), levelFiles(), null));
        else if (id.startsWith("explore:")) launchAssetView(id.substring(8));
        else if (id.equals("models")) launchAssetView(null);
        else if (id.equals("log")) shareLog();
        else if (id.equals("back")) back();
    }

    private void back() {
        if (!mMenu.pop()) return;
        if (mMenu.depth() == 1) mMenu.replace(mainPage());       // the autosave may have changed
    }

    // ------------------------------------------------------------------ saves: export / import

    private List<LauncherPages.Incoming> mIncoming = new ArrayList<>();
    private int mExportSlot = -1;
    private boolean mTouchLong;

    /** Touch: holding a save slot on the Load Game page exports it. */
    private final Runnable mExportHold = new Runnable() {
        @Override
        public void run() {
            if (exportSelectedSlot()) mTouchLong = true;
        }
    };

    private void toast(String s) {
        android.widget.Toast.makeText(this, s, android.widget.Toast.LENGTH_LONG).show();
    }

    private void createDocument(String type, String name, int request) {
        Intent i = new Intent(Intent.ACTION_CREATE_DOCUMENT);
        i.addCategory(Intent.CATEGORY_OPENABLE);
        i.setType(type);
        i.putExtra(Intent.EXTRA_TITLE, name);
        try {
            startActivityForResult(i, request);
        } catch (android.content.ActivityNotFoundException e) {
            toast("No file picker on this device.");
        }
    }

    /** Load Game page, a slot with a save selected: export it (Y, or a long touch). */
    private boolean exportSelectedSlot() {
        LauncherModel.Page p = mMenu.page();
        LauncherModel.Item it = mMenu.selected();
        if (p == null || !p.id.equals("load") || it == null || !it.id.startsWith("slot:")) return false;
        int n = Integer.parseInt(it.id.substring(5));
        if (!new File(saveDir(), LauncherPages.fileName(n)).isFile()) return false;
        mExportSlot = n;
        createDocument("application/octet-stream", LauncherPages.fileName(n), EXPORT_SLOT);
        return true;
    }

    private void exportAll(Uri uri) {
        File[] files = saveDir().listFiles();
        int n = 0;
        try (OutputStream raw = getContentResolver().openOutputStream(uri);
             java.util.zip.ZipOutputStream zip = new java.util.zip.ZipOutputStream(raw)) {
            if (raw == null) throw new IOException("can't write there");
            byte[] buf = new byte[64 * 1024];
            if (files != null) {
                for (File f : files) {
                    if (!f.isFile() || !LauncherPages.exported(f.getName())) continue;
                    ZipEntry e = new ZipEntry(f.getName());
                    e.setTime(f.lastModified());
                    zip.putNextEntry(e);
                    try (InputStream in = new FileInputStream(f)) {
                        int r;
                        while ((r = in.read(buf)) > 0) zip.write(buf, 0, r);
                    }
                    zip.closeEntry();
                    n++;
                }
            }
        } catch (IOException | SecurityException e) {
            toast("Couldn't export the saves: " + e.getMessage());
            return;
        }
        toast("Exported " + n + " file" + (n == 1 ? "" : "s") + ".");
    }

    private void exportSlot(Uri uri) {
        File f = new File(saveDir(), LauncherPages.fileName(mExportSlot));
        try (InputStream in = new FileInputStream(f); OutputStream out = getContentResolver().openOutputStream(uri)) {
            if (out == null) throw new IOException("can't write there");
            byte[] buf = new byte[64 * 1024];
            int r;
            while ((r = in.read(buf)) > 0) out.write(buf, 0, r);
        } catch (IOException | SecurityException e) {
            toast("Couldn't export the slot: " + e.getMessage());
            return;
        }
        toast("Exported " + f.getName() + ".");
    }

    private static final int MAX_SAVE = 4 * 1024 * 1024;

    private static byte[] readAll(InputStream in) throws IOException {
        java.io.ByteArrayOutputStream b = new java.io.ByteArrayOutputStream();
        byte[] buf = new byte[64 * 1024];
        int r;
        while ((r = in.read(buf)) > 0) {
            b.write(buf, 0, r);
            if (b.size() > MAX_SAVE) throw new IOException("too big for a save");
        }
        return b.toByteArray();
    }

    /** Import: a .zip of save files (or one .tsk); shows what it holds before anything is written. */
    private void readImport(Uri uri) {
        List<LauncherPages.Incoming> found = new ArrayList<>();
        String display = displayName(uri);
        boolean single = false;
        try {
            byte[] head = new byte[4];
            try (InputStream in = getContentResolver().openInputStream(uri)) {
                if (in == null) throw new IOException("can't open the file");
                if (in.read(head) < 4) head[0] = 0;
            }
            if (head[0] == 'P' && head[1] == 'K' && head[2] == 3 && head[3] == 4) {
                try (ZipInputStream zip = new ZipInputStream(getContentResolver().openInputStream(uri))) {
                    ZipEntry e;
                    while ((e = zip.getNextEntry()) != null) {
                        if (e.isDirectory()) continue;
                        String base = e.getName().replace('\\', '/');
                        base = base.substring(base.lastIndexOf('/') + 1);   // never a path: no zip-slip
                        int n = LauncherPages.slotOf(base);
                        if (n == -1) continue;
                        LauncherPages.Incoming f = new LauncherPages.Incoming();
                        f.name = n >= 0 ? LauncherPages.fileName(n) : base.toLowerCase(Locale.ROOT);
                        f.data = readAll(zip);
                        f.info.number = n;
                        found.add(f);
                    }
                }
            } else {
                single = true;
                LauncherPages.Incoming f = new LauncherPages.Incoming();
                try (InputStream in = getContentResolver().openInputStream(uri)) {
                    f.data = readAll(in);
                }
                int n = display != null ? LauncherPages.slotOf(display) : -1;
                f.info.number = n;
                f.name = n >= 0 ? LauncherPages.fileName(n) : n == -2 ? display.toLowerCase(Locale.ROOT) : null;
                LauncherPages.readTsk(new java.io.ByteArrayInputStream(f.data), f.info);
                if (!f.info.present && !new String(f.data, 0, Math.min(9, f.data.length), "ISO-8859-1").startsWith("\bDUMMY")) {     // \b = the length byte 8
                    toast("That isn't a save file (.tsk) or a .zip of them.");
                    return;
                }
                found.add(f);
            }
        } catch (IOException | SecurityException e) {
            toast("Couldn't read the file: " + e.getMessage());
            return;
        }
        if (found.isEmpty()) {
            toast("No saves in that file.");
            return;
        }
        for (LauncherPages.Incoming f : found) {
            int n = f.info.number;
            LauncherPages.readTsk(new java.io.ByteArrayInputStream(f.data), f.info);
            f.info.number = n;
            if (f.name != null) f.replaces = new File(saveDir(), f.name).isFile();
        }
        mIncoming = found;
        if (single && found.get(0).name == null) mMenu.push(LauncherPages.importTarget(LauncherPages.slots(saveDir())));
        else mMenu.push(LauncherPages.importPage(found));
    }

    private String displayName(Uri uri) {
        try (Cursor c = getContentResolver().query(uri, new String[] { OpenableColumns.DISPLAY_NAME }, null, null, null)) {
            if (c != null && c.moveToFirst() && !c.isNull(0)) return c.getString(0);
        } catch (RuntimeException ignored) { }
        return null;
    }

    private void importSaves() {
        File dir = saveDir();
        dir.mkdirs();
        int n = 0;
        for (LauncherPages.Incoming f : mIncoming) {
            if (f.name == null) continue;
            File tmp = new File(dir, f.name + ".tmp"), out = new File(dir, f.name);
            try (OutputStream o = new FileOutputStream(tmp)) {
                o.write(f.data);
            } catch (IOException | SecurityException e) {
                tmp.delete();
                continue;
            }
            out.delete();
            if (tmp.renameTo(out)) n++;
            else tmp.delete();
        }
        // the game rewrites slots.txt at its next start; until then the launcher reads the .tsk headers
        new File(dir, "slots.txt").delete();
        mIncoming = new ArrayList<>();
        toast("Imported " + n + " save file" + (n == 1 ? "" : "s") + ".");
        loadPictures();                                         // Continue's picture may change
        mMenu.reset(mainPage());
    }

    /** The pad (d-pad / stick, A, B, Select / Y) and the keyboard drive the launcher. */
    @Override
    public boolean dispatchKeyEvent(android.view.KeyEvent e) {
        if (mMenu == null || mTop == null) return super.dispatchKeyEvent(e);
        int k = e.getKeyCode();
        int act;
        switch (k) {
            case android.view.KeyEvent.KEYCODE_DPAD_UP: act = 1; break;
            case android.view.KeyEvent.KEYCODE_DPAD_DOWN: act = 2; break;
            case android.view.KeyEvent.KEYCODE_DPAD_LEFT:
            case android.view.KeyEvent.KEYCODE_DPAD_RIGHT: act = 3; break;
            case android.view.KeyEvent.KEYCODE_DPAD_CENTER:
            case android.view.KeyEvent.KEYCODE_ENTER:
            case android.view.KeyEvent.KEYCODE_NUMPAD_ENTER:
            case android.view.KeyEvent.KEYCODE_BUTTON_A:
            case android.view.KeyEvent.KEYCODE_BUTTON_START: act = 4; break;
            case android.view.KeyEvent.KEYCODE_BUTTON_B:
            case android.view.KeyEvent.KEYCODE_ESCAPE: act = 5; break;
            case android.view.KeyEvent.KEYCODE_BACK: act = 6; break;
            case android.view.KeyEvent.KEYCODE_BUTTON_SELECT:
            case android.view.KeyEvent.KEYCODE_BUTTON_Y: act = 7; break;
            default: return super.dispatchKeyEvent(e);
        }
        if (e.getAction() != android.view.KeyEvent.ACTION_DOWN) return true;    // act on the press only
        boolean repeat = e.getRepeatCount() > 0;
        if (act == 1) mMenu.move(-1);
        else if (act == 2) mMenu.move(1);
        else if (repeat) return true;
        else if (act == 3) {                                    // left / right flip a setting
            LauncherModel.Item it = mMenu.selected();
            if (it != null && (it.id.equals("wide") || it.id.equals("ui") || it.id.equals("energy") || it.id.equals("holo")
                    || it.id.equals("panel"))) {
                mStepDir = k == android.view.KeyEvent.KEYCODE_DPAD_LEFT ? -1 : 1;
                choose();
                mStepDir = 1;
            }
        } else if (act == 4) choose();
        else if (act == 5) back();
        else if (act == 6) {
            if (mMenu.depth() > 1) back();
            else finish();
        } else if (!exportSelectedSlot()) revealTest();
        return true;
    }

    /** Touch: tap a button to choose it; a long press on the title shows the test item. */
    private boolean touch(android.view.MotionEvent e) {
        float x = e.getX(), y = e.getY();
        switch (e.getActionMasked()) {
            case android.view.MotionEvent.ACTION_DOWN:
                mTouchItem = mTop.itemAt(x, y);
                if (mTouchItem >= 0) mMenu.select(mTouchItem);
                if (mTop.onHeader(x, y)) ui.postDelayed(mRevealTest, 800);
                mTouchLong = false;
                if (mTouchItem >= 0) ui.postDelayed(mExportHold, 700);
                return true;
            case android.view.MotionEvent.ACTION_UP:
                ui.removeCallbacks(mRevealTest);
                ui.removeCallbacks(mExportHold);
                if (mTouchLong) {
                    mTouchItem = -1;
                    return true;
                }
                if (mTouchItem >= 0 && mTop.itemAt(x, y) == mTouchItem && mMenu.select(mTouchItem)) choose();
                mTouchItem = -1;
                return true;
            case android.view.MotionEvent.ACTION_CANCEL:
                ui.removeCallbacks(mRevealTest);
                ui.removeCallbacks(mExportHold);
                mTouchItem = -1;
                return true;
            default:
                return true;
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        mResumed = true;
        if (displays() != null) displays().registerDisplayListener(mDisplayWatch, ui);
        if (mTop != null) {
            showBottom("launcher");
            ui.removeCallbacks(mTick);
            ui.post(mTick);
        }
    }

    @Override
    protected void onPause() {
        mResumed = false;
        if (displays() != null) displays().unregisterDisplayListener(mDisplayWatch);
        ui.removeCallbacks(mTick);
        hideBottom();
        super.onPause();
    }

    /** On a dual-screen device (and the dual screen setting on) the bottom screen shows the page's
     *  options and the selected item's details / save-slot preview. */
    private void showBottom() {
        showBottom(null);
    }

    /** why: the display log line's reason (BottomScreen), null for none. */
    private void showBottom(String why) {
        android.view.Display d = BottomScreen.find(this, displays(), why);
        boolean want = mMenu != null && d != null && CompanionPanel.wanted(prefs(), true);
        if (!want) { hideBottom(); return; }
        if (mBottom != null && mBottom.getDisplay().getDisplayId() == d.getDisplayId()) return;
        hideBottom();
        final LauncherPresentation b = new LauncherPresentation(this, d, mMenu);
        // Presentation dismisses itself when its display goes away or changes size: forget it, look again
        b.setOnDismissListener(new android.content.DialogInterface.OnDismissListener() {
            @Override
            public void onDismiss(android.content.DialogInterface dialog) {
                if (mBottom != b) return;
                mBottom = null;
                ui.post(new Runnable() {
                    @Override public void run() { if (mResumed) showBottom("launcher panel closed"); }
                });
            }
        });
        try {
            b.show();
        } catch (android.view.WindowManager.InvalidDisplayException | IllegalStateException ex) {
            return;
        }
        mBottom = b;
    }

    private void hideBottom() {
        if (mBottom == null) return;
        LauncherPresentation b = mBottom;
        mBottom = null;                                 // before dismiss: the dismiss listener ignores it
        try {
            b.dismiss();
        } catch (RuntimeException ex) {
            // the display went away
        }
    }

    /** port: a TV / cast display coming or going never moves the launcher's bottom page (BottomScreen). */
    private boolean mResumed;
    private final android.hardware.display.DisplayManager.DisplayListener mDisplayWatch =
            new android.hardware.display.DisplayManager.DisplayListener() {
        @Override public void onDisplayAdded(int id) { if (mTop != null) showBottom("launcher, display " + id + " added"); }
        @Override public void onDisplayRemoved(int id) { if (mTop != null) showBottom("launcher, display " + id + " removed"); }
        @Override public void onDisplayChanged(int id) { }
    };

    /** The bottom screen's window: one LauncherView, never focused or touched (the pad stays here). */
    static final class LauncherPresentation extends android.app.Presentation {
        LauncherView view;
        private final LauncherModel mModel;

        LauncherPresentation(android.content.Context outer, android.view.Display display, LauncherModel model) {
            super(outer, display);
            mModel = model;
        }

        @Override
        protected void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);
            getWindow().addFlags(android.view.WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE
                    | android.view.WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE);
            view = new LauncherView(getContext(), mModel, true);
            setContentView(view);
        }
    }

    /** Test: the installed gam/*.gam files, sorted. */
    private List<String> levelFiles() {
        File data = findReadableData();
        File[] gams = data != null ? new File(data, "gam").listFiles() : null;
        List<String> names = new ArrayList<>();
        if (gams != null) {
            for (File f : gams) {
                String n = f.getName();
                if (f.isFile() && n.toLowerCase(Locale.ROOT).endsWith(".gam")) names.add(n);
            }
        }
        Collections.sort(names, String.CASE_INSENSITIVE_ORDER);
        if (names.isEmpty()) names.add("Level5.gam");
        return names;
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
        text += displayLog();
        Intent i = new Intent(Intent.ACTION_SEND);
        i.setType("text/plain");
        i.putExtra(Intent.EXTRA_SUBJECT, "Negatron log");
        i.putExtra(Intent.EXTRA_TEXT, text);
        startActivity(Intent.createChooser(i, "Share log"));
    }

    /** port: the end of display-log.txt (BottomScreen: the displays seen, the bottom screen chosen). */
    private String displayLog() {
        File f = new File(getExternalFilesDir(null), BottomScreen.LOG_FILE);
        if (!f.isFile()) return "";
        try (FileInputStream in = new FileInputStream(f)) {
            long len = f.length(), max = 8 * 1024;
            if (len > max) in.skip(len - max);
            byte[] b = new byte[(int) Math.min(len, max)];
            int n = 0, r;
            while (n < b.length && (r = in.read(b, n, b.length - n)) > 0) n += r;
            return "\n\n--- displays (" + BottomScreen.LOG_FILE + ") ---\n" + new String(b, 0, n, "UTF-8");
        } catch (IOException e) {
            return "";
        }
    }

    // ------------------------------------------------------------------ UI

    private void buildUi(File base) {
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setGravity(Gravity.CENTER);
        root.setBackgroundColor(Color.rgb(20, 24, 40));
        int pad = dp(32);
        root.setPadding(pad, pad, pad, pad);

        TextView title = new TextView(this);
        title.setText("Jimmy Neutron vs. Jimmy Negatron");
        title.setTextColor(Color.rgb(255, 210, 90));
        title.setTextSize(26);
        title.setTypeface(Typeface.DEFAULT_BOLD);
        title.setGravity(Gravity.CENTER);
        root.addView(title, wrap());

        message = new TextView(this);
        message.setTextColor(Color.rgb(220, 225, 235));
        message.setTextSize(17);
        message.setGravity(Gravity.CENTER);
        message.setPadding(0, dp(20), 0, dp(24));
        message.setText("Game files needed.\n\nChoose the game's CD image (Jimmy Neutron vs. Jimmy "
                + "Negatron.iso): the game is installed from it, about 270 MB. A zip of an installed "
                + "game's Program_Executable_Files folder works too.");
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
        choose.setOnClickListener(v -> pickZip());
        root.addView(choose, wrap());

        setContentView(root);
        choose.requestFocus();      // so the Thor's A button works without touching the screen
    }

    private int dp(int v) { return (int) (v * getResources().getDisplayMetrics().density + 0.5f); }

    private static LinearLayout.LayoutParams wrap() {
        return new LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
    }

    private void pickZip() {
        Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        i.addCategory(Intent.CATEGORY_OPENABLE);
        i.setType("*/*");
        i.putExtra(Intent.EXTRA_MIME_TYPES, new String[] {
                "application/x-iso9660-image", "application/x-cd-image", "application/zip",
                "application/x-zip-compressed", "application/octet-stream" });
        startActivityForResult(i, PICK_ZIP);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == PICK_BOXART && resultCode == RESULT_OK && data != null && data.getData() != null) {
            setPicture(data.getData(), BOX_ART);
            return;
        }
        if (resultCode == RESULT_OK && data != null && data.getData() != null && mMenu != null) {
            if (requestCode == EXPORT_ALL) { exportAll(data.getData()); return; }
            if (requestCode == EXPORT_SLOT && mExportSlot >= 0) { exportSlot(data.getData()); return; }
            if (requestCode == IMPORT_SAVES) { readImport(data.getData()); return; }
            if (requestCode == PICK_HD) { installHd(data.getData()); return; }
        }
        if (requestCode == PICK_BACKGROUND && resultCode == RESULT_OK && data != null && data.getData() != null) {
            setPicture(data.getData(), BACKGROUND);
            return;
        }
        if (requestCode == PICK_ZIP && resultCode == RESULT_OK && data != null && data.getData() != null) {
            importZip(data.getData());
        }
    }

    // ------------------------------------------------------------------ import

    private void importZip(final Uri uri) {
        if (isIso(uri)) { importIso(uri); return; }
        choose.setEnabled(false);
        progress.setVisibility(View.VISIBLE);
        progress.setProgress(0);
        message.setText("Unpacking game files…");
        final long total = querySize(uri);

        new Thread(() -> {
            String error = null;
            File base = getExternalFilesDir(null);
            File tmp = new File(base, "game.import");
            File dest = new File(base, "game");
            try {
                deleteTree(tmp);
                int files = unzip(uri, tmp, total);
                if (!canRead(new File(new File(tmp, "omt"), "level1.omt"))) {
                    error = files == 0 && looksLikeInstaller(uri)
                            ? "That looks like the game's installer. Pick Negatron-GameFiles.zip instead "
                              + "(the installer keeps the files inside data1.cab)."
                            : "That zip doesn't contain the game files (omt/level1.omt is missing).";
                } else {
                    deleteTree(dest);
                    if (!tmp.renameTo(dest)) throw new IOException("could not move files into place");
                }
            } catch (IOException | SecurityException e) {
                error = "Couldn't unpack the zip: " + e.getMessage();
            } finally {
                deleteTree(tmp);
            }
            final String err = error;
            ui.post(() -> {
                if (err == null) {
                    startGame();
                } else {
                    progress.setVisibility(View.GONE);
                    message.setText(err + "\n\nChoose another file to try again.");
                    choose.setEnabled(true);
                    choose.requestFocus();
                }
            });
        }).start();
    }

    // ------------------------------------------------------------------ import from the CD image

    private boolean isIso(Uri uri) {
        try (ParcelFileDescriptor pfd = getContentResolver().openFileDescriptor(uri, "r")) {
            return pfd != null && GameImporter.isIso(pfd.getFd());
        } catch (IOException | RuntimeException | UnsatisfiedLinkError e) {
            return false;
        }
    }

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
            File dest = new File(base, "game");
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
                    if (!canRead(new File(new File(tmp, "omt"), "level1.omt"))) {
                        error = "The CD image was unpacked but the game files are missing (omt/level1.omt).";
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
                    startGame();
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

    /** Unpacks the game folders from the zip into dir. Returns the number of game files written. */
    private int unzip(Uri uri, File dir, final long total) throws IOException {
        int written = 0;
        final long[] read = { 0 };
        final long[] lastUi = { 0 };
        InputStream raw = getContentResolver().openInputStream(uri);
        if (raw == null) throw new IOException("can't open the file");
        InputStream counting = new FilterInputStream(raw) {
            @Override public int read() throws IOException { int b = super.read(); if (b >= 0) tick(1); return b; }
            @Override public int read(byte[] b, int off, int len) throws IOException {
                int n = super.read(b, off, len); if (n > 0) tick(n); return n;
            }
            private void tick(long n) {
                read[0] += n;
                if (total > 0 && read[0] - lastUi[0] > 512 * 1024) {
                    lastUi[0] = read[0];
                    final int p = (int) Math.min(1000, read[0] * 1000 / total);
                    ui.post(() -> progress.setProgress(p));
                }
            }
        };
        byte[] buf = new byte[256 * 1024];
        try (ZipInputStream zip = new ZipInputStream(counting)) {
            ZipEntry e;
            while ((e = zip.getNextEntry()) != null) {
                String rel = gamePath(e.getName());
                if (rel == null || e.isDirectory()) continue;
                File out = new File(dir, rel);
                if (!out.getCanonicalPath().startsWith(dir.getCanonicalPath() + File.separator)) continue;  // zip-slip
                File parent = out.getParentFile();
                if (parent != null) parent.mkdirs();
                try (OutputStream os = new BufferedOutputStream(new FileOutputStream(out), 256 * 1024)) {
                    int n;
                    while ((n = zip.read(buf)) > 0) os.write(buf, 0, n);
                }
                written++;
            }
        }
        return written;
    }

    /** "Program_Executable_Files/omt/level1.omt" -> "omt/level1.omt"; null for anything else. */
    private static String gamePath(String name) {
        String n = name.replace('\\', '/');
        String low = n.toLowerCase(Locale.ROOT);
        // The game reads its built-in data (text, tables, defaults) from the player's own
        // Neutron2.exe; the source tree has none of it. Other programs and DLLs aren't needed.
        if (low.equals("neutron2.exe") || low.endsWith("/neutron2.exe")) return "Neutron2.exe";
        if (low.endsWith(".exe") || low.endsWith(".dll")) return null;
        String[] parts = n.split("/");
        for (int i = 0; i < parts.length - 1; i++) {
            for (String d : GAME_DIRS) {
                if (parts[i].equalsIgnoreCase(d)) {
                    StringBuilder sb = new StringBuilder(d);           // normalise folder case
                    for (int k = i + 1; k < parts.length; k++) sb.append('/').append(parts[k]);
                    return sb.toString();
                }
            }
        }
        return null;
    }

    private boolean looksLikeInstaller(Uri uri) {
        try (ZipInputStream zip = new ZipInputStream(getContentResolver().openInputStream(uri))) {
            ZipEntry e;
            int checked = 0;
            while ((e = zip.getNextEntry()) != null && checked++ < 200) {
                if (e.getName().toLowerCase(Locale.ROOT).endsWith("data1.cab")) return true;
            }
        } catch (IOException | RuntimeException ignored) { }
        return false;
    }

    /** port: the 16:9 widescreen setting (on by default; NegatronActivity passes it as NEG_WIDESCREEN). */
    static final String PREF_WIDESCREEN = "widescreen";
    /** port: the in-game UI, Bill's UI (default) or Vanilla (NegatronActivity passes it as NEG_UI). */
    static final String PREF_BILLS_UI = "bills_ui";
    /** port: Bill's UI's energy meter, "a" TUBE (default) / "b" ORBIT / "c" DIAL (NEG_UI_ENERGY). */
    static final String PREF_ENERGY = "energy_meter";
    /** port: Bill's UI's gadget hologram, "a" BAY (default) / "b" CONSOLE / "c" WHEEL / "off" (NEG_UI_HOLO). */
    static final String PREF_HOLO = "gadget_hologram";
    /** Left / right on a cycling setting: -1 / 1. */
    private int mStepDir = 1;

    private static void deleteTree(File f) {
        if (f == null || !f.exists()) return;
        File[] kids = f.listFiles();
        if (kids != null) for (File k : kids) deleteTree(k);
        //noinspection ResultOfMethodCallIgnored
        f.delete();
    }
}
