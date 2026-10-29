package com.sirbilliams.spongebob;

import android.app.Activity;
import android.app.Presentation;
import android.content.Context;
import android.hardware.display.DisplayManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.Display;
import android.view.WindowManager;

/**
 * port: the launcher's calm bottom-screen scene (docs/launcher.md): a LauncherView in bottom mode in a Presentation on the
 * Thor's built-in bottom screen (BottomScreen.find, never a TV / cast display), shown while SetupActivity is resumed and the
 * "Bottom-screen panel" setting is on. The window never takes focus, so the pad keeps driving the top-screen launcher.
 */
final class LauncherBottom implements DisplayManager.DisplayListener {
    interface Fill { void fill(LauncherView view); }

    private final Activity activity;
    private final LauncherView.Actions dials;
    private final Fill fill;
    private final DisplayManager displays;
    private final Handler handler = new Handler(Looper.getMainLooper());
    private Screen screen;
    private boolean resumed, wanted;

    LauncherBottom(Activity activity, LauncherView.Actions dials, Fill fill) {
        this.activity = activity; this.dials = dials; this.fill = fill;
        displays = (DisplayManager) activity.getSystemService(Context.DISPLAY_SERVICE);
    }

    void resume(boolean wanted) {
        resumed = true; this.wanted = wanted;
        if (displays != null) displays.registerDisplayListener(this, handler);
        update();
    }
    void pause() {
        resumed = false;
        if (displays != null) displays.unregisterDisplayListener(this);
        update();
    }
    /** Re-reads the save card / dials into the bottom view. */
    void refresh() { if (screen != null && screen.view != null) fill.fill(screen.view); }

    private void update() {
        Display d = resumed && wanted ? BottomScreen.find(activity, displays) : null;
        if (screen != null && (d == null || screen.getDisplay().getDisplayId() != d.getDisplayId())) hide();
        if (d != null && screen == null) show(d);
    }

    private void show(Display d) {
        final Screen s = new Screen(activity, d);
        s.setOnDismissListener(new android.content.DialogInterface.OnDismissListener() {
            @Override public void onDismiss(android.content.DialogInterface dialog) {
                if (screen != s) return;
                screen = null;
                handler.post(new Runnable() { @Override public void run() { update(); } });
            }
        });
        try {
            s.show();
        } catch (WindowManager.InvalidDisplayException | IllegalStateException e) {
            return;
        }
        screen = s;
    }

    private void hide() {
        Screen s = screen;
        screen = null;
        try { s.dismiss(); } catch (RuntimeException e) { /* the display went away */ }
    }

    @Override public void onDisplayAdded(int id) { update(); }
    @Override public void onDisplayRemoved(int id) { update(); }
    @Override public void onDisplayChanged(int id) { update(); }

    private final class Screen extends Presentation {
        LauncherView view;
        Screen(Context outer, Display display) { super(outer, display); }
        @Override protected void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);
            getWindow().addFlags(WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE | WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
            view = new LauncherView(getContext(), dials, true);
            fill.fill(view);
            setContentView(view);
        }
    }
}
