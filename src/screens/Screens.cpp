#include "Screens.hpp"
#include "../core/Config.hpp"
#include "../core/Locale.hpp"
#include "../game/Bear.hpp"
#include "../game/Catalog.hpp"
#include "../game/Challenges.hpp"
#include "../game/Customers.hpp"
#include "../game/Days.hpp"

#include <raymath.h>
#include <algorithm>
#include <cmath>
#include <string>

// ---------------------------------------------------------------------------
// Éléments communs
// ---------------------------------------------------------------------------

static void background(App& app, bool ice = true)
{
    Ui& ui = app.ui;
    DrawRectangleGradientV(0, 0, (int)ui.W + 1, (int)ui.H + 1, Color{40, 96, 160, 255}, Color{150, 200, 240, 255});
    if (ice) {
        DrawRectangle(0, (int)(ui.H * 0.78f), (int)ui.W + 1, (int)(ui.H * 0.22f) + 1, Color{232, 244, 255, 255});
        DrawEllipse((int)(ui.W * 0.25f), (int)(ui.H * 0.78f), ui.W * 0.5f, 40, Color{232, 244, 255, 255});
        DrawEllipse((int)(ui.W * 0.8f), (int)(ui.H * 0.785f), ui.W * 0.4f, 30, Color{232, 244, 255, 255});
    }
    for (int i = 0; i < 70; i++) {
        float sx = std::fmod(i * 97.13f + std::sin(app.time * 0.7f + i) * 20.0f, ui.W);
        float sy = std::fmod(i * 53.71f + app.time * (40.0f + (i % 5) * 18.0f), ui.H);
        DrawCircleV({sx, sy}, 2.0f + (i % 3), Fade(WHITE, 0.75f));
    }
}

static bool backButton(App& app)
{
    Ui& ui = app.ui;
    Rectangle r = {20, cfg::SAFE_TOP, 150, 76};
    bool hit = ui.button(r, tr(Str::Back), Color{40, 60, 100, 230}, 32);
    return hit || app.input.backPressed();
}

static void coinsChip(App& app)
{
    Ui& ui = app.ui;
    const char* t = TextFormat("%d", app.save.coins);
    float w = ui.measure(t, 40).x + 90;
    Rectangle r = {ui.W - 20 - w, cfg::SAFE_TOP + 4, w, 68};
    ui.panel(r, pal::dim, 0.6f);
    ui.coinIcon({r.x + 36, r.y + 34}, 20);
    ui.text(t, r.x + 66, r.y + 14, 40, WHITE);
}

static void title(App& app, const char* t, float y)
{
    app.ui.textShadow(t, app.ui.W * 0.5f, y, 60, WHITE, Align::Center);
}

// Zone défilante verticale ; renvoie le décalage à appliquer.
static float scrollArea(App& app, Rectangle area, float contentH)
{
    static float lastY = -1.0f;
    static float vel = 0.0f;
    Vector2 p;
    if (app.input.dragPos(app.ui.toScreen(area), p)) {
        float y = p.y / app.ui.scale;
        if (lastY >= 0) { vel = y - lastY; app.shopScroll -= vel; }
        lastY = y;
    } else {
        lastY = -1.0f;
        app.shopScroll -= vel;
        vel *= 0.9f;
    }
    app.shopScroll -= GetMouseWheelMove() * 80.0f;
    float maxS = std::max(0.0f, contentH - area.height);
    if (app.shopScroll < 0) { app.shopScroll = 0; vel = 0; }
    if (app.shopScroll > maxS) { app.shopScroll = maxS; vel = 0; }
    return app.shopScroll;
}

static bool visibleIn(Rectangle r, Rectangle area)
{
    return r.y >= area.y - 4 && r.y + r.height <= area.y + area.height + 4;
}

void drawNotify(App& app)
{
    if (app.toastTime <= 0.0f) return;
    Ui& ui = app.ui;
    app.toastTime -= GetFrameTime();
    float a = std::min(1.0f, app.toastTime / 0.3f);
    Rectangle box = {50, ui.H - 210, ui.W - 100, 0};
    float h = ui.textBox(app.toastText.c_str(), {box.x + 24, box.y + 18, box.width - 48, 200}, 30, WHITE, Align::Center, false);
    ui.panel({box.x, box.y, box.width, h + 36}, Fade(Color{20, 30, 50, 255}, 0.9f * a), 0.4f);
    ui.textBox(app.toastText.c_str(), {box.x + 24, box.y + 18, box.width - 48, 200}, 30, Fade(WHITE, a), Align::Center);
}

void drawAdOverlay(App& app)
{
    Ui& ui = app.ui;
    DrawRectangle(0, 0, (int)ui.W + 1, (int)ui.H + 1, Color{8, 14, 28, 235});
    ui.text(L("PUBLICITÉ", "AD"), ui.W * 0.5f, ui.H * 0.38f, 64, WHITE, Align::Center);
    const char* sub = app.adSlot == AdSlot::Energy ? L("5 journées pour Barnabé", "5 days for Barnaby")
                     : app.adSlot == AdSlot::Gold ? L("Un petit tas de pièces", "A small pile of coins")
                     : L("Fin de journée", "End of the day");
    ui.text(sub, ui.W * 0.5f, ui.H * 0.38f + 80, 32, Color{200, 220, 240, 255}, Align::Center);
    if (app.adTime > 1.2f)
        ui.text(L("Touche pour fermer", "Tap to close"), ui.W * 0.5f, ui.H * 0.62f, 30, pal::gold, Align::Center);
}

// ---------------------------------------------------------------------------
// Histoire (scénario)
// ---------------------------------------------------------------------------

struct StoryPage { const char* fr; const char* en; int art; };

