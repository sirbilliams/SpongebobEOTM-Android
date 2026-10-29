package com.sirbilliams.spongebob;

import android.graphics.Bitmap;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;
import java.util.Locale;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;
import java.util.zip.ZipOutputStream;

/**
 * port: the launcher's view of the game's saves (docs/saves.md): saved/slot0..98.sb2 and saved/quick.sb2 in the game folder.
 * Reads the header (name, 'SHOT' thumbnail 320x240 RGB565, scene id), exports saves to one .zip and imports a .zip back.
 * Only those save files are ever written (via .tmp + rename, like the game); nothing else in the game folder is touched.
 */
final class SaveFiles {
    static final int SHOT = 0x53484f54, THUMB_W = 320, THUMB_H = 240;
    static final Pattern NAME = Pattern.compile("(?:slot([0-9]|[1-8][0-9]|9[0-8])|quick)\\.sb2");
    static final int MAX_SAVE = 4 << 20, MAX_ENTRIES = 128;

    static final class Info {
        String file;          // "slot3.sb2" / "quick.sb2"
        int slot;             // -1: the quick save
        String name = "";     // typed on the game's Save Game screen
        int scene;            // scene<<16 | level<<8 | chapter
        long modified;
        byte[] data;          // import: the whole file
        int[] thumb;          // ARGB 320x240, or null
        Bitmap bitmap() { return thumb == null ? null : Bitmap.createBitmap(thumb, THUMB_W, THUMB_H, Bitmap.Config.ARGB_8888); }
        int chapter() { return scene & 0xf; }
        String label() { return slot < 0 ? "Quick save" : "Slot " + (slot + 1); }
        String place() { return prettyName(scene); }
        String date() { return modified > 0 ? new SimpleDateFormat("d MMM yyyy, HH:mm", Locale.getDefault()).format(new Date(modified)) : ""; }
    }

    private SaveFiles() {}

    static int slotOf(String file) {
        Matcher m = NAME.matcher(file);
        if (!m.matches()) return Integer.MIN_VALUE;
        return m.group(1) == null ? -1 : Integer.parseInt(m.group(1));
    }

    private static int u32(byte[] b, int o) { return (b[o] & 255) | (b[o + 1] & 255) << 8 | (b[o + 2] & 255) << 16 | (b[o + 3] & 255) << 24; }

    /** Parses a save's header; null when it is not a save (empty placeholder, wrong magic, cut short). */
    static Info parse(String file, byte[] b, int len, long modified, boolean thumbnail) {
        if (len < 44 || u32(b, 32) != SHOT) return null;
        Info s = new Info();
        s.file = file; s.slot = slotOf(file); s.modified = modified;
        int n = 0; while (n < 32 && b[n] != 0) n++;
        try { s.name = new String(b, 0, n, "ISO-8859-1").trim(); } catch (java.io.UnsupportedEncodingException e) { s.name = ""; }
        int count = u32(b, 36), o = 40;
        if (count > 0) {
            if (count > len - o - 4) return null;
            if (thumbnail && count >= THUMB_W * THUMB_H * 2) {
                int[] px = new int[THUMB_W * THUMB_H];
                for (int i = 0; i < px.length; i++) {
                    int v = (b[o + i * 2] & 255) | (b[o + i * 2 + 1] & 255) << 8;
                    int r = v >> 11 & 31, g = v >> 5 & 63, bl = v & 31;
                    px[i] = 0xff000000 | (r << 3 | r >> 2) << 16 | (g << 2 | g >> 4) << 8 | (bl << 3 | bl >> 2);
                }
                s.thumb = px;
            }
            o += count;
        }
        if (o + 4 > len) return null;
        s.scene = u32(b, o);
        return s;
    }

    /** Reads just the header part of a save file (the name, thumbnail and scene id). */
    static Info read(File f, boolean thumbnail) {
        long len = f.length();
        if (len < 44 || len > MAX_SAVE) return null;
        int want = (int) Math.min(len, 44 + THUMB_W * THUMB_H * 2 + 64);
        byte[] b = new byte[want];
        try (InputStream in = new FileInputStream(f)) {
            int n = 0, r;
            while (n < want && (r = in.read(b, n, want - n)) > 0) n += r;
            return parse(f.getName(), b, n, f.lastModified(), thumbnail);
        } catch (IOException | SecurityException e) {
            return null;
        }
    }

