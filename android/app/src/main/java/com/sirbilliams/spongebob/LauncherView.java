package com.sirbilliams.spongebob;

import android.content.Context;
import android.graphics.*;
import android.os.SystemClock;
import android.view.*;

/** Original procedural nautical launcher; coordinates are a letterboxed 1920 x 1080 deck. */
public final class LauncherView extends View {
    public interface Actions { void select(int row); void back(); }
    private final Actions actions;
    private final Paint p = new Paint(Paint.ANTI_ALIAS_FLAG);
    private String title = "Welcome aboard", detail = "", status = "";
    private String[] rows = new String[0];
    private int selected, pressed = -1;
    private boolean busy, stickHeld;
    public LauncherView(Context context, Actions actions) {
        super(context); this.actions = actions;
        setFocusable(true); setFocusableInTouchMode(true);
        setContentDescription("Nautical launcher. D-pad to navigate, A to select, B to return.");
    }
    public void page(String title, String detail, String status, String... rows) {
        this.title=title; this.detail=detail; this.status=status; this.rows=rows;
        selected=0; pressed=-1;
        setContentDescription(title + ". " + java.util.Arrays.toString(rows));
        invalidate();
    }
    public void focusRow(int row) { selected=Math.max(0,Math.min(row,rows.length-1)); invalidate(); }
    public void status(String text, boolean busy) { this.status=text; this.busy=busy; invalidate(); }
    private void fill(int color) { p.setShader(null); p.setStyle(Paint.Style.FILL); p.setColor(color); }
    private void line(Canvas c,float x,float y,float xx,float yy,int color,float width) {
        fill(color); p.setStrokeWidth(width); c.drawLine(x,y,xx,yy,p);
    }
    private void text(Canvas c,String s,float x,float y,float size,int color) {
        fill(color); p.setTypeface(Typeface.create("sans-serif-rounded",Typeface.BOLD)); p.setTextSize(size);
        p.setTextAlign(Paint.Align.LEFT); c.drawText(s,x,y,p);
    }
    private void circle(Canvas c,float x,float y,float r,int color) { fill(color); c.drawCircle(x,y,r,p); }
    private float scale() { return Math.min(getWidth()/1920f,getHeight()/1080f); }
    @Override protected void onDraw(Canvas c) {
        super.onDraw(c);
        fill(0xff032c40); c.drawRect(0,0,getWidth(),getHeight(),p);
        float s=scale(); if(s<=0)return;
        c.save(); c.translate((getWidth()-1920*s)/2,(getHeight()-1080*s)/2); c.scale(s,s);
        p.setShader(new LinearGradient(0,0,0,1080,0xff087d93,0xff021e35,Shader.TileMode.CLAMP));
        c.drawRect(0,0,1920,1080,p); p.setShader(null);
        float time=(SystemClock.uptimeMillis()%24000)/24f;
        for(int i=0;i<32;i++) {
            float x=(i*317)%1920, y=1080-((i*83+time*(0.25f+i%3*.12f))%1160);
            fill(0xff489aaa); p.setStyle(Paint.Style.STROKE); p.setStrokeWidth(3);
            c.drawCircle(x,y,7+i%5*5,p);
        }
        for(int i=0;i<15;i++) {
            float x=i*145;
            for(int j=0;j<7;j++) line(c,x+(float)Math.sin(j+i)*19,1080-j*26,x+(float)Math.sin(j+1+i)*19,1054-j*26,0xff126b68,12);
        }
        text(c,"THE DEEP SEA / GAME LAUNCHER",112,110,26,0xffa9e5dd);
        text(c,title,110,188,66,0xffffe9a6);
        text(c,"A little adventure beyond the surface",114,241,27,0xffb0d8db);
        // Rope surrounds the timber board; short diagonal strands give it a twisted texture.
        fill(0xffd5ac68); c.drawRoundRect(new RectF(98,286,1118,966),35,35,p);
        for(int y=305;y<950;y+=19) {
            line(c,99,y,115,y+12,0xff866137,3); line(c,1102,y,1117,y+12,0xff866137,3);
        }
        for(int i=0;i<8;i++) {
            fill(i%2==0?0xff684330:0xff734c34); c.drawRect(120,306+i*80,1096,386+i*80,p);
            line(c,120,385+i*80,1096,385+i*80,0xff382e28,3);
            line(c,140,323+i*80,1070,330+i*80,0xff8c6140,2);
        }
        for(int i=0;i<rows.length;i++) {
            float y=326+i*96;
            fill(i==selected?0xffffd47d:0xffb59255); c.drawRoundRect(new RectF(151,y,1066,y+80),40,40,p);
            fill(i==selected?0xff106d7b:0xff143d4b); c.drawRoundRect(new RectF(159,y+7,1058,y+73),33,33,p);
            circle(c,187,y+40,8,0xffebc77c); circle(c,1030,y+40,8,0xffebc77c);
            float size=34; p.setTextSize(size);
            while(p.measureText(rows[i])>775 && size>22) { size--; p.setTextSize(size); }
            text(c,rows[i],220,y+52,size,i==selected?0xffffe6a5:0xffd9e5df);
        }
        // Brass wheel, with eight spokes and wooden handles.
        float cx=1500,cy=465;
        for(int i=0;i<8;i++) {
            double a=i*Math.PI/4; float dx=(float)Math.cos(a),dy=(float)Math.sin(a);
            line(c,cx+dx*40,cy+dy*40,cx+dx*195,cy+dy*195,0xffbe8b48,17);
            circle(c,cx+dx*195,cy+dy*195,15,0xffd8aa61);
        }
        fill(0xffd3a557); p.setStyle(Paint.Style.STROKE); p.setStrokeWidth(23); c.drawCircle(cx,cy,140,p);
        circle(c,cx,cy,49,0xff9b703d); circle(c,cx,cy,28,0xffffdf8e);
        text(c,detail,1200,745,32,0xffffe6a5);
        String[] lines=status.split("\n");
        for(int i=0;i<lines.length;i++) text(c,lines[i],1200,803+i*37,25,0xffb9deda);
        text(c,busy?"INSTALLING / Please keep this screen open":"D-PAD / Move     A / Select     B / Back",115,1026,27,0xffc9dfd7);
        c.restore(); postInvalidateDelayed(40);
    }
    private void move(int delta) { if(rows.length>0 && !busy) {selected=(selected+delta+rows.length)%rows.length;invalidate();} }
    @Override public boolean onKeyDown(int key,KeyEvent event) {
        if(key==KeyEvent.KEYCODE_BACK || key==KeyEvent.KEYCODE_BUTTON_B) { if(!busy)actions.back(); return true; }
        if(key==KeyEvent.KEYCODE_DPAD_DOWN || key==KeyEvent.KEYCODE_DPAD_RIGHT) {move(1);return true;}
        if(key==KeyEvent.KEYCODE_DPAD_UP || key==KeyEvent.KEYCODE_DPAD_LEFT) {move(-1);return true;}
        if(key==KeyEvent.KEYCODE_BUTTON_A || key==KeyEvent.KEYCODE_DPAD_CENTER || key==KeyEvent.KEYCODE_ENTER) {
            if(event.getRepeatCount()==0 && !busy && rows.length>0)actions.select(selected); return true;
        }
        return super.onKeyDown(key,event);
    }
    @Override public boolean onGenericMotionEvent(MotionEvent e) {
        float axis=e.getAxisValue(MotionEvent.AXIS_Y);
        if(Math.abs(axis)<.3f)stickHeld=false;
        else if(Math.abs(axis)>.6f && !stickHeld) {move(axis>0?1:-1);stickHeld=true;}
        return true;
    }
    private int hit(MotionEvent e) {
        float s=scale(); if(s<=0)return -1;
        float x=(e.getX()-(getWidth()-1920*s)/2)/s,y=(e.getY()-(getHeight()-1080*s)/2)/s;
        int row=(int)((y-326)/96);
        return x>=151 && x<=1066 && y>=326 && row<rows.length && y<=326+row*96+80?row:-1;
    }
    @Override public boolean onTouchEvent(MotionEvent e) {
        if(busy)return true;
        if(e.getActionMasked()==MotionEvent.ACTION_DOWN) {pressed=hit(e);if(pressed>=0)selected=pressed;invalidate();return true;}
        if(e.getActionMasked()==MotionEvent.ACTION_UP) {int row=hit(e);if(row>=0 && row==pressed){performClick();actions.select(row);}pressed=-1;return true;}
        if(e.getActionMasked()==MotionEvent.ACTION_CANCEL)pressed=-1;
        return true;
    }
    @Override public boolean performClick() {super.performClick();return true;}
}
