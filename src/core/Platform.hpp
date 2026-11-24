// Services spécifiques à la plateforme (vibration, cycle de vie Android).
#pragma once

namespace platform {
void vibrate(int milliseconds);
void shutdown();

// Notifications locales (programmées sur l'appareil, sans serveur).
void requestNotifications();      // autorisation : Android 13+ et iOS
void scheduleNotification(int id, long long delaySec, const char* title, const char* text); // id 1..31
void cancelNotifications();       // annule celles prévues et efface celles affichées

// Statistiques : le lot est écrit sur disque puis envoyé (POST JSON) en tâche de fond,
// renvoyé plus tard si le réseau manque. PC : affiché dans la console.
void sendAnalytics(const char* url, const char* body);

// Faux si une autre appli joue de la musique (Spotify, Deezer, appel…).
void initAudioFocus();
bool allowGameMusic();

// Appelé par raylib (rcore_android.c) quand l'appli perd / regagne le focus.
// Le jeu y branche la mise en pause de la musique et du service.
using FocusCallback = void (*)(bool focused);
void setFocusCallback(FocusCallback cb);
}
