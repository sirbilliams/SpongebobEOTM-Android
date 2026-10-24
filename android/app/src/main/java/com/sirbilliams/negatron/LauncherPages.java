package com.sirbilliams.negatron;

import java.io.BufferedReader;
import java.io.DataInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;
import java.util.Locale;

/**
 * port: the launcher's pages (SetupActivity builds them, LauncherView draws them) and the save-slot
 * listing. Plain Java, so tools/companion/render-menu.sh can draw the same pages on the desktop.
 */
final class LauncherPages {
    private LauncherPages() {}

    /** The game takes NEG_START=continue / load:<slot> (game/src/port/StartMode.h). */
    static final boolean GAME_LOADS = true;
    static final int SLOTS = 10;

    /** A saved game: its slot, the level it was saved in, the score and when. */
    static final class Slot {
        int number;                 // 1..10, 0 = the autosave
        String levelName = "", gam = "";
        long score = -1;
        long time;                  // ms
        boolean present;

        /** The level as the player knows it: the game's own name, else the .gam file. */
        String level() {
            if (levelName.length() > 0) return levelName;
            return gam.toLowerCase(Locale.ROOT).endsWith(".gam") ? gam.substring(0, gam.length() - 4) : gam;
        }

        String[] detail() {
            String title = number == 0 ? "AUTOSAVE" : "SLOT " + number;
            if (!present) return new String[] { title, "EMPTY" };
            ArrayList<String> d = new ArrayList<>();
            d.add(title);
            d.add("LEVEL  " + level().toUpperCase(Locale.ROOT));
            if (score >= 0) d.add("SCORE  " + score);
            if (time > 0) d.add("SAVED  " + new SimpleDateFormat("yyyy-MM-dd HH:mm", Locale.ROOT).format(new Date(time)));
            return d.toArray(new String[0]);
        }
    }

    /** The autosave (index 0) and slots 1..10, from save/slots.txt (written by the game: TAB-separated
     *  slot, level id, level name, gam, score, mtime; docs/controls.md), else from the .tsk headers. */
    static Slot[] slots(File saveDir) {
        Slot[] out = new Slot[SLOTS + 1];
        for (int n = 0; n <= SLOTS; n++) {
            out[n] = new Slot();
            out[n].number = n;
        }
        File list = new File(saveDir, "slots.txt");
        if (list.isFile()) {
            try (BufferedReader r = new BufferedReader(new InputStreamReader(new FileInputStream(list), "ISO-8859-1"))) {
                String line;
                while ((line = r.readLine()) != null) {
                    if (line.startsWith("#")) continue;
                    String[] f = line.split("\t", -1);
                    if (f.length < 6) continue;
                    int n = f[0].equalsIgnoreCase("autosave") ? 0 : parseInt(f[0], -1);
                    if (n < 0 || n > SLOTS) continue;
                    Slot s = out[n];
                    s.levelName = f[2].trim();
                    s.gam = f[3].trim();
                    s.score = parseInt(f[4], -1);
                    s.time = parseInt(f[5], 0) * 1000L;
                    s.present = true;
                }
            } catch (IOException e) {
                // as if empty
            }
            return out;
        }
        for (int n = 0; n <= SLOTS; n++) readTsk(saveDir, out[n]);
        return out;
    }

    private static int parseInt(String s, int def) {
        try {
            return Integer.parseInt(s.trim());
        } catch (NumberFormatException e) {
            return def;
        }
    }

    /** The game has not written slots.txt yet: the .tsk header (big-endian: version string,
     *  "DUMMY   " = empty; int level 4cc; gam string; task string; float health; short lives; int score). */
    private static void readTsk(File saveDir, Slot s) {
        File f = new File(saveDir, fileName(s.number));
        if (!f.isFile()) return;
        try (InputStream in = new FileInputStream(f)) {
            readTsk(in, s);
            if (s.present) s.time = f.lastModified();
        } catch (IOException e) {
            s.present = false;
        }
    }

