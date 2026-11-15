#include "Session.hpp"
#include "Catalog.hpp"
#include "Customers.hpp"
#include "../core/Config.hpp"
#include "../core/Locale.hpp"

#include <raymath.h>
#include <algorithm>
#include <cmath>

namespace {
constexpr int NUM_RESTAURANTS = 4;
constexpr Color OCEAN = {22, 72, 156, 255};

enum PartKind { PK_PUFF = 0, PK_SNOW, PK_SPARK, PK_RAINBOW, PK_BUBBLE, PK_COIN };

float rnd(float a, float b) { return a + (b - a) * (GetRandomValue(0, 10000) / 10000.0f); }

float wrapDeg(float a)
{
    a = std::fmod(a + 180.0f, 360.0f);
    if (a < 0) a += 360.0f;
    return a - 180.0f;
}

Str strAt(Str base, int k) { return (Str)((int)base + k); }
}

// ---------------------------------------------------------------------------
// Mise en place
// ---------------------------------------------------------------------------

Session::Session(App& app) : app_(app)
{
    world_.generate();
    weather_.generate();
    day_ = cfg::dayIndex(app_.save.day);
    quota_ = cfg::dayQuota(app_.save.day);
    dayLength_ = cfg::dayLength(app_.save.day);
    mood_ = &moodFor(day_);
    bonus_ = equippedBonus(app_.save);
    weather_.setDayClock(dayLength_);
    weather_.setIntensity(cfg::dayWeather(app_.save.day) * mood_->wind);
    if (mood_->nightHours > 0.0f) weather_.shiftHours(mood_->nightHours);
    bear_.pos = world_.randomIcePoint(world_.center(), 0.0f, 350.0f);
    placeBuildings();
    challenges::refresh(app_.save);

    tuto_ = app_.save.tutorialDone ? -1 : 0;
    cooldown_ = tuto_ >= 0 ? 1e9f : 1.2f;
    if (tuto_ < 0) announceDay();

    cam_.target = bear_.pos;
    zoom_ = GetScreenWidth() / cfg::VIEW_WIDTH_WORLD;
}

Session::~Session()
{
    world_.unload();
    weather_.unload();
}

void Session::placeBuildings()
{
    Vector2 c = world_.center();
    auto farEnough = [&](Vector2 p, float minD) {
        if (Vector2Distance(p, bear_.pos) < 650.0f) return false;
        for (auto& b : buildings_) if (Vector2Distance(p, b.pos) < minD) return false;
        return true;
    };
    for (int i = 0; i < NUM_RESTAURANTS + CUSTOMER_COUNT; i++) {
        bool rest = i < NUM_RESTAURANTS;
        Vector2 p{};
        for (int tries = 0; tries < 300; tries++) {
            p = world_.randomIcePoint(c, rest ? 500.0f : 900.0f, rest ? 2300.0f : 2900.0f);
            if (farEnough(p, tries < 200 ? 1000.0f : 600.0f)) break;
        }
        buildings_.push_back({p, rest, rest ? -1 : i - NUM_RESTAURANTS});
    }
}

Vector2 Session::target() const
{
    if (route_.empty()) return bear_.pos;
    if (state_ == State::Pickup) return buildings_[cur().restaurant].pos;
    return cur().home;
}

Session::Order& Session::cur() { return route_[focus_]; }
const Session::Order& Session::cur() const { return route_[focus_]; }

int Session::bagSlots() const { return 1 + app_.save.upgrades[UP_BAG]; }

