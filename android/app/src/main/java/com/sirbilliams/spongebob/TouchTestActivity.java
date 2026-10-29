package com.sirbilliams.spongebob;

import android.app.Activity;
import android.app.Presentation;
import android.content.Context;
import android.hardware.display.DisplayManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.Display;
import android.view.MotionEvent;
import android.view.WindowManager;

/**
 * port: the launcher's "Touch test" (Engine room). Opens a test screen on the top display (this Activity) and on the bottom one (a
 * Presentation, made exactly as the game's panel: same flags, same routing of bottom-screen touches that land in this window).
 * Both show draw-where-you-touch plus the counters, and everything is written to touch-log.txt like in the game. Touch each screen
 * in turn: a count on the other screen's line means the touchscreens are crossed; "forwarded" means the router had to rescue them.
 * Back to leave.
 */
public class TouchTestActivity extends Activity {
    private final Handler mHandler = new Handler(Looper.getMainLooper());
    private TouchTestView mTop;
    private TestPresentation mBottom;
    private TouchRouter mRouter;
    private int mBottomId = -1;
    private boolean mFocusable;

    private final Runnable mTick = new Runnable() {
        @Override public void run() {
            String[] lines = {
                "Touches that reached the bottom window:   " + TouchDiag.count(TouchDiag.PANEL),
                "Touches that reached the top (game) window: " + TouchDiag.count(TouchDiag.ACTIVITY),
                "  ... of those forwarded to the bottom:  " + TouchDiag.count(TouchDiag.FORWARDED),
                "Last device: " + TouchDiag.lastDevice(),
                mBottom == null ? "No bottom screen found." : "Bottom display id " + mBottomId + (mFocusable ? ", window focusable" : ", window not focusable"),
                "Touch each screen. Back = leave. Share log has the details."
            };
            if (mTop != null) mTop.setInfo("TOUCH TEST - top screen", lines);
            if (mBottom != null && mBottom.view != null) mBottom.view.setInfo("TOUCH TEST - bottom screen", lines);
            mHandler.postDelayed(this, 200);
        }
    };

    @Override
    protected void onCreate(Bundle b) {
        super.onCreate(b);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        TouchDiag.init(this);
        mRouter = new TouchRouter(this);
        mTop = new TouchTestView(this);
        setContentView(mTop);
    }

    @Override
    protected void onResume() {
        super.onResume();
        DisplayManager dm = (DisplayManager) getSystemService(Context.DISPLAY_SERVICE);
        Display d = BottomScreen.find(this, dm, "touch test");
        mFocusable = getSharedPreferences(CompanionPanel.PREFS, MODE_PRIVATE).getBoolean(CompanionPanel.KEY_FOCUSABLE, false);
        if (d != null && mBottom == null) {
            mBottom = new TestPresentation(this, d);
            try {
                mBottom.show();
                mBottomId = d.getDisplayId();
            } catch (WindowManager.InvalidDisplayException | IllegalStateException e) {
                mBottom = null;
            }
        }
        mHandler.postDelayed(new Runnable() {
            @Override public void run() {
                if (mBottom != null && mBottom.view != null)
                    TouchDiag.dumpSession(TouchTestActivity.this, mBottomId, mBottom.view.getWidth(), mBottom.view.getHeight(),
                            getWindowManager().getDefaultDisplay().getDisplayId(), "touch test");
                else TouchDiag.dumpSession(TouchTestActivity.this, -1, 0, 0, getWindowManager().getDefaultDisplay().getDisplayId(), "touch test, no bottom screen");
            }
        }, 800);
        mHandler.post(mTick);
    }

    @Override
    protected void onPause() {
        mHandler.removeCallbacksAndMessages(null);
        if (mBottom != null) { try { mBottom.dismiss(); } catch (RuntimeException e) { /* gone */ } mBottom = null; }
        super.onPause();
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent e) {
        TouchDiag.event(TouchDiag.ACTIVITY, e, "top-window");
        if (mBottom != null && mBottom.view != null && mBottom.view.getWidth() > 0
                && mRouter.isBottom(e, mBottomId, mBottom.view.getWidth(), mBottom.view.getHeight())) {
            MotionEvent c = TouchRouter.mapped(e, this, mBottom.view, mBottomId);
            TouchDiag.event(TouchDiag.FORWARDED, c, "forwarded-to-bottom");
            mBottom.view.dispatchTouchEvent(c);
            c.recycle();
            return true;
        }
        return super.dispatchTouchEvent(e);
    }

    private final class TestPresentation extends Presentation {
        TouchTestView view;
        TestPresentation(Context outer, Display display) { super(outer, display); }

        @Override
        protected void onCreate(Bundle b) {
            super.onCreate(b);
            int flags = WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON | WindowManager.LayoutParams.FLAG_SPLIT_TOUCH;
            getWindow().addFlags(mFocusable ? flags | WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL : flags | WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE);
            view = new TouchTestView(getContext());
            setContentView(view);
        }

        @Override
        public boolean dispatchTouchEvent(MotionEvent e) {
            TouchDiag.event(TouchDiag.PANEL, e, "bottom-window");
            mRouter.learn(e);
            return super.dispatchTouchEvent(e);
        }

        @Override
        public boolean dispatchKeyEvent(android.view.KeyEvent e) {
            if (mFocusable) return TouchTestActivity.this.dispatchKeyEvent(e);
            return super.dispatchKeyEvent(e);
        }
    }
}
