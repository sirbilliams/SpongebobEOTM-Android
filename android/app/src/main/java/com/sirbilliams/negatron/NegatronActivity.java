package com.sirbilliams.negatron;

import android.app.AlertDialog;
import android.content.Intent;
import android.content.DialogInterface;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.system.ErrnoException;
import android.system.Os;
import android.view.KeyEvent;
import android.view.WindowManager;

import org.libsdl.app.SDLActivity;

/**
 * Hosts the OMT engine. SDLActivity loads libSDL2.so and libmain.so (viewer) or libnegatron.so
 * (the reconstructed game), then calls SDL_main
 * (engine/sdl/OMediaSDLMain.cpp). Game files are read from Android/data/<package>/files/.
 */
public class NegatronActivity extends SDLActivity {

    /** Intent extra: which native app to run, "main" (the model viewer) or "negatron" (the game). */
    static final String EXTRA_LIB = "lib";
    /** Intent extra: a level to play straight away without its opening (the NEG_TEST_PLAY test hook). */
    static final String EXTRA_TEST_PLAY = "test_play";
    /** Intent extra: where the game starts (NEG_START: menu, new, continue, load:<slot>; the launcher). */
    static final String EXTRA_START = "start";
    /** Intent extra: the Asset Viewer explores this level (NEG_ASSET_VIEW, game/src/port/AssetView.h). */
    static final String EXTRA_ASSET_VIEW = "asset_view";

    @Override
    protected String[] getLibraries() {
        String lib = getIntent() != null ? getIntent().getStringExtra(EXTRA_LIB) : null;
        return new String[] { "SDL2", "negatron".equals(lib) ? "negatron" : "main" };
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        // Set (or clear: the process can outlive a run) the test hook before the engine starts.
        String test = getIntent() != null ? getIntent().getStringExtra(EXTRA_TEST_PLAY) : null;
        try {
            if (test != null) Os.setenv("NEG_TEST_PLAY", test, true);
            else Os.unsetenv("NEG_TEST_PLAY");
            String start = getIntent() != null ? getIntent().getStringExtra(EXTRA_START) : null;
            if (start != null) Os.setenv("NEG_START", start, true);
            else Os.unsetenv("NEG_START");
            String view = getIntent() != null ? getIntent().getStringExtra(EXTRA_ASSET_VIEW) : null;
            if (view != null) Os.setenv("NEG_ASSET_VIEW", view, true);     // port: the Asset Viewer
            else Os.unsetenv("NEG_ASSET_VIEW");
            // port: the launcher's pictures, saved once by the game (game/src/port/LauncherArt.h)
            if (getExternalFilesDir(null) != null)
                Os.setenv("NEG_LAUNCHER_ART", new java.io.File(getExternalFilesDir(null), SetupActivity.ART_DIR).getPath(), true);
            // port: 16:9 widescreen (game/src/port/Widescreen.h), the setup screen's option
            boolean wide = getSharedPreferences(CompanionPanel.PREFS, MODE_PRIVATE)
                    .getBoolean(SetupActivity.PREF_WIDESCREEN, true);
            Os.setenv("NEG_WIDESCREEN", wide ? "1" : "0", true);
            // port: the in-game UI (game/src/port/BillsUi.h), the launcher's Settings / the port menu
            boolean bills = getSharedPreferences(CompanionPanel.PREFS, MODE_PRIVATE)
                    .getBoolean(SetupActivity.PREF_BILLS_UI, true);
            Os.setenv("NEG_UI", bills ? "bills" : "vanilla", true);
            Os.setenv("NEG_UI_ENERGY", getSharedPreferences(CompanionPanel.PREFS, MODE_PRIVATE)
                    .getString(SetupActivity.PREF_ENERGY, "a"), true);
            Os.setenv("NEG_UI_HOLO", getSharedPreferences(CompanionPanel.PREFS, MODE_PRIVATE)
                    .getString(SetupActivity.PREF_HOLO, "a"), true);
            // port: the app's version for the pause screen's credit line
            try {
                android.content.pm.PackageInfo pi = getPackageManager().getPackageInfo(getPackageName(), 0);
                Os.setenv("NEG_APP_VERSION", pi.versionName, true);
            } catch (android.content.pm.PackageManager.NameNotFoundException e) {
                Os.unsetenv("NEG_APP_VERSION");
            }
        } catch (ErrnoException e) {
            // no test hook then
        }
        super.onCreate(savedInstanceState);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        // Make sure the files folder exists so it shows up over USB for copying the game data.
        if (getExternalFilesDir(null) != null) {
            getExternalFilesDir(null).mkdirs();
        }
        // port: the bottom-screen companion panel, for the game only (docs/controls.md)
        String lib = getIntent() != null ? getIntent().getStringExtra(EXTRA_LIB) : null;
        if ("negatron".equals(lib)) mPanel = new CompanionPanel(this);
        // port: the game runs on the default (top) display, so casting mirrors it (docs/controls.md
        // "Casting / TV"; the launcher opens it there). The log says where it really is.
        android.view.Display on = getWindowManager().getDefaultDisplay();
        int id = on != null ? on.getDisplayId() : -1;
        BottomScreen.log(this, "game on display " + id
                + (id == android.view.Display.DEFAULT_DISPLAY ? " (the top screen)" : " - NOT the top screen"));
    }

