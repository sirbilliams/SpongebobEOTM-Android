package com.sirbilliams.spongebob;

import android.os.Bundle;
import android.system.ErrnoException;
import android.system.Os;
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

    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL2", "spongebob" };
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        try {
            File base = getExternalFilesDir(null);
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
}
