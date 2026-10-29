package android.animation;
/** Deterministic endpoint-only animator for desktop snapshots, not animation timing tests. */
public class ValueAnimator {
 public interface AnimatorUpdateListener {void onAnimationUpdate(ValueAnimator a);}
 private float value; private AnimatorUpdateListener listener;
 public static ValueAnimator ofFloat(float... v){ValueAnimator a=new ValueAnimator();a.value=v[v.length-1];return a;}
 public void setDuration(long ms){} public void addUpdateListener(AnimatorUpdateListener l){listener=l;}
 public Object getAnimatedValue(){return value;} public void cancel(){} public void start(){if(listener!=null)listener.onAnimationUpdate(this);}
}
