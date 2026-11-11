package com.sirbilliams.spongebob;

import android.app.ActivityOptions;
import android.app.Presentation;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.hardware.display.DisplayManager;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.Display;
import android.view.Gravity;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.PixelCopy;
import android.view.SurfaceView;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

/** Owns TV selection, moving the existing SDL task, and firmware fallback. Main thread only. */
final class TvOutput implements DisplayManager.DisplayListener {
    static final String KEY = "tv_output"; // boolean: Auto=true (default), Off=false
    private final SpongeActivity activity;
    private final DisplayManager displays;
    private final Handler handler = new Handler(Looper.getMainLooper());
    private boolean resumed, moving, closed;
    private int refused = -1;
    private Presentation padWindow, mirrorWindow;
    private FrameLayout overlay;
    private CompanionView pad;
    private ImageView mirror;
    private Bitmap front, back;
    private boolean copying;
    private int copyError = -1;

    TvOutput(SpongeActivity a) {
        activity = a;
        displays = (DisplayManager) a.getSystemService(Context.DISPLAY_SERVICE);
        if (displays != null) displays.registerDisplayListener(this, handler);
        BottomScreen.find(a, displays, "TV start");
    }

    static boolean auto(Context c) {
        return c.getSharedPreferences("settings", Context.MODE_PRIVATE | Context.MODE_MULTI_PROCESS)
                .getBoolean(KEY, true);
    }

    static Display external(Context c, DisplayManager dm) {
        if (dm == null) return null;
        Display bottom = BottomScreen.find(c, dm);
        Display pick = null;
        for (Display d : dm.getDisplays(DisplayManager.DISPLAY_CATEGORY_PRESENTATION)) {
            int type = BottomScreen.typeOf(d);
            if (!d.isValid() || d.getDisplayId() == Display.DEFAULT_DISPLAY
                    || (bottom != null && d.getDisplayId() == bottom.getDisplayId())
                    || (d.getFlags() & Display.FLAG_PRIVATE) != 0
                    || (d.getFlags() & Display.FLAG_PRESENTATION) == 0
                    || type == BottomScreen.TYPE_INTERNAL || type == BottomScreen.TYPE_OVERLAY) continue;
            // Public virtual displays can be wireless receivers; presentation category is required.
            if (pick == null || d.getDisplayId() < pick.getDisplayId()) pick = d;
        }
        return pick;
    }

    void resume() { resumed = true; handler.post(update); }
    void pause() { resumed = false; hide(); }
    void destroy() {
        closed = true;
        if (displays != null) displays.unregisterDisplayListener(this);
        handler.removeCallbacksAndMessages(null);
        hide();
    }
    /** Rescue top-screen touches delivered to the focused TV Activity by Thor firmware. */
    boolean routeTouch(MotionEvent e) {
        if (padWindow == null || pad == null) return false;
        InputDevice device = e.getDevice();
        if (TouchDiag.displayIdOf(e) != Display.DEFAULT_DISPLAY
                && (device == null || TouchDiag.associatedDisplay(device) != Display.DEFAULT_DISPLAY)) return false;
        MotionEvent copy = MotionEvent.obtain(e);
        // A top-display event already has top pixels; a mislabelled event has game-window pixels.
        if (TouchDiag.displayIdOf(e) != Display.DEFAULT_DISPLAY) {
            View decor = activity.getWindow().getDecorView();
            copy.setLocation(e.getX() * pad.getWidth() / Math.max(1, decor.getWidth()),
                    e.getY() * pad.getHeight() / Math.max(1, decor.getHeight()));
        }
        pad.dispatchTouchEvent(copy);
        copy.recycle();
        return true;
    }

    private int current() { return activity.getWindowManager().getDefaultDisplay().getDisplayId(); }
    private final Runnable update = () -> update();