int Session::nearestIndex(bool carried) const
{
    int best = -1;
    float bd = 1e9f;
    for (int i = 0; i < (int)route_.size(); i++) {
        const Order& o = route_[i];
        if (o.finished || o.carried != carried) continue;
        Vector2 p = carried ? o.home : buildings_[o.restaurant].pos;
        float d = Vector2Distance(bear_.pos, p);
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}

// ---------------------------------------------------------------------------
// Commandes
// ---------------------------------------------------------------------------

Session::Order Session::makeOffer(bool tutorial)
{
    Order o;
    float spread = tutorial ? 1.0f : cfg::daySpread(day_) * (mood_ ? mood_->spread : 1.0f);

    std::vector<int> ok;
    for (int i = 0; i < NUM_RESTAURANTS; i++) {
        float d = Vector2Distance(bear_.pos, buildings_[i].pos);
        if (d >= (tutorial ? 300.0f : cfg::PICKUP_MIN_DIST) && d <= (tutorial ? 1400.0f : cfg::PICKUP_MAX_DIST * spread)) ok.push_back(i);
    }
    if (ok.empty()) {
        int best = 0; float bestD = 1e9f;
        for (int i = 0; i < NUM_RESTAURANTS; i++) {
            float d = std::fabs(Vector2Distance(bear_.pos, buildings_[i].pos) - 1000.0f);
            if (d < bestD) { bestD = d; best = i; }
        }
        ok.push_back(best);
    }
    o.restaurant = ok[GetRandomValue(0, (int)ok.size() - 1)];
    Vector2 rest = buildings_[o.restaurant].pos;

    bool skip[CUSTOMER_COUNT] = {};
    for (int i = 0; i < offerN_; i++) if (offers_[i].customer >= 0) skip[offers_[i].customer] = true;
    for (const Order& r : route_) if (r.customer >= 0) skip[r.customer] = true;

    if (!tutorial && rnd(0, 1) < cfg::REGULAR_CHANCE) {
        float total = 0;
        float w[CUSTOMER_COUNT];
        for (int i = 0; i < CUSTOMER_COUNT; i++) {
            float d = Vector2Distance(rest, buildings_[NUM_RESTAURANTS + i].pos);
            w[i] = (skip[i] || i == lastCustomer_) ? 0.0f : customers::def(i).weight;
            if (d < 600.0f) w[i] *= 0.1f;
            total += w[i];
        }
        if (total > 0.0f) {
            float r = rnd(0, total);
            for (int i = 0; i < CUSTOMER_COUNT; i++) {
                r -= w[i];
                if (r <= 0 && w[i] > 0) { o.customer = i; break; }
            }
        }
    }
    if (o.customer >= 0) {
        o.home = buildings_[NUM_RESTAURANTS + o.customer].pos;
        lastCustomer_ = o.customer;
    } else {
        o.home = world_.randomIcePoint(rest, tutorial ? 800.0f : cfg::DROP_MIN_DIST * (0.9f + 0.1f * spread),
                                       tutorial ? 1200.0f : cfg::DROP_MAX_DIST * spread);
        o.flavor = GetRandomValue(0, 4);
    }
    o.distance = Vector2Distance(rest, o.home);
    return o;
}

void Session::openOffers()
{
    if (tuto_ >= 0) {
        offerN_ = 0;
        route_.clear();
        route_.push_back(makeOffer(true));
        focus_ = 0;
        routeSize_ = 1;
        state_ = State::Pickup;
        const Order& o = cur();
        if (o.customer >= 0) toast(customers::name(o.customer), customers::orderLine(o.customer, customers::tier(app_.save.custCount[o.customer])), o.customer, customers::def(o.customer).color);
        else toast("Ubear", tr(strAt(Str::Order1, o.flavor)), PORTRAIT_UBEAR, pal::blue);
        app_.assets.playSfx(app_.assets.zip, 0.5f, 1.8f);
        return;
    }
    offerN_ = 0;
    for (int i = 0; i < 3; i++) {
        offers_[i] = makeOffer(false);
        offerSel_[i] = false;
        offerN_ = i + 1;
    }
    state_ = State::Choosing;
}

void Session::startRoute()
{
    route_.clear();
    for (int i = 0; i < offerN_; i++) if (offerSel_[i]) route_.push_back(offers_[i]);
    if (route_.empty()) return;
    routeSize_ = (int)route_.size();
    focus_ = nearestIndex(false);
    if (focus_ < 0) focus_ = 0;
    state_ = State::Pickup;
    toast("Ubear", TextFormat(routeSize_ > 1 ? L("Tournée de %d commandes. Le sac est lourd, les plats refroidissent ensemble.",
                                                 "Route of %d orders. The bag is heavy, and they all cool together.")
                                              : L("Une seule commande. Trajet court.", "Just one order. Short trip."),
                              routeSize_),
          PORTRAIT_UBEAR, pal::blue, 3.4f);
    app_.assets.playSfx(app_.assets.zip, 0.5f, 1.8f);
    app_.vibrate(30);
}

void Session::arm(Order& o)
{
    float timeMult = o.customer >= 0 ? customers::def(o.customer).timeMult : 1.0f;
    float heat = (tuto_ >= 0 ? 1.0f : cfg::dayHeat(day_)) * (mood_ ? mood_->heat : 1.0f);
    o.totalTime = (cfg::ORDER_BASE_TIME + o.distance * cfg::ORDER_TIME_PER_PX)
                * cfg::BAG_TIME_MULT[app_.save.upgrades[UP_BAG]] * timeMult * bonus_.heat / heat;
    if (o.totalTime < 8.0f) o.totalTime = 8.0f;
    o.heat = 1.0f;
    o.carried = true;
}

void Session::advanceAfterStop()
{
    int drop = nearestIndex(true);
    if (drop >= 0) { focus_ = drop; state_ = State::Deliver; return; }
    int pick = nearestIndex(false);
    if (pick >= 0) { focus_ = pick; state_ = State::Pickup; return; }
    route_.clear();
    state_ = State::Waiting;
    cooldown_ = cfg::ORDER_COOLDOWN;
    if (tuto_ == 3) finishTutorial();
    if (!app_.autopilot && tuto_ < 0 && res_.deliveries >= quota_) dayDoneDelay_ = 1.6f;
}

void Session::pickup()
{
    int rest = cur().restaurant;
    int n = 0;
    for (Order& o : route_) {
        if (!o.finished && !o.carried && o.restaurant == rest) { arm(o); n++; }
    }
    app_.assets.playSfx(app_.assets.zip);
    app_.vibrate(40);
    burst(buildings_[rest].pos, Color{255, 220, 120, 255}, 14, 300.0f, PK_SPARK);
    if (tuto_ == 2) tuto_ = 3;
    if (nearestIndex(false) >= 0) {
        focus_ = nearestIndex(false);
        state_ = State::Pickup;
        alert(L("Prochain restaurant", "Next restaurant"), WHITE);
    } else {
        alert(n > 1 ? L("Sac plein. Livre avant que ça refroidisse.", "Bag full. Deliver before it cools.") : tr(Str::PickedUp), WHITE);
        advanceAfterStop();
    }
}

void Session::deliver()
{
    SaveData& s = app_.save;
    int c = cur().customer;
    bool regular = c >= 0;
    int before = regular ? s.custCount[c] : 0;

    float base = cfg::REWARD_BASE + cur().distance * cfg::REWARD_PER_PX;
    float speedMult = regular ? customers::def(c).speedBonusMult : 1.0f;
    float streakScale = mood_ ? mood_->streak : 1.0f;
    float mult = 1.0f + cur().heat * cfg::REWARD_SPEED_BONUS * speedMult;
    mult *= 1.0f + std::min(streak_ * cfg::STREAK_BONUS * streakScale, cfg::STREAK_MAX_BONUS * streakScale);
    if (regular) mult *= customers::tipMult(c, before);
    bool aurora = weather_.auroraActive();
    bool blizzard = weather_.blizzardActive() || cur().blizzard;
    if (aurora) mult *= cfg::AURORA_REWARD_MULT;
    if (weather_.blizzardActive()) mult *= cfg::BLIZZARD_REWARD_MULT;
    mult *= cfg::dayPay(day_) * bonus_.tip * (mood_ ? mood_->pay : 1.0f);
    if (routeSize_ > 1) mult *= 1.0f + 0.12f * (routeSize_ - 1);
    int coins = (int)std::lround(base * mult);

    bool yetiMilestone = regular && c == CU_YETI && before + 1 == cfg::YETI_PATIENCE;
    if (yetiMilestone) coins += cfg::YETI_BIG_TIP;

    streak_++;
    res_.bestStreak = std::max(res_.bestStreak, streak_);
    res_.deliveries++;
    res_.coins += coins;
    if (aurora) res_.auroraDeliveries++;
    s.coins += coins;
    s.totalDeliveries++;
    s.bestStreak = std::max(s.bestStreak, streak_);

    // retours
    app_.assets.playSfx(app_.assets.coin);
    app_.vibrate(yetiMilestone ? 250 : 60);
    burst(bear_.pos, pal::gold, 18, 420.0f, PK_COIN);
    floats_.push_back({Vector2Add(bear_.pos, {0, -120}), coins > 0 ? TextFormat("+%d", coins) : "+0", pal::gold, 1.6f});
    alert(TextFormat("%s +%d", tr(Str::Delivered), coins), pal::gold);

    if (regular) {
        res_.regulars++;
        s.custCount[c]++;
        int tb = customers::tier(before), ta = customers::tier(s.custCount[c]);
        if (yetiMilestone) {
            toast(customers::name(c), customers::yetiBigTipLine(), c, pal::gold, 5.0f);
        } else {
            toast(customers::name(c), customers::thanksLine(c, ta), c, customers::def(c).color);
        }
        if (ta > tb) {
            toast(TextFormat("%s : %s", customers::name(c), customers::tierName(ta)), customers::tierUpLine(c, ta), c, pal::purple, 4.2f);
        }
    } else {
        toast("Ubear", tr(strAt(Str::Deliv1, GetRandomValue(0, 3))), PORTRAIT_UBEAR, pal::green, 3.0f);
    }

    // défis du jour
    DeliveryReport rep;
    rep.hail = cur().hail; rep.fog = cur().fog; rep.wind = cur().wind; rep.water = cur().water;
    rep.night = weather_.isNight(); rep.blizzard = blizzard; rep.aurora = aurora; rep.regular = regular;
    rep.heatLeft = cur().heat; rep.coins = coins; rep.streak = streak_;
    for (auto& done : challenges::onDelivery(s, rep)) {
        res_.challenges++;
        if (done.allBonus) {
            toast(L("Les 3 défis du jour !", "All 3 daily challenges!"),
                  TextFormat(L("Gaston est impressionné. Bonus +%d pièces.", "Gaston is impressed. Bonus +%d coins."), done.reward),
                  PORTRAIT_GASTON, pal::gold, 4.0f);
        } else {
            std::string d = challenges::describe(challenges::get(s, done.slot));
            toast(L("Défi du jour accompli !", "Daily challenge complete!"), TextFormat("%s  +%d", d.c_str(), done.reward),
                  PORTRAIT_UBEAR, pal::gold, 4.0f);
        }
        app_.vibrate(120);
    }
    save::write(s);

    cur().finished = true;
    advanceAfterStop();
}

void Session::fail()
{
    streak_ = 0;
    res_.failed++;
    if (cur().customer >= 0) {
        toast(customers::name(cur().customer), customers::failLine(cur().customer), cur().customer, pal::red);
    } else {
        toast("Ubear", tr(strAt(Str::Fail1, GetRandomValue(0, 2))), PORTRAIT_UBEAR, pal::red);
    }
    alert(tr(Str::Expired), pal::red);
    app_.vibrate(200);
    cur().finished = true;
    cur().heat = 0.0f;
    advanceAfterStop();
    if (state_ == State::Waiting) cooldown_ = cfg::ORDER_COOLDOWN * 1.5f;
}

void Session::finishTutorial()
{
    if (tuto_ < 0) return;
    tuto_ = -1;
    app_.save.tutorialDone = true;
    save::write(app_.save);
    alert(tr(Str::TutoDone), pal::green);
    if (state_ == State::Waiting) cooldown_ = std::min(cooldown_, cfg::ORDER_COOLDOWN);
    announceDay();
}

void Session::announceDay()
{
    if (announced_) return;
    announced_ = true;
    const char* extra = day_ <= 1
        ? L("Les clients sont indulgents.", "Customers are forgiving.")
        : L("Plus loin, et ça refroidit plus vite.", "Farther away, and it cools faster.");
    toast("Ubear", TextFormat(L("Jour %d : %d commandes. %s", "Day %d: %d orders. %s"), day_, quota_,
                              mood_ ? L(mood_->nameFr, mood_->nameEn) : extra),
          PORTRAIT_UBEAR, day_ <= 1 ? pal::blue : pal::red, 4.2f);
}

void Session::endShift()
{
    if (done_) return;
    SaveData& s = app_.save;
    res_.duration = time_;
    res_.day = day_;
    res_.quota = quota_;
    res_.quotaMet = res_.deliveries >= quota_;
    res_.record = res_.coins > s.bestShiftCoins && res_.coins > 0;
    if (res_.record) s.bestShiftCoins = res_.coins;
    s.day = day_ + 1;
    save::write(s);
    done_ = true;
}

float Session::objH() const
{
    if (state_ == State::Choosing) return 72.0f;
    bool multi = route_.size() > 1;
    if (state_ == State::Deliver) return multi ? 196.0f : 128.0f;
    if (state_ == State::Pickup && multi) return 150.0f;
    return 96.0f;
}

float Session::objectiveBottom() const
{
    return cfg::SAFE_TOP + 174.0f + objH();
}

Rectangle Session::offerRect(int i) const
{
    float W = app_.ui.W;
    float gap = 12.0f;
    float w = (W - 32.0f - gap * 2.0f) / 3.0f;
    return {16.0f + i * (w + gap), app_.ui.H * 0.40f, w, 250.0f};
}

Rectangle Session::launchRect() const
{
    return {70, app_.ui.H * 0.40f + 266, app_.ui.W - 140, 84};
}

Rectangle Session::cargoRect(int i) const
{
    int n = (int)route_.size();
    if (n < 1) n = 1;
    float gap = 10.0f;
    float w = std::min(200.0f, (app_.ui.W - 48.0f - gap * (n - 1)) / n);
    float y = cfg::SAFE_TOP + 174.0f + (state_ == State::Deliver ? 128.0f : 96.0f);
    return {24.0f + i * (w + gap), y, w, 52.0f};
}

float Session::controlTop() const
{
    // Sous le menu et sous l'emplacement de la notification : cette zone ne pilote pas l'ours.
    return objectiveBottom() + 176.0f;
}

void Session::checkDayEnd(float dt)
{
    if (app_.autopilot || tuto_ >= 0 || done_) return;
    float left = dayLength_ - weather_.elapsed();
    if (!closeWarned_ && left < 28.0f && left > 0.0f && res_.deliveries < quota_) {
        closeWarned_ = true;
        toast("Ubear", L("Dernières secondes avant la fermeture. Accélère, Barnabé.",
                         "Last seconds before closing. Hurry, Barnaby."),
              PORTRAIT_UBEAR, pal::red, 3.2f);
    }
    if (dayDoneDelay_ > 0.0f) {
        dayDoneDelay_ -= dt;
        if (dayDoneDelay_ <= 0.0f) endShift();
        return;
    }
    if (left <= 0.0f) {
        res_.timeUp = true;
        for (const Order& o : route_) if (o.carried && !o.finished) res_.failed++;
        endShift();
    }
}

void Session::debugTeleportNearTarget()
{
    Vector2 t = target();
    bear_.pos = Vector2Add(t, {0, 260});
    bear_.vel = {0, 0};
}

// ---------------------------------------------------------------------------
// Entrées
// ---------------------------------------------------------------------------

Rectangle Session::pauseRect() const { return {app_.ui.W - 104, cfg::SAFE_TOP + 6, 84, 84}; }
Rectangle Session::brakeRect() const { return {app_.ui.W * 0.5f - 85, app_.ui.H - 230, 170, 170}; }
Rectangle Session::skipRect() const  { return {24, app_.ui.H - 314, 200, 72}; }

void Session::doPush(Paw p)
{
    const SaveData& s = app_.save;
    float imp = cfg::PUSH_IMPULSE * cfg::SKATE_POWER[s.upgrades[UP_SKATES]] * bonus_.push;
    if (inWater_) imp *= Lerp(1.0f, cfg::WATER_PUSH_MULT, bonus_.water);
    float anchor = cfg::ANCHOR_WEATHER[s.upgrades[UP_ANCHOR]];
    imp *= Lerp(1.0f, cfg::HAIL_PUSH_MULT, ws_.hail * anchor);
    bear_.push(p, imp, bonus_.turn);

    Vector2 at = bear_.pawWorld(p);
    for (int i = 0; i < 6; i++) {
        Vector2 v = Vector2Scale(Vector2Rotate(bear_.forward(), rnd(2.4f, 3.9f)), rnd(80, 260));
        parts_.push_back({at, v, 0.5f, 0.5f, rnd(14, 26), inWater_ ? Color{200, 230, 255, 255} : WHITE, PK_PUFF});
    }
    app_.vibrate(12);
    Paw mate = (p == PAW_BL) ? PAW_TL : PAW_TR;
    bear_.pawPulse[mate] = 0.85f;
    if (tuto_ == 0 && p == PAW_BL) tuto_ = 1;
    else if (tuto_ == 1 && p == PAW_BR) { tuto_ = 2; cooldown_ = 0.6f; announceDay(); }
}

void Session::handleInput()
{
    Ui& ui = app_.ui;
    Input& in = app_.input;

    if (ui.pressed(pauseRect())) { paused_ = true; return; }
    if (tuto_ >= 0 && ui.pressed(skipRect())) { finishTutorial(); }

    braking_ = false;
    if (app_.save.upgrades[UP_BRAKES] > 0) {
        ui.pressed(brakeRect());
        braking_ = ui.held(brakeRect()) || IsKeyDown(KEY_SPACE);
    }

    if (state_ == State::Choosing) {
        int sel = 0;
        for (int i = 0; i < offerN_; i++) if (offerSel_[i]) sel++;
        for (int i = 0; i < offerN_; i++) {
            if (!ui.tapped(offerRect(i))) continue;
            if (bagSlots() <= 1) {
                for (int k = 0; k < offerN_; k++) offerSel_[k] = false;
                offerSel_[i] = true;
                sel = 1;
            } else if (offerSel_[i]) {
                offerSel_[i] = false;
                sel--;
            } else if (sel < bagSlots()) {
                offerSel_[i] = true;
                sel++;
            } else {
                for (int k = 0; k < offerN_; k++) if (offerSel_[k]) { offerSel_[k] = false; break; }
                offerSel_[i] = true;
            }
        }
        if (ui.tapped(launchRect())) {
            if (sel > 0) startRoute();
            else app_.notify(L("Choisis au moins une commande.", "Pick at least one order."));
        }
        return;
    } else if ((state_ == State::Pickup || state_ == State::Deliver) && route_.size() > 1) {
        for (int i = 0; i < (int)route_.size(); i++) {
            if (route_[i].finished) continue;
            if (!ui.tapped(cargoRect(i))) continue;
            bool ok = state_ == State::Deliver ? route_[i].carried : !route_[i].carried;
            if (ok) focus_ = i;
        }
    }

    float cut = controlTop();
    for (auto& e : in.began()) {
        if (e.consumed) continue;
        e.consumed = true;
        Vector2 v = ui.toVirtual(e.pos);
        bool onChoice = false;
        if (state_ == State::Choosing) {
            if (CheckCollisionPointRec(v, launchRect())) onChoice = true;
            for (int i = 0; i < offerN_ && !onChoice; i++) if (CheckCollisionPointRec(v, offerRect(i))) onChoice = true;
        }
        if (route_.size() > 1 && (state_ == State::Pickup || state_ == State::Deliver)) {
            for (int i = 0; i < (int)route_.size() && !onChoice; i++)
                if (CheckCollisionPointRec(v, cargoRect(i))) onChoice = true;
        }
        if (onChoice || v.y < cut) continue;
        doPush(v.x < ui.W * 0.5f ? PAW_BL : PAW_BR);
    }
    if (IsKeyPressed(KEY_F) || IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) doPush(PAW_BL);
    if (IsKeyPressed(KEY_J) || IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) doPush(PAW_BR);
}

void Session::autopilot(float dt)
{
    autoT_ -= dt;
    if (autoT_ > 0) return;
    autoT_ = 0.2f;
    if (tuto_ == 0) { doPush(PAW_BL); return; }
    if (tuto_ == 1) { doPush(PAW_BR); return; }
    if (state_ == State::Choosing) {
        int want = std::min(2, bagSlots());
        int sel = 0;
        for (int i = 0; i < offerN_; i++) if (offerSel_[i]) sel++;
        for (int i = 0; i < offerN_ && sel < want; i++) if (!offerSel_[i]) { offerSel_[i] = true; sel++; }
        startRoute();
        return;
    }
    if (state_ == State::Waiting || route_.empty()) return;
    Vector2 d = Vector2Subtract(target(), bear_.pos);
    float want = std::atan2(d.x, -d.y) * RAD2DEG;
    float diff = wrapDeg(want - bear_.heading);
    Paw p = (autoAlt_++ % 2) ? PAW_BL : PAW_BR;
    if (diff < -16.0f) p = PAW_BL;
    else if (diff > 16.0f) p = PAW_BR;
    doPush(p);
}

// ---------------------------------------------------------------------------
// Mise à jour
// ---------------------------------------------------------------------------

void Session::debugLookAt(int building)
{
    lookAt_ = building;
    if (building >= 0) tuto_ = -1;
}

void Session::applyLook()
{
    if (lookAt_ < 0 || lookAt_ >= (int)buildings_.size()) return;
    Vector2 p = buildings_[lookAt_].pos;
    bear_.pos = {p.x + 280.0f, p.y + 40.0f};
    bear_.vel = {0, 0};
    bear_.angle = -20.0f;
    float sw = (float)GetScreenWidth(), sh = (float)GetScreenHeight();
    cam_.target = p;
    cam_.offset = {sw * 0.5f, sh * 0.55f};
    zoom_ = sw / cfg::VIEW_WIDTH_WORLD;
    cam_.zoom = zoom_;
    if (state_ == State::Choosing) state_ = State::Waiting;
    paused_ = false;
}

void Session::update(float dt)
{
    if (app_.input.backPressed()) paused_ = !paused_;
    if (lookAt_ >= 0) { applyLook(); return; }
    if (paused_ || done_) return;

    if (state_ == State::Choosing) {
        handleInput();
        if (app_.autopilot) autopilot(dt);
        if (!toasts_.empty()) {
            toastT_ += dt;
            if (toastT_ >= toasts_.front().duration) { toasts_.pop_front(); toastT_ = 0.0f; }
        }
        alertT_ = std::max(0.0f, alertT_ - dt);
        return;
    }

    time_ += dt;

    handleInput();
    if (paused_) return;
    if (app_.autopilot) autopilot(dt);

    const SaveData& s = app_.save;
    ws_ = weather_.sample(bear_.pos);
    inWater_ = world_.inWater(bear_.pos);
    float anchor = cfg::ANCHOR_WEATHER[s.upgrades[UP_ANCHOR]];
    BearEnv env;
    env.wind = Vector2Scale(ws_.wind, anchor * bonus_.wind);
    env.water = inWater_;
    env.extraDamping = ws_.hail * cfg::HAIL_DAMPING_ADD * anchor + (braking_ ? cfg::BRAKE_DAMPING[s.upgrades[UP_BRAKES]] : 0.0f);
    bear_.update(dt, env);

    float k = std::min(1.0f, cfg::FOG_FADE_SPEED * dt);
    fogAmt_ += (ws_.fog - fogAmt_) * k;
    hailAmt_ += (ws_.hail - hailAmt_) * k * 2.0f;

    // alertes d'entrée dans une zone
    bool now[4] = {ws_.inWind && !weather_.blizzardActive(), ws_.inFog && !weather_.blizzardActive(), ws_.inHail, inWater_};
    const Str msg[4] = {Str::WindAlert, Str::FogAlert, Str::HailAlert, Str::WaterAlert};
    const Color col[4] = {Color{180, 220, 255, 255}, WHITE, Color{200, 200, 255, 255}, Color{120, 200, 255, 255}};
    for (int i = 0; i < 4; i++) {
        alertCd_[i] -= dt;
        if (now[i] && !was_[i] && alertCd_[i] <= 0) { alert(tr(msg[i]), col[i]); alertCd_[i] = 8.0f; }
        was_[i] = now[i];
    }

    // caméra
    float sw = (float)GetScreenWidth(), sh = (float)GetScreenHeight();
    float zoomTarget = sw / cfg::VIEW_WIDTH_WORLD * (1.0f - 0.15f * std::min(bear_.speed() / 1500.0f, 1.0f));
    zoom_ += (zoomTarget - zoom_) * std::min(1.0f, 2.0f * dt);
    Vector2 ahead = Vector2ClampValue(Vector2Scale(bear_.vel, cfg::CAMERA_LOOKAHEAD), 0.0f, 220.0f);
    Vector2 look = Vector2Add(bear_.pos, ahead);
    cam_.target = Vector2Lerp(cam_.target, look, std::min(1.0f, cfg::CAMERA_SMOOTH * dt));
    cam_.offset = {sw * 0.5f, sh * 0.55f};
    cam_.zoom = zoom_;

    Vector2 tl = GetScreenToWorld2D({0, 0}, cam_), br = GetScreenToWorld2D({sw, sh}, cam_);
    weather_.update(dt, {tl.x, tl.y, br.x - tl.x, br.y - tl.y}, tuto_ < 0);
    handleWeatherEvents();
    checkDayEnd(dt);
    if (done_) return;

    // commande en cours
    switch (state_) {
    case State::Waiting:
        if (dayDoneDelay_ <= 0.0f && (app_.autopilot || res_.deliveries < quota_)) {
            cooldown_ -= dt;
            if (cooldown_ <= 0) openOffers();
        }
        break;
    case State::Choosing:
        break;
    case State::Pickup:
        if (!route_.empty() && Vector2Distance(bear_.pos, buildings_[cur().restaurant].pos) < cfg::DELIVERY_RADIUS) pickup();
        break;
    case State::Deliver: {
        float cool = (weather_.blizzardActive() ? cfg::BLIZZARD_COOL_MULT : 1.0f) * (tuto_ >= 0 ? 0.4f : 1.0f);
        for (int i = (int)route_.size() - 1; i >= 0; i--) {
            Order& o = route_[i];
            if (!o.carried || o.finished) continue;
            if (inWater_) o.water = true;
            if (ws_.inHail) o.hail = true;
            if (ws_.inFog) o.fog = true;
            if (ws_.inWind) o.wind = true;
            if (weather_.blizzardActive()) o.blizzard = true;
            o.heat -= dt / o.totalTime * cool;
            if (o.heat <= 0.0f) {
                focus_ = i;
                fail();
                if (state_ != State::Deliver || route_.empty()) break;
            }
        }
        if (state_ == State::Deliver && !route_.empty() && !cur().finished
            && Vector2Distance(bear_.pos, cur().home) < cfg::DELIVERY_RADIUS) deliver();
        break;
    }
    }

    updateParticles(dt);

    if (!toasts_.empty()) {
        toastT_ += dt * (toasts_.size() > 2 ? 1.6f : 1.0f);
        if (toastT_ >= toasts_.front().duration) { toasts_.pop_front(); toastT_ = 0.0f; }
    }
    alertT_ = std::max(0.0f, alertT_ - dt);
}

void Session::handleWeatherEvents()
{
    SaveData& s = app_.save;
    for (WeatherEvent e : weather_.events()) {
        switch (e) {
        case EV_BLIZZARD_WARN:
            toast(L("Alerte blizzard", "Blizzard warning"),
                  L("Vent + brouillard dans quelques secondes. Accrochez vos poissons.",
                    "Wind + fog in a few seconds. Hold on to your fish."), PORTRAIT_UBEAR, pal::red, 4.0f);
            app_.vibrate(150);
            break;
        case EV_BLIZZARD_START:
            alert(L("BLIZZARD !", "BLIZZARD!"), WHITE);
            toast("Ubear", TextFormat(L("Prime blizzard : +%d %% sur les livraisons. Le plat refroidit plus vite.",
                                        "Blizzard bonus: +%d%% on deliveries. Food cools faster."),
                                      (int)std::lround((cfg::BLIZZARD_REWARD_MULT - 1.0f) * 100)), PORTRAIT_UBEAR, pal::blue, 4.0f);
            break;
        case EV_BLIZZARD_END:
            res_.blizzards++;
            s.blizzardsSurvived++;
            toast("Ubear", L("Le blizzard se calme. Barnabé est toujours debout. Enfin, sur ses rollers.",
                             "The blizzard calms down. Barnaby is still standing. Well, on his skates."), PORTRAIT_UBEAR, pal::green);
            break;
        case EV_NIGHT:
            toast("Ubear", L("La nuit tombe sur Glagla-sur-Mer. Suivez la flèche.",
                             "Night falls over Glagla-sur-Mer. Follow the arrow."), PORTRAIT_UBEAR, pal::purple);
            break;
        case EV_DAWN:
            toast("Ubear", L("Le soleil se lève. Personne n'a dormi.", "The sun rises. Nobody slept."), PORTRAIT_UBEAR, pal::gold);
            break;
        case EV_AURORA_START:
            s.auroras++;
            alert(L("AURORE BORÉALE", "NORTHERN LIGHTS"), Color{120, 255, 180, 255});
            toast("Ubear", TextFormat(L("Aurore boréale ! Pourboires x%d tant qu'elle brille.",
                                        "Northern lights! Tips x%d while they shine."), (int)cfg::AURORA_REWARD_MULT),
                  PORTRAIT_UBEAR, pal::green, 4.0f);
            app_.vibrate(80);
            break;
        case EV_AURORA_END:
            toast("Ubear", L("L'aurore s'efface. Les pourboires redeviennent raisonnables.",
                             "The aurora fades. Tips are back to reasonable."), PORTRAIT_UBEAR, pal::grey);
            break;
        }
    }
}

void Session::updateParticles(float dt)
{
    // traînée cosmétique
    const std::string& trail = app_.save.equipped[COS_TRAIL];
    float sp = bear_.speed();
    if (trail != "trail_none" && sp > 200.0f) {
        trailAcc_ += dt * sp / 45.0f;
        while (trailAcc_ >= 1.0f) {
            trailAcc_ -= 1.0f;
            Vector2 at = Vector2Add(bear_.pos, Vector2Add(Vector2Scale(bear_.forward(), -70.0f), {rnd(-30, 30), rnd(-30, 30)}));
            Vector2 v = {rnd(-40, 40), rnd(-40, 40)};
            if (trail == "trail_snow")         parts_.push_back({at, v, 0.9f, 0.9f, rnd(10, 20), WHITE, PK_SNOW});
            else if (trail == "trail_sparks")  parts_.push_back({at, Vector2Scale(v, 3), 0.6f, 0.6f, rnd(8, 14), Color{255, (unsigned char)GetRandomValue(150, 230), 60, 255}, PK_SPARK});
            else if (trail == "trail_bubbles") parts_.push_back({at, {v.x, v.y - 60}, 1.2f, 1.2f, rnd(8, 18), Color{150, 215, 255, 255}, PK_BUBBLE});
            else if (trail == "trail_rainbow") parts_.push_back({at, v, 1.0f, 1.0f, rnd(14, 22), ColorFromHSV(std::fmod(time_ * 240.0f, 360.0f), 0.7f, 1.0f), PK_RAINBOW});
        }
    }
    for (size_t i = 0; i < parts_.size();) {
        Particle& p = parts_[i];
        p.life -= dt;
        p.pos = Vector2Add(p.pos, Vector2Scale(p.vel, dt));
        p.vel = Vector2Scale(p.vel, std::exp(-3.0f * dt));
        if (p.life <= 0) { parts_[i] = parts_.back(); parts_.pop_back(); }
        else i++;
    }
    for (size_t i = 0; i < floats_.size();) {
        floats_[i].life -= dt;
        floats_[i].pos.y -= 90.0f * dt;
        if (floats_[i].life <= 0) floats_.erase(floats_.begin() + i);
        else i++;
    }
}

void Session::toast(const std::string& title, const std::string& text, int portrait, Color accent, float duration)
{
    if (toasts_.size() > 5) toasts_.pop_front();
    toasts_.push_back({title, text, portrait, accent, duration});
}

void Session::alert(const std::string& text, Color c)
{
    alertText_ = text;
    alertColor_ = c;
    alertT_ = 2.0f;
}

void Session::burst(Vector2 at, Color c, int n, float speed, int kind)
{
    for (int i = 0; i < n; i++) {
        float a = rnd(0, 2 * PI);
        Vector2 v = {std::cos(a) * rnd(0.3f, 1.0f) * speed, std::sin(a) * rnd(0.3f, 1.0f) * speed};
        parts_.push_back({at, v, 0.9f, 0.9f, rnd(10, 18), c, kind});
    }
}

// ---------------------------------------------------------------------------
// Rendu
// ---------------------------------------------------------------------------

void Session::draw()
{
    ClearBackground(OCEAN);
    BeginMode2D(cam_);
    drawWorld();
    EndMode2D();
    drawScreenEffects();

    Ui& ui = app_.ui;
    ui.begin();
    drawHud();
    if (state_ == State::Choosing) {
        DrawRectangle(0, 0, (int)ui.W + 1, (int)ui.H + 1, Color{6, 12, 28, 150});
        ui.text(L("La banquise est en pause", "The ice is paused"), ui.W * 0.5f, ui.H * 0.34f, 32, WHITE, Align::Center);
        drawChoice();
    }
    drawToast();
    if (tuto_ >= 0) drawTutorial();
    if (paused_) drawPause();
    ui.end();
}

void Session::drawWorld()
{
    Assets& a = app_.assets;
    world_.draw();
    weather_.drawGroundZones();

    Vector2 tgt = target();
    bool active = state_ == State::Pickup || state_ == State::Deliver;
    float t = time_;

    // anneau de destination
    if (active) {
        float pulse = 0.5f + 0.5f * std::sin(t * 4.0f);
        DrawCircleV(tgt, cfg::DELIVERY_RADIUS, Fade(state_ == State::Pickup ? ORANGE : SKYBLUE, 0.18f + 0.1f * pulse));
        DrawRing(tgt, cfg::DELIVERY_RADIUS - 8, cfg::DELIVERY_RADIUS, 0, 360, 64, Fade(RED, 0.5f + 0.3f * pulse));
    }

    const float bsz = 380.0f;
    auto drawBuilding = [&](const Texture2D& tex, Vector2 pos, Color tint) {
        if (tex.id == 0) return;
        DrawTexturePro(tex, {0, 0, (float)tex.width, (float)tex.height}, {pos.x, pos.y, bsz, bsz},
                       {bsz * 0.5f, bsz * 0.5f}, 0, tint);
    };
    for (auto& b : buildings_) {
        drawBuilding(b.restaurant ? a.restaurant : a.homeFor(b.customer), b.pos, WHITE);
        if (!b.restaurant) {
            const char* nm = customers::name(b.customer);
            Vector2 m = MeasureTextEx(a.font, nm, 44, 1);
            float ly = b.pos.y + bsz * 0.52f;
            DrawRectangleRounded({b.pos.x - m.x * 0.5f - 14, ly, m.x + 28, 54}, 0.5f, 6, Fade(customers::def(b.customer).color, 0.85f));
            DrawTextEx(a.font, nm, {b.pos.x - m.x * 0.5f, ly + 5}, 44, 1, WHITE);
        }
    }
    if (active) {
        for (int i = 0; i < (int)route_.size(); i++) {
            const Order& o = route_[i];
            if (o.finished || o.customer >= 0) continue;
            bool here = i == focus_ && state_ == State::Deliver;
            drawBuilding(a.igloo, o.home, Fade(WHITE, here ? 1.0f : 0.45f));
        }
        for (int i = 0; i < (int)route_.size(); i++) {
            if (route_[i].finished || i == focus_) continue;
            Vector2 p = route_[i].carried || state_ == State::Deliver ? route_[i].home : buildings_[route_[i].restaurant].pos;
            DrawCircleV(p, 46, Fade(GOLD, 0.35f));
        }
    }

    weather_.drawFogZones();

    if (active) {
        float bob = std::sin(t * 3.0f) * 18.0f;
        float s = 170.0f;
        DrawTexturePro(a.arrow, {0, 0, (float)a.arrow.width, (float)a.arrow.height}, {tgt.x, tgt.y - 230 + bob, s, s}, {s * 0.5f, s * 0.5f}, 0, WHITE);
    }

    // particules (sous l'ours)
    for (auto& p : parts_) {
        float k = p.life / p.maxLife;
        switch (p.kind) {
        case PK_BUBBLE:
            DrawRing(p.pos, p.size * (1.6f - k * 0.6f) - 2.5f, p.size * (1.6f - k * 0.6f), 0, 360, 16, Fade(p.color, k));
            break;
        case PK_SPARK:
        case PK_COIN:
            BeginBlendMode(BLEND_ADDITIVE);
            DrawTexturePro(a.particle, {0, 0, 64, 64}, {p.pos.x, p.pos.y, p.size * 2.5f, p.size * 2.5f}, {p.size * 1.25f, p.size * 1.25f}, 0, Fade(p.color, k));
            EndBlendMode();
            break;
        default:
            DrawTexturePro(a.particle, {0, 0, 64, 64}, {p.pos.x, p.pos.y, p.size * 2.2f, p.size * 2.2f}, {p.size * 1.1f, p.size * 1.1f}, 0, Fade(p.color, k * 0.9f));
            break;
        }
    }

    drawBear(a, app_.save, bear_.pos, bear_.angle, cfg::BEAR_SCALE, bear_.pawPulse);
    weather_.drawParticles(a);

    for (auto& f : floats_) {
        Vector2 m = MeasureTextEx(a.font, f.text.c_str(), 64, 1);
        DrawTextEx(a.font, f.text.c_str(), {f.pos.x - m.x * 0.5f + 3, f.pos.y + 4}, 64, 1, Fade(BLACK, 0.4f * std::min(1.0f, f.life)));
        DrawTextEx(a.font, f.text.c_str(), {f.pos.x - m.x * 0.5f, f.pos.y}, 64, 1, Fade(f.color, std::min(1.0f, f.life)));
    }
}

void Session::drawScreenEffects()
{
    Assets& a = app_.assets;
    float sw = (float)GetScreenWidth(), sh = (float)GetScreenHeight();
    Vector2 bs = GetWorldToScreen2D(bear_.pos, cam_);

    // grêle à l'écran
    if (hailAmt_ > 0.02f) {
        float tile = 192.0f * app_.ui.scale * 1.4f;
        float off = std::fmod(time_ * tile * 2.2f, tile);
        for (float y = -tile + off; y < sh + tile; y += tile)
            for (float x = -tile - off * 0.6f; x < sw + tile; x += tile)
                DrawTexturePro(a.rain, {0, 0, (float)a.rain.width, (float)a.rain.height}, {x, y, tile, tile}, {0, 0}, 0, Fade(WHITE, hailAmt_ * 0.9f));
    }

    // brouillard : vignette (plus serrée pendant un blizzard)
    if (fogAmt_ > 0.01f) {
        const Texture2D& f = weather_.blizzard() > 0.3f ? a.fog[2] : a.fog[1];
        float side = std::min(sw, sh) * 1.25f;
        Rectangle r = {sw * 0.5f - side * 0.5f, sh * 0.55f - side * 0.5f, side, side};
        Color edge = Fade(WHITE, 0.74f * fogAmt_);
        DrawTexturePro(f, {0, 0, (float)f.width, (float)f.height}, r, {0, 0}, 0, Fade(WHITE, fogAmt_));
        DrawRectangle(0, 0, (int)sw, (int)std::max(0.0f, r.y), edge);
        DrawRectangle(0, (int)(r.y + r.height), (int)sw, (int)(sh - r.y - r.height) + 1, edge);
        DrawRectangle(0, (int)r.y, (int)std::max(0.0f, r.x), (int)r.height, edge);
        DrawRectangle((int)(r.x + r.width), (int)r.y, (int)(sw - r.x - r.width) + 1, (int)r.height, edge);
        DrawRectangle(0, 0, (int)sw, (int)sh, Fade(WHITE, 0.18f * fogAmt_));
    }
    weather_.drawBlizzard(a, sw, sh);

    // nuit : obscurité avec halo autour de Barnabé
    float dark = weather_.darkness() * (1.0f - 0.3f * weather_.aurora());
    if (dark > 0.01f) {
        Color nc = {6, 12, 42, (unsigned char)(225 * dark)};
        float r = sw * 0.62f;
        Rectangle L = {bs.x - r, bs.y - r, 2 * r, 2 * r};
        DrawTexturePro(a.light, {0, 0, (float)a.light.width, (float)a.light.height}, L, {0, 0}, 0, nc);
        DrawRectangle(0, 0, (int)sw, (int)std::max(0.0f, L.y), nc);
        DrawRectangle(0, (int)(L.y + L.height), (int)sw, (int)(sh - L.y - L.height) + 1, nc);
        DrawRectangle(0, (int)L.y, (int)std::max(0.0f, L.x), (int)L.height, nc);
        DrawRectangle((int)(L.x + L.width), (int)L.y, (int)(sw - L.x - L.width) + 1, (int)L.height, nc);

        // fenêtres allumées
        BeginBlendMode(BLEND_ADDITIVE);
        for (auto& b : buildings_) {
            Vector2 p = GetWorldToScreen2D(b.pos, cam_);
            if (p.x < -200 || p.y < -200 || p.x > sw + 200 || p.y > sh + 200) continue;
            float s = 380.0f * zoom_;
            DrawTexturePro(a.particle, {0, 0, 64, 64}, {p.x, p.y, s, s}, {s * 0.5f, s * 0.5f}, 0, Fade(Color{255, 200, 110, 255}, 0.6f * dark));
        }
        EndBlendMode();
    }
    weather_.drawAurora(sw, sh);

    if (state_ == State::Waiting) return;
    Vector2 ts = GetWorldToScreen2D(target(), cam_);

    // GPS niveau 3 : la destination reste visible à travers le brouillard
    if (app_.save.upgrades[UP_GPS] >= 3 && fogAmt_ > 0.2f) {
        float s = 110.0f * app_.ui.scale;
        float bob = std::sin(time_ * 3.0f) * 8.0f;
        DrawTexturePro(a.arrow, {0, 0, (float)a.arrow.width, (float)a.arrow.height}, {ts.x, ts.y - s * 0.6f + bob, s, s}, {s * 0.5f, s * 0.5f}, 0, Fade(WHITE, 0.95f));
    }

    // flèche de direction autour de Barnabé
    Vector2 d = Vector2Subtract(ts, bs);
    float len = Vector2Length(d);
    if (len > 1.0f) {
        Vector2 n = Vector2Scale(d, 1.0f / len);
        float R = 150.0f * zoom_;
        if (len < R * 1.3f) return;
        Vector2 tip = Vector2Add(bs, Vector2Scale(n, R + 34 * app_.ui.scale));
        Vector2 base = Vector2Add(bs, Vector2Scale(n, R));
        Vector2 side = {-n.y, n.x};
        float w = 26.0f * app_.ui.scale;
        Vector2 p1 = Vector2Add(base, Vector2Scale(side, w)), p2 = Vector2Subtract(base, Vector2Scale(side, w));
        DrawTriangle(Vector2Add(tip, {2, 4}), Vector2Add(p2, {2, 4}), Vector2Add(p1, {2, 4}), Fade(BLACK, 0.3f));
        DrawTriangle(tip, p2, p1, Color{235, 40, 40, 255});
    }
}

void Session::drawMinimap(Rectangle r)
{
    Assets& a = app_.assets;
    DrawRectangleRounded({r.x - 6, r.y - 6, r.width + 12, r.height + 12}, 0.12f, 8, Fade(BLACK, 0.45f));
    DrawTexturePro(world_.texture(), {0, 0, (float)world_.texture().width, (float)world_.texture().height}, r, {0, 0}, 0, WHITE);
    weather_.drawMinimapZones(r);
    auto toMap = [&](Vector2 p) { return Vector2{r.x + p.x / cfg::WORLD_SIZE * r.width, r.y + p.y / cfg::WORLD_SIZE * r.height}; };
    for (auto& b : buildings_) DrawCircleV(toMap(b.pos), 4.5f, b.restaurant ? Color{220, 80, 50, 255} : Color{60, 130, 210, 255});
    if (state_ != State::Waiting) {
        Vector2 t = toMap(target());
        DrawCircleV(t, 8.0f + 2.0f * std::sin(time_ * 6.0f), RED);
        DrawCircleV(t, 4.0f, WHITE);
    }
    Vector2 b = toMap(bear_.pos);
    DrawCircleV(b, 7, BLACK);
    DrawCircleV(b, 5, WHITE);
    (void)a;
}

void Session::drawChoice()
{
    Ui& ui = app_.ui;
    int sel = 0;
    for (int i = 0; i < offerN_; i++) if (offerSel_[i]) sel++;
    for (int i = 0; i < offerN_; i++) {
        const Order& o = offers_[i];
        Rectangle r = offerRect(i);
        ui.panel(r, offerSel_[i] ? Color{255, 248, 226, 250} : Color{255, 255, 255, 235}, 0.2f);
        if (offerSel_[i]) DrawRectangleRoundedLinesEx(r, 0.16f, 8, 5, pal::gold);
        int who = o.customer >= 0 ? o.customer : PORTRAIT_UBEAR;
        customers::drawPortrait(who, {r.x + r.width * 0.5f, r.y + 58}, 36, time_);
        const char* nm = o.customer >= 0 ? customers::name(o.customer) : L("Passage", "Walk-in");
        ui.textBox(nm, {r.x + 8, r.y + 100, r.width - 16, 50}, 24, pal::ink, Align::Center);
        int meters = (int)(o.distance / 10.0f);
        ui.text(TextFormat("%d m", meters), r.x + r.width * 0.5f, r.y + 156, 26, pal::soft, Align::Center);
        float pay = (cfg::REWARD_BASE + o.distance * cfg::REWARD_PER_PX) * cfg::dayPay(day_) * bonus_.tip * (mood_ ? mood_->pay : 1.0f);
        if (o.customer >= 0) pay *= customers::tipMult(o.customer, app_.save.custCount[o.customer]);
        ui.coinIcon({r.x + r.width * 0.5f - 46, r.y + 204}, 16);
        ui.text(TextFormat("~%d", std::max(1, (int)std::lround(pay))), r.x + r.width * 0.5f - 22, r.y + 188, 28, pal::ink);
    }
    const char* label = sel > 1 ? TextFormat(L("Tournée ×%d", "Route ×%d"), sel)
                       : sel == 1 ? L("Prendre cette commande", "Take this order")
                                  : L("Choisis une commande", "Pick an order");
    Rectangle b = launchRect();
    Color btn = sel > 0 ? pal::red : pal::grey;
    DrawRectangleRounded({b.x, b.y + 6, b.width, b.height}, 0.4f, 8, shade(btn, 0.65f));
    DrawRectangleRounded(b, 0.4f, 8, btn);
    ui.text(label, b.x + b.width * 0.5f, b.y + 22, 32, WHITE, Align::Center);
    ui.text(TextFormat(L("Sac : %d / %d", "Bag: %d / %d"), sel, bagSlots()),
            app_.ui.W * 0.5f, b.y + b.height + 10, 26, WHITE, Align::Center);
}

void Session::drawHud()
{
    Ui& ui = app_.ui;
    const SaveData& s = app_.save;
    float W = ui.W, H = ui.H, top = cfg::SAFE_TOP;

    // barre du haut
    ui.panel({16, top, W - 32, 96}, pal::dim, 0.4f);
    ui.coinIcon({60, top + 48}, 24);
    ui.textShadow(TextFormat("%d", s.coins), 96, top + 24, 48, WHITE);
    if (streak_ > 0) ui.textShadow(TextFormat("%s x%d", tr(Str::Streak), streak_), W * 0.47f, top + 30, 36, pal::gold, Align::Center);

    float h = weather_.hour();
    int hh = (int)h, mm = (int)((h - hh) * 60) / 10 * 10;
    bool night = weather_.isNight();
    Vector2 ic = {W - 236, top + 48};
    if (night) {
        DrawCircleV(ic, 15, Color{240, 240, 210, 255});
        DrawCircleV({ic.x + 7, ic.y - 5}, 13, Color{24, 34, 60, 255});
    } else {
        DrawCircleV(ic, 13, pal::gold);
        for (int i = 0; i < 8; i++) {
            float an = i * PI / 4;
            DrawLineEx({ic.x + std::cos(an) * 17, ic.y + std::sin(an) * 17}, {ic.x + std::cos(an) * 23, ic.y + std::sin(an) * 23}, 3, pal::gold);
        }
    }
    ui.textShadow(TextFormat("%02d:%02d", hh, mm), W - 212, top + 30, 36, WHITE);

    Rectangle pr = pauseRect();
    DrawRectangleRounded(pr, 0.35f, 8, ui.held(pr) ? shade(pal::blue, 0.8f) : pal::blue);
    DrawRectangleRounded({pr.x + 24, pr.y + 20, 12, 44}, 0.5f, 4, WHITE);
    DrawRectangleRounded({pr.x + 48, pr.y + 20, 12, 44}, 0.5f, 4, WHITE);

    // journée : numéro, quota, temps restant avant la fermeture
    float stripY = top + 108;
    ui.panel({16, stripY, W - 32, 58}, pal::dim, 0.35f);
    ui.text(TextFormat(L("Jour %d", "Day %d"), day_), 36, stripY + 6, 30, WHITE);
    bool quotaFull = res_.deliveries >= quota_;
    ui.text(TextFormat(L("Commandes  %d/%d", "Orders  %d/%d"), res_.deliveries, quota_),
            W - 36, stripY + 8, 28, quotaFull ? pal::green : WHITE, Align::Right);
    float left = dayLength_ > 1.0f ? 1.0f - weather_.elapsed() / dayLength_ : 0.0f;
    if (left < 0.0f) left = 0.0f;
    if (left > 1.0f) left = 1.0f;
    ui.bar({36, stripY + 40, W - 72, 10}, left, left < 0.18f ? pal::red : pal::gold, Color{40, 52, 78, 255});

    // objectif : reste sous le menu, les notifications passent en dessous
    Rectangle op = {16, top + 174, W - 32, objH()};
    ui.panel(op, pal::panel, 0.3f);
    std::string line1, line2;
    int portrait = -1;
    if (state_ == State::Waiting) {
        line1 = L("En attente d'une commande…", "Waiting for an order…");
        portrait = PORTRAIT_UBEAR;
    } else if (state_ == State::Choosing) {
        line1 = TextFormat(L("Choisis jusqu'à %d commandes", "Pick up to %d orders"), bagSlots());
        portrait = PORTRAIT_UBEAR;
    } else if (state_ == State::Pickup) {
        line1 = tr(Str::GoRestaurant);
        portrait = cur().customer >= 0 ? cur().customer : PORTRAIT_UBEAR;
    } else {
        line1 = cur().customer >= 0 ? TextFormat(L("Livre à %s", "Deliver to %s"), customers::name(cur().customer)) : tr(Str::GoHouse);
        portrait = cur().customer >= 0 ? cur().customer : PORTRAIT_UBEAR;
    }
    customers::drawPortrait(portrait, {op.x + 52, op.y + 48}, 34, time_);
    ui.textBox(line1.c_str(), {op.x + 100, op.y + 14, op.width - 116, 40}, 30, pal::ink);
    if ((state_ == State::Pickup || state_ == State::Deliver) && s.upgrades[UP_GPS] >= 1) {
        float dist = Vector2Distance(bear_.pos, target()) / 10.0f;
        ui.text(TextFormat("%s : %d m", tr(Str::Distance), (int)dist), op.x + 100, op.y + 54, 26, pal::soft);
    }
    if (state_ == State::Deliver) {
        float heat = std::max(0.0f, cur().heat);
        Color hc = ColorLerp(Color{90, 170, 240, 255}, Color{245, 110, 40, 255}, heat);
        ui.text(tr(Str::Hot), op.x + 24, op.y + 88, 26, pal::soft);
        ui.bar({op.x + 130, op.y + 92, op.width - 154, 22}, heat, hc, Color{215, 225, 240, 255});
    }
    if (route_.size() > 1 && (state_ == State::Pickup || state_ == State::Deliver)) {
        for (int i = 0; i < (int)route_.size(); i++) {
            const Order& o = route_[i];
            Rectangle cr = cargoRect(i);
            bool on = i == focus_;
            ui.panel(cr, on ? Color{255, 248, 230, 245} : pal::panel, 0.2f);
            int who = o.customer >= 0 ? o.customer : PORTRAIT_UBEAR;
            customers::drawPortrait(who, {cr.x + 26, cr.y + 26}, 16, time_);
            if (o.carried && !o.finished) {
                Color hc = ColorLerp(Color{90, 170, 240, 255}, Color{245, 110, 40, 255}, std::max(0.0f, o.heat));
                ui.bar({cr.x + 50, cr.y + 18, cr.width - 62, 16}, std::max(0.0f, o.heat), hc, Color{215, 225, 240, 255});
            } else if (!o.carried) {
                ui.text(L("resto", "resto"), cr.x + 50, cr.y + 14, 20, pal::soft);
            }
        }
    }

    // événements en cours
    float cy = controlTop() + 8;
    float cx = 16;
    auto chip = [&](const char* label, Color bg) {
        Vector2 m = ui.measure(label, 28);
        ui.panel({cx, cy, m.x + 32, 50}, bg, 0.6f);
        ui.text(label, cx + 16, cy + 11, 28, WHITE);
        cy += 62;
    };
    if (mood_) chip(L(mood_->nameFr, mood_->nameEn), Color{70, 90, 150, 230});
    float warn = weather_.blizzardWarning();
    if (warn >= 0.0f && std::fmod(time_, 0.6f) < 0.4f) chip(TextFormat(L("Blizzard dans %d s", "Blizzard in %d s"), (int)std::ceil(warn)), pal::red);
    if (weather_.blizzardActive()) chip(TextFormat(L("BLIZZARD  +%d %%  %d s", "BLIZZARD  +%d%%  %d s"),
                                                   (int)std::lround((cfg::BLIZZARD_REWARD_MULT - 1) * 100), (int)std::ceil(weather_.blizzardTimeLeft())), Color{90, 130, 190, 240});
    if (weather_.auroraActive()) chip(TextFormat(L("AURORE  x%d  %d s", "AURORA  x%d  %d s"), (int)cfg::AURORA_REWARD_MULT,
                                                 (int)std::ceil(weather_.auroraTimeLeft())), Color{40, 170, 120, 240});

    if (s.upgrades[UP_GPS] >= 2) drawMinimap({W - 196, op.y + op.height + 20, 176, 176});

    // frein
    if (s.upgrades[UP_BRAKES] > 0) {
        Rectangle br = brakeRect();
        Vector2 c = {br.x + br.width * 0.5f, br.y + br.height * 0.5f};
        DrawCircleV({c.x, c.y + 6}, 78, Fade(BLACK, 0.3f));
        DrawCircleV(c, 78, braking_ ? Color{200, 40, 40, 240} : Color{226, 72, 72, 200});
        DrawRing(c, 66, 74, 0, 360, 48, Fade(WHITE, 0.8f));
        ui.text(tr(Str::Brake), c.x, c.y - 16, 32, WHITE, Align::Center);
    }

    // alerte centrale
    if (alertT_ > 0.0f) {
        float a = std::min(1.0f, alertT_ / 0.4f);
        float sz = 52.0f;
        float y = H * 0.33f - (1.0f - std::min(1.0f, (2.0f - alertT_) * 5.0f)) * 30.0f;
        Rectangle box = {40, y, W - 80, 200};
        float hgt = ui.textBox(alertText_.c_str(), box, sz, WHITE, Align::Center, false);
        ui.panel({60, y - 16, W - 120, hgt + 32}, Fade(Color{10, 20, 40, 255}, 0.45f * a), 0.4f);
        ui.textBox(alertText_.c_str(), {box.x + 2, box.y + 3, box.width, box.height}, sz, Fade(BLACK, 0.5f * a), Align::Center);
        ui.textBox(alertText_.c_str(), box, sz, Fade(alertColor_, a), Align::Center);
    }
}

Rectangle Session::toastRect() const
{
    if (toasts_.empty()) return {0, 0, 0, 0};
    const Ui& ui = app_.ui;
    const Toast& t = toasts_.front();
    float in = std::min(1.0f, toastT_ / 0.25f);
    float out = std::min(1.0f, (t.duration - toastT_) / 0.25f);
    float k = std::max(0.0f, std::min(in, out));
    float ease = 1.0f - (1.0f - k) * (1.0f - k);
    float rest = objectiveBottom() + 10.0f;
    float gap = app_.save.upgrades[UP_GPS] >= 2 ? 200.0f : 0.0f;
    Rectangle r = {16, rest, ui.W - 32 - gap, 150};
    float textH = ui.textBox(t.text.c_str(), {r.x + 116, r.y + 58, r.width - 136, 200}, 27, pal::ink, Align::Left, false);
    r.height = std::max(130.0f, 70.0f + textH);
    float limit = ui.H - r.height - 24.0f;
    if (app_.save.upgrades[UP_BRAKES] > 0) limit = ui.H - 250.0f - r.height - 12.0f;
    if (r.y > limit) r.y = limit;
    r.y += (1.0f - ease) * (r.height + 24.0f);
    return r;
}

void Session::drawToast()
{
    if (toasts_.empty()) return;
    Ui& ui = app_.ui;
    const Toast& t = toasts_.front();
    Rectangle r = toastRect();
    ui.panel(r, Color{255, 255, 255, 250}, 0.25f);
    DrawRectangleRounded({r.x, r.y, 12, r.height}, 1.0f, 4, t.accent);
    customers::drawPortrait(t.portrait, {r.x + 62, r.y + 64}, 42, time_);
    ui.text(t.title.c_str(), r.x + 116, r.y + 16, 32, shade(t.accent, 0.8f));
    ui.text(L("maintenant", "now"), r.x + r.width - 20, r.y + 20, 22, pal::grey, Align::Right);
    ui.textBox(t.text.c_str(), {r.x + 116, r.y + 58, r.width - 136, 200}, 27, pal::ink);
}

void Session::drawTutorial()
{
    Ui& ui = app_.ui;
    float W = ui.W, H = ui.H;
    if (tuto_ <= 1) {
        bool left = tuto_ == 0;
        float y0 = controlTop();
        Rectangle q = {left ? 0.0f : W * 0.5f, y0, W * 0.5f, H - y0};
        float pulse = 0.5f + 0.5f * std::sin(time_ * 5.0f);
        DrawRectangleRec(q, Fade(WHITE, 0.12f + 0.12f * pulse));
        DrawRectangleLinesEx(q, 6, Fade(pal::gold, 0.6f + 0.4f * pulse));
        Vector2 c = {q.x + q.width * 0.5f, y0 + (H - y0) * 0.42f};
        DrawCircleV(c, 46 + 10 * pulse, Fade(pal::gold, 0.5f));
        DrawCircleV(c, 30, WHITE);
    }
    Str msg = tuto_ == 0 ? Str::Tuto1 : tuto_ == 1 ? Str::Tuto2 : tuto_ == 2 ? Str::Tuto5 : Str::Tuto6;
    Rectangle box = {50, controlTop() + 16.0f, W - 100, 0};
    if (tuto_ <= 1) box.y = controlTop() - 168.0f;
    float h = ui.textBox(tr(msg), {box.x + 24, box.y + 20, box.width - 48, 300}, 34, pal::ink, Align::Center, false);
    ui.panel({box.x, box.y, box.width, h + 40}, Color{255, 255, 255, 235}, 0.3f);
    ui.textBox(tr(msg), {box.x + 24, box.y + 20, box.width - 48, 300}, 34, pal::ink, Align::Center);

    Rectangle sk = skipRect();
    DrawRectangleRounded(sk, 0.5f, 8, Fade(BLACK, 0.5f));
    ui.text(tr(Str::TutoSkip), sk.x + sk.width * 0.5f, sk.y + 18, 34, WHITE, Align::Center);
}

void Session::drawPause()
{
    Ui& ui = app_.ui;
    float W = ui.W, H = ui.H;
    DrawRectangle(0, 0, (int)W + 1, (int)H + 1, pal::dim);
    Rectangle p = {40, H * 0.5f - 430, W - 80, 860};
    ui.panel(p, pal::panel, 0.08f);
    ui.text(tr(Str::Pause), W * 0.5f, p.y + 36, 64, pal::ink, Align::Center);

    ui.coinIcon({p.x + 80, p.y + 150}, 22);
    ui.text(TextFormat("+%d", res_.coins), p.x + 112, p.y + 130, 40, pal::ink);
    ui.text(TextFormat(L("Jour %d  ·  %d/%d", "Day %d  ·  %d/%d"), day_, res_.deliveries, quota_),
            p.x + p.width - 40, p.y + 132, 32, pal::soft, Align::Right);

    ui.text(L("Défis du jour", "Daily challenges"), p.x + 40, p.y + 200, 36, pal::blue);
    float y = p.y + 252;
    for (int i = 0; i < CHALLENGE_SLOTS; i++) {
        ChallengeDef c = challenges::get(app_.save, i);
        bool done = app_.save.chalDone[i];
        std::string d = challenges::describe(c);
        ui.textBox(d.c_str(), {p.x + 40, y, p.width - 80, 80}, 28, done ? pal::green : pal::ink);
        ui.bar({p.x + 40, y + 40, p.width - 220, 18}, (float)app_.save.chalProg[i] / c.target, done ? pal::green : pal::gold, Color{220, 228, 240, 255});
        ui.text(done ? "OK" : TextFormat("%d/%d", app_.save.chalProg[i], c.target), p.x + p.width - 40, y + 32, 28, pal::soft, Align::Right);
        y += 92;
    }

    if (ui.button({p.x + 60, p.y + p.height - 290, p.width - 120, 100}, tr(Str::Resume), pal::blue, 44)) paused_ = false;
    if (ui.button({p.x + 60, p.y + p.height - 160, p.width - 120, 100}, tr(Str::EndShift), pal::red, 40)) endShift();
}
