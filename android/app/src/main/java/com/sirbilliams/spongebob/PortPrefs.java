package com.sirbilliams.spongebob;

import android.app.Activity;
import android.content.SharedPreferences;

/**
 * port: the in-game port menu's bridge to the launcher's preferences (docs/port-menu.md). The native side
 * (game/src/port/PortMenu.cpp) loads the starting values with {@link #load} and, on every change in the port menu,
 * calls {@link #save}; both write the very same keys the launcher's Captain's settings use:
 * <pre>
 *   "settings": widescreen (int 0 Fill, 1 Side panels, 2 Stretch, 3 Off), cursor (int 0 Spatula, 1 Net, 2 Bubble, 3 Classic),
 *               pad_cursor (boolean), diagnostics (boolean, new: the in-game overlay)
 *   CompanionPanel.PREFS: bottom_panel (boolean: panel / touchpad on), bottom_mode (int CompanionView.MODE_PANEL / MODE_PAD)
 * </pre>
 * Native calls come from the game thread; the bottom screen is changed on the UI thread, at once.
 */
final class PortPrefs {
    static final String SETTINGS = "settings";
    static final String KEY_DIAG = "diagnostics";

    private PortPrefs() {}

    /** {bottom (0 Panel, 1 Touchpad, 2 Off), cursor, widescreen, pad_cursor (0/1), diagnostics (0/1)}. */
    static int[] load(Activity a) {
        SharedPreferences s = a.getSharedPreferences(SETTINGS, Activity.MODE_PRIVATE);
        SharedPreferences p = a.getSharedPreferences(CompanionPanel.PREFS, Activity.MODE_PRIVATE | Activity.MODE_MULTI_PROCESS);
        boolean on = a instanceof SpongeActivity && ((SpongeActivity) a).mPanel != null
                ? ((SpongeActivity) a).mPanel.isEnabled() : CompanionPanel.wanted(p, false);
        int mode = p.getInt(CompanionPanel.KEY_MODE, CompanionView.MODE_PANEL);
        int bottom = !on ? 2 : mode == CompanionView.MODE_PAD ? 1 : 0;
        return new int[] {
            bottom,
            Math.floorMod(s.getInt("cursor", 0), 4),
            Math.floorMod(s.getInt("widescreen", 0), 4),
            s.getBoolean("pad_cursor", true) ? 1 : 0,
            s.getBoolean(KEY_DIAG, false) ? 1 : 0 };
    }

    static void save(final Activity a, int bottom, int cursor, int widescreen, int pad, int diag) {
        a.getSharedPreferences(SETTINGS, Activity.MODE_PRIVATE).edit()
                .putInt("cursor", Math.floorMod(cursor, 4)).putInt("widescreen", Math.floorMod(widescreen, 4))
                .putBoolean("pad_cursor", pad != 0).putBoolean(KEY_DIAG, diag != 0).apply();
        a.getSharedPreferences(CompanionPanel.PREFS, Activity.MODE_PRIVATE | Activity.MODE_MULTI_PROCESS).edit()
                .putBoolean(CompanionPanel.KEY_ON, bottom != 2)
                .putInt(CompanionPanel.KEY_MODE, bottom == 1 ? CompanionView.MODE_PAD : CompanionView.MODE_PANEL).apply();
        a.runOnUiThread(new Runnable() {
            @Override public void run() {
                if (a instanceof SpongeActivity && ((SpongeActivity) a).mPanel != null) ((SpongeActivity) a).mPanel.portApply();
            }
        });
    }
}
