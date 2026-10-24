package com.sirbilliams.negatron;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.LinearGradient;
import android.graphics.Paint;
import android.graphics.RadialGradient;
import android.graphics.RectF;
import android.graphics.Shader;
import android.graphics.Typeface;
import android.os.SystemClock;
import android.view.View;

import java.util.ArrayList;
import java.util.Random;

/**
 * port: the launcher menu (SetupActivity) in the bottom-screen panel's style (CompanionView):
 * brushed stainless-steel plates with screws, push buttons with green LEDs, LCD readouts and a
 * green CRT window. Everything is drawn in code (no artwork in the repo); a picture from the
 * player's own game files can be shown in the CRT (LauncherModel.art).
 *
 * Two layouts: the main screen (laid out at 1920 x 1080: the buttons on the left, the info window
 * on the right) and the bottom screen (1240 x 1080: the page's options in the CRT and the selected
 * item's details / save-slot preview below). Both scale to the size they get.
 */
final class LauncherView extends View {
    private static final int STEEL_HI = Color.rgb(222, 226, 230);
    private static final int STEEL_LO = Color.rgb(128, 134, 142);
    private static final int ENGRAVE = Color.rgb(70, 74, 80);
    private static final int LCD_BG = Color.rgb(8, 22, 12);
    private static final int LED_ON = Color.rgb(96, 255, 110);
    private static final int LED_DIM = Color.rgb(28, 88, 36);
    private static final int CRT_BG = Color.rgb(4, 16, 8);
    private static final int PHOSPHOR = Color.rgb(110, 255, 140);
    private static final int AMBER = Color.rgb(255, 196, 70);

    // main screen layout
    private static final float TW = 1920, TH = 1080;
    private static final RectF T_HEAD = new RectF(36, 30, 1884, 160);
    private static final RectF T_MENU = new RectF(36, 190, 1010, 1050);
    private static final RectF T_INFO = new RectF(1040, 190, 1884, 1050);
    private static final float ROW_X = 76, ROW_W = 894, ROW_H = 84, ROW_STEP = 102, ROWS_Y = 236;
    // bottom screen layout
    private static final float BW = 1240, BH = 1080;
    private static final RectF B_HEAD = new RectF(20, 20, 1220, 124);
    private static final RectF B_LIST = new RectF(20, 144, 1220, 744);
    private static final RectF B_PREV = new RectF(20, 764, 1220, 1060);
    private static final float BEZEL = 22;

