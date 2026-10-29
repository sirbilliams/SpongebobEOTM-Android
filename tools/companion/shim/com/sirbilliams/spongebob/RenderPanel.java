package com.sirbilliams.spongebob;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.view.MotionEvent;
import java.io.File;
import java.nio.file.Files;
import javax.imageio.ImageIO;

/** Desktop check of the companion panel (tools/companion/render.sh): draws the real CompanionView
 *  from a snapshot JSON file (an empty / missing file = "no game yet").
 *  Optional 3rd argument: "pad" = the Touchpad layout, "pad:tap" / "pad:drag" / "pad:hold" also performs that gesture on the pad
 *  (the Pad callbacks print what the game would receive) and renders the view while the finger is still down. */
public final class RenderPanel {
    public static void main(String[] a) throws Exception {
        if (a.length < 2) {
            System.err.println("usage: RenderPanel <snap.json|-> <out.png> [pad|pad:tap|pad:drag|pad:hold]");
            System.exit(2);
        }
        String json = a[0].equals("-") ? null : new String(Files.readAllBytes(new File(a[0]).toPath()), "UTF-8");
        CompanionView v = new CompanionView(new Context());
        v.setTestSize(1240, 1080);
        v.setSnapshot(json);
        v.setPad(new CompanionView.Pad() {
            @Override public void move(float dx, float dy, int b) { System.out.println("pad dx=" + dx + " dy=" + dy + " button=" + b); }
        });
        v.setSender(new CompanionView.Sender() {
            @Override public void send(String cmd) { System.out.println("send " + cmd); }
        });
        Bitmap out = Bitmap.createBitmap(1240, 1080, Bitmap.Config.ARGB_8888);
        String m = a.length > 2 ? a[2] : "";
        if (m.equals("test")) {                       // the launcher's Touch test screen (TouchTestView), a finger mid-swipe
            TouchTestView tv = new TouchTestView(new Context());
            tv.setTestSize(1240, 1080);
            tv.setInfo("TOUCH TEST - bottom screen", new String[] { "Touches that reached the bottom window:   3",
                "Touches that reached the top (game) window: 1", "  ... of those forwarded to the bottom:  0",
                "Last device: 7(example touchscreen)", "Bottom display id 4, window not focusable", "Touch each screen. Back = leave." });
            for (int i = 0; i < 20; i++) tv.onTouchEvent(ev(1000 + i * 16, i == 0 ? MotionEvent.ACTION_DOWN : MotionEvent.ACTION_MOVE, 300 + i * 30, 600 - i * 8));
            tv.drawForTest(new Canvas(out));
            ImageIO.write(out.img, "png", new File(a[1]));
            return;
        }
        if (m.equals("banner") && json != null) v.setSnapshot(json.replace("\"n\":2", "\"n\":3"));    // a new combine result arrives: the banner shows
        if (m.startsWith("page:")) {                   // page:map | page:talk | page:goals | page:pocket (+ ":pad" = Touchpad mode)
            String[] parts = m.split(":");
            v.setPage(parts[1].equals("map") ? CompanionView.PAGE_MAP : parts[1].equals("talk") ? CompanionView.PAGE_TALK
                    : parts[1].equals("goals") ? CompanionView.PAGE_GOALS : CompanionView.PAGE_POCKET);
            if (parts.length > 2 && parts[2].equals("pad")) v.setMode(CompanionView.MODE_PAD);
        }
        if (m.equals("combine") || m.equals("combine:mid") || m.equals("maptap") || m.equals("tabtap")) {
            // gestures with the real touch code; the Sender prints what the game would receive
            v.drawForTest(new Canvas(Bitmap.createBitmap(1240, 1080, Bitmap.Config.ARGB_8888)));
            long t = 1000;
            if (m.startsWith("combine")) {                // drag the first card onto the second
                String[] ids = System.getProperty("cards", "isp1,ict2").split(",");
                float[] f = v.hitCenterForTest(ids[0]), g = v.hitCenterForTest(ids[1]);
                v.onTouchEvent(ev(t, MotionEvent.ACTION_DOWN, f[0], f[1]));
                int steps = 10;
                int stop = m.equals("combine:mid") ? steps - 2 : steps;
                for (int i = 1; i <= stop; i++) v.onTouchEvent(ev(t + i * 16, MotionEvent.ACTION_MOVE, f[0] + (g[0] - f[0]) * i / steps, f[1] + (g[1] - f[1]) * i / steps));
                if (!m.equals("combine:mid")) v.onTouchEvent(ev(t + 200, MotionEvent.ACTION_UP, g[0], g[1]));
            } else if (m.equals("maptap")) {              // tap every place of the chart
                int k = 0;
                String[] cmds = System.getProperty("taps", "").split(";");
                for (String cmd : cmds) {
                    float[] f = v.hitCenterForTest(cmd);
                    if (f == null) { System.out.println("no target " + cmd); continue; }
                    v.onTouchEvent(ev(t + k * 400, MotionEvent.ACTION_DOWN, f[0], f[1])); v.onTouchEvent(ev(t + k * 400 + 60, MotionEvent.ACTION_UP, f[0], f[1]));
                    v.drawForTest(new Canvas(Bitmap.createBitmap(1240, 1080, Bitmap.Config.ARGB_8888))); k++;
                }
            } else {                                      // tabtap: Map tab, then Talk tab
                for (String cmd : System.getProperty("taps", "tab:1").split(";")) {
                    float[] f = v.hitCenterForTest(cmd);
                    v.onTouchEvent(ev(t, MotionEvent.ACTION_DOWN, f[0], f[1])); v.onTouchEvent(ev(t + 60, MotionEvent.ACTION_UP, f[0], f[1]));
                    v.drawForTest(new Canvas(Bitmap.createBitmap(1240, 1080, Bitmap.Config.ARGB_8888)));
                    t += 200;
                }
                System.out.println("page " + v.getPage());
            }
        }
        if (m.startsWith("pad")) {
            v.setMode(CompanionView.MODE_PAD);
            v.drawForTest(new Canvas(Bitmap.createBitmap(1240, 1080, Bitmap.Config.ARGB_8888)));   // lay out once: the pad's rectangle
            long t = 1000;
            if (m.equals("pad:tap")) {
                v.onTouchEvent(ev(t, MotionEvent.ACTION_DOWN, 600, 400)); v.onTouchEvent(ev(t + 90, MotionEvent.ACTION_UP, 600, 400));
            } else if (m.equals("pad:drag")) {
                v.onTouchEvent(ev(t, MotionEvent.ACTION_DOWN, 300, 400));
                for (int i = 1; i <= 8; i++) v.onTouchEvent(ev(t + i * 16, MotionEvent.ACTION_MOVE, 300 + i * 12, 400 + i * 4));      // slow
                for (int i = 1; i <= 6; i++) v.onTouchEvent(ev(t + 140 + i * 16, MotionEvent.ACTION_MOVE, 396 + i * 60, 432 + i * 8));   // flick
            } else if (m.equals("pad:hold")) {
                v.onTouchEvent(ev(t, MotionEvent.ACTION_DOWN, 500, 450));
                v.onTouchEvent(ev(t + 400, MotionEvent.ACTION_MOVE, 502, 450));              // still: the hold arms (button down)
                for (int i = 1; i <= 5; i++) v.onTouchEvent(ev(t + 400 + i * 16, MotionEvent.ACTION_MOVE, 502 + i * 40, 450 + i * 10));
            }
        }
        v.drawForTest(new Canvas(out));
        ImageIO.write(out.img, "png", new File(a[1]));
    }

    private static MotionEvent ev(long t, int action, float x, float y) { return MotionEvent.obtain(t, t, action, x, y, 0); }
}
