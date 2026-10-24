package com.sirbilliams.spongebob;

/**
 * Installs the game files from the game's CD image (.iso), as the CD's setup program does,
 * plus the movies and help.omt the game reads from the CD:
 * finds the InstallShield cabinet on the disc and unpacks the game folder (libimporter.so,
 * engine/importer/SbImport.c).
 */
final class GameImporter {
    static { System.loadLibrary("importer"); }

    interface Progress {
        /** stage 0: copying the installer off the disc (bytes), 1: unpacking (files). Return true to cancel. */
        boolean onProgress(int stage, long done, long total);
    }

    /** True if the file open on fd is an ISO 9660 CD image. */
    static native boolean isIso(int fd);

    /** Installs into dest (tmp holds the copied installer meanwhile). Returns null, or the error for the player. */
    static native String importIso(int fd, String dest, String tmp, Progress progress);

    private GameImporter() { }
}
