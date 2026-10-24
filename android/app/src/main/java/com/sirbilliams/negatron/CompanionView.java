package com.sirbilliams.negatron;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.LinearGradient;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.RadialGradient;
import android.graphics.RectF;
import android.graphics.Shader;
import android.graphics.Typeface;
import android.os.SystemClock;
import android.view.View;

import org.json.JSONArray;
import org.json.JSONObject;

import java.util.HashMap;
import java.util.Random;

/**
 * port: draws the companion snapshot (see game/src/port/Companion.h for the fields) as a retro
 * lab gadget: brushed stainless-steel plates with screws, green LED indicators and segment
 * readouts, push buttons that light up, and the level map in a recessed green CRT window with a
 * radar sweep. All drawn here in code (no artwork in the repo); the pictures of the level, the
 * gadgets, pickups and neutrons come from the player's own game files at run time (the map is
 * rasterised by the native side, the icons are the game's own canvases).
 *
 * Laid out for the Thor's 1240 x 1080 bottom screen (scaled to whatever size it gets). The
 * static chrome is drawn once per size into a bitmap; a snapshot is parsed once (setSnapshot)
 * and drawing then allocates nothing. Fonts: the system's sans-serif / monospace.
 */
final class CompanionView extends View {
    /** Where the map picture and icons come from (the JNI on Android, PNG files on the desktop). */
    interface Source {
        /** [gen, w, h, ARGB...] or null. */
        int[] map();
        /** [w, h, ARGB...] or null. */
        int[] icon(int id);
    }

    // palette: steel, LEDs, CRT phosphor; red / yellow only as small accents
    private static final int STEEL_HI = Color.rgb(222, 226, 230);
    private static final int STEEL_MID = Color.rgb(176, 182, 189);
    private static final int STEEL_LO = Color.rgb(128, 134, 142);
    private static final int ENGRAVE = Color.rgb(70, 74, 80);
    private static final int LCD_BG = Color.rgb(8, 22, 12);
    private static final int LED_ON = Color.rgb(96, 255, 110);
    private static final int LED_DIM = Color.rgb(28, 88, 36);
    private static final int LED_GHOST = Color.rgb(18, 44, 22);
    private static final int CRT_BG = Color.rgb(4, 16, 8);
    private static final int PHOSPHOR = Color.rgb(110, 255, 140);
    private static final int WARN_RED = Color.rgb(255, 70, 60);
    private static final int WARN_YEL = Color.rgb(255, 214, 60);

    // map zoom: world units per layout pixel
    private static final float ZOOM_NEAR = 5f, ZOOM_FAR = 12f;

    // layout (Thor pixels)
    private static final float LW = 1240, LH = 1080;
    private static final RectF R_HEAD = new RectF(20, 20, 424, 104);
    private static final RectF R_GAUGE = new RectF(20, 116, 424, 326);
    private static final RectF R_LEVEL = new RectF(20, 338, 424, 570);
    private static final RectF R_SHEET = new RectF(20, 582, 424, 1060);
    private static final RectF R_MAP = new RectF(440, 20, 1220, 1060);
    private static final float BEZEL = 22;
    private static final float DIAL_X = 20 + 112, DIAL_Y = 116 + 98, DIAL_R = 80;

