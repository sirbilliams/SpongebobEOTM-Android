package android.view;
/** Desktop shim (tools/companion): just enough of MotionEvent to drive CompanionView's touch code from RenderPanel (one pointer). */
public class MotionEvent {
 public static final int ACTION_DOWN=0,ACTION_UP=1,ACTION_MOVE=2,ACTION_CANCEL=3,ACTION_POINTER_DOWN=5,ACTION_POINTER_UP=6;
 private int action=ACTION_CANCEL; private float x, y; private long time;
 public static MotionEvent obtain(long downTime, long eventTime, int action, float x, float y, int metaState) {
  MotionEvent e = new MotionEvent(); e.action = action; e.x = x; e.y = y; e.time = eventTime; return e;
 }
 public int getActionMasked(){return action;} public int getActionIndex(){return 0;}
 public long getEventTime(){return time;}
 public int getPointerCount(){return 1;} public float getX(){return x;} public float getY(){return y;}
 public float getX(int i){return x;} public float getY(int i){return y;}
}
