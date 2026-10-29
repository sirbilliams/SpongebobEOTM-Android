package com.sirbilliams.spongebob;

import android.os.Bundle;
import android.system.ErrnoException;
import android.system.Os;
import android.view.MotionEvent;
import android.view.WindowManager;

import org.libsdl.app.SDLActivity;

import java.io.File;

/**
 * Hosts the engine. SDLActivity loads libSDL2.so and libspongebob.so (the reconstructed game), then
 * calls SDL_main (engine/sdl/OMediaSDLMain.cpp). The game runs in the installed folder
 * (Android/data/<package>/files/game); the engine writes omt-log.txt next to it, which the launcher's
 * Test page shares.
 */
public class SpongeActivity extends SDLActivity {

    /** Intent extra: run the crash-backtrace test (OMT_CRASHTEST: the game raises SIGSEGV at startup). */
    static final String EXTRA_CRASH_TEST = "crash_test";

    CompanionPanel mPanel;                                  // lane c: package-private, PortPrefs applies the port menu's bottom-screen choice to it

    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL2", "spongebob" };
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        try {
            String[] modes = { "fill", "panels", "stretch", "off" };
            int mode = getSharedPreferences("settings", MODE_PRIVATE).getInt("widescreen", 0);
            Os.setenv("SB_WIDESCREEN", modes[Math.floorMod(mode, 4)], true);
            // port: reset the environment on every launch, including Classic.
            String[] cursors = {"spatula", "net", "bubble", "classic"};
            Os.setenv("SB_CURSOR", cursors[Math.floorMod(getSharedPreferences("settings", MODE_PRIVATE).getInt("cursor", 0), 4)], true);
            File base = getExternalFilesDir(null);
            Os.setenv("SB_PAD_CURSOR", getSharedPreferences("settings", MODE_PRIVATE).getBoolean("pad_cursor", true) ? "1" : "0", true);
            if (base != null) Os.setenv("OMT_DATA", new File(base, "game").getPath(), true);
            if (getIntent() != null && getIntent().getBooleanExtra(EXTRA_CRASH_TEST, false))
                Os.setenv("OMT_CRASHTEST", "1", true);
            else
                Os.unsetenv("OMT_CRASHTEST");              // (the process can outlive a run)
        } catch (ErrnoException e) {
            // the engine then looks in files/ itself
        }
        super.onCreate(savedInstanceState);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
    }

    // ------------------------------------------------------------------ port: companion panel (docs/companion.md)

    @Override
    protected void onResume() {
        super.onResume();
        if (mPanel == null) mPanel = new CompanionPanel(this);
        mPanel.resume();                                    // the bottom-screen panel, when a second screen is there
    }

    /** port: bottom-screen touches that reach the game window are forwarded to the panel (docs/bottom-touch.md); the rest is SDL's. */
    @Override
    public boolean dispatchTouchEvent(MotionEvent e) {
        if (mPanel != null && mPanel.routeTouch(e)) return true;
        return super.dispatchTouchEvent(e);
    }

    @Override
    protected void onPause() {
        if (mPanel != null) mPanel.pause();
        super.onPause();
    }
}
