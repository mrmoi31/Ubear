#include "Save.hpp"
#include "Config.hpp"

#include <raylib.h>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

void SaveData::applyDefaults()
{
    const char* defaults[COS_COUNT] = {"skin_polaire", "hat_none", "bag_original", "skate_original", "trail_none"};
    for (int i = 0; i < COS_COUNT; i++) {
        owned.insert(defaults[i]);
        if (equipped[i].empty()) equipped[i] = defaults[i];
    }
}

static std::vector<std::string> split(const std::string& s, char sep)
{
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == sep) { if (!cur.empty()) out.push_back(cur); cur.clear(); }
        else cur += c;
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

template <typename T>
static void readInts(const std::string& val, T* dst, int count)
{
    auto parts = split(val, ',');
    for (size_t i = 0; i < parts.size() && (int)i < count; i++) dst[i] = (T)atoi(parts[i].c_str());
}

template <typename T>
static void writeInts(std::ostringstream& out, const char* key, const T* src, int count)
{
    out << key << "=";
    for (int i = 0; i < count; i++) out << (int)src[i] << (i + 1 < count ? "," : "");
    out << "\n";
}

// Sur Android, raylib redirige les écritures vers le stockage interne de l'app.
// Sur PC (tests), on évite d'écrire dans assets/ pour ne pas l'embarquer dans l'APK.
static const char* savePath()
{
#if defined(PLATFORM_ANDROID)
    return cfg::SAVE_FILE;
#else
    return TextFormat("%s%s", GetApplicationDirectory(), cfg::SAVE_FILE);
#endif
}

bool save::load(SaveData& d)
{
    d = SaveData{};
    char* text = LoadFileText(savePath());
    if (!text) { d.applyDefaults(); return false; }

    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);

        if      (key == "coins")           d.coins = atoi(val.c_str());
        else if (key == "musicVol")        d.musicVol = (float)atof(val.c_str());
        else if (key == "sfxVol")          d.sfxVol = (float)atof(val.c_str());
        else if (key == "vibration")       { int v = atoi(val.c_str()); d.vibration = (v >= 0 && v <= 2) ? v : 1; }
        else if (key == "lang")            d.lang = atoi(val.c_str());
        else if (key == "analytics")       d.analytics = atoi(val.c_str()) != 0;
        else if (key == "installId")       d.installId = val;
        else if (key == "tutorialDone")    d.tutorialDone = atoi(val.c_str()) != 0;
        else if (key == "bestShiftCoins")  d.bestShiftCoins = atoi(val.c_str());
        else if (key == "totalDeliveries") d.totalDeliveries = atoi(val.c_str());
        else if (key == "bestStreak")      d.bestStreak = atoi(val.c_str());
        else if (key == "day")             d.day = atoi(val.c_str());
        else if (key == "storySeen")       d.storySeen = atoi(val.c_str()) != 0;
        else if (key == "chalDay")         d.chalDay = atoi(val.c_str());
        else if (key == "chalProg")        readInts(val, d.chalProg, CHALLENGE_SLOTS);
        else if (key == "chalDone")        readInts(val, d.chalDone, CHALLENGE_SLOTS);
        else if (key == "chalBonus")       d.chalBonus = atoi(val.c_str()) != 0;
        else if (key == "chalTotalDone")   d.chalTotalDone = atoi(val.c_str());
        else if (key == "customers")       readInts(val, d.custCount, CUSTOMER_COUNT);
        else if (key == "blizzards")       d.blizzardsSurvived = atoi(val.c_str());
        else if (key == "auroras")         d.auroras = atoi(val.c_str());
        else if (key == "energy")          d.energy = atoi(val.c_str());
        else if (key == "energyAt")        d.energyAt = atoll(val.c_str());
        else if (key == "rewardDate")      d.rewardDate = atoi(val.c_str());
        else if (key == "rewardStreak")    d.rewardStreak = atoi(val.c_str());
        else if (key == "adsRemoved")      d.adsRemoved = atoi(val.c_str()) != 0;
        else if (key == "energyAdDate")    d.energyAdDate = atoi(val.c_str());
        else if (key == "energyAdsUsed")   d.energyAdsUsed = atoi(val.c_str());
        else if (key == "goldAdDate")      d.goldAdDate = atoi(val.c_str());
        else if (key == "goldAdsUsed")     d.goldAdsUsed = atoi(val.c_str());
        else if (key == "upgrades") {
            auto parts = split(val, ',');
            for (size_t i = 0; i < parts.size() && i < UP_COUNT; i++) {
                int v = atoi(parts[i].c_str());
                d.upgrades[i] = v < 0 ? 0 : (v > 3 ? 3 : v);
            }
        }
        else if (key == "owned") {
            for (auto& id : split(val, ',')) d.owned.insert(id);
        }
        else if (key == "equipped") {
            auto parts = split(val, ',');
            for (size_t i = 0; i < parts.size() && i < COS_COUNT; i++) d.equipped[i] = parts[i];
        }
    }
    UnloadFileText(text);
    if (d.coins < 0) d.coins = 0;
    d.applyDefaults();
    return true;
}

