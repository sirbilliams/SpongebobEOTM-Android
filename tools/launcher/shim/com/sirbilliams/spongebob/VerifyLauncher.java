package com.sirbilliams.spongebob;
import android.content.Context;
import android.view.*;
/** Exercises real navigation and touch transforms; no simulated Android Activity lifecycle. */
public final class VerifyLauncher {
 private static int row=-1, backs;
 private static void expect(boolean ok) {if(!ok)throw new AssertionError("launcher input");}
 public static void main(String[] args) {
  LauncherView v=new LauncherView(new Context(),new LauncherView.Actions(){public void select(int r){row=r;}public void back(){backs++;}});
  v.setTestSize(1920,1080);v.page("Test","","","One","Two","Three");
  KeyEvent key=new KeyEvent();
  v.onKeyDown(KeyEvent.KEYCODE_DPAD_UP,key);v.onKeyDown(KeyEvent.KEYCODE_BUTTON_A,key);expect(row==2);
  v.onKeyDown(KeyEvent.KEYCODE_DPAD_DOWN,key);v.onKeyDown(KeyEvent.KEYCODE_BUTTON_A,key);expect(row==0);
  v.onGenericMotionEvent(new MotionEvent(2,0,0,1));v.onGenericMotionEvent(new MotionEvent(2,0,0,1));
  v.onKeyDown(KeyEvent.KEYCODE_BUTTON_A,key);expect(row==1);
  v.onGenericMotionEvent(new MotionEvent(2,0,0,0));v.onGenericMotionEvent(new MotionEvent(2,0,0,1));
  v.onKeyDown(KeyEvent.KEYCODE_BUTTON_A,key);expect(row==2);
  v.onKeyDown(KeyEvent.KEYCODE_BUTTON_B,key);expect(backs==1);
  for(int width:new int[]{1920,1280,2400}) {
   v.setTestSize(width,1080);float s=Math.min(width/1920f,1),ox=(width-1920*s)/2,oy=(1080-1080*s)/2;
   float on=LauncherView.rowTop(3,1)+40, gap=LauncherView.rowTop(3,1)+LauncherView.SIGN_H+10;
   row=-1;v.onTouchEvent(new MotionEvent(0,ox+500*s,oy+on*s,0));v.onTouchEvent(new MotionEvent(1,ox+500*s,oy+on*s,0));expect(row==1);
   row=-1;v.onTouchEvent(new MotionEvent(0,ox+500*s,oy+gap*s,0));v.onTouchEvent(new MotionEvent(1,ox+500*s,oy+gap*s,0));expect(row==-1);
  }
  v.status("Installing",true);row=-1;v.onKeyDown(KeyEvent.KEYCODE_BUTTON_A,key);v.onKeyDown(KeyEvent.KEYCODE_BUTTON_B,key);expect(row==-1 && backs==1);
  LauncherView b=new LauncherView(new Context(),new LauncherView.Actions(){public void select(int r){row=r;}public void back(){backs++;}},true);
  b.setTestSize(1240,1080);b.setDials(new String[]{"a","b","c","d"},new String[]{"1","2","3","4"},new int[]{0,0,0,0},new int[]{2,2,2,2});
  row=-1;b.onTouchEvent(new MotionEvent(0,LauncherView.dialX(3),LauncherView.dialY(3),0));b.onTouchEvent(new MotionEvent(1,LauncherView.dialX(3),LauncherView.dialY(3),0));expect(row==3);
  row=-1;b.onTouchEvent(new MotionEvent(0,300,100,0));b.onTouchEvent(new MotionEvent(1,300,100,0));expect(row==-1);
  System.out.println("Launcher input: PASS (D-pad, stick latch, A/B, touch at three aspect ratios, import lock, bottom-screen dials)");
 }
}
