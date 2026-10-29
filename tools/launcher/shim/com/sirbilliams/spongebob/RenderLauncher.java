package com.sirbilliams.spongebob;
import android.content.Context;
import android.graphics.*;
import java.io.File;
import javax.imageio.ImageIO;
public final class RenderLauncher {
 public static void main(String[] args)throws Exception {
  LauncherView v=new LauncherView(new Context(),new LauncherView.Actions(){public void select(int row){}public void back(){}});
  v.setTestSize(1920,1080);
  String page=args[0];
  switch(page) {
   case "main":v.page("Welcome aboard","Ready to set sail","Game files installed.\nChoose Play to begin.","Play","Import game","Settings","Test","About","Quit");break;
   case "play":v.page("Set sail","Welcome back, captain","A save was found in saved/.\nResume from the game's menu.","Start game","Back");break;
   case "import":v.page("Import game","Bring your own CD image","Choose a game CD image (.iso).\nAbout 1 GB of space is needed.\nReinstall replaces game files.","Choose CD image (.iso)","Back");break;
   case "settings":v.page("Captain's settings","Make yourself at home","Choices apply on next launch.\nTouch always controls the cursor.","Widescreen: Side panels","Bottom-screen panel: On","Right-stick cursor: On","Back");break;
   case "test":v.page("Engine room","Tools & diagnostics","Logs include the current\nand previous run.","Share log","App version: 1.0 (abcdef0)","Reset settings","Crash test","Reinstall from CD image","Back");break;
   case "about":v.page("About this voyage","An independent engine port","Original launcher shapes.\nBring your own game files.","Version: 1.0 (abcdef0)","Open-source licences","Back");break;
   default:throw new IllegalArgumentException(page);
  }
  Bitmap b=Bitmap.createBitmap(1920,1080,Bitmap.Config.ARGB_8888);v.drawForTest(new Canvas(b));ImageIO.write(b.img,"png",new File(args[1]));
 }
}