bool save::write(const SaveData& d)
{
    std::ostringstream out;
    out << "coins=" << d.coins << "\n";
    out << "upgrades=";
    for (int i = 0; i < UP_COUNT; i++) out << d.upgrades[i] << (i + 1 < UP_COUNT ? "," : "");
    out << "\n";
    out << "owned=";
    bool first = true;
    for (auto& id : d.owned) { out << (first ? "" : ",") << id; first = false; }
    out << "\n";
    out << "equipped=";
    for (int i = 0; i < COS_COUNT; i++) out << d.equipped[i] << (i + 1 < COS_COUNT ? "," : "");
    out << "\n";
    out << "musicVol=" << d.musicVol << "\n";
    out << "sfxVol=" << d.sfxVol << "\n";
    out << "vibration=" << d.vibration << "\n";
    out << "lang=" << d.lang << "\n";
    out << "analytics=" << (d.analytics ? 1 : 0) << "\n";
    out << "installId=" << d.installId << "\n";
    out << "tutorialDone=" << (d.tutorialDone ? 1 : 0) << "\n";
    out << "bestShiftCoins=" << d.bestShiftCoins << "\n";
    out << "totalDeliveries=" << d.totalDeliveries << "\n";
    out << "bestStreak=" << d.bestStreak << "\n";
    out << "day=" << d.day << "\n";
    out << "storySeen=" << (d.storySeen ? 1 : 0) << "\n";
    out << "chalDay=" << d.chalDay << "\n";
    writeInts(out, "chalProg", d.chalProg, CHALLENGE_SLOTS);
    writeInts(out, "chalDone", d.chalDone, CHALLENGE_SLOTS);
    out << "chalBonus=" << (d.chalBonus ? 1 : 0) << "\n";
    out << "chalTotalDone=" << d.chalTotalDone << "\n";
    writeInts(out, "customers", d.custCount, CUSTOMER_COUNT);
    out << "blizzards=" << d.blizzardsSurvived << "\n";
    out << "auroras=" << d.auroras << "\n";
    out << "energy=" << d.energy << "\n";
    out << "energyAt=" << d.energyAt << "\n";
    out << "rewardDate=" << d.rewardDate << "\n";
    out << "rewardStreak=" << d.rewardStreak << "\n";
    out << "adsRemoved=" << (d.adsRemoved ? 1 : 0) << "\n";
    out << "energyAdDate=" << d.energyAdDate << "\n";
    out << "energyAdsUsed=" << d.energyAdsUsed << "\n";
    out << "goldAdDate=" << d.goldAdDate << "\n";
    out << "goldAdsUsed=" << d.goldAdsUsed << "\n";

    std::string s = out.str();
    // SaveFileText attend un char* non-const
    std::vector<char> buf(s.begin(), s.end());
    buf.push_back('\0');
    return SaveFileText(savePath(), buf.data());
}

void save::erase(SaveData& d)
{
    float mv = d.musicVol, sv = d.sfxVol;
    int vib = d.vibration;
    int lang = d.lang;
    bool stats = d.analytics;
    std::string install = d.installId;   // même appareil : même identifiant
    d = SaveData{};
    d.musicVol = mv; d.sfxVol = sv; d.vibration = vib; d.lang = lang;
    d.analytics = stats; d.installId = install;
    d.storySeen = true;
    d.applyDefaults();
    write(d);
}
