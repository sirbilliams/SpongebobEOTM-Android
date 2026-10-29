package android.view;
import android.content.Context;
import android.graphics.Canvas;
/** Desktop shim (tools/companion): a size and onDraw. */
public class View {
    private int mW, mH;
    public View(Context c) {}
    public void setTestSize(int w, int h) { mW = w; mH = h; }
    public int getWidth() { return mW; }
    public int getHeight() { return mH; }
    public void invalidate() {}
    public void setBackgroundColor(int c) {}
    public void setContentDescription(String s) {}
    public boolean onTouchEvent(MotionEvent e) { return false; }
    /** Timers are not run by the shim: RenderPanel drives the touch code with event times instead. */
    public boolean postDelayed(Runnable r, long ms) { return true; }
    public boolean removeCallbacks(Runnable r) { return true; }
    public boolean performClick() { return true; }
    protected void onDetachedFromWindow() {}
    protected void onDraw(Canvas c) {}
    public void drawForTest(Canvas c) { onDraw(c); }
}