    static void readTsk(InputStream raw, Slot s) {
        try {
            DataInputStream in = new DataInputStream(raw);
            if (pascal(in).trim().equalsIgnoreCase("DUMMY")) return;
            in.readInt();
            s.gam = pascal(in);
            pascal(in);
            in.readFloat();
            in.readShort();
            s.score = in.readInt();
            s.present = true;
        } catch (IOException e) {
            s.present = false;
        }
    }

    /** The save file of slot n (0 = the autosave). */
    static String fileName(int n) {
        return n == 0 ? "autosave.tsk" : "JimmyGame" + n + ".tsk";
    }

    // ------------------------------------------------------------------ export / import

    /** The save folder's files that travel (the game's own .tsk format, as the PC game's save
     *  folder): everything but slots.txt, which the game rewrites at each start. */
    static boolean exported(String name) {
        return !name.equalsIgnoreCase("slots.txt") && !name.endsWith(".tmp");
    }

    /** The slot a save file's name stands for: 0 autosave.tsk, 1..10 JimmyGame<n>.tsk, -2 another
     *  save file the game keeps (autosave01..17.tsk), -1 not a save file. */
    static int slotOf(String name) {
        String n = name.toLowerCase(Locale.ROOT);
        if (n.equals("autosave.tsk")) return 0;
        if (n.matches("jimmygame([1-9]|10)\\.tsk")) return Integer.parseInt(n.substring(9, n.length() - 4));
        if (n.matches("autosave[0-9]{1,2}\\.tsk")) return -2;
        return -1;
    }

    /** A save file found in an import: its name in the save folder, its bytes and what it holds. */
    static final class Incoming {
        String name;
        byte[] data;
        Slot info = new Slot();
        boolean replaces;           // a save of that name is already there
    }

    static LauncherModel.Page saves() {
        LauncherModel.Page p = new LauncherModel.Page("saves", "Saves");
        p.pic = "saves";
        p.add("export_all", "Export saves", "Write all the game's saves (the autosave and slots 1-10) to one "
                + ".zip: Downloads, Drive... They are the original game's own files, usable in the PC game's "
                + "save folder too. One slot: Load Game, then Y.");
        p.add("import", "Import saves", "Bring saves back from a .zip (or one .tsk file). You see what is in it "
                + "and confirm before any save is replaced.");
        p.add("back", "Back", "Back to the main menu.");
        return p;
    }

    /** What an import holds: Cancel first (the default), Import, then one row per file. */
    static LauncherModel.Page importPage(List<Incoming> files) {
        LauncherModel.Page p = new LauncherModel.Page("import", "Import saves");
        p.pic = "saves";
        int replaced = 0;
        StringBuilder which = new StringBuilder();
        for (Incoming f : files) {
            if (!f.replaces) continue;
            replaced++;
            if (which.length() > 0) which.append(", ");
            which.append(f.info.number == 0 ? "autosave" : f.info.number > 0 ? "slot " + f.info.number : f.name);
        }
        p.warning = replaced == 0 ? "Nothing you have is replaced."
                : "Replaces " + replaced + " save" + (replaced == 1 ? "" : "s") + " you have: " + which + ".";
        p.add("back", "Cancel", "Keep your saves as they are.");
        p.add("import_yes", replaced > 0 ? "Import and replace" : "Import",
                "Copy the " + files.size() + " file" + (files.size() == 1 ? "" : "s") + " into the game's save folder.");
        for (Incoming f : files) {
            int n = f.info.number;
            LauncherModel.Item i = p.add("file", f.name, f.replaces ? "Replaces the save you have." : "New.");
            i.value = f.replaces ? "REPLACES" : "NEW";
            i.detail = n >= 0 ? f.info.detail() : new String[] { f.name.toUpperCase(Locale.ROOT) };
        }
        p.sel = 0;
        return p;
    }

    /** A single .tsk whose name is no slot's: which slot it goes into. */
    static LauncherModel.Page importTarget(Slot[] have) {
        LauncherModel.Page p = new LauncherModel.Page("import_to", "Import into slot");
        p.pic = "saves";
        for (int n = 1; n <= SLOTS; n++) {
            Slot s = have[n];
            LauncherModel.Item i = p.add("to:" + n, n + "  " + (s.present ? s.level() : "---"),
                    s.present ? "Put the save here (you confirm before this one is replaced)." : "Put the save here.");
            i.detail = s.detail();
        }
        p.add("back", "Cancel", "Keep your saves as they are.");
        return p;
    }