static const StoryPage STORY[] = {
    {"Sur la banquise de Glagla-sur-Mer, la seule application de livraison qui fonctionne par -40 °C s'appelle Ubear.",
     "On the ice floe of Glagla-sur-Mer, the only delivery app that works at -40 °C is called Ubear.", 0},
    {"Son unique coursier : Barnabé. Ours polaire, 8 ans, 450 kg, quatre rollers « empruntés » au vestiaire de la patinoire.",
     "Its only courier: Barnaby. Polar bear, 8 years old, 450 kg, four skates \"borrowed\" from the ice rink locker room.", 1},
    {"Problème : Barnabé n'a jamais appris à freiner. Chaque coin de l'écran est une patte. Chaque poussée propulse ET fait tourner.",
     "Problem: Barnaby never learned to brake. Each corner of the screen is a paw. Each push propels AND turns.", 2},
    {"La météo est caractérielle. Le vent pousse, le brouillard aveugle, la grêle ralentit. Et parfois, c'est le blizzard.",
     "The weather has a temper. Wind pushes, fog blinds, hail slows you down. And sometimes, a blizzard hits.", 3},
    {"Mais la nuit, quand l'aurore boréale s'allume, les clients deviennent très généreux.",
     "But at night, when the northern lights glow, customers get very generous.", 4},
    {"Les habitués l'attendent : Pénélope, Gérard, Roxane, les jumeaux… et un yéti qui ne laisse jamais de pourboire.",
     "The regulars are waiting: Penelope, Gerard, Roxane, the twins… and a yeti who never tips.", 5},
    {"Les pourboires financent son rêve chez Gaston, le morse grognon : de vrais freins, un GPS, un sac isotherme… et une couronne.",
     "Tips fund his dream at grumpy Gaston the walrus's shop: real brakes, a GPS, a thermal bag… and a crown.", 6},
    {"« Livré chaud, ou c'est l'ours qui paie. »", "\"Delivered hot, or the bear pays.\"", 7},
};
static const int STORY_COUNT = sizeof(STORY) / sizeof(STORY[0]);

static void storyArt(App& app, int art, Vector2 c)
{
    Assets& a = app.assets;
    float t = app.time;
    switch (art) {
    case 0:
    case 7: {
        float s = 300.0f + 10.0f * std::sin(t * 2.0f);
        DrawTexturePro(a.logo, {0, 0, (float)a.logo.width, (float)a.logo.height}, {c.x, c.y, s, s}, {s * 0.5f, s * 0.5f}, 0, WHITE);
        break;
    }
    case 1:
        drawBear(a, app.save, c, std::sin(t) * 12.0f, 0.5f);
        break;
    case 2: {
        float ang = std::sin(t * 1.3f) * 30.0f;
        drawBear(a, app.save, c, ang, 0.38f);
        const char* lab[4] = {"-60°", "+60°", "-40°", "+40°"};
        Vector2 pos[4] = {{c.x - 230, c.y - 230}, {c.x + 230, c.y - 230}, {c.x - 230, c.y + 230}, {c.x + 230, c.y + 230}};
        for (int i = 0; i < 4; i++) {
            float pulse = std::fmod(t * 1.5f, 4.0f);
            bool on = (int)pulse == i;
            DrawCircleV(pos[i], on ? 66 : 58, on ? pal::gold : Fade(WHITE, 0.85f));
            app.ui.text(lab[i], pos[i].x, pos[i].y - 18, 36, pal::ink, Align::Center);
        }
        break;
    }
    case 3: {
        DrawCircleV(c, 230, Fade(WHITE, 0.25f));
        for (int i = 0; i < 3; i++) {
            float x = std::fmod(t * 300.0f + i * 160.0f, 520.0f) - 260.0f;
            DrawTexturePro(a.wind, {0, 0, (float)a.wind.width, (float)a.wind.height}, {c.x + x, c.y - 120 + i * 90.0f, 300, 95}, {150, 47}, 0, Fade(WHITE, 0.8f));
        }
        DrawTexturePro(a.blizzard, {0, 0, 256, 256}, {c.x - 230, c.y - 230 + std::fmod(t * 80.0f, 40.0f), 460, 460}, {0, 0}, 0, WHITE);
        drawBear(a, app.save, c, std::sin(t * 5.0f) * 25.0f, 0.3f);
        break;
    }
    case 4: {
        DrawCircleV(c, 240, Color{10, 20, 50, 255});
        BeginScissorMode((int)app.ui.toScreen(Vector2{c.x - 240, 0}).x, (int)app.ui.toScreen(Vector2{0, c.y - 240}).y,
                         (int)(480 * app.ui.scale), (int)(480 * app.ui.scale));
        BeginBlendMode(BLEND_ADDITIVE);
        for (int k = 0; k < 3; k++) {
            Color col = k == 0 ? Color{60, 255, 150, 255} : (k == 1 ? Color{70, 220, 255, 255} : Color{190, 100, 255, 255});
            for (float x = -240; x < 240; x += 8) {
                float y = c.y - 60 + k * 40 + std::sin(x * 0.02f + t + k) * 30;
                DrawRectangleGradientV((int)(c.x + x), (int)(y - 120), 9, 120, Fade(col, 0), Fade(col, 0.5f));
            }
        }
        EndBlendMode();
        EndScissorMode();
        DrawRing(c, 236, 250, 0, 360, 64, Color{150, 200, 240, 255});
        app.ui.coinIcon({c.x - 60, c.y + 130}, 40);
        app.ui.textShadow("x2", c.x + 10, c.y + 100, 64, pal::gold);
        break;
    }
    case 5: {
        for (int i = 0; i < CUSTOMER_COUNT; i++) {
            float ang = -PI / 2 + i * 2 * PI / CUSTOMER_COUNT + t * 0.2f;
            customers::drawPortrait(i, {c.x + std::cos(ang) * 170, c.y + std::sin(ang) * 170}, 70, t);
        }
        drawBear(a, app.save, c, t * 30.0f, 0.18f);
        break;
    }
    case 6:
        customers::drawPortrait(PORTRAIT_GASTON, c, 170, t);
        break;
    default: break;
    }
}