    // ------------------------------------------------------------------ port: companion panel

    private CompanionPanel mPanel;
    private final Handler mUi = new Handler(Looper.getMainLooper());
    private KeyEvent mBackDown, mBackUp;
    private boolean mBackLong, mBackPending, mSwallowBackUp;

    // ------------------------------------------------------------------ port: suspend

    /* SDL pauses the game thread (blocks its event loop, pauses the audio, keeps the GL context;
     * engine/sdl/OMediaSDLInput.cpp then stops the clock and the game pauses itself) only in
     * onStop on Android 7+. The Thor's lid / screen can go off without that, so the game is also
     * suspended in onPause and when the main display turns off while the activity stays resumed. */
    private boolean mResumed, mScreenOff;
    private android.hardware.display.DisplayManager mDisplays;

    private final android.hardware.display.DisplayManager.DisplayListener mScreenWatch =
            new android.hardware.display.DisplayManager.DisplayListener() {
        @Override public void onDisplayAdded(int id) { }
        @Override public void onDisplayRemoved(int id) { }
        @Override public void onDisplayChanged(int id) {
            if (id == android.view.Display.DEFAULT_DISPLAY) screenChanged();
        }
    };

    private void screenChanged() {
        android.view.Display d = mDisplays != null ? mDisplays.getDisplay(android.view.Display.DEFAULT_DISPLAY) : null;
        boolean off = d != null && d.getState() != android.view.Display.STATE_ON
                && d.getState() != android.view.Display.STATE_UNKNOWN;
        if (off == mScreenOff) return;
        mScreenOff = off;
        if (!mResumed) return;                          // onPause / onResume take care of it
        if (off) suspendGame();
        else resumeGame();
    }

    private void suspendGame() {
        if (mPanel != null) mPanel.pause();             // the bottom screen stops polling
        pauseNativeThread();
    }

    private void resumeGame() {
        resumeNativeThread();
        if (mPanel != null) mPanel.resume();
    }

    @Override
    protected void onResume() {
        super.onResume();
        mResumed = true;
        if (mDisplays == null) {
            mDisplays = (android.hardware.display.DisplayManager) getSystemService(DISPLAY_SERVICE);
            if (mDisplays != null) mDisplays.registerDisplayListener(mScreenWatch, mUi);
        }
        screenChanged();
        if (!mScreenOff) resumeGame();
    }

    @Override
    protected void onPause() {
        mResumed = false;
        suspendGame();
        super.onPause();
    }

    /* port: back to the launcher when the game ends. When the game quits (the PDA menu's Exit, or
     * any normal end of SDL_main), SDLActivity finishes this activity; SDL and the game keep
     * static state that a second start in the same process would inherit, so the game runs in its
     * own process (AndroidManifest: android:process=":game"), which ends here once SDL has shut
     * down (the game saved what it saves, audio and GL closed). The launcher, in the app's main
     * process, then reads the save slots afresh. */
    @Override
    protected void onDestroy() {
        if (mDisplays != null) mDisplays.unregisterDisplayListener(mScreenWatch);
        mDisplays = null;
        boolean ending = isFinishing() && !isChangingConfigurations();
        super.onDestroy();                              // SDL: quits and joins the game thread
        if (ending) {
            Intent i = new Intent(this, SetupActivity.class);
            i.putExtra(SetupActivity.EXTRA_FROM_GAME, true);
            if (getIntent() != null && getIntent().getStringExtra(SetupActivity.EXTRA_RETURN) != null)
                i.putExtra(SetupActivity.EXTRA_RETURN, getIntent().getStringExtra(SetupActivity.EXTRA_RETURN));
            startActivity(i, SetupActivity.onTopScreen());
            android.os.Process.killProcess(android.os.Process.myPid());
        }
    }

