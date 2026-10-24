package com.sirbilliams.negatron;

import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.RectF;

/**
 * port: the launcher's own illustrations (LauncherView's info window, when the game's picture for
 * an option is not there yet): atomic-age lab motifs drawn in code as glowing phosphor lines on a
 * 640 x 480 transparent canvas - an atom (New Game), a play arrow in a dial (Continue), a floppy
 * disk (Load Game), a floppy and a PDA (Saves), gears and a gauge (Settings), a flask (Test), a
 * rocket (Quit), a magnifier over a wireframe cube (Asset viewer). Original drawings: no characters, logos or art from the game or the show.
 */
final class LauncherArt {
    private LauncherArt() {}

    private static final int GLOW = Color.argb(60, 110, 255, 140);
    private static final int LINE = Color.rgb(120, 255, 150);
    private static final int FILL = Color.argb(60, 110, 255, 140);

    static void draw(Canvas c, String key) {
        Paint p = new Paint(Paint.ANTI_ALIAS_FLAG);
        if ("continue".equals(key)) play(c, p);
        else if ("load".equals(key)) floppy(c, p, 320, 240, 1.15f, true);
        else if ("saves".equals(key)) { floppy(c, p, 225, 250, 0.8f, false); pda(c, p, 440, 240); }
        else if ("settings".equals(key)) { gear(c, p, 255, 255, 110, 12); gear(c, p, 425, 175, 62, 8); gauge(c, p, 450, 345); }
        else if ("test".equals(key)) flask(c, p);
        else if ("quit".equals(key)) rocket(c, p);
        else if ("assets".equals(key)) magnifier(c, p);
        else atom(c, p);
    }

    // a stroke drawn twice: wide and faint (the glow), then thin and bright
    private static void glow(Paint p, boolean wide) {
        p.setShader(null);
        p.setStyle(Paint.Style.STROKE);
        p.setStrokeCap(Paint.Cap.ROUND);
        p.setStrokeWidth(wide ? 16 : 5);
        p.setColor(wide ? GLOW : LINE);
    }

    private static void fill(Paint p) {
        p.setShader(null);
        p.setStyle(Paint.Style.FILL);
        p.setColor(FILL);
    }

    private static void circle(Canvas c, Paint p, float x, float y, float r) {
        for (int k = 0; k < 2; k++) { glow(p, k == 0); c.drawCircle(x, y, r, p); }
    }

    private static void line(Canvas c, Paint p, float x0, float y0, float x1, float y1) {
        for (int k = 0; k < 2; k++) { glow(p, k == 0); c.drawLine(x0, y0, x1, y1, p); }
    }

    private static void rect(Canvas c, Paint p, float l, float t, float r, float b, float rad, boolean filled) {
        RectF o = new RectF(l, t, r, b);
        if (filled) { fill(p); c.drawRoundRect(o, rad, rad, p); }
        for (int k = 0; k < 2; k++) { glow(p, k == 0); c.drawRoundRect(o, rad, rad, p); }
    }

    private static void path(Canvas c, Paint p, Path path, boolean filled) {
        if (filled) { fill(p); c.drawPath(path, p); }
        for (int k = 0; k < 2; k++) { glow(p, k == 0); c.drawPath(path, p); }
    }

    private static void dot(Canvas c, Paint p, float x, float y, float r) {
        fill(p);
        p.setColor(Color.argb(90, 110, 255, 140));
        c.drawCircle(x, y, r * 2, p);
        p.setColor(Color.rgb(210, 255, 220));
        c.drawCircle(x, y, r, p);
    }

    private static void atom(Canvas c, Paint p) {
        float x = 320, y = 240, r = 190;
        for (int i = 0; i < 3; i++) {
            c.save();
            c.rotate(i * 60, x, y);
            RectF o = new RectF(x - r, y - r * 0.36f, x + r, y + r * 0.36f);
            for (int k = 0; k < 2; k++) { glow(p, k == 0); c.drawOval(o, p); }
            dot(c, p, x + (float) Math.cos(1.1 + i * 2.1) * r, y + (float) Math.sin(1.1 + i * 2.1) * r * 0.36f, 10);
            c.restore();
        }
        dot(c, p, x, y, 26);
    }