void screenStory(App& app)
{
    Ui& ui = app.ui;
    background(app, false);
    int page = std::clamp(app.storyPage, 0, STORY_COUNT - 1);
    const StoryPage& p = STORY[page];
    float artY = cfg::SAFE_TOP + 120 + (ui.H - 1100) * 0.25f + 260;
    storyArt(app, p.art, {ui.W * 0.5f, artY});

    Rectangle box = {40, artY + 320, ui.W - 80, 0};
    float h = ui.textBox(L(p.fr, p.en), {box.x + 30, box.y + 30, box.width - 60, 400}, 36, pal::ink, Align::Center, false);
    ui.panel({box.x, box.y, box.width, h + 60}, pal::panel, 0.2f);
    ui.textBox(L(p.fr, p.en), {box.x + 30, box.y + 30, box.width - 60, 400}, 36, pal::ink, Align::Center);

    for (int i = 0; i < STORY_COUNT; i++)
        DrawCircleV({ui.W * 0.5f + (i - (STORY_COUNT - 1) * 0.5f) * 30, ui.H - 70}, i == page ? 9.0f : 6.0f, Fade(WHITE, i == page ? 1.0f : 0.5f));

    bool last = page == STORY_COUNT - 1;
    auto finish = [&]() {
        app.save.storySeen = true;
        app.persist();
        app.go(app.storyReturn);
    };
    if (ui.button({ui.W - 200, cfg::SAFE_TOP, 180, 76}, tr(Str::TutoSkip), Color{40, 60, 100, 230}, 32)) { finish(); return; }
    if (last) {
        if (ui.button({100, ui.H - 210, ui.W - 200, 104}, tr(Str::Play), pal::red, 48)) finish();
    } else {
        float blink = 0.5f + 0.5f * std::sin(app.time * 4.0f);
        ui.text(tr(Str::TapToContinue), ui.W * 0.5f, ui.H - 150, 32, Fade(WHITE, 0.5f + 0.5f * blink), Align::Center);
        if (ui.tapped({0, cfg::SAFE_TOP + 90, ui.W, ui.H})) app.storyPage++;
    }
    if (app.input.backPressed()) finish();
}

// ---------------------------------------------------------------------------
// Menu principal
// ---------------------------------------------------------------------------

void screenMenu(App& app)
{
    Ui& ui = app.ui;
    background(app);
    coinsChip(app);
    vitality::refresh(app.save);
    if (rewards::ready(app.save)) {
        Rectangle g = {20, cfg::SAFE_TOP + 8, 250, 68};
        int gift = rewards::peek(app.save);
        if (ui.button(g, TextFormat(L("Cadeau +%d", "Gift +%d"), gift), pal::gold, 28)) {
            int got = rewards::claim(app.save);
            app.persist();
            app.assets.playSfx(app.assets.coin);
            app.notify(TextFormat(L("Jour %d : +%d pièces. À demain !", "Day %d: +%d coins. See you tomorrow!"),
                                  app.save.rewardStreak, got));
        }
    }

    float logoY = cfg::SAFE_TOP + (ui.H < 1250 ? 30 : 100);
    float s = ui.H < 1250 ? 170.0f : 230.0f;
    DrawTexturePro(app.assets.logo, {0, 0, (float)app.assets.logo.width, (float)app.assets.logo.height},
                   {ui.W * 0.5f, logoY + s * 0.5f, s, s}, {s * 0.5f, s * 0.5f}, std::sin(app.time * 1.5f) * 3.0f, WHITE);
    ui.textShadow(tr(Str::Tagline), ui.W * 0.5f, logoY + s + 18, 30, WHITE, Align::Center);

    float bh = 92, gap = 16;
    float by = ui.H - 40 - 70 - 5 * (bh + gap);
    float midY = (logoY + s + 60 + by) * 0.5f;
    float t = app.time * 0.9f;
    float bearY = std::min(midY + std::sin(t * 2.0f) * 18.0f, by - 168.0f);
    Vector2 bp = {ui.W * 0.5f + std::sin(t) * 150.0f, bearY};
    drawBear(app.assets, app.save, bp, std::cos(t) * 40.0f, ui.H < 1250 ? 0.14f : 0.19f);

    int jour = cfg::dayIndex(app.save.day);
    const MoodFx& mood = moodFor(jour);
    int secs = vitality::secondsToNext(app.save);
    int eh = secs / 3600, em = (secs % 3600) / 60;
    const char* energyLine = secs > 0
        ? TextFormat(L("Énergie %d/%d    +1 dans %dh%02d", "Energy %d/%d    +1 in %dh%02d"), app.save.energy, cfg::ENERGY_MAX, eh, em)
        : TextFormat(L("Énergie %d/%d", "Energy %d/%d"), app.save.energy, cfg::ENERGY_MAX);
    ui.text(energyLine, ui.W * 0.5f, by - 112, 26, app.save.energy > 0 ? WHITE : Color{255, 170, 170, 255}, Align::Center);
    float pipY = by - 78;
    float pip0 = ui.W * 0.5f - (cfg::ENERGY_MAX - 1) * 18.0f;
    for (int i = 0; i < cfg::ENERGY_MAX; i++) {
        DrawCircleV({pip0 + i * 36.0f, pipY}, 11, i < app.save.energy ? pal::gold : Color{40, 60, 90, 180});
    }
    ui.text(TextFormat(L("Jour %d · %d commandes · %s", "Day %d · %d orders · %s"), jour, cfg::dayQuota(app.save.day), L(mood.nameFr, mood.nameEn)),
            ui.W * 0.5f, by - 46, 26, WHITE, Align::Center);
    offers::sync(app.save);
    bool awake = app.save.energy > 0;
    int adsLeft = offers::energyAdsLeft(app.save);
    if (!awake && adsLeft > 0 && ui.button({80, by - 168, ui.W - 160, 64},
            TextFormat(L("Pub : +5 journées (%d/3)", "Ad: +5 days (%d/3)"), cfg::ENERGY_AD_MAX - adsLeft + 1),
            pal::gold, 26)) {
        app.beginAd(AdSlot::Energy);
    }
    Rectangle r = {80, by, ui.W - 160, bh};
    if (ui.button(r, awake ? tr(Str::Play) : L("Barnabé dort", "Barnaby is asleep"), awake ? pal::red : pal::grey, 44, awake)) {
        app.startShift();
        return;
    }
    r.y += bh + gap;
    challenges::refresh(app.save);
    int done = challenges::doneCount(app.save);
    if (ui.button(r, TextFormat("%s  %d/3", L("Défis du jour", "Daily challenges"), done), Color{230, 150, 30, 255}, 38)) app.go(Screen::Challenges);
    r.y += bh + gap;
    if (ui.button(r, L("Carnet de clients", "Customer book"), pal::purple, 38)) app.go(Screen::Customers);
    r.y += bh + gap;
    if (ui.button(r, tr(Str::Shop), pal::green, 38)) app.go(Screen::Shop);
    r.y += bh + gap;
    if (ui.button(r, tr(Str::Settings), pal::blue, 38)) app.go(Screen::Settings);

    Rectangle hist = {ui.W * 0.5f - 140, ui.H - 96, 280, 60};
    ui.text(L("Revoir l'histoire", "Replay the story"), ui.W * 0.5f, hist.y + 14, 30, Fade(pal::ink, 0.8f), Align::Center);
    if (ui.tapped(hist)) { app.storyPage = 0; app.storyReturn = Screen::Menu; app.go(Screen::Story); }

    if (app.input.backPressed()) app.quit = true;
}