    /** Back held for 0.6 s, or pressed twice within 0.35 s, opens the port menu. A single back
     *  still reaches the game (as Esc), 0.35 s late. */
    private final Runnable mBackLongPress = new Runnable() {
        @Override
        public void run() {
            mBackLong = true;
            showPortMenu();
        }
    };

    private final Runnable mDeliverBack = new Runnable() {
        @Override
        public void run() {
            mBackPending = false;
            if (mBackDown != null) superDispatchKeyEvent(mBackDown);
            if (mBackUp != null) superDispatchKeyEvent(mBackUp);
            mBackDown = mBackUp = null;
        }
    };

    private boolean superDispatchKeyEvent(KeyEvent e) {
        return super.dispatchKeyEvent(e);
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent e) {
        if (mPanel == null || e.getKeyCode() != KeyEvent.KEYCODE_BACK) return super.dispatchKeyEvent(e);
        if (e.getAction() == KeyEvent.ACTION_DOWN) {
            if (e.getRepeatCount() > 0) return true;
            if (mBackPending) {                         // second back: the menu
                mUi.removeCallbacks(mDeliverBack);
                mBackPending = false;
                mBackDown = mBackUp = null;
                mSwallowBackUp = true;
                showPortMenu();
                return true;
            }
            mBackDown = e;
            mBackLong = false;
            mSwallowBackUp = false;                     // (an up that went to the menu dialog)
            mUi.postDelayed(mBackLongPress, 600);
            return true;
        }
        if (e.getAction() == KeyEvent.ACTION_UP) {
            mUi.removeCallbacks(mBackLongPress);
            if (mSwallowBackUp) {
                mSwallowBackUp = false;
                return true;
            }
            if (mBackLong) {                            // the long press opened the menu
                mBackLong = false;
                mBackDown = null;
                return true;
            }
            mBackUp = e;
            mBackPending = true;
            mUi.postDelayed(mDeliverBack, 350);
            return true;
        }
        return true;
    }

    private void showPortMenu() {
        if (mPanel == null || isFinishing()) return;
        final boolean on = mPanel.isEnabled();
        final boolean far = mPanel.isZoomFar();
        final android.content.SharedPreferences prefs = getSharedPreferences(CompanionPanel.PREFS, MODE_PRIVATE);
        String panel = "Bottom screen panel: " + (on ? "ON" : "OFF")
                + (mPanel.hasSecondDisplay() ? "" : " (no second screen found)");
        String zoom = "Panel map zoom: " + (far ? "FAR" : "NEAR");
        final boolean bills = prefs.getBoolean(SetupActivity.PREF_BILLS_UI, true);
        String ui = "In-game UI: " + (bills ? "BILL'S UI" : "VANILLA");
        final String energy = prefs.getString(SetupActivity.PREF_ENERGY, "a");
        final String holo = prefs.getString(SetupActivity.PREF_HOLO, "a");
        final String[] items = bills
                ? new String[] { panel, zoom, ui, "Energy meter: " + LauncherPages.energyName(energy),
                        "Gadget hologram: " + LauncherPages.holoName(holo), "Close" }
                : new String[] { panel, zoom, ui, "Close" };
        new AlertDialog.Builder(this)
                .setTitle("Negatron")
                .setItems(items, new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface d, int which) {
                        if (which == 0) mPanel.setEnabled(!on);
                        else if (which == 1) mPanel.setZoomFar(!far);
                        else if (which == 2) {          // port: Bill's UI / Vanilla, applied at once
                            prefs.edit().putBoolean(SetupActivity.PREF_BILLS_UI, !bills).apply();
                            Companion.setBillsUi(!bills);
                        } else if (which == 3 && bills) {   // port: the next energy meter, at once
                            String next = LauncherPages.energyStep(energy, 1);
                            prefs.edit().putString(SetupActivity.PREF_ENERGY, next).apply();
                            Companion.setEnergyMeter(next.charAt(0) - 'a' + 1);
                        } else if (which == 4 && bills) {   // port: the next gadget hologram, at once
                            String next = LauncherPages.holoStep(holo, 1);
                            prefs.edit().putString(SetupActivity.PREF_HOLO, next).apply();
                            Companion.setGadgetHologram(LauncherPages.holoNumber(next));
                        }
                    }
                })
                .show();
    }
}
