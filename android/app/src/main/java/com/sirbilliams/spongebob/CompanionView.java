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

    /** The touchpad's way into the game: a relative cursor move (the game's 640x480 logical pixels) and the left button (1 down, 0 up, -1 unchanged). */
    public interface Pad { void move(float dx, float dy, int button); }

    /** Press feedback (haptic + click): kind 1 = a tap on a target / a pad click, 2 = firm (a pad drag starts, a card is picked). */
    public interface Feedback { void tap(int kind); }

    /** The player switched Panel / Touchpad on the panel itself. */
    public interface ModeListener { void modeChanged(int mode); }

    public static final int MODE_PANEL = 0, MODE_PAD = 1;

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
    private Pad pad;
    private Feedback feedback;
    private ModeListener modeListener;
    private int mode = MODE_PANEL;

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

    // ---- pages: Pocket (the dashboard), Map, Talk, Goals ----
    public static final int PAGE_POCKET = 0, PAGE_MAP = 1, PAGE_TALK = 2, PAGE_GOALS = 3;
    private static final String[] TABS = { "POCKET", "MAP", "TALK", "GOALS" };
    private int page = PAGE_POCKET;
    private static final float TAB_Y = 184, TAB_H = 56, CONTENT_Y = 256;
    private static final class Area { String name = "", cmd = ""; int x, y, w, h; boolean ok, visited, here; }
    private final List<Area> areas = new ArrayList<Area>();
    private boolean mapBusy;
    private String mapNote = ""; private long mapNoteUntil;
    private static final class Line { int id; String speaker = "", text = ""; boolean voice; }
    private final List<Line> talk = new ArrayList<Line>();
    private float talkScroll; private boolean talkStick = true; private int talkSeen;
    private final RectF listRect = new RectF();                        // the scrollable list of the Talk page (empty elsewhere)
    private int combineSerial = -1; private boolean combineOk; private String combineText = ""; private long combineAt;
    private int goalSeen, goalTotal, goalScripts;
    private final List<String> placeNames = new ArrayList<String>();
    private final List<int[]> placeCounts = new ArrayList<int[]>();

    // ---- touch ----
    private static final class Hit {
        final RectF box = new RectF(); String cmd; String item;      // item: an inventory card (can be dragged onto another)
        Hit(float l, float t, float rr, float b, String c) { box.set(l, t, rr, b); cmd = c; }
    }
    private final List<Hit> hits = new ArrayList<Hit>();
    private Hit down;
    private float scale = 1f, offX, offY;
    // dragging an inventory card onto another (= combine), and scrolling the Talk list
    private static final float DRAG_SLOP = 26f;
    private Hit dragCand;
    private boolean dragging, scrollTouch, scrolling;
    private float dragX, dragY, touchX0, touchY0, scrollLastY;

    public CompanionView(Context c) {
        super(c);
        setContentDescription("SpongeBob companion panel");
    }

    public void setSender(Sender s) { sender = s; }
    public void setPad(Pad p) { pad = p; }
    public void setFeedback(Feedback f) { feedback = f; }
    public void setModeListener(ModeListener l) { modeListener = l; }
    public int getMode() { return mode; }
    public void setMode(int m) {
        int nm = m == MODE_PAD ? MODE_PAD : MODE_PANEL;
        if (nm != mode) { padRelease(false); mode = nm; }
        invalidate();
    }
    private void tap(int kind) { if (feedback != null) feedback.tap(kind); }

    /** Parses a snapshot (null / empty: the game is not running yet) and redraws. */
    public void setSnapshot(String json) {
        haveGame = false; dialogue = choiceMode = cutscene = false;
        items.clear(); itemNames.clear(); choices.clear(); visited.clear(); iconDir = "";
        areas.clear(); talk.clear(); placeNames.clear(); placeCounts.clear(); mapBusy = false; goalSeen = goalTotal = goalScripts = 0;
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
                parseExtra(o);
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

    /** The map, talk, combine and goals parts of the snapshot (docs/companion.md). */
    private void parseExtra(JSONObject o) {
        JSONArray m = o.optJSONArray("map");
        if (m != null) for (int i = 0; i < m.length(); i++) {
            JSONObject j = m.optJSONObject(i);
            if (j == null) continue;
            Area a = new Area();
            a.name = j.optString("n", ""); a.cmd = j.optString("c", "");
            a.x = j.optInt("x"); a.y = j.optInt("y"); a.w = j.optInt("w"); a.h = j.optInt("h");
            a.ok = j.optInt("ok") != 0; a.visited = j.optInt("v") != 0; a.here = j.optInt("here") != 0;
            areas.add(a);
        }
        mapBusy = o.optInt("mapbusy", 0) != 0;
        JSONArray t = o.optJSONArray("talk");
        if (t != null) for (int i = 0; i < t.length(); i++) {
            JSONObject j = t.optJSONObject(i);
            if (j == null) continue;
            Line l = new Line();
            l.id = j.optInt("i"); l.speaker = j.optString("s", ""); l.text = j.optString("t", ""); l.voice = j.optInt("v") != 0;
            talk.add(l);
        }
        JSONObject cb = o.optJSONObject("combine");
        if (cb != null) {
            int n = cb.optInt("n", 0);
            if (combineSerial < 0) combineSerial = n;             // a result from before the panel came up is not news
            else if (n != combineSerial) {
                combineSerial = n; combineOk = cb.optInt("ok") != 0; combineText = cb.optString("t", "");
                combineAt = SystemClock.uptimeMillis();
            }
        }
        JSONObject g = o.optJSONObject("goals");
        if (g != null) {
            goalSeen = g.optInt("sv"); goalTotal = g.optInt("st"); goalScripts = g.optInt("sc");
            JSONArray pl = g.optJSONArray("places");
            if (pl != null) for (int i = 0; i < pl.length(); i++) {
                JSONObject j = pl.optJSONObject(i);
                if (j == null) continue;
                placeNames.add(j.optString("n", "")); placeCounts.add(new int[] { j.optInt("v"), j.optInt("t") });
            }
        }
    }

    public void setPage(int p) { page = Math.max(0, Math.min(3, p)); talkStick = true; invalidate(); }
    public int getPage() { return page; }

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

    // The touchpad (MODE_PAD): one finger. A drag moves the game's cursor relatively with acceleration (slow = precise, fast = across the
    // screen); a quick tap is a left click; press and hold without moving, then drag = the left button held while moving (carry an item
    // onto a character). All of it goes through Pad.move to the same pointer path as the right stick.
    private static final float PAD_SLOP = 22f;           // layout px a finger may wander and still be a tap / a hold
    private static final long TAP_MS = 320, HOLD_MS = 380, CLICK_MS = 70;
    private final RectF padRect = new RectF();
    private boolean padOn, padSlopLeft, padHeld;         // finger on the pad / has left the slop circle / the left button is down (drag armed)
    private float padStartX, padStartY, padLastX, padLastY, padX, padY, padSpeed;
    private long padStartT, padLastT;
    private final Runnable holdTimer = new Runnable() { @Override public void run() { padArm(); } };

    @Override
    public boolean onTouchEvent(MotionEvent e) {
        float x = (e.getX() - offX) / scale, y = (e.getY() - offY) / scale;
        long t = e.getEventTime();
        switch (e.getActionMasked()) {
            case MotionEvent.ACTION_DOWN:
                down = hitAt(x, y);
                dragCand = down != null && down.item != null ? down : null;
                dragging = scrolling = false;
                dragX = touchX0 = x; dragY = touchY0 = y; scrollLastY = y;
                scrollTouch = page == PAGE_TALK && x >= listRect.left && x <= listRect.right && y >= listRect.top && y <= listRect.bottom;
                if (down != null) tap(down.cmd.startsWith("item:") ? 2 : 1);
                else if (mode == MODE_PAD && (x >= padRect.left && x <= padRect.right && y >= padRect.top && y <= padRect.bottom)) padStart(x, y, t);
                invalidate();
                return true;
            case MotionEvent.ACTION_MOVE:
                if (padOn) padMove(x, y, t);
                if (dragCand != null && !dragging && Math.hypot(x - touchX0, y - touchY0) > DRAG_SLOP) { dragging = true; tap(2); }
                if (dragging) { dragX = x; dragY = y; invalidate(); }
                if (scrollTouch && !scrolling && Math.abs(y - touchY0) > 18) { scrolling = true; down = null; }
                if (scrolling) { talkScroll -= y - scrollLastY; talkStick = false; invalidate(); }
                scrollLastY = y;
                return true;
            case MotionEvent.ACTION_UP: {
                if (padOn) { padEnd(x, y, t); invalidate(); return true; }
                if (dragging) {                                  // a card was dragged: dropped on another card = combine (the game's own code decides)
                    Hit tgt = cardAt(x, y);
                    String a = dragCand.item;
                    dragging = false; dragCand = null; down = null;
                    invalidate();
                    if (tgt != null && !tgt.item.equalsIgnoreCase(a) && sender != null) { tap(1); sender.send("combine:" + a + ":" + tgt.item); }
                    return true;
                }
                if (scrolling) { scrolling = scrollTouch = false; down = null; invalidate(); return true; }
                Hit h = hitAt(x, y);
                Hit d = down;
                down = null;
                invalidate();
                if (h != null && h == d) fire(h.cmd);
                return true;
            }
            case MotionEvent.ACTION_CANCEL:
                down = null; dragging = scrolling = scrollTouch = false; dragCand = null;
                padRelease(true);
                invalidate();
                return true;
            default:
                return true;
        }
    }

    private void padStart(float x, float y, long t) {
        padOn = true; padSlopLeft = false; padHeld = false;
        padStartX = padLastX = padX = x; padStartY = padLastY = padY = y;
        padStartT = padLastT = t; padSpeed = 0;
        removeCallbacks(holdTimer);
        postDelayed(holdTimer, HOLD_MS);
    }

    /** Press and hold without moving: the left button goes down; the next moves drag. */
    private void padArm() {
        if (!padOn || padHeld || padSlopLeft) return;
        padHeld = true;
        tap(2);
        if (pad != null) pad.move(0, 0, 1);
        invalidate();
    }

    private void padMove(float x, float y, long t) {
        padX = x; padY = y;
        if (!padHeld && !padSlopLeft && t - padStartT >= HOLD_MS && Math.hypot(x - padStartX, y - padStartY) < PAD_SLOP) padArm();
        if (!padSlopLeft && Math.hypot(x - padStartX, y - padStartY) >= PAD_SLOP) {
            padSlopLeft = true;                          // not a tap / a hold any more (a held button stays down)
            removeCallbacks(holdTimer);
            padLastX = x; padLastY = y; padLastT = t;    // the cursor starts from here: no jump by the slop
            return;
        }
        if (!padSlopLeft && !padHeld) return;
        float dx = x - padLastX, dy = y - padLastY;
        long dt = Math.max(1, t - padLastT);
        padLastX = x; padLastY = y; padLastT = t;
        if (dx == 0 && dy == 0) return;
        float speed = (float) Math.hypot(dx, dy) / dt;   // layout px per ms
        padSpeed = padSpeed * 0.6f + speed * 0.4f;
        float gain = 0.45f + Math.min(1.8f, padSpeed * 0.9f);   // 0.45 crawling .. 2.25 flicking (1160 px of pad = ~520 .. 2600 game px)
        if (pad != null) pad.move(dx * gain, dy * gain, -1);
        invalidate();
    }

    private void padEnd(float x, float y, long t) {
        removeCallbacks(holdTimer);
        if (!padHeld && !padSlopLeft && t - padStartT >= HOLD_MS && Math.hypot(x - padStartX, y - padStartY) < PAD_SLOP) padArm();
        boolean quickTap = !padHeld && !padSlopLeft && t - padStartT <= TAP_MS;
        padOn = false;
        if (padHeld) {                                   // end of a drag / of a long press: the button comes up here
            padHeld = false;
            if (pad != null) pad.move(0, 0, 0);
        } else if (quickTap) {                           // tap = left click: down now, up a moment later (the game polls the button once a frame)
            tap(1);
            if (pad != null) pad.move(0, 0, 1);
            postDelayed(new Runnable() { @Override public void run() { if (pad != null) pad.move(0, 0, 0); } }, CLICK_MS);
        }
    }

    /** The finger is gone for good (cancel, mode change): the button must not stay down. */
    private void padRelease(boolean cancel) {
        removeCallbacks(holdTimer);
        if (padHeld && pad != null) pad.move(0, 0, 0);
        padOn = padHeld = padSlopLeft = false;
    }

    private Hit hitAt(float x, float y) {
        for (int i = hits.size() - 1; i >= 0; i--) {
            Hit h = hits.get(i);
            if (x >= h.box.left && x <= h.box.right && y >= h.box.top && y <= h.box.bottom) return h;
        }
        return null;
    }

    /** The inventory card under x,y (a drop target), or null. */
    private Hit cardAt(float x, float y) {
        for (int i = hits.size() - 1; i >= 0; i--) {
            Hit h = hits.get(i);
            if (h.item != null && x >= h.box.left && x <= h.box.right && y >= h.box.top && y <= h.box.bottom) return h;
        }
        return null;
    }

    /** Test helper (tools/companion/panel-test.sh): the centre of a hit target by its command or item id, in view pixels; null if not drawn. */
    public float[] hitCenterForTest(String cmdOrItem) {
        for (Hit h : hits)
            if (cmdOrItem.equals(h.cmd) || cmdOrItem.equals(h.item) || (cmdOrItem.startsWith("note:") && h.cmd.startsWith(cmdOrItem))) return new float[] { offX + h.box.centerX() * scale, offY + h.box.centerY() * scale };
        return null;
    }

    private void fire(String cmd) {
        if (cmd.startsWith("tab:")) {                        // panel-local: Pocket / Map / Talk / Goals
            setPage(Integer.parseInt(cmd.substring(4)));
            return;
        }
        if (cmd.startsWith("note:")) {                       // panel-local: a short message (a map place that is not open)
            mapNote = cmd.substring(5); mapNoteUntil = SystemClock.uptimeMillis() + 3500;
            invalidate();
            return;
        }
        if (cmd.startsWith("mode:")) {                       // panel-local: Panel / Touchpad
            mode = cmd.endsWith("1") ? MODE_PAD : MODE_PANEL;
            if (modeListener != null) modeListener.modeChanged(mode);
            invalidate();
            return;
        }
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
        float y = CONTENT_Y;
        padRect.set(0, 0, 0, 0);
        listRect.set(0, 0, 0, 0);
        int pg = dialogue && choiceMode ? PAGE_POCKET : page;      // a question to answer always shows the Pocket page
        if (haveGame) drawTabs(c, pg);
        if (!haveGame) {
            drawWaiting(c);
        } else if (pg == PAGE_MAP) {
            drawMap(c, y);
        } else if (pg == PAGE_TALK) {
            drawTalk(c, y);
        } else if (pg == PAGE_GOALS) {
            drawGoals(c, y);
        } else if (mode == MODE_PAD && !(dialogue && choiceMode)) {
            drawPadLayout(c, y, t);
        } else {
            if (hint.length() > 0) y = drawHint(c, y) + 18;
            if (dialogue) y = drawDialogue(c, y) + 18;
            drawInventory(c, y, 944);
            drawTrail(c, 960);
        }
        if (haveGame && pg == PAGE_POCKET) drawCombineBanner(c, t);
        drawDragGhost(c);
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
        modeSwitch(c);
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

    /** Panel | Touchpad: a two-segment switch in the header plank. */
    private void modeSwitch(Canvas c) {
        float l = 548, t0 = 36, w = 170, h = 48;
        for (int i = 0; i < 2; i++) {
            float x = l + i * (w + 4);
            Hit hh = new Hit(x, t0, x + w, t0 + h + 6, "mode:" + i);
            hits.add(hh);
            boolean on = mode == i, pr = hh == down;
            r.set(x, t0, x + w, t0 + h);
            p.setStyle(Paint.Style.FILL);
            p.setColor(on ? GOLD : (pr ? WOOD_DK : 0x66000000));
            c.drawRoundRect(r, 24, 24, p);
            p.setStyle(Paint.Style.STROKE); p.setStrokeWidth(3); p.setColor(WOOD_EDGE);
            c.drawRoundRect(r, 24, 24, p);
            text(c, i == 0 ? "PANEL" : "TOUCHPAD", x + w / 2, t0 + 33, 22, on ? INK : 0xFFF3E3C0, true, Paint.Align.CENTER);
        }
        p.setStyle(Paint.Style.FILL);
    }

    /**
     * Touchpad layout: [slim dialogue strip when a line is open] / the pad / the pocket as one slim row (4 cards, paging) with HINT and PUT AWAY.
     * (A dialogue's choices need no cursor: that case uses the Panel layout, big buttons.)
     */
    private void drawPadLayout(Canvas c, float y, long t) {
        if (dialogue) {                                    // a line is being said: Next (also tapping the pad advances in the game)
            List<String> lines = wrap(line, 28, 800, 2, false);
            plank(c, 40, y, 1200, y + 140, false);
            text(c, speaker.length() > 0 ? speaker : "...", 68, y + 44, 26, GOLD, true, Paint.Align.LEFT);
            float ty = y + 82;
            for (String s : lines) { text(c, s, 68, ty, 28, 0xFFFFFFFF, false, Paint.Align.LEFT); ty += 34; }
            bigButton(c, 900, y + 24, 1176, y + 116, "Next", "advance", GOLD, INK, 0);
            y += 140 + 14;
        }
        float padBottom = 862;
        drawPad(c, 40, y, 1200, padBottom, t);
        drawPadInventory(c, padBottom + 14, 1066);
    }

    private void drawPad(Canvas c, float l, float t0, float rr, float b, long t) {
        padRect.set(l, t0, rr, b);
        boolean live = padOn;
        r.set(l + 4, t0 + 8, rr + 4, b + 8);
        p.setStyle(Paint.Style.FILL); p.setColor(0x44000000);
        c.drawRoundRect(r, 34, 34, p);
        r.set(l, t0, rr, b);
        p.setShader(new LinearGradient(0, t0, 0, b, live ? 0xFF2FB3D6 : 0xFF1F8FB4, live ? 0xFF0F6C94 : 0xFF0B5273, Shader.TileMode.CLAMP));
        c.drawRoundRect(r, 34, 34, p);
        p.setShader(null);
        // dot grid: the pad's surface; the dots drift with the finger, so a drag is felt as movement
        c.save();
        c.clipRect(l + 8, t0 + 8, rr - 8, b - 8);
        float ox = live ? (padX - padStartX) * 0.25f % 58 : 0, oy = live ? (padY - padStartY) * 0.25f % 58 : 0;
        p.setColor(live ? 0x55FFFFFF : 0x30FFFFFF);
        for (float gy = t0 + 29 + oy - 58; gy < b; gy += 58)
            for (float gx = l + 29 + ox - 58; gx < rr; gx += 58) c.drawCircle(gx, gy, 3.2f, p);
        c.restore();
        p.setStyle(Paint.Style.STROKE); p.setStrokeWidth(10); p.setColor(live ? GOLD : WOOD);
        c.drawRoundRect(r, 34, 34, p);
        p.setStrokeWidth(3); p.setColor(WOOD_EDGE);
        r.set(l - 5, t0 - 5, rr + 5, b + 5);
        c.drawRoundRect(r, 38, 38, p);
        p.setStyle(Paint.Style.FILL);
        float cx = (l + rr) / 2, cy = (t0 + b) / 2;
        if (!live) {
            text(c, "TOUCHPAD", cx, cy - 18, 40, 0xCCFFFFFF, true, Paint.Align.CENTER);
            text(c, "drag = move the cursor    tap = click", cx, cy + 28, 26, 0xAAFFFFFF, false, Paint.Align.CENTER);
            text(c, "hold, then drag = carry", cx, cy + 62, 26, 0xAAFFFFFF, false, Paint.Align.CENTER);
        } else {
            // the finger: a ring (gold and filled once the button is down), and a tail back to where it started
            p.setStyle(Paint.Style.STROKE); p.setStrokeWidth(4); p.setColor(0x66FFFFFF);
            c.drawLine(padStartX, padStartY, padX, padY, p);
            p.setStyle(Paint.Style.FILL);
            p.setColor(padHeld ? 0x88FFD43B : 0x44FFFFFF);
            c.drawCircle(padX, padY, padHeld ? 62 : 48, p);
            p.setStyle(Paint.Style.STROKE); p.setStrokeWidth(6); p.setColor(padHeld ? GOLD : 0xFFFFFFFF);
            c.drawCircle(padX, padY, padHeld ? 62 : 48, p);
            p.setStyle(Paint.Style.FILL);
            if (padHeld) text(c, "HOLDING", cx, b - 30, 28, GOLD, true, Paint.Align.CENTER);
        }
    }

    /** One slim row of the pocket for the touchpad layout: tap a card = take it in hand (the next pad click uses it), HINT, PUT AWAY, paging. */
    private void drawPadInventory(Canvas c, float top, float bottom) {
        text(c, "POCKET", 52, top + 30, 26, GOLD, true, Paint.Align.LEFT);
        if (selected.length() > 0) text(c, "In hand: " + itemName(items.indexOf(selected), selected), 200, top + 30, 24, 0xFFFFFFFF, true, Paint.Align.LEFT);
        chip(c, 1200 - 170, top + 2, 1200, top + 44, "HINT", "hint", true);
        if (selected.length() > 0) chip(c, 1200 - 400, top + 2, 1200 - 182, top + 44, "PUT AWAY", "noitem", true);
        float y0 = top + 54, ch = bottom - y0;
        int cols = 4, perPage = cols;
        float gap = 14, cw = (1160 - gap * (cols - 1)) / cols;
        int pages = Math.max(1, (items.size() + perPage - 1) / perPage);
        if (invPage >= pages) invPage = pages - 1;
        if (items.isEmpty()) {
            r.set(40, y0, 1200, y0 + ch);
            p.setStyle(Paint.Style.FILL); p.setColor(0x33000000);
            c.drawRoundRect(r, 24, 24, p);
            p.setStyle(Paint.Style.STROKE); p.setStrokeWidth(4); p.setColor(0x66FFFFFF);
            c.drawRoundRect(r, 24, 24, p);
            p.setStyle(Paint.Style.FILL);
            text(c, "Nothing in your pockets yet", W / 2, r.centerY() + 10, 30, 0xCCFFFFFF, false, Paint.Align.CENTER);
            return;
        }
        boolean arrows = pages > 1;
        float aw = arrows ? 56 : 0;                       // paging arrows take the sides of the row
        cw = (1160 - 2 * aw - gap * (cols - 1)) / cols;
        for (int i = 0; i < perPage; i++) {
            int idx = invPage * perPage + i;
            if (idx >= items.size()) break;
            float x = 40 + aw + i * (cw + gap);
            itemCard(c, x, y0, x + cw, y0 + ch, items.get(idx), itemName(idx, items.get(idx)), true);
        }
        if (arrows) {
            hits.add(new Hit(36, y0, 40 + aw, y0 + ch, "page:-1"));
            hits.add(new Hit(1200 - aw, y0, 1204, y0 + ch, "page:1"));
            text(c, "<", 40 + aw / 2 - 4, y0 + ch / 2 + 16, 48, invPage > 0 ? GOLD : 0x55FFFFFF, true, Paint.Align.CENTER);
            text(c, ">", 1200 - aw / 2 + 4, y0 + ch / 2 + 16, 48, invPage < pages - 1 ? GOLD : 0x55FFFFFF, true, Paint.Align.CENTER);
        }
    }

    /** A small rounded button. */
    private void chip(Canvas c, float l, float t0, float rr, float b, String label, String cmd, boolean enabled) {
        Hit h = new Hit(l - 6, t0 - 6, rr + 6, b + 6, cmd);       // a finger target bigger than the drawing
        if (enabled) hits.add(h);
        boolean pr = h == down;
        r.set(l, t0, rr, b);
        p.setStyle(Paint.Style.FILL); p.setColor(pr ? GOLD_DK : (enabled ? PAPER : 0x55FFFFFF));
        c.drawRoundRect(r, 20, 20, p);
        p.setStyle(Paint.Style.STROKE); p.setStrokeWidth(3); p.setColor(WOOD_EDGE);
        c.drawRoundRect(r, 20, 20, p);
        text(c, label, (l + rr) / 2, b - 12, 22, INK, true, Paint.Align.CENTER);
        p.setStyle(Paint.Style.FILL);
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
    private float drawHint(Canvas c, float y) { return drawTicket(c, y, "HINT", hint, 2); }

    private float drawTicket(Canvas c, float y, String title, String body, int maxLines) {
        List<String> lines = wrap(body, 30, 1040, maxLines, false);
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
        text(c, title, 112, y + 40, 22, RED, true, Paint.Align.LEFT);
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
        boolean compact = (dialogue && choiceMode) || bottom - (top + 44) < 200;
        float headH = 44;
        text(c, "POCKET", 52, top + 32, 28, GOLD, true, Paint.Align.LEFT);
        if (selected.length() > 0) text(c, "In hand: " + itemName(items.indexOf(selected), selected), 1200, top + 32, 26, 0xFFFFFFFF, true, Paint.Align.RIGHT);
        else if (items.size() > 1) text(c, "Drag one card onto another to combine", 1200, top + 32, 24, 0xFFF3E3C0, false, Paint.Align.RIGHT);
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
        h.item = id;
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
        if (dragging && dragCand != null) {
            if (id.equalsIgnoreCase(dragCand.item)) {                 // the card being carried
                r.set(l, t, rr, b);
                p.setStyle(Paint.Style.FILL); p.setColor(0x66000000);
                c.drawRoundRect(r, 22, 22, p);
            } else if (dragX >= l && dragX <= rr && dragY >= t && dragY <= b) {   // a card to drop on
                r.set(l - 6, t - 6, rr + 6, b + 6);
                p.setStyle(Paint.Style.STROKE); p.setStrokeWidth(8); p.setColor(TEAL);
                c.drawRoundRect(r, 26, 26, p);
                p.setStyle(Paint.Style.FILL);
                text(c, "COMBINE", (l + rr) / 2, t + 26, 22, TEAL, true, Paint.Align.CENTER);
            }
        }
    }

    /** The card in the finger while dragging. */
    private void drawDragGhost(Canvas c) {
        if (!dragging || dragCand == null) return;
        String id = dragCand.item;
        Bitmap pic = picture(id);
        float rad = 54;
        p.setStyle(Paint.Style.FILL); p.setColor(0x66000000);
        c.drawCircle(dragX + 5, dragY + 9, rad, p);
        p.setColor(pic != null ? 0xFF3B8FC4 : hue(id));
        c.drawCircle(dragX, dragY, rad, p);
        if (pic != null) {
            r.set(dragX - rad * 0.92f, dragY - rad * 0.92f, dragX + rad * 0.92f, dragY + rad * 0.92f);
            c.drawBitmap(pic, null, r, picPaint);
        } else text(c, initials(id), dragX, dragY + rad * 0.3f, rad * 0.9f, 0xFFFFFFFF, true, Paint.Align.CENTER);
        p.setStyle(Paint.Style.STROKE); p.setStrokeWidth(5); p.setColor(GOLD);
        c.drawCircle(dragX, dragY, rad, p);
        p.setStyle(Paint.Style.FILL);
    }

    /** The result of the last combine: what was made, or the game's own refusal comment. Shown for 6 s. */
    private void drawCombineBanner(Canvas c, long t) {
        if (combineText.length() == 0 || t - combineAt > 6000) return;
        List<String> lines = wrap(combineText, 28, 900, 2, true);
        float h = 44 + lines.size() * 34;
        float by = mode == MODE_PAD ? 770 : 868;             // floats over the bottom of the pad / of the pocket cards
        r.set(120, by, 1120, by + h);
        p.setStyle(Paint.Style.FILL); p.setColor(0x55000000);
        c.drawRoundRect(new RectF(124, by + 8, 1124, by + 8 + h), 20, 20, p);
        p.setColor(combineOk ? 0xFFDDF3D0 : 0xFFFFE0D8);
        c.drawRoundRect(r, 20, 20, p);
        p.setStyle(Paint.Style.STROKE); p.setStrokeWidth(4); p.setColor(combineOk ? KELP_DK : RED);
        c.drawRoundRect(r, 20, 20, p);
        p.setStyle(Paint.Style.FILL);
        float ty = by + 24 + 16;
        text(c, combineOk ? "COMBINED" : "NO LUCK", 144, ty - 2, 20, combineOk ? KELP_DK : RED, true, Paint.Align.LEFT);
        for (String s : lines) { text(c, s, 1100, ty + 4, 28, INK, true, Paint.Align.RIGHT); ty += 34; }
    }

    // ------------------------------------------------------------------ tabs and pages

    private void drawTabs(Canvas c, int shown) {
        float w = 206, gap = 8;
        for (int i = 0; i < 4; i++) {
            float x = 40 + i * (w + gap);
            Hit hh = new Hit(x, TAB_Y, x + w, TAB_Y + TAB_H + 6, "tab:" + i);
            hits.add(hh);
            boolean on = shown == i, pr = hh == down;
            r.set(x, TAB_Y, x + w, TAB_Y + TAB_H);
            p.setStyle(Paint.Style.FILL);
            p.setColor(on ? GOLD : (pr ? WOOD_DK : WOOD));
            c.drawRoundRect(r, 24, 24, p);
            p.setStyle(Paint.Style.STROKE); p.setStrokeWidth(4); p.setColor(WOOD_EDGE);
            c.drawRoundRect(r, 24, 24, p);
            text(c, TABS[i], x + w / 2, TAB_Y + 38, 26, on ? INK : 0xFFF3E3C0, true, Paint.Align.CENTER);
        }
        // REVEAL: the game shows its hidden hot spots for a moment (lane e implements the 'reveal' command)
        chip(c, 40 + 4 * (w + gap) + 6, TAB_Y, 1200, TAB_Y + TAB_H, "REVEAL", "reveal", !(dialogue || cutscene));
        p.setStyle(Paint.Style.FILL);
    }

    private static final int GREY = 0xFF8A9AA0;

    private void drawMap(Canvas c, float top) {
        float l = 40, rr = 1200, b = 944;
        plank(c, l, top, rr, b, false);
        float sc = 1.28f, mw = 640 * sc, mh = 480 * sc;
        float ox = l + 26, oy = top + (b - top - mh) / 2;
        // the chart: sand paper with waves, the map scene's own layout (640x480)
        r.set(ox, oy, ox + mw, oy + mh);
        p.setStyle(Paint.Style.FILL); p.setColor(0xFFF2E3B0);
        c.drawRoundRect(r, 26, 26, p);
        c.save();
        c.clipRect(r);
        p.setStyle(Paint.Style.STROKE); p.setStrokeWidth(3); p.setColor(0x2236A6C4);
        for (float wy = oy + 30; wy < oy + mh; wy += 46) {
            path.reset(); path.moveTo(ox, wy);
            for (float wx = ox; wx < ox + mw; wx += 40) path.lineTo(wx + 20, wy + ((int) ((wx - ox) / 40) % 2 == 0 ? -8 : 8));
            c.drawPath(path, p);
        }
        c.restore();
        p.setStyle(Paint.Style.STROKE); p.setStrokeWidth(5); p.setColor(PAPER_EDGE);
        c.drawRoundRect(r, 26, 26, p);
        p.setStyle(Paint.Style.FILL);
        long now = SystemClock.uptimeMillis();
        if (areas.isEmpty()) text(c, "No chart for this place", ox + mw / 2, oy + mh / 2, 34, INK, true, Paint.Align.CENTER);
        int open = 0, seenN = 0;
        for (Area a : areas) {
            float cx = ox + (a.x + a.w / 2f) * sc, cy = oy + (a.y + a.h / 2f) * sc - 14;
            float rad = Math.max(44, Math.min(78, Math.min(a.w, a.h) * sc * 0.30f));
            boolean go = a.ok && !mapBusy && !a.here;
            if (a.ok) open++;
            if (a.visited) seenN++;
            int fill = a.here ? RED : (!a.ok ? GREY : (a.visited ? TEAL : GOLD));
            String cmd = go ? "travel:" + a.cmd
                    : "note:" + (a.here ? "You are here." : (!a.ok ? "Not open yet - keep exploring." : "Finish what you are doing first."));
            Hit h = new Hit(cx - rad - 12, cy - rad - 12, cx + rad + 12, cy + rad + 74, cmd);
            hits.add(h);
            boolean pr = h == down;
            p.setStyle(Paint.Style.FILL); p.setColor(0x55000000);
            c.drawCircle(cx + 4, cy + 7, rad, p);
            p.setColor(pr ? GOLD_DK : fill);
            c.drawCircle(cx, cy, rad, p);
            p.setColor(0x33FFFFFF);
            c.drawCircle(cx - rad * 0.3f, cy - rad * 0.35f, rad * 0.5f, p);
            p.setStyle(Paint.Style.STROKE); p.setStrokeWidth(6); p.setColor(WOOD_EDGE);
            c.drawCircle(cx, cy, rad, p);
            if (go) {                                   // a pulsing ring: you can go here
                p.setStrokeWidth(5); p.setColor(0xCCFFFFFF);
                c.drawCircle(cx, cy, rad + 8 + (float) Math.sin(now / 300.0) * 3, p);
            }
            p.setStyle(Paint.Style.FILL);
            String glyph = a.here ? "HERE" : (!a.ok ? "?" : (a.visited ? "GO" : "NEW"));
            text(c, glyph, cx, cy + (glyph.length() == 1 ? 18 : 10), glyph.length() == 1 ? 52 : 30, 0xFFFFFFFF, true, Paint.Align.CENTER);
            String label = a.name.length() > 0 ? a.name : "Not open yet";
            List<String> nm = wrap(label, 24, 230, 2, true);
            float pw = 0;
            for (String s : nm) pw = Math.max(pw, p.measureText(s));
            pw = Math.min(250, pw + 28);
            float ph = nm.size() * 28 + 14, py = cy + rad + 10;
            float lx = Math.max(ox + 8, Math.min(ox + mw - pw - 8, cx - pw / 2));
            r.set(lx, py, lx + pw, py + ph);
            p.setStyle(Paint.Style.FILL); p.setColor(a.ok || a.visited ? PAPER : 0xCCE0E0E0);
            c.drawRoundRect(r, 16, 16, p);
            p.setStyle(Paint.Style.STROKE); p.setStrokeWidth(3); p.setColor(WOOD_EDGE);
            c.drawRoundRect(r, 16, 16, p);
            float ny = py + 28;
            for (String s : nm) { text(c, s, lx + pw / 2, ny, 24, a.ok || a.visited ? INK : 0xFF6A767A, true, Paint.Align.CENTER); ny += 28; }
        }
        // the side column: legend and the note
        float cx0 = ox + mw + 26, y = top + 56;
        text(c, "CHART", cx0, y, 30, GOLD, true, Paint.Align.LEFT);
        y += 22;
        String[] names = { "You are here", "New place", "Been there", "Not open yet" };
        int[] cols = { RED, GOLD, TEAL, GREY };
        for (int i = 0; i < 4; i++) {
            y += 42;
            p.setStyle(Paint.Style.FILL); p.setColor(cols[i]);
            c.drawCircle(cx0 + 16, y - 9, 15, p);
            p.setStyle(Paint.Style.STROKE); p.setStrokeWidth(3); p.setColor(WOOD_EDGE);
            c.drawCircle(cx0 + 16, y - 9, 15, p);
            text(c, names[i], cx0 + 44, y, 24, 0xFFFFFFFF, false, Paint.Align.LEFT);
        }
        y += 56;
        text(c, seenN + " of " + areas.size() + " places seen", cx0, y, 22, 0xFFF3E3C0, false, Paint.Align.LEFT);
        y += 40;
        List<String> how = wrap("Tap a place that is open to go there, as with the map button in the game.", 22, 1176 - cx0, 5, false);
        for (String s : how) { text(c, s, cx0, y, 22, 0xFFF3E3C0, false, Paint.Align.LEFT); y += 28; }
        String msg = mapBusy ? "Finish what you are doing first." : (now < mapNoteUntil ? mapNote : "");
        if (msg.length() > 0) {
            y += 14;
            List<String> ml = wrap(msg, 24, 1176 - cx0 - 28, 3, true);
            float mh2 = ml.size() * 30 + 22;
            r.set(cx0, y, 1180, y + mh2);
            p.setStyle(Paint.Style.FILL); p.setColor(PAPER);
            c.drawRoundRect(r, 16, 16, p);
            p.setStyle(Paint.Style.STROKE); p.setStrokeWidth(3); p.setColor(RED);
            c.drawRoundRect(r, 16, 16, p);
            float ty = y + 32;
            for (String s : ml) { text(c, s, cx0 + 14, ty, 24, INK, true, Paint.Align.LEFT); ty += 30; }
        }
        p.setStyle(Paint.Style.FILL);
    }

    private void drawTalk(Canvas c, float top) {
        float l = 40, rr = 1200, b = 944;
        plank(c, l, top, rr, b, false);
        text(c, "WHAT WAS SAID", 66, top + 46, 28, GOLD, true, Paint.Align.LEFT);
        text(c, talk.size() + (talk.size() == 1 ? " line" : " lines"), rr - 30, top + 46, 24, 0xFFF3E3C0, false, Paint.Align.RIGHT);
        listRect.set(60, top + 64, rr - 20, b - 20);
        float vw = listRect.width(), vh = listRect.height();
        if (talk.isEmpty()) {
            text(c, "Nobody has said anything yet.", (l + rr) / 2, top + 300, 34, 0xFFFFFFFF, true, Paint.Align.CENTER);
            text(c, "The conversations you have in the game collect here.", (l + rr) / 2, top + 350, 26, 0xFFF3E3C0, false, Paint.Align.CENTER);
            return;
        }
        float textW = vw - 40 - 120;
        int n = talk.size();
        float[] hgt = new float[n];
        List<List<String>> wrapped = new ArrayList<List<String>>();
        float total = 0;
        for (int i = 0; i < n; i++) {
            List<String> ls = wrap(talk.get(i).text, 28, textW, 6, false);
            wrapped.add(ls);
            hgt[i] = Math.max(96, 54 + ls.size() * 34 + 14);
            total += hgt[i] + 12;
        }
        float maxScroll = Math.max(0, total - 12 - vh);
        if (n != talkSeen) { if (talkStick) talkScroll = maxScroll; talkSeen = n; }     // a new line: stay at the bottom unless the player scrolled up
        if (talkStick) talkScroll = maxScroll;
        talkScroll = Math.max(0, Math.min(maxScroll, talkScroll));
        if (talkScroll >= maxScroll - 2) talkStick = true;
        c.save();
        c.clipRect(listRect);
        float y = listRect.top - talkScroll + (total - 12 < vh ? vh - (total - 12) : 0);     // a short log sits at the bottom, newest last
        boolean canPlay = !dialogue;
        for (int i = 0; i < n; i++) {
            float h = hgt[i];
            if (y + h >= listRect.top && y <= listRect.bottom) {
                Line ln = talk.get(i);
                r.set(listRect.left, y, listRect.right - 14, y + h);
                p.setStyle(Paint.Style.FILL); p.setColor(0x44000000);
                c.drawRoundRect(r, 18, 18, p);
                text(c, ln.speaker.length() > 0 ? ln.speaker : "...", listRect.left + 20, y + 36, 26, GOLD, true, Paint.Align.LEFT);
                float ty = y + 36 + 34;
                for (String s : wrapped.get(i)) { text(c, s, listRect.left + 20, ty - 4, 28, 0xFFFFFFFF, false, Paint.Align.LEFT); ty += 34; }
                // the replay-voice button
                float bx = listRect.right - 14 - 62, by = y + h / 2;
                boolean live = ln.voice && canPlay;
                if (live) {
                    float tt = Math.max(by - 40, listRect.top), bb = Math.min(by + 40, listRect.bottom);
                    if (bb > tt) { Hit hh = new Hit(bx - 46, tt, bx + 46, bb, "voice:" + ln.id); hits.add(hh); }
                }
                boolean pr = live && down != null && down.cmd.equals("voice:" + ln.id);
                p.setStyle(Paint.Style.FILL); p.setColor(live ? (pr ? GOLD_DK : TEAL) : 0x55FFFFFF);
                c.drawCircle(bx, by, 34, p);
                p.setStyle(Paint.Style.STROKE); p.setStrokeWidth(4); p.setColor(live ? WOOD_EDGE : 0x55000000);
                c.drawCircle(bx, by, 34, p);
                p.setStyle(Paint.Style.FILL); p.setColor(live ? 0xFFFFFFFF : 0x88FFFFFF);        // a speaker: cone and two waves
                path.reset(); path.moveTo(bx - 16, by - 7); path.lineTo(bx - 7, by - 7); path.lineTo(bx + 3, by - 16);
                path.lineTo(bx + 3, by + 16); path.lineTo(bx - 7, by + 7); path.lineTo(bx - 16, by + 7); path.close();
                c.drawPath(path, p);
                p.setStyle(Paint.Style.STROKE); p.setStrokeWidth(3);
                c.drawLine(bx + 10, by - 6, bx + 14, by - 10, p); c.drawLine(bx + 12, by, bx + 18, by, p); c.drawLine(bx + 10, by + 6, bx + 14, by + 10, p);
                p.setStyle(Paint.Style.FILL);
            }
            y += h + 12;
        }
        c.restore();
        if (maxScroll > 0) {                                           // scroll bar
            float th = Math.max(50, vh * vh / (total - 12));
            float ty = listRect.top + (vh - th) * (talkScroll / maxScroll);
            r.set(listRect.right - 8, ty, listRect.right - 2, ty + th);
            p.setStyle(Paint.Style.FILL); p.setColor(0x99FFFFFF);
            c.drawRoundRect(r, 3, 3, p);
        }
    }

    private void drawGoals(Canvas c, float top) {
        float y = drawTicket(c, top, "WHAT NEXT", hint.length() > 0 ? hint : "Nothing to suggest yet. Talk to everyone and look around.", 3) + 14;
        float l = 40, rr = 1200, b = 944;
        plank(c, l, y, rr, b, false);
        text(c, chapterTitle.length() > 0 ? chapterTitle.toUpperCase() : "CHAPTER", 66, y + 48, 28, GOLD, true, Paint.Align.LEFT);
        // overall bar
        float frac = goalTotal > 0 ? Math.min(1f, goalSeen / (float) goalTotal) : 0;
        text(c, goalSeen + " of " + goalTotal + " scenes seen", rr - 30, y + 48, 26, 0xFFFFFFFF, true, Paint.Align.RIGHT);
        bar(c, 66, y + 66, rr - 30, y + 98, frac);
        text(c, "Story scripts seen: " + goalScripts, 66, y + 138, 24, 0xFFF3E3C0, false, Paint.Align.LEFT);
        float ry = y + 168;
        int rows = placeNames.size();
        float rh = Math.min(64, (b - 24 - ry) / Math.max(1, rows));
        for (int i = 0; i < rows; i++) {
            int[] v = placeCounts.get(i);
            text(c, placeNames.get(i), 66, ry + rh * 0.62f, 26, 0xFFFFFFFF, true, Paint.Align.LEFT);
            float f2 = v[1] > 0 ? Math.min(1f, v[0] / (float) v[1]) : 0;
            bar(c, 560, ry + rh * 0.22f, rr - 120, ry + rh * 0.22f + 26, f2);
            text(c, v[0] + "/" + v[1], rr - 30, ry + rh * 0.62f, 24, v[0] >= v[1] && v[1] > 0 ? GOLD : 0xFFF3E3C0, true, Paint.Align.RIGHT);
            ry += rh;
        }
        if (rows == 0) text(c, "Nothing to count in this place yet.", 66, y + 230, 26, 0xFFF3E3C0, false, Paint.Align.LEFT);
        p.setStyle(Paint.Style.FILL);
    }

    private void bar(Canvas c, float l, float t, float rr, float b, float frac) {
        r.set(l, t, rr, b);
        p.setStyle(Paint.Style.FILL); p.setColor(0x55000000);
        c.drawRoundRect(r, (b - t) / 2, (b - t) / 2, p);
        if (frac > 0) {
            r.set(l, t, l + Math.max(b - t, (rr - l) * frac), b);
            p.setColor(frac >= 1f ? GOLD : KELP);
            c.drawRoundRect(r, (b - t) / 2, (b - t) / 2, p);
        }
        r.set(l, t, rr, b);
        p.setStyle(Paint.Style.STROKE); p.setStrokeWidth(3); p.setColor(WOOD_EDGE);
        c.drawRoundRect(r, (b - t) / 2, (b - t) / 2, p);
        p.setStyle(Paint.Style.FILL);
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
