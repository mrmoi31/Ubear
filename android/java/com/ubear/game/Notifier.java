package com.ubear.game;

import android.app.Activity;
import android.app.AlarmManager;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Build;

// Notifications locales : énergie pleine + rappel quotidien.
// Programmées par le jeu (JNI) quand il passe en arrière-plan, annulées à son retour.
public final class Notifier extends BroadcastReceiver {
    private static final String CHANNEL = "ubear_rappels";
    private static final String PERMISSION = "android.permission.POST_NOTIFICATIONS";
    private static final int MAX_ID = 32;   // les ids du jeu restent sous cette borne

    public static void requestPermission(final Activity a) {
        if (Build.VERSION.SDK_INT < 33 || a.checkSelfPermission(PERMISSION) == PackageManager.PERMISSION_GRANTED) return;
        // Appelé depuis le thread natif : la demande doit partir du thread UI.
        a.runOnUiThread(new Runnable() {
            @Override public void run() { a.requestPermissions(new String[] { PERMISSION }, 4242); }
        });
    }

    public static void schedule(Context ctx, int id, long delaySec, String title, String text) {
        AlarmManager am = (AlarmManager) ctx.getSystemService(Context.ALARM_SERVICE);
        if (am == null) return;
        Intent i = new Intent(ctx, Notifier.class).putExtra("id", id).putExtra("title", title).putExtra("text", text);
        PendingIntent pi = PendingIntent.getBroadcast(ctx, id, i, PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);
        // Alarme non exacte : pas de permission SCHEDULE_EXACT_ALARM, quelques minutes de décalage possibles.
        am.set(AlarmManager.RTC, System.currentTimeMillis() + Math.max(1L, delaySec) * 1000L, pi);
    }

    public static void cancelAll(Context ctx) {
        AlarmManager am = (AlarmManager) ctx.getSystemService(Context.ALARM_SERVICE);
        for (int id = 1; id < MAX_ID && am != null; id++) {
            Intent i = new Intent(ctx, Notifier.class);
            PendingIntent pi = PendingIntent.getBroadcast(ctx, id, i, PendingIntent.FLAG_NO_CREATE | PendingIntent.FLAG_IMMUTABLE);
            if (pi != null) { am.cancel(pi); pi.cancel(); }
        }
        NotificationManager nm = (NotificationManager) ctx.getSystemService(Context.NOTIFICATION_SERVICE);
        if (nm != null) nm.cancelAll();
    }

    @Override
    public void onReceive(Context ctx, Intent intent) {
        if (Build.VERSION.SDK_INT >= 33 && ctx.checkSelfPermission(PERMISSION) != PackageManager.PERMISSION_GRANTED) return;
        NotificationManager nm = (NotificationManager) ctx.getSystemService(Context.NOTIFICATION_SERVICE);
        if (nm == null) return;

        Notification.Builder b;
        if (Build.VERSION.SDK_INT >= 26) {
            nm.createNotificationChannel(new NotificationChannel(CHANNEL, "Rappels", NotificationManager.IMPORTANCE_DEFAULT));
            b = new Notification.Builder(ctx, CHANNEL);
        } else {
            b = new Notification.Builder(ctx);
        }
        Intent open = ctx.getPackageManager().getLaunchIntentForPackage(ctx.getPackageName());
        PendingIntent tap = open == null ? null
                : PendingIntent.getActivity(ctx, 0, open, PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);
        b.setSmallIcon(ctx.getApplicationInfo().icon)
         .setContentTitle(intent.getStringExtra("title"))
         .setContentText(intent.getStringExtra("text"))
         .setStyle(new Notification.BigTextStyle().bigText(intent.getStringExtra("text")))
         .setAutoCancel(true);
        if (tap != null) b.setContentIntent(tap);
        nm.notify(intent.getIntExtra("id", 1), b.build());
    }
}