    private void update() {
        if (!resumed || moving || closed) return;
        Display tv = auto(activity) ? external(activity, displays) : null;
        int target = tv == null ? Display.DEFAULT_DISPLAY : tv.getDisplayId();
        if (current() != target && target != refused) {
            if (Build.VERSION.SDK_INT >= 26 && activity.getPackageManager().hasSystemFeature(
                    PackageManager.FEATURE_ACTIVITIES_ON_SECONDARY_DISPLAYS)) {
                moving = true;
                hide();
                BottomScreen.log(activity, "TV move task " + activity.getTaskId() + " from " + current() + " to " + target);
                try {
                    // singleTask + dedicated affinity reuses this Activity/native engine, never SDL onDestroy.
                    Intent i = new Intent(activity, SpongeActivity.class);
                    i.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_SINGLE_TOP);
                    activity.startActivity(i, ActivityOptions.makeBasic().setLaunchDisplayId(target).toBundle());
                } catch (RuntimeException e) {
                    BottomScreen.log(activity, "TV move refused: " + e);
                }
                handler.postDelayed(() -> {
                    moving = false;
                    if (current() != target) {
                        refused = target;
                        BottomScreen.log(activity, "TV task stayed on " + current() + "; fallback for " + target);
                    } else BottomScreen.log(activity, "TV task now on " + current());
                    update();
                }, 1000);
                return;
            }
            refused = target;
            BottomScreen.log(activity, "TV secondary activities unavailable; using Presentation");
        }
        if (tv == null) { hide(); return; }
        if (current() == target) {
            if (mirrorWindow != null) hide();
            if (pad == null) showPad(true);
        } else if (current() == Display.DEFAULT_DISPLAY && mirrorWindow == null) {
            showMirror(tv);
        }
    }

    private FrameLayout newPad(Context c) {
        FrameLayout root = new FrameLayout(c);
        pad = new CompanionView(c);
        pad.setMode(CompanionView.MODE_PAD);
        pad.setModeListener(mode -> pad.setMode(CompanionView.MODE_PAD));
        pad.setSender(Companion::command);
        pad.setPad(Companion::touchpad);
        pad.setFeedback(kind -> pad.performHapticFeedback(android.view.HapticFeedbackConstants.KEYBOARD_TAP));
        root.addView(pad, new FrameLayout.LayoutParams(-1, -1));
        TextView label = new TextView(c);
        label.setText("Playing on TV");
        label.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_YES);
        label.setTextColor(0xffffe6af);
        label.setBackgroundColor(0xdd102e3a);
        label.setGravity(Gravity.CENTER);
        root.addView(label, new FrameLayout.LayoutParams(-1, 40, Gravity.TOP));
        return root;
    }

    private Presentation window(Display d) {
        Presentation p = new Presentation(activity, d);
        p.getWindow().addFlags(WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE
                | WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON | WindowManager.LayoutParams.FLAG_FULLSCREEN);
        p.getWindow().getDecorView().setSystemUiVisibility(5894);
        return p;
    }

    private void showPad(boolean separate) {
        try {
            if (separate) {
                Display top = displays.getDisplay(Display.DEFAULT_DISPLAY);
                if (top == null) return;
                padWindow = window(top);
                padWindow.setContentView(newPad(padWindow.getContext()));
                padWindow.show();
                padWindow.setOnDismissListener(dialog -> {
                    hide();
                    if (!closed) handler.postDelayed(update, 300);
                });
            } else {
                overlay = newPad(activity);
                activity.addContentView(overlay, new ViewGroup.LayoutParams(-1, -1));
            }
            Companion.setEnabled(true);
            handler.post(poll);
        } catch (RuntimeException e) {
            BottomScreen.log(activity, "TV touchpad unavailable: " + e);
            hide();
        }
    }

    private void showMirror(Display tv) {
        try {
            mirrorWindow = window(tv);
            mirror = new ImageView(mirrorWindow.getContext());
            mirror.setBackgroundColor(0xff000000);
            mirror.setScaleType(ImageView.ScaleType.FIT_CENTER);
            mirrorWindow.setContentView(mirror);
            mirrorWindow.show();
            mirrorWindow.setOnDismissListener(dialog -> {
                hide();
                // A resolution change can dismiss a Presentation without removing the display.
                if (!closed) handler.postDelayed(update, 300);
            });
            showPad(false);
            handler.post(capture);
        } catch (RuntimeException e) {
            BottomScreen.log(activity, "TV Presentation unavailable: " + e);
            hide();
        }
    }

    private final Runnable poll = new Runnable() {
        @Override public void run() {
            if (pad == null || !resumed || closed) return;
            Companion.setEnabled(true); // the top pad also needs snapshots when the bottom panel is off
            pad.setSnapshot(Companion.snapshot());
            pad.tick();
            handler.postDelayed(this, 100);
        }
    };

    private SurfaceView surface(View v) {
        if (v instanceof SurfaceView) return (SurfaceView) v;
        if (v instanceof ViewGroup) {
            ViewGroup g = (ViewGroup) v;
            for (int n = 0; n < g.getChildCount(); n++) {
                SurfaceView s = surface(g.getChildAt(n));
                if (s != null) return s;
            }
        }
        return null;
    }

    private final Runnable capture = new Runnable() {
        @Override public void run() {
            if (mirror == null || closed || !resumed || copying) return;
            SurfaceView source = surface(activity.getWindow().getDecorView());
            if (source == null || source.getWidth() < 1 || source.getHeight() < 1
                    || !source.getHolder().getSurface().isValid()) {
                handler.postDelayed(this, 200); return;
            }
            int w = Math.min(1280, source.getWidth());
            int h = Math.max(1, Math.round(w * (float) source.getHeight() / source.getWidth()));
            if (back == null || back.getWidth() != w || back.getHeight() != h)
                back = Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888);
            final Bitmap dest = back;
            final ImageView sink = mirror;
            copying = true;
            try {
                PixelCopy.request(source, dest, result -> {
                    copying = false;
                    if (mirror != sink || closed) {
                        if (mirror != null && !closed) handler.post(capture);
                        return;
                    }
                    if (result == PixelCopy.SUCCESS) {
                        back = front; front = dest;
                        mirror.setImageBitmap(front);
                    } else if (copyError != result) {
                        copyError = result;
                        BottomScreen.log(activity, "TV PixelCopy error " + result);
                    }
                    handler.postDelayed(this, 67); // at most 15 fps; one copy in flight
                }, handler);
            } catch (IllegalArgumentException e) {
                copying = false;
                handler.postDelayed(this, 200);
            }
        }
    };

    private void hide() {
        handler.removeCallbacks(poll);
        handler.removeCallbacks(capture);
        Companion.touchpad(0, 0, 0);
        Companion.setEnabled(resumed && activity.mPanel != null && activity.mPanel.isEnabled());
        if (overlay != null && overlay.getParent() instanceof ViewGroup)
            ((ViewGroup) overlay.getParent()).removeView(overlay);
        overlay = null; pad = null;
        Presentation p = padWindow; padWindow = null;
        if (p != null) { p.setOnDismissListener(null); p.dismiss(); }
        p = mirrorWindow; mirrorWindow = null; mirror = null;
        if (p != null) { p.setOnDismissListener(null); p.dismiss(); }
        front = back = null; // never recycle a bitmap while PixelCopy or the renderer owns it
    }

    @Override public void onDisplayAdded(int id) {
        BottomScreen.log(activity, "TV display added " + id + ": " + displays.getDisplay(id));
        refused = -1; handler.post(update);
    }
    @Override public void onDisplayRemoved(int id) {
        BottomScreen.log(activity, "TV display removed " + id);
        refused = -1; hide(); handler.post(update);
    }
    @Override public void onDisplayChanged(int id) { handler.post(update); }
}
