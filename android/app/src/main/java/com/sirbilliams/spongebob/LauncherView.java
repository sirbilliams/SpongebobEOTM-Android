package com.sirbilliams.spongebob;

import android.content.Context;
import android.graphics.*;
import android.os.SystemClock;
import android.view.*;

/**
 * Original procedural underwater launcher (docs/launcher.md). Everything is drawn in code on a letterboxed stage
 * (1920 x 1080 on the top screen, 1240 x 1080 in bottom-screen mode): a dock overhead, light through the deck boards,
 * kelp / coral / sand at three depths, bubbles, drifting jellyfish, menu signs hanging from the title plaque and a
 * bottle holding the page text. Page changes "dive": the water column scrolls and the signs are hauled in on their ropes.
 * Per-frame work allocates nothing: shapes and shaders are built once, scratch paths are reset and reused.
 */
public final class LauncherView extends View {
    // port: shared labels for the Captain's cursor preference.
    static final String[] CURSORS = {"Spatula", "Net", "Bubble", "Classic"};
    /** Top screen: select(row) / back(). Bottom screen: select(dial) when a dial is tapped. */
    public interface Actions { void select(int row); void back(); }
    /** Optional cues (LauncherSound). */
    public interface Sounds { void move(); void select(); void tap(); }

    // ---------------------------------------------------------------- stage layout
    static final float H = 1080, TOP_W = 1920, BOTTOM_W = 1240;
    static final float BEAM_T = 54, BEAM_B = 122;
    static final float SIGN_X = 132, SIGN_W = 760, SIGN_H = 84, SIGN_TOP = 246;
    static final float PLAQUE_CX = SIGN_X + SIGN_W / 2, PLAQUE_T = 66, PLAQUE_B = 206;
    static final float BOTTLE_CX = 1462, BOTTLE_T = 214, BOTTLE_B = 918, BOTTLE_HW = 300;
    static final long DIVE_MS = 860;
    static final float COLUMN_PX = 300;     // background gradient scroll per page depth
    static final float STREAM_PX = 280;     // bubbles / snow / gauge scroll per page depth
    static final int MAX_ROWS = 12;
    static final float ROPE_IN = 128;      // rope eye screws, in from each sign end
    private static final float[] JIT_X = {0, 26, -6, 18, -12, 22, 6, -4, 14, -10, 18, 0};
    private static final float[] JIT_W = {0, -40, 12, -22, 20, -10, -28, 14, -12, 6, -20, 0};
    static float rowStep(int n) { return n > 9 ? 76 : n > 8 ? 85 : n > 7 ? 92 : n > 6 ? 98 : 106; }
    static float rowTop(int n, int i) { return SIGN_TOP + i * rowStep(n); }
    static float rowLeft(int i) { return SIGN_X + JIT_X[i % JIT_X.length]; }
    static float rowWidth(int i) { return SIGN_W + JIT_W[i % JIT_W.length]; }
    static final float TEXT_W = 2 * BOTTLE_HW - 144;
    // the box-art frame on a page with the save card (below its few signs)
    static final float ART_L = 190, ART_T = 540, ART_R = 830, ART_B = 900;
    // bottom-screen dials
    static final float DIAL_R = 112;
    static float dialX(int i) { return 730 + (i % 2) * 300; }
    static float dialY(int i) { return 400 + (i / 2) * 330; }

    private static final class Page {
        String title = "", detail = "";
        String[] rows = new String[0], status = new String[0];
        float[] rowSize = new float[0];
        float titleSize = 52, detailSize = 34;
        String[] detailLines = new String[0];
        int depth, progress = -1;
        String progressText = "", depthText = "";
        boolean card;
    }

