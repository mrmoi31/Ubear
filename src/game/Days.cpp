#include "Days.hpp"
#include "Challenges.hpp"
#include "../core/Config.hpp"
#include "../core/Locale.hpp"

#include <ctime>

static const int GIFT[7] = {40, 55, 75, 100, 130, 170, 250};

static const MoodFx MOODS[] = {
    {"Vent du large", "Offshore wind", "Le vent pousse plus fort.", "The wind pushes harder.",
     1.0f, 1.05f, 1.0f, 1.45f, 1.0f, 0.0f},
    {"Grêle tenace", "Stubborn hail", "Les plats refroidissent plus vite.", "Food cools faster.",
     1.28f, 1.15f, 1.0f, 1.1f, 1.0f, 0.0f},
    {"Clients pressés", "Customers in a hurry", "Beaucoup moins de temps au chaud.", "Much less time to stay hot.",
     1.22f, 1.2f, 1.0f, 1.0f, 1.0f, 0.0f},
    {"Pourboires fous", "Wild tips", "Plus de pièces, un peu moins de temps.", "More coins, a little less time.",
     1.12f, 1.4f, 1.0f, 1.0f, 1.15f, 0.0f},
    {"Igloos lointains", "Far igloos", "Plus loin, mieux payé.", "Farther away, better pay.",
     1.0f, 1.25f, 1.35f, 1.0f, 1.0f, 0.0f},
    {"Alerte blizzard", "Blizzard watch", "Le blizzard arrive plus tôt.", "The blizzard comes sooner.",
     1.08f, 1.15f, 1.0f, 1.7f, 1.0f, 0.0f},
    {"Nuit précoce", "Early night", "La nuit tombe tôt : l'aurore peut payer.", "Night falls early: the aurora can pay.",
     1.0f, 1.1f, 1.0f, 1.05f, 1.0f, 1.6f},
    {"Série brûlante", "Hot streak", "Les séries rapportent gros, mais ça refroidit.", "Streaks pay big, but food cools.",
     1.18f, 1.0f, 1.0f, 1.0f, 1.7f, 0.0f},
};

const MoodFx& moodFor(int day)
{
    int n = (int)(sizeof(MOODS) / sizeof(MOODS[0]));
    int i = cfg::dayIndex(day) - 1;
    return MOODS[(i % n + n) % n];
}

static int dayBefore(int ymd)
{
    std::tm tm{};
    tm.tm_year = ymd / 10000 - 1900;
    tm.tm_mon = (ymd / 100) % 100 - 1;
    tm.tm_mday = ymd % 100;
    tm.tm_hour = 12;
    std::time_t t = std::mktime(&tm);
    t -= 24 * 60 * 60;
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    return (tm.tm_year + 1900) * 10000 + (tm.tm_mon + 1) * 100 + tm.tm_mday;
}

static long long nowSec() { return (long long)std::time(nullptr); }

void vitality::refresh(SaveData& d)
{
    long long now = nowSec();
    if (d.energy < 0) d.energy = 0;
    if (d.energyAt <= 0) {
        d.energy = cfg::ENERGY_MAX;
        d.energyAt = now;
        return;
    }
    if (d.energy >= cfg::ENERGY_MAX) {
        d.energy = cfg::ENERGY_MAX;
        d.energyAt = now;
        return;
    }
    long long gained = (now - d.energyAt) / cfg::ENERGY_REGEN_SEC;
    if (gained > 0) {
        d.energy += (int)gained;
        d.energyAt += gained * cfg::ENERGY_REGEN_SEC;
        if (d.energy >= cfg::ENERGY_MAX) {
            d.energy = cfg::ENERGY_MAX;
            d.energyAt = now;
        }
    }
}

void vitality::spend(SaveData& d)
{
    refresh(d);
    if (d.energy >= cfg::ENERGY_MAX) d.energyAt = nowSec();
    if (d.energy > 0) d.energy--;
}

int vitality::secondsToNext(const SaveData& d)
{
    if (d.energy >= cfg::ENERGY_MAX || d.energyAt <= 0) return 0;
    long long rem = cfg::ENERGY_REGEN_SEC - (nowSec() - d.energyAt);
    return rem < 0 ? 0 : (int)rem;
}

bool rewards::ready(const SaveData& d) { return d.rewardDate != challenges::today(); }

static int nextStreak(const SaveData& d)
{
    if (!rewards::ready(d)) return d.rewardStreak < 1 ? 1 : d.rewardStreak;
    int s = (d.rewardDate == dayBefore(challenges::today())) ? d.rewardStreak + 1 : 1;
    if (s > 7) s = 1;
    if (s < 1) s = 1;
    return s;
}

int rewards::peek(const SaveData& d) { return GIFT[nextStreak(d) - 1]; }

int rewards::streakAfterClaim(const SaveData& d) { return nextStreak(d); }

int rewards::claim(SaveData& d)
{
    if (!ready(d)) return 0;
    int s = nextStreak(d);
    int amt = GIFT[s - 1];
    d.rewardStreak = s;
    d.rewardDate = challenges::today();
    d.coins += amt;
    return amt;
}

static const StoreOffer STORE[] = {
    {"remove_ads", 0, true, "3,99 €", "Supprimer les pubs", "Remove ads"},
    {"coins_small", 250, false, "0,99 €", "Poignée de pièces", "A handful of coins"},
    {"coins_mid", 900, false, "2,99 €", "Sac de pièces", "A bag of coins"},
    {"coins_big", 2500, false, "6,99 €", "Coffre de Gaston", "Gaston's chest"},
};

void offers::sync(SaveData& d)
{
    int today = challenges::today();
    if (d.energyAdDate != today) { d.energyAdDate = today; d.energyAdsUsed = 0; }
    if (d.goldAdDate != today) { d.goldAdDate = today; d.goldAdsUsed = 0; }
}

int offers::energyAdsLeft(const SaveData& d)
{
    int used = d.energyAdDate == challenges::today() ? d.energyAdsUsed : 0;
    int left = cfg::ENERGY_AD_MAX - used;
    return left > 0 ? left : 0;
}

bool offers::grantEnergyAd(SaveData& d)
{
    sync(d);
    if (d.energyAdsUsed >= cfg::ENERGY_AD_MAX) return false;
    d.energyAdsUsed++;
    d.energy += cfg::ENERGY_AD_GRANT;
    if (d.energy > cfg::ENERGY_MAX) d.energy = cfg::ENERGY_MAX;
    if (d.energy >= cfg::ENERGY_MAX) d.energyAt = nowSec();
    return true;
}

int offers::goldAdsLeft(const SaveData& d)
{
    int used = d.goldAdDate == challenges::today() ? d.goldAdsUsed : 0;
    int left = cfg::GOLD_AD_MAX - used;
    return left > 0 ? left : 0;
}

int offers::grantGoldAd(SaveData& d)
{
    sync(d);
    if (d.goldAdsUsed >= cfg::GOLD_AD_MAX) return 0;
    d.goldAdsUsed++;
    d.coins += cfg::GOLD_AD_GRANT;
    return cfg::GOLD_AD_GRANT;
}

int offers::storeCount() { return (int)(sizeof(STORE) / sizeof(STORE[0])); }

const StoreOffer& offers::store(int i)
{
    if (i < 0 || i >= storeCount()) i = 0;
    return STORE[i];
}

bool offers::buy(SaveData& d, int i)
{
    if (i < 0 || i >= storeCount()) return false;
    const StoreOffer& o = STORE[i];
    if (o.removeAds) {
        if (d.adsRemoved) return false;
        d.adsRemoved = true;
        return true;
    }
    d.coins += o.coins;
    return true;
}
