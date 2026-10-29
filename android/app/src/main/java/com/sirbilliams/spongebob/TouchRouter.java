package com.sirbilliams.spongebob;

import android.app.Activity;
import android.content.Context;
import android.content.SharedPreferences;
import android.view.Display;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.View;

import java.util.HashMap;
import java.util.Map;

/**
 * port: routing of bottom-screen touches that reach the GAME window instead of the panel (docs/bottom-touch.md).
 *
 * On some firmwares (AYN's "bind input to the focused / touched screen") the bottom touchscreen's events are delivered to the
 * focused window (the game Activity on the top screen) rather than to the non-focusable Presentation, with the display id of
 * the bottom display, or with the top display's but from the bottom touchscreen's input device. The game Activity asks
 * {@link #isBottom} for every touch; a bottom-screen touch is mapped and forwarded to the panel view and consumed (so it never
 * clicks the game). A touch from any other device is left alone (the top screen is a normal absolute pointer).
 *
 * How a device is known to be the bottom touchscreen, in this order:
 *  1. the event's display id is the panel's display id (API 29+);
 *  2. the device is bound to the panel's display (hidden InputDevice.getAssociatedDisplayId, API 29+);
 *  3. its descriptor was seen delivering events to the panel view or with the panel's display id earlier (remembered in prefs);
 *  4. with two or more touchscreens, the one whose range has the bottom screen's aspect ratio (1240x1080), not the top's (16:9).
 */
final class TouchRouter {
    private static final String KEY_DESC = "bottom_touch_desc";

    private final Context mCtx;
    private final SharedPreferences mPrefs;
    private final Map<Integer, Boolean> mVerdict = new HashMap<Integer, Boolean>();   // device id -> is the bottom touchscreen
    private String mLearned;

    TouchRouter(Context ctx) {
        mCtx = ctx;
        mPrefs = ctx.getSharedPreferences(CompanionPanel.PREFS, Context.MODE_PRIVATE | Context.MODE_MULTI_PROCESS);
        mLearned = mPrefs.getString(KEY_DESC, null);
    }

    /** Forgets the per-device verdicts (the panel moved to another display, or a device came and went). */
    void reset() { mVerdict.clear(); }

    /** Remembers the device as the bottom touchscreen (it delivered a touch to the panel itself). */
    void learn(MotionEvent e) {
        InputDevice d = InputDevice.getDevice(e.getDeviceId());
        if (d == null || d.getDescriptor() == null) return;
        if (!d.getDescriptor().equals(mLearned)) {
            mLearned = d.getDescriptor();
            mPrefs.edit().putString(KEY_DESC, mLearned).apply();
            mVerdict.clear();
            TouchDiag.log("learned bottom touchscreen: " + TouchDiag.deviceLabel(e.getDeviceId()) + " " + mLearned);
        }
        mVerdict.put(e.getDeviceId(), Boolean.TRUE);
    }

    /** Whether a touch reaching the game window came from the bottom screen. */
    boolean isBottom(MotionEvent e, int panelDisplayId, int panelW, int panelH) {
        if (TouchDiag.displayIdOf(e) == panelDisplayId && panelDisplayId != Display.DEFAULT_DISPLAY) {
            learn(e);
            return true;
        }
        Boolean v = mVerdict.get(e.getDeviceId());
        if (v == null) {
            v = decide(e.getDeviceId(), panelDisplayId, panelW, panelH);
            mVerdict.put(e.getDeviceId(), v);
        }
        return v;
    }

    private Boolean decide(int id, int panelDisplayId, int panelW, int panelH) {
        InputDevice d = InputDevice.getDevice(id);
        if (d == null || (d.getSources() & InputDevice.SOURCE_TOUCHSCREEN) != InputDevice.SOURCE_TOUCHSCREEN) return Boolean.FALSE;
        String why = null;
        if (TouchDiag.associatedDisplay(d) == panelDisplayId && panelDisplayId != Display.DEFAULT_DISPLAY) why = "bound to the panel display";
        else if (mLearned != null && mLearned.equals(d.getDescriptor())) why = "remembered as bottom";
        else if (byRange(d, panelW, panelH)) why = "range matches the bottom screen's aspect";
        TouchDiag.log("device " + TouchDiag.deviceLabel(id) + " -> " + (why != null ? "BOTTOM (" + why + ")" : "top/other"));
        return why != null;
    }

    /** Two or more touchscreens: this one has the panel's aspect (within 4%) and no other touchscreen does. */
    private boolean byRange(InputDevice d, int panelW, int panelH) {
        if (panelW <= 0 || panelH <= 0) return false;
        double want = (double) panelW / panelH;
        int touch = 0;
        boolean me = false;
        for (int id : InputDevice.getDeviceIds()) {
            InputDevice o = InputDevice.getDevice(id);
            if (o == null || (o.getSources() & InputDevice.SOURCE_TOUCHSCREEN) != InputDevice.SOURCE_TOUCHSCREEN) continue;
            InputDevice.MotionRange rx = o.getMotionRange(MotionEvent.AXIS_X, InputDevice.SOURCE_TOUCHSCREEN);
            InputDevice.MotionRange ry = o.getMotionRange(MotionEvent.AXIS_Y, InputDevice.SOURCE_TOUCHSCREEN);
            if (rx == null || ry == null || ry.getMax() <= 0) continue;
            touch++;
            double a = (rx.getMax() + 1) / (ry.getMax() + 1);
            boolean match = Math.abs(a - want) / want < 0.04;
            if (o.getId() == d.getId()) me = match;
            else if (match) return false;                 // two look alike: no verdict
        }
        return touch >= 2 && me;
    }

    /**
     * Maps a touch seen by the game window onto the panel view's pixels. With the panel's display id the coordinates are
     * already the bottom display's pixels; otherwise they are the top window's, scaled from the window to the panel.
     */
    static MotionEvent mapped(MotionEvent e, Activity game, View panel, int panelDisplayId) {
        MotionEvent c = MotionEvent.obtain(e);
        float x = e.getX(), y = e.getY();
        View decor = game.getWindow().getDecorView();
        int pw = panel.getWidth(), ph = panel.getHeight();
        if (pw <= 0 || ph <= 0 || decor.getWidth() <= 0 || decor.getHeight() <= 0) return c;
        boolean native2 = TouchDiag.displayIdOf(e) == panelDisplayId && panelDisplayId != Display.DEFAULT_DISPLAY;
        if (!native2 || x > pw + 1 || y > ph + 1) {
            x = x / decor.getWidth() * pw;
            y = y / decor.getHeight() * ph;
        }
        c.setLocation(x, y);
        return c;
    }
}