// ---------------------------------------------------------------------------
// Défis quotidiens
// ---------------------------------------------------------------------------

void screenChallenges(App& app)
{
    Ui& ui = app.ui;
    challenges::refresh(app.save);
    background(app);
    if (backButton(app)) { app.go(Screen::Menu); return; }
    coinsChip(app);
    title(app, L("Défis du jour", "Daily challenges"), cfg::SAFE_TOP + 110);

    int secs = challenges::secondsUntilReset();
    ui.textShadow(TextFormat(L("Nouveaux défis dans %dh%02d", "New challenges in %dh%02d"), secs / 3600, (secs / 60) % 60),
                  ui.W * 0.5f, cfg::SAFE_TOP + 184, 30, WHITE, Align::Center);

    float y = cfg::SAFE_TOP + 250;
    static const Color tierCol[3] = {{66, 176, 104, 255}, {230, 150, 30, 255}, {210, 60, 70, 255}};
    for (int i = 0; i < CHALLENGE_SLOTS; i++) {
        ChallengeDef c = challenges::get(app.save, i);
        bool done = app.save.chalDone[i];
        Rectangle card = {40, y, ui.W - 80, 196};
        ui.panel(card, done ? Color{230, 250, 236, 245} : pal::panel, 0.18f);
        DrawRectangleRounded({card.x, card.y, card.width, 46}, 0.5f, 6, tierCol[i]);
        ui.text(challenges::tierName(i), card.x + 24, card.y + 8, 30, WHITE);
        ui.coinIcon({card.x + card.width - 120, card.y + 23}, 15);
        ui.text(TextFormat("+%d", c.reward), card.x + card.width - 96, card.y + 8, 30, WHITE);
        std::string d = challenges::describe(c);
        ui.textBox(d.c_str(), {card.x + 24, card.y + 62, card.width - 48, 80}, 34, pal::ink);
        float prog = (float)app.save.chalProg[i] / c.target;
        ui.bar({card.x + 24, card.y + 146, card.width - 200, 26}, prog, done ? pal::green : pal::gold, Color{220, 228, 240, 255});
        ui.text(done ? L("Accompli !", "Done!") : TextFormat("%d / %d", app.save.chalProg[i], c.target),
                card.x + card.width - 24, card.y + 142, 30, done ? pal::green : pal::soft, Align::Right);
        y += 216;
    }

    Rectangle bonus = {40, y, ui.W - 80, 110};
    ui.panel(bonus, app.save.chalBonus ? Color{255, 244, 210, 245} : Color{255, 255, 255, 200}, 0.25f);
    customers::drawPortrait(PORTRAIT_GASTON, {bonus.x + 60, bonus.y + 55}, 40, app.time);
    ui.textBox(app.save.chalBonus ? L("Les 3 défis sont faits. Gaston a payé. Il boude.", "All 3 done. Gaston paid. He's sulking.")
                                  : TextFormat(L("Les 3 défis du jour : bonus de %d pièces.", "All 3 daily challenges: %d coin bonus."), cfg::CHALLENGE_ALL_BONUS),
               {bonus.x + 120, bonus.y + 18, bonus.width - 140, 90}, 30, pal::ink);
    y += 130;
    ui.textShadow(TextFormat(L("Défis accomplis au total : %d", "Challenges completed: %d"), app.save.chalTotalDone),
                  ui.W * 0.5f, y + 10, 28, WHITE, Align::Center);

    if (ui.H >= 1250 && ui.button({80, ui.H - 150, ui.W - 160, 96}, tr(Str::Play), pal::red, 44)) app.startShift();
}

// ---------------------------------------------------------------------------
// Carnet de clients
// ---------------------------------------------------------------------------

void screenCustomers(App& app)
{
    Ui& ui = app.ui;
    background(app);
    if (backButton(app)) { app.go(Screen::Menu); return; }
    title(app, L("Carnet de clients", "Customer book"), cfg::SAFE_TOP + 110);

    Rectangle area = {0, cfg::SAFE_TOP + 190, ui.W, ui.H - cfg::SAFE_TOP - 210};
    const float cardH = 250, gap = 20;
    float off = scrollArea(app, area, CUSTOMER_COUNT * (cardH + gap));
    Rectangle sc = ui.toScreen(area);
    BeginScissorMode((int)sc.x, (int)sc.y, (int)sc.width, (int)sc.height);
    for (int i = 0; i < CUSTOMER_COUNT; i++) {
        int n = app.save.custCount[i];
        int tr_ = customers::tier(n);
        Rectangle card = {30, area.y + i * (cardH + gap) - off, ui.W - 60, cardH};
        ui.panel(card, pal::panel, 0.12f);
        customers::drawPortrait(i, {card.x + 82, card.y + 90}, 62, app.time);
        ui.text(customers::name(i), card.x + 166, card.y + 20, 40, shade(customers::def(i).color, 0.8f));
        ui.text(customers::tierName(tr_), card.x + card.width - 24, card.y + 28, 28, pal::purple, Align::Right);
        ui.textBox(customers::desc(i), {card.x + 166, card.y + 70, card.width - 190, 80}, 25, pal::soft);

        int next = tr_ + 1 < customers::TIER_COUNT ? customers::tierThreshold(tr_ + 1) : -1;
        float prog = next > 0 ? (float)(n - customers::tierThreshold(tr_)) / (next - customers::tierThreshold(tr_)) : 1.0f;
        ui.bar({card.x + 24, card.y + 166, card.width - 220, 20}, prog, customers::def(i).color, Color{220, 228, 240, 255});
        ui.text(next > 0 ? TextFormat("%d / %d", n, next) : TextFormat("%d", n), card.x + card.width - 24, card.y + 160, 28, pal::soft, Align::Right);

        std::string quote;
        if (n == 0) quote = L("Pas encore livré. Il paraît que c'est quelqu'un.", "Not delivered yet. Supposedly quite a character.");
        else if (i == CU_YETI && n < cfg::YETI_PATIENCE)
            quote = TextFormat(L("Objectif long : %d/%d. Plus que %d, puis +%d pièces.",
                                  "Long goal: %d/%d. %d to go, then +%d coins."),
                                n, cfg::YETI_PATIENCE, cfg::YETI_PATIENCE - n, cfg::YETI_BIG_TIP);
        else if (i == CU_YETI)
            quote = L("Le yéti te connaît. Il grogne un peu moins.", "The yeti knows you. He grunts a little less.");
        else quote = TextFormat("« %s »", customers::thanksLine(i, tr_));
        ui.textBox(quote.c_str(), {card.x + 24, card.y + 196, card.width - 48, 50}, 25, pal::ink);
    }
    EndScissorMode();
}

