// Sauvegarde locale (fichier texte clé=valeur dans le stockage interne de l'app).
#pragma once
#include <set>
#include <string>

enum Upgrade { UP_SKATES = 0, UP_BRAKES, UP_GPS, UP_BAG, UP_ANCHOR, UP_COUNT };
enum CosCat  { COS_SKIN = 0, COS_HAT, COS_BAG, COS_SKATE, COS_TRAIL, COS_COUNT };

constexpr int CUSTOMER_COUNT  = 5;
constexpr int CHALLENGE_SLOTS = 3;

struct SaveData {
    int   coins = 0;
    int   upgrades[UP_COUNT] = {0, 0, 0, 0, 0};
    std::set<std::string> owned;            // ids de cosmétiques possédés
    std::string equipped[COS_COUNT];        // id équipé par catégorie
    float musicVol = 0.6f;
    float sfxVol   = 1.0f;
    int   vibration = 1;                    // 0 aucune, 1 toutes, 2 importantes seulement
    int   lang = 0;                         // 0 FR, 1 EN
    bool  analytics = false;                // statistiques anonymes acceptées
    bool  consentAsked = false;             // question des statistiques posée au premier lancement
    std::string installId;                  // identifiant aléatoire de l'installation (statistiques)
    bool  tutorialDone = false;
    bool  storySeen = false;
    int   bestShiftCoins = 0;
    int   totalDeliveries = 0;
    int   bestStreak = 0;
    int   day = 1;                          // prochaine journée à jouer (1, 2, 3...)

    // Défis quotidiens : jour (AAAAMMJJ) auquel se rapportent la progression et les validations.
    int   chalDay = 0;
    int   chalProg[CHALLENGE_SLOTS] = {0, 0, 0};
    bool  chalDone[CHALLENGE_SLOTS] = {false, false, false};
    bool  chalBonus = false;
    int   chalTotalDone = 0;

    // Clients récurrents : nombre de livraisons réussies par client.
    int   custCount[CUSTOMER_COUNT] = {0, 0, 0, 0, 0};

    int   blizzardsSurvived = 0;
    int   auroras = 0;

    int       energy = 5;               // points pour lancer une journée
    long long energyAt = 0;             // ancre de régénération (secondes unix)
    int       rewardDate = 0;           // AAAAMMJJ du dernier cadeau
    int       rewardStreak = 0;         // jours d'affilée, 1..7
    bool      adsRemoved = false;
    int       energyAdDate = 0;         // AAAAMMJJ
    int       energyAdsUsed = 0;        // pubs énergie ce jour, max 3
    int       goldAdDate = 0;
    int       goldAdsUsed = 0;

    void applyDefaults();                   // cosmétiques gratuits possédés/équipés
};

namespace save {
bool load(SaveData& out);
bool write(const SaveData& data);
void erase(SaveData& data);
}