    private final LauncherModel mModel;
    private final boolean mBottom;
    private final Paint mFill = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint mStroke = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint mText = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint mBmp = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.FILTER_BITMAP_FLAG);
    private final RectF mRect = new RectF();
    private final Typeface mBold = Typeface.create(Typeface.SANS_SERIF, Typeface.BOLD);
    private final Typeface mMono = Typeface.create(Typeface.MONOSPACE, Typeface.BOLD);
    private Bitmap mChrome;
    private int mChromeW, mChromeH;
    private boolean mCover;                 // the chrome is the main menu's cover background
    private String mPicKey, mPrevKey;       // the info window's picture and the one fading out
    private long mFadeStart;
    private static final float FADE_MS = 280;
    /** Bill's choice: the options always show the drawn illustrations (LauncherArt), not the game's
     *  own pictures (LauncherModel.pics), which stay available behind this switch. */
    static final boolean GAME_PICTURES = false;
    private final java.util.HashMap<String, Bitmap> mDrawn = new java.util.HashMap<>();
    private Object mCoverArt;               // ... for this picture

    LauncherView(Context c, LauncherModel model, boolean bottom) {
        super(c);
        mModel = model;
        mBottom = bottom;
        mStroke.setStyle(Paint.Style.STROKE);
        model.listen(new LauncherModel.Listener() {
            @Override public void changed() { invalidate(); }
        });
    }

    /** Animation step (the atom, the blinking LEDs). */
    void tick() { invalidate(); }

    private float scale() {
        return Math.min(getWidth() / (mBottom ? BW : TW), getHeight() / (mBottom ? BH : TH));
    }

    private float offX() { return (getWidth() - (mBottom ? BW : TW) * scale()) / 2; }

    private float offY() { return (getHeight() - (mBottom ? BH : TH) * scale()) / 2; }

    /** Main screen: the item under a touch at view coordinates (x, y), or -1. */
    int itemAt(float x, float y) {
        LauncherModel.Page p = mModel.page();
        if (p == null || mBottom) return -1;
        float u = scale();
        if (u <= 0) return -1;
        x = (x - offX()) / u;
        y = (y - offY()) / u;
        if (x < ROW_X || x > ROW_X + ROW_W) return -1;
        int first = firstRow(p, visibleRows(p));
        float y0 = rowsTop(p);
        int k = (int) Math.floor((y - y0) / ROW_STEP);
        if (y < y0 || k >= visibleRows(p) || (y - y0) - k * ROW_STEP > ROW_H) return -1;
        k += first;
        return k < p.items.size() ? k : -1;
    }

    /** Main screen: whether (x, y) is on the header plate (a long press there shows the test item). */
    boolean onHeader(float x, float y) {
        float u = scale();
        if (u <= 0 || mBottom) return false;
        x = (x - offX()) / u;
        y = (y - offY()) / u;
        return x >= T_HEAD.left && x <= T_HEAD.right && y >= T_HEAD.top && y <= T_HEAD.bottom;
    }

    @Override
    protected void onDraw(Canvas c) {
        int w = getWidth(), h = getHeight();
        if (w <= 0 || h <= 0) return;
        float u = scale();
        LauncherModel.Page pg = mModel.page();
        boolean cover = mBottom && pg != null && pg.id.equals("main");
        Object coverArt = cover ? coverBitmap() : mBottom ? null : mModel.background;
        if (mChrome == null || mChromeW != w || mChromeH != h || cover != mCover || coverArt != mCoverArt) {
            mCover = cover;
            mCoverArt = coverArt;
            buildChrome(w, h, u, offX(), offY());
        }
        c.drawBitmap(mChrome, 0, 0, null);
        c.save();
        c.translate(offX(), offY());
        c.scale(u, u);
        LauncherModel.Page p = mModel.page();
        if (p != null) {
            if (mBottom) drawBottom(c, p);
            else drawTop(c, p);
        }
        c.restore();
    }

    // ------------------------------------------------------------------ main screen

    private float rowsTop(LauncherModel.Page p) { return p.warning != null ? ROWS_Y + 190 : ROWS_Y; }

    private int visibleRows(LauncherModel.Page p) { return p.warning != null ? 5 : 7; }

    private static int firstRow(LauncherModel.Page p, int vis) {
        int n = p.items.size();
        if (n <= vis) return 0;
        return Math.max(0, Math.min(p.sel - (vis - 1) / 2, n - vis));
    }

    private void drawTop(Canvas c, LauncherModel.Page p) {
        long t = SystemClock.uptimeMillis();
        // header: the game's name and the page in the LCD, two status LEDs
        text(c, "JIMMY NEUTRON vs. JIMMY NEGATRON", T_HEAD.left + 52, T_HEAD.top + 82, 44, LED_ON, Paint.Align.LEFT, true);
        text(c, p.title.toUpperCase(java.util.Locale.ROOT), T_HEAD.right - 150, T_HEAD.top + 82, 40, PHOSPHOR,
                Paint.Align.RIGHT, true);
        led(c, T_HEAD.right - 98, T_HEAD.centerY(), 11, 2, LED_ON);
        led(c, T_HEAD.right - 64, T_HEAD.centerY(), 11, (t / 500) % 2 == 0 ? 2 : 1, AMBER);

        if (p.warning != null) {
            lcd(c, ROW_X, ROWS_Y, ROW_W, 160);
            led(c, ROW_X + 40, ROWS_Y + 48, 12, (t / 400) % 2 == 0 ? 2 : 1, AMBER);
            String[] lines = wrap(p.warning, 44);
            for (int i = 0; i < lines.length && i < 4; i++)
                text(c, lines[i], ROW_X + 76, ROWS_Y + 58 + i * 36, 30, AMBER, Paint.Align.LEFT, true);
        }
        int vis = visibleRows(p), first = firstRow(p, vis);
        float y = rowsTop(p);
        for (int k = first; k < p.items.size() && k < first + vis; k++, y += ROW_STEP)
            row(c, p.items.get(k), k == p.sel, y);
        if (first > 0) arrow(c, ROW_X + ROW_W / 2, rowsTop(p) - 14, true);
        if (first + vis < p.items.size()) arrow(c, ROW_X + ROW_W / 2, rowsTop(p) + vis * ROW_STEP - 6, false);
        engraved(c, p.footer != null ? p.footer : "D-PAD / STICK  CHOOSE     A  SELECT     B  BACK",
                T_MENU.centerX(), T_MENU.bottom - 26, 22, true, false);

        // the info window: the option's picture (the game's own, else drawn here), cross-faded
        // when the selection changes, then the selected item's help / preview
        RectF crt = new RectF(T_INFO.left + BEZEL, T_INFO.top + BEZEL, T_INFO.right - BEZEL, T_INFO.bottom - BEZEL);
        float picH = crt.height() * 0.54f;
        String key = mModel.picKey();
        if (key == null) key = "";
        if (!key.equals(mPicKey)) {
            mPrevKey = mPicKey;
            mPicKey = key;
            mFadeStart = mPrevKey == null ? t - (long) FADE_MS : t;    // the first one: no fade
        }
        float f = Math.min(1f, Math.max(0f, (t - mFadeStart) / FADE_MS));
        float aw = Math.min(crt.width() - 60, (picH - 40) * 4f / 3f), ah = aw * 3f / 4f;
        RectF pic = new RectF(crt.centerX() - aw / 2, crt.top + 30, crt.centerX() + aw / 2, crt.top + 30 + ah);
        if (f < 1f && mPrevKey != null) picture(c, mPrevKey, pic, 1f);
        picture(c, mPicKey, pic, f);
        mStroke.setShader(null);
        mStroke.setStrokeWidth(2);
        mStroke.setColor(Color.argb(90, 110, 255, 140));
        c.drawLine(crt.left + 30, crt.top + picH + 10, crt.right - 30, crt.top + picH + 10, mStroke);
        info(c, mModel.selected(), crt.left + 36, crt.top + picH + 64, crt.width() - 72, 30);
    }

    /** One push button: LED, engraved label, a value in an LCD on the right. */
    private void row(Canvas c, LauncherModel.Item it, boolean sel, float y) {
        if (it.hint) {
            engraved(c, it.label, ROW_X + 30, y + 54, 30, false, true);
            return;
        }
        pill(c, ROW_X, y, ROW_W, ROW_H, sel);
        led(c, ROW_X + 46, y + ROW_H / 2, 13, sel ? 2 : it.enabled ? 1 : 0, LED_ON);
        String label = it.label.toUpperCase(java.util.Locale.ROOT);
        float room = ROW_W - 86 - (it.value != null ? 250 : 40);   // long labels get smaller letters
        float size = Math.min(38, room / (0.68f * Math.max(1, label.length())));
        engraved(c, label, ROW_X + 86, y + 56 - (38 - size) * 0.35f, size, false, !it.enabled);
        if (it.value != null) {
            float vw = 210, vx = ROW_X + ROW_W - vw - 22;
            lcd(c, vx, y + 15, vw, ROW_H - 30);
            if (it.progress >= 0) {                                 // port: an install's progress bar
                mFill.setShader(null);
                mFill.setColor(Color.argb(110, 110, 255, 140));
                c.drawRect(vx + 6, y + 21, vx + 6 + (vw - 12) * Math.min(1f, it.progress), y + ROW_H - 21, mFill);
            }
            text(c, it.value, vx + vw / 2, y + 55, 30, it.enabled ? LED_ON : LED_DIM, Paint.Align.CENTER, true);
        }
    }

    /** The picture for key in r with alpha a: the game's own (with scanlines), else the drawing. */
    private void picture(Canvas c, String key, RectF r, float a) {
        if (key == null || a <= 0) return;
        Object o = GAME_PICTURES ? mModel.pics.get(key) : null;
        boolean game = o instanceof Bitmap;
        Bitmap b = game ? (Bitmap) o : drawn(key);
        if (b == null) return;
        mBmp.setAlpha(Math.round(255 * a));
        c.drawBitmap(b, null, r, mBmp);
        mBmp.setAlpha(255);
        if (game && a >= 1f) scanlines(c, r);
    }

    /** An illustration drawn in code (phosphor green, atomic-age lab motifs), 640 x 480, cached. */
    private Bitmap drawn(String key) {
        Bitmap b = mDrawn.get(key);
        if (b != null) return b;
        b = Bitmap.createBitmap(640, 480, Bitmap.Config.ARGB_8888);
        Canvas c = new Canvas(b);
        LauncherArt.draw(c, key);
        mDrawn.put(key, b);
        return b;
    }

    // ------------------------------------------------------------------ bottom screen

    private void drawBottom(Canvas c, LauncherModel.Page p) {
        long t = SystemClock.uptimeMillis();
        if (p.id.equals("main")) {
            drawCover(c, t);
            return;
        }
        text(c, p.title.toUpperCase(java.util.Locale.ROOT), B_HEAD.left + 48, B_HEAD.top + 68, 40, LED_ON, Paint.Align.LEFT, true);
        for (int i = 0; i < 4; i++)
            led(c, B_HEAD.right - 150 + i * 32, B_HEAD.centerY(), 9, (t / 300 + i) % 4 == 0 ? 2 : 1, LED_ON);

        // the options in the CRT, the selected one in reverse video
        RectF crt = new RectF(B_LIST.left + BEZEL, B_LIST.top + BEZEL, B_LIST.right - BEZEL, B_LIST.bottom - BEZEL);
        float rowH = 66, y0 = crt.top + 26;
        int vis = (int) ((crt.height() - 40) / rowH);
        int first = firstRow(p, vis);
        for (int k = first; k < p.items.size() && k < first + vis; k++) {
            LauncherModel.Item it = p.items.get(k);
            float y = y0 + (k - first) * rowH;
            boolean sel = k == p.sel;
            if (sel) {
                mFill.setShader(null);
                mFill.setColor(Color.argb(210, 110, 255, 140));
                mRect.set(crt.left + 24, y, crt.right - 24, y + rowH - 8);
                c.drawRoundRect(mRect, 8, 8, mFill);
            }
            int col = sel ? CRT_BG : it.enabled && !it.hint ? PHOSPHOR : Color.rgb(50, 120, 66);
            text(c, (sel ? "> " : "  ") + it.label.toUpperCase(java.util.Locale.ROOT), crt.left + 40, y + 44, 36, col,
                    Paint.Align.LEFT, true);
            if (it.value != null) text(c, it.value, crt.right - 44, y + 44, 36, col, Paint.Align.RIGHT, true);
        }
        if (first > 0) text(c, "^ MORE", crt.right - 44, crt.top + 2 + 22, 22, PHOSPHOR, Paint.Align.RIGHT, true);
        if (first + vis < p.items.size())
            text(c, "v MORE", crt.right - 44, crt.bottom - 10, 22, PHOSPHOR, Paint.Align.RIGHT, true);

        // the selected item's details (a save slot's preview) in the LCD below
        RectF l = new RectF(B_PREV.left + 30, B_PREV.top + 30, B_PREV.right - 30, B_PREV.bottom - 30);
        float ty = l.top + 54;
        if (p.warning != null) {
            String[] lines = wrap(p.warning, 52);
            for (int i = 0; i < lines.length && i < 2; i++, ty += 38)
                text(c, lines[i], l.left + 30, ty, 30, AMBER, Paint.Align.LEFT, true);
            ty += 10;
        }
        info(c, mModel.selected(), l.left + 30, ty, l.width() - 60, 32);
    }

    /** Bottom screen, main menu: the box art alone (aspect kept) in a steel plate, else the game's
     *  own menu picture, else the atom (in a CRT). The plate is in the chrome (coverRect). */
    private void drawCover(Canvas c, long t) {
        Bitmap b = coverBitmap();
        if (b == null) {
            atom(c, BW / 2, BH / 2, 300, t);
            return;
        }
        coverRect(b, mRect);
        c.drawBitmap(b, null, mRect, mBmp);
    }

    private Bitmap coverBitmap() {
        return mModel.boxArt instanceof Bitmap ? (Bitmap) mModel.boxArt
                : mModel.art instanceof Bitmap ? (Bitmap) mModel.art : null;
    }

    private static final float COVER_MARGIN = 20, COVER_BEZEL = 34;

    /** Where the cover picture goes (aspect kept, centred, inside its plate); the whole CRT without one. */
    private static void coverRect(Bitmap b, RectF out) {
        float m = COVER_MARGIN + COVER_BEZEL;
        if (b == null) {
            out.set(m, m, BW - m, BH - m);
            return;
        }
        float sc = Math.min((BW - 2 * m) / b.getWidth(), (BH - 2 * m) / b.getHeight());
        float w = b.getWidth() * sc, h = b.getHeight() * sc;
        out.set((BW - w) / 2, (BH - h) / 2, (BW + w) / 2, (BH + h) / 2);
    }

    // ------------------------------------------------------------------ shared pieces

    /** The selected item's preview lines (save slot) or its help text, wrapped. */
    private void info(Canvas c, LauncherModel.Item it, float x, float y, float w, float size) {
        if (it == null) return;
        int cols = Math.max(10, (int) (w / (size * 0.62f)));
        float line = size * 1.3f;
        if (it.detail != null) {
            for (int i = 0; i < it.detail.length; i++, y += line * 1.15f)
                text(c, it.detail[i], x, y, i == 0 ? size * 1.25f : size, i == 0 ? LED_ON : PHOSPHOR, Paint.Align.LEFT, true);
        }
        if (it.help != null && it.detail == null) {
            for (String s : wrap(it.help, cols)) {
                text(c, s, x, y, size, it.enabled ? PHOSPHOR : Color.rgb(60, 150, 80), Paint.Align.LEFT, true);
                y += line;
            }
        }
    }

    /** Word wrap at a column count (the LCD font is monospace). */
    static String[] wrap(String s, int cols) {
        ArrayList<String> out = new ArrayList<>();
        for (String para : s.split("\n")) {
            StringBuilder b = new StringBuilder();
            for (String word : para.split(" ")) {
                if (b.length() > 0 && b.length() + 1 + word.length() > cols) {
                    out.add(b.toString());
                    b.setLength(0);
                }
                if (b.length() > 0) b.append(' ');
                b.append(word);
            }
            out.add(b.toString());
        }
        return out.toArray(new String[0]);
    }

    private void text(Canvas c, String s, float x, float y, float size, int colour, Paint.Align a, boolean mono) {
        mText.setTypeface(mono ? mMono : mBold);
        mText.setTextAlign(a);
        mText.setTextSize(size);
        mText.setColor(colour);
        c.drawText(s, x, y, mText);
        mText.setTextAlign(Paint.Align.LEFT);
    }

    /** Engraved label: dark letters with a light edge below (paler when the item is disabled). */
    private void engraved(Canvas c, String s, float x, float y, float size, boolean centre, boolean faint) {
        Paint.Align a = centre ? Paint.Align.CENTER : Paint.Align.LEFT;
        text(c, s, x, y + 1.5f, size, Color.argb(faint ? 90 : 160, 255, 255, 255), a, false);
        text(c, s, x, y, size, faint ? Color.argb(110, 70, 74, 80) : ENGRAVE, a, false);
    }

    private void pill(Canvas c, float x, float y, float w, float h, boolean sel) {
        mFill.setShader(null);
        if (sel) {
            mFill.setColor(Color.argb(70, 96, 255, 110));
            mRect.set(x - 8, y - 8, x + w + 8, y + h + 8);
            c.drawRoundRect(mRect, h / 2 + 8, h / 2 + 8, mFill);
        }
        mFill.setColor(Color.rgb(70, 74, 80));
        mRect.set(x, y + 3, x + w, y + h + 3);
        c.drawRoundRect(mRect, h / 2, h / 2, mFill);
        mFill.setShader(new LinearGradient(x, y, x, y + h, sel ? Color.rgb(236, 240, 238) : Color.rgb(206, 210, 215),
                sel ? Color.rgb(176, 186, 180) : Color.rgb(150, 155, 162), Shader.TileMode.CLAMP));
        mRect.set(x, y, x + w, y + h);
        c.drawRoundRect(mRect, h / 2, h / 2, mFill);
        mFill.setShader(null);
        mStroke.setShader(null);
        mStroke.setStrokeWidth(sel ? 3 : 1.5f);
        mStroke.setColor(sel ? Color.argb(220, 96, 255, 110) : Color.argb(120, 255, 255, 255));
        c.drawRoundRect(mRect, h / 2, h / 2, mStroke);
    }

    private void arrow(Canvas c, float x, float y, boolean up) {
        mFill.setShader(null);
        mFill.setColor(ENGRAVE);
        float d = up ? -1 : 1;
        for (int i = 0; i < 3; i++) c.drawCircle(x - 24 + i * 24, y + d * (i == 1 ? 6 : 0), 5, mFill);
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
        mFill.setColor(state == 2 ? colour : state == 1
                ? Color.rgb(Color.red(colour) * 3 / 10, Color.green(colour) * 3 / 10, Color.blue(colour) * 3 / 10) : STEEL_LO);
        c.drawCircle(x, y, r, mFill);
        mFill.setColor(Color.argb(state == 2 ? 200 : 90, 255, 255, 255));
        c.drawCircle(x - r * 0.35f, y - r * 0.35f, r * 0.32f, mFill);
    }

    /** A generic atom (nucleus and three orbits, the electrons going round) in phosphor green. */
    private void atom(Canvas c, float x, float y, float r, long t) {
        mStroke.setShader(null);
        mStroke.setStrokeWidth(5);
        mFill.setShader(null);
        for (int i = 0; i < 3; i++) {
            c.save();
            c.rotate(i * 60, x, y);
            mStroke.setColor(Color.argb(200, 110, 255, 140));
            mRect.set(x - r, y - r * 0.36f, x + r, y + r * 0.36f);
            c.drawOval(mRect, mStroke);
            double a = (t / 900.0 + i * 2.1) % (2 * Math.PI);
            mFill.setColor(Color.argb(80, 110, 255, 140));
            c.drawCircle(x + (float) Math.cos(a) * r, y + (float) Math.sin(a) * r * 0.36f, 18, mFill);
            mFill.setColor(Color.rgb(200, 255, 210));
            c.drawCircle(x + (float) Math.cos(a) * r, y + (float) Math.sin(a) * r * 0.36f, 9, mFill);
            c.restore();
        }
        mFill.setShader(new RadialGradient(x - 6, y - 6, r * 0.2f, new int[] { Color.rgb(220, 255, 225), LED_ON }, null,
                Shader.TileMode.CLAMP));
        c.drawCircle(x, y, r * 0.15f, mFill);
        mFill.setShader(null);
    }

    private void scanlines(Canvas c, RectF r) {
        mStroke.setShader(null);
        mStroke.setStrokeWidth(1);
        mStroke.setColor(Color.argb(46, 0, 0, 0));
        for (float y = r.top; y < r.bottom; y += 4) c.drawLine(r.left, y, r.right, y, mStroke);
    }

    // ------------------------------------------------------------------ the static chrome

    private void buildChrome(int w, int h, float u, float ox, float oy) {
        if (mChrome != null) mChrome.recycle();
        mChrome = Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888);
        mChromeW = w;
        mChromeH = h;
        Canvas c = new Canvas(mChrome);
        Bitmap bg = !mBottom && mModel.background instanceof Bitmap ? (Bitmap) mModel.background : null;
        if (bg != null) {
            // the player's picture over the whole screen, cover-fit (cropped to the screen's
            // shape), dimmed so the plates stay readable
            float sc = Math.max((float) w / bg.getWidth(), (float) h / bg.getHeight());
            float bw = bg.getWidth() * sc, bh = bg.getHeight() * sc;
            c.drawBitmap(bg, null, new RectF((w - bw) / 2, (h - bh) / 2, (w + bw) / 2, (h + bh) / 2), mBmp);
            mFill.setShader(null);
            mFill.setColor(Color.argb(110, 0, 0, 0));
            c.drawRect(0, 0, w, h, mFill);
        } else {
            brushed(c, 0, 0, w, h, Color.rgb(150, 156, 163), Color.rgb(112, 118, 126));
        }
        c.save();
        c.translate(ox, oy);
        c.scale(u, u);
        if (mBottom && mCover) {
            // the cover in a steel plate like the panel's (bevel, corner screws), on darker steel
            mFill.setShader(null);
            mFill.setColor(Color.argb(110, 10, 12, 16));
            c.drawRect(-4000, -4000, BW + 4000, BH + 4000, mFill);   // the whole screen
            Bitmap b = coverBitmap();
            RectF in = new RectF();
            coverRect(b, in);
            plate(c, new RectF(in.left - COVER_BEZEL, in.top - COVER_BEZEL, in.right + COVER_BEZEL, in.bottom + COVER_BEZEL));
            if (b == null) crt(c, in);
            else {
                mFill.setColor(Color.argb(150, 255, 255, 255));        // the recess the picture sits in
                c.drawRect(in.left - 3, in.top - 3, in.right + 4, in.bottom + 4, mFill);
                mFill.setColor(Color.rgb(40, 44, 50));
                c.drawRect(in.left - 4, in.top - 4, in.right + 1, in.bottom + 1, mFill);
            }
        } else if (mBottom) {
            plate(c, B_HEAD);
            plate(c, B_LIST);
            plate(c, B_PREV);
            lcd(c, B_HEAD.left + 18, B_HEAD.top + 16, B_HEAD.width() - 36, B_HEAD.height() - 32);
            crt(c, new RectF(B_LIST.left + BEZEL, B_LIST.top + BEZEL, B_LIST.right - BEZEL, B_LIST.bottom - BEZEL));
            lcd(c, B_PREV.left + 30, B_PREV.top + 30, B_PREV.width() - 60, B_PREV.height() - 60);
        } else {
            plate(c, T_HEAD);
            plate(c, T_MENU);
            plate(c, T_INFO);
            lcd(c, T_HEAD.left + 22, T_HEAD.top + 20, T_HEAD.width() - 44, T_HEAD.height() - 40);
            crt(c, new RectF(T_INFO.left + BEZEL, T_INFO.top + BEZEL, T_INFO.right - BEZEL, T_INFO.bottom - BEZEL));
        }
        c.restore();
    }

    private void crt(Canvas c, RectF crt) {
        Paint p = new Paint(Paint.ANTI_ALIAS_FLAG);
        p.setColor(Color.rgb(40, 44, 48));
        RectF o = new RectF(crt.left - 6, crt.top - 6, crt.right + 6, crt.bottom + 6);
        c.drawRoundRect(o, 26, 26, p);
        p.setShader(new RadialGradient(crt.centerX(), crt.centerY(), Math.max(crt.width(), crt.height()) * 0.7f,
                new int[] { Color.rgb(10, 34, 16), CRT_BG }, null, Shader.TileMode.CLAMP));
        c.drawRoundRect(crt, 20, 20, p);
        p.setShader(null);
        p.setStyle(Paint.Style.STROKE);
        p.setStrokeWidth(3);
        p.setColor(Color.argb(120, 255, 255, 255));
        c.drawLine(o.left + 20, o.bottom + 1, o.right - 20, o.bottom + 1, p);
    }

    /** Brushed steel: a gradient with fine horizontal streaks (same seed every time). */
    private void brushed(Canvas c, float l, float t, float r, float b, int top, int bottom) {
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
        p.setColor(Color.argb(90, 0, 0, 0));
        RectF s = new RectF(r.left + 3, r.top + 5, r.right + 3, r.bottom + 5);
        c.drawRoundRect(s, 22, 22, p);
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
        s.set(r.left + 1.5f, r.top + 1.5f, r.right - 1.5f, r.bottom - 1.5f);
        c.drawRoundRect(s, 21, 21, p);
        p.setColor(Color.argb(140, 40, 44, 50));
        p.setStrokeWidth(2);
        s.set(r.left + 3, r.top + 4, r.right - 0.5f, r.bottom - 0.5f);
        c.drawRoundRect(s, 21, 21, p);
        p.setStyle(Paint.Style.FILL);
        float k = 13;
        screw(c, r.left + k, r.top + k, p);
        screw(c, r.right - k, r.top + k, p);
        screw(c, r.left + k, r.bottom - k, p);
        screw(c, r.right - k, r.bottom - k, p);
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
}
