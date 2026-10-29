package com.sirbilliams.spongebob;

import android.content.Context;
import android.net.Uri;

import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.FilterInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.Locale;
import java.util.HashSet;
import java.util.Set;
import java.util.concurrent.locks.ReentrantLock;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

/** port: the HD background pack (docs/hd-pack.md). Installs a zip made by tools/hd/sb_hdtool (upscaled copies of the
 *  player's own scene backgrounds and skies) into the game data folder's hd/, which the game's loader
 *  (engine/sdl/SbHd.cpp) reads: sceneart/&lt;scene&gt;/&lt;canvas&gt;.png and manifest.tsv. The pack is made from the player's own
 *  game; nothing of it ships with the app. Staging in hd.import, the old pack kept as hd.previous until the swap
 *  succeeds, recovery of an interrupted swap, zip-slip / duplicate / size checks.
 *
 *  Launcher hook (LauncherView is not touched here): count(data) for the item's state, install(ctx, uri, data, total,
 *  progress) on a worker thread from the file picker's result, remove(data), where data is the game data folder (the
 *  one that holds sceneart/). */
final class HdPack {
    static final String DIR = "hd";
    private static final ReentrantLock operation = new ReentrantLock();

    interface Progress { void at(int permille); }

    static final class Result {
        int pictures, skipped;
        boolean manifest;
        String error;

        String message() {
            if (error != null) return error;
            return "HD background pack installed: " + pictures + " pictures"
                    + (skipped > 0 ? " (" + skipped + " other files left out)" : "") + ".";
        }
    }

    /** The number of pictures installed (0 without a pack). */
    static int count(File data) {
        if (data == null) return 0;
        // A recreated Activity must not block its UI thread behind an ongoing ZIP import.
        if (!operation.tryLock()) return countPng(new File(data, DIR), 0);
        try {
            try { recover(data); } catch (IOException ex) { /* Keep the backup for a later retry. */ }
            return countPng(new File(data, DIR), 0);
        } finally { operation.unlock(); }
    }

    private static int countPng(File dir, int depth) {
        File[] l = dir.listFiles();
        if (l == null) return 0;
        int n = 0;
        for (File f : l) {
            if (f.isDirectory()) { if (depth < 3) n += countPng(f, depth + 1); }
            else if (f.getName().toLowerCase(Locale.ROOT).endsWith(".png")) n++;
        }
        return n;
    }

    static boolean remove(File data) {
        if (data == null || !operation.tryLock()) return false;
        try {
            // Remove the backup first so a later recovery cannot resurrect a removed pack.
            return deleteTree(new File(data, DIR + ".previous")) && deleteTree(new File(data, DIR));
        } finally { operation.unlock(); }
    }

    /** A zip entry's place in the pack ("sceneart/c1l0s1/background.png", "manifest.tsv"), or null for anything else
     *  (README, other pictures). Folders before sceneart/ (or before manifest.tsv) are dropped; names are lower-cased
     *  as the loader matches them without regard to case. */
    static String packPath(String name) {
        if (name == null || name.isEmpty()) return null;
        String normalized = name.replace('\\', '/');
        if (normalized.startsWith("/") || normalized.endsWith("/")) return null;
        String[] parts = normalized.split("/", -1);
        for (String part : parts)
            if (part.isEmpty() || part.equals(".") || part.equals("..") || part.indexOf(':') >= 0)
                return null;
        int n = parts.length;
        String file = parts[n - 1], low = file.toLowerCase(Locale.ROOT);
        if (low.equals("manifest.tsv")) return "manifest.tsv";
        if (!low.endsWith(".png") || low.length() <= 4) return null;
        String stem = low.substring(0, low.length() - 4);
        if (n >= 3 && parts[n - 3].equalsIgnoreCase("sceneart") && !parts[n - 2].isEmpty())
            return "sceneart/" + parts[n - 2].toLowerCase(Locale.ROOT) + "/" + stem + ".png";
        return null;
    }

    /** Unpacks the zip into data/hd (replacing a pack already there). Runs on a worker thread. */
    static Result install(Context ctx, Uri uri, File data, final long total, final Progress progress) {
        try {
            InputStream raw = ctx.getContentResolver().openInputStream(uri);
            if (raw == null) throw new IOException("can't open the file");
            return install(raw, data, total, progress);
        } catch (IOException | SecurityException ex) {
            Result r = new Result();
            r.error = "Couldn't install the pack: " + ex.getMessage();
            return r;
        }
    }

