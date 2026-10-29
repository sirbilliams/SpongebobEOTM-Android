package android.view;
import android.content.Context;
/** Compile-only gesture shim; desktop snapshots do not exercise pinch recognition. */
public class ScaleGestureDetector {
 public static class SimpleOnScaleGestureListener {public boolean onScale(ScaleGestureDetector d){return false;}}
 public ScaleGestureDetector(Context c,SimpleOnScaleGestureListener l){} public boolean onTouchEvent(MotionEvent e){return false;}
 public boolean isInProgress(){return false;} public float getScaleFactor(){return 1;}
}