// ---------------------------------------------------------------------------
// Boutique de Gaston
// ---------------------------------------------------------------------------

static const Str CAT_NAMES[COS_COUNT] = {Str::CatSkin, Str::CatHat, Str::CatBag, Str::CatSkate, Str::CatTrail};

static void drawSheet(const Texture2D& tex, Rectangle src, Rectangle dst)
{
    if (tex.id == 0 || src.width < 1.0f || src.height < 1.0f) return;
    DrawTexturePro(tex, src, dst, {dst.width * 0.5f, dst.height * 0.5f}, 0, WHITE);
}

static Rectangle fit(Vector2 c, Rectangle src, float size)
{
    float k = size / src.height;
    return {c.x, c.y, src.width * k, src.height * k};
}

static void drawCosmeticIcon(App& app, const CosmeticDef& d, Vector2 c, float size)
{
    if (d.cat == COS_TRAIL) {
        if (std::string(d.id) == "trail_none") { DrawRing(c, size * 0.3f, size * 0.36f, 0, 360, 32, pal::grey); return; }
        bool snow = std::string(d.id) == "trail_snow";
        for (int i = 0; i < 6; i++) {
            Color col = std::string(d.id) == "trail_rainbow" ? ColorFromHSV(i * 60.0f + app.time * 90.0f, 0.7f, 1.0f) : d.swatch;
            float x = c.x - size * 0.35f + i * size * 0.14f;
            float y = c.y + std::sin(app.time * 3 + i) * size * 0.08f;
            float r = size * (0.045f + i * 0.013f);
            if (snow) {
                DrawCircleV({x, y}, r + size * 0.018f, Color{28, 58, 98, 255});
                DrawCircleV({x, y}, r * 0.62f, Color{248, 252, 255, 255});
            } else if (std::string(d.id) == "trail_bubbles") {
                DrawRing({x, y}, r * 0.7f, r, 0, 360, 12, col);
            } else {
                DrawCircleV({x, y}, r, col);
            }
        }
        return;
    }

    DrawCircleV(c, size * 0.48f, Color{176, 208, 232, 255});

    if (!d.file) {
        if (d.cat == COS_HAT || d.cat == COS_BAG || d.cat == COS_SKATE) {
            DrawRing(c, size * 0.22f, size * 0.32f, 0, 360, 24, pal::grey);
        }
        return;
    }

    std::string key = (d.cat == COS_SKIN) ? std::string("fur|") + d.file : std::string("icon|") + d.file;
    const Texture2D* icon = app.assets.look(key);
    if (!icon) icon = app.assets.cosmetic(d.file);   // calque brut si l'icône pré-calculée manque
    if (!icon) icon = &app.assets.body;
    Rectangle src = {0, 0, (float)icon->width, (float)icon->height};
    drawSheet(*icon, src, fit(c, src, size));
}