    private static void play(Canvas c, Paint p) {
        float x = 320, y = 240, r = 175;
        circle(c, p, x, y, r);
        for (int i = 0; i < 24; i++) {                              // a dial's ticks
            double a = Math.toRadians(i * 15);
            float r0 = r + 18, r1 = r + (i % 6 == 0 ? 44 : 30);
            line(c, p, x + (float) Math.cos(a) * r0, y + (float) Math.sin(a) * r0,
                    x + (float) Math.cos(a) * r1, y + (float) Math.sin(a) * r1);
        }
        Path t = new Path();
        t.moveTo(x - 55, y - 85);
        t.lineTo(x + 95, y);
        t.lineTo(x - 55, y + 85);
        t.close();
        path(c, p, t, true);
    }

    /** A floppy disk centred at (x, y); with an arrow coming out of it (Load). */
    private static void floppy(Canvas c, Paint p, float x, float y, float s, boolean arrow) {
        float h = 150 * s;
        Path d = new Path();                                        // the case, one corner cut
        d.moveTo(x - h, y - h);
        d.lineTo(x + h - 30 * s, y - h);
        d.lineTo(x + h, y - h + 30 * s);
        d.lineTo(x + h, y + h);
        d.lineTo(x - h, y + h);
        d.close();
        path(c, p, d, true);
        rect(c, p, x - h * 0.55f, y - h, x + h * 0.45f, y - h * 0.38f, 6, false);   // the shutter
        rect(c, p, x + h * 0.12f, y - h * 0.9f, x + h * 0.3f, y - h * 0.5f, 3, false);
        rect(c, p, x - h * 0.7f, y + h * 0.05f, x + h * 0.7f, y + h * 0.88f, 10, false); // the label
        for (int i = 0; i < 3; i++) line(c, p, x - h * 0.5f, y + h * (0.3f + i * 0.2f), x + h * 0.5f, y + h * (0.3f + i * 0.2f));
        if (arrow) {
            float ax = x + h + 70, top = y - 70, bot = y + 70;
            line(c, p, ax, bot, ax, top);
            line(c, p, ax, top, ax - 32, top + 36);
            line(c, p, ax, top, ax + 32, top + 36);
        }
    }

    private static void pda(Canvas c, Paint p, float x, float y) {
        rect(c, p, x - 85, y - 150, x + 85, y + 150, 26, true);
        rect(c, p, x - 60, y - 115, x + 60, y + 25, 8, false);      // the screen
        for (int i = 0; i < 4; i++) line(c, p, x - 42, y - 85 + i * 28, x + (i % 2 == 0 ? 40 : 15), y - 85 + i * 28);
        for (int i = 0; i < 3; i++) circle(c, p, x - 45 + i * 45, y + 80, 15);
        line(c, p, x + 55, y - 150, x + 70, y - 205);               // the antenna
        dot(c, p, x + 70, y - 210, 7);
    }

    private static void gear(Canvas c, Paint p, float x, float y, float r, int teeth) {
        for (int i = 0; i < teeth; i++) {
            c.save();
            c.rotate(i * 360f / teeth, x, y);
            rect(c, p, x - r * 0.16f, y - r - r * 0.28f, x + r * 0.16f, y - r + 4, 4, true);
            c.restore();
        }
        circle(c, p, x, y, r);
        circle(c, p, x, y, r * 0.32f);
        dot(c, p, x, y, 6);
    }

    private static void gauge(Canvas c, Paint p, float x, float y) {
        RectF o = new RectF(x - 95, y - 95, x + 95, y + 95);
        for (int k = 0; k < 2; k++) { glow(p, k == 0); c.drawArc(o, 200, 140, false, p); }
        for (int i = 0; i <= 6; i++) {
            double a = Math.toRadians(200 + i * 140 / 6.0);
            line(c, p, x + (float) Math.cos(a) * 95, y + (float) Math.sin(a) * 95,
                    x + (float) Math.cos(a) * 75, y + (float) Math.sin(a) * 75);
        }
        double n = Math.toRadians(300);
        line(c, p, x, y, x + (float) Math.cos(n) * 80, y + (float) Math.sin(n) * 80);
        dot(c, p, x, y, 8);
    }

