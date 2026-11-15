// UBear — « Livré chaud, ou c'est l'ours qui paie. »
#include "App.hpp"
#include "core/Config.hpp"
#include "core/Locale.hpp"
#include "core/Platform.hpp"
#include "game/Challenges.hpp"
#include "game/Days.hpp"
#include "game/Session.hpp"
#include "screens/Screens.hpp"

#include <raylib.h>
#include <algorithm>
#include <cstring>
#include <ctime>
#include <string>

static App* g_app = nullptr;

App::App() = default;
App::~App() = default;

void App::go(Screen s)
{
    screen = s;
    screenTime = 0.0f;
    confirmReset = false;
    shopScroll = 0.0f;
    if (s == Screen::Shop) { shopSelected.clear(); gastonLine = GetRandomValue(0, 100); }
}

void App::persist() { save::write(save); }

static void syncMusic(bool poll);

void App::applyAudio()
{
    assets.sfxVolume = save.sfxVol;
    if (assets.audioReady) syncMusic(true);
}

void App::vibrate(int ms)
{
    if (save.vibration && !autopilot) platform::vibrate(ms);
}

void App::notify(const std::string& text)
{
    toastText = text;
    toastTime = 2.2f;
}

void App::startShift()
{
    if (!autopilot) {
        vitality::refresh(save);
        if (save.energy <= 0) {
            int secs = vitality::secondsToNext(save);
            int h = secs / 3600, m = (secs % 3600) / 60;
            notify(TextFormat(L("Barnabé dort encore. Énergie dans %dh%02d.",
                                "Barnaby is still asleep. Energy in %dh%02d."), h, m));
            return;
        }
        vitality::spend(save);
        persist();
    }
    session = std::make_unique<Session>(*this);
    go(Screen::Game);
}

void App::finishShift()
{
    lastResult = session->result();
    session.reset();
    go(Screen::Results);
    if (!autopilot && !save.adsRemoved) beginAd(AdSlot::EndShift);
}

void App::beginAd(AdSlot slot)
{
    adSlot = slot;
    adTime = 0.0f;
}

void App::finishAd()
{
    offers::sync(save);
    if (adSlot == AdSlot::Energy && offers::grantEnergyAd(save))
        notify(L("+5 journées.", "+5 days."));
    else if (adSlot == AdSlot::Gold) {
        int n = offers::grantGoldAd(save);
        if (n > 0) notify(TextFormat(L("+%d pièces.", "+%d coins."), n));
    }
    if (adSlot != AdSlot::None) persist();
    adSlot = AdSlot::None;
    adTime = 0.0f;
}

static bool g_appFocused = true;
static bool g_musicOn = false;
static float g_musicPoll = 1.0f;

static void syncMusic(bool poll)
{
    if (!g_app || !g_app->assets.audioReady) return;
    if (poll) g_musicPoll = 1.0f;
    bool want = g_appFocused && platform::allowGameMusic() && g_app->save.musicVol > 0.001f;
    if (want && !g_musicOn) {
        PlayMusicStream(g_app->assets.music);
        SetMusicVolume(g_app->assets.music, g_app->save.musicVol);
        g_musicOn = true;
    } else if (!want && g_musicOn) {
        StopMusicStream(g_app->assets.music);
        g_musicOn = false;
    } else if (want) {
        SetMusicVolume(g_app->assets.music, g_app->save.musicVol);
    }
}

static void onFocus(bool focused)
{
    g_appFocused = focused;
    if (!g_app || !g_app->assets.audioReady) return;
    if (!focused) {
        if (g_app->session) g_app->session->pause();
        save::write(g_app->save);
    }
    syncMusic(true);
}

// ---------------------------------------------------------------------------
// Autotest (PC uniquement) : parcourt les écrans, joue un service, capture.
// ---------------------------------------------------------------------------
#if !defined(PLATFORM_ANDROID)
struct SelfTest {
    bool        on = false;
    std::string dir;
    int         frame = 0;

    void shot(const char* name)
    {
        Image img = LoadImageFromScreen();
        // écran Windows à 200 % : le rendu occupe le coin bas-gauche du tampon
        int sw = GetScreenWidth(), sh = GetScreenHeight();
        if (img.width > sw) ImageCrop(&img, {0, (float)(img.height - sh), (float)sw, (float)sh});
        ExportImage(img, TextFormat("%s/%s.png", dir.c_str(), name));
        UnloadImage(img);
        TraceLog(LOG_WARNING, "SELFTEST: capture %s", name);
    }