    private final Actions actions;
    private final boolean bottom;
    private final float W;
    private Sounds sounds;
    private final Paint p = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint bmp = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.FILTER_BITMAP_FLAG);
    private final RectF r = new RectF();
    private final Rect src = new Rect();
    private final Path path = new Path();
    private final Typeface bold = Typeface.create("sans-serif", Typeface.BOLD);
    private final Typeface heavy = Typeface.create("sans-serif-black", Typeface.NORMAL);
    private final Typeface medium = Typeface.create("sans-serif-medium", Typeface.NORMAL);

    private Page cur = new Page(), old = new Page();
    private int selected, oldSelected, pressed = -1;
    private boolean busy, stickHeld, reducedMotion, diving;
    private float camDepth, diveFrom, diveTo, diveE = 1;
    private long diveStart, lastFrame = -1;
    private float t;                                   // scene time, seconds
    private final float[] focus = new float[MAX_ROWS], oldFocus = new float[MAX_ROWS];
    private final float[] swing = new float[MAX_ROWS], swingV = new float[MAX_ROWS];
    // per-frame sign transforms: [page 0 = current, 1 = old][row]
    private final float[][] sx = new float[2][MAX_ROWS], sy = new float[2][MAX_ROWS], sa = new float[2][MAX_ROWS], ss = new float[2][MAX_ROWS];

    // save card ("Continue"), box art, bottom-screen dials
    private Bitmap thumb, art;
    private String cardHead = "", cardDate = "";
    private String[] cardLines = new String[0];
    private int cardChapter;
    private String[] dialLabel = new String[0], dialValue = new String[0];
    private int[] dialIndex = new int[0], dialCount = new int[0];
    private final float[] dialAngle = new float[4], dialPress = new float[4], dialValueSize = new float[4], dialLabelW = new float[4];

    public LauncherView(Context context, Actions actions) { this(context, actions, false); }
    /** bottom: the calm bottom-screen scene with the save card and the settings dials. */
    public LauncherView(Context context, Actions actions, boolean bottom) {
        super(context); this.actions = actions; this.bottom = bottom; W = bottom ? BOTTOM_W : TOP_W;
        setFocusable(!bottom); setFocusableInTouchMode(!bottom);
        setContentDescription(bottom ? "Bottom screen: last save and settings dials." : "Underwater launcher. D-pad to navigate, A to select, B to return.");
        buildScene();
        if (bottom) camDepth = 1.4f;
    }
    public void setSounds(Sounds s) { sounds = s; }
    /** System animations off (animator duration scale 0): a still scene, instant page changes. */
    public void setReducedMotion(boolean on) { reducedMotion = on; invalidate(); }

    public void page(String title, String detail, String status, String... rows) { page(cur.depth, title, detail, status, rows); }
    /** Shows a page; depth (0 = surface) picks how far the camera dives. Re-showing the same page keeps its focus glow. */
    public void page(int depth, String title, String detail, String status, String... rows) {
        boolean same = title.equals(cur.title) && depth == cur.depth;
        if (!same) {
            if (cur.title.length() > 0 && !reducedMotion) {
                Page swap = old; old = cur; cur = swap;
                System.arraycopy(focus, 0, oldFocus, 0, MAX_ROWS);
                oldSelected = selected;
                diveFrom = camDepth; diveTo = depth; diveStart = SystemClock.uptimeMillis(); diving = true; diveE = 0;
                burst(BOTTLE_CX - 300, 980, 14, 1.6f);
            } else { camDepth = depth; diving = false; diveE = 1; }
            for (int i = 0; i < MAX_ROWS; i++) { focus[i] = 0; swing[i] = 0; swingV[i] = 0; }
        }
        Page pg = cur;
        pg.title = title; pg.detail = detail; pg.rows = rows; pg.depth = depth; pg.card = false;
        pg.depthText = "~  " + (2 + depth * 6) + " m below the dock  ~";
        pg.rowSize = new float[rows.length];
        p.setTypeface(bold);
        for (int i = 0; i < rows.length; i++) pg.rowSize[i] = fit(rows[i], 38, 24, rowWidth(i) - 150);
        p.setTypeface(heavy); pg.titleSize = fit(title, 54, 34, TEXT_W);
        p.setTypeface(medium); pg.detailSize = fit(detail.indexOf('\n') < 0 ? detail : "", 34, 29, TEXT_W);
        pg.detailLines = wrap(detail, pg.detailSize, TEXT_W);
        setStatusLines(pg, status);
        selected = 0; pressed = -1;
        if (reducedMotion) focus[0] = 1;
        StringBuilder d = new StringBuilder(title).append(". ").append(detail).append(". ");
        for (String row : rows) d.append(row).append(", ");
        setContentDescription(d.toString());
        invalidate();
    }
    /** The page just shown carries the save card in its bottle and the box-art frame below its signs. */
    public void withCard() { cur.card = true; invalidate(); }
    public void focusRow(int row) {
        selected = Math.max(0, Math.min(row, cur.rows.length - 1));
        if (reducedMotion) for (int i = 0; i < MAX_ROWS; i++) focus[i] = i == selected ? 1 : 0;
        invalidate();
    }
    public void status(String text, boolean busy) { this.busy = busy; setStatusLines(cur, text); invalidate(); }
    /** The last save: thumbnail (or null), "Chapter 2", the place name, the date; head null = no save. */
    public void setCard(Bitmap thumbnail, String head, String place, String date, int chapter) {
        thumb = thumbnail; cardHead = head == null ? "" : head; cardDate = date == null ? "" : date; cardChapter = chapter;
        p.setTypeface(medium);
        cardLines = place == null ? new String[0] : wrap(bottom ? place : place.replace(" - ", "\n"), bottom ? 30 : 25, bottom ? 400 : TEXT_W - 218);
        invalidate();
    }
    public void setArt(Bitmap picture) { art = picture; invalidate(); }
    /** Bottom screen: up to four dials (label, value text, value index of count). */
    public void setDials(String[] labels, String[] values, int[] index, int[] count) {
        dialLabel = labels; dialValue = values; dialIndex = index; dialCount = count;
        p.setTypeface(bold);
        for (int i = 0; i < 4 && i < labels.length; i++) {
            dialValueSize[i] = fit(values[i], 28, 18, 2 * DIAL_R - 60);
            p.setTextSize(30); dialLabelW[i] = p.measureText(labels[i]) / 2 + 22;
        }
        invalidate();
    }

    // ---------------------------------------------------------------- text helpers (page changes only)
    private float fit(String s, float size, float min, float max) {
        p.setTextSize(size);
        while (size > min && p.measureText(s) > max) { size--; p.setTextSize(size); }
        return size;
    }
    private String[] wrap(String text, float size, float max) {
        p.setTextSize(size);
        java.util.ArrayList<String> out = new java.util.ArrayList<String>();
        for (String para : text.split("\n")) {
            String line = "";
            for (String word : para.split(" ")) {
                String next = line.length() == 0 ? word : line + " " + word;
                if (line.length() > 0 && p.measureText(next) > max) { out.add(line); line = word; } else line = next;
            }
            if (line.length() > 0 || para.length() == 0) out.add(line);
        }
        while (out.size() > 0 && out.get(out.size() - 1).length() == 0) out.remove(out.size() - 1);
        return out.toArray(new String[0]);
    }
    private void setStatusLines(Page pg, String text) {
        pg.progress = -1;
        String body = text == null ? "" : text;
        int nl = body.lastIndexOf('\n');
        String last = body.substring(nl + 1).trim();
        if (last.endsWith("%") && last.length() <= 4) {
            try { pg.progress = Integer.parseInt(last.substring(0, last.length() - 1)); pg.progressText = last; body = nl >= 0 ? body.substring(0, nl) : ""; }
            catch (NumberFormatException ignored) { }
        }
        p.setTypeface(medium);
        pg.status = wrap(body, 28, TEXT_W);
    }

    // ---------------------------------------------------------------- scene (built once)
    private Shader bgShader, rayShader, beamShader, plankShader, plaqueShader, plaqueText, parchShader, glassShader,
        sandBackShader, sandFrontShader, haloShader, pilingShader, deckShader, brassShader;
    private final Shader[] jellyHalo = new Shader[3], jellyBell = new Shader[3];
    private static final int RAYS = 7;
    private final float[] rayX = new float[RAYS], rayPh = new float[RAYS];
    private final Path[] rayPath = new Path[RAYS];
    private final Path ridge = new Path(), sandBack = new Path(), sandFront = new Path(), ripples = new Path(),
        beamGrain = new Path(), fringe = new Path(), bell = new Path(), bottle = new Path(), bottleHi = new Path(), plaqueGrain = new Path();
    private static final int KELP = 21;
    private final float[] kx = new float[KELP], kh = new float[KELP], kw = new float[KELP], kph = new float[KELP];
    private final int[] klayer = new int[KELP];
    private float[] coral = new float[0];     // x0 y0 x1 y1 width layer colour
    private int coralN;
    private float plaqueHW;
    private static final int JELLY = 3, AMBIENT = 34, SNOW = 44, POOL = 48;
    private final float[] jx0 = {300, 1150, 1700}, jy0 = {520, 330, 700}, jv = {16, -11, 9}, jr = {62, 46, 74}, jph = {0.3f, 2.1f, 4.0f};
    private final float[] jox = new float[JELLY], joy = new float[JELLY], jvx = new float[JELLY], jvy = new float[JELLY], jdim = new float[JELLY];
    private final float[] bx = new float[POOL], by = new float[POOL], bvy = new float[POOL], brad = new float[POOL], blife = new float[POOL];
    private static final String[] GAUGE = new String[61];
    static { for (int i = 0; i < GAUGE.length; i++) GAUGE[i] = Integer.toString(i); }
    private static final String PLAQUE_1 = "Employee of the Month", PLAQUE_2 = "THE  PORT  LAUNCHER", BOTTOM_TITLE = "Harbour office";
    private static final int[] CORAL_COL = {0xff9b4f63, 0xffb8703f, 0xff7c5a9a, 0xffd3735e, 0xffe39a5a, 0xffa86ac0};

    private static float rnd(int i) { double v = Math.sin(i * 12.9898 + 78.233) * 43758.5453; return (float) (v - Math.floor(v)); }

    private void buildScene() {
        bgShader = new LinearGradient(0, 0, 0, H + 3 * COLUMN_PX,
            new int[] {0xff5cc9d4, 0xff1f95ad, 0xff0e6687, 0xff0a4768, 0xff073152, 0xff05223f},
            new float[] {0, .14f, .36f, .58f, .8f, 1}, Shader.TileMode.CLAMP);
        rayShader = new LinearGradient(0, BEAM_B, 0, 1000, 0x70f2fdff, 0x00f2fdff, Shader.TileMode.CLAMP);
        for (int i = 0; i < RAYS; i++) {
            rayX[i] = (bottom ? 80 + i * 175 : 150 + i * 268) + rnd(i) * 80; rayPh[i] = rnd(i + 40) * 6.28f;
            float wTop = 10 + rnd(i + 3) * 10, wBot = 150 + rnd(i + 7) * 140, slant = 120 + rnd(i + 9) * 70;
            Path rp = new Path();
            rp.moveTo(-wTop, BEAM_T); rp.lineTo(wTop, BEAM_T); rp.lineTo(slant + wBot / 2, 1000); rp.lineTo(slant - wBot / 2, 1000); rp.close();
            rayPath[i] = rp;
        }
        deckShader = new LinearGradient(0, 0, 0, BEAM_T, 0xff22170f, 0xff3a2818, Shader.TileMode.CLAMP);
        beamShader = new LinearGradient(0, BEAM_T, 0, BEAM_B, new int[] {0xff7d5634, 0xff5c3d24, 0xff36231a},
            new float[] {0, .55f, 1}, Shader.TileMode.CLAMP);
        for (int i = 0; i < 4; i++) {
            float y = BEAM_T + 14 + i * 14;
            beamGrain.moveTo(0, y);
            for (int x = 0; x <= 1920; x += 160) beamGrain.quadTo(x + 80, y + (rnd(i * 31 + x) - .5f) * 9, x + 160, y + (rnd(i * 17 + x) - .5f) * 5);
        }
        for (int x = 8; x < 1920; x += 13) {    // weed fringe hanging under the beam
            float len = 8 + rnd(x) * 26 * (0.4f + 0.6f * (float) Math.abs(Math.sin(x * 0.004)));
            fringe.moveTo(x, BEAM_B - 2); fringe.quadTo(x + 4, BEAM_B + len * .6f, x + 1, BEAM_B + len);
        }
        plankShader = new LinearGradient(0, 0, 0, SIGN_H, new int[] {0xff7a5235, 0xff5e3e27, 0xff47301f},
            new float[] {0, .45f, 1}, Shader.TileMode.CLAMP);
        pilingShader = new LinearGradient(0, 0, 0, H, 0xff3a2a1d, 0x5a23303a, Shader.TileMode.CLAMP);
        brassShader = new RadialGradient(-30, -40, DIAL_R * 1.6f, new int[] {0xfff6dc98, 0xffc6964a, 0xff7a5426},
            new float[] {0, .55f, 1}, Shader.TileMode.CLAMP);
        p.setTypeface(heavy); p.setTextSize(56);
        plaqueHW = bottom ? 300 : Math.max(SIGN_W / 2 + 20, p.measureText(PLAQUE_1) / 2 + 64);
        float pcx = bottom ? W / 2 : PLAQUE_CX;
        plaqueShader = new LinearGradient(0, PLAQUE_T, 0, PLAQUE_B, new int[] {0xff8a5f3b, 0xff6d4a2e, 0xff4f3420},
            new float[] {0, .5f, 1}, Shader.TileMode.CLAMP);
        plaqueText = new LinearGradient(0, PLAQUE_T + 40, 0, PLAQUE_T + 100, 0xfffde3ad, 0xffd59e5c, Shader.TileMode.CLAMP);
        for (int i = 0; i < 5; i++) {
            float y = PLAQUE_T + 22 + i * 26, x0 = pcx - plaqueHW + 18, x1 = pcx + plaqueHW - 18;
            plaqueGrain.moveTo(x0, y);
            for (float x = x0; x < x1; x += 120) plaqueGrain.quadTo(x + 60, y + (rnd(i * 13 + (int) x) - .5f) * 10, Math.min(x1, x + 120), y + (rnd(i * 7 + (int) x) - .5f) * 6);
        }
        parchShader = new LinearGradient(0, -260, 0, 300, 0xfff8ecd0, 0xffe6cfa0, Shader.TileMode.CLAMP);
        glassShader = new LinearGradient(-BOTTLE_HW, 0, BOTTLE_HW, 0, new int[] {0x5a9ff7e6, 0x2a4fd6c4, 0x183aa89a, 0x4a7fe6d6},
            new float[] {0, .3f, .75f, 1}, Shader.TileMode.CLAMP);
        haloShader = new RadialGradient(0, 0, 100, new int[] {0xe0ffa8ea, 0x90c09cff, 0x3870e8ff, 0x0070e8ff},
            new float[] {0, .45f, .75f, 1}, Shader.TileMode.CLAMP);
        int[][] jc = {{0xffffa6e2, 0xffc77dff}, {0xff8ff4ff, 0xff5fa8ff}, {0xffd8b0ff, 0xffff8fc8}};
        for (int i = 0; i < JELLY; i++) {
            jellyHalo[i] = new RadialGradient(0, -30, 100, new int[] {(jc[i][0] & 0xffffff) | 0x58000000, (jc[i][1] & 0xffffff) | 0x1a000000, jc[i][1] & 0xffffff},
                new float[] {0, .55f, 1}, Shader.TileMode.CLAMP);
            jellyBell[i] = new RadialGradient(0, -40, 110, new int[] {(jc[i][0] & 0xffffff) | 0xe0000000, (jc[i][1] & 0xffffff) | 0x90000000, (jc[i][1] & 0xffffff) | 0x50000000},
                new float[] {0, .6f, 1}, Shader.TileMode.CLAMP);
        }
        if (bottom) { jx0[0] = 140; jx0[1] = 560; jx0[2] = 1000; jy0[0] = 860; jy0[1] = 760; jy0[2] = 880; jr[2] = 54; }
        // jellyfish bell, radius 100, rim on y = 0 with scallops
        bell.moveTo(-100, 0);
        bell.cubicTo(-104, -70, -58, -112, 0, -112);
        bell.cubicTo(58, -112, 104, -70, 100, 0);
        for (int i = 0; i < 8; i++) { float x0 = 100 - i * 25; bell.quadTo(x0 - 12.5f, 16, x0 - 25, 0); }
        bell.close();
        ridge.moveTo(-40, H);
        for (int x = -40; x <= 1960; x += 80) ridge.lineTo(x, 858 + (float) Math.sin(x * 0.006) * 34 + (float) Math.sin(x * 0.017 + 1) * 14 + rnd(x) * 10);
        ridge.lineTo(1960, H); ridge.close();
        sandBack.moveTo(-40, H + 240);
        for (int x = -40; x <= 1960; x += 60) sandBack.lineTo(x, 948 + (float) Math.sin(x * 0.004 + 2) * 18 + (float) Math.sin(x * 0.013) * 6);
        sandBack.lineTo(1960, H + 240); sandBack.close();
        sandBackShader = new LinearGradient(0, 930, 0, H, 0xff8d8a72, 0xff6f6c58, Shader.TileMode.CLAMP);
        sandFront.moveTo(-40, H + 240);
        sandFront.lineTo(-40, 1012);
        for (int x = 0; x <= 1960; x += 120) sandFront.quadTo(x - 60, 1000 + (float) Math.sin(x * 0.01) * 10, x, 1008 + (float) Math.sin(x * 0.005 + 1) * 8);
        sandFront.lineTo(1960, H + 240); sandFront.close();
        sandFrontShader = new LinearGradient(0, 995, 0, H, 0xffc8b88e, 0xffa8956c, Shader.TileMode.CLAMP);
        for (int k = 0; k < 6; k++) {
            float y = 1020 + k * 11 + rnd(k) * 4;
            for (int x = -20 + k * 37 % 90; x < 1940; x += 150 + (int) (rnd(k * 9 + x) * 60)) {
                float len = 60 + rnd(x + k) * 70;
                ripples.moveTo(x, y); ripples.quadTo(x + len / 2, y - 6, x + len, y);
            }
        }
        // kelp: 7 far, 9 mid, 5 near (near only at the edges, clear of the signs and bottle)
        for (int i = 0; i < KELP; i++) {
            int layer = i < 7 ? 0 : i < 16 ? 1 : 2; klayer[i] = layer;
            if (layer == 0) { kx[i] = 120 + i * 270 + rnd(i) * 120; kh[i] = 260 + rnd(i + 5) * 190; kw[i] = 20 + rnd(i + 8) * 10; }
            else if (layer == 1) { int j = i - 7; kx[i] = 60 + j * 225 + rnd(i) * 90; kh[i] = 380 + rnd(i + 5) * 260; kw[i] = 30 + rnd(i + 8) * 14; }
            else { float[] ex = bottom ? new float[] {22, 74, 1192, 1140, 1240} : new float[] {22, 74, 1872, 1820, 1022}; kx[i] = ex[i - 16]; kh[i] = 300 + rnd(i + 5) * 200; kw[i] = 40 + rnd(i + 8) * 14; }
            kph[i] = rnd(i + 21) * 6.28f;
        }
        if (bottom) for (int i = 0; i < KELP; i++) if (klayer[i] < 2) kx[i] *= BOTTOM_W / TOP_W;
        java.util.ArrayList<float[]> segs = new java.util.ArrayList<float[]>();
        float[][] corals = {{560, 940, 0, 1}, {1060, 950, 1, 1}, {1790, 930, 0, 1}, {300, 952, 2, 1}, {1240, 940, 1, 1},
                            {1000, 1030, 0, 2}, {1890, 1040, 2, 2}, {90, 1046, 1, 2}};
        for (int i = 0; i < corals.length; i++) {
            float cx = bottom ? corals[i][0] * BOTTOM_W / TOP_W : corals[i][0];
            branch(segs, cx, corals[i][1], -90 + (rnd(i) - .5f) * 16, 70 + rnd(i + 2) * 30,
                corals[i][3] == 2 ? 20 : 16, 4, (int) corals[i][3], (int) corals[i][2], i * 7);
        }
        coral = new float[segs.size() * 7]; coralN = segs.size();
        for (int i = 0; i < coralN; i++) System.arraycopy(segs.get(i), 0, coral, i * 7, 7);
        // bottle outline, local coords: centre x 0, top of the cork at y 0
        float hw = BOTTLE_HW, top = 128, bot = BOTTLE_B - BOTTLE_T, neck = 70;
        bottle.moveTo(-neck, 34);
        bottle.lineTo(-neck, 80);
        bottle.cubicTo(-neck, top - 10, -hw, top - 20, -hw, top + 70);
        bottle.lineTo(-hw, bot - 70);
        bottle.cubicTo(-hw, bot - 10, -hw + 40, bot, -hw + 90, bot);
        bottle.lineTo(hw - 90, bot);
        bottle.cubicTo(hw - 40, bot, hw, bot - 10, hw, bot - 70);
        bottle.lineTo(hw, top + 70);
        bottle.cubicTo(hw, top - 20, neck, top - 10, neck, 80);
        bottle.lineTo(neck, 34);
        bottle.close();
        bottleHi.moveTo(-hw + 30, top + 90); bottleHi.lineTo(-hw + 30, bot - 110);
        bottleHi.moveTo(-neck + 16, 52); bottleHi.lineTo(-neck + 16, 92);
    }
    private void branch(java.util.ArrayList<float[]> out, float x, float y, float ang, float len, float w, int depth, int layer, int col, int seed) {
        double a = Math.toRadians(ang);
        float x1 = x + (float) Math.cos(a) * len, y1 = y + (float) Math.sin(a) * len;
        out.add(new float[] {x, y, x1, y1, w, layer, col + (layer == 2 ? 3 : 0)});
        if (depth == 0) return;
        int kids = depth > 2 ? 2 : 1 + (rnd(seed) > .5f ? 1 : 0);
        for (int k = 0; k < kids; k++) {
            float spread = (kids == 1 ? (rnd(seed + k) - .5f) * 30 : (k == 0 ? -1 : 1) * (18 + rnd(seed + k * 3) * 20));
            branch(out, x1, y1, ang + spread, len * (0.68f + rnd(seed + k + 9) * .18f), w * .72f, depth - 1, layer, col, seed * 3 + k + 1);
        }
    }

    // ---------------------------------------------------------------- paint helpers
    private void fill(int color) { p.setShader(null); p.setStyle(Paint.Style.FILL); p.setColor(color); }
    private void stroke(int color, float width) { p.setShader(null); p.setStyle(Paint.Style.STROKE); p.setColor(color); p.setStrokeWidth(width); p.setStrokeCap(Paint.Cap.ROUND); }
    private void shade(Shader s, int alpha) { p.setShader(s); p.setStyle(Paint.Style.FILL); p.setColor(0xffffffff); p.setAlpha(alpha); }
    private void text(Canvas c, String s, float x, float y, float size, int color, Typeface tf, Paint.Align align) {
        fill(color); p.setTypeface(tf); p.setTextSize(size); p.setTextAlign(align); c.drawText(s, x, y, p);
    }
    private static float clamp(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }
    private static float ease(float x) { x = clamp(x); return x < .5f ? 4 * x * x * x : 1 - (float) Math.pow(-2 * x + 2, 3) / 2; }
    private static float wrapf(float v, float lo, float hi) { float span = hi - lo; v = (v - lo) % span; if (v < 0) v += span; return v + lo; }
    private static float sin(float a) { return (float) Math.sin(a); }
    private float scale() { return Math.min(getWidth() / W, getHeight() / H); }
    private static int blend(int a, int b, float f) {
        int ar = a >> 16 & 255, ag = a >> 8 & 255, ab = a & 255, br = b >> 16 & 255, bg = b >> 8 & 255, bb = b & 255;
        return 0xff000000 | (int) (ar + (br - ar) * f) << 16 | (int) (ag + (bg - ag) * f) << 8 | (int) (ab + (bb - ab) * f);
    }

    // ---------------------------------------------------------------- frame
    @Override protected void onDraw(Canvas c) {
        super.onDraw(c);
        long now = SystemClock.uptimeMillis();
        float dt = lastFrame < 0 ? 0 : Math.min(.05f, (now - lastFrame) / 1000f);
        lastFrame = now;
        t = reducedMotion ? 24 : (now % 3600000L) / 1000f;
        step(now, dt);
        c.drawColor(0xff021a2c);
        float s = scale(); if (s <= 0) return;
        c.save(); c.translate((getWidth() - W * s) / 2, (getHeight() - H * s) / 2); c.scale(s, s);
        c.clipRect(0, 0, W, H);
        drawWater(c);
        drawFloor(c);
        if (bottom) {
            drawFront(c);
            drawDock(c);
            drawBottom(c);
        } else {
            drawFront(c);
            float dir = diveTo >= diveFrom ? 1 : -1;
            if (diving) drawContent(c, old, 1, -dir * diveE * 1160, oldFocus, oldSelected);
            drawContent(c, cur, 0, diving ? dir * (1 - diveE) * 1160 : 0, focus, selected);
            drawDock(c);
            drawHints(c);
        }
        c.restore();
        if (!reducedMotion) postInvalidateOnAnimation();
    }

    private void step(long now, float dt) {
        if (diving) {
            float x = (now - diveStart) / (float) DIVE_MS;
            diveE = ease(x);
            camDepth = diveFrom + (diveTo - diveFrom) * diveE;
            if (x >= 1) { diving = false; diveE = 1; camDepth = diveTo; }
        }
        float k = reducedMotion ? 1 : 1 - (float) Math.exp(-dt * 11);
        for (int i = 0; i < MAX_ROWS; i++) {
            float target = i == selected ? 1 : 0;
            focus[i] += (target - focus[i]) * k;
            if (i == pressed) focus[i] = Math.max(focus[i], .6f);
            swingV[i] += (-34 * swing[i] - 5 * swingV[i]) * dt;      // damped spring for the focus swing (degrees)
            swing[i] += swingV[i] * dt;
        }
        for (int i = 0; i < 4 && i < dialIndex.length; i++) {
            float target = dialCount[i] > 1 ? -75 + 150f * dialIndex[i] / (dialCount[i] - 1) : 0;
            dialAngle[i] += (target - dialAngle[i]) * (reducedMotion ? 1 : 1 - (float) Math.exp(-dt * 7));
            dialPress[i] *= (float) Math.exp(-dt * 5);
        }
        for (int i = 0; i < JELLY; i++) {
            jox[i] += jvx[i] * dt; joy[i] += jvy[i] * dt;
            float damp = (float) Math.exp(-dt * 2.4f);
            jvx[i] *= damp; jvy[i] *= damp;
            joy[i] *= (float) Math.exp(-dt * .25f);
            jdim[i] *= (float) Math.exp(-dt * 1.1f);
        }
        for (int i = 0; i < POOL; i++) if (blife[i] > 0) {
            blife[i] -= dt; by[i] -= bvy[i] * dt; bvy[i] += 60 * dt;
            bx[i] += sin(t * 5 + i) * 18 * dt;
        }
    }

    private float current() { return sin(t * .11f) * .6f + sin(t * .043f + 1) * .4f; }

    private void drawWater(Canvas c) {
        float bgY = camDepth * COLUMN_PX;
        c.save(); c.translate(0, -bgY); shade(bgShader, 255); c.drawRect(0, bgY, W, bgY + H, p); c.restore();
        float light = clamp(1 - camDepth * .27f);
        c.save(); c.translate(0, -camDepth * 14);       // far ridge, far kelp, far coral
        for (int i = 0; i < KELP; i++) if (klayer[i] == 0) kelp(c, i, 880, 0xff0f5a72);
        fill(0xff0c4d66); c.drawPath(ridge, p);
        drawCoral(c, 0);
        c.restore();
        p.setShader(rayShader); p.setStyle(Paint.Style.FILL); p.setColor(0xffffffff);   // light shafts from the deck gaps
        for (int i = 0; i < RAYS; i++) {
            float a = (.35f + .65f * (.5f + .5f * sin(t * .37f + rayPh[i]))) * light;
            if (a < .02f) continue;
            p.setAlpha((int) (a * 255));
            c.save(); c.translate(rayX[i] + sin(t * .05f + rayPh[i]) * 40 + current() * 20, 0); c.drawPath(rayPath[i], p); c.restore();
        }
        fill(0x55d8f0f0);                                   // marine snow
        for (int i = 0; i < SNOW; i++) {
            float y = wrapf(rnd(i + 300) * H + t * (6 + rnd(i) * 10) - camDepth * STREAM_PX * .6f, -10, H + 10);
            float x = wrapf(rnd(i + 200) * W + t * 9 * current() + sin(t * .3f + i) * 14, -10, W + 10);
            c.drawCircle(x, y, 1.2f + rnd(i + 99) * 1.8f, p);
        }
        // pilings; the right one carries a painted depth gauge
        float rp = W - 84;
        shade(pilingShader, 255); c.drawRect(34, 0, 94, H, p); c.drawRect(rp, 0, rp + 62, H, p);
        stroke(0x30000000, 3); c.drawLine(52, 0, 52, H, p); c.drawLine(rp + 20, 0, rp + 20, H, p);
        float meters = 2 + camDepth * 6;
        for (int m = Math.max(0, (int) meters - 9); m <= meters + 10 && m < GAUGE.length; m++) {
            float y = 560 + (m - meters) * 64;
            boolean big = m % 5 == 0;
            stroke(0xd0f2e6c8, big ? 5 : 3); c.drawLine(rp + 4, y, rp + (big ? 40 : 26), y, p);
            if (big) text(c, GAUGE[m], rp + 31, y - 10, 26, 0xe0f2e6c8, bold, Paint.Align.CENTER);
        }
        fill(0xff8f8b7e);                                   // barnacles
        for (int i = 0; i < 9; i++) { c.drawCircle(38 + rnd(i) * 50, 860 + rnd(i + 5) * 160, 4 + rnd(i + 2) * 5, p); c.drawCircle(rp + 6 + rnd(i + 30) * 50, 820 + rnd(i + 35) * 200, 4 + rnd(i + 32) * 5, p); }
    }

    private void kelp(Canvas c, int i, float base, int color) {
        int n = 11; float h = kh[i], w = kw[i], cur = current() * (klayer[i] + 1) * 18;
        float amp = 22 + klayer[i] * 10, spd = .55f + rnd(i + 50) * .3f;
        path.reset();
        for (int side = 0; side < 2; side++) {
            for (int j = 0; j <= n; j++) {
                int k = side == 0 ? j : n - j;
                float f = k / (float) n;
                float x = kx[i] + amp * (float) Math.pow(f, 1.4) * sin(t * spd + kph[i] - k * .38f) + cur * f * f;
                float y = base + 30 - h * f;
                float half = w * (1 - .55f * f) * (.75f + .35f * sin(k * 1.7f + kph[i])) / 2;
                float xx = side == 0 ? x - half : x + half;
                if (j == 0 && side == 0) path.moveTo(xx, y); else path.lineTo(xx, y);
            }
        }
        path.close();
        fill(color); c.drawPath(path, p);
    }

    private void drawCoral(Canvas c, int layer) {
        p.setStyle(Paint.Style.STROKE); p.setShader(null); p.setStrokeCap(Paint.Cap.ROUND);
        for (int i = 0; i < coralN; i++) {
            int o = i * 7; if ((int) coral[o + 5] != layer) continue;
            int col = CORAL_COL[(int) coral[o + 6]];
            p.setColor(layer == 0 ? blend(col, 0xff0c4d66, .55f) : layer == 1 ? blend(col, 0xff0e5a70, .3f) : col);
            p.setStrokeWidth(coral[o + 4]);
            float sway = sin(t * .7f + coral[o] * .01f) * (1 - (coral[o + 3] - 700) / 400f) * 3;
            c.drawLine(coral[o], coral[o + 1], coral[o + 2] + sway, coral[o + 3], p);
        }
    }

    private void drawFloor(Canvas c) {
        c.save(); c.translate(0, -camDepth * 22);
        shade(sandBackShader, 255); c.drawPath(sandBack, p);
        for (int i = 0; i < KELP; i++) if (klayer[i] == 1) kelp(c, i, 950, i % 2 == 0 ? 0xff136a55 : 0xff1a7660);
        drawCoral(c, 1);
        float k = bottom ? BOTTOM_W / TOP_W : 1;
        fill(0xff4d5a5c); r.set(640 * k, 920, 640 * k + 150, 975); c.drawOval(r, p); r.set(1530 * k, 930, 1530 * k + 120, 978); c.drawOval(r, p);
        fill(0xff5f6c6b); r.set(660 * k, 924, 660 * k + 100, 950); c.drawOval(r, p);
        c.restore();
        for (int i = 0; i < JELLY; i++) jelly(c, i);
        // ambient bubbles, and a vent stream in the gap between the signs and the bottle
        for (int i = 0; i < AMBIENT; i++) {
            boolean vent = i < 9;
            float sp = vent ? 120 + rnd(i) * 40 : 45 + rnd(i) * 60;
            float y = H + 40 - wrapf(t * sp + rnd(i + 70) * 1300 + camDepth * STREAM_PX, 0, 1300);
            float x = (vent ? (bottom ? 610 : 1030) + rnd(i + 3) * 30 : rnd(i + 11) * W) + sin(t * 1.4f + i) * (vent ? 6 : 12) + current() * (H - y) * .04f;
            bubble(c, x, y, vent ? 3 + rnd(i + 5) * 5 : 4 + rnd(i + 5) * 10);
        }
        for (int i = 0; i < POOL; i++) if (blife[i] > 0) bubble(c, bx[i], by[i], brad[i]);
    }

    private void bubble(Canvas c, float x, float y, float rad) {
        fill(0x1ad8f6ff); c.drawCircle(x, y, rad, p);
        stroke(0x99d8f6ff, rad > 7 ? 2.4f : 1.8f); c.drawCircle(x, y, rad, p);
        fill(0xc0ffffff); c.drawCircle(x - rad * .35f, y - rad * .38f, Math.max(1.2f, rad * .24f), p);
    }

    private float jellyX(int i) { return wrapf(jx0[i] + t * jv[i] + jox[i] + current() * 30, -220, W + 220); }
    private float jellyY(int i) { return jy0[i] + 80 * sin(t * .19f + jph[i]) + joy[i] - camDepth * 110; }

    private void jelly(Canvas c, int i) {
        float x = jellyX(i), y = jellyY(i);
        float pulse = sin(t * (2.1f + i * .2f) + jph[i] + jdim[i] * 6);
        float alpha = 1 - .68f * clamp(jdim[i]);
        float rad = jr[i];
        c.save(); c.translate(x, y); c.rotate(sin(t * .4f + jph[i]) * 6 + jvx[i] * .02f);
        p.setShader(jellyHalo[i]); p.setStyle(Paint.Style.FILL); p.setColor(0xffffffff); p.setAlpha((int) (255 * alpha * (.75f + .25f * pulse)));
        c.save(); c.scale(rad / 60, rad / 60); c.drawCircle(0, -30, 100, p); c.restore();
        path.reset();                                       // tentacles hang from the rim
        for (int k = 0; k < 6; k++) {
            float tx = (-.75f + k * .3f) * rad;
            path.moveTo(tx, 0);
            for (int j = 1; j <= 8; j++) {
                float f = j / 8f;
                path.lineTo(tx + sin(t * 2.4f + k + j * .7f + jph[i]) * 9 * f - jvx[i] * .4f * f, f * rad * (1.9f - pulse * .15f));
            }
        }
        stroke(i == 1 ? 0xff9ff2ff : 0xffffc4ee, 2.6f); p.setAlpha((int) (150 * alpha)); c.drawPath(path, p);
        path.reset();
        for (int k = 0; k < 2; k++) {
            float tx = (k == 0 ? -.18f : .18f) * rad;
            path.moveTo(tx, 0);
            for (int j = 1; j <= 6; j++) { float f = j / 6f; path.lineTo(tx + sin(t * 1.6f + k * 2 + j) * 12 * f, f * rad * 1.15f); }
        }
        p.setStrokeWidth(7); p.setAlpha((int) (120 * alpha)); c.drawPath(path, p);
        c.scale(rad / 100 * (1 + .07f * pulse), rad / 100 * (1 - .09f * pulse));
        p.setShader(jellyBell[i]); p.setStyle(Paint.Style.FILL); p.setColor(0xffffffff); p.setAlpha((int) (255 * alpha)); c.drawPath(bell, p);
        stroke(0xffffffff, 4); p.setAlpha((int) (110 * alpha)); r.set(-62, -84, 62, 30); c.drawArc(r, 200, 140, false, p);
        c.restore();
    }

    private void drawFront(Canvas c) {
        c.save(); c.translate(0, -camDepth * 30);
        shade(sandFrontShader, 255); c.drawPath(sandFront, p);
        stroke(0x40755f3c, 3); c.drawPath(ripples, p);
        float light = clamp(1 - camDepth * .27f);          // light dapples on the sand
        if (light > .05f) {
            fill(0xffffffff);
            for (int i = 0; i < 9; i++) {
                float x = wrapf(rnd(i + 500) * W + t * 14 + sin(t * .6f + i) * 30, -80, W + 80), y = 1030 + rnd(i + 510) * 40;
                p.setAlpha((int) (28 * light * (.5f + .5f * sin(t * 1.3f + i * 2))));
                r.set(x - 46 - rnd(i) * 30, y - 7, x + 46 + rnd(i) * 30, y + 7); c.drawOval(r, p);
            }
        }
        drawCoral(c, 2);
        for (int i = 0; i < KELP; i++) if (klayer[i] == 2) kelp(c, i, 1050, i % 2 == 0 ? 0xff1d7a4a : 0xff268a54);
        c.restore();
    }

    // ---------------------------------------------------------------- dock and plaque
    private void drawDock(Canvas c) {
        shade(deckShader, 255); c.drawRect(0, 0, W, BEAM_T, p);    // deck boards seen from below, light through the gaps
        for (int x = 0; x < W; x += 128) {
            float a = .55f + .45f * sin(t * .9f + x * .013f);
            fill(0xffd8fbff); p.setAlpha((int) (200 * a)); c.drawRect(x + 121, 0, x + 127, BEAM_T, p);
            fill(0x18ffffff); c.drawRect(x + 6, 0, x + 12, BEAM_T, p);
        }
        c.save(); c.translate(current() * 3, 0);
        stroke(0xff2f5a3a, 3); c.drawPath(fringe, p);
        c.restore();
        shade(beamShader, 255); c.drawRect(0, BEAM_T, W, BEAM_B, p);
        stroke(0x30000000, 2); c.drawPath(beamGrain, p);
        stroke(0x40ffe2b8, 2); c.drawLine(0, BEAM_T + 2, W, BEAM_T + 2, p);
        fill(0xff2a1a10); c.drawRect(0, BEAM_B - 4, W, BEAM_B, p);
        for (int x = 160; x < W; x += 320) { fill(0xff3b3a38); c.drawCircle(x, BEAM_T + 34, 9, p); fill(0xff7c7a72); c.drawCircle(x - 2, BEAM_T + 32, 4, p); }
        float pcx = bottom ? W / 2 : PLAQUE_CX, pb = bottom ? PLAQUE_B - 30 : PLAQUE_B;
        float l = pcx - plaqueHW, rr = pcx + plaqueHW;
        fill(0x55000000); r.set(l + 8, PLAQUE_T + 12, rr + 8, pb + 14); c.drawRoundRect(r, 30, 30, p);
        shade(plaqueShader, 255); r.set(l, PLAQUE_T, rr, pb); c.drawRoundRect(r, 30, 30, p);
        stroke(0x38000000, 2.5f); c.drawPath(plaqueGrain, p);
        stroke(0xff2c1b10, 4); c.drawRoundRect(r, 30, 30, p);
        stroke(0x50ffd9a8, 2); r.set(l + 10, PLAQUE_T + 9, rr - 10, pb - 9); c.drawRoundRect(r, 22, 22, p);
        for (int k = 0; k < 4; k++) { float bxp = k % 2 == 0 ? l + 28 : rr - 28, byp = k < 2 ? PLAQUE_T + 28 : pb - 28;
            fill(0xff2b2a28); c.drawCircle(bxp, byp, 9, p); fill(0xffb2a27f); c.drawCircle(bxp - 2, byp - 2, 4.5f, p); }
        if (bottom) { carve(c, BOTTOM_TITLE, pcx, PLAQUE_T + 76, 50); return; }
        carve(c, PLAQUE_1, pcx, PLAQUE_T + 92, 56);
        text(c, PLAQUE_2, pcx, PLAQUE_T + 128, 22, 0xffe7c590, bold, Paint.Align.CENTER);
    }
    /** Raised letters cut from the plaque: shadow, dark rim, light rim, then a warm wood-grain face. */
    private void carve(Canvas c, String s, float x, float y, float size) {
        text(c, s, x + 3, y + 5, size, 0x90170c05, heavy, Paint.Align.CENTER);
        text(c, s, x + 1.5f, y + 2, size, 0xff3a2414, heavy, Paint.Align.CENTER);
        text(c, s, x - 1.2f, y - 1.5f, size, 0xa0ffeccc, heavy, Paint.Align.CENTER);
        p.setShader(plaqueText); p.setColor(0xffffffff); c.drawText(s, x, y, p); p.setShader(null);
    }

    // ---------------------------------------------------------------- signs, ropes, bottle, art
    private void layoutSigns(Page pg, int slot, float offY, float[] f) {
        int n = Math.min(pg.rows.length, MAX_ROWS);
        for (int i = 0; i < n; i++) {
            float lag = i * .55f;
            float w = rowWidth(i);
            sx[slot][i] = rowLeft(i) + w / 2 + sin(t * .8f + lag) * 5 + current() * 6 + f[i] * 34;
            sy[slot][i] = rowTop(pg.rows.length, i) + offY + sin(t * 1.1f + i) * 2.5f;
            sa[slot][i] = sin(t * .8f + lag + .6f) * .9f + (slot == 0 ? swing[i] : 0);
            ss[slot][i] = 1 + .055f * f[i];
        }
    }
    /** Local sign point (lx from centre, ly from top) to stage coordinates. */
    private float tx(int slot, int i, float lx, float ly) {
        double a = Math.toRadians(sa[slot][i]); float s = ss[slot][i];
        return sx[slot][i] + (float) (lx * Math.cos(a) - ly * Math.sin(a)) * s;
    }
    private float ty(int slot, int i, float lx, float ly) {
        double a = Math.toRadians(sa[slot][i]); float s = ss[slot][i];
        return sy[slot][i] + (float) (lx * Math.sin(a) + ly * Math.cos(a)) * s;
    }

    private void drawContent(Canvas c, Page pg, int slot, float offY, float[] f, int sel) {
        int n = Math.min(pg.rows.length, MAX_ROWS);
        layoutSigns(pg, slot, offY, f);
        if (pg.card && art != null) artFrame(c, offY);
        for (int i = 0; i < n; i++) if (f[i] > .02f) {       // jellyfish glow behind the focused sign
            float pulse = .8f + .2f * sin(t * 2.6f);
            p.setShader(haloShader); p.setStyle(Paint.Style.FILL); p.setColor(0xffffffff); p.setAlpha((int) (255 * f[i] * pulse));
            c.save(); c.translate(sx[slot][i], sy[slot][i] + SIGN_H / 2);
            c.scale(rowWidth(i) / 200 * 1.45f, SIGN_H / 200 * 5.2f); c.drawCircle(0, 0, 100, p); c.restore();
        }
        for (int i = 0; i < n; i++) {                      // ropes: plaque eye hooks to the first sign, then sign to sign
            float hw = rowWidth(i) / 2 - ROPE_IN;
            for (int side = -1; side <= 1; side += 2) {
                float x1 = tx(slot, i, side * hw, 12), y1 = ty(slot, i, side * hw, 12);
                float x0, y0;
                if (i == 0) { x0 = PLAQUE_CX + side * (SIGN_W / 2 - ROPE_IN); y0 = PLAQUE_B - 8; }
                else { float phw = rowWidth(i - 1) / 2 - ROPE_IN; x0 = tx(slot, i - 1, side * phw, SIGN_H - 12); y0 = ty(slot, i - 1, side * phw, SIGN_H - 12); }
                if (y1 < y0 + 4) continue;
                float slack = (1 - f[i]) * (6 + Math.min(40, (y1 - y0) * .05f));
                rope(c, x0, y0, x1, y1, side * slack, f[i]);
            }
        }
        for (int i = 0; i < n; i++) if (i != sel) sign(c, pg, slot, i, f[i]);
        if (sel < n) sign(c, pg, slot, sel, f[sel]);
        bottleCard(c, pg, offY);
    }

    private void rope(Canvas c, float x0, float y0, float x1, float y1, float sag, float taut) {
        path.reset(); path.moveTo(x0, y0); path.quadTo((x0 + x1) / 2 + sag, (y0 + y1) / 2, x1, y1);
        stroke(0xff4a3520, 7); c.drawPath(path, p);
        stroke(taut > .5f ? 0xffe8c993 : 0xffc6a473, 3); c.drawPath(path, p);
    }

    private void sign(Canvas c, Page pg, int slot, int i, float f) {
        float w = rowWidth(i), hw = w / 2;
        c.save(); c.translate(sx[slot][i], sy[slot][i]); c.rotate(sa[slot][i]); c.scale(ss[slot][i], ss[slot][i]);
        fill(0x50000000); r.set(-hw + 8 + f * 6, 10 + f * 8, hw + 8 + f * 6, SIGN_H + 10 + f * 8); c.drawRoundRect(r, 24, 24, p);
        c.save(); c.translate(-hw, 0); shade(plankShader, 255); r.set(0, 0, w, SIGN_H); c.drawRoundRect(r, 24, 24, p); c.restore();
        stroke(0x36000000, 2);                              // grain and weathering
        for (int k = 0; k < 3; k++) { float y = 20 + k * 20 + rnd(i * 5 + k) * 6; c.drawLine(-hw + 24 + rnd(i + k) * 40, y, hw - 30 - rnd(i * 3 + k) * 60, y + (rnd(k + i * 7) - .5f) * 6, p); }
        stroke(0x40ffdfb0, 2); c.drawLine(-hw + 22, 4, hw - 22, 4, p);
        fill(0xff8c877a);
        if (i % 3 != 1) { c.drawCircle(-hw + 30, SIGN_H - 14, 6, p); c.drawCircle(-hw + 44, SIGN_H - 9, 4, p); c.drawCircle(-hw + 21, SIGN_H - 24, 3.5f, p); }
        else { c.drawCircle(hw - 28, SIGN_H - 12, 5, p); c.drawCircle(hw - 40, SIGN_H - 8, 3.5f, p); }
        r.set(-hw, 0, hw, SIGN_H);
        stroke(0xff2a190e, 3.5f); c.drawRoundRect(r, 24, 24, p);
        if (f > .02f) {
            float pulse = .75f + .25f * sin(t * 2.6f);
            stroke(0xffffb2ec, 5); p.setAlpha((int) (230 * f * pulse)); r.set(-hw - 4, -4, hw + 4, SIGN_H + 4); c.drawRoundRect(r, 28, 28, p);
            stroke(0xffa8f6ff, 2); p.setAlpha((int) (200 * f)); r.set(-hw + 3, 3, hw - 3, SIGN_H - 3); c.drawRoundRect(r, 21, 21, p);
        }
        float e = hw - ROPE_IN;                                  // eye screws for the ropes
        stroke(0xffb9ab86, 3);
        for (int side = -1; side <= 1; side += 2) { c.drawCircle(side * e, 12, 6, p); c.drawCircle(side * e, SIGN_H - 12, 6, p); }
        float bxp = -hw + 54, byp = SIGN_H / 2;            // a small corked bottle marks each sign; the focused one lights up
        fill(f > .02f ? blend(0xff2f6f74, 0xffffc6f0, f) : 0xff2f6f74); r.set(bxp - 15, byp - 10, bxp + 15, byp + 24); c.drawRoundRect(r, 10, 10, p);
        r.set(bxp - 6, byp - 22, bxp + 6, byp - 6); c.drawRoundRect(r, 3, 3, p);
        fill(0xffc79a5c); r.set(bxp - 5, byp - 28, bxp + 5, byp - 19); c.drawRoundRect(r, 2, 2, p);
        fill(0x90ffffff); r.set(bxp - 10, byp - 4, bxp - 5, byp + 16); c.drawRoundRect(r, 2, 2, p);
        float size = i < pg.rowSize.length ? pg.rowSize[i] : 34;
        text(c, pg.rows[i], -hw + 99, SIGN_H / 2 + size * .36f + 2, size, 0xa0140a04, bold, Paint.Align.LEFT);
        text(c, pg.rows[i], -hw + 97, SIGN_H / 2 + size * .36f, size, f > .5f ? 0xfffff3d6 : 0xfff0dcb8, bold, Paint.Align.LEFT);
        c.restore();
    }

    /** Draws b centre-cropped to fill dst (the reused src / r). */
    private void picture(Canvas c, Bitmap b, float l, float tp, float rr, float bt) {
        float bw = b.getWidth(), bh = b.getHeight(), want = (rr - l) / (bt - tp);
        if (bw / bh > want) { int cw = (int) (bh * want); src.set((int) (bw - cw) / 2, 0, (int) (bw + cw) / 2, (int) bh); }
        else { int ch = (int) (bw / want); src.set(0, (int) (bh - ch) / 2, (int) bw, (int) (bh + ch) / 2); }
        r.set(l, tp, rr, bt); c.drawBitmap(b, src, r, bmp);
    }

    /** The player's own picture in a driftwood frame, propped on the sand below the Play page's signs. */
    private void artFrame(Canvas c, float offY) {
        float tilt = -1.6f + sin(t * .5f) * .3f;
        c.save(); c.translate(0, offY); c.rotate(tilt, (ART_L + ART_R) / 2, ART_B);
        fill(0x55000000); r.set(ART_L - 10, ART_T + 4, ART_R + 22, ART_B + 22); c.drawRoundRect(r, 18, 18, p);
        fill(0xff6a4a30); r.set(ART_L - 22, ART_T - 22, ART_R + 22, ART_B + 22); c.drawRoundRect(r, 18, 18, p);
        stroke(0xff2c1b10, 4); c.drawRoundRect(r, 18, 18, p);
        stroke(0x50ffd9a8, 2); r.set(ART_L - 14, ART_T - 14, ART_R + 14, ART_B + 14); c.drawRoundRect(r, 12, 12, p);
        picture(c, art, ART_L, ART_T, ART_R, ART_B);
        stroke(0xff2c1b10, 3); r.set(ART_L, ART_T, ART_R, ART_B); c.drawRect(r, p);
        fill(0x30ffffff); path.reset(); path.moveTo(ART_L, ART_T); path.lineTo(ART_L + 180, ART_T); path.lineTo(ART_L, ART_T + 140); path.close(); c.drawPath(path, p);
        fill(0xff8c877a); c.drawCircle(ART_R + 6, ART_B + 4, 8, p); c.drawCircle(ART_R - 12, ART_B + 12, 5, p);
        c.restore();
    }

    private void bottleCard(Canvas c, Page pg, float offY) {
        float bob = sin(t * .9f) * 8, tilt = sin(t * .6f + 1) * .7f;
        float top = BOTTLE_T + offY + bob;
        float anchorY = 1004 - camDepth * 30;               // tether to a stone on the sand
        if (anchorY > top + BOTTLE_B - BOTTLE_T) {
            path.reset(); path.moveTo(BOTTLE_CX + 120, top + BOTTLE_B - BOTTLE_T - 4);
            path.quadTo(BOTTLE_CX + 170 + current() * 20, (top + BOTTLE_B - BOTTLE_T + anchorY) / 2, BOTTLE_CX + 150, anchorY);
            stroke(0xff4a3520, 6); c.drawPath(path, p); stroke(0xffc6a473, 2.5f); c.drawPath(path, p);
            fill(0xff59615f); r.set(BOTTLE_CX + 112, anchorY - 16, BOTTLE_CX + 196, anchorY + 18); c.drawOval(r, p);
        }
        c.save(); c.translate(BOTTLE_CX, top); c.rotate(tilt);
        float bot = BOTTLE_B - BOTTLE_T, hw = BOTTLE_HW;
        fill(0x40000000); c.save(); c.translate(10, 14); c.drawPath(bottle, p); c.restore();
        float pl = -hw + 38, pr = hw - 38, pt = 160, pb = bot - 46;   // parchment rolled inside
        c.save(); c.translate(0, (pt + pb) / 2); shade(parchShader, 255); r.set(pl, pt - (pt + pb) / 2, pr, pb - (pt + pb) / 2); c.drawRoundRect(r, 12, 12, p); c.restore();
        fill(0xffcdb07a); r.set(pl - 10, pt - 14, pr + 10, pt + 14); c.drawRoundRect(r, 14, 14, p);
        r.set(pl - 10, pb - 14, pr + 10, pb + 14); c.drawRoundRect(r, 14, 14, p);
        fill(0xffe9d4a4); r.set(pl - 4, pt - 9, pr + 4, pt + 3); c.drawRoundRect(r, 8, 8, p);
        r.set(pl - 4, pb - 9, pr + 4, pb + 3); c.drawRoundRect(r, 8, 8, p);
        float x = pl + 34, y = pt + 34 + pg.titleSize;
        text(c, pg.title, x, y, pg.titleSize, 0xff4b2a12, heavy, Paint.Align.LEFT);
        y += 22; stroke(0x705a3a1c, 3); path.reset(); path.moveTo(x, y); path.quadTo(x + 160, y - 6, x + 360, y + 2); c.drawPath(path, p);
        fill(0xff7a4f22); c.drawCircle(x + 378, y + 2, 5, p);
        y += 18;
        for (int k = 0; k < pg.detailLines.length; k++) { y += pg.detailSize + 8; text(c, pg.detailLines[k], x, y, pg.detailSize, 0xff6b3d18, medium, Paint.Align.LEFT); }
        if (pg.card && cardHead.length() > 0) {
            y += 26;
            float tw = 200, th = 150;
            fill(0xff3a2414); r.set(x - 5, y - 5, x + tw + 5, y + th + 5); c.drawRoundRect(r, 6, 6, p);
            if (thumb != null) picture(c, thumb, x, y, x + tw, y + th);
            else { fill(0xff6a8f90); r.set(x, y, x + tw, y + th); c.drawRect(r, p); text(c, "no picture", x + tw / 2, y + th / 2 + 8, 22, 0xffe8f4f0, medium, Paint.Align.CENTER); }
            float cx = x + tw + 18, cy = y + 26;
            text(c, cardHead, cx, cy, 28, 0xff4b2a12, heavy, Paint.Align.LEFT);
            for (int k = 0; k < cardLines.length && k < 4; k++) { cy += 32; text(c, cardLines[k], cx, cy, 25, 0xff5a3a1e, medium, Paint.Align.LEFT); }
            y += th + 36;
            text(c, cardDate, x, y, 23, 0xff7a5a3a, medium, Paint.Align.LEFT);
        }
        y += 18;
        for (int k = 0; k < pg.status.length; k++) { y += 38; text(c, pg.status[k], x, y, 28, 0xff4f3d2c, medium, Paint.Align.LEFT); }
        if (pg.progress >= 0) {
            y += 34;
            float bw = pr - pl - 68;
            fill(0xffcdb48a); r.set(x, y, x + bw, y + 30); c.drawRoundRect(r, 15, 15, p);
            fill(0xff2f8f8c); r.set(x + 4, y + 4, x + 4 + (bw - 8) * clamp(pg.progress / 100f), y + 26); c.drawRoundRect(r, 11, 11, p);
            text(c, pg.progressText, x + bw / 2, y + 23, 22, 0xff2b1c10, bold, Paint.Align.CENTER);
        }
        if (!pg.card) text(c, pg.depthText, 0, pb - 30, 24, 0xff9a7a52, medium, Paint.Align.CENTER);
        shade(glassShader, 255); c.drawPath(bottle, p);     // glass over the parchment
        stroke(0x90c8fff6, 3.5f); c.drawPath(bottle, p);
        stroke(0x70ffffff, 9); c.drawPath(bottleHi, p);
        fill(0xff2b6f68); r.set(-74, 30, 74, 46); c.drawRoundRect(r, 8, 8, p);
        fill(0xffb7834d); r.set(-56, -4, 56, 40); c.drawRoundRect(r, 10, 10, p);   // cork
        fill(0xffd8a669); r.set(-56, -4, 56, 10); c.drawRoundRect(r, 8, 8, p);
        fill(0x50000000); for (int k = 0; k < 5; k++) c.drawCircle(-36 + k * 18, 22 + (k % 2) * 6, 3, p);
        c.restore();
    }

    private void drawHints(Canvas c) {
        float y = 1046, cx = 560;
        fill(0xb8062533); r.set(cx - 420, y - 28, cx + 420, y + 24); c.drawRoundRect(r, 26, 26, p);
        stroke(0x5099e6ea, 2); c.drawRoundRect(r, 26, 26, p);
        if (busy) { text(c, "Installing - please keep this screen open", cx, y + 9, 26, 0xfff3e6c8, bold, Paint.Align.CENTER); return; }
        key(c, cx - 380, y, "+"); text(c, "Move", cx - 348, y + 9, 26, 0xfff3e6c8, bold, Paint.Align.LEFT);
        key(c, cx - 200, y, "A"); text(c, "Select", cx - 168, y + 9, 26, 0xfff3e6c8, bold, Paint.Align.LEFT);
        key(c, cx - 10, y, "B"); text(c, "Back", cx + 22, y + 9, 26, 0xfff3e6c8, bold, Paint.Align.LEFT);
        text(c, "Tap the water", cx + 400, y + 9, 22, 0xb0bfe9ea, medium, Paint.Align.RIGHT);
    }
    private void key(Canvas c, float x, float y, String k) {
        fill(0xfff0dcb8); c.drawCircle(x, y, 18, p);
        text(c, k, x, y + 8, 22, 0xff3a2414, bold, Paint.Align.CENTER);
    }

    // ---------------------------------------------------------------- bottom screen: save card, progress pearls, dials
    private void drawBottom(Canvas c) {
        float l = 110, tp = 230, rr = 560;
        fill(0x55000000); r.set(l - 10, tp + 6, rr + 26, 960); c.drawRoundRect(r, 26, 26, p);
        fill(0xff6a4a30); r.set(l - 18, tp - 18, rr + 18, 950); c.drawRoundRect(r, 26, 26, p);
        stroke(0xff2c1b10, 4); c.drawRoundRect(r, 26, 26, p);
        c.save(); c.translate(0, (tp + 930) / 2); shade(parchShader, 255); r.set(l, tp - (tp + 930) / 2, rr, 930 - (tp + 930) / 2); c.drawRoundRect(r, 14, 14, p); c.restore();
        float x = l + 24, y = tp + 56;
        text(c, cardHead.length() > 0 ? "Last voyage" : "No saves yet", x, y, 40, 0xff4b2a12, heavy, Paint.Align.LEFT);
        y += 24;
        float pw = rr - l - 48, ph = pw * 3 / 4;
        Bitmap pic = thumb != null ? thumb : art;
        fill(0xff3a2414); r.set(x - 5, y - 5, x + pw + 5, y + ph + 5); c.drawRoundRect(r, 8, 8, p);
        if (pic != null) picture(c, pic, x, y, x + pw, y + ph);
        else { fill(0xff5f8a8c); r.set(x, y, x + pw, y + ph); c.drawRect(r, p); text(c, "Play a while, then save", x + pw / 2, y + ph / 2 + 10, 26, 0xffe8f4f0, medium, Paint.Align.CENTER); }
        y += ph + 50;
        if (cardHead.length() > 0) {
            text(c, cardHead, x, y, 32, 0xff4b2a12, heavy, Paint.Align.LEFT);
            for (int k = 0; k < cardLines.length && k < 2; k++) { y += 36; text(c, cardLines[k], x, y, 30, 0xff5a3a1e, medium, Paint.Align.LEFT); }
            y += 34; text(c, cardDate, x, y, 24, 0xff7a5a3a, medium, Paint.Align.LEFT);
        }
        for (int k = 0; k < 4; k++) {                       // chapter progress: four pearls in shells
            float px = x + 30 + k * 104, py = 852;
            fill(0xffb98a6a); r.set(px - 36, py - 22, px + 36, py + 26); c.drawArc(r, 0, 180, true, p);
            boolean done = k < cardChapter;
            fill(done ? 0xfff6eef8 : 0xff8f7c6c); c.drawCircle(px, py, 17, p);
            if (done) { fill(0xc0ffffff); c.drawCircle(px - 6, py - 6, 5, p); }
            text(c, GAUGE[k + 1], px, py + 52, 22, 0xff5a3a1e, bold, Paint.Align.CENTER);
        }
        text(c, "Chapters", x + 186, 806, 24, 0xff7a5a3a, bold, Paint.Align.CENTER);
        for (int i = 0; i < 4 && i < dialLabel.length; i++) dial(c, i);
        fill(0xb8062533); r.set(620, 1020, 1180, 1066); c.drawRoundRect(r, 23, 23, p);
        text(c, "Tap a dial - applies on next launch", 900, 1051, 24, 0xfff3e6c8, medium, Paint.Align.CENTER);
    }
    private void dial(Canvas c, int i) {
        float x = dialX(i), y = dialY(i), pr = 1 + .06f * dialPress[i];
        c.save(); c.translate(x, y); c.scale(pr, pr);
        fill(0x50000000); c.drawCircle(8, 12, DIAL_R + 8, p);
        p.setShader(brassShader); p.setStyle(Paint.Style.FILL); p.setColor(0xffffffff); c.drawCircle(0, 0, DIAL_R + 8, p);
        fill(0xfff3ead2); c.drawCircle(0, 0, DIAL_R - 10, p);
        stroke(0xff7a5426, 3); c.drawCircle(0, 0, DIAL_R - 10, p);
        int n = Math.max(2, dialCount[i]);
        for (int k = 0; k < n; k++) {
            double a = Math.toRadians(-90 - 75 + 150.0 * k / (n - 1));
            float cs = (float) Math.cos(a), sn = (float) Math.sin(a);
            stroke(k == dialIndex[i] ? 0xffc0396b : 0xff5a3a1e, k == dialIndex[i] ? 7 : 4);
            c.drawLine(cs * (DIAL_R - 34), sn * (DIAL_R - 34), cs * (DIAL_R - 18), sn * (DIAL_R - 18), p);
        }
        c.save(); c.rotate(dialAngle[i]);
        stroke(0xff2b1c10, 8); c.drawLine(0, 8, 0, -(DIAL_R - 40), p);
        fill(0xffc0396b); path.reset(); path.moveTo(-9, -(DIAL_R - 52)); path.lineTo(9, -(DIAL_R - 52)); path.lineTo(0, -(DIAL_R - 30)); path.close(); c.drawPath(path, p);
        c.restore();
        fill(0xff2b1c10); c.drawCircle(0, 0, 14, p); fill(0xffd8b070); c.drawCircle(-3, -3, 5, p);
        text(c, dialValue[i], 0, 58, dialValueSize[i], 0xff2b1c10, bold, Paint.Align.CENTER);
        c.restore();
        float lw = dialLabelW[i];
        fill(0xb8062533); r.set(x - lw, y + DIAL_R + 18, x + lw, y + DIAL_R + 62); c.drawRoundRect(r, 22, 22, p);
        text(c, dialLabel[i], x, y + DIAL_R + 51, 30, 0xfff3e6c8, bold, Paint.Align.CENTER);
    }

    // ---------------------------------------------------------------- reactions
    private void burst(float x, float y, int count, float speed) {
        if (reducedMotion) return;
        int made = 0;
        for (int i = 0; i < POOL && made < count; i++) if (blife[i] <= 0) {
            float a = rnd((int) (t * 1000) + i * 7);
            bx[i] = x + (a - .5f) * 60; by[i] = y + (rnd(i + (int) (t * 333)) - .5f) * 30;
            bvy[i] = (80 + a * 140) * speed; brad[i] = 4 + rnd(i * 3 + (int) (t * 77)) * 11; blife[i] = 2.2f + a * 1.6f;
            made++;
        }
    }
    private void tapWater(float x, float y) {
        burst(x, y, 10, 1);
        for (int i = 0; i < JELLY; i++) {
            float dx = jellyX(i) - x, dy = jellyY(i) - y, d = (float) Math.sqrt(dx * dx + dy * dy);
            if (d < 520) {
                float push = 520 * (1 - d / 520) + 120;
                jvx[i] += dx / Math.max(1, d) * push; jvy[i] += dy / Math.max(1, d) * push;
                jdim[i] = 1;
            }
        }
        if (sounds != null) sounds.tap();
    }

    // ---------------------------------------------------------------- input
    private void move(int delta) {
        int n = cur.rows.length;
        if (n > 0 && !busy) {
            selected = (selected + delta + n) % n;
            swingV[selected] += delta > 0 ? 70 : -70;
            if (sounds != null) sounds.move();
            invalidate();
        }
    }
    private void choose(int row) {
        if (sounds != null) sounds.select();
        if (row < MAX_ROWS) burst(sx[0][row] - rowWidth(row) / 2 + 54, sy[0][row] + 10, 6, .8f);
        actions.select(row);
    }
    @Override public boolean onKeyDown(int key, KeyEvent event) {
        if (bottom) return super.onKeyDown(key, event);
        if (key == KeyEvent.KEYCODE_BACK || key == KeyEvent.KEYCODE_BUTTON_B) { if (!busy) actions.back(); return true; }
        if (key == KeyEvent.KEYCODE_DPAD_DOWN || key == KeyEvent.KEYCODE_DPAD_RIGHT) { move(1); return true; }
        if (key == KeyEvent.KEYCODE_DPAD_UP || key == KeyEvent.KEYCODE_DPAD_LEFT) { move(-1); return true; }
        if (key == KeyEvent.KEYCODE_BUTTON_A || key == KeyEvent.KEYCODE_DPAD_CENTER || key == KeyEvent.KEYCODE_ENTER) {
            if (event.getRepeatCount() == 0 && !busy && cur.rows.length > 0) choose(selected);
            return true;
        }
        return super.onKeyDown(key, event);
    }
    @Override public boolean onGenericMotionEvent(MotionEvent e) {
        if (bottom) return false;
        float axis = e.getAxisValue(MotionEvent.AXIS_Y);
        if (Math.abs(axis) < .3f) stickHeld = false;
        else if (Math.abs(axis) > .6f && !stickHeld) { move(axis > 0 ? 1 : -1); stickHeld = true; }
        return true;
    }
    private float stageX(MotionEvent e) { float s = scale(); return (e.getX() - (getWidth() - W * s) / 2) / s; }
    private float stageY(MotionEvent e) { float s = scale(); return (e.getY() - (getHeight() - H * s) / 2) / s; }
    /** Row under a stage point, using the signs' resting places (they only bob a few units). */
    int hit(float x, float y) {
        if (diving) return -1;
        int n = cur.rows.length;
        for (int i = 0; i < n; i++) {
            float top = rowTop(n, i), l = rowLeft(i);
            if (x >= l && x <= l + rowWidth(i) + 26 && y >= top && y <= top + SIGN_H) return i;
        }
        return -1;
    }
    /** Bottom screen: dial under a stage point, or -1. */
    int hitDial(float x, float y) {
        for (int i = 0; i < 4 && i < dialLabel.length; i++) {
            float dx = x - dialX(i), dy = y - dialY(i);
            if (dx * dx + dy * dy <= (DIAL_R + 40) * (DIAL_R + 40)) return i;
        }
        return -1;
    }
    @Override public boolean onTouchEvent(MotionEvent e) {
        if (scale() <= 0) return true;
        float x = stageX(e), y = stageY(e);
        if (bottom) {
            if (e.getActionMasked() == MotionEvent.ACTION_DOWN) { pressed = hitDial(x, y); if (pressed >= 0) dialPress[pressed] = 1; else tapWater(x, y); }
            if (e.getActionMasked() == MotionEvent.ACTION_UP) { int d = hitDial(x, y); if (d >= 0 && d == pressed) { if (sounds != null) sounds.select(); actions.select(d); } pressed = -1; }
            if (e.getActionMasked() == MotionEvent.ACTION_CANCEL) pressed = -1;
            invalidate(); return true;
        }
        if (busy) return true;
        if (e.getActionMasked() == MotionEvent.ACTION_DOWN) {
            pressed = hit(x, y);
            if (pressed >= 0) { if (pressed != selected) swingV[pressed] += 60; selected = pressed; }
            else tapWater(x, y);
            invalidate(); return true;
        }
        if (e.getActionMasked() == MotionEvent.ACTION_UP) {
            int row = hit(x, y);
            if (row >= 0 && row == pressed) { performClick(); choose(row); }
            pressed = -1; return true;
        }
        if (e.getActionMasked() == MotionEvent.ACTION_CANCEL) pressed = -1;
        return true;
    }
    @Override public boolean performClick() { super.performClick(); return true; }
}
