package com.sirbilliams.spongebob;

import android.content.Context;
import android.util.Log;
import android.view.Display;
import android.view.InputDevice;
import android.view.MotionEvent;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.lang.reflect.Method;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;

/**
 * port: bottom-screen touch diagnostics (docs/bottom-touch.md). Always on and cheap: a handful of lines when the panel opens
 * (every input device, the displays) and the first few MotionEvents seen by the panel view and by the game Activity, written to
 * <files>/touch-log.txt (+ logcat "SpongeBob"). The launcher's Share log sends this file with omt-log.txt and display-log.txt, so
 * the log answers: does the bottom touchscreen deliver events to the panel, to the game window, or nowhere, and with which display id.
 * Counters are kept for the Touch test screen. One process only writes (the game process, or the test Activity).
 */
final class TouchDiag {
    static final String TAG = "SpongeBob";
    static final String LOG_FILE = "touch-log.txt";
    static final int MAX_EVENTS = 20;                   // MotionEvents logged per source per session

    /** Touches seen so far, per source: [0] the panel view, [1] the game (or test) Activity, [2] of those forwarded to the panel. */
    static final int PANEL = 0, ACTIVITY = 1, FORWARDED = 2;
    private static final int[] sCount = new int[3];
    private static final int[] sLogged = new int[3];
    private static float[] sLast = new float[6];        // last x,y per source
    private static String sLastDevice = "";
    private static Context sCtx;

    private TouchDiag() {}

    static void init(Context context) {
        sCtx = context.getApplicationContext() != null ? context.getApplicationContext() : context;
    }

    /** A new session: counters and the per-source budget start again. */
    static void reset() {
        for (int i = 0; i < 3; i++) { sCount[i] = 0; sLogged[i] = 0; }
    }

    static int count(int source) { return sCount[source]; }
    static float lastX(int source) { return sLast[source * 2]; }
    static float lastY(int source) { return sLast[source * 2 + 1]; }
    static String lastDevice() { return sLastDevice; }

    /** One MotionEvent from source (PANEL / ACTIVITY / FORWARDED): counts it, logs the first MAX_EVENTS (all of their down / up, a few moves). */
    static void event(int source, MotionEvent e, String where) {
        int a = e.getActionMasked();
        if (a == MotionEvent.ACTION_DOWN) sCount[source]++;
        sLast[source * 2] = e.getX(); sLast[source * 2 + 1] = e.getY();
        sLastDevice = deviceLabel(e.getDeviceId());
        if (sLogged[source] >= MAX_EVENTS) return;
        if (a == MotionEvent.ACTION_MOVE && sLogged[source] > 6 && (e.getEventTime() % 8) != 0) return;   // moves are many: the first and a sample
        sLogged[source]++;
        log(where + " #" + sLogged[source] + " " + MotionEvent.actionToString(a)
                + " x=" + Math.round(e.getX()) + " y=" + Math.round(e.getY())
                + " raw=" + Math.round(e.getRawX()) + "," + Math.round(e.getRawY())
                + " display=" + displayIdOf(e) + " device=" + deviceLabel(e.getDeviceId())
                + " src=0x" + Integer.toHexString(e.getSource()) + " ptrs=" + e.getPointerCount());
    }

    /** MotionEvent.getDisplayId() (API 29); the default display (0) before that. */
    static int displayIdOf(MotionEvent e) {
        if (android.os.Build.VERSION.SDK_INT < 29) return Display.DEFAULT_DISPLAY;
        try {
            Method m = MotionEvent.class.getMethod("getDisplayId");
            return (Integer) m.invoke(e);
        } catch (ReflectiveOperationException | RuntimeException ex) {
            return Display.DEFAULT_DISPLAY;
        }
    }

    /** The display the input device is bound to (hidden InputDevice.getAssociatedDisplayId, API 29+), or -1 when unknown / unbound. */
    static int associatedDisplay(InputDevice d) {
        if (d == null || android.os.Build.VERSION.SDK_INT < 29) return -1;
        try {
            Method m = InputDevice.class.getMethod("getAssociatedDisplayId");
            return (Integer) m.invoke(d);
        } catch (ReflectiveOperationException | RuntimeException ex) {
            return -1;
        }
    }