    // Appelé avant le rendu de la frame.
    void before(App& app)
    {
        int f = frame;
        if (f == 1) { app.save = SaveData{}; app.save.applyDefaults(); app.save.coins = 1234; app.storyPage = 0; app.go(Screen::Story); }
        if (f == 30) app.storyPage = 2;
        if (f == 60) app.storyPage = 5;
        if (f == 90) app.go(Screen::Menu);
        if (f == 120) { app.go(Screen::Shop); app.shopTab = 0; }
        if (f == 150) {
            app.shopTab = 1; app.shopCat = 0;
            for (const char* id : {"skin_panda", "skin_marron", "hat_couronne", "bag_rouge", "skate_or", "trail_rainbow"}) app.save.owned.insert(id);
            app.save.equipped[COS_SKIN] = "skin_panda"; app.save.equipped[COS_HAT] = "hat_couronne";
            app.save.equipped[COS_BAG] = "bag_rouge"; app.save.equipped[COS_SKATE] = "skate_or";
            app.save.equipped[COS_TRAIL] = "trail_rainbow";
        }
        if (f == 157) app.shopCat = 1;
        if (f == 162) app.shopCat = 2;
        if (f == 174) app.shopCat = 3;
        if (f == 179) app.shopCat = 4;
        if (f == 420 && app.session) app.session->debugLookAt(0);
        if (f == 460 && app.session) app.session->debugLookAt(4);
        if (f == 500 && app.session) app.session->debugLookAt(5);
        if (f == 540 && app.session) app.session->debugLookAt(6);
        if (f == 580 && app.session) app.session->debugLookAt(7);
        if (f == 620 && app.session) app.session->debugLookAt(8);
        if (f == 660 && app.session) app.session->debugLookAt(-1);
        if (f == 180) { app.save.custCount[0] = 12; app.save.custCount[1] = 4; app.save.custCount[3] = 49; app.go(Screen::Customers); }
        if (f == 210) { app.save.chalProg[0] = 2; app.go(Screen::Challenges); }
        if (f == 240) app.go(Screen::Settings);
        if (f == 270) {
            app.save.upgrades[UP_GPS] = 3; app.save.upgrades[UP_BRAKES] = 1; app.save.upgrades[UP_BAG] = 2;
            app.save.tutorialDone = false;
            app.autopilot = true;
            app.startShift();
        }
        if (f == 3000 && app.session) app.session->debugNight(true);
        if (f == 3700 && app.session) app.session->debugBlizzard();
        if (f == 5200 && app.session) app.session->pause();
        if (f == 5230 && app.session) app.session->endShift();
        if (f == 5300) app.quit = true;
    }

    // Appelé juste avant EndDrawing().
    void after(App& app)
    {
        static const struct { int f; const char* n; } shots[] = {
            {20, "01_histoire"}, {50, "02_histoire_commandes"}, {80, "03_histoire_clients"}, {110, "04_menu"},
            {140, "05_boutique_equipement"}, {156, "06_pelages"}, {160, "06_chapeaux"}, {168, "06_sacs"}, {178, "06_rollers"}, {179, "06_trainees"},
            {200, "07_carnet_clients"},
            {440, "18_hub"}, {480, "19_pingouin"}, {520, "20_morse"}, {560, "21_renard"}, {600, "22_yeti"}, {640, "23_phoques"},
            {230, "08_defis"}, {260, "09_options"}, {300, "10_tutoriel"}, {1800, "11_service"},
            {2600, "12_service_bis"}, {3300, "13_nuit_aurore"}, {3900, "14_blizzard"}, {4300, "15_blizzard_fin"},
            {5215, "16_pause"}, {5260, "17_bilan"},
        };
        for (auto& s : shots) if (s.f == frame) shot(s.n);
        if (frame == 5250) {
            const ShiftResult& r = app.lastResult;
            TraceLog(LOG_WARNING, "SELFTEST: livraisons=%d echecs=%d pieces=%d serie=%d habitues=%d defis=%d blizzards=%d aurore=%d",
                     r.deliveries, r.failed, r.coins, r.bestStreak, r.regulars, r.challenges, r.blizzards, r.auroraDeliveries);
        }
        frame++;
    }
};
#endif

#if defined(PLATFORM_IOS)
extern "C" {
const char *IosResourcePath(void);
int ios_boot(int argc, char **argv);
void ubear_ios_init(void);
int ubear_ios_frame(void);
}
#endif

static bool locateAssets()
{
#if defined(PLATFORM_ANDROID)
    return true;
#elif defined(PLATFORM_IOS)
    const char *res = IosResourcePath();
    if (res && res[0]) ChangeDirectory(res);
    return FileExists("Body.png");
#else
    const char* base = GetApplicationDirectory();
    for (const char* rel : {"assets", "../assets", "../../assets", "../../../assets"}) {
        const char* dir = TextFormat("%s%s", base, rel);
        if (FileExists(TextFormat("%s/Body.png", dir))) { ChangeDirectory(dir); return true; }
    }
    return FileExists("Body.png");
#endif
}