    /** Every real save in saved/ (quick save first, then by slot). */
    static List<Info> list(File saved, boolean thumbnails) {
        List<Info> out = new ArrayList<Info>();
        Info q = read(new File(saved, "quick.sb2"), thumbnails);
        if (q != null) out.add(q);
        for (int i = 0; i <= 98; i++) { Info s = read(new File(saved, "slot" + i + ".sb2"), thumbnails); if (s != null) out.add(s); }
        return out;
    }

    /** The save Resume would load: settings.cfg's Save= slot when it exists, else the newest save; null when there is none. */
    static Info last(File saved, File... configs) {
        for (File cfg : configs) {
            int slot = saveSetting(cfg);
            if (slot >= 0) { Info s = read(new File(saved, "slot" + slot + ".sb2"), true); if (s != null) return s; }
        }
        Info best = null;
        for (Info s : list(saved, false)) if (best == null || s.modified > best.modified) best = s;
        return best == null ? null : read(new File(saved, best.file), true);
    }

    private static int saveSetting(File cfg) {
        if (cfg == null || !cfg.isFile() || cfg.length() > 4096) return -1;
        try (InputStream in = new FileInputStream(cfg)) {
            byte[] b = new byte[(int) cfg.length()]; int n = 0, r;
            while (n < b.length && (r = in.read(b, n, b.length - n)) > 0) n += r;
            for (String line : new String(b, 0, n, "UTF-8").split("\n")) {
                line = line.trim();
                if (line.startsWith("Save=")) { int v = Integer.parseInt(line.substring(5).trim()); return v >= 0 && v <= 98 ? v : -1; }
            }
        } catch (IOException | NumberFormatException | SecurityException e) { }
        return -1;
    }

    static boolean used(File saved, String file) { File f = new File(saved, file); return f.isFile() && f.length() > 0; }

    // ---------------------------------------------------------------- zip export / import

    static void exportZip(File saved, List<Info> saves, OutputStream raw) throws IOException {
        try (ZipOutputStream zip = new ZipOutputStream(raw)) {
            byte[] buf = new byte[16384];
            for (Info s : saves) {
                File f = new File(saved, s.file);
                ZipEntry e = new ZipEntry("saved/" + s.file);
                e.setTime(f.lastModified());
                zip.putNextEntry(e);
                try (InputStream in = new FileInputStream(f)) { int n; while ((n = in.read(buf)) > 0) zip.write(buf, 0, n); }
                zip.closeEntry();
            }
        }
    }

    /** The saves inside a .zip (any folder; only slotN.sb2 / quick.sb2 names that parse as saves). */
    static List<Info> readZip(InputStream raw) throws IOException {
        List<Info> out = new ArrayList<Info>();
        try (ZipInputStream zip = new ZipInputStream(raw)) {
            ZipEntry e; int entries = 0;
            byte[] buf = new byte[16384];
            while ((e = zip.getNextEntry()) != null) {
                if (++entries > MAX_ENTRIES) throw new IOException("too many files in the .zip");
                if (e.isDirectory()) continue;
                String base = e.getName().replace('\\', '/');
                base = base.substring(base.lastIndexOf('/') + 1).toLowerCase(Locale.ROOT);
                if (slotOf(base) == Integer.MIN_VALUE) continue;
                ByteArrayOutputStream data = new ByteArrayOutputStream();
                int n;
                while ((n = zip.read(buf)) > 0) {
                    data.write(buf, 0, n);
                    if (data.size() > MAX_SAVE) throw new IOException(base + " is too big to be a save");
                }
                byte[] b = data.toByteArray();
                Info s = parse(base, b, b.length, e.getTime() > 0 ? e.getTime() : 0, true);
                if (s == null) continue;
                s.data = b;
                for (int i = 0; i < out.size(); i++) if (out.get(i).file.equals(base)) { out.remove(i); break; }
                out.add(s);
            }
        }
        return out;
    }

    /**
     * Writes imported saves into saved/. freeSlots: every save goes to an unused slot number (the quick save too);
     * else each keeps its own name, replacing what is there. Returns a line per save written.
     */
    static List<String> importSaves(File saved, List<Info> saves, boolean freeSlots) throws IOException {
        if (!saved.isDirectory() && !saved.mkdirs()) throw new IOException("no saved/ folder");
        List<String> done = new ArrayList<String>();
        boolean[] taken = new boolean[99];
        for (int i = 0; i <= 98; i++) taken[i] = used(saved, "slot" + i + ".sb2");
        for (Info s : saves) {
            String target = s.file;
            if (freeSlots) {
                int slot = -1;
                for (int i = 0; i <= 98; i++) if (!taken[i]) { slot = i; break; }
                if (slot < 0) throw new IOException("all 99 slots are used");
                taken[slot] = true;
                target = "slot" + slot + ".sb2";
            }
            if (slotOf(target) == Integer.MIN_VALUE) continue;
            File tmp = new File(saved, target + ".tmp"), dest = new File(saved, target);
            try (OutputStream out = new FileOutputStream(tmp)) { out.write(s.data); }
            if (!tmp.renameTo(dest)) { tmp.delete(); throw new IOException("could not write " + target); }
            done.add(s.label() + " -> " + (slotOf(target) < 0 ? "quick save" : "slot " + (slotOf(target) + 1)));
        }
        return done;
    }