void screenShop(App& app)
{
    Ui& ui = app.ui;
    SaveData& s = app.save;
    background(app, false);
    if (backButton(app)) { app.go(Screen::Menu); return; }
    coinsChip(app);
    title(app, tr(Str::Shop), cfg::SAFE_TOP + 100);

    // Gaston
    Rectangle gp = {30, cfg::SAFE_TOP + 172, ui.W - 60, 120};
    ui.panel(gp, pal::panel, 0.25f);
    customers::drawPortrait(PORTRAIT_GASTON, {gp.x + 62, gp.y + 60}, 46, app.time);
    std::string quote;
    if (const CosmeticDef* d = findCosmetic(app.shopSelected)) {
        std::string bonus = bonusLine(d->bonus);
        if (bonus.empty()) quote = TextFormat("« %s »", gastonIdle((int)(d - COSMETICS)));
        else quote = std::string("« ") + bonus + " »";
    }
    else if (app.shopSelected.rfind("up", 0) == 0) quote = TextFormat("« %s »", gastonQuote((Upgrade)(app.shopSelected[2] - '0')));
    else quote = TextFormat("« %s »", gastonIdle(app.gastonLine));
    ui.text("Gaston", gp.x + 124, gp.y + 12, 28, Color{40, 120, 80, 255});
    ui.textBox(quote.c_str(), {gp.x + 124, gp.y + 46, gp.width - 144, 70}, 26, pal::ink);

    // onglets
    float tw = (ui.W - 60 - 20) / 3.0f;
    Rectangle t0 = {30, gp.y + gp.height + 20, tw, 70};
    Rectangle t1 = {t0.x + tw + 10, t0.y, tw, 70};
    Rectangle t2 = {t1.x + tw + 10, t0.y, tw, 70};
    if (ui.button(t0, tr(Str::Equipment), app.shopTab == 0 ? pal::blue : pal::grey, 24)) { app.shopTab = 0; app.shopScroll = 0; }
    if (ui.button(t1, tr(Str::Cosmetics), app.shopTab == 1 ? pal::blue : pal::grey, 24)) { app.shopTab = 1; app.shopScroll = 0; }
    if (ui.button(t2, L("Offres", "Offers"), app.shopTab == 2 ? pal::gold : pal::grey, 24)) { app.shopTab = 2; app.shopScroll = 0; app.confirmOffer = -1; }

    float top = t0.y + t0.height + 24;

    if (app.shopTab == 0) {
        Rectangle area = {0, top, ui.W, ui.H - top - 20};
        const float cardH = 196, gap = 18;
        float off = scrollArea(app, area, UP_COUNT * (cardH + gap));
        Rectangle sc = ui.toScreen(area);
        BeginScissorMode((int)sc.x, (int)sc.y, (int)sc.width, (int)sc.height);
        for (int i = 0; i < UP_COUNT; i++) {
            const UpgradeDef& u = UPGRADES[i];
            int lvl = s.upgrades[i];
            Rectangle card = {30, top + i * (cardH + gap) - off, ui.W - 60, cardH};
            ui.panel(card, pal::panel, 0.15f);
            ui.text(tr(u.name), card.x + 24, card.y + 18, 38, pal::ink);
            for (int k = 0; k < 3; k++) {
                Rectangle pip = {card.x + card.width - 150 + k * 44, card.y + 26, 34, 22};
                DrawRectangleRounded(pip, 0.5f, 4, k < lvl ? pal::green : Color{215, 222, 235, 255});
            }
            ui.textBox(tr(u.desc), {card.x + 24, card.y + 66, card.width - 280, 120}, 26, pal::soft);
            Rectangle br = {card.x + card.width - 230, card.y + 96, 206, 80};
            if (lvl >= 3) {
                ui.button(br, tr(Str::Max), pal::grey, 34, false);
            } else {
                int price = u.price[lvl];
                bool can = s.coins >= price;
                if (ui.button(br, TextFormat("%d", price), can ? pal::green : shade(pal::green, 0.7f), 36) && visibleIn(br, area)) {
                    if (can) {
                        s.coins -= price;
                        s.upgrades[i]++;
                        app.shopSelected = TextFormat("up%d", i);
                        app.assets.playSfx(app.assets.coin);
                        app.vibrate(50);
                        app.persist();
                        app.notify(TextFormat("%s %s %d", tr(u.name), tr(Str::Level), s.upgrades[i]));
                    } else {
                        app.notify(tr(Str::NotEnough));
                    }
                }
                ui.coinIcon({br.x + 34, br.y + 40}, 16);
            }
        }
        EndScissorMode();
        return;
    }

    if (app.shopTab == 2) {
        offers::sync(s);
        if (app.confirmOffer >= 0) {
            const StoreOffer& o = offers::store(app.confirmOffer);
            DrawRectangle(0, 0, (int)ui.W + 1, (int)ui.H + 1, Color{6, 12, 28, 180});
            Rectangle p = {50, ui.H * 0.32f, ui.W - 100, 340};
            ui.panel(p, pal::panel, 0.05f);
            ui.text(L(o.nameFr, o.nameEn), ui.W * 0.5f, p.y + 30, 36, pal::ink, Align::Center);
            ui.text(o.price, ui.W * 0.5f, p.y + 90, 48, pal::blue, Align::Center);
            if (ui.button({p.x + 40, p.y + 160, p.width - 80, 70}, L("Confirmer", "Confirm"), pal::green, 32)) {
                if (offers::buy(s, app.confirmOffer)) {
                    app.persist();
                    app.notify(o.removeAds ? L("Pubs retirées.", "Ads removed.") : L("Pièces ajoutées.", "Coins added."));
                }
                app.confirmOffer = -1;
            }
            if (ui.button({p.x + 40, p.y + 244, p.width - 80, 64}, L("Annuler", "Cancel"), pal::grey, 28))
                app.confirmOffer = -1;
            return;
        }
        float y = top;
        auto card = [&](const char* title, const char* sub, const char* action, Color col) -> bool {
            Rectangle r = {30, y, ui.W - 60, 118};
            ui.panel(r, pal::panel, 0.12f);
            ui.text(title, r.x + 20, r.y + 14, 30, pal::ink);
            ui.textBox(sub, {r.x + 20, r.y + 50, r.width - 230, 60}, 22, pal::soft);
            bool hit = ui.button({r.x + r.width - 200, r.y + 28, 180, 64}, action, col, 24);
            y += 132;
            return hit;
        };
        if (s.adsRemoved) card(L("Pubs de fin de journée", "End-of-day ads"), L("Déjà retirées.", "Already removed."), L("Actif", "On"), pal::green);
        else if (card(L("Supprimer les pubs", "Remove ads"), L("Plus de pub forcée en fin de journée.", "No forced ad at the end of the day."), "3,99 €", pal::blue))
            app.confirmOffer = 0;
        int goldLeft = offers::goldAdsLeft(s);
        if (goldLeft > 0 && card(L("Pub récompensée", "Rewarded ad"),
                TextFormat(L("%d pièces. Encore %d aujourd'hui.", "%d coins. %d left today."), cfg::GOLD_AD_GRANT, goldLeft),
                L("Regarder", "Watch"), pal::gold))
            app.beginAd(AdSlot::Gold);
        else if (goldLeft <= 0)
            card(L("Pub récompensée", "Rewarded ad"), L("Reviens demain.", "Come back tomorrow."), L("Fait", "Done"), pal::grey);
        for (int i = 1; i < offers::storeCount(); i++) {
            const StoreOffer& o = offers::store(i);
            if (card(L(o.nameFr, o.nameEn), TextFormat(L("%d pièces", "%d coins"), o.coins), o.price, pal::green))
                app.confirmOffer = i;
        }
        ui.text(L("Le paiement du store sera branché ici. Cet essai crédite le compte.",
                  "Store billing will be connected here. This trial credits the account."),
                ui.W * 0.5f, ui.H - 48, 20, Fade(WHITE, 0.8f), Align::Center);
        return;
    }

    // Aperçu : uniquement le cosmétique de la catégorie, celui sélectionné ou l'équipé.
    float prevY = top + 120;
    SaveData preview = s;
    const char* bare[COS_COUNT] = {"skin_polaire", "hat_none", "bag_original", "skate_original", "trail_none"};
    for (int c = 0; c < COS_COUNT; c++)
        if (c != app.shopCat) preview.equipped[c] = (c == COS_SKIN) ? s.equipped[COS_SKIN] : bare[c];
    if (const CosmeticDef* sel = findCosmetic(app.shopSelected))
        if (sel->cat == app.shopCat) preview.equipped[app.shopCat] = sel->id;
    app.assets.syncShopPreview(preview);
    drawBear(app.assets, preview, {ui.W * 0.5f, prevY}, std::sin(app.time * 0.8f) * 25.0f, 0.27f, nullptr, true, true,
             &app.assets.shopPreview());
    float cy = prevY + 140;
    float cw = (ui.W - 60 - 4 * 10) / 5.0f;
    for (int c = 0; c < COS_COUNT; c++) {
        Rectangle r = {30 + c * (cw + 10), cy, cw, 64};
        if (ui.button(r, tr(CAT_NAMES[c]), app.shopCat == c ? pal::purple : pal::grey, 24)) {
            app.shopCat = c;
            app.shopScroll = 0;
            app.shopSelected = s.equipped[c];
        }
    }
    Rectangle area = {0, cy + 84, ui.W, ui.H - cy - 104};
    std::vector<const CosmeticDef*> items;
    for (int i = 0; i < COSMETIC_COUNT; i++) if (COSMETICS[i].cat == app.shopCat) items.push_back(&COSMETICS[i]);
    const int cols = 3;
    const float gap = 14, w = (ui.W - 60 - gap * (cols - 1)) / cols, h = w * 1.25f;
    int rows = ((int)items.size() + cols - 1) / cols;
    float off = scrollArea(app, area, rows * (h + gap));
    Rectangle sc = ui.toScreen(area);
    BeginScissorMode((int)sc.x, (int)sc.y, (int)sc.width, (int)sc.height);
    for (size_t i = 0; i < items.size(); i++) {
        const CosmeticDef& d = *items[i];
        Rectangle r = {30 + (i % cols) * (w + gap), area.y + (i / cols) * (h + gap) - off, w, h};
        bool owned = s.owned.count(d.id) > 0;
        bool equipped = s.equipped[d.cat] == d.id;
        bool selected = app.shopSelected == d.id;
        ui.panel(r, equipped ? Color{226, 244, 232, 250} : pal::panel, 0.15f);
        if (equipped) DrawRectangleRoundedLinesEx(r, 0.15f, 8, 4, pal::green);
        else if (selected) DrawRectangleRoundedLinesEx(r, 0.15f, 8, 3, pal::blue);
        drawCosmeticIcon(app, d, {r.x + r.width * 0.5f, r.y + r.height * 0.36f}, w * 0.62f);
        ui.textBox(tr(d.name), {r.x + 8, r.y + r.height * 0.58f, r.width - 16, 40}, 20, pal::ink, Align::Center);
        std::string bonus = bonusLine(d.bonus);
        ui.text(bonus.empty() ? L("Aucun bonus", "No bonus") : bonus.c_str(),
                r.x + r.width * 0.5f, r.y + r.height * 0.74f, 16, bonus.empty() ? pal::grey : pal::green, Align::Center);
        const char* state = equipped ? tr(Str::Equipped) : owned ? tr(Str::Equip) : (d.price == 0 ? tr(Str::Free) : TextFormat("%d", d.price));
        Color sc2 = equipped ? pal::green : owned ? pal::blue : (s.coins >= d.price ? pal::gold : pal::grey);
        Rectangle pr = {r.x + 10, r.y + r.height - 48, r.width - 20, 38};
        DrawRectangleRounded(pr, 0.5f, 6, sc2);
        ui.text(state, pr.x + pr.width * 0.5f, pr.y + 7, 24, WHITE, Align::Center);
        if (ui.tapped(r) && visibleIn(r, area)) {
            app.shopSelected = d.id;
            if (owned) {
                s.equipped[d.cat] = d.id;
                app.persist();
            } else if (s.coins >= d.price) {
                s.coins -= d.price;
                s.owned.insert(d.id);
                s.equipped[d.cat] = d.id;
                app.assets.playSfx(app.assets.coin);
                app.vibrate(50);
                app.persist();
            } else {
                app.notify(tr(Str::NotEnough));
            }
        }
    }
    EndScissorMode();
}