    private static String pascal(DataInputStream in) throws IOException {
        int n = in.readUnsignedByte();
        byte[] b = new byte[n];
        in.readFully(b);
        StringBuilder sb = new StringBuilder();
        for (byte x : b) sb.append(x >= 32 && x < 127 ? (char) x : '?');
        return sb.toString();
    }

    static LauncherModel.Page main(Slot autosave, boolean test) {
        LauncherModel.Page p = new LauncherModel.Page("main", "Main menu");
        boolean canContinue = GAME_LOADS && autosave.present;
        LauncherModel.Item i = p.add("continue", "Continue", canContinue
                ? "Carry on from the autosave."
                : GAME_LOADS ? "No autosave yet: start a new game." : "Continue from the autosave: coming with the game's save / load.");
        i.enabled = canContinue;
        if (canContinue) i.detail = autosave.detail();
        p.add("new", "New game", "Start the story from the beginning. The autosave is replaced.");
        i = p.add("load", "Load game", GAME_LOADS ? "Pick one of the game's save slots."
                : "The game's save slots: coming with the game's save / load.");
        i.enabled = GAME_LOADS;
        p.add("saves", "Saves", "Export the game's saves to a file, or import them.");
        p.add("assets", "Asset viewer", "Fly freely through any level, or look at the game's models.");
        p.add("settings", "Settings", "Widescreen, the bottom screen, box art and background.");
        if (test) p.add("test", "Test", "Jump to a level, the model viewer, share the log.");
        p.add("quit", "Quit", "Close the game.");
        for (LauncherModel.Item it : p.items) it.pic = it.id;     // each option its own picture
        return p;
    }

    static LauncherModel.Page newGame() {
        LauncherModel.Page p = new LauncherModel.Page("new", "New game");
        p.pic = "new";
        p.warning = "A new game wipes the autosave: Continue will start from the beginning.";
        p.add("new_yes", "Start a new game", "Wipe the autosave and start from the beginning.");
        p.add("back", "Cancel", "Back to the main menu.");
        p.sel = 1;                                  // the safe choice first
        return p;
    }

    static LauncherModel.Page load(Slot[] slots) {
        LauncherModel.Page p = new LauncherModel.Page("load", "Load game");
        p.pic = "load";
        for (int n = 1; n < slots.length; n++) {
            Slot s = slots[n];
            LauncherModel.Item i = p.add("slot:" + n, n + "  " + (s.present ? s.level() : "---"),
                    s.present ? "Load this game. Y (or hold): export this slot to a file." : "Empty slot.");
            i.value = s.present && s.score >= 0 ? String.valueOf(s.score) : null;
            i.enabled = s.present && GAME_LOADS;
            i.detail = s.detail();
        }
        p.add("back", "Back", "Back to the main menu.");
        p.footer = "A  LOAD     Y  EXPORT THIS SLOT     B  BACK";
        return p;
    }

    static LauncherModel.Page settings(boolean wide, boolean panel, boolean secondScreen,
            boolean boxArt, boolean background) {
        return settings(wide, true, "a", "a", panel, secondScreen, boxArt, background);
    }

    /** port: Bill's UI's gadget hologram (NEG_UI_HOLO a / b / c / off, game/src/port/BillsUi.h). */
    static final String[] HOLO_IDS = { "a", "b", "c", "off" };
    static final String[] HOLO_NAMES = { "BAY", "CONSOLE", "WHEEL", "OFF" };

    static String holoName(String id) {
        for (int i = 0; i < HOLO_IDS.length; i++) if (HOLO_IDS[i].equals(id)) return HOLO_NAMES[i];
        return HOLO_NAMES[0];
    }

    /** The next (dir 1) or previous (dir -1) gadget hologram after `id`. */
    static String holoStep(String id, int dir) {
        int n = HOLO_IDS.length, i = 0;
        for (int k = 0; k < n; k++) if (HOLO_IDS[k].equals(id)) i = k;
        return HOLO_IDS[((i + dir) % n + n) % n];
    }

