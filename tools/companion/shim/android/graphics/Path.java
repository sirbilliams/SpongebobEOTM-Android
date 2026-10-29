package android.graphics;
/** Desktop shim (tools/companion). */
public class Path {
    final java.awt.geom.Path2D.Float p = new java.awt.geom.Path2D.Float();
    public void reset() { p.reset(); }
    public void moveTo(float x, float y) { p.moveTo(x, y); }
    public void lineTo(float x, float y) { p.lineTo(x, y); }
    public void close() { p.closePath(); }
}