    private final Paint mFill = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint mStroke = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint mText = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint mBmp = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.FILTER_BITMAP_FLAG);
    private final Path mPath = new Path();
    private final RectF mRect = new RectF();
    private final RectF mRect2 = new RectF();
    private final Typeface mBold = Typeface.create(Typeface.SANS_SERIF, Typeface.BOLD);
    private final Typeface mMono = Typeface.create(Typeface.MONOSPACE, Typeface.BOLD);

    private Source mSource;
    private Bitmap mChrome;                 // the static plates, drawn once per size
    private int mChromeW, mChromeH;
    private Bitmap mMap;                    // the level map, green-tinted
    private int mMapGen;
    private final HashMap<Integer, Bitmap> mIcons = new HashMap<>();
    private boolean mFar;
    private String mLast;

    // --- the parsed snapshot (setSnapshot), read by onDraw
    private boolean mHave;
    private String mLevel = "", mMode = "", mScore = "0", mEnergyText = "0", mTime = "";
    private boolean mCut, mPlaying;
    private float mEnergy = -1;
    private boolean mBoss;
    private float mBossHp;
    private String mBossName = "";
    // the level's collectables: neutrons, checkpoints, then the pickups by kind (icon, got, all)
    private int[] mColIcon = new int[0], mColGot = new int[0], mColAll = new int[0];
    private String[] mColText = new String[0];
    private int mNeutronIcon = -1, mEnergyIcon = -1;
    private int mGadget, mGadgetIcon = -1;
    private float mGadgetCharge = -1;
    // the inventory sheet: icon, count, owned, grid column, grid row
    private int[] mShIcon = new int[0], mShCount = new int[0], mShCol = new int[0], mShRow = new int[0];
    private boolean[] mShOwned = new boolean[0];
    private String[] mShText = new String[0];
    private int mShCols = 1, mShRows = 1, mShOwnedCount;
    private String mShSummary = "";
    private final HashMap<Integer, Bitmap> mIconsDim = new HashMap<>();
    private String[] mCaption = new String[0];
    private int mMapW, mMapH;
    private float mMapS = 1;
    private boolean mJimmy, mGoddard;
    private float mJx, mJy, mHx, mHy;
    private char[] mPtKind = new char[0];
    private float[] mPtX = new float[0], mPtY = new float[0];
    private int[] mPtIcon = new int[0];
    private String[] mPtLabel = new String[0];
    // the Vox world picker (port/VoxPicker.h): cards left to right, the lit one
    private String[] mPickName = new String[0];
    private int[] mPickWire = new int[0], mPickColour = new int[0];
    private int mPickSel = -1;

    CompanionView(Context c) {
        super(c);
        setBackgroundColor(STEEL_MID);
    }

    void setSource(Source s) { mSource = s; }

    void setZoomFar(boolean far) {
        mFar = far;
        invalidate();
    }

    /** Called with every poll (10 a second): the sweep moves even when nothing else does. */
    void tick() { invalidate(); }

    /** New snapshot (JSON) from the poll; parsed only when it changed. */
    void setSnapshot(String json) {
        if (json == null || json.isEmpty() || json.equals(mLast)) return;
        mLast = json;
        try {
            parse(new JSONObject(json));
        } catch (Exception e) {
            return;
        }
        invalidate();
    }

    // ------------------------------------------------------------------ parsing

    private void parse(JSONObject d) {
        mHave = true;
        String level = d.optString("level", "");
        int dot = level.lastIndexOf('.');
        // the game's own level name (its Load/Save page's), else the file name
        String name = d.optString("name", "");
        mLevel = !name.isEmpty() ? name : (dot > 0 ? level.substring(0, dot) : level).toUpperCase();
        int mode = d.optInt("mode", -1);
        mMode = modeName(mode);
        mCut = d.optInt("cut", 0) == 1;
        mPlaying = mode == 5;
        mEnergy = (float) (d.optDouble("energy", -1) / d.optDouble("energy_max", 100));
        mEnergyText = String.valueOf(Math.round(d.optDouble("energy", 0)));
        mEnergyIcon = d.optInt("energy_icon", -1);
        mScore = String.valueOf(d.optInt("score", 0));
        int secs = (int) d.optDouble("time", 0);
        mTime = (secs / 60 < 10 ? "0" : "") + (secs / 60) + ":" + (secs % 60 < 10 ? "0" : "") + (secs % 60);

        JSONObject boss = d.optJSONObject("boss");
        mBoss = boss != null && (boss.optInt("bar", 0) == 1 || boss.optDouble("hp", 1) < 1);
        if (boss != null) {
            mBossHp = (float) boss.optDouble("hp", 0);
            mBossName = boss.optString("name", "Boss").toUpperCase();
        }

        // collectables of this level: neutrons, checkpoints, then each kind of pickup
        JSONArray ne = d.optJSONArray("neutrons"), ck = d.optJSONArray("checkpoints"), col = d.optJSONArray("collect");
        mNeutronIcon = ne != null ? ne.optInt(2, -1) : -1;
        int n = 0;
        if (ne != null && ne.optInt(1, 0) > 0) n++;
        if (ck != null && ck.optInt(1, 0) > 0) n++;
        n += col != null ? col.length() : 0;
        mColIcon = new int[n];
        mColGot = new int[n];
        mColAll = new int[n];
        mColText = new String[n];
        int k = 0;
        if (ne != null && ne.optInt(1, 0) > 0) {
            mColIcon[k] = mEnergyIcon >= 0 ? mEnergyIcon : mNeutronIcon;
            mColGot[k] = ne.optInt(0, 0);
            mColAll[k++] = ne.optInt(1, 0);
        }
        if (ck != null && ck.optInt(1, 0) > 0) {
            mColIcon[k] = ck.optInt(2, -1) >= 0 ? ck.optInt(2, -1) : -2;      // -2: a drawn flag
            mColGot[k] = ck.optInt(0, 0);
            mColAll[k++] = ck.optInt(1, 0);
        }
        for (int i = 0; col != null && i < col.length(); i++) {
            JSONArray e = col.optJSONArray(i);
            if (e == null) continue;
            mColIcon[k] = e.optInt(3, -1);
            mColGot[k] = e.optInt(1, 0);
            mColAll[k++] = e.optInt(2, 0);
        }
        for (int i = 0; i < n; i++) {
            mColText[i] = mColGot[i] + "/" + mColAll[i];
            if (mColIcon[i] >= 0) fetchIcon(mColIcon[i]);
        }

        // the current gadget and its HUD bar
        mGadget = d.optInt("gadget", 0);
        mGadgetIcon = d.optInt("gadget_icon", -1);
        mGadgetCharge = (float) d.optDouble("gadget_charge", -1);
        if (mGadgetIcon >= 0) fetchIcon(mGadgetIcon);

        // the inventory sheet, laid out on the game's own grid (its 640 x 480 page positions)
        JSONArray sh = d.optJSONArray("sheet");
        n = sh != null ? sh.length() : 0;
        if (mShIcon.length != n) {
            mShIcon = new int[n];
            mShCount = new int[n];
            mShCol = new int[n];
            mShRow = new int[n];
            mShOwned = new boolean[n];
            mShText = new String[n];
        }
        // the game's own order on its page (rows of 40 px, left to right), in a compact grid
        long[] order = new long[n];
        mShOwnedCount = 0;
        for (int i = 0; i < n; i++) {
            JSONArray e = sh.optJSONArray(i);
            if (e == null) continue;
            mShIcon[i] = e.optInt(2, -1);
            mShCount[i] = e.optInt(1, 0);
            mShOwned[i] = e.optInt(3, 0) != 0;
            if (mShOwned[i]) mShOwnedCount++;
            mShText[i] = mShCount[i] > 1 ? String.valueOf(mShCount[i]) : null;
            order[i] = ((long) (e.optInt(5, 0) / 40) << 32) | ((long) e.optInt(4, 0) << 8) | i;
            if (mShIcon[i] >= 0) fetchIcon(mShIcon[i]);
        }
        java.util.Arrays.sort(order);
        mShCols = 6;
        mShRows = Math.max(1, (n + mShCols - 1) / mShCols);
        for (int k2 = 0; k2 < n; k2++) {
            int i = (int) (order[k2] & 0xff);
            mShCol[i] = k2 % mShCols;
            mShRow[i] = k2 / mShCols;
        }
        mShSummary = mShOwnedCount + "/" + n;

        String cap = d.optString("caption", "");
        mCaption = cap.isEmpty() ? new String[0] : cap.split("\n");

        JSONObject map = d.optJSONObject("map");
        int gen = map != null ? map.optInt("gen", 0) : 0;
        mMapW = map != null ? map.optInt("w", 0) : 0;
        mMapH = map != null ? map.optInt("h", 0) : 0;
        mMapS = map != null ? (float) map.optDouble("s", 1) : 1;
        if (gen != mMapGen) fetchMap(gen);

        JSONArray j = d.optJSONArray("jimmy");
        mJimmy = j != null && j.length() >= 4;
        if (mJimmy) {
            mJx = (float) j.optDouble(0, 0);
            mJy = (float) j.optDouble(1, 0);
            mHx = (float) j.optDouble(2, 0);
            mHy = (float) j.optDouble(3, -1);
        }

        JSONArray p = d.optJSONArray("pts");
        n = p != null ? p.length() : 0;
        mPtKind = new char[n];
        mPtX = new float[n];
        mPtY = new float[n];
        mPtIcon = new int[n];
        mPtLabel = new String[n];
        for (int i = 0; i < n; i++) {
            JSONArray e = p.optJSONArray(i);
            if (e == null) continue;
            String kind = e.optString(0, "?");
            mPtKind[i] = kind.isEmpty() ? '?' : kind.charAt(0);
            mPtX[i] = (float) e.optDouble(1, 0);
            mPtY[i] = (float) e.optDouble(2, 0);
            Object extra = e.opt(3);
            mPtIcon[i] = extra instanceof Number ? ((Number) extra).intValue() : -1;
            if (extra instanceof String) {
                String s = (String) extra;
                int dt = s.lastIndexOf('.');
                mPtLabel[i] = dt > 0 ? s.substring(0, dt).toUpperCase() : s;   // a level name as it is, a file name in caps
            }
            if (mPtIcon[i] >= 0) fetchIcon(mPtIcon[i]);
        }
        mGoddard = false;
        for (int i = 0; i < n; i++) if (mPtKind[i] == 'g') mGoddard = true;
        JSONObject pk = d.optJSONObject("picker");
        JSONArray cards = pk != null ? pk.optJSONArray("cards") : null;
        n = cards != null ? cards.length() : 0;
        if (mPickName.length != n) {
            mPickName = new String[n];
            mPickWire = new int[n];
            mPickColour = new int[n];
        }
        mPickSel = pk != null ? pk.optInt("sel", -1) : -1;
        for (int i = 0; i < n; i++) {
            JSONArray e = cards.optJSONArray(i);
            mPickName[i] = e != null ? e.optString(0, "") : "";
            mPickWire[i] = e != null ? e.optInt(1, -1) : -1;
            mPickColour[i] = e != null ? e.optInt(2, -1) : -1;
            if (mPickWire[i] >= 0) fetchIcon(mPickWire[i]);
            if (mPickColour[i] >= 0) fetchIcon(mPickColour[i]);
        }
        if (mNeutronIcon >= 0) fetchIcon(mNeutronIcon);
        if (mEnergyIcon >= 0) fetchIcon(mEnergyIcon);
    }

    private void fetchMap(int gen) {
        if (mSource == null) return;
        int[] a = gen > 0 ? mSource.map() : null;
        if (a == null || a.length < 3 || a[0] != gen) {
            if (gen == 0) { mMap = null; mMapGen = 0; }
            return;                                         // (asked again with the next snapshot)
        }
        int w = a[1], h = a[2];
        if (w <= 0 || h <= 0 || a.length < 3 + w * h) return;
        int[] px = new int[w * h];
        // green phosphor: brightness to green, a little of the colour kept
        for (int i = 0; i < px.length; i++) {
            int c = a[3 + i];
            int al = c >>> 24;
            if (al == 0) { px[i] = 0; continue; }
            int r = (c >> 16) & 0xff, gg = (c >> 8) & 0xff, b = c & 0xff;
            int l = (r * 30 + gg * 59 + b * 11) / 100;
            int pr = (l * 22 + r * 18) / 100, pg = Math.min(255, (l * 80 + gg * 14) / 100), pb = (l * 30 + b * 14) / 100;
            px[i] = (al << 24) | (pr << 16) | (pg << 8) | pb;
        }
        if (mMap != null) mMap.recycle();
        mMap = Bitmap.createBitmap(px, w, h, Bitmap.Config.ARGB_8888);
        mMapGen = gen;
    }

    private void fetchIcon(int id) {
        if (mSource == null || mIcons.containsKey(id)) return;
        int[] a = mSource.icon(id);
        if (a == null || a.length < 2 || a.length < 2 + a[0] * a[1]) return;
        int[] px = new int[a[0] * a[1]];
        System.arraycopy(a, 2, px, 0, px.length);
        mIcons.put(id, Bitmap.createBitmap(px, a[0], a[1], Bitmap.Config.ARGB_8888));
        // and a dark silhouette for the sheet's items Jimmy doesn't have
        for (int i = 0; i < px.length; i++) {
            int c = px[i], al = c >>> 24;
            int l = (((c >> 16) & 0xff) * 30 + ((c >> 8) & 0xff) * 59 + (c & 0xff) * 11) / 100;
            px[i] = (al * 3 / 4 << 24) | ((l / 6) << 16) | ((l / 4 + 14) << 8) | (l / 6);
        }
        mIconsDim.put(id, Bitmap.createBitmap(px, a[0], a[1], Bitmap.Config.ARGB_8888));
    }

    // ------------------------------------------------------------------ drawing

    @Override
    protected void onDraw(Canvas c) {
        int w = getWidth(), h = getHeight();
        if (w <= 0 || h <= 0) return;
        float u = Math.min(w / LW, h / LH);
        float ox = (w - LW * u) / 2, oy = (h - LH * u) / 2;
        if (mChrome == null || mChromeW != w || mChromeH != h) buildChrome(w, h, u, ox, oy);
        c.drawBitmap(mChrome, 0, 0, null);
        c.save();
        c.translate(ox, oy);
        c.scale(u, u);
        if (!mHave) {
            lcdText(c, "WAITING FOR THE GAME", R_HEAD.left + 30, R_HEAD.top + 58, 28, LED_ON);
        } else {
            drawHeader(c);
            drawGauges(c);
            drawLevel(c);
            drawSheet(c);
        }
        if (mPickName.length > 0) drawPicker(c);
        else drawMap(c);
        c.restore();
    }

    private void drawHeader(Canvas c) {
        float ts = Math.min(30, 160f / (0.62f * Math.max(1, mLevel.length())));   // long names smaller
        lcdText(c, mLevel, R_HEAD.left + 30, R_HEAD.top + 44 + ts * 0.35f, ts, LED_ON);
        float lx = R_HEAD.right - 190;
        led(c, lx, R_HEAD.top + 32, 8, mCut ? 2 : 1, mCut ? WARN_YEL : LED_ON);
        lcdText(c, mMode, lx + 16, R_HEAD.top + 39, 18, mCut ? WARN_YEL : LED_ON);
        if (mPlaying || !mTime.isEmpty()) segText(c, mTime, lx + 14, R_HEAD.top + 46, 20, 0, PHOSPHOR);
    }

    private void drawGauges(Canvas c) {
        // energy dial: 240 degrees, zones as the game's own bar (green from 70, yellow from 30)
        float cx = DIAL_X, cy = DIAL_Y, r = DIAL_R;
        dial(c, cx, cy, r, mEnergy);
        segText(c, mEnergyText, cx - 32, cy + 24, 26, 3, energyColour(mEnergy * 100));
        engraved(c, "ENERGY", cx, R_GAUGE.bottom - 12, 18, true);
        Bitmap hud = mEnergyIcon >= 0 ? mIcons.get(mEnergyIcon) : null;
        if (hud != null) drawIcon(c, hud, cx, cy - 36, 40);

        float x = R_GAUGE.left + 222, y = R_GAUGE.top + 22;
        engraved(c, "SCORE", x + 2, y + 2, 16, false);
        lcd(c, x, y + 8, 168, 50);
        segText(c, mScore, x + 10, y + 16, 34, 6, LED_ON);
        y += 74;
        if (mBoss) {                                            // the boss's health bar
            engraved(c, mBossName, x + 2, y + 2, 16, false);
            lcd(c, x, y + 8, 168, 40);
            float f = Math.max(0, Math.min(1, mBossHp));
            mFill.setShader(null);
            mFill.setColor(WARN_RED);
            mRect.set(x + 8, y + 16, x + 8 + 152 * f, y + 40);
            c.drawRoundRect(mRect, 6, 6, mFill);
            for (int i = 1; i < 10; i++) {
                mFill.setColor(LCD_BG);
                c.drawRect(x + 8 + 15.2f * i - 1.5f, y + 16, x + 8 + 15.2f * i + 1.5f, y + 40, mFill);
            }
            return;
        }
        engraved(c, "GADGET", x + 2, y + 2, 16, false);
        float bx = x + 34, by = y + 50;
        Bitmap g = mGadgetIcon >= 0 ? mIcons.get(mGadgetIcon) : null;
        button(c, bx, by, 30, g != null ? 2 : 0);
        if (g != null) drawIcon(c, g, bx, by, 50);
        else lcdText(c, "-", bx - 6, by + 8, 22, LED_DIM);
        if (mGadgetCharge >= 0) {                               // the HUD's gadget bar (red when empty)
            float f = Math.max(0, Math.min(1, mGadgetCharge / 100f));
            lcd(c, x + 80, y + 30, 88, 40);
            mFill.setShader(null);
            mFill.setColor(f > 0 ? LED_ON : WARN_RED);
            mRect.set(x + 86, y + 36, x + 86 + 76 * Math.max(f, 0.04f), y + 64);
            c.drawRoundRect(mRect, 5, 5, mFill);
        }
        led(c, x + 88, y + 92, 6, mGoddard ? 2 : 0, LED_ON);       // Goddard in the level, on the map
        engraved(c, "GODDARD", x + 100, y + 98, 15, false);
    }

    /** This level's collectables: the game's picture, collected / all, lit when all are in. */
    private void drawLevel(Canvas c) {
        engraved(c, "THIS LEVEL", R_LEVEL.left + 26, R_LEVEL.top + 30, 18, false);
        float x0 = R_LEVEL.left + 22, y0 = R_LEVEL.top + 42;
        float tw = (R_LEVEL.width() - 44) / 3, th = 58;
        int max = 9;
        if (mColIcon.length == 0) lcdText(c, "NOTHING TO COLLECT", x0 + 6, y0 + 36, 20, LED_DIM);
        for (int i = 0; i < mColIcon.length && i < max; i++) {
            float tx = x0 + (i % 3) * tw, ty = y0 + (i / 3) * (th + 6);
            boolean all = mColAll[i] > 0 && mColGot[i] >= mColAll[i];
            lcd(c, tx, ty, tw - 8, th);
            float ix = tx + 28, iy = ty + th / 2;
            Bitmap b = mColIcon[i] >= 0 ? mIcons.get(mColIcon[i]) : null;
            if (b != null) drawIcon(c, b, ix - 2, iy, 46);
            else if (mColIcon[i] == -2) flag(c, ix, iy, 14, LED_ON);
            else star(c, ix, iy, 16, WARN_YEL);
            segText(c, mColText[i], tx + 52, ty + 19, 20, 0, all ? WARN_YEL : LED_ON);
            if (all) led(c, tx + tw - 18, ty + 10, 4, 2, LED_ON);
        }
    }

    /** The inventory sheet (the pickups page Jimmy opens): the game's own pictures on its own
     *  grid; lit when Jimmy has the item (with how many), a dark silhouette when not. */
    private void drawSheet(Canvas c) {
        engraved(c, "INVENTORY", R_SHEET.left + 26, R_SHEET.top + 30, 18, false);
        engraved(c, mShSummary, R_SHEET.right - 30, R_SHEET.top + 30, 18, false, true);
        float x0 = R_SHEET.left + 20, y0 = R_SHEET.top + 44;
        float cw = (R_SHEET.width() - 40) / mShCols, ch = (R_SHEET.bottom - 20 - y0) / mShRows;
        float size = Math.min(cw, ch) - 6;
        for (int i = 0; i < mShIcon.length; i++) {
            float cx = x0 + (mShCol[i] + 0.5f) * cw, cy = y0 + (mShRow[i] + 0.5f) * ch;
            mRect.set(cx - cw / 2 + 2, cy - ch / 2 + 2, cx + cw / 2 - 2, cy + ch / 2 - 2);
            mFill.setShader(null);
            mFill.setColor(mShOwned[i] ? Color.rgb(16, 44, 22) : LCD_BG);
            c.drawRoundRect(mRect, 8, 8, mFill);
            if (mShOwned[i]) {
                mStroke.setShader(null);
                mStroke.setStrokeWidth(2.5f);
                mStroke.setColor(LED_ON);
                c.drawRoundRect(mRect, 8, 8, mStroke);
            }
            Bitmap b = mShIcon[i] >= 0 ? (mShOwned[i] ? mIcons.get(mShIcon[i]) : mIconsDim.get(mShIcon[i])) : null;
            if (b != null) drawIcon(c, b, cx, cy, size);
            if (mShOwned[i] && mShText[i] != null) {
                mFill.setColor(Color.argb(200, 0, 0, 0));
                c.drawCircle(cx + cw / 2 - 13, cy + ch / 2 - 13, 11, mFill);
                mText.setTypeface(mMono);
                mText.setTextAlign(Paint.Align.CENTER);
                mText.setTextSize(16);
                mText.setColor(LED_ON);
                c.drawText(mShText[i], cx + cw / 2 - 13, cy + ch / 2 - 7, mText);
                mText.setTextAlign(Paint.Align.LEFT);
            }
        }
    }

    // --- the map window

    private void drawMap(Canvas c) {
        RectF crt = mRect2;
        crt.set(R_MAP.left + BEZEL, R_MAP.top + BEZEL, R_MAP.right - BEZEL, R_MAP.bottom - BEZEL);
        float cx = crt.centerX(), cy = crt.centerY();
        c.save();
        c.clipRect(crt);
        if (mMap == null || !mJimmy) {
            lcdText(c, mHave ? "NO SIGNAL" : "", cx - 70, cy, 30, LED_DIM);
        } else {
            float k = mMapS / (mFar ? ZOOM_FAR : ZOOM_NEAR);     // layout px per map px
            float left = cx - mJx * k, top = cy - mJy * k;
            if (mMapW == mMap.getWidth() && mMapH == mMap.getHeight()) {
                mRect.set(left, top, left + mMapW * k, top + mMapH * k);
                c.drawBitmap(mMap, null, mRect, mBmp);
            }
            float iconScale = mFar ? 0.7f : 1f;
            // points, then Jimmy on top
            for (int i = 0; i < mPtKind.length; i++) {
                float px = left + mPtX[i] * k, py = top + mPtY[i] * k;
                char kind = mPtKind[i];
                float margin = (kind == 'o' || kind == 'x' || kind == 'X') ? -12 : 40;   // marked on the edge when out
                boolean in = px > crt.left - margin && px < crt.right + margin && py > crt.top - margin && py < crt.bottom + margin;
                if (!in) {
                    if (kind == 'o' || kind == 'x' || kind == 'X') edgeMarker(c, crt, cx, cy, px, py, kind, mPtLabel[i]);
                    continue;
                }
                drawPoint(c, kind, px, py, mPtIcon[i], mPtLabel[i], iconScale);
            }
            jimmy(c, cx, cy);
        }
        // scanlines and the sweep
        mStroke.setShader(null);
        mStroke.setStrokeWidth(1.2f);
        mStroke.setColor(Color.argb(28, 0, 0, 0));
        for (float y = crt.top; y < crt.bottom; y += 4) c.drawLine(crt.left, y, crt.right, y, mStroke);
        float a = (SystemClock.uptimeMillis() % 6000L) / 6000f * 360f;
        float rr = (float) Math.hypot(crt.width(), crt.height()) / 2;
        mRect.set(cx - rr, cy - rr, cx + rr, cy + rr);
        mFill.setShader(null);
        for (int i = 0; i < 10; i++) {                                 // fading trail
            mFill.setColor(Color.argb(22 - i * 2, 110, 255, 140));
            c.drawArc(mRect, a - 90 - (i + 1) * 4, 4, true, mFill);
        }
        mStroke.setColor(Color.argb(150, 140, 255, 160));
        mStroke.setStrokeWidth(2.5f);
        double ar = Math.toRadians(a - 90);
        c.drawLine(cx, cy, cx + (float) Math.cos(ar) * rr, cy + (float) Math.sin(ar) * rr, mStroke);
        c.restore();

        // compass, zoom and scale bar on the glass
        lcdText(c, "N", cx - 9, crt.top + 34, 26, PHOSPHOR);
        mFill.setColor(PHOSPHOR);
        mPath.reset();
        mPath.moveTo(cx, crt.top + 6);
        mPath.lineTo(cx - 8, crt.top + 14);
        mPath.lineTo(cx + 8, crt.top + 14);
        mPath.close();
        c.drawPath(mPath, mFill);
        lcdText(c, mFar ? "ZOOM FAR" : "ZOOM NEAR", crt.left + 18, crt.bottom - 18, 20, PHOSPHOR);
        float bar = 1000f / (mFar ? ZOOM_FAR : ZOOM_NEAR);             // 1000 world units
        mStroke.setColor(PHOSPHOR);
        mStroke.setStrokeWidth(3);
        float bx = crt.right - 24 - bar, by = crt.bottom - 26;
        c.drawLine(bx, by, bx + bar, by, mStroke);
        c.drawLine(bx, by - 8, bx, by + 8, mStroke);
        c.drawLine(bx + bar, by - 8, bx + bar, by + 8, mStroke);
        lcdText(c, "1000", bx + bar / 2 - 24, by - 12, 18, PHOSPHOR);

        // the nearest open exit: an arrow towards it and the level it leads to (top left)
        int ex = -1;
        float best = Float.MAX_VALUE;
        for (int i = 0; mJimmy && i < mPtKind.length; i++) {
            if (mPtKind[i] != 'x') continue;
            float dx = mPtX[i] - mJx, dy = mPtY[i] - mJy, d2 = dx * dx + dy * dy;
            if (d2 < best) { best = d2; ex = i; }
        }
        if (ex >= 0) {
            float lx = crt.left + 16, ly = crt.top + 14;
            lcd(c, lx, ly, 250, 52);
            float ax = lx + 28, ay = ly + 26;
            float dx = mPtX[ex] - mJx, dy = mPtY[ex] - mJy, len = (float) Math.sqrt(best);
            if (len > 1e-3f) { dx /= len; dy /= len; } else { dx = 0; dy = -1; }
            mFill.setShader(null);
            mFill.setColor(LED_ON);
            mPath.reset();
            mPath.moveTo(ax + dx * 18, ay + dy * 18);
            mPath.lineTo(ax - dx * 12 - dy * 12, ay - dy * 12 + dx * 12);
            mPath.lineTo(ax - dx * 5, ay - dy * 5);
            mPath.lineTo(ax - dx * 12 + dy * 12, ay - dy * 12 - dx * 12);
            mPath.close();
            c.drawPath(mPath, mFill);
            lcdText(c, "EXIT", lx + 56, ly + 22, 15, LED_DIM);
            lcdText(c, mPtLabel[ex] != null ? mPtLabel[ex] : "", lx + 56, ly + 42, 20, LED_ON);
        }

        // the caption being typed (the game's own text), along the bottom of the glass
        if (mCaption.length > 0) {
            int lines = Math.min(3, mCaption.length);
            float bh = 16 + lines * 26;
            float lx = crt.left + 16, ly = crt.bottom - 52 - bh;
            lcd(c, lx, ly, crt.width() - 32, bh);
            for (int i = 0; i < lines; i++)
                lcdText(c, mCaption[mCaption.length - lines + i], lx + 12, ly + 30 + i * 26, 20, PHOSPHOR);
        }
    }

    /** The Vox world picker, in the map's window: one steel card per world, left to right as on
     *  the game's screen, the chosen one raised and lit (its colour picture, LED on). */
    private void drawPicker(Canvas c) {
        RectF crt = mRect2;
        crt.set(R_MAP.left + BEZEL, R_MAP.top + BEZEL, R_MAP.right - BEZEL, R_MAP.bottom - BEZEL);
        mFill.setShader(null);
        mFill.setColor(CRT_BG);
        c.drawRect(crt.left, crt.top, crt.right, crt.bottom, mFill);
        lcdText(c, "VOX", crt.left + 30, crt.top + 70, 44, LED_ON);
        lcdText(c, "CHOOSE A WORLD", crt.left + 150, crt.top + 70, 34, LED_DIM);
        int n = mPickName.length;
        float gap = 18, cw = (crt.width() - 40 - gap * (n - 1)) / n, ch = Math.min(560, cw * 2.4f);
        float top = crt.centerY() - ch / 2 + 10;
        for (int i = 0; i < n; i++) {
            boolean lit = i == mPickSel;
            float l = crt.left + 20 + i * (cw + gap), t = top - (lit ? 10 : 0);
            // glow, body, edges
            if (lit) {
                mFill.setColor(Color.argb(60, 96, 255, 110));
                mRect.set(l - 12, t - 12, l + cw + 12, t + ch + 12);
                c.drawRoundRect(mRect, 26, 26, mFill);
            }
            mFill.setColor(Color.argb(110, 0, 0, 0));
            mRect.set(l + 4, t + 7, l + cw + 4, t + ch + 7);
            c.drawRoundRect(mRect, 18, 18, mFill);
            mFill.setColor(lit ? STEEL_HI : STEEL_MID);
            mRect.set(l, t, l + cw, t + ch);
            c.drawRoundRect(mRect, 18, 18, mFill);
            mStroke.setShader(null);
            mStroke.setStrokeWidth(lit ? 5 : 2);
            mStroke.setColor(lit ? LED_ON : STEEL_LO);
            mStroke.setStyle(Paint.Style.STROKE);
            c.drawRoundRect(mRect, 18, 18, mStroke);
            mStroke.setStyle(Paint.Style.FILL);              // (as the other users of mStroke have it)
            // the picture in a CRT inset
            float ps = cw - 36, px = l + 18, py = t + 22;
            mFill.setColor(Color.rgb(50, 54, 60));
            mRect.set(px - 4, py - 4, px + ps + 4, py + ps + 4);
            c.drawRoundRect(mRect, 8, 8, mFill);
            mFill.setColor(CRT_BG);
            c.drawRect(px, py, px + ps, py + ps, mFill);
            Bitmap b = mIcons.get(lit ? mPickColour[i] : mPickWire[i]);
            if (b == null) b = mIcons.get(mPickWire[i]);
            if (b != null) drawIcon(c, b, px + ps / 2, py + ps / 2, ps);
            if (!lit) {
                mFill.setColor(Color.argb(90, 4, 16, 8));
                c.drawRect(px, py, px + ps, py + ps, mFill);
            }
            // the name, engraved, sized to the card
            String name = mPickName[i];
            float size = Math.min(34, (cw - 24) / (0.66f * Math.max(4, name.length())));
            engraved(c, name, l + cw / 2, py + ps + 26 + size, size, true);
            // the LED and the button hint
            float ly = t + ch - 46;
            led(c, l + cw / 2, ly, 14, lit ? 2 : 1, LED_ON);
            if (lit) engraved(c, "A  GO", l + cw / 2, ly + 40, 20, true);
        }
        lcdText(c, "< >  CHOOSE    A  GO    B  BACK", crt.left + 30, crt.bottom - 34, 26, LED_DIM);
    }

    private void drawPoint(Canvas c, char kind, float px, float py, int iconId, String label, float sc) {
        Bitmap icon = iconId >= 0 ? mIcons.get(iconId) : null;
        switch (kind) {
            case 'p':
                if (icon != null) {
                    mFill.setShader(null);
                    mFill.setColor(Color.argb(170, 0, 0, 0));
                    c.drawCircle(px, py, 26 * sc, mFill);
                    drawIcon(c, icon, px, py, 46 * sc);
                } else star(c, px, py, 12 * sc, WARN_YEL);
                break;
            case 'n':
                if (icon != null) {
                    mFill.setShader(null);
                    mFill.setColor(Color.argb(150, 0, 0, 0));
                    c.drawCircle(px, py, 15 * sc, mFill);
                    drawIcon(c, icon, px, py, 38 * sc);
                }
                else atom(c, px, py, 10 * sc, PHOSPHOR);
                break;
            case 'c':
                if (icon != null) {
                    mFill.setShader(null);
                    mFill.setColor(Color.argb(150, 0, 0, 0));
                    c.drawCircle(px, py, 20 * sc, mFill);
                    drawIcon(c, icon, px, py, 40 * sc);
                }
                else flag(c, px, py, 12 * sc, LED_ON);
                break;
            case 'o': {
                float pulse = (SystemClock.uptimeMillis() % 1200L) / 1200f;
                mStroke.setShader(null);
                mStroke.setStrokeWidth(4);
                mStroke.setColor(Color.argb((int) (255 * (1 - pulse)), 255, 214, 60));
                c.drawCircle(px, py, 14 + 26 * pulse, mStroke);
                star(c, px, py, 14, WARN_YEL);
                break;
            }
            case 'x':
            case 'X': {
                boolean open = kind == 'x';
                int col = open ? LED_ON : WARN_RED;
                mStroke.setShader(null);
                mStroke.setStrokeWidth(4);
                mStroke.setColor(col);
                mRect.set(px - 12, py - 18, px + 12, py + 18);
                c.drawRoundRect(mRect, 10, 10, mStroke);
                mFill.setShader(null);
                mFill.setColor(col);
                c.drawCircle(px + 5, py + 2, 3, mFill);
                if (label != null) lcdText(c, label, px + 20, py + 8, 20, col);
                break;
            }
            case 'd':
                mStroke.setShader(null);
                mStroke.setStrokeWidth(6);
                mStroke.setColor(Color.rgb(190, 196, 204));
                c.drawLine(px - 10, py, px + 10, py, mStroke);
                break;
            case 'f':
                atom(c, px, py, 11 * sc, Color.rgb(120, 230, 255));
                break;
            case 'r':                                           // a red neutron (its menu picture)
                if (icon != null) {
                    mFill.setShader(null);
                    mFill.setColor(Color.argb(150, 0, 0, 0));
                    c.drawCircle(px, py, 18 * sc, mFill);
                    drawIcon(c, icon, px, py, 38 * sc);
                } else atom(c, px, py, 11 * sc, WARN_RED);
                break;
            case 'g': {                                         // Goddard: a bone-white paw ring
                mStroke.setShader(null);
                mStroke.setStrokeWidth(3);
                mStroke.setColor(Color.rgb(230, 236, 240));
                c.drawCircle(px, py, 11, mStroke);
                mFill.setShader(null);
                mFill.setColor(Color.rgb(230, 236, 240));
                c.drawCircle(px, py, 4, mFill);
                break;
            }
            case 'e':
                mFill.setShader(null);
                mFill.setColor(WARN_RED);
                mPath.reset();
                mPath.moveTo(px, py - 10);
                mPath.lineTo(px + 9, py);
                mPath.lineTo(px, py + 10);
                mPath.lineTo(px - 9, py);
                mPath.close();
                c.drawPath(mPath, mFill);
                break;
            default:
                break;
        }
    }

    /** An objective or exit off the screen: a chevron on the CRT's edge pointing at it. */
    private void edgeMarker(Canvas c, RectF crt, float cx, float cy, float px, float py, char kind, String label) {
        float dx = px - cx, dy = py - cy;
        float hw = crt.width() / 2 - 30, hh = crt.height() / 2 - 30;
        float t = Math.min(Math.abs(dx) > 0 ? hw / Math.abs(dx) : 1e9f, Math.abs(dy) > 0 ? hh / Math.abs(dy) : 1e9f);
        float ex = cx + dx * t, ey = cy + dy * t;
        float len = (float) Math.hypot(dx, dy);
        float ux = dx / len, uy = dy / len;
        int col = kind == 'o' ? WARN_YEL : kind == 'x' ? LED_ON : WARN_RED;
        mFill.setShader(null);
        mFill.setColor(col);
        mPath.reset();
        mPath.moveTo(ex + ux * 16, ey + uy * 16);
        mPath.lineTo(ex - ux * 10 - uy * 12, ey - uy * 10 + ux * 12);
        mPath.lineTo(ex - ux * 10 + uy * 12, ey - uy * 10 - ux * 12);
        mPath.close();
        c.drawPath(mPath, mFill);
        if (label != null) {
            mText.setTextAlign(ux > 0.3f ? Paint.Align.RIGHT : Paint.Align.LEFT);
            float lx = ex - ux * 24, ly = ey - uy * 24 + 7;
            mText.setTypeface(mMono);
            mText.setTextSize(18);
            mText.setColor(col);
            c.drawText(label, lx, ly, mText);
            mText.setTextAlign(Paint.Align.LEFT);
        }
    }

    /** Jimmy: a white arrow along his heading over a small electron-orbit ring. */
    private void jimmy(Canvas c, float x, float y) {
        mFill.setShader(null);
        mFill.setColor(Color.argb(80, 110, 255, 140));
        c.drawCircle(x, y, 36, mFill);
        mStroke.setShader(null);
        mStroke.setStrokeWidth(2);
        mStroke.setColor(Color.argb(200, 200, 255, 210));
        mRect.set(x - 36, y - 12, x + 36, y + 12);
        c.save();
        for (int i = 0; i < 3; i++) {
            c.drawOval(mRect, mStroke);
            c.rotate(60, x, y);
        }
        c.restore();
        float s = 30;
        mPath.reset();
        mPath.moveTo(x + mHx * s, y + mHy * s);
        mPath.lineTo(x - mHx * s * 0.7f - mHy * s * 0.6f, y - mHy * s * 0.7f + mHx * s * 0.6f);
        mPath.lineTo(x - mHx * s * 0.35f, y - mHy * s * 0.35f);
        mPath.lineTo(x - mHx * s * 0.7f + mHy * s * 0.6f, y - mHy * s * 0.7f - mHx * s * 0.6f);
        mPath.close();
        mStroke.setColor(Color.rgb(10, 40, 18));
        mStroke.setStrokeWidth(6);
        c.drawPath(mPath, mStroke);
        mFill.setColor(Color.WHITE);
        c.drawPath(mPath, mFill);
    }

    // ------------------------------------------------------------------ the static chrome

    private void buildChrome(int w, int h, float u, float ox, float oy) {
        if (mChrome != null) mChrome.recycle();
        mChrome = Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888);
        mChromeW = w;
        mChromeH = h;
        Canvas c = new Canvas(mChrome);
        // the case: darker brushed steel
        brushed(c, 0, 0, w, h, Color.rgb(150, 156, 163), Color.rgb(112, 118, 126), 1);
        c.save();
        c.translate(ox, oy);
        c.scale(u, u);
        RectF[] plates = { R_HEAD, R_GAUGE, R_LEVEL, R_SHEET, R_MAP };
        for (RectF r : plates) plate(c, r);
        lcd(c, R_HEAD.left + 18, R_HEAD.top + 18, R_HEAD.width() - 36, R_HEAD.height() - 36);
        // the dial face and the CRT recess
        dialFace(c, DIAL_X, DIAL_Y, DIAL_R);
        RectF crt = new RectF(R_MAP.left + BEZEL, R_MAP.top + BEZEL, R_MAP.right - BEZEL, R_MAP.bottom - BEZEL);
        Paint p = new Paint(Paint.ANTI_ALIAS_FLAG);
        p.setColor(Color.rgb(40, 44, 48));
        RectF o = new RectF(crt.left - 6, crt.top - 6, crt.right + 6, crt.bottom + 6);
        c.drawRoundRect(o, 26, 26, p);
        p.setShader(new RadialGradient(crt.centerX(), crt.centerY(), crt.height() * 0.7f,
                new int[] { Color.rgb(10, 34, 16), CRT_BG }, null, Shader.TileMode.CLAMP));
        c.drawRoundRect(crt, 20, 20, p);
        p.setShader(null);
        p.setStyle(Paint.Style.STROKE);
        p.setStrokeWidth(3);
        p.setColor(Color.argb(120, 255, 255, 255));
        c.drawLine(o.left + 20, o.bottom + 1, o.right - 20, o.bottom + 1, p);
        // range rings on the glass (faint)
        p.setStrokeWidth(1.5f);
        p.setColor(Color.argb(40, 110, 255, 140));
        for (int i = 1; i <= 3; i++) c.drawCircle(crt.centerX(), crt.centerY(), crt.width() * 0.16f * i, p);
        c.drawLine(crt.left, crt.centerY(), crt.right, crt.centerY(), p);
        c.drawLine(crt.centerX(), crt.top, crt.centerX(), crt.bottom, p);
        c.restore();
    }

    /** Brushed steel: a vertical gradient with fine horizontal streaks (same seed every time). */
    private void brushed(Canvas c, float l, float t, float r, float b, int top, int bottom, float u) {
        Paint p = new Paint();
        p.setShader(new LinearGradient(l, t, r * 0.3f + l * 0.7f, b, top, bottom, Shader.TileMode.CLAMP));
        c.drawRect(l, t, r, b, p);
        p.setShader(null);
        p.setStrokeWidth(1);
        Random rnd = new Random(1234);
        for (float y = t; y < b; y += 1.5f) {
            int a = rnd.nextInt(26);
            p.setColor(rnd.nextBoolean() ? Color.argb(a, 255, 255, 255) : Color.argb(a, 0, 0, 0));
            float x0 = l + rnd.nextFloat() * (r - l) * 0.3f;
            c.drawLine(x0, y, x0 + (r - l) * (0.4f + rnd.nextFloat() * 0.6f), y, p);
        }
    }

    private void plate(Canvas c, RectF r) {
        Paint p = new Paint(Paint.ANTI_ALIAS_FLAG);
        // drop shadow, body, brushing, bevel
        p.setColor(Color.argb(90, 0, 0, 0));
        mRect.set(r.left + 3, r.top + 5, r.right + 3, r.bottom + 5);
        c.drawRoundRect(mRect, 22, 22, p);
        p.setShader(new LinearGradient(r.left, r.top, r.right, r.bottom, STEEL_HI, STEEL_LO, Shader.TileMode.CLAMP));
        c.drawRoundRect(r, 22, 22, p);
        p.setShader(null);
        p.setStrokeWidth(1);
        Random rnd = new Random((long) (r.left * 31 + r.top));
        for (float y = r.top + 8; y < r.bottom - 8; y += 1.5f) {
            int a = rnd.nextInt(30);
            p.setColor(rnd.nextBoolean() ? Color.argb(a, 255, 255, 255) : Color.argb(a, 60, 60, 60));
            float x0 = r.left + 10 + rnd.nextFloat() * r.width() * 0.25f;
            c.drawLine(x0, y, Math.min(r.right - 10, x0 + r.width() * (0.3f + rnd.nextFloat() * 0.7f)), y, p);
        }
        p.setStyle(Paint.Style.STROKE);
        p.setStrokeWidth(3);
        p.setColor(Color.argb(170, 255, 255, 255));
        mRect.set(r.left + 1.5f, r.top + 1.5f, r.right - 1.5f, r.bottom - 1.5f);
        c.drawRoundRect(mRect, 21, 21, p);
        p.setColor(Color.argb(140, 40, 44, 50));
        p.setStrokeWidth(2);
        mRect.set(r.left + 3, r.top + 4, r.right - 0.5f, r.bottom - 0.5f);
        c.drawRoundRect(mRect, 21, 21, p);
        p.setStyle(Paint.Style.FILL);
        // screws in the corners
        float s = 13;
        screw(c, r.left + s, r.top + s, p);
        screw(c, r.right - s, r.top + s, p);
        screw(c, r.left + s, r.bottom - s, p);
        screw(c, r.right - s, r.bottom - s, p);
    }

    private void screw(Canvas c, float x, float y, Paint p) {
        p.setShader(new RadialGradient(x - 2, y - 2, 7, new int[] { Color.rgb(245, 247, 250), Color.rgb(120, 126, 134) },
                null, Shader.TileMode.CLAMP));
        c.drawCircle(x, y, 6, p);
        p.setShader(null);
        p.setStyle(Paint.Style.STROKE);
        p.setStrokeWidth(1.6f);
        p.setColor(Color.rgb(70, 74, 80));
        c.drawLine(x - 4, y + 2.5f, x + 4, y - 2.5f, p);
        p.setStrokeWidth(1);
        p.setColor(Color.argb(120, 0, 0, 0));
        c.drawCircle(x, y, 6, p);
        p.setStyle(Paint.Style.FILL);
    }

    private void dialFace(Canvas c, float cx, float cy, float r) {
        Paint p = new Paint(Paint.ANTI_ALIAS_FLAG);
        p.setShader(new RadialGradient(cx, cy - r * 0.3f, r * 1.2f, new int[] { Color.rgb(18, 40, 24), Color.rgb(4, 12, 6) },
                null, Shader.TileMode.CLAMP));
        c.drawCircle(cx, cy, r, p);
        p.setShader(null);
        p.setStyle(Paint.Style.STROKE);
        p.setStrokeWidth(6);
        p.setShader(new LinearGradient(cx - r, cy - r, cx + r, cy + r, STEEL_HI, STEEL_LO, Shader.TileMode.CLAMP));
        c.drawCircle(cx, cy, r + 3, p);
        p.setShader(null);
        // zones: red below 30, yellow below 70, green above (as the game's energy bar)
        RectF a = new RectF(cx - r + 12, cy - r + 12, cx + r - 12, cy + r - 12);
        p.setStrokeWidth(8);
        p.setColor(Color.argb(200, 255, 70, 60));
        c.drawArc(a, 150, 240 * 0.3f, false, p);
        p.setColor(Color.argb(200, 255, 214, 60));
        c.drawArc(a, 150 + 240 * 0.3f, 240 * 0.4f, false, p);
        p.setColor(Color.argb(200, 96, 255, 110));
        c.drawArc(a, 150 + 240 * 0.7f, 240 * 0.3f, false, p);
        p.setStrokeWidth(2);
        p.setColor(Color.argb(160, 110, 255, 140));
        for (int i = 0; i <= 10; i++) {
            double ang = Math.toRadians(150 + 24 * i);
            float r0 = r - 24, r1 = r - (i % 5 == 0 ? 36 : 30);
            c.drawLine(cx + (float) Math.cos(ang) * r0, cy + (float) Math.sin(ang) * r0,
                    cx + (float) Math.cos(ang) * r1, cy + (float) Math.sin(ang) * r1, p);
        }
    }

    // ------------------------------------------------------------------ widgets

    private void dial(Canvas c, float cx, float cy, float r, float f) {
        if (f < 0) return;
        f = Math.max(0, Math.min(1, f));
        double ang = Math.toRadians(150 + 240 * f);
        float nx = (float) Math.cos(ang), ny = (float) Math.sin(ang);
        mStroke.setShader(null);
        mStroke.setStrokeCap(Paint.Cap.ROUND);
        mStroke.setStrokeWidth(6);
        mStroke.setColor(energyColour(f * 100));
        c.drawLine(cx - nx * 10, cy - ny * 10, cx + nx * (r - 20), cy + ny * (r - 20), mStroke);
        mStroke.setStrokeCap(Paint.Cap.BUTT);
        mFill.setShader(null);
        mFill.setColor(STEEL_MID);
        c.drawCircle(cx, cy, 11, mFill);
        mFill.setColor(STEEL_HI);
        c.drawCircle(cx - 2, cy - 2, 6, mFill);
    }

    private static int energyColour(float e) {
        return e >= 70 ? LED_ON : e >= 30 ? WARN_YEL : WARN_RED;
    }

    /** A dark LCD window let into the steel. */
    private void lcd(Canvas c, float x, float y, float w, float h) {
        mFill.setShader(null);
        mFill.setColor(Color.argb(150, 255, 255, 255));
        mRect.set(x - 1, y - 1, x + w + 2, y + h + 2);
        c.drawRoundRect(mRect, 10, 10, mFill);
        mFill.setColor(Color.rgb(50, 54, 60));
        mRect.set(x - 2, y - 2, x + w, y + h);
        c.drawRoundRect(mRect, 10, 10, mFill);
        mFill.setColor(LCD_BG);
        mRect.set(x, y, x + w, y + h);
        c.drawRoundRect(mRect, 9, 9, mFill);
    }

    /** An LED: state 0 off (steel), 1 dim, 2 lit with a glow. */
    private void led(Canvas c, float x, float y, float r, int state, int colour) {
        mFill.setShader(null);
        if (state == 2) {
            mFill.setColor(Color.argb(70, Color.red(colour), Color.green(colour), Color.blue(colour)));
            c.drawCircle(x, y, r * 2.1f, mFill);
        }
        mFill.setColor(Color.rgb(60, 64, 70));
        c.drawCircle(x, y, r + 2.5f, mFill);
        int col = state == 2 ? colour : state == 1 ? dim(colour) : STEEL_LO;
        mFill.setColor(col);
        c.drawCircle(x, y, r, mFill);
        mFill.setColor(Color.argb(state == 2 ? 200 : 90, 255, 255, 255));
        c.drawCircle(x - r * 0.35f, y - r * 0.35f, r * 0.32f, mFill);
    }

    private static int dim(int colour) {
        return Color.rgb(Color.red(colour) * 3 / 10, Color.green(colour) * 3 / 10, Color.blue(colour) * 3 / 10);
    }

    /** A round push button with an LED ring: 1 lit dim, 2 lit bright. */
    private void button(Canvas c, float x, float y, float r, int state) {
        mFill.setShader(null);
        if (state == 2) {
            mFill.setColor(Color.argb(90, 96, 255, 110));
            c.drawCircle(x, y, r + 14, mFill);
        }
        mFill.setColor(Color.rgb(60, 64, 70));
        c.drawCircle(x, y, r + 6, mFill);
        mFill.setColor(state == 2 ? LED_ON : state == 1 ? LED_DIM : STEEL_LO);
        c.drawCircle(x, y, r + 4, mFill);
        mFill.setColor(Color.rgb(196, 200, 206));
        c.drawCircle(x, y, r, mFill);
        mFill.setColor(Color.rgb(226, 230, 234));
        c.drawCircle(x - 2, y - 2, r - 4, mFill);
    }

    /** A pill-shaped push button (the tasks): steel, slightly raised when active. */
    private void pill(Canvas c, float x, float y, float w, float h, boolean active) {
        mFill.setShader(null);
        mFill.setColor(Color.rgb(70, 74, 80));
        mRect.set(x, y + 2, x + w, y + h + 2);
        c.drawRoundRect(mRect, h / 2, h / 2, mFill);
        mFill.setColor(active ? Color.rgb(214, 220, 216) : Color.rgb(170, 175, 181));
        mRect.set(x, y, x + w, y + h);
        c.drawRoundRect(mRect, h / 2, h / 2, mFill);
        if (active) {
            mStroke.setShader(null);
            mStroke.setStrokeWidth(2);
            mStroke.setColor(Color.argb(160, 96, 255, 110));
            c.drawRoundRect(mRect, h / 2, h / 2, mStroke);
        }
    }

    private void drawIcon(Canvas c, Bitmap b, float x, float y, float size) {
        float sc = size / Math.max(b.getWidth(), b.getHeight());
        float w = b.getWidth() * sc, h = b.getHeight() * sc;
        mRect.set(x - w / 2, y - h / 2, x + w / 2, y + h / 2);
        c.drawBitmap(b, null, mRect, mBmp);
    }

    /** Text in the LCD font (monospace, green). */
    private void lcdText(Canvas c, String s, float x, float y, float size, int colour) {
        mText.setTypeface(mMono);
        mText.setTextAlign(Paint.Align.LEFT);
        mText.setTextSize(size);
        mText.setColor(colour);
        c.drawText(s, x, y, mText);
    }

    /** Engraved label: dark letters with a light edge below. */
    private void engraved(Canvas c, String s, float x, float y, float size, boolean centre) {
        engraved(c, s, x, y, size, centre, false);
    }

    private void engraved(Canvas c, String s, float x, float y, float size, boolean centre, boolean right) {
        mText.setTypeface(mBold);
        mText.setTextAlign(right ? Paint.Align.RIGHT : centre ? Paint.Align.CENTER : Paint.Align.LEFT);
        mText.setTextSize(size);
        mText.setColor(Color.argb(160, 255, 255, 255));
        c.drawText(s, x, y + 1.5f, mText);
        mText.setColor(ENGRAVE);
        c.drawText(s, x, y, mText);
        mText.setTextAlign(Paint.Align.LEFT);
    }

    // seven-segment readout: segments a..g as bits 0..6
    private static final int[] SEGS = {
        0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f
    };

    /** Digits (and '/', '-') as seven-segment LEDs with the unlit segments ghosted; `pad` digits
     *  are always drawn (unlit leading ones), 0 = as many as the text has. Top-left at (x, y). */
    private void segText(Canvas c, String s, float x, float y, float h, int pad, int colour) {
        float w = h * 0.55f, gap = h * 0.18f;
        int lead = Math.max(0, pad - s.length());
        float cx = x;
        for (int i = 0; i < lead; i++) {
            seg(c, cx, y, w, h, 0, colour);
            cx += w + gap;
        }
        for (int i = 0; i < s.length(); i++) {
            char ch = s.charAt(i);
            if (ch >= '0' && ch <= '9') seg(c, cx, y, w, h, SEGS[ch - '0'], colour);
            else if (ch == '-') seg(c, cx, y, w, h, 0x40, colour);
            else if (ch == ':') {
                mFill.setShader(null);
                mFill.setColor(colour);
                c.drawCircle(cx + w * 0.2f, y + h * 0.3f, h * 0.07f, mFill);
                c.drawCircle(cx + w * 0.2f, y + h * 0.7f, h * 0.07f, mFill);
                cx += w * 0.4f + gap;
                continue;
            }
            else if (ch == '/') {
                mStroke.setShader(null);
                mStroke.setStrokeWidth(h * 0.1f);
                mStroke.setColor(colour);
                c.drawLine(cx + w * 0.7f, y + h * 0.1f, cx + w * 0.1f, y + h * 0.9f, mStroke);
                cx += w * 0.8f + gap;
                continue;
            }
            cx += w + gap;
        }
    }

    private void seg(Canvas c, float x, float y, float w, float h, int bits, int colour) {
        float t = h * 0.12f, hh = h / 2;
        for (int sgm = 0; sgm < 7; sgm++) {
            boolean on = (bits & (1 << sgm)) != 0;
            mFill.setShader(null);
            mFill.setColor(on ? colour : LED_GHOST);
            float x0, y0, x1, y1;
            switch (sgm) {
                case 0: x0 = x + t; y0 = y; x1 = x + w - t; y1 = y + t; break;                      // a
                case 1: x0 = x + w - t; y0 = y + t * 0.7f; x1 = x + w; y1 = y + hh - t * 0.3f; break;  // b
                case 2: x0 = x + w - t; y0 = y + hh + t * 0.3f; x1 = x + w; y1 = y + h - t * 0.7f; break; // c
                case 3: x0 = x + t; y0 = y + h - t; x1 = x + w - t; y1 = y + h; break;              // d
                case 4: x0 = x; y0 = y + hh + t * 0.3f; x1 = x + t; y1 = y + h - t * 0.7f; break;      // e
                case 5: x0 = x; y0 = y + t * 0.7f; x1 = x + t; y1 = y + hh - t * 0.3f; break;          // f
                default: x0 = x + t; y0 = y + hh - t / 2; x1 = x + w - t; y1 = y + hh + t / 2; break; // g
            }
            mRect.set(x0, y0, x1, y1);
            c.drawRoundRect(mRect, t / 2, t / 2, mFill);
        }
    }

    /** Electron orbits: three ellipses round a nucleus. */
    private void atom(Canvas c, float x, float y, float r, int colour) {
        mStroke.setShader(null);
        mStroke.setStrokeWidth(Math.max(1.5f, r * 0.14f));
        mStroke.setColor(colour);
        mRect.set(x - r, y - r * 0.38f, x + r, y + r * 0.38f);
        c.save();
        for (int i = 0; i < 3; i++) {
            c.drawOval(mRect, mStroke);
            c.rotate(60, x, y);
        }
        c.restore();
        mFill.setShader(null);
        mFill.setColor(colour);
        c.drawCircle(x, y, r * 0.22f, mFill);
    }

    /** A four-point starburst. */
    private void star(Canvas c, float x, float y, float r, int colour) {
        mFill.setShader(null);
        mFill.setColor(colour);
        mPath.reset();
        for (int i = 0; i < 8; i++) {
            double a = Math.PI / 4 * i - Math.PI / 2;
            float rr = (i % 2 == 0) ? r : r * 0.32f;
            float px = x + (float) Math.cos(a) * rr, py = y + (float) Math.sin(a) * rr;
            if (i == 0) mPath.moveTo(px, py);
            else mPath.lineTo(px, py);
        }
        mPath.close();
        c.drawPath(mPath, mFill);
    }

    /** A little pennant on a pole (checkpoints when the game's sprite is not there). */
    private void flag(Canvas c, float x, float y, float r, int colour) {
        mStroke.setShader(null);
        mStroke.setStrokeWidth(Math.max(2, r * 0.18f));
        mStroke.setColor(colour);
        c.drawLine(x - r * 0.5f, y + r, x - r * 0.5f, y - r, mStroke);
        mFill.setShader(null);
        mFill.setColor(colour);
        mPath.reset();
        mPath.moveTo(x - r * 0.5f, y - r);
        mPath.lineTo(x + r, y - r * 0.5f);
        mPath.lineTo(x - r * 0.5f, y);
        mPath.close();
        c.drawPath(mPath, mFill);
    }

    private static String modeName(int m) {
        switch (m) {
            case 0: return "START";
            case 1: return "LOADING";
            case 2: return "SAVING";
            case 3: return "CUT SCENE";
            case 4: return "MENU";
            case 5: return "PLAYING";
            case 6: return "PAUSED";
            default: return "";
        }
    }
}
