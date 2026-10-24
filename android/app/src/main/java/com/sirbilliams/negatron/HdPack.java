package com.sirbilliams.negatron;

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
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

/** port: the HD texture pack (docs/hd-pack.md). Installs a zip of upscaled copies of the game's own
 *  pictures into the game data's hd/ folder, which the game's loader (engine/sdl/OMediaHDPack.cpp)
 *  reads: omt/&lt;database&gt;/&lt;id&gt;.png, png/&lt;name&gt;.png, grn/&lt;model&gt;_&lt;n&gt;.png. The pack is the
 *  player's own (made from their game files); nothing of it ships with the app. */
final class HdPack {
    static final String DIR = "hd";

    interface Progress { void at(int permille); }

    static final class Result {
        int omt, png, grn, skipped;
        String error;

        String message() {
            if (error != null) return error;
            return "HD texture pack installed: " + omt + " omt, " + png + " png and " + grn + " model pictures"
                    + (skipped > 0 ? " (" + skipped + " other files left out)" : "") + ".";
        }
    }

    /** The number of pictures installed (0 without a pack). */
    static int count(File data) {
        return data == null ? 0 : countPng(new File(data, DIR), 0);
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

    static void remove(File data) {
        if (data != null) deleteTree(new File(data, DIR));
    }

    /** A zip entry's place in the pack ("omt/level1/1234.png", "png/sub.png", "grn/jimmybase_0.png"), or
     *  null for anything else (README, manifest). Leading folders before omt/png/grn are dropped. */
    static String packPath(String name) {
        String[] parts = name.replace('\\', '/').split("/");
        int n = parts.length;
        String file = parts[n - 1], low = file.toLowerCase(Locale.ROOT);
        if (!low.endsWith(".png") || low.length() <= 4) return null;
        String stem = file.substring(0, file.length() - 4);
        if (n >= 3 && parts[n - 3].equalsIgnoreCase("omt") && stem.matches("\\d+") && !parts[n - 2].isEmpty())
            return "omt/" + parts[n - 2].toLowerCase(Locale.ROOT) + "/" + stem + ".png";
        if (n >= 2 && parts[n - 2].equalsIgnoreCase("png")) return "png/" + file;
        if (n >= 2 && parts[n - 2].equalsIgnoreCase("grn") && stem.matches(".+_\\d+")) return "grn/" + file;
        return null;
    }

    /** Unpacks the zip into data/hd (replacing a pack already there). Runs on a worker thread. */
    static Result install(Context ctx, Uri uri, File data, final long total, final Progress progress) {
        Result r = new Result();
        File tmp = new File(data, DIR + ".import"), dest = new File(data, DIR);
        if (total > 0 && data.getUsableSpace() < total + total / 20 + (64L << 20)) {
            r.error = "Not enough free space for the pack (it needs about " + (total >> 20) + " MB).";
            return r;
        }
        final long[] read = { 0 }, lastUi = { 0 };
        try {
            deleteTree(tmp);
            InputStream raw = ctx.getContentResolver().openInputStream(uri);
            if (raw == null) throw new IOException("can't open the file");
            InputStream counting = new FilterInputStream(raw) {
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
            byte[] buf = new byte[256 * 1024];
            String canon = tmp.getCanonicalPath() + File.separator;
            try (ZipInputStream zip = new ZipInputStream(counting)) {
                ZipEntry e;
                while ((e = zip.getNextEntry()) != null) {
                    if (e.isDirectory()) continue;
                    String rel = packPath(e.getName());
                    if (rel == null) { r.skipped++; continue; }
                    File out = new File(tmp, rel);
                    if (!out.getCanonicalPath().startsWith(canon)) { r.skipped++; continue; }    // zip-slip
                    File parent = out.getParentFile();
                    if (parent != null) parent.mkdirs();
                    try (OutputStream os = new BufferedOutputStream(new FileOutputStream(out), 256 * 1024)) {
                        int k;
                        while ((k = zip.read(buf)) > 0) os.write(buf, 0, k);
                    }
                    if (rel.startsWith("omt/")) r.omt++;
                    else if (rel.startsWith("png/")) r.png++;
                    else r.grn++;
                }
            }
            if (r.omt + r.png + r.grn == 0) {
                r.error = "That zip isn't an HD texture pack: no omt/<database>/<id>.png, png/<name>.png "
                        + "or grn/<model>_<n>.png pictures in it.";
            } else {
                deleteTree(dest);
                if (!tmp.renameTo(dest)) throw new IOException("could not move the pictures into place");
            }
        } catch (IOException | SecurityException ex) {
            r.error = "Couldn't install the pack: " + ex.getMessage();
        } finally {
            deleteTree(tmp);
        }
        progress.at(1000);
        return r;
    }

    static void deleteTree(File f) {
        File[] l = f.listFiles();
        if (l != null) for (File c : l) deleteTree(c);
        f.delete();
    }
}
