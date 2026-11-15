// Textes FR / EN. Toutes les chaînes affichées passent par tr().
#pragma once

enum class Lang { FR = 0, EN = 1 };

enum class Str {
    AppName, Tagline, Play, Shop, Settings, Back, Resume, EndShift, Pause, Coins,
    GoRestaurant, GoHouse, PickedUp, Delivered, Expired, Streak, Distance, Reward,
    Equipment, Cosmetics, Buy, Equip, Equipped, Max, Level, NotEnough, Free, Owned,
    UpSkatesName, UpSkatesDesc, UpBrakesName, UpBrakesDesc, UpGpsName, UpGpsDesc,
    UpBagName, UpBagDesc, UpAnchorName, UpAnchorDesc,
    CatSkin, CatHat, CatBag, CatSkate, CatTrail,
    CosNone, CosOriginal,
    SkinPolaire, SkinBrun, SkinMarron, SkinPanda, SkinDore,
    HatBonnetRouge, HatBonnetVert, HatCasquette, HatLivreur, HatHautDeForme, HatCouronne,
    BagBleu, BagRouge, BagVert, BagNoir, BagOr,
    SkateRouge, SkateBleu, SkateVert, SkateNoir, SkateOr,
    TrailSnow, TrailSparks, TrailRainbow, TrailBubbles,
    Music, Sfx, Vibration, Language, ResetSave, ConfirmReset, SaveErased,
    ShiftOver, CoinsEarned, Deliveries, BestStreak, Record, NewRecord, Replay, Menu,
    Tuto1, Tuto2, Tuto3, Tuto4, Tuto5, Tuto6, TutoSkip, TutoDone,
    WindAlert, FogAlert, HailAlert, WaterAlert, Brake,
    Order1, Order2, Order3, Order4, Order5,
    Deliv1, Deliv2, Deliv3, Deliv4,
    Fail1, Fail2, Fail3,
    Hot, TapToContinue, ShopKeeper,
    COUNT
};

void        setLang(Lang l);
Lang        getLang();
const char* tr(Str s);

// Texte ponctuel (dialogues, défis...) sans passer par l'enum.
inline const char* L(const char* fr, const char* en) { return getLang() == Lang::FR ? fr : en; }
