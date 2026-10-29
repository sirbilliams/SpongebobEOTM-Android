package com.sirbilliams.spongebob;

import android.content.Context;
import android.media.AudioAttributes;
import android.media.SoundPool;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;

/**
 * Launcher cues, synthesised in code (no recorded sounds): a soft bubble blip when the focus moves, a wood creak with a
 * bubble on select, a little bubble cluster when the water is tapped. Written once as tiny WAVs to the cache, played by SoundPool.
 */
final class LauncherSound implements LauncherView.Sounds {
    private static final int RATE = 22050;
    private final SoundPool pool;
    private final int blip, creak, cluster;

    LauncherSound(Context context) {
        pool = new SoundPool.Builder().setMaxStreams(4).setAudioAttributes(new AudioAttributes.Builder()
            .setUsage(AudioAttributes.USAGE_GAME).setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION).build()).build();
        File dir = context.getCacheDir();
        blip = load(new File(dir, "launcher-blip.wav"), bubble(0.09f, 520, 1500, 0));
        creak = load(new File(dir, "launcher-creak.wav"), creak());
        cluster = load(new File(dir, "launcher-bubbles.wav"), cluster());
    }

    public void move() { play(blip, .32f, 1f); }
    public void select() { play(creak, .34f, 1f); play(blip, .3f, .8f); }
    public void tap() { play(cluster, .3f, 1f); }
    void release() { pool.release(); }

    private void play(int id, float vol, float rate) { if (id > 0) pool.play(id, vol, vol, 1, 0, rate); }

    private int load(File f, short[] pcm) {
        try (FileOutputStream out = new FileOutputStream(f)) {
            out.write(wav(pcm));
            return pool.load(f.getPath(), 1);
        } catch (IOException | RuntimeException e) {
            return 0;
        }
    }

    /** A rising sine chirp with a fast attack and exponential decay: an air bubble leaving the water. */
    private static short[] bubble(float seconds, float f0, float f1, int pad) {
        int n = (int) (seconds * RATE) + pad;
        short[] s = new short[n];
        double phase = 0;
        for (int i = pad; i < n; i++) {
            float x = (i - pad) / (float) (n - pad);
            phase += 2 * Math.PI * (f0 + (f1 - f0) * x * x) / RATE;
            double env = Math.min(1, x * 40) * Math.exp(-x * 5);
            s[i] = (short) (Math.sin(phase) * env * 12000);
        }
        return s;
    }

    private static short[] cluster() {
        short[] out = new short[(int) (.32f * RATE)];
        int[] at = {0, 1900, 3600, 5200};
        float[][] f = {{600, 1700}, {800, 2100}, {500, 1400}, {900, 2300}};
        for (int k = 0; k < at.length; k++) {
            short[] b = bubble(.07f, f[k][0], f[k][1], 0);
            for (int i = 0; i < b.length && at[k] + i < out.length; i++) out[at[k] + i] = (short) Math.max(-32000, Math.min(32000, out[at[k] + i] + b[i] * .7f));
        }
        return out;
    }

    /** Stick-slip: a train of short resonant clicks at a wandering rate, like a rope rubbing a wet pier post. */
    private static short[] creak() {
        int n = (int) (.26f * RATE);
        short[] s = new short[n];
        double next = 0, ring = 0, ringV = 0;
        long seed = 12345;
        for (int i = 0; i < n; i++) {
            float x = i / (float) n;
            if (i >= next) {
                ringV += 0.9;
                double rate = 55 + 35 * Math.sin(x * 9) + 20 * x;
                seed = seed * 6364136223846793005L + 1442695040888963407L;
                next = i + RATE / rate * (0.85 + ((seed >>> 40) & 255) / 255.0 * 0.3);
            }
            // a damped 420 Hz resonator excited by each click
            double w = 2 * Math.PI * 420 / RATE;
            ringV -= ring * w * w; ringV *= 0.985; ring += ringV;
            double env = Math.min(1, x * 12) * (1 - x);
            s[i] = (short) Math.max(-32000, Math.min(32000, ring * env * 1200));
        }
        return s;
    }

    private static byte[] wav(short[] pcm) {
        int data = pcm.length * 2;
        byte[] b = new byte[44 + data];
        put(b, 0, "RIFF"); le(b, 4, 36 + data, 4); put(b, 8, "WAVEfmt "); le(b, 16, 16, 4); le(b, 20, 1, 2); le(b, 22, 1, 2);
        le(b, 24, RATE, 4); le(b, 28, RATE * 2, 4); le(b, 32, 2, 2); le(b, 34, 16, 2); put(b, 36, "data"); le(b, 40, data, 4);
        for (int i = 0; i < pcm.length; i++) le(b, 44 + i * 2, pcm[i], 2);
        return b;
    }
    private static void put(byte[] b, int o, String s) { for (int i = 0; i < s.length(); i++) b[o + i] = (byte) s.charAt(i); }
    private static void le(byte[] b, int o, int v, int n) { for (int i = 0; i < n; i++) b[o + i] = (byte) (v >> (8 * i)); }
}