    private static void flask(Canvas c, Paint p) {
        float x = 320;
        Path f = new Path();
        f.moveTo(x - 38, 95);
        f.lineTo(x - 38, 210);
        f.lineTo(x - 150, 410);
        f.lineTo(x + 150, 410);
        f.lineTo(x + 38, 210);
        f.lineTo(x + 38, 95);
        path(c, p, f, false);
        line(c, p, x - 60, 95, x + 60, 95);                         // the lip
        Path liquid = new Path();                                   // the liquid, two thirds up
        liquid.moveTo(x - 100, 320);
        liquid.lineTo(x + 100, 320);
        liquid.lineTo(x + 140, 400);
        liquid.lineTo(x - 140, 400);
        liquid.close();
        path(c, p, liquid, true);
        float[][] bub = { { -30, 280, 14 }, { 20, 240, 10 }, { -8, 190, 8 }, { 22, 140, 12 }, { -18, 60, 9 }, { 40, 35, 6 } };
        for (float[] b : bub) circle(c, p, x + b[0], b[1], b[2]);
    }

    /** Asset viewer: a wireframe cube (front and back faces, dashed hidden edges) under a magnifier. */
    private static void magnifier(Canvas c, Paint p) {
        c.save();
        c.translate(320, 240);
        c.scale(1.3f, 1.3f);
        c.translate(-330, -240);
        float x = 255, y = 265, s = 105, dx = 70, dy = -60;          // the front face's centre, half size, depth
        float[][] f = { { x - s, y - s }, { x + s, y - s }, { x + s, y + s }, { x - s, y + s } };
        for (int i = 0; i < 4; i++) {
            float[] a = f[i], b = f[(i + 1) % 4];
            line(c, p, a[0], a[1], b[0], b[1]);                     // the front face
            boolean hidden = i == 2 || i == 3;                      // the back face's lower / left edges
            if (!hidden) line(c, p, a[0] + dx, a[1] + dy, b[0] + dx, b[1] + dy);
            if (i != 3) line(c, p, a[0], a[1], a[0] + dx, a[1] + dy);   // the depth edges
        }
        float hx = f[3][0] + dx, hy = f[3][1] + dy;                  // the hidden corner, dotted
        for (int k = 0; k < 3; k++) {
            float[] to = k == 0 ? f[3] : k == 1 ? new float[] { f[2][0] + dx, f[2][1] + dy } : new float[] { f[0][0] + dx, f[0][1] + dy };
            float ex = to[0] - hx, ey = to[1] - hy;
            for (int d = 0; d < 6; d++) line(c, p, hx + ex * d / 6f, hy + ey * d / 6f, hx + ex * (d + 0.45f) / 6f, hy + ey * (d + 0.45f) / 6f);
        }
        for (float[] a : f) dot(c, p, a[0], a[1], 6);
        float mx = 390, my = 205, r = 92;                           // the magnifier: lens, rim, handle
        fill(p);
        c.drawCircle(mx, my, r, p);
        circle(c, p, mx, my, r);
        circle(c, p, mx, my, r + 14);
        float hx0 = mx + (r + 14) * 0.7071f, hy0 = my + (r + 14) * 0.7071f;
        Path h = new Path();
        h.moveTo(hx0 - 14, hy0 + 8);
        h.lineTo(hx0 + 8, hy0 - 14);
        h.lineTo(hx0 + 118, hy0 + 96);
        h.lineTo(hx0 + 96, hy0 + 118);
        h.close();
        path(c, p, h, true);
        line(c, p, mx - 50, my - 30, mx - 22, my - 58);              // a glint on the glass
        c.restore();
    }

    private static void rocket(Canvas c, Paint p) {
        c.save();
        c.rotate(35, 320, 240);
        float x = 320;
        Path body = new Path();
        body.moveTo(x, 40);                                         // the nose
        body.lineTo(x + 70, 150);
        body.lineTo(x + 70, 330);
        body.lineTo(x - 70, 330);
        body.lineTo(x - 70, 150);
        body.close();
        path(c, p, body, true);
        Path fin = new Path();
        fin.moveTo(x - 70, 250);
        fin.lineTo(x - 135, 360);
        fin.lineTo(x - 70, 330);
        path(c, p, fin, false);
        Path fin2 = new Path();
        fin2.moveTo(x + 70, 250);
        fin2.lineTo(x + 135, 360);
        fin2.lineTo(x + 70, 330);
        path(c, p, fin2, false);
        circle(c, p, x, 190, 34);                                   // the window
        dot(c, p, x, 190, 8);
        line(c, p, x - 70, 150, x + 70, 150);
        for (int i = -1; i <= 1; i++) line(c, p, x + i * 35, 345, x + i * 50, 420 - Math.abs(i) * 25);  // the flame
        c.restore();
    }
}