// ---------------------------------------------------------------------------
// Options
// ---------------------------------------------------------------------------

void screenSettings(App& app)
{
    Ui& ui = app.ui;
    SaveData& s = app.save;
    background(app);
    if (backButton(app)) { app.persist(); app.go(Screen::Menu); return; }
    title(app, tr(Str::Settings), cfg::SAFE_TOP + 110);

    Rectangle p = {40, cfg::SAFE_TOP + 200, ui.W - 80, 760};
    ui.panel(p, pal::panel, 0.08f);
    float y = p.y + 40;
    ui.text(tr(Str::Music), p.x + 40, y, 34, pal::ink);
    if (ui.slider({p.x + 40, y + 60, p.width - 80, 18}, s.musicVol)) app.applyAudio();
    y += 130;
    ui.text(tr(Str::Sfx), p.x + 40, y, 34, pal::ink);
    if (ui.slider({p.x + 40, y + 60, p.width - 80, 18}, s.sfxVol)) app.applyAudio();
    if (app.input.ended().size() && app.screenTime > 0.2f) {
        for (auto& e : app.input.ended())
            if (CheckCollisionPointRec(e.start, ui.toScreen({p.x, y + 20, p.width, 100}))) app.assets.playSfx(app.assets.coin);
    }
    y += 140;
    const char* vibLabel = s.vibration == 0 ? "OFF" : s.vibration == 2 ? L("Importantes", "Key only") : L("Toutes", "All");
    if (ui.button({p.x + 40, y, p.width - 80, 90}, TextFormat("%s : %s", tr(Str::Vibration), vibLabel),
                  s.vibration ? pal::green : pal::grey, 36)) {
        s.vibration = s.vibration == 1 ? 2 : s.vibration == 2 ? 0 : 1;   // Toutes -> Importantes -> OFF
        app.vibrate(80);
        app.persist();
    }
    y += 110;
    if (ui.button({p.x + 40, y, p.width - 80, 90}, tr(Str::Language), pal::blue, 36)) {
        s.lang = 1 - s.lang;
        setLang((Lang)s.lang);
        app.persist();
    }
    y += 110;
    if (ui.button({p.x + 40, y, p.width - 80, 90}, L("Revoir l'histoire", "Replay the story"), pal::purple, 36)) {
        app.storyPage = 0;
        app.storyReturn = Screen::Settings;
        app.go(Screen::Story);
        return;
    }
    y += 110;
    if (ui.button({p.x + 40, y, p.width - 80, 90}, app.confirmReset ? tr(Str::ConfirmReset) : tr(Str::ResetSave), pal::red, 34)) {
        if (app.confirmReset) {
            save::erase(s);
            challenges::refresh(s);
            app.persist();
            app.confirmReset = false;
            app.notify(tr(Str::SaveErased));
        } else {
            app.confirmReset = true;
        }
    }
    ui.text(TextFormat("UBear v%s", cfg::VERSION), ui.W * 0.5f, ui.H - 70, 26, Fade(pal::ink, 0.6f), Align::Center);
}

