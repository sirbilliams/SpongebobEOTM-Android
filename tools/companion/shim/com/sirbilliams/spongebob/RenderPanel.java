package com.sirbilliams.spongebob;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import java.io.File;
import java.nio.file.Files;
import javax.imageio.ImageIO;

/** Desktop check of the companion panel (tools/companion/render.sh): draws the real CompanionView
 *  from a snapshot JSON file (an empty / missing file = "no game yet"). */
public final class RenderPanel {
    public static void main(String[] a) throws Exception {
        if (a.length < 2) {
            System.err.println("usage: RenderPanel <snap.json|-> <out.png>");
            System.exit(2);
        }
        String json = a[0].equals("-") ? null : new String(Files.readAllBytes(new File(a[0]).toPath()), "UTF-8");
        CompanionView v = new CompanionView(new Context());
        v.setTestSize(1240, 1080);
        v.setSnapshot(json);
        Bitmap out = Bitmap.createBitmap(1240, 1080, Bitmap.Config.ARGB_8888);
        v.drawForTest(new Canvas(out));
        ImageIO.write(out.img, "png", new File(a[1]));
    }
}
