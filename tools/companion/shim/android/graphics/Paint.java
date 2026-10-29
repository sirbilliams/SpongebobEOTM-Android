package android.graphics;
/** Desktop shim (tools/companion). */
public class Paint {
    public static final int ANTI_ALIAS_FLAG = 1, FILTER_BITMAP_FLAG = 2;
    public enum Style { FILL, STROKE }
    public enum Align { LEFT, CENTER, RIGHT }
    public enum Cap { BUTT, ROUND }
    int color = 0xff000000; Style style = Style.FILL; float strokeWidth = 1; Cap cap = Cap.BUTT;
    float textSize = 12; Typeface typeface = Typeface.SANS_SERIF; Align align = Align.LEFT; Shader shader;
    public Paint() {}
    public Paint(int flags) {}
    int alpha = 255;
    public void setColor(int c) { color = c; }
    public void setAlpha(int a) { alpha = a; }
    public void setStyle(Style s) { style = s; }
    public void setStrokeWidth(float w) { strokeWidth = w; }
    public void setStrokeCap(Cap c) { cap = c; }
    public void setTextSize(float s) { textSize = s; }
    public void setTypeface(Typeface t) { typeface = t; }
    public void setTextAlign(Align a) { align = a; }
    public Shader setShader(Shader s) { shader = s; return s; }
    java.awt.Font font() { return new java.awt.Font(typeface.family, typeface.style == 1 ? java.awt.Font.BOLD : java.awt.Font.PLAIN, 1).deriveFont(textSize); }
    public float measureText(String t) {
        java.awt.image.BufferedImage b = new java.awt.image.BufferedImage(1, 1, java.awt.image.BufferedImage.TYPE_INT_ARGB);
        java.awt.Graphics2D g = b.createGraphics();
        g.setRenderingHint(java.awt.RenderingHints.KEY_TEXT_ANTIALIASING, java.awt.RenderingHints.VALUE_TEXT_ANTIALIAS_ON);
        g.setFont(font());
        float w = (float) g.getFontMetrics().getStringBounds(t, g).getWidth();
        g.dispose();
        return w;
    }
}