    /** The game's number for a hologram id: 0 off, 1 BAY, 2 CONSOLE, 3 WHEEL. */
    static int holoNumber(String id) {
        if ("off".equals(id)) return 0;
        if ("b".equals(id)) return 2;
        if ("c".equals(id)) return 3;
        return 1;
    }

    /** port: the energy meter designs of Bill's UI (NEG_UI_ENERGY a / b / c, game/src/port/BillsUi.h). */
    static final String[] ENERGY_IDS = { "a", "b", "c" };
    static final String[] ENERGY_NAMES = { "TUBE", "ORBIT", "DIAL" };

    static String energyName(String id) {
        for (int i = 0; i < ENERGY_IDS.length; i++) if (ENERGY_IDS[i].equals(id)) return ENERGY_NAMES[i];
        return ENERGY_NAMES[0];
    }

    /** The next (dir 1) or previous (dir -1) energy meter after `id`. */
    static String energyStep(String id, int dir) {
        int n = ENERGY_IDS.length, i = 0;
        for (int k = 0; k < n; k++) if (ENERGY_IDS[k].equals(id)) i = k;
        return ENERGY_IDS[((i + dir) % n + n) % n];
    }

    static LauncherModel.Page settings(boolean wide, boolean billsUi, String energy, String holo, boolean panel,
            boolean secondScreen, boolean boxArt, boolean background) {
        LauncherModel.Page p = new LauncherModel.Page("settings", "Settings");
        p.pic = "settings";
        LauncherModel.Item i = p.add("wide", "Widescreen 16:9", "Fill a 16:9 screen; the HUD moves to the edges. Off: the original 4:3.");
        i.value = wide ? "ON" : "OFF";
        i = p.add("ui", "In-game UI", "Bill's UI: the port's polish (the pause screen's scope, the score "
                + "counting up, framed help pictures, the sonar readout centred). Vanilla: the original look. "
                + "Also in game: hold Back.");
        i.value = billsUi ? "BILL'S UI" : "VANILLA";
        if (billsUi) {
            i = p.add("energy", "Energy meter", "Bill's UI's energy meter: TUBE (a steel LED tube), ORBIT (an "
                    + "atom with an orbit gauge) or DIAL (the bottom screen's dial). Also in game: hold Back.");
            i.value = energyName(energy);
            i = p.add("holo", "Gadget hologram", "Bill's UI's picture of the current gadget or vehicle: BAY (a "
                    + "framed bay), CONSOLE (a wrist console with your gadgets), WHEEL (a gadget wheel) or OFF "
                    + "(the game's own). Also in game: hold Back.");
            i.value = holoName(holo);
        }
        i = p.add("panel", "Dual screen", (secondScreen ? "" : "No second screen found. ")
                + "The bottom screen shows the level map, the tasks and the gadgets while you play "
                + "(on by default when there is a second screen). Also in game: hold Back.");
        i.value = panel ? "ON" : "OFF";
        i = p.add("boxart", "Box art: choose image...", "A picture of the game's box (your own photo or "
                + "scan) for the bottom screen while the main menu is up. It is copied into the app."
                + (boxArt ? " Now: your picture." : " Now: a picture from the game."));
        i = p.add("boxart_clear", "Box art: clear", "Remove the chosen box art (the bottom screen then shows "
                + "a picture from the game).");
        i.enabled = boxArt;
        i = p.add("bg", "Menu background: choose image...", "A picture of your own behind this menu "
                + "(16:9 fits best). It is copied into the app."
                + (background ? " Now: your picture." : " Now: brushed steel."));
        i = p.add("bg_clear", "Menu background: clear", "Back to the plain brushed steel.");
        i.enabled = background;
        i = p.add("hd", "HD texture pack: install (.zip)", "Sharper pictures: a zip of upscaled copies of "
                + "the game's own textures (omt/, png/ and grn/ folders, each picture 2x, 4x... its size). "
                + "It is unpacked into the game's hd/ folder.");
        i = p.add("hd_remove", "HD texture pack: remove", "Delete the installed HD pack (the game goes back to "
                + "its own pictures).");
        p.add("back", "Back", "Back to the main menu.");
        return p;
    }