    // port: stream entry point also permits regression tests without an Android device.
    static Result install(InputStream raw, File data, final long total, final Progress progress) {
        Result r = new Result();
        File tmp = new File(data, DIR + ".import"), dest = new File(data, DIR);
        final long[] read = { 0 }, lastUi = { 0 };
        operation.lock();
        try (InputStream source = raw) {
            recover(data);
            if (!deleteTree(tmp)) throw new IOException("could not clear the temporary pack");
            InputStream counting = new FilterInputStream(source) {
                @Override public int read() throws IOException { int b = super.read(); if (b >= 0) tick(1); return b; }
                @Override public int read(byte[] b, int off, int len) throws IOException {
                    int k = super.read(b, off, len); if (k > 0) tick(k); return k;
                }
                private void tick(long k) {
                    read[0] += k;
                    if (total > 0 && read[0] - lastUi[0] > 1024 * 1024) {
                        lastUi[0] = read[0];
                        progress.at((int) Math.min(999, read[0] * 1000 / total));
                    }
                }
            };
            Set<String> paths = new HashSet<>();
            byte[] buf = new byte[256 * 1024];
            String canon = tmp.getCanonicalPath() + File.separator;
            try (ZipInputStream zip = new ZipInputStream(counting)) {
                ZipEntry e;
                while ((e = zip.getNextEntry()) != null) {
                    if (e.isDirectory()) continue;
                    String rel = packPath(e.getName());
                    if (rel == null) { r.skipped++; continue; }
                    // The loader matches case-insensitively; rel is already lower case: reject duplicates.
                    if (!paths.add(rel))
                        throw new IOException("duplicate picture: " + rel);
                    File out = new File(tmp, rel);
                    if (!out.getCanonicalPath().startsWith(canon)) { r.skipped++; continue; }    // zip-slip
                    File parent = out.getParentFile();
                    if (parent != null && !parent.isDirectory() && !parent.mkdirs())
                        throw new IOException("could not create the pack folder");
                    try (OutputStream os = new BufferedOutputStream(new FileOutputStream(out), 256 * 1024)) {
                        int k;
                        long written = 0;
                        while ((k = zip.read(buf)) != -1) {
                            written += k;
                            // Bound individual compressed pictures and check actual expanded bytes,
                            // not the compressed ZIP size (which can be unknown to a document provider).
                            if (written > (256L << 20)) throw new IOException("picture exceeds 256 MB: " + rel);
                            if (data.getUsableSpace() < k + (64L << 20))
                                throw new IOException("not enough free space for the expanded pack");
                            os.write(buf, 0, k);
                        }
                    }
                    if (rel.equals("manifest.tsv")) r.manifest = true;
                    else if (!isPng(out)) { out.delete(); paths.remove(rel); r.skipped++; }    // not a PNG: left out (the game would skip it anyway)
                    else r.pictures++;
                }
            }
            if (r.pictures == 0) {
                r.error = "That zip isn't an HD background pack: no sceneart/<scene>/<canvas>.png pictures in it.";
            } else {
                publish(tmp, dest, new File(data, DIR + ".previous"));
            }
        } catch (IOException | SecurityException ex) {
            r.error = "Couldn't install the pack: " + ex.getMessage();
        } finally {
            try { deleteTree(tmp); } finally { operation.unlock(); }
        }
        progress.at(1000);
        return r;
    }

    private static boolean isPng(File f) {
        byte[] sig = { (byte) 0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a }, got = new byte[8];
        try (InputStream in = new java.io.FileInputStream(f)) {
            int n = 0, k;
            while (n < 8 && (k = in.read(got, n, 8 - n)) > 0) n += k;
            return n == 8 && java.util.Arrays.equals(sig, got);
        } catch (IOException ex) { return false; }
    }

    /** Recover an interrupted swap before the launcher counts pictures or installs another pack. */
    private static void recover(File data) throws IOException {
        File dest = new File(data, DIR), previous = new File(data, DIR + ".previous");
        if (previous.exists() && !dest.exists() && !previous.renameTo(dest))
            throw new IOException("could not restore the previous pack");
    }

    static void publish(File tmp, File dest, File previous) throws IOException {
        if (!deleteTree(previous)) throw new IOException("could not clear the previous pack backup");
        boolean hadPack = dest.exists();
        if (hadPack && !dest.renameTo(previous)) throw new IOException("could not back up the installed pack");
        if (!tmp.renameTo(dest)) {
            if (hadPack && !previous.renameTo(dest))
                throw new IOException("could not restore the old pack; it is kept in hd.previous");
            throw new IOException("could not move the pictures into place; old pack kept");
        }
        deleteTree(previous); // If cleanup fails, keep it until the next successful operation.
    }

    static boolean deleteTree(File f) {
        if (!f.exists()) return true;
        File[] l = f.listFiles();
        if (l != null) for (File c : l) if (!deleteTree(c)) return false;
        return f.delete();
    }
}
