package com.sirbilliams.spongebob;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.LinearGradient;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.RectF;
import android.graphics.Shader;
import android.graphics.Typeface;
import android.os.SystemClock;
import android.view.MotionEvent;
import android.view.View;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.File;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * port: the bottom-screen companion panel of the dual-screen mode (docs/companion.md). Everything is drawn
 * in code (no show art): a Bikini Bottom / Krusty Krab look, with a wood-plank header, rope, bubbles and kelp.
 * Shows the chapter and scene, the inventory as large touch cards (tap = take the item in hand, as clicking
 * it in the game's bar), the open dialogue's choices as big buttons (tap = choose), a hint line, menu / skip
 * buttons and the trail of scenes visited. Touches go out through {@link Sender} as short commands.
 * Laid out in a 1240 x 1080 space (the Thor's bottom screen) and scaled to fit.
 */
public class CompanionView extends View {

    /** Where a tap goes: Companion.command on the device. */
    public interface Sender { void send(String cmd); }

    private static final float W = 1240f, H = 1080f;

    // Bikini Bottom palette
    private static final int SEA_TOP = 0xFF1B8FB4, SEA_BOT = 0xFF0A4E70, SAND = 0xFFE3C98B, SAND_DK = 0xFFC9A862;
    private static final int WOOD = 0xFF9C6A3A, WOOD_LT = 0xFFB98550, WOOD_DK = 0xFF6B4423, WOOD_EDGE = 0xFF3F2814;
    private static final int ROPE = 0xFFD8C08A, ROPE_DK = 0xFF9B8350;
    private static final int PAPER = 0xFFFFF4D2, PAPER_EDGE = 0xFFD9C48A, INK = 0xFF3A2A18;
    private static final int GOLD = 0xFFFFD43B, GOLD_DK = 0xFFE0A800, KELP = 0xFF2E9A4E, KELP_DK = 0xFF1F7038;
    private static final int RED = 0xFFD1493B, TEAL = 0xFF28B7B0;

    private final Paint p = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint picPaint = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.FILTER_BITMAP_FLAG);
    private final RectF r = new RectF();
    private final Path path = new Path();
    private Sender sender;

    // ---- snapshot ----
    private boolean haveGame;
    private String sceneTitle = "", chapterTitle = "", hint = "", caption = "", selected = "";
    private String speaker = "", line = "";
    private boolean dialogue, choiceMode, cutscene;
    private final List<String> items = new ArrayList<String>();
    private final List<String> itemNames = new ArrayList<String>();     // display names, parallel to items ("names" in the snapshot)
    private String iconDir = "";                                        // folder of <id>.png pictures the game renders from the player's own files
    private final Map<String, Bitmap> pics = new HashMap<String, Bitmap>();
    private final Map<String, Long> picMiss = new HashMap<String, Long>();   // when a missing picture was last looked for
    private final List<String> choices = new ArrayList<String>();
    private final List<String> visited = new ArrayList<String>();
    private int invPage;

    // ---- touch ----
    private static final class Hit {
        final RectF box = new RectF(); String cmd;
        Hit(float l, float t, float rr, float b, String c) { box.set(l, t, rr, b); cmd = c; }
    }
    private final List<Hit> hits = new ArrayList<Hit>();
    private Hit down;
    private float scale = 1f, offX, offY;

    public CompanionView(Context c) {
        super(c);
        setContentDescription("SpongeBob companion panel");
    }

    public void setSender(Sender s) { sender = s; }

    /** Parses a snapshot (null / empty: the game is not running yet) and redraws. */
    public void setSnapshot(String json) {
        haveGame = false; dialogue = choiceMode = cutscene = false;
        items.clear(); itemNames.clear(); choices.clear(); visited.clear(); iconDir = "";
        sceneTitle = chapterTitle = hint = caption = selected = speaker = line = "";
        if (json != null && json.length() > 2) {
            try {
                JSONObject o = new JSONObject(json);
                haveGame = o.optInt("game", 0) != 0;
                chapterTitle = o.optString("chapter_title", "");
                sceneTitle = o.optString("scene_title", "");
                hint = o.optString("hint", "");
                caption = o.optString("caption", "");
                selected = o.optString("sel", "");
                cutscene = o.optInt("cut", 0) != 0;
                strings(o.optJSONArray("items"), items);
                strings(o.optJSONArray("names"), itemNames);
                iconDir = o.optString("icons", "");
                strings(o.optJSONArray("visited"), visited);
                JSONObject d = o.optJSONObject("dlg");
                if (d != null) {
                    dialogue = true;
                    choiceMode = d.optInt("choice", 0) != 0;
                    speaker = d.optString("speaker", "");
                    line = d.optString("text", "");
                    strings(d.optJSONArray("choices"), choices);
                }
            } catch (org.json.JSONException | RuntimeException e) {   // Android's org.json throws the checked JSONException
                haveGame = false;
            }
        }
        invalidate();
    }

    /** The item's picture (a small PNG the game wrote), or null (not there yet, or not readable): then the initials badge is drawn. */
    private Bitmap picture(String id) {
        if (iconDir.length() == 0 || id == null || id.length() == 0) return null;
        Bitmap b = pics.get(id);
        if (b != null) return b;
        long now = SystemClock.uptimeMillis();
        Long last = picMiss.get(id);
        if (last != null && now - last < 1500) return null;           // the game renders it a moment after the item appears
        File f = new File(iconDir, id.toLowerCase() + ".png");
        if (f.isFile()) {
            try { b = BitmapFactory.decodeFile(f.getPath()); } catch (RuntimeException e) { b = null; }
        }
        if (b == null) { picMiss.put(id, now); return null; }
        pics.put(id, b);
        return b;
    }

    private String itemName(int idx, String id) {
        if (idx >= 0 && idx < itemNames.size() && itemNames.get(idx).length() > 0) return itemNames.get(idx);
        return prettyName(id);
    }

    private static void strings(JSONArray a, List<String> out) {
        if (a == null) return;
        for (int i = 0; i < a.length(); i++) out.add(a.optString(i, ""));
    }

    /** Called by the poller: keeps the bubbles and kelp moving. */
    public void tick() { invalidate(); }

    // ------------------------------------------------------------------ touch

    @Override
    public boolean onTouchEvent(MotionEvent e) {
        float x = (e.getX() - offX) / scale, y = (e.getY() - offY) / scale;
        switch (e.getActionMasked()) {
            case MotionEvent.ACTION_DOWN:
                down = hitAt(x, y);
                invalidate();
                return true;
            case MotionEvent.ACTION_UP: {
                Hit h = hitAt(x, y);
                Hit d = down;
                down = null;
                invalidate();
                if (h != null && h == d) fire(h.cmd);
                return true;
            }
            case MotionEvent.ACTION_CANCEL:
                down = null;
                invalidate();
                return true;
            default:
                return true;
        }
    }

    private Hit hitAt(float x, float y) {
        for (int i = hits.size() - 1; i >= 0; i--) {
            Hit h = hits.get(i);
            if (x >= h.box.left && x <= h.box.right && y >= h.box.top && y <= h.box.bottom) return h;
        }
        return null;
    }

    private void fire(String cmd) {
        if (cmd.startsWith("page:")) {                       // panel-local: inventory paging
            invPage = Math.max(0, invPage + Integer.parseInt(cmd.substring(5)));
            invalidate();
            return;
        }
        if (sender != null) sender.send(cmd);
    }

    // ------------------------------------------------------------------ drawing

    @Override
    protected void onDraw(Canvas c) {
        int w = getWidth(), h = getHeight();
        scale = Math.min(w / W, h / H);
        offX = (w - W * scale) / 2f;
        offY = (h - H * scale) / 2f;
        hits.clear();
        long t = SystemClock.uptimeMillis();
        c.save();
        c.translate(offX, offY);
        c.scale(scale, scale);
        c.save();
        c.clipRect(0, 0, W, H);
        drawSea(c, t);
        drawHeader(c, t);
        float y = 196;
        if (!haveGame) {
            drawWaiting(c);
        } else {
            if (hint.length() > 0) y = drawHint(c, y) + 18;
            if (dialogue) y = drawDialogue(c, y) + 18;
            drawInventory(c, y, 944);
            drawTrail(c, 960);
        }
        c.restore();
        c.restore();
    }

    private void drawSea(Canvas c, long t) {
        p.setStyle(Paint.Style.FILL);
        p.setShader(new LinearGradient(0, 0, 0, H, SEA_TOP, SEA_BOT, Shader.TileMode.CLAMP));
        c.drawRect(0, 0, W, H, p);
        p.setShader(null);
        // light shafts
        p.setColor(0x10FFFFFF);
        for (int i = 0; i < 4; i++) {
            float x0 = 120 + i * 330, sway = (float) Math.sin(t / 2600.0 + i) * 30;
            path.reset();
            path.moveTo(x0 + sway, 0); path.lineTo(x0 + 90 + sway, 0);
            path.lineTo(x0 + 260 - sway, H); path.lineTo(x0 + 120 - sway, H);
            path.close();
            c.drawPath(path, p);
        }
        // sand floor with a few ripples
        p.setColor(SAND);
        path.reset();
        path.moveTo(0, 1010);
        for (int x = 0; x <= 1240; x += 40) path.lineTo(x, 1010 + (float) Math.sin(x / 90.0) * 8);
        path.lineTo(W, H); path.lineTo(0, H); path.close();
        c.drawPath(path, p);
        p.setColor(SAND_DK);
        p.setStyle(Paint.Style.STROKE); p.setStrokeWidth(3);
        for (int i = 0; i < 6; i++) c.drawLine(60 + i * 210, 1050 + (i % 2) * 10, 150 + i * 210, 1050 + (i % 2) * 10, p);
        p.setStyle(Paint.Style.FILL);
        // kelp at the edges
        kelp(c, 18, 1030, 300, t, 0); kelp(c, 62, 1030, 210, t, 1.7f);
        kelp(c, 1196, 1030, 280, t, 2.4f); kelp(c, 1160, 1030, 190, t, 0.8f);
        // bubbles: deterministic from the clock
        p.setStyle(Paint.Style.STROKE); p.setStrokeWidth(3);
        for (int i = 0; i < 14; i++) {
            float seed = i * 97.3f;
            float prog = ((t / (60f + (i % 5) * 14f)) + seed * 7) % 1100f;
            float x = 40 + (seed * 53) % 1160 + (float) Math.sin(prog / 60.0 + i) * 12;
            float rad = 6 + (i % 4) * 4;
            p.setColor(0x66FFFFFF);
            c.drawCircle(x, 1040 - prog, rad, p);
            p.setColor(0x88FFFFFF);
            c.drawCircle(x - rad * 0.35f, 1040 - prog - rad * 0.35f, rad * 0.2f, p);
        }
        p.setStyle(Paint.Style.FILL);
    }

    private void kelp(Canvas c, float x, float base, float height, long t, float phase) {
        p.setStyle(Paint.Style.FILL);
        for (int s = 0; s < 2; s++) {
            p.setColor(s == 0 ? KELP_DK : KELP);
            float ox = s * 12;
            path.reset();
            int n = 12;
            for (int i = 0; i <= n; i++) {
                float f = i / (float) n;
                float px = x + ox + (float) Math.sin(t / 900.0 + phase + f * 3.0) * 22 * f;
                if (i == 0) path.moveTo(px - 9, base - f * height); else path.lineTo(px - 9 * (1 - f * 0.8f), base - f * height);
            }
            for (int i = n; i >= 0; i--) {
                float f = i / (float) n;
                float px = x + ox + (float) Math.sin(t / 900.0 + phase + f * 3.0) * 22 * f;
                path.lineTo(px + 9 * (1 - f * 0.8f), base - f * height);
            }
            path.close();
            c.drawPath(path, p);
        }
    }

    /** A wooden plank with grain, rivets and a darker edge. */
    private void plank(Canvas c, float l, float t, float rr, float b, boolean pressed) {
        r.set(l + 4, t + 8, rr + 4, b + 8);
        p.setStyle(Paint.Style.FILL); p.setColor(0x44000000);
        c.drawRoundRect(r, 22, 22, p);
        r.set(l, t, rr, b);
        p.setShader(new LinearGradient(0, t, 0, b, pressed ? WOOD_DK : WOOD_LT, pressed ? WOOD_EDGE : WOOD, Shader.TileMode.CLAMP));
        c.drawRoundRect(r, 22, 22, p);
        p.setShader(null);
        c.save();
        c.clipRect(l + 6, t + 6, rr - 6, b - 6);
        p.setStyle(Paint.Style.STROKE); p.setStrokeWidth(2); p.setColor(0x33000000);
        for (float gy = t + 14; gy < b; gy += 17) {
            float wob = (float) Math.sin(gy * 0.37 + l) * 4;
            c.drawLine(l + 10, gy + wob, rr - 10, gy - wob, p);
        }
        c.restore();
        p.setStyle(Paint.Style.STROKE); p.setStrokeWidth(5); p.setColor(WOOD_EDGE);
        c.drawRoundRect(r, 22, 22, p);
        p.setStyle(Paint.Style.FILL); p.setColor(0xFF2B1A0B);
        c.drawCircle(l + 16, t + 16, 4.5f, p); c.drawCircle(rr - 16, t + 16, 4.5f, p);
        c.drawCircle(l + 16, b - 16, 4.5f, p); c.drawCircle(rr - 16, b - 16, 4.5f, p);
    }

    private void rope(Canvas c, float x0, float y0, float x1, float y1) {
        p.setStyle(Paint.Style.STROKE); p.setStrokeCap(Paint.Cap.ROUND);
        p.setStrokeWidth(9); p.setColor(ROPE_DK);
        c.drawLine(x0, y0, x1, y1, p);
        p.setStrokeWidth(5); p.setColor(ROPE);
        c.drawLine(x0, y0, x1, y1, p);
        p.setStrokeWidth(2); p.setColor(ROPE_DK);
        float len = (float) Math.hypot(x1 - x0, y1 - y0);
        for (float d = 6; d < len; d += 14) {
            float f = d / len, cx = x0 + (x1 - x0) * f, cy = y0 + (y1 - y0) * f;
            c.drawLine(cx - 3, cy - 4, cx + 3, cy + 4, p);
        }
        p.setStyle(Paint.Style.FILL);
    }

    private void text(Canvas c, String s, float x, float y, float size, int color, boolean bold, Paint.Align a) {
        p.setStyle(Paint.Style.FILL);
        p.setTypeface(bold ? Typeface.create(Typeface.SANS_SERIF, Typeface.BOLD) : Typeface.SANS_SERIF);
        p.setTextSize(size); p.setTextAlign(a);
        p.setColor(0x66000000);
        if (color == 0xFFFFFFFF || color == GOLD) c.drawText(s, x + 2, y + 3, p);
        p.setColor(color);
        c.drawText(s, x, y, p);
        p.setTextAlign(Paint.Align.LEFT);
    }

    /** Word-wraps s into at most maxLines lines of width maxW at size (the last line gets an ellipsis). */
    private List<String> wrap(String s, float size, float maxW, int maxLines, boolean bold) {
        p.setTypeface(bold ? Typeface.create(Typeface.SANS_SERIF, Typeface.BOLD) : Typeface.SANS_SERIF);
        p.setTextSize(size);
        List<String> out = new ArrayList<String>();
        String[] words = s.split("\\s+");
        StringBuilder cur = new StringBuilder();
        for (int i = 0; i < words.length; i++) {
            String tryLine = cur.length() == 0 ? words[i] : cur + " " + words[i];
            if (p.measureText(tryLine) <= maxW || cur.length() == 0) {
                cur = new StringBuilder(tryLine);
            } else {
                out.add(cur.toString());
                cur = new StringBuilder(words[i]);
            }
        }
        if (cur.length() > 0) out.add(cur.toString());
        if (out.size() > maxLines) {
            String last = out.get(maxLines - 1);
            while (last.length() > 1 && p.measureText(last + "...") > maxW) last = last.substring(0, last.length() - 1);
            List<String> cut = new ArrayList<String>(out.subList(0, maxLines - 1));
            cut.add(last + "...");
            return cut;
        }
        return out;
    }

    private void drawHeader(Canvas c, long t) {
        rope(c, 120, 0, 120, 30); rope(c, 1120, 0, 1120, 30);
        plank(c, 24, 24, 1216 - 280, 172, false);
        String top = chapterTitle.length() > 0 ? chapterTitle : (haveGame ? "" : "BIKINI BOTTOM");
        text(c, top.toUpperCase(), 56, 74, 26, GOLD, true, Paint.Align.LEFT);
        String title = haveGame ? (sceneTitle.length() > 0 ? sceneTitle : "...") : "Companion panel";
        List<String> lines = wrap(title, 46, 880 - 56, 2, true);
        float ty = lines.size() > 1 ? 116 : 134;
        for (String s : lines) { text(c, s, 56, ty, 46, 0xFFFFFFFF, true, Paint.Align.LEFT); ty += 46; }
        // porthole buttons: menu and skip
        porthole(c, 1216 - 250, 98, 62, "MENU", "menu", true, 0);
        porthole(c, 1216 - 90, 98, 62, "SKIP", "skip", haveGame && (cutscene || (dialogue && !choiceMode)), 1);   // live in a cutscene or on a dialogue line
    }

    private void porthole(Canvas c, float cx, float cy, float rad, String label, String cmd, boolean enabled, int glyph) {
        Hit h = enabled ? new Hit(cx - rad, cy - rad, cx + rad, cy + rad, cmd) : null;
        if (h != null) hits.add(h);
        boolean pr = h != null && h == down;
        p.setStyle(Paint.Style.FILL); p.setColor(0x44000000);
        c.drawCircle(cx + 3, cy + 6, rad, p);
        p.setColor(enabled ? (pr ? GOLD_DK : 0xFFCFA25A) : 0xFF7A6A52);
        c.drawCircle(cx, cy, rad, p);
        p.setColor(enabled ? (pr ? 0xFF0A3A55 : TEAL) : 0xFF3C5560);
        c.drawCircle(cx, cy, rad - 12, p);
        p.setStyle(Paint.Style.STROKE); p.setStrokeWidth(5); p.setColor(WOOD_EDGE);
        c.drawCircle(cx, cy, rad, p);
        c.drawCircle(cx, cy, rad - 12, p);
        p.setStyle(Paint.Style.FILL);
        int ink = enabled ? 0xFFFFFFFF : 0x88FFFFFF;
        p.setColor(ink);
        if (glyph == 0) {                        // three lines
            for (int i = -1; i <= 1; i++) { r.set(cx - 22, cy - 6 + i * 14 - 3, cx + 22, cy - 6 + i * 14 + 3); c.drawRoundRect(r, 3, 3, p); }
        } else {                                  // double arrow
            path.reset(); path.moveTo(cx - 24, cy - 24); path.lineTo(cx - 2, cy - 6); path.lineTo(cx - 24, cy + 12); path.close();
            c.drawPath(path, p);
            path.reset(); path.moveTo(cx + 2, cy - 24); path.lineTo(cx + 24, cy - 6); path.lineTo(cx + 2, cy + 12); path.close();
            c.drawPath(path, p);
        }
        text(c, label, cx, cy + 36, 20, ink, true, Paint.Align.CENTER);
    }

    private void drawWaiting(Canvas c) {
        plank(c, 120, 300, 1120, 620, false);
        text(c, "Waiting for the game", W / 2, 420, 54, 0xFFFFFFFF, true, Paint.Align.CENTER);
        text(c, "The inventory, dialogue choices and hints", W / 2, 490, 30, 0xFFF3E3C0, false, Paint.Align.CENTER);
        text(c, "show up here when you start playing.", W / 2, 532, 30, 0xFFF3E3C0, false, Paint.Align.CENTER);
        // a lone jellyfish-style bubble cluster, to keep the page alive
        p.setStyle(Paint.Style.STROKE); p.setStrokeWidth(4); p.setColor(0xAAFFFFFF);
        c.drawCircle(W / 2 - 40, 590, 14, p); c.drawCircle(W / 2, 580, 20, p); c.drawCircle(W / 2 + 46, 592, 11, p);
        p.setStyle(Paint.Style.FILL);
    }

    /** The hint as an order ticket; returns the bottom y. */
    private float drawHint(Canvas c, float y) {
        List<String> lines = wrap(hint, 30, 1040, 2, false);
        float h = 60 + lines.size() * 38;
        r.set(40, y, 1200, y + h);
        p.setStyle(Paint.Style.FILL); p.setColor(0x33000000);
        c.drawRoundRect(new RectF(44, y + 6, 1204, y + h + 6), 14, 14, p);
        p.setColor(PAPER);
        c.drawRoundRect(r, 14, 14, p);
        p.setStyle(Paint.Style.STROKE); p.setStrokeWidth(3); p.setColor(PAPER_EDGE);
        c.drawRoundRect(r, 14, 14, p);
        // torn bottom edge
        p.setStyle(Paint.Style.FILL); p.setColor(PAPER);
        path.reset(); path.moveTo(60, y + h - 2);
        for (int x = 60; x <= 1180; x += 28) { path.lineTo(x + 14, y + h + 10); path.lineTo(x + 28, y + h - 2); }
        path.close();
        c.drawPath(path, p);
        p.setColor(RED);
        c.drawCircle(78, y + 34, 15, p);
        text(c, "?", 78, y + 45, 28, 0xFFFFFFFF, true, Paint.Align.CENTER);
        text(c, "HINT", 112, y + 40, 22, RED, true, Paint.Align.LEFT);
        float ty = y + 48 + 24;
        for (String s : lines) { text(c, s, 112, ty, 30, INK, false, Paint.Align.LEFT); ty += 38; }
        return y + h + 8;
    }

    /** The open dialogue: the line being said, and the choices as big buttons. Returns the bottom y. */
    private float drawDialogue(Canvas c, float y) {
        float bottom;
        if (!choiceMode || choices.isEmpty()) {
            List<String> lines = wrap(line, 34, 1080, 4, false);
            float h = Math.max(330, 130 + lines.size() * 44 + 110);
            if (h > 560) h = 560;
            plank(c, 40, y, 1200, y + h, false);
            text(c, speaker.length() > 0 ? speaker : "...", 72, y + 56, 30, GOLD, true, Paint.Align.LEFT);
            float ty = y + 108;
            for (String s : lines) { text(c, s, 72, ty, 34, 0xFFFFFFFF, false, Paint.Align.LEFT); ty += 44; }
            float by = y + h - 96;
            bigButton(c, 72, by, 1168 - 40, by + 76, "Next", "advance", GOLD, INK, 0);
            return y + h;
        }
        text(c, "Say something", 52, y + 30, 28, 0xFFFFFFFF, true, Paint.Align.LEFT);
        y += 46;
        int n = Math.min(choices.size(), 5);
        float avail = 940 - y - 190;                 // keep room for the inventory row
        float bh = Math.max(96, Math.min(150, (avail - (n - 1) * 14) / n));
        for (int i = 0; i < n; i++) {
            bigButton(c, 40, y, 1200, y + bh, choices.get(i), "choice:" + i, WOOD_LT, 0xFFFFFFFF, i + 1);
            y += bh + 14;
        }
        return y - 14;
    }

    private void bigButton(Canvas c, float l, float t, float rr, float b, String label, String cmd, int fill, int ink, int number) {
        Hit h = new Hit(l, t, rr, b, cmd);
        hits.add(h);
        boolean pr = h == down;
        plank(c, l, t, rr, b, pr);
        float tx = l + 34;
        if (number > 0) {
            p.setStyle(Paint.Style.FILL); p.setColor(pr ? GOLD_DK : GOLD);
            float cy = (t + b) / 2;
            c.drawCircle(l + 56, cy, 30, p);
            p.setStyle(Paint.Style.STROKE); p.setStrokeWidth(4); p.setColor(WOOD_EDGE);
            c.drawCircle(l + 56, cy, 30, p);
            text(c, String.valueOf(number), l + 56, cy + 13, 36, INK, true, Paint.Align.CENTER);
            tx = l + 108;
        }
        float bh = b - t;
        float size = bh >= 120 ? 36 : 32;
        List<String> lines = wrap(label, size, rr - tx - 30, Math.max(1, (int) ((bh - 24) / (size + 10))), true);
        float total = lines.size() * (size + 10);
        float ty = t + (bh - total) / 2 + size;
        for (String s : lines) {
            if (number == 0) text(c, s, (l + rr) / 2, ty, size, ink, true, Paint.Align.CENTER);
            else text(c, s, tx, ty, size, ink, true, Paint.Align.LEFT);
            ty += size + 10;
        }
    }

    private void drawInventory(Canvas c, float top, float bottom) {
        boolean compact = dialogue && choiceMode;
        float headH = 44;
        text(c, "POCKET", 52, top + 32, 28, GOLD, true, Paint.Align.LEFT);
        if (selected.length() > 0) text(c, "In hand: " + itemName(items.indexOf(selected), selected), 1200, top + 32, 26, 0xFFFFFFFF, true, Paint.Align.RIGHT);
        float y0 = top + headH;
        float avail = bottom - y0;
        int cols = compact ? 4 : 3;
        float gap = 18, cw = (1160 - gap * (cols - 1)) / cols;
        float ch = compact ? 130 : 200;
        int rows = Math.max(1, (int) ((avail + gap) / (ch + gap)));
        if (!compact && items.size() > cols * rows) rows = Math.max(1, rows);
        int perPage = cols * rows;
        int pages = Math.max(1, (items.size() + perPage - 1) / perPage);
        if (invPage >= pages) invPage = pages - 1;
        if (items.isEmpty()) {
            r.set(40, y0, 1200, y0 + Math.min(avail, compact ? 100 : 170));
            p.setStyle(Paint.Style.FILL); p.setColor(0x33000000);
            c.drawRoundRect(r, 24, 24, p);
            p.setStyle(Paint.Style.STROKE); p.setStrokeWidth(4); p.setColor(0x66FFFFFF);
            c.drawRoundRect(r, 24, 24, p);
            p.setStyle(Paint.Style.FILL);
            text(c, "Nothing in your pockets yet", W / 2, r.centerY() + 10, 32, 0xCCFFFFFF, false, Paint.Align.CENTER);
            return;
        }
        for (int i = 0; i < perPage; i++) {
            int idx = invPage * perPage + i;
            if (idx >= items.size()) break;
            float x = 40 + (i % cols) * (cw + gap), y = y0 + (i / cols) * (ch + gap);
            itemCard(c, x, y, x + cw, y + ch, items.get(idx), itemName(idx, items.get(idx)), compact);
        }
        if (pages > 1) {
            float py = bottom - 4;
            text(c, (invPage + 1) + " / " + pages, 1100, py - 8 + 0, 24, 0xFFFFFFFF, true, Paint.Align.CENTER);
            if (invPage > 0) hits.add(new Hit(950, py - 56, 1030, py + 10, "page:-1"));
            if (invPage < pages - 1) hits.add(new Hit(1170 - 80, py - 56, 1170, py + 10, "page:1"));
            text(c, "<", 990, py - 8, 44, invPage > 0 ? GOLD : 0x55FFFFFF, true, Paint.Align.CENTER);
            text(c, ">", 1170 - 40, py - 8, 44, invPage < pages - 1 ? GOLD : 0x55FFFFFF, true, Paint.Align.CENTER);
        }
    }

    /** An inventory item as a card: its picture (the game renders it from the player's files), else a coloured badge with its initials; and its name. */
    private void itemCard(Canvas c, float l, float t, float rr, float b, String id, String name, boolean compact) {
        boolean sel = id.equalsIgnoreCase(selected);
        Hit h = new Hit(l, t, rr, b, sel ? "noitem" : "item:" + id);
        hits.add(h);
        boolean pr = h == down;
        plank(c, l, t, rr, b, pr);
        if (sel) {
            r.set(l - 6, t - 6, rr + 6, b + 6);
            p.setStyle(Paint.Style.STROKE); p.setStrokeWidth(8); p.setColor(GOLD);
            c.drawRoundRect(r, 26, 26, p);
        }
        float cx = compact ? l + 62 : (l + rr) / 2, cy = compact ? (t + b) / 2 : t + 78;
        float rad = compact ? 40 : 56;
        Bitmap pic = picture(id);
        if (pic != null) {                                   // a bubble with the item's picture in it
            p.setStyle(Paint.Style.FILL); p.setColor(0x55000000);
            c.drawCircle(cx + 3, cy + 5, rad, p);
            p.setColor(0xFF3B8FC4);
            c.drawCircle(cx, cy, rad, p);
            p.setColor(0x33FFFFFF);
            c.drawCircle(cx - rad * 0.3f, cy - rad * 0.35f, rad * 0.5f, p);
            float half = rad * 0.92f;
            r.set(cx - half, cy - half, cx + half, cy + half);
            c.drawBitmap(pic, null, r, picPaint);
            p.setStyle(Paint.Style.STROKE); p.setStrokeWidth(5); p.setColor(WOOD_EDGE);
            c.drawCircle(cx, cy, rad, p);
        } else {
            int hue = hue(id);
            p.setStyle(Paint.Style.FILL); p.setColor(0x55000000);
            c.drawCircle(cx + 3, cy + 5, rad, p);
            p.setColor(hue);
            c.drawCircle(cx, cy, rad, p);
            p.setColor(0x33FFFFFF);
            c.drawCircle(cx - rad * 0.3f, cy - rad * 0.35f, rad * 0.5f, p);
            p.setStyle(Paint.Style.STROKE); p.setStrokeWidth(5); p.setColor(WOOD_EDGE);
            c.drawCircle(cx, cy, rad, p);
            text(c, initials(id), cx, cy + rad * 0.3f, rad * 0.9f, 0xFFFFFFFF, true, Paint.Align.CENTER);
        }
        if (compact) {
            List<String> lines = wrap(name, 26, rr - (cx + rad) - 24, 3, true);
            float ty = (t + b) / 2 - (lines.size() - 1) * 15 + 9;
            for (String s : lines) { text(c, s, cx + rad + 14, ty, 26, 0xFFFFFFFF, true, Paint.Align.LEFT); ty += 30; }
        } else {
            List<String> lines = wrap(name, 28, rr - l - 24, 2, true);
            float ty = t + 158 - (lines.size() - 1) * 15 + 4;
            for (String s : lines) { text(c, s, (l + rr) / 2, ty, 28, 0xFFFFFFFF, true, Paint.Align.CENTER); ty += 32; }
        }
        if (sel && !compact) text(c, "IN HAND", rr - 14, t + 30, 20, GOLD, true, Paint.Align.RIGHT);
    }

    /** The scenes visited, newest on the right: chips on a rope. */
    private void drawTrail(Canvas c, float y) {
        if (visited.isEmpty()) return;
        rope(c, 40, y + 26, 1200, y + 26);
        text(c, "MAP", 52, y + 14, 20, GOLD, true, Paint.Align.LEFT);
        int show = Math.min(visited.size(), 6);
        float cw = 178, gap = 12, x = 1200 - show * (cw + gap) + gap;
        for (int i = visited.size() - show; i < visited.size(); i++) {
            boolean cur = i == visited.size() - 1;
            r.set(x, y + 40, x + cw, y + 90);
            p.setStyle(Paint.Style.FILL); p.setColor(cur ? GOLD : PAPER);
            c.drawRoundRect(r, 16, 16, p);
            p.setStyle(Paint.Style.STROKE); p.setStrokeWidth(3); p.setColor(WOOD_EDGE);
            c.drawRoundRect(r, 16, 16, p);
            List<String> nm = wrap(visited.get(i), 20, cw - 16, 2, false);   // the friendly scene names are long: two lines
            float ny = nm.size() > 1 ? y + 60 : y + 72;
            for (String s : nm) { text(c, s, x + cw / 2, ny, 20, INK, false, Paint.Align.CENTER); ny += 22; }
            x += cw + gap;
        }
    }

    // ------------------------------------------------------------------ item names

    /** Readable name for an item id until the game's own item table is read ("key_card" -> "Key card"). */
    static String prettyName(String id) {
        if (id == null || id.length() == 0) return "";
        String s = id.replace('_', ' ').replace('-', ' ').trim();
        return Character.toUpperCase(s.charAt(0)) + s.substring(1);
    }

    private static String initials(String id) {
        String s = id.replaceAll("[^A-Za-z0-9 _-]", "");
        if (s.length() == 0) return "?";
        String[] parts = s.split("[ _-]+");
        if (parts.length >= 2) return ("" + parts[0].charAt(0) + parts[1].charAt(0)).toUpperCase();
        return s.substring(0, Math.min(2, s.length())).toUpperCase();
    }

    private static int hue(String id) {
        int h = 0;
        for (int i = 0; i < id.length(); i++) h = h * 31 + id.charAt(i);
        int[] pal = { 0xFFE0674A, 0xFF3FA66B, 0xFF4B8FD6, 0xFFB566D8, 0xFFE0A02E, 0xFF2BA6A0, 0xFFD6577F };
        return pal[(h & 0x7fffffff) % pal.length];
    }
}
