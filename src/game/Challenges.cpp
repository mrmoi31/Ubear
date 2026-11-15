#include "Challenges.hpp"
#include "../core/Config.hpp"
#include "../core/Locale.hpp"

#include <algorithm>
#include <cstdio>
#include <ctime>

static const ChallengeDef EASY[] = {
    {CH_DELIVERIES, 5, 50}, {CH_HOT, 3, 50}, {CH_REGULARS, 3, 50}, {CH_DRY, 2, 60}, {CH_NIGHT, 2, 60},
};
static const ChallengeDef MEDIUM[] = {
    {CH_HAIL, 5, 120}, {CH_DRY, 3, 100}, {CH_FOG, 4, 110}, {CH_WIND, 4, 110}, {CH_STREAK, 5, 120}, {CH_COINS, 150, 100},
};
static const ChallengeDef HARD[] = {
    {CH_BLIZZARD, 2, 220}, {CH_AURORA, 3, 220}, {CH_STREAK, 10, 250}, {CH_DRY, 8, 200}, {CH_COINS, 400, 200}, {CH_HAIL, 10, 220},
};

template <size_t N> static constexpr int countOf(const ChallengeDef (&)[N]) { return (int)N; }

static unsigned lcg(unsigned& s) { s = s * 1664525u + 1013904223u; return s >> 8; }

static void pickForDay(int day, ChallengeDef out[CHALLENGE_SLOTS])
{
    unsigned s = (unsigned)day * 2654435761u + 12345u;
    for (int i = 0; i < 4; i++) lcg(s);
    out[0] = EASY[lcg(s) % countOf(EASY)];
    do { out[1] = MEDIUM[lcg(s) % countOf(MEDIUM)]; } while (out[1].type == out[0].type);
    do { out[2] = HARD[lcg(s) % countOf(HARD)]; } while (out[2].type == out[0].type || out[2].type == out[1].type);
}

int challenges::today()
{
    time_t t = time(nullptr);
    struct tm lt = *localtime(&t);
    return (lt.tm_year + 1900) * 10000 + (lt.tm_mon + 1) * 100 + lt.tm_mday;
}

int challenges::secondsUntilReset()
{
    time_t t = time(nullptr);
    struct tm lt = *localtime(&t);
    return 86400 - (lt.tm_hour * 3600 + lt.tm_min * 60 + lt.tm_sec);
}

bool challenges::refresh(SaveData& d)
{
    int day = today();
    if (d.chalDay == day) return false;
    d.chalDay = day;
    for (int i = 0; i < CHALLENGE_SLOTS; i++) { d.chalProg[i] = 0; d.chalDone[i] = false; }
    d.chalBonus = false;
    return true;
}

ChallengeDef challenges::get(const SaveData& d, int slot)
{
    ChallengeDef defs[CHALLENGE_SLOTS];
    pickForDay(d.chalDay ? d.chalDay : today(), defs);
    return defs[slot];
}

std::string challenges::describe(const ChallengeDef& c)
{
    const char* fmt = "";
    switch (c.type) {
    case CH_DELIVERIES: fmt = L("%d livraisons", "%d deliveries"); break;
    case CH_HAIL:       fmt = L("%d livraisons dans la grêle", "%d deliveries through hail"); break;
    case CH_DRY:        fmt = L("%d livraisons sans toucher l'eau", "%d deliveries without touching water"); break;
    case CH_FOG:        fmt = L("%d livraisons dans le brouillard", "%d deliveries through fog"); break;
    case CH_WIND:       fmt = L("%d livraisons dans le vent", "%d deliveries through wind"); break;
    case CH_STREAK:     fmt = L("Une série de %d livraisons", "A streak of %d deliveries"); break;
    case CH_COINS:      fmt = L("Gagner %d pièces aujourd'hui", "Earn %d coins today"); break;
    case CH_HOT:        fmt = L("%d livraisons encore bien chaudes", "%d piping-hot deliveries"); break;
    case CH_NIGHT:      fmt = L("%d livraisons de nuit", "%d night deliveries"); break;
    case CH_BLIZZARD:   fmt = L("%d livraisons pendant un blizzard", "%d deliveries during a blizzard"); break;
    case CH_AURORA:     fmt = L("%d livraisons sous l'aurore boréale", "%d deliveries under the aurora"); break;
    case CH_REGULARS:   fmt = L("%d livraisons à des habitués", "%d deliveries to regulars"); break;
    default: break;
    }
    char buf[160];
    snprintf(buf, sizeof(buf), fmt, c.target);
    return buf;
}

const char* challenges::tierName(int slot)
{
    static const char* fr[] = {"Facile", "Moyen", "Difficile"};
    static const char* en[] = {"Easy", "Medium", "Hard"};
    return L(fr[slot], en[slot]);
}

int challenges::doneCount(const SaveData& d)
{
    int n = 0;
    for (int i = 0; i < CHALLENGE_SLOTS; i++) n += d.chalDone[i] ? 1 : 0;
    return n;
}

static bool matches(ChalType t, const DeliveryReport& r)
{
    switch (t) {
    case CH_DELIVERIES: return true;
    case CH_HAIL:       return r.hail;
    case CH_DRY:        return !r.water;
    case CH_FOG:        return r.fog;
    case CH_WIND:       return r.wind;
    case CH_HOT:        return r.heatLeft >= 0.6f;
    case CH_NIGHT:      return r.night;
    case CH_BLIZZARD:   return r.blizzard;
    case CH_AURORA:     return r.aurora;
    case CH_REGULARS:   return r.regular;
    default:            return false;
    }
}

std::vector<challenges::Completion> challenges::onDelivery(SaveData& d, const DeliveryReport& r)
{
    refresh(d);
    std::vector<Completion> done;
    for (int i = 0; i < CHALLENGE_SLOTS; i++) {
        if (d.chalDone[i]) continue;
        ChallengeDef c = get(d, i);
        if (c.type == CH_STREAK)     d.chalProg[i] = std::max(d.chalProg[i], r.streak);
        else if (c.type == CH_COINS) d.chalProg[i] += r.coins;
        else if (matches(c.type, r)) d.chalProg[i] += 1;

        if (d.chalProg[i] >= c.target) {
            d.chalProg[i] = c.target;
            d.chalDone[i] = true;
            d.chalTotalDone++;
            d.coins += c.reward;
            done.push_back({i, c.reward, false});
        }
    }
    if (!d.chalBonus && doneCount(d) == CHALLENGE_SLOTS) {
        d.chalBonus = true;
        d.coins += cfg::CHALLENGE_ALL_BONUS;
        done.push_back({-1, cfg::CHALLENGE_ALL_BONUS, true});
    }
    return done;
}
