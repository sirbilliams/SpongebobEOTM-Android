package com.sirbilliams.spongebob;

/**
 * port: JNI bridge to the dual-screen companion (game/src/port/Companion.cpp, in libspongebob.so).
 * SDLActivity loads that library for the game only, so every call is guarded: a missing library just
 * means "no data" (the panel then shows its "waiting for the game" page).
 */
final class Companion {
    private static boolean sBroken;

    private Companion() {}

    private static native String nativeSnapshot();
    private static native void nativeSetEnabled(boolean on);
    private static native void nativeCommand(String cmd);
    private static native void nativeTouchpad(float dx, float dy, int button);

    /** The latest snapshot as JSON, or null when the game library is not there. */
    static String snapshot() {
        if (sBroken) return null;
        try {
            return nativeSnapshot();
        } catch (UnsatisfiedLinkError e) {
            sBroken = true;
            return null;
        }
    }

    /** Starts / stops building snapshots on the game thread (off by default: no cost). */
    static void setEnabled(boolean on) {
        if (sBroken) return;
        try {
            nativeSetEnabled(on);
        } catch (UnsatisfiedLinkError e) {
            sBroken = true;
        }
    }

    /**
     * A touch on the panel, queued for the game thread (CompanionFeed.h): "item:<id>", "noitem",
     * "choice:<n>", "advance", "menu", "skip".
     */
    static void command(String cmd) {
        if (sBroken) return;
        try {
            nativeCommand(cmd);
        } catch (UnsatisfiedLinkError e) {
            sBroken = true;
        }
    }

    /**
     * The bottom-screen touchpad: a relative move of the game's cursor (dx, dy in the game's 640x480 logical pixels) and the left
     * button (1 down, 0 up, -1 unchanged). Applied by the game thread with the right stick's pointer (OMediaEventManager).
     */
    static void touchpad(float dx, float dy, int button) {
        if (sBroken) return;
        try {
            nativeTouchpad(dx, dy, button);
        } catch (UnsatisfiedLinkError e) {
            sBroken = true;
        }
    }
}