    static String deviceLabel(int id) {
        InputDevice d = InputDevice.getDevice(id);
        return id + (d != null ? "(" + d.getName() + ")" : "");
    }

    /** The panel opened: every input device and the displays, once per session. */
    static void dumpSession(Context ctx, int panelDisplayId, int panelW, int panelH, int gameDisplayId, String why) {
        reset();
        log("---- touch session (" + why + "): panel display " + panelDisplayId + " " + panelW + "x" + panelH
                + ", game display " + gameDisplayId + ", API " + android.os.Build.VERSION.SDK_INT
                + ", " + android.os.Build.MANUFACTURER + " " + android.os.Build.MODEL + " ----");
        int[] ids = InputDevice.getDeviceIds();
        for (int id : ids) {
            InputDevice d = InputDevice.getDevice(id);
            if (d == null) continue;
            int s = d.getSources();
            StringBuilder b = new StringBuilder("input device ").append(id).append(" \"").append(d.getName()).append('"');
            b.append(" sources=0x").append(Integer.toHexString(s));
            if ((s & InputDevice.SOURCE_TOUCHSCREEN) == InputDevice.SOURCE_TOUCHSCREEN) b.append(" TOUCHSCREEN");
            if ((s & InputDevice.SOURCE_TOUCHPAD) == InputDevice.SOURCE_TOUCHPAD) b.append(" TOUCHPAD");
            if ((s & InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD) b.append(" GAMEPAD");
            if ((s & InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK) b.append(" JOYSTICK");
            if ((s & InputDevice.SOURCE_MOUSE) == InputDevice.SOURCE_MOUSE) b.append(" MOUSE");
            b.append(" desc=").append(d.getDescriptor());
            b.append(" vid:pid=").append(Integer.toHexString(d.getVendorId())).append(':').append(Integer.toHexString(d.getProductId()));
            b.append(d.isVirtual() ? " virtual" : "");
            InputDevice.MotionRange rx = d.getMotionRange(MotionEvent.AXIS_X, InputDevice.SOURCE_TOUCHSCREEN);
            InputDevice.MotionRange ry = d.getMotionRange(MotionEvent.AXIS_Y, InputDevice.SOURCE_TOUCHSCREEN);
            if (rx != null && ry != null) b.append(" range=").append(Math.round(rx.getMax())).append('x').append(Math.round(ry.getMax()));
            b.append(" associatedDisplay=").append(associatedDisplay(d));
            log(b.toString());
        }
        android.hardware.display.DisplayManager dm = (android.hardware.display.DisplayManager) ctx.getSystemService(Context.DISPLAY_SERVICE);
        if (dm != null) {
            for (Display d : dm.getDisplays()) {
                android.graphics.Point p = new android.graphics.Point();
                d.getRealSize(p);
                log("display " + d.getDisplayId() + " \"" + d.getName() + "\" " + p.x + "x" + p.y + " flags=0x" + Integer.toHexString(d.getFlags())
                        + (d.getDisplayId() == panelDisplayId ? " <- panel" : "") + (d.getDisplayId() == gameDisplayId ? " <- game" : ""));
            }
        }
    }

    /** One line to logcat and to touch-log.txt (kept below 64 KB: starts afresh when full). */
    static void log(String line) {
        Log.i(TAG, "touch: " + line);
        File dir = sCtx != null ? sCtx.getExternalFilesDir(null) : null;
        if (dir == null) return;
        File f = new File(dir, LOG_FILE);
        boolean append = f.length() < 64 * 1024;
        String stamp = new SimpleDateFormat("HH:mm:ss.SSS", Locale.ROOT).format(new Date());
        FileOutputStream out = null;
        try {
            out = new FileOutputStream(f, append);
            out.write((stamp + " " + line + "\n").getBytes("UTF-8"));
        } catch (IOException e) {
            // no log then
        } finally {
            if (out != null) try { out.close(); } catch (IOException e) { /* ignore */ }
        }
    }
}
