#include "Analytics.hpp"
#include "Config.hpp"
#include "Locale.hpp"
#include "Platform.hpp"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <random>
#include <vector>

namespace {
SaveData* g_save = nullptr;
std::vector<std::string> g_queue;
std::string g_session;
bool g_open = false;   // session en cours
std::chrono::steady_clock::time_point g_sessionStart;

#if defined(PLATFORM_ANDROID)
const char* const PLATFORM_NAME = "android";
#elif defined(PLATFORM_IOS)
const char* const PLATFORM_NAME = "ios";
#else
const char* const PLATFORM_NAME = "desktop";
#endif

std::string randomHex(int bytes)
{
    static std::mt19937_64 rng{std::random_device{}() ^ (unsigned long long)std::time(nullptr)};
    static const char* HEX = "0123456789abcdef";
    std::string out;
    for (int i = 0; i < bytes; i++) {
        unsigned v = (unsigned)(rng() & 0xff);
        out += HEX[v >> 4];
        out += HEX[v & 15];
    }
    return out;
}

std::string isoNow()
{
    std::time_t t = std::time(nullptr);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buf;
}

bool enabled() { return g_save && g_save->analytics; }

void openSession()
{
    g_session = randomHex(16);
    g_sessionStart = std::chrono::steady_clock::now();
    g_open = true;
    analytics::track("session_start");
}

void closeSession()
{
    if (!g_open) return;
    double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - g_sessionStart).count();
    analytics::track("session_end", {analytics::num("duration_s", (double)(long long)secs)});
    g_open = false;
}
} // namespace

std::string analytics::escape(const std::string& s)
{
    std::string out = "\"";
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') { out += '\\'; out += (char)c; }
        else if (c == '\n') out += "\\n";
        else if (c < 0x20) { char b[8]; std::snprintf(b, sizeof(b), "\\u%04x", c); out += b; }
        else out += (char)c;   // l'UTF-8 (accents) passe tel quel
    }
    return out + "\"";
}

analytics::Prop analytics::num(const char* key, double v)
{
    char b[32];
    std::snprintf(b, sizeof(b), "%.10g", v);
    return {key, b};
}

analytics::Prop analytics::str(const char* key, const std::string& v) { return {key, escape(v)}; }
analytics::Prop analytics::flag(const char* key, bool v) { return {key, v ? "true" : "false"}; }

void analytics::start(SaveData& save)
{
    g_save = &save;
    if (save.installId.empty()) {
        save.installId = randomHex(16);
        save::write(save);
    }
    openSession();
    flush();   // pousse aussi les lots restés en attente lors des parties précédentes
}

void analytics::foreground(bool on)
{
    if (on) {
        if (!g_open) openSession();
    } else {
        closeSession();
        flush();
    }
}

void analytics::track(const char* event, std::initializer_list<Prop> props)
{
    if (!enabled()) return;
    SaveData& s = *g_save;
    if (s.installId.empty()) s.installId = randomHex(16);   // sauvegarde remplacée ou abîmée
    std::string e = "{\"event\":" + escape(event) + ",\"timestamp\":\"" + isoNow() + "\",\"properties\":{";
    e += "\"distinct_id\":" + escape(s.installId);
    e += ",\"$session_id\":" + escape(g_session);
    e += ",\"$process_person_profile\":false";
    e += ",\"platform\":" + escape(PLATFORM_NAME);
    e += ",\"app_version\":" + escape(cfg::VERSION);
    e += ",\"lang\":" + escape(s.lang == 0 ? "fr" : "en");
    e += ",\"player_day\":" + num("", s.day).json;
    e += ",\"player_coins\":" + num("", s.coins).json;
    for (const Prop& p : props) e += "," + escape(p.key) + ":" + p.json;
    e += "}}";
    g_queue.push_back(std::move(e));
    if (g_queue.size() >= 30) flush();
}

void analytics::flush()
{
    if (g_queue.empty()) return;
#if defined(PLATFORM_ANDROID) || defined(PLATFORM_IOS)
    // Pas de clé PostHog configurée : rien ne part sur le réseau.
    if (!cfg::ANALYTICS_KEY[0]) { g_queue.clear(); return; }
#endif
    std::string body = "{\"api_key\":" + escape(cfg::ANALYTICS_KEY) + ",\"batch\":[";
    for (size_t i = 0; i < g_queue.size(); i++) body += (i ? "," : "") + g_queue[i];
    body += "]}";
    g_queue.clear();
    platform::sendAnalytics(cfg::ANALYTICS_URL, body.c_str());
}

void analytics::setEnabled(bool on)
{
    if (!g_save || g_save->analytics == on) return;
    if (!on) {
        g_queue.clear();       // refus : on jette aussi ce qui n'est pas encore parti
        g_save->analytics = false;
        g_open = false;
    } else {
        g_save->analytics = true;
        openSession();
    }
}
