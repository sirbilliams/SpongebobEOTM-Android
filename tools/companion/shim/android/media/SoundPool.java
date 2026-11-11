package android.media;
/** Desktop shim (tools/companion): compile-only, plays nothing. */
public class SoundPool {
    public interface OnLoadCompleteListener { void onLoadComplete(SoundPool sp, int id, int status); }
    public SoundPool(int streams, int type, int q) {}
    public void setOnLoadCompleteListener(OnLoadCompleteListener l) {}
    public int load(String path, int prio) { return 1; }
    public int play(int id, float l, float r, int prio, int loop, float rate) { return 1; }
    public void release() {}
}