    // ---------------------------------------------------------------- place names
    // port: Java copy of game/src/port/CompanionFeed.h companionPlace / companionPrettyName (keep the two in step). No art.
    private static final String[][][] PLACES = {
        {},
        { {"Intro"}, {"Bikini Bottom", "Map"}, {"The Krusty Krab", "", "Dining Room", "Order Counter", "Salad Bar", "Kitchen", "Dish Pit", "Outside", "Storage Cabinet"},
          {"The Neighborhood", "", "Garden", "Front Yard", "Back Yard", "Living Room", "Kitchen", "Parking Sign"},
          {"The Supermarket", "", "Entrance", "Parking Lot", "Checkout", "Food Aisle", "Food Aisle", "Food Aisle", "Parking Lot"},
          {"Goo Lagoon", "", "Beach", "Parking Lot", "Lifeguard Area", "Lifeguard Station", "Sand Castle", "Castle Exit", "Boom Box", "Treasure Chest", "Treasure Chest"},
          {"Downtown", "", "Bus Stop", "Jewelry Shop", "Jewelry Shop Counter", "Bus Stop"} },
        { {"Intro"}, {"Rock Bottom", "Map"}, {"The Bus Station", "", "Ticket Window", "Waiting Area", "Road", "Road", "Road", "Road", "Entrance"},
          {"The Rusty Anchor", "", "Entrance", "Parking Lot", "Bar", "Cola Bar", "Kegs", ""},
          {"WSEA TV", "Camera", "Entrance", "Parking Lot", "News Room", "Studio", "Control Room", "Weather Control Room", "Camera Room", "Broadcast Room", "Stage"},
          {"Marlin's Cave", "", "Entrance", "Magic Hat", "Slot Machine", "Cavern", "Stairs", "Stairs", "", "Exit"}, null },
        { {"Intro"}, {"Bikini Bottom", "Map"}, {"The Diner", "", "Outside", "Entrance", "Dining Area", "Jukebox", "Dart Board"},
          {"Jellyfish Fields", "", "Fields", "Fields", "", "Fields"},
          {"Home Lane", "Living Room", "Pineapple Row", "Squidward's House", "Stairs", "Road", "Pineapple Row", "Pyramid", "Stage", "SpongeBob's House", "Living Room"},
          {"Sardine's", "", "Entrance", "Dining Room", "Dining Room"},
          {"Sandy's Tree", "Entrance", "Dome Door", "Picnic Area", "Picnic Table", "Dome", "Picnic Area"} },
        { {"Intro"}, {"Bikini Bottom", "Map"}, {"The Rocket Landing", "", "Bottoms Up", "Phone Booth", "Rocket", ""},
          {"Waverly Hills", "", "Gate", "Fountain", "Rodeo Drive", "Rodeo Drive", "Shack", "", "Shack Back Room", "Mansion Living Room", "Pedestal"},
          {"Sublime Seafoods", "Entrance", "", "Bottoms Up", "Security Gate", "Path", "Entrance", "Dining Room", "Lobby", "Reserved Table", "Binoculars"},
          {"Oxygen Springs", "", "Entrance", "Geysers", "Locker Room", "Geysers", "Locker Room"}, null },
    };

    /** "The Krusty Krab - Kitchen"; unknown places keep a plain "Chapter 1, Level 2, Scene 6" (companionPrettyName). */
    static String prettyName(int id) {
        int ch = id & 0xf, lv = (id >> 8) & 0xf, sc = (id >> 16) & 0xf;
        String[] p = ch >= 1 && ch <= 4 && lv <= 6 && lv < PLACES[ch].length ? PLACES[ch][lv] : null;
        if (p == null) return "Chapter " + ch + ", Level " + lv + ", Scene " + sc;
        if (lv == 0 || lv == 1) return p[0];
        String room = sc < 10 && sc + 1 < p.length ? p[sc + 1] : null;
        if (room != null && room.length() > 0) return p[0] + " - " + room;
        return p[0] + " - Area " + sc;
    }
}
