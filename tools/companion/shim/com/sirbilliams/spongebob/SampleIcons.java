package com.sirbilliams.spongebob;

import java.awt.Color;
import java.awt.Graphics2D;
import java.awt.RenderingHints;
import java.awt.image.BufferedImage;
import java.io.File;
import javax.imageio.ImageIO;

/** Desktop check of the companion panel: writes simple stand-in pictures (own drawings, 96x96 with alpha) for the
 *  item ids of the sample snapshots, so the card layout can be checked without the game.  The game writes real
 *  pictures of the item models into the same kind of folder (Hud::ensureIcon). */
public final class SampleIcons {
    public static void main(String[] a) throws Exception {
        File dir = new File(a[0]);
        dir.mkdirs();
        String[] ids = { "isp1", "ict2", "ikp5", "ikc1", "iqu1", "ico1" };
        Color[] cols = { new Color(0xB0B0B8), new Color(0xB8901C), new Color(0xE8E8F0), new Color(0x2F6FD0), new Color(0x9A9AA2), new Color(0xE8D830) };
        for (int i = 0; i < ids.length; i++) {
            BufferedImage im = new BufferedImage(96, 96, BufferedImage.TYPE_INT_ARGB);
            Graphics2D g = im.createGraphics();
            g.setRenderingHint(RenderingHints.KEY_ANTIALIASING, RenderingHints.VALUE_ANTIALIAS_ON);
            g.setColor(cols[i]);
            switch (i % 3) {
                case 0: g.fillRoundRect(22, 30, 52, 52, 10, 10); g.setColor(cols[i].darker()); g.fillRect(40, 8, 8, 26); g.fillRect(30, 8, 28, 7); break;
                case 1: g.fillOval(12, 14, 72, 68); g.setColor(cols[i].darker()); g.drawOval(12, 14, 72, 68); break;
                default: g.fillRoundRect(26, 10, 44, 76, 14, 14); g.setColor(Color.WHITE); g.fillRect(34, 30, 28, 24); break;
            }
            g.dispose();
            ImageIO.write(im, "png", new File(dir, ids[i] + ".png"));
        }
    }
}