// ---------------------------------------------------------------------------
// Bilan du service
// ---------------------------------------------------------------------------

void screenResults(App& app)
{
    Ui& ui = app.ui;
    const ShiftResult& r = app.lastResult;
    background(app);
    title(app, TextFormat(L("Jour %d terminé", "Day %d over"), r.day), cfg::SAFE_TOP + 70);

    Rectangle p = {40, cfg::SAFE_TOP + 150, ui.W - 80, std::min(560.0f, ui.H - cfg::SAFE_TOP - 150 - 460)};
    ui.panel(p, pal::panel, 0.08f);
    ui.text(tr(Str::CoinsEarned), ui.W * 0.5f, p.y + 30, 34, pal::soft, Align::Center);
    ui.coinIcon({ui.W * 0.5f - 90, p.y + 125}, 34);
    float pop = 1.0f + 0.08f * std::max(0.0f, 1.0f - app.screenTime * 2.0f);
    ui.text(TextFormat("+%d", r.coins), ui.W * 0.5f - 40, p.y + 88, 80 * pop, pal::ink);
    if (r.record) ui.text(tr(Str::NewRecord), ui.W * 0.5f, p.y + 180, 38, pal::red, Align::Center);
    else ui.text(TextFormat("%s : %d", tr(Str::Record), app.save.bestShiftCoins), ui.W * 0.5f, p.y + 184, 30, pal::soft, Align::Center);

    int mins = (int)(r.duration / 60), secs = (int)r.duration % 60;
    struct Row { const char* label; std::string value; };
    Row rows[] = {
        {L("Commandes du jour", "Today's orders"), TextFormat("%d / %d", r.deliveries, r.quota)},
        {L("Plats refroidis", "Cold dishes"), TextFormat("%d", r.failed)},
        {tr(Str::BestStreak), TextFormat("%d", r.bestStreak)},
        {L("Habitués servis", "Regulars served"), TextFormat("%d", r.regulars)},
        {L("Sous l'aurore", "Under the aurora"), TextFormat("%d", r.auroraDeliveries)},
        {L("Blizzards survécus", "Blizzards survived"), TextFormat("%d", r.blizzards)},
        {L("Défis accomplis", "Challenges done"), TextFormat("%d", r.challenges)},
        {L("Durée du service", "Shift length"), TextFormat("%d:%02d", mins, secs)},
    };
    float y = p.y + 250;
    float rowH = std::min(64.0f, (p.height - 270) / 8.0f);
    float fs = std::min(32.0f, rowH * 0.62f);
    for (auto& row : rows) {
        ui.text(row.label, p.x + 40, y, fs, pal::ink);
        ui.text(row.value.c_str(), p.x + p.width - 40, y, fs, pal::blue, Align::Right);
        DrawLineEx({p.x + 40, y + rowH * 0.72f}, {p.x + p.width - 40, y + rowH * 0.72f}, 2, Color{225, 230, 240, 255});
        y += rowH;
    }
    const char* quip = r.quotaMet ? L("Toutes les commandes sont livrées. Barnabé peut enfin s'asseoir.",
                                      "Every order made it. Barnaby can finally sit down.")
                     : r.timeUp ? L("La fermeture a sonné avant la fin des commandes. Demain sera plus dur.",
                                    "Closing time came first. Tomorrow will be tougher.")
                     : r.deliveries == 0 ? L("Ubear ne licencie jamais. Il n'a qu'un seul livreur.",
                                             "Ubear never fires anyone. It only has one courier.")
                     : L("Barnabé rentre dormir. Les pièces de la journée sont à lui.",
                         "Barnaby goes to sleep. Today's coins are his.");
    ui.textBox(quip, {40, p.y + p.height + 8, ui.W - 80, 64}, 24, WHITE, Align::Center);

    int tomorrow = app.save.day;
    const MoodFx& mood = moodFor(tomorrow);
    NextBuy buy = nextPurchase(app.save);
    int yeti = std::min(app.save.custCount[CU_YETI], cfg::YETI_PATIENCE);
    Rectangle tom = {40, p.y + p.height + 78, ui.W - 80, 210};
    ui.panel(tom, Color{255, 255, 255, 235}, 0.2f);
    ui.text(TextFormat(L("Demain, jour %d · %s", "Tomorrow, day %d · %s"), tomorrow, L(mood.nameFr, mood.nameEn)),
            tom.x + 20, tom.y + 14, 28, pal::ink);
    ui.textBox(L(mood.blurbFr, mood.blurbEn), {tom.x + 20, tom.y + 50, tom.width - 40, 40}, 22, pal::soft);
    if (!buy.found)
        ui.text(L("Gaston n'a plus rien en rayon.", "Gaston's shelves are empty."), tom.x + 20, tom.y + 96, 24, pal::soft);
    else if (buy.missing > 0)
        ui.text(TextFormat(L("Il manque %d pièces pour %s.", "%d coins short of %s."), buy.missing, buy.name),
                tom.x + 20, tom.y + 96, 24, pal::ink);
    else
        ui.text(TextFormat(L("Tu peux acheter %s (%d).", "You can buy %s (%d)."), buy.name, buy.price),
                tom.x + 20, tom.y + 96, 24, pal::green);
    ui.bar({tom.x + 20, tom.y + 146, tom.width - 40, 18}, (float)yeti / cfg::YETI_PATIENCE, pal::gold, Color{220, 228, 240, 255});
    ui.text(TextFormat(L("Yéti %d/%d", "Yeti %d/%d"), yeti, cfg::YETI_PATIENCE), tom.x + 20, tom.y + 170, 24, pal::ink);

    float by = ui.H - 140;
    if (ui.button({40, by, (ui.W - 100) * 0.5f, 100}, tr(Str::Replay), pal::red, 40)) { app.startShift(); return; }
    if (ui.button({60 + (ui.W - 100) * 0.5f, by, (ui.W - 100) * 0.5f, 100}, tr(Str::Menu), pal::blue, 40) || app.input.backPressed()) app.go(Screen::Menu);
}
