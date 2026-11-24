package com.ubear.game;

import android.content.Context;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.Arrays;
import java.util.Locale;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

// Envoi des statistiques : chaque lot est écrit dans files/telemetry/ puis posté en tâche de fond.
// Un lot n'est supprimé qu'une fois accepté par le serveur ; sans réseau, il repart au prochain envoi.
public final class Telemetry {
    private static final ExecutorService EXEC = Executors.newSingleThreadExecutor();
    private static final int MAX_FILES = 200;   // hors ligne longtemps : on garde les 200 lots les plus récents
    private static int seq = 0;

    public static void send(Context ctx, final String url, byte[] body) {
        final File dir = new File(ctx.getApplicationContext().getFilesDir(), "telemetry");
        if (!dir.isDirectory() && !dir.mkdirs()) return;
        String name = String.format(Locale.ROOT, "%013d_%04d.json", System.currentTimeMillis(), seq++ % 10000);
        try (FileOutputStream out = new FileOutputStream(new File(dir, name))) {
            out.write(body);
        } catch (IOException e) {
            return;
        }
        EXEC.execute(new Runnable() {
            @Override public void run() { flush(dir, url); }
        });
    }

    private static void flush(File dir, String url) {
        File[] files = dir.listFiles();
        if (files == null) return;
        Arrays.sort(files);
        int drop = files.length - MAX_FILES;
        for (File f : files) {
            if (drop-- > 0) { f.delete(); continue; }
            byte[] body = read(f);
            int code = body == null ? 400 : post(url, body);
            if (code >= 200 && code < 500) f.delete();   // accepté, ou refusé définitivement (4xx)
            else return;                                  // pas de réseau / serveur en panne : on réessaiera
        }
    }

    private static byte[] read(File f) {
        try (FileInputStream in = new FileInputStream(f)) {
            byte[] b = new byte[(int) f.length()];
            int off = 0;
            while (off < b.length) {
                int n = in.read(b, off, b.length - off);
                if (n < 0) return null;
                off += n;
            }
            return b;
        } catch (IOException e) {
            return null;
        }
    }

    private static int post(String url, byte[] body) {
        HttpURLConnection c = null;
        try {
            c = (HttpURLConnection) new URL(url).openConnection();
            c.setConnectTimeout(10000);
            c.setReadTimeout(10000);
            c.setRequestMethod("POST");
            c.setDoOutput(true);
            c.setRequestProperty("Content-Type", "application/json");
            c.setFixedLengthStreamingMode(body.length);
            try (OutputStream o = c.getOutputStream()) { o.write(body); }
            return c.getResponseCode();
        } catch (IOException e) {
            return -1;
        } finally {
            if (c != null) c.disconnect();
        }
    }
}
