package com.sirbilliams.spongebob;
import android.content.Context;
import android.graphics.*;
import android.os.SystemClock;
import java.io.File;
import javax.imageio.ImageIO;
/**
 * Renders the real LauncherView frame by frame on a stepped clock (60 fps): every page after it settles, three idle frames,
 * a frame half way through a dive, the water-tap reaction and the bottom-screen scene. Pictures are made up here (no game art).
 */
public final class RenderLauncher {
 static long clock;
 static void frames(LauncherView v, int n, Canvas c) { for (int i = 0; i < n; i++) { clock += 16; SystemClock.testNow = clock; v.drawForTest(c); } }
 static void shot(LauncherView v, String out, int w) throws Exception {
  Bitmap b = Bitmap.createBitmap(w, 1080, Bitmap.Config.ARGB_8888); clock += 16; SystemClock.testNow = clock; v.drawForTest(new Canvas(b));
  ImageIO.write(b.img, "png", new File(out)); System.out.println(out);
 }
 /** A stand-in save thumbnail: a soft test card (no game picture). */
 static Bitmap fakeThumb() {
  int[] px = new int[320 * 240];
  for (int y = 0; y < 240; y++) for (int x = 0; x < 320; x++) {
   int band = x * 6 / 320; int[] cols = {0xff4f9bd6, 0xff6cc48a, 0xffe3c25a, 0xffe08a4e, 0xffc65d7b, 0xff7d6cc4};
   int c = y < 170 ? cols[band] : ((x / 20 + y / 20) % 2 == 0 ? 0xff2a3a4a : 0xff44566a);
   px[y * 320 + x] = c;
  }
  return Bitmap.createBitmap(px, 320, 240, Bitmap.Config.ARGB_8888);
 }
 /** A stand-in for the player's own picture: a sunset gradient with a sun. */
 static Bitmap fakeArt() {
  int w = 800, h = 500; int[] px = new int[w * h];
  for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
   float f = y / (float) h; int r = (int) (250 - f * 120), g = (int) (150 - f * 90), b = (int) (90 + f * 100);
   float dx = x - 520, dy = y - 300; if (dx * dx + dy * dy < 90 * 90) { r = 255; g = 220; b = 120; }
   if (y > 330) { r = r / 3; g = g / 2 + 30; b = b / 2 + 70; }
   px[y * w + x] = 0xff000000 | r << 16 | g << 8 | b;
  }
  return Bitmap.createBitmap(px, w, h, Bitmap.Config.ARGB_8888);
 }
 static void page(LauncherView v, String page) {
  switch (page) {
   case "main": v.page(0, "Welcome aboard", "Ready to set sail", "Game files installed.\nChoose Play to begin.", "Play", "Import game", "Saves", "Settings", "Engine room", "About", "Quit"); break;
   case "play": v.page(1, "Set sail", "Welcome back, captain", "Choose Resume in the game's menu to carry on.", "Start game", "Back"); v.withCard(); break;
   case "import": v.page(1, "Import game", "Bring your own CD image", "Choose a game CD image (.iso).\nAbout 1 GB of space is needed.\nReinstall replaces game files.", "Choose CD image (.iso)", "Back"); break;
   case "saves": v.page(2, "Ship's locker", "4 saves on board", "Export makes one .zip where you choose.\nImport shows every save before anything is replaced.", "Export all saves (.zip)", "Export one save", "Import saves (.zip)", "Back"); break;
   case "settings": v.page(2, "Captain's settings", "Make yourself at home", "Choices apply on next launch.\nTouchpad: bottom screen moves the cursor.\nPanel window: only if bottom touch fails.", "Widescreen: Side panels", "Bottom-screen panel: On", "Bottom screen: Touchpad", "Panel window: Safe (default)", "Right-stick cursor: On", "Cursor: Spatula", "Box art: Your picture", "Back"); break;
   case "test": v.page(3, "Engine room", "Tools & diagnostics", "Logs include the current\nand previous run.", "Share log", "Touch test", "App version: 1.0 (abcdef0)", "Reset settings", "Crash test", "Reinstall from CD image", "Tools", "Back"); break;
  case "tools": v.page(4, "Tools", "Assets & HD pack", "Browse the game's models and scenes.\nHD pack installed: 38 scenes, 41.3 MB.\nHD changes apply on next launch.", "Model viewer", "Scene explorer", "HD backgrounds: On", "HD pack: Install (.zip)", "HD pack: Remove", "Back"); break;
   case "about": v.page(1, "About this voyage", "An independent engine port", "Original launcher shapes.\nBring your own game files.", "Version: 1.0 (abcdef0)", "Open-source licences", "Back"); break;
   default: throw new IllegalArgumentException(page);
  }
 }
 public static void main(String[] a) throws Exception {
  String out = a[0]; new File(out).mkdirs();
  LauncherView.Actions none = new LauncherView.Actions() { public void select(int row) {} public void back() {} };
  Bitmap scratch = Bitmap.createBitmap(1920, 1080, Bitmap.Config.ARGB_8888); Canvas sc = new Canvas(scratch);
  Bitmap thumb = fakeThumb(), art = fakeArt();
  for (String pg : new String[] {"main", "play", "import", "saves", "settings", "test", "tools", "about"}) {
   clock = 40000; SystemClock.testNow = clock;
   LauncherView v = new LauncherView(new Context(), none); v.setTestSize(1920, 1080);
   v.setCard(thumb, "Chapter 2", "WSEA TV - News Room", "Saved 27 Oct 2026, 21:14", 2); v.setArt(art);
   page(v, pg);
   if (pg.equals("settings")) v.focusRow(4);
   frames(v, 90, sc);
   shot(v, out + "/page-" + pg + ".png", 1920);
  }
  // idle animation: three frames of the main page, 2.5 s apart
  clock = 40000; SystemClock.testNow = clock;
  LauncherView v = new LauncherView(new Context(), none); v.setTestSize(1920, 1080);
  page(v, "main"); frames(v, 60, sc);
  for (int i = 1; i <= 3; i++) { frames(v, 150, sc); shot(v, out + "/idle-" + i + ".png", 1920); }
  // dive: main -> settings, captured 45% of the way
  page(v, "settings"); frames(v, (int) (LauncherView.DIVE_MS * .45f / 16), sc); shot(v, out + "/dive-mid.png", 1920);
  frames(v, 80, sc);
  // rise back to the surface, 55% of the way
  page(v, "main"); frames(v, (int) (LauncherView.DIVE_MS * .55f / 16), sc); shot(v, out + "/rise-mid.png", 1920);
  frames(v, 80, sc);
  // tap the water near a jellyfish: bubbles, the jellyfish dims and scoots
  v.onTouchEvent(new android.view.MotionEvent(0, 1050, 420, 0)); v.onTouchEvent(new android.view.MotionEvent(1, 1050, 420, 0));
  frames(v, 18, sc); shot(v, out + "/tap.png", 1920);
  // import in progress
  page(v, "import"); frames(v, 80, sc); v.status("Unpacking the game files...\n57%", true); shot(v, out + "/importing.png", 1920);
  // reduced motion (animator scale 0): still scene, instant page
  LauncherView rm = new LauncherView(new Context(), none); rm.setTestSize(1920, 1080); rm.setReducedMotion(true); page(rm, "main"); page(rm, "about");
  shot(rm, out + "/reduced-motion.png", 1920);
  // bottom screen (1240 x 1080)
  clock = 40000;
  LauncherView b = new LauncherView(new Context(), none, true); b.setTestSize(1240, 1080);
  b.setCard(thumb, "Chapter 2", "WSEA TV - News Room", "Saved 27 Oct 2026, 21:14", 2); b.setArt(art);
  b.setDials(new String[] {"Widescreen", "Right stick", "Cursor", "Bottom screen"}, new String[] {"Side panels", "On", "Spatula", "Touchpad"},
   new int[] {1, 1, 0, 0}, new int[] {4, 2, 4, 2});
  Bitmap bs = Bitmap.createBitmap(1240, 1080, Bitmap.Config.ARGB_8888); Canvas bc = new Canvas(bs);
  frames(b, 90, bc); shot(b, out + "/bottom.png", 1240);
  LauncherView b2 = new LauncherView(new Context(), none, true); b2.setTestSize(1240, 1080);
  b2.setCard(null, null, null, null, 0);
  b2.setDials(new String[] {"Widescreen", "Right stick", "Cursor", "Bottom screen"}, new String[] {"Fill", "Off", "Classic", "Panel"},
   new int[] {0, 0, 3, 1}, new int[] {4, 2, 4, 2});
  frames(b2, 90, bc); shot(b2, out + "/bottom-nosave.png", 1240);
 }
}
