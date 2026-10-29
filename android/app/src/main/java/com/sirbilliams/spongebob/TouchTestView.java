package com.sirbilliams.spongebob;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Typeface;
import android.view.MotionEvent;
import android.view.View;

import java.util.ArrayList;
import java.util.List;

/**
 * port: one screen of the launcher's "Touch test" (TouchTestActivity): the same view is put on the top and on the bottom screen.
 * Draws every finger on it (a dot with a short trail), a title, and the lines of text the Activity feeds in (the counters of
 * both screens: how many touches the panel window got, how many reached the game window, how many were forwarded). Touches
 * come from the real window or are forwarded by TouchRouter, so what a finger does here is what it would do to the game.
 */
public class TouchTestView extends View {
    private static final class Pt { float x, y; long t; Pt(float x, float y, long t) { this.x = x; this.y = y; this.t = t; } }
    private final List<Pt> trail = new ArrayList<Pt>();
    private final Paint p = new Paint(Paint.ANTI_ALIAS_FLAG);
    private String title = "";
    private String[] lines = new String[0];
    private boolean down;
    private float fx, fy;

    public TouchTestView(Context c) { super(c); }

    public void setInfo(String title, String[] lines) { this.title = title; this.lines = lines; invalidate(); }

    @Override
    public boolean onTouchEvent(MotionEvent e) {
        int a = e.getActionMasked();
        fx = e.getX(); fy = e.getY();
        if (a == MotionEvent.ACTION_DOWN) { down = true; trail.clear(); }
        if (a == MotionEvent.ACTION_UP || a == MotionEvent.ACTION_CANCEL) down = false;
        trail.add(new Pt(fx, fy, e.getEventTime()));
        if (trail.size() > 60) trail.remove(0);
        invalidate();
        return true;
    }

    @Override
    protected void onDraw(Canvas c) {
        int w = getWidth(), h = getHeight();
        p.setStyle(Paint.Style.FILL); p.setColor(0xFF0A3A55);
        c.drawRect(0, 0, w, h, p);
        p.setColor(0x22FFFFFF);
        for (int x = 0; x < w; x += 80) c.drawRect(x, 0, x + 2, h, p);
        for (int y = 0; y < h; y += 80) c.drawRect(0, y, w, y + 2, p);
        p.setTypeface(Typeface.create(Typeface.SANS_SERIF, Typeface.BOLD));
        float s = Math.max(1f, Math.min(w, h) / 1080f);
        p.setTextSize(48 * s); p.setColor(0xFFFFD43B);
        c.drawText(title, 40 * s, 80 * s, p);
        p.setTypeface(Typeface.SANS_SERIF);
        p.setTextSize(30 * s); p.setColor(0xFFFFFFFF);
        float y = 140 * s;
        for (String l : lines) { c.drawText(l, 40 * s, y, p); y += 42 * s; }
        p.setStyle(Paint.Style.STROKE); p.setStrokeWidth(6 * s);
        for (int i = 1; i < trail.size(); i++) {
            p.setColor(0x66FFFFFF);
            c.drawLine(trail.get(i - 1).x, trail.get(i - 1).y, trail.get(i).x, trail.get(i).y, p);
        }
        if (!trail.isEmpty()) {
            p.setStyle(Paint.Style.FILL); p.setColor(down ? 0xCCFFD43B : 0x66FFFFFF);
            c.drawCircle(fx, fy, 46 * s, p);
        }
    }
}