struct Host {
    App app;
#if !defined(PLATFORM_ANDROID)
    SelfTest test;
#endif
};

static Host *g_host = nullptr;

static void bootGame(int argc, char **argv)
{
    g_host = new Host();
#if !defined(PLATFORM_ANDROID) && !defined(PLATFORM_IOS)
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--selftest") == 0 && i + 1 < argc) { g_host->test.on = true; g_host->test.dir = argv[++i]; }
    }
#else
    (void)argc; (void)argv;
#endif

    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
#if defined(PLATFORM_ANDROID) || defined(PLATFORM_IOS)
    InitWindow(0, 0, "UBear");
#else
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(450, 900, "UBear");
#endif
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);
    SetRandomSeed((unsigned)time(nullptr));
    locateAssets();

    App &app = g_host->app;
    g_app = &app;
    platform::setFocusCallback(onFocus);
    save::load(app.save);
    setLang((Lang)app.save.lang);
    challenges::refresh(app.save);
    app.assets.load();
    app.ui.attach(&app.assets, &app.input);
    app.applyAudio();
    platform::initAudioFocus();
    syncMusic(true);
    app.screen = app.save.storySeen ? Screen::Menu : Screen::Story;

#if !defined(PLATFORM_ANDROID) && !defined(PLATFORM_IOS)
    if (g_host->test.on) SetTargetFPS(0);
#endif
}

static void shutdownGame()
{
    if (!g_host) return;
    App &app = g_host->app;
    save::write(app.save);
    app.session.reset();
    app.assets.unload();
    platform::shutdown();
    g_app = nullptr;
    CloseWindow();
    delete g_host;
    g_host = nullptr;
}

static void frameGame()
{
    App &app = g_host->app;
    float dt = std::min(GetFrameTime(), 0.05f);
#if !defined(PLATFORM_ANDROID)
    if (g_host->test.on) { dt = 1.0f / 60.0f; g_host->test.before(app); }
#endif
    app.time += dt;
    app.screenTime += dt;
    app.input.update();
    if (app.adSlot != AdSlot::None) {
        app.adTime += dt;
        bool close = app.adTime > 2.4f || (app.adTime > 1.2f && !app.input.ended().empty());
        app.input.consumeAll();
        if (close) app.finishAd();
    }
    app.ui.beginFrame();
    g_musicPoll += dt;
    if (g_musicPoll >= 0.4f) { g_musicPoll = 0.0f; syncMusic(false); }
    if (app.assets.audioReady && g_musicOn) UpdateMusicStream(app.assets.music);

    if (app.screen == Screen::Game && app.session) app.session->update(dt);
    if (app.screen == Screen::Game && app.session && app.session->done()) app.finishShift();

    app.assets.syncLooks(app.save);
    BeginDrawing();
    if (app.screen == Screen::Game && app.session) {
        app.session->draw();
    } else {
        ClearBackground(pal::bg);
        app.ui.begin();
        switch (app.screen) {
        case Screen::Story:      screenStory(app); break;
        case Screen::Menu:       screenMenu(app); break;
        case Screen::Shop:       screenShop(app); break;
        case Screen::Settings:   screenSettings(app); break;
        case Screen::Results:    screenResults(app); break;
        case Screen::Challenges: screenChallenges(app); break;
        case Screen::Customers:  screenCustomers(app); break;
        default:                 app.go(Screen::Menu); break;
        }
        drawNotify(app);
        app.ui.end();
    }
#if !defined(PLATFORM_ANDROID)
    if (g_host->test.on) g_host->test.after(app);
#endif
    if (app.adSlot != AdSlot::None) {
        app.ui.begin();
        drawAdOverlay(app);
        app.ui.end();
    }
    EndDrawing();
}

#if defined(PLATFORM_IOS)
extern "C" void ubear_ios_init(void) { bootGame(0, nullptr); }
extern "C" int ubear_ios_frame(void)
{
    if (!g_host) return 0;
    if (g_host->app.quit || WindowShouldClose()) { shutdownGame(); return 0; }
    frameGame();
    if (!g_host || g_host->app.quit || WindowShouldClose()) { shutdownGame(); return 0; }
    return 1;
}
#endif

int main(int argc, char **argv)
{
#if defined(PLATFORM_IOS)
    return ios_boot(argc, argv);
#else
    bootGame(argc, argv);
    while (g_host && !WindowShouldClose() && !g_host->app.quit) frameGame();
    shutdownGame();
    return 0;
#endif
}
