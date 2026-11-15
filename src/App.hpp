// État global de l'application : ressources, sauvegarde, écran courant.
#pragma once
#include "core/Assets.hpp"
#include "core/Input.hpp"
#include "core/Save.hpp"
#include "core/Ui.hpp"

#include <memory>
#include <string>

enum class Screen { Story, Menu, Game, Shop, Settings, Results, Challenges, Customers };
enum class AdSlot { None, EndShift, Energy, Gold };

struct ShiftResult {
    int   coins = 0, deliveries = 0, failed = 0, bestStreak = 0;
    int   regulars = 0, challenges = 0, blizzards = 0, auroraDeliveries = 0;
    int   day = 1, quota = 3;
    bool  record = false, quotaMet = false, timeUp = false;
    float duration = 0.0f;
};

class Session;

struct App {
    Assets   assets;
    SaveData save;
    Input    input;
    Ui       ui;

    Screen screen = Screen::Menu;
    float  time = 0.0f, screenTime = 0.0f;
    bool   quit = false;
    bool   autopilot = false;           // tests automatiques (PC)

    std::unique_ptr<Session> session;
    ShiftResult lastResult;

    // état des écrans
    int   storyPage = 0;
    Screen storyReturn = Screen::Menu;
    int   shopTab = 0, shopCat = 0;
    float shopScroll = 0.0f;            // défilement des listes (boutique, carnet)
    std::string shopSelected;
    std::string toastText;
    float toastTime = 0.0f;
    int   gastonLine = 0;
    bool  confirmReset = false;
    AdSlot adSlot = AdSlot::None;
    float adTime = 0.0f;
    int   confirmOffer = -1;

    App();
    ~App();

    void go(Screen s);
    void persist();
    void applyAudio();
    void vibrate(int ms);
    void notify(const std::string& text);   // petit message en bas d'écran (menus)
    void startShift();
    void finishShift();
    void beginAd(AdSlot slot);
    void finishAd();
};
