package android.view;
/** Input samples for checking the launcher's actual coordinate mapping and stick latch. */
public class MotionEvent {
 public static final int ACTION_DOWN=0,ACTION_UP=1,ACTION_MOVE=2,ACTION_CANCEL=3,AXIS_Y=1;
 private final int action; private final float x,y,axis;
 public MotionEvent(int action,float x,float y,float axis){this.action=action;this.x=x;this.y=y;this.axis=axis;}
 public int getActionMasked(){return action;}
 public float getX(){return x;} public float getY(){return y;}
 public float getAxisValue(int axis){return this.axis;}
}
