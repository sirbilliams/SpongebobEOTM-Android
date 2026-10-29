package android.graphics;
import java.io.File;
/** Desktop shim (tools/companion): decodeFile through ImageIO; null when the file cannot be read, like Android. */
public class BitmapFactory {
    public static Bitmap decodeFile(String path) {
        try {
            java.awt.image.BufferedImage i = javax.imageio.ImageIO.read(new File(path));
            return i == null ? null : Bitmap.wrap(i);
        } catch (java.io.IOException e) { return null; }
    }
}
