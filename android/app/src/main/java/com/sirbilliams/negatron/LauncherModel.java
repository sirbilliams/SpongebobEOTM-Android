package com.sirbilliams.negatron;

import java.util.ArrayList;
import java.util.List;

/**
 * port: the state of the launcher menu (SetupActivity): a stack of pages, each a list of items with
 * a selection. Drawn by {@link LauncherView} on the main screen and, on a dual-screen device, by a
 * second LauncherView on the bottom screen; plain Java so the desktop preview can build it too.
 */
final class LauncherModel {
    static final class Item {
        final String id;
        final String label;
        String value;               // shown on the right (ON / OFF ...), or null
        String help;                // what the item does, shown in the info window
        String[] detail;            // a save slot's preview lines, or null
        boolean enabled = true;
        boolean hint;               // drawn as a note, not a button (e.g. "later")
        float progress = -1;        // 0..1: a progress bar in the value's LCD (an install); -1 none
        String pic;                 // the info window's picture (LauncherView), else the page's

        Item(String id, String label, String help) {
            this.id = id;
            this.label = label;
            this.help = help;
        }
    }

    static final class Page {
        final String id;
        final String title;
        final List<Item> items = new ArrayList<>();
        int sel;
        String warning;             // a page with a warning shows it above the items (New Game)
        String footer;              // the pad hint under the buttons, or null for the usual one
        String pic;                 // the info window's picture for the page's items (LauncherView)

        Page(String id, String title) {
            this.id = id;
            this.title = title;
        }

        Item add(String id, String label, String help) {
            Item i = new Item(id, label, help);
            items.add(i);
            return i;
        }
    }

    interface Listener { void changed(); }

    private final ArrayList<Page> mStack = new ArrayList<>();
    private final ArrayList<Listener> mListeners = new ArrayList<>();
    /** A picture from the game's own files (read at run time), shown in the info window; or null. */
    Object art;
    /** The game's own pictures for the info window by picture key (continue, new, load, saves,
     *  settings, test, quit), read at run time; a key without one gets a drawn illustration. */
    final java.util.HashMap<String, Object> pics = new java.util.HashMap<>();

    /** The picture key of the selected item (its own, else its page's), or null. */
    String picKey() {
        Page p = page();
        Item i = selected();
        if (i != null && i.pic != null) return i.pic;
        return p != null ? p.pic : null;
    }
    /** The game's box art, which the player chose (Settings), shown alone on the bottom screen
     *  while the main menu is up; or null (then the picture above, or a drawn atom). */
    Object boxArt;
    /** The player's menu background (Settings), drawn behind the main screen's plates; or null. */
    Object background;

    void listen(Listener l) { mListeners.add(l); }

    void changed() { for (Listener l : mListeners) l.changed(); }

    Page page() { return mStack.isEmpty() ? null : mStack.get(mStack.size() - 1); }

    int depth() { return mStack.size(); }

    /** Replaces the whole stack with one page (the main menu). */
    void reset(Page p) {
        mStack.clear();
        push(p);
    }

    void push(Page p) {
        mStack.add(p);
        if (!selectable(p, p.sel)) p.sel = next(p, p.sel, 1);
        changed();
    }

    /** Replaces the top page (a rebuilt one), keeping its selection. */
    void replace(Page p) {
        Page old = page();
        if (old != null) {
            p.sel = Math.min(old.sel, Math.max(0, p.items.size() - 1));
            mStack.set(mStack.size() - 1, p);
        } else {
            mStack.add(p);
        }
        if (!selectable(p, p.sel)) p.sel = next(p, p.sel, 1);
        changed();
    }

    /** Back one page; false on the main menu. */
    boolean pop() {
        if (mStack.size() <= 1) return false;
        mStack.remove(mStack.size() - 1);
        changed();
        return true;
    }

    Item selected() {
        Page p = page();
        return p == null || p.sel < 0 || p.sel >= p.items.size() ? null : p.items.get(p.sel);
    }

    /** Moves the selection by d (+1 down, -1 up), skipping disabled items; wraps around. */
    void move(int d) {
        Page p = page();
        if (p == null) return;
        int n = next(p, p.sel, d);
        if (n != p.sel) {
            p.sel = n;
            changed();
        }
    }

    /** Selects item k if it can be chosen; true then. */
    boolean select(int k) {
        Page p = page();
        if (p == null || !selectable(p, k)) return false;
        if (p.sel != k) {
            p.sel = k;
            changed();
        }
        return true;
    }

    static boolean selectable(Page p, int k) {
        return k >= 0 && k < p.items.size() && p.items.get(k).enabled && !p.items.get(k).hint;
    }

    private static int next(Page p, int from, int d) {
        int n = p.items.size();
        for (int s = 1; s <= n; s++) {
            int k = ((from + d * s) % n + n) % n;
            if (selectable(p, k)) return k;
        }
        return from;
    }
}
