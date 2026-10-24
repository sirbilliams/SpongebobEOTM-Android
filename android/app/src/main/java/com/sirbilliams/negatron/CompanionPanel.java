package com.sirbilliams.negatron;

import android.app.Activity;
import android.app.Presentation;
import android.content.Context;
import android.content.SharedPreferences;
import android.hardware.display.DisplayManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.Display;
import android.view.WindowManager;

/**
 * port: the bottom-screen companion panel (AYN Thor: its second display). Shows a
 * {@link CompanionView} in an android.app.Presentation on the Thor's built-in bottom screen
 * ({@link BottomScreen}: never a TV / cast / HDMI display, so mirroring shows the game), polling the native snapshot 10 times a second. On by default when a second
 * display is present (off otherwise); a choice the player makes (the port menu, the launcher's
 * settings) is remembered and wins. With no second display it simply shows nothing. The panel
 * never takes focus or touches, so the game keeps the input.
 */
final class CompanionPanel implements DisplayManager.DisplayListener {
    static final String PREFS = "negatron";
    static final String KEY_ON = "bottom_panel";
    static final String KEY_FAR = "bottom_panel_zoom_far";

    private final Activity mActivity;
    private final DisplayManager mDisplays;
    private final SharedPreferences mPrefs;
    private final Handler mHandler = new Handler(Looper.getMainLooper());
    private PanelPresentation mPresentation;
    private boolean mResumed;
    private boolean mFar;

    private final Runnable mPoll = new Runnable() {
        @Override
        public void run() {
            if (mPresentation == null) return;
            if (mPresentation.view != null) {
                mPresentation.view.setSnapshot(Companion.snapshot());
                mPresentation.view.tick();
            }
            mHandler.postDelayed(this, 100);
        }
    };

    CompanionPanel(Activity activity) {
        mActivity = activity;
        mDisplays = (DisplayManager) activity.getSystemService(Context.DISPLAY_SERVICE);
        mPrefs = activity.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
        mFar = mPrefs.getBoolean(KEY_FAR, false);
    }

    /** Whether the panel is wanted: the player's remembered choice, else on when a second display is present. */
    boolean isEnabled() { return wanted(mPrefs, hasSecondDisplay()); }

    static boolean wanted(SharedPreferences prefs, boolean secondDisplay) {
        return prefs.contains(KEY_ON) ? prefs.getBoolean(KEY_ON, false) : secondDisplay;
    }

    /** The map's zoom: near (false) or far (true), remembered. */
    boolean isZoomFar() { return mFar; }

    void setZoomFar(boolean far) {
        mFar = far;
        mPrefs.edit().putBoolean(KEY_FAR, far).apply();
        if (mPresentation != null && mPresentation.view != null) mPresentation.view.setZoomFar(far);
    }

    boolean hasSecondDisplay() { return findDisplay(mActivity, mDisplays) != null; }

    void setEnabled(boolean on) {
        mPrefs.edit().putBoolean(KEY_ON, on).apply();
        update(null);
    }

    void resume() {
        mResumed = true;
        if (mDisplays != null) mDisplays.registerDisplayListener(this, mHandler);
        update("resume");
    }

    void pause() {
        mResumed = false;
        if (mDisplays != null) mDisplays.unregisterDisplayListener(this);
        update(null);
    }

    /** The second screen: the Thor's built-in bottom screen (never an external / cast display), or null. */
    static Display findDisplay(Context context, DisplayManager displays) {
        return BottomScreen.find(context, displays);
    }

    /** why: the log line's reason (BottomScreen), null for none. */
    private void update(String why) {
        Display d = mResumed ? BottomScreen.find(mActivity, mDisplays, why) : null;
        if (d != null && !wanted(mPrefs, true)) d = null;
        if (mPresentation != null
                && (d == null || mPresentation.getDisplay().getDisplayId() != d.getDisplayId())) {
            hide();
        }
        if (d != null && mPresentation == null) show(d);
    }

    private void show(Display d) {
        final PanelPresentation p = new PanelPresentation(mActivity, d, mFar);
        // Presentation dismisses itself when its display goes away or changes size: forget it, look again
        p.setOnDismissListener(new android.content.DialogInterface.OnDismissListener() {
            @Override
            public void onDismiss(android.content.DialogInterface dialog) {
                if (mPresentation != p) return;
                mHandler.removeCallbacks(mPoll);
                Companion.setEnabled(false);
                mPresentation = null;
                mHandler.post(new Runnable() {
                    @Override public void run() { update("panel closed"); }
                });
            }
        });
        try {
            p.show();
        } catch (WindowManager.InvalidDisplayException | IllegalStateException e) {
            return;
        }
        mPresentation = p;
        Companion.setEnabled(true);
        mHandler.removeCallbacks(mPoll);
        mHandler.post(mPoll);
    }

    private void hide() {
        mHandler.removeCallbacks(mPoll);
        Companion.setEnabled(false);
        PanelPresentation p = mPresentation;
        mPresentation = null;                           // before dismiss: the dismiss listener ignores it
        try {
            p.dismiss();
        } catch (RuntimeException e) {
            // the display went away
        }
    }

    // A TV / cast display coming or going never moves the panel: find() keeps choosing the built-in screen.
    @Override public void onDisplayAdded(int displayId) { update("display " + displayId + " added"); }
    @Override public void onDisplayRemoved(int displayId) { update("display " + displayId + " removed"); }
    @Override public void onDisplayChanged(int displayId) { update(null); }

    /** The Presentation window: one full-size CompanionView, never focused or touched. */
    static final class PanelPresentation extends Presentation {
        CompanionView view;
        private final boolean mFar;

        PanelPresentation(Context outer, Display display, boolean far) {
            super(outer, display);
            mFar = far;
        }

        @Override
        protected void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);
            getWindow().addFlags(WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE
                    | WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE
                    | WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
            view = new CompanionView(getContext());
            view.setZoomFar(mFar);
            view.setSource(new CompanionView.Source() {
                @Override public int[] map() { return Companion.map(); }
                @Override public int[] icon(int id) { return Companion.icon(id); }
            });
            setContentView(view);
        }
    }
}