    static LauncherModel.Page test() {
        LauncherModel.Page p = new LauncherModel.Page("test", "Test");
        p.pic = "test";
        p.add("levels", "Jump to a level", "Play one level straight away, without its opening (NEG_TEST_PLAY).");
        p.add("viewer", "Level viewer (OMT)", "The engine's own viewer: fly through the level .omt files.");
        p.add("log", "Share log", "Send the last run's log (omt-log.txt) as text.");
        p.add("back", "Back", "Back to the main menu.");
        return p;
    }

    // ------------------------------------------------------------------ port: the Asset Viewer

    static LauncherModel.Page assets() {
        LauncherModel.Page p = new LauncherModel.Page("assets", "Asset viewer");
        p.pic = "assets";
        p.add("explore", "Explore a level", "Fly freely through any level with a noclip camera: "
                + "nothing moves, no story, no enemies, nothing is saved.");
        LauncherModel.Item m = p.add("models", "Model viewer", null);
        m.detail = new String[] { "Models: .grn .ase .omt", "Up/down model  L1/R1 jump 10  Sticks turn",
            "L2/R2 zoom  A auto-turn  B leave" };
        p.add("back", "Back", "Back to the main menu.");
        return p;
    }

    /** The explorer's controls (NEG_ASSET_VIEW, game/src/port/AssetView.h). */
    static final String[] EXPLORE_HELP = {
        "L stick move  R stick look  L2/R2 down/up",
        "A speed  Y fog  X Jimmy  Start reset  B leave" };

    /** Explore a level: the installed gam files, named as the bottom-screen panel names them (save/levels.txt,
     *  written by the game at every start; before that, the file names). */
    static LauncherModel.Page explore(File saveDir, List<String> gams, String select) {
        java.util.HashMap<String, String> names = new java.util.HashMap<>();
        File list = new File(saveDir, "levels.txt");
        if (list.isFile()) {
            try (BufferedReader r = new BufferedReader(new InputStreamReader(new FileInputStream(list), "ISO-8859-1"))) {
                String line;
                while ((line = r.readLine()) != null) {
                    if (line.startsWith("#")) continue;
                    String[] f = line.split("\t", -1);
                    if (f.length >= 2 && f[1].trim().length() > 0) names.put(f[0].toLowerCase(Locale.ROOT), f[1].trim());
                }
            } catch (IOException e) {
                // the file names then
            }
        }
        java.util.HashMap<String, Integer> uses = new java.util.HashMap<>();
        for (String g : gams) {
            String n = names.get(g.toLowerCase(Locale.ROOT));
            if (n != null) uses.put(n, uses.containsKey(n) ? uses.get(n) + 1 : 1);
        }
        LauncherModel.Page p = new LauncherModel.Page("explore", "Explore a level");
        p.pic = "assets";
        for (String g : gams) {
            String base = g.toLowerCase(Locale.ROOT).endsWith(".gam") ? g.substring(0, g.length() - 4) : g;
            String n = names.get(g.toLowerCase(Locale.ROOT));
            String label = n == null ? base : uses.get(n) > 1 ? n + " (" + base + ")" : n;
            LauncherModel.Item i = p.add("explore:" + g, label, null);
            i.detail = new String[] { label + "   " + g, EXPLORE_HELP[0], EXPLORE_HELP[1] };
            if (g.equalsIgnoreCase(select != null ? select : "Level5.gam")) p.sel = p.items.size() - 1;
        }
        p.add("back", "Back", "Back to the Asset viewer.");
        return p;
    }

    static LauncherModel.Page levels(List<String> gams) {
        LauncherModel.Page p = new LauncherModel.Page("levels", "Jump to a level");
        p.pic = "test";
        for (String g : gams) {
            p.add("level:" + g, g, "Play " + g + " from its start.");
            if (g.equalsIgnoreCase("Level5.gam")) p.sel = p.items.size() - 1;
        }
        p.add("back", "Back", "Back to the test page.");
        return p;
    }
}
