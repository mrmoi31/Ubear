// Constantes de gameplay, d'économie et de rendu.
// Tout ce qui se règle "à la main" est ici.
#pragma once

namespace cfg {

// ---- Monde ----------------------------------------------------------------
constexpr int   MAP_IMAGE_SIZE = 800;                     // image cellulaire générée
constexpr float MAP_SCALE      = 10.0f;                   // 1 pixel image = 10 unités monde
constexpr float WORLD_SIZE     = MAP_IMAGE_SIZE * MAP_SCALE; // 8000 x 8000
constexpr int   CELL_SIZE      = 40;                      // taille des cellules de banquise
constexpr int   RIVER_POWER    = 3;                       // finesse des rivières
constexpr float ISLAND_INNER   = 0.20f;                   // rayon (normalisé) de l'île
constexpr float ISLAND_OUTER   = 0.25f;
constexpr float ICE_THRESHOLD  = 0.85f;                   // iciness > seuil = glace

// ---- Caméra ---------------------------------------------------------------
constexpr float VIEW_WIDTH_WORLD = 900.0f;                // largeur visible en unités monde
constexpr float CAMERA_SMOOTH    = 8.0f;
constexpr float CAMERA_LOOKAHEAD = 0.25f;                 // secondes de vitesse anticipées

// ---- Ours -----------------------------------------------------------------
constexpr float BEAR_SCALE       = 0.20f;                 // 676 px * 0.2 = ~135 unités monde
constexpr float PUSH_IMPULSE     = 1100.0f;               // unités/s ajoutées par poussée
constexpr float DAMPING_ICE      = 7.4f;                  // friction (par seconde)
constexpr float DAMPING_WATER    = 16.0f;
constexpr float WATER_PUSH_MULT  = 0.45f;
constexpr float TURN_SMOOTH      = 14.0f;                 // vitesse de rotation visuelle
constexpr float MAX_SPEED        = 2200.0f;

// Niveaux d'équipement : index 0 = non acheté, 1..3 = niveaux
constexpr float SKATE_POWER[4]      = {1.00f, 1.15f, 1.32f, 1.50f};
constexpr float BRAKE_DAMPING[4]    = {0.0f, 10.0f, 18.0f, 30.0f};
constexpr float BAG_TIME_MULT[4]    = {1.00f, 1.25f, 1.50f, 1.90f};
constexpr float ANCHOR_WEATHER[4]   = {1.00f, 0.70f, 0.40f, 0.15f};
// GPS : 0 flèche, 1 +distance, 2 +mini-carte, 3 +cible visible dans le brouillard

// ---- Météo ----------------------------------------------------------------
constexpr int   WEATHER_MASK_RES   = 256;
constexpr float WIND_ACCEL         = 1500.0f;             // unités/s²
constexpr float HAIL_DAMPING_ADD   = 6.0f;
constexpr float HAIL_PUSH_MULT     = 0.75f;
constexpr float FOG_FADE_SPEED     = 1.2f;                // /s

// ---- Livraisons -----------------------------------------------------------
constexpr float DELIVERY_RADIUS    = 200.0f;
constexpr float PICKUP_MIN_DIST    = 700.0f;
constexpr float PICKUP_MAX_DIST    = 1800.0f;
constexpr float DROP_MIN_DIST      = 900.0f;
constexpr float DROP_MAX_DIST      = 2200.0f;
constexpr float ORDER_BASE_TIME    = 10.0f;               // secondes
constexpr float ORDER_TIME_PER_PX  = 1.0f / 150.0f;
constexpr float ORDER_COOLDOWN     = 1.6f;                // pause entre deux commandes

// ---- Économie (progression lente) ----------------------------------------
constexpr int   REWARD_BASE        = 5;
constexpr float REWARD_PER_PX      = 1.0f / 200.0f;
constexpr float REWARD_SPEED_BONUS = 0.5f;                // +50% max si livré très vite
constexpr float STREAK_BONUS       = 0.10f;               // +10% par livraison consécutive
constexpr float STREAK_MAX_BONUS   = 0.50f;

// ---- Cycle jour / nuit ----------------------------------------------------
constexpr float DAY_START_HOUR   = 17.0f;                 // la journée commence à 17 h
constexpr float DAY_END_HOUR     = 22.5f;                 // fermeture : 22 h 30
constexpr float SECONDS_PER_HOUR = 48.0f;                 // repli si l'horloge n'est pas calée sur la journée
constexpr float NIGHT_START      = 21.0f;
constexpr float NIGHT_END        = 5.0f;
constexpr float TWILIGHT_HOURS   = 1.5f;                  // fondu crépuscule / aube

// ---- Journées (style fin de journée) -------------------------------------
// Jour 1 : 4 commandes. Ensuite plus de commandes, moins de marge,
// plats qui refroidissent plus vite, clients plus loin, météo plus rude.
inline int dayIndex(int saved) { return saved < 1 ? 1 : saved; }

// Croissance douce jusqu'au jour 4, puis exponentielle (plafonnée).
inline float powGrow(float base, int steps)
{
    if (steps < 0) steps = 0;
    if (steps > 24) steps = 24;
    float r = 1.0f;
    for (int i = 0; i < steps; i++) r *= base;
    return r;
}

inline int dayQuota(int saved)
{
    int d = dayIndex(saved);
    if (d < 5) return 3 + d;                 // 4, 5, 6, 7
    int extra = d - 5;
    int q = 8 + extra + extra / 2;           // 8, 9, 11, 12, 14...
    return q > 12 ? 12 : q;
}

inline float dayHeat(int saved)
{
    int d = dayIndex(saved);
    if (d < 5) return 1.0f + (d - 1) * 0.07f;
    float h = 1.45f * powGrow(1.16f, d - 5);
    return h > 3.4f ? 3.4f : h;
}

inline float daySpread(int saved)
{
    int d = dayIndex(saved);
    if (d < 5) return 1.0f + (d - 1) * 0.05f;
    float s = 1.32f * powGrow(1.10f, d - 5);
    return s > 2.3f ? 2.3f : s;
}

inline float dayPay(int saved)
{
    int d = dayIndex(saved);
    if (d < 5) return 1.0f + (d - 1) * 0.04f;
    float p = 1.18f * powGrow(1.06f, d - 5); // moins vite que la difficulté
    return p > 2.0f ? 2.0f : p;
}

inline float dayWeather(int saved)
{
    int d = dayIndex(saved);
    if (d < 5) return 1.0f + (d - 1) * 0.08f;
    float w = 1.40f * powGrow(1.12f, d - 5);
    return w > 2.8f ? 2.8f : w;
}

inline float dayLength(int saved)
{
    int d = dayIndex(saved);
    int q = dayQuota(saved);
    if (d < 5) return q * 70.0f + 30.0f;
    float squeeze = powGrow(0.90f, d - 5);
    if (squeeze < 0.55f) squeeze = 0.55f;
    return q * 54.0f * squeeze + 20.0f;
}

// ---- Énergie (temps réel) -------------------------------------------------
constexpr int ENERGY_MAX       = 5;
constexpr int ENERGY_REGEN_SEC = 3 * 3600;                // 1 point toutes les 3 heures
constexpr int ENERGY_AD_GRANT  = 5;                       // une pub rend 5 journées
constexpr int ENERGY_AD_MAX    = 3;                       // plafond par jour réel
constexpr int GOLD_AD_GRANT    = 90;
constexpr int GOLD_AD_MAX      = 4;

// ---- Événements météo extrêmes --------------------------------------------
constexpr float BLIZZARD_FIRST_MIN   = 70.0f;             // secondes avant le 1er blizzard
constexpr float BLIZZARD_FIRST_MAX   = 110.0f;
constexpr float BLIZZARD_GAP_MIN     = 110.0f;
constexpr float BLIZZARD_GAP_MAX     = 170.0f;
constexpr float BLIZZARD_DURATION    = 24.0f;
constexpr float BLIZZARD_WARNING     = 4.0f;
constexpr float BLIZZARD_WIND_MULT   = 1.35f;             // vent global = WIND_ACCEL * mult
constexpr float BLIZZARD_REWARD_MULT = 1.5f;              // prime de risque
constexpr float BLIZZARD_COOL_MULT   = 1.3f;              // le plat refroidit plus vite
constexpr float AURORA_CHANCE        = 0.8f;              // probabilité par nuit
constexpr float AURORA_DURATION      = 45.0f;
constexpr float AURORA_REWARD_MULT   = 2.0f;              // pourboires doublés

// ---- Clients récurrents ---------------------------------------------------
constexpr float REGULAR_CHANCE = 0.65f;                   // part des commandes passées par un habitué
constexpr int   YETI_PATIENCE  = 50;                      // livraisons avant le premier pourboire du yéti
constexpr int   YETI_BIG_TIP   = 2500;

// ---- Défis quotidiens -----------------------------------------------------
constexpr int   CHALLENGE_ALL_BONUS = 100;                // bonus quand les 3 défis du jour sont faits

// ---- UI -------------------------------------------------------------------
constexpr int   DESIGN_W = 720;
constexpr int   DESIGN_H = 1280;
constexpr float SAFE_TOP = 56.0f;                         // marge pour encoche / caméra frontale

constexpr const char* SAVE_FILE = "ubear_save.txt";
constexpr const char* VERSION   = "1.0.0";

// ---- Statistiques joueurs (PostHog, hébergement UE) -------------------------
// Clé "Project API key" (phc_...) de PostHog > Project settings. Clé vide = rien n'est envoyé.
// Cette clé ne permet que d'écrire des événements : elle peut être dans l'appli.
constexpr const char* ANALYTICS_URL = "https://eu.i.posthog.com/batch/";
constexpr const char* ANALYTICS_KEY = "";

} // namespace cfg
