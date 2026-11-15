package com.ubear.game;

import android.content.Context;
import android.media.AudioAttributes;
import android.media.AudioFocusRequest;
import android.media.AudioManager;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;

// La musique du jeu cède la place à Spotify, Deezer, un appel, etc.
public final class AudioFocus implements AudioManager.OnAudioFocusChangeListener {
    private static volatile boolean allow = true;
    private static AudioManager am;
    private static AudioFocus listener;
    private static AudioFocusRequest request;

    public static void init(Context ctx) {
        am = (AudioManager) ctx.getSystemService(Context.AUDIO_SERVICE);
        listener = new AudioFocus();
        if (am != null && am.isMusicActive()) {
            allow = false;
            return;
        }
        requestGain();
    }

    public static boolean allowsMusic() {
        if (allow) return true;
        if (am != null && !am.isMusicActive()) requestGain();
        return allow;
    }

    private static synchronized void requestGain() {
        if (am == null || listener == null) return;
        int result;
        if (Build.VERSION.SDK_INT >= 26) {
            AudioAttributes attrs = new AudioAttributes.Builder()
                    .setUsage(AudioAttributes.USAGE_GAME)
                    .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
                    .build();
            request = new AudioFocusRequest.Builder(AudioManager.AUDIOFOCUS_GAIN)
                    .setAudioAttributes(attrs)
                    .setOnAudioFocusChangeListener(listener, new Handler(Looper.getMainLooper()))
                    .setAcceptsDelayedFocusGain(true)
                    .build();
            result = am.requestAudioFocus(request);
        } else {
            result = am.requestAudioFocus(listener, AudioManager.STREAM_MUSIC, AudioManager.AUDIOFOCUS_GAIN);
        }
        allow = result == AudioManager.AUDIOFOCUS_REQUEST_GRANTED;
    }

    @Override
    public void onAudioFocusChange(int focusChange) {
        if (focusChange == AudioManager.AUDIOFOCUS_GAIN) {
            allow = true;
            return;
        }
        allow = false;
        if (am == null) return;
        if (Build.VERSION.SDK_INT >= 26 && request != null) am.abandonAudioFocusRequest(request);
        else am.abandonAudioFocus(this);
    }
}
