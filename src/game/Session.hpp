// Un service de livraison : de la première commande au bilan.
#pragma once
#include "../App.hpp"
#include "Bear.hpp"
#include "Catalog.hpp"
#include "Challenges.hpp"
#include "Days.hpp"
#include "Weather.hpp"
#include "World.hpp"

#include <deque>
#include <string>
#include <vector>

class Session {
public:
    explicit Session(App& app);
    ~Session();

    void update(float dt);
    void draw();

    bool done() const { return done_; }
    const ShiftResult& result() const { return res_; }
    void pause() { paused_ = true; }
    void endShift();

    // démo / tests
    void debugBlizzard() { weather_.forceBlizzard(); }
    void debugNight(bool aurora) { weather_.forceNight(aurora); }
    void debugTeleportNearTarget();
    void debugLookAt(int building);   // -1 reprend le jeu ; 0..3 hubs, 4..8 habitants

private:
    enum class State { Waiting, Choosing, Pickup, Deliver };
    struct Building { Vector2 pos; bool restaurant; int customer; };
    struct Order {
        int     restaurant = 0;
        int     customer = -1;              // -1 : client de passage
        Vector2 home{0, 0};
        float   distance = 0.0f, totalTime = 1.0f, heat = 1.0f;
        int     flavor = 0;
        bool    water = false, hail = false, fog = false, wind = false, blizzard = false;
        bool    carried = false, finished = false;
    };
    struct Toast { std::string title, text; int portrait; Color accent; float duration; };
    struct Particle { Vector2 pos, vel; float life, maxLife, size; Color color; int kind; };
    struct FloatText { Vector2 pos; std::string text; Color color; float life; };

    // logique
    void placeBuildings();
    Order makeOffer(bool tutorial);
    void openOffers();
    void startRoute();
    void arm(Order& o);
    int  nearestIndex(bool carried) const;
    int  bagSlots() const;
    void pickup();
    void deliver();
    void fail();
    void advanceAfterStop();
    Order& cur();
    const Order& cur() const;
    void doPush(Paw p);
    void handleInput();
    void autopilot(float dt);
    void handleWeatherEvents();
    void updateParticles(float dt);
    void finishTutorial();
    void applyLook();
    void announceDay();
    void checkDayEnd(float dt);
    float objH() const;
    float objectiveBottom() const;
    float controlTop() const;
    Vector2 target() const;
    Rectangle offerRect(int i) const;
    Rectangle launchRect() const;
    Rectangle cargoRect(int i) const;
    void drawChoice();

    // retours joueur
    void toast(const std::string& title, const std::string& text, int portrait, Color accent, float duration = 3.6f);
    void alert(const std::string& text, Color c);
    void burst(Vector2 at, Color c, int n, float speed, int kind);

    // rendu
    void drawWorld();
    void drawScreenEffects();
    void drawHud();
    void drawToast();
    Rectangle toastRect() const;
    void drawTutorial();
    void drawPause();
    void drawMinimap(Rectangle r);
    Rectangle pauseRect() const;
    Rectangle brakeRect() const;
    Rectangle skipRect() const;

    App&     app_;
    World    world_;
    Weather  weather_;
    Bear     bear_;
    Camera2D cam_{};
    float    zoom_ = 1.0f;

    std::vector<Building> buildings_;
    std::vector<Order> route_;
    int    focus_ = 0;
    int    routeSize_ = 1;
    Order  offers_[3];
    bool   offerSel_[3] = {false, false, false};
    int    offerN_ = 0;
    State  state_ = State::Waiting;
    const MoodFx* mood_ = nullptr;
    CosBonus bonus_{};
    float  cooldown_ = 1.0f;
    int    lastCustomer_ = -1;
    int    streak_ = 0;
    ShiftResult res_;
    float  time_ = 0.0f;
    bool   paused_ = false, done_ = false, braking_ = false;
    int    day_ = 1, quota_ = 3;
    float  dayLength_ = 270.0f;
    float  dayDoneDelay_ = 0.0f;
    bool   closeWarned_ = false, announced_ = false;

    WeatherSample ws_;
    bool   inWater_ = false;
    float  fogAmt_ = 0.0f, hailAmt_ = 0.0f;
    bool   was_[4] = {false, false, false, false};   // vent, brouillard, grêle, eau
    float  alertCd_[4] = {0, 0, 0, 0};

    std::deque<Toast> toasts_;
    float  toastT_ = 0.0f;
    std::string alertText_;
    Color  alertColor_ = WHITE;
    float  alertT_ = 0.0f;

    std::vector<Particle>  parts_;
    std::vector<FloatText> floats_;
    float  trailAcc_ = 0.0f;

    int    tuto_ = -1;          // -1 : pas de tutoriel ; 0 gauche ; 1 droite ; 2 restaurant ; 3 igloo
    int    lookAt_ = -1;        // bâtiment cadré pour les captures, -1 sinon
    float  autoT_ = 0.0f;
    int    autoAlt_ = 0;
};
