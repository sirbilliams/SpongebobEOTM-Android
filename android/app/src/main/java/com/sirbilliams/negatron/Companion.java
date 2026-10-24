package com.sirbilliams.negatron;

/**
 * port: JNI bridge to the dual-screen companion snapshot (game/src/port/Companion.cpp).
 * The natives live in libnegatron.so, which SDLActivity loads only for the game (not for the
 * model viewer); every call is guarded so a missing library just means "no data".
 */
final class Companion {
    private static boolean sBroken;

    private Companion() {}

    private static native String nativeSnapshot();
    private static native void nativeSetEnabled(boolean on);
    private static native int[] nativeMap();
    private static native int[] nativeIcon(int id);
    private static native void nativeSetBillsUi(boolean on);
    private static native void nativeSetEnergyMeter(int design);
    private static native void nativeSetGadgetHologram(int design);

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

    /** The level map: [gen, w, h, ARGB pixels...], or null. */
    static int[] map() {
        if (sBroken) return null;
        try {
            return nativeMap();
        } catch (UnsatisfiedLinkError e) {
            sBroken = true;
            return null;
        }
    }

    /** An icon the snapshot names: [w, h, ARGB pixels...], or null. */
    static int[] icon(int id) {
        if (sBroken) return null;
        try {
            return nativeIcon(id);
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

    /** port: switches the in-game UI (Bill's UI / Vanilla) while the game runs (game/src/port/BillsUi.h). */
    static void setBillsUi(boolean on) {
        try {
            nativeSetBillsUi(on);
        } catch (UnsatisfiedLinkError e) {
            // not the game
        }
    }

    /** port: Bill's UI's energy meter, 1 TUBE / 2 ORBIT / 3 DIAL, switched while the game runs. */
    static void setEnergyMeter(int design) {
        try {
            nativeSetEnergyMeter(design);
        } catch (UnsatisfiedLinkError e) {
            // not the game
        }
    }

    /** port: Bill's UI's gadget hologram, 0 OFF / 1 BAY / 2 CONSOLE / 3 WHEEL, switched while the game runs. */
    static void setGadgetHologram(int design) {
        try {
            nativeSetGadgetHologram(design);
        } catch (UnsatisfiedLinkError e) {
            // not the game
        }
    }
}
