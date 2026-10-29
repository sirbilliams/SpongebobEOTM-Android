package com.sirbilliams.spongebob;

import android.app.Activity;
import android.app.Presentation;
import android.content.Context;
import android.content.SharedPreferences;
import android.hardware.display.DisplayManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.Display;
import android.view.HapticFeedbackConstants;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.SoundEffectConstants;
import android.view.View;
import android.view.WindowManager;

/**
 * port: the bottom-screen companion panel (AYN Thor: its second display; docs/companion.md). Shows a
 * {@link CompanionView} in an android.app.Presentation on the Thor's built-in bottom screen
 * ({@link BottomScreen}: never a TV / cast / HDMI display, so mirroring shows the game), polling the native snapshot 10 times a second. On by default when a second
 * display is present (off otherwise); a choice the player makes (the port menu, the launcher's
 * settings) is remembered and wins. With no second display it simply shows nothing. The panel
 * never takes controller focus. Bottom-screen touches that land in the game window instead are forwarded here ({@link TouchRouter}); everything is logged for the diagnosis ({@link TouchDiag}). Its touch targets (inventory items, dialogue choices, menu, skip) are sent
 * to the game thread as commands (Companion.command); controller input stays with the game.
 */
final class CompanionPanel implements DisplayManager.DisplayListener {
    static final String PREFS = "spongebob";
    static final String KEY_ON = "bottom_panel";
    /** int: CompanionView.MODE_PANEL (0, default) or MODE_PAD (1): what the bottom screen shows (launcher settings, or the panel's own switch). */
    static final String KEY_MODE = "bottom_mode";
    /** boolean (default false): the panel window may take input focus (the variant that forwards keys back; docs/bottom-touch.md). */
    static final String KEY_FOCUSABLE = "panel_focusable";

    private final Activity mActivity;
    private final DisplayManager mDisplays;
    private final SharedPreferences mPrefs;
    private final Handler mHandler = new Handler(Looper.getMainLooper());
    private PanelPresentation mPresentation;
    private boolean mResumed;
    private final TouchRouter mRouter;

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
        mPrefs = activity.getSharedPreferences(PREFS, Context.MODE_PRIVATE | Context.MODE_MULTI_PROCESS);   // the launcher (another process) writes the mode
        TouchDiag.init(activity);
        mRouter = new TouchRouter(activity);
    }

    /**
     * Every touch the game Activity gets (SpongeActivity.dispatchTouchEvent): logged for the diagnosis, and a touch of the bottom
     * touchscreen that landed here is forwarded to the panel and consumed (true) instead of clicking the game.
     */
    boolean routeTouch(MotionEvent e) {
        TouchDiag.event(TouchDiag.ACTIVITY, e, "game-window");
        PanelPresentation p = mPresentation;
        if (p == null || p.view == null || p.view.getWidth() <= 0) return false;
        Display d = p.getDisplay();
        if (!mRouter.isBottom(e, d.getDisplayId(), p.view.getWidth(), p.view.getHeight())) return false;
        MotionEvent c = TouchRouter.mapped(e, mActivity, p.view, d.getDisplayId());
        TouchDiag.event(TouchDiag.FORWARDED, c, "forwarded-to-panel");
        p.view.dispatchTouchEvent(c);
        c.recycle();
        return true;
    }

    /** Whether the panel is wanted: the player's remembered choice, else on when a second display is present. */
    boolean isEnabled() { return wanted(mPrefs, hasSecondDisplay()); }

    static boolean wanted(SharedPreferences prefs, boolean secondDisplay) {
        return prefs.contains(KEY_ON) ? prefs.getBoolean(KEY_ON, false) : secondDisplay;
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
        final PanelPresentation p = new PanelPresentation(mActivity, d);
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
        mRouter.reset();
        // the panel's size is known after layout: the dump waits for it (the first poll tick)
        final Display shown = d;
        mHandler.postDelayed(new Runnable() {
            @Override public void run() {
                if (mPresentation != p || p.view == null) return;
                TouchDiag.dumpSession(mActivity, shown.getDisplayId(), p.view.getWidth(), p.view.getHeight(),
                        mActivity.getWindowManager().getDefaultDisplay().getDisplayId(),
                        "panel opened, window " + (mPrefs.getBoolean(KEY_FOCUSABLE, false) ? "focusable" : "not focusable")
                        + ", mode " + mPrefs.getInt(KEY_MODE, 0));
            }
        }, 800);
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

    /**
     * The Presentation window: one full-size CompanionView. By default never focused (FLAG_NOT_FOCUSABLE: gamepad keys and the
     * game's input focus stay with the game Activity, and SDL does not pause: it pauses when its window loses focus); touch goes to the
     * panel's targets. With KEY_FOCUSABLE the window may take focus (touch-modal off) and forwards every key back to the game.
     */
    final class PanelPresentation extends Presentation {
        CompanionView view;
        PanelPresentation(Context outer, Display display) {
            super(outer, display);
        }

        @Override
        protected void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);
            int flags = WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON | WindowManager.LayoutParams.FLAG_SPLIT_TOUCH;
            if (!mPrefs.getBoolean(KEY_FOCUSABLE, false)) flags |= WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE;
            else flags |= WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL;
            getWindow().addFlags(flags);
            view = new CompanionView(getContext());
            view.setSender(new CompanionView.Sender() {
                @Override public void send(String cmd) { Companion.command(cmd); }
            });
            view.setPad(new CompanionView.Pad() {
                @Override public void move(float dx, float dy, int button) { Companion.touchpad(dx, dy, button); }
            });
            view.setFeedback(new CompanionView.Feedback() {
                @Override public void tap(int kind) { feedback(view, kind); }
            });
            view.setModeListener(new CompanionView.ModeListener() {
                @Override public void modeChanged(int mode) { mPrefs.edit().putInt(KEY_MODE, mode).apply(); }
            });
            view.setMode(mPrefs.getInt(KEY_MODE, CompanionView.MODE_PANEL));
            setContentView(view);
        }

        @Override
        public boolean dispatchTouchEvent(MotionEvent e) {
            TouchDiag.event(TouchDiag.PANEL, e, "panel-window");
            mRouter.learn(e);                                    // this device delivers to the panel: it is the bottom touchscreen
            return super.dispatchTouchEvent(e);
        }

        @Override
        public boolean dispatchKeyEvent(KeyEvent e) {
            if (mPrefs.getBoolean(KEY_FOCUSABLE, false)) return mActivity.dispatchKeyEvent(e);   // gamepad / Back keys belong to the game
            return super.dispatchKeyEvent(e);
        }
    }

    /** Press feedback: a short haptic tick (kind 1 = soft tap, 2 = firm: a drag starts / a card is taken) and the system click. */
    private void feedback(View v, int kind) {
        try {
            v.performHapticFeedback(kind == 2 ? HapticFeedbackConstants.LONG_PRESS : HapticFeedbackConstants.VIRTUAL_KEY,
                    HapticFeedbackConstants.FLAG_IGNORE_GLOBAL_SETTING);
            v.playSoundEffect(SoundEffectConstants.CLICK);
            android.os.Vibrator vib = (android.os.Vibrator) mActivity.getSystemService(Context.VIBRATOR_SERVICE);
            if (vib != null && vib.hasVibrator()) vib.vibrate(kind == 2 ? 24 : 10);
        } catch (RuntimeException e) {
            // no feedback then
        }
    }
}
