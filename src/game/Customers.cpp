#include "Customers.hpp"
#include "../core/Config.hpp"
#include "../core/Locale.hpp"
#include "../core/Save.hpp"
#include "../core/Ui.hpp"

#include <cmath>

static const CustomerDef DEFS[CUSTOMER_COUNT] = {
    {"Pénélope", "Penelope",
     "Pingouine en télétravail. Toujours pressée, toujours généreuse.",
     "Remote-working penguin. Always in a hurry, always generous.",
     Color{70, 90, 140, 255}, 1.25f, 0.85f, 1.0f, 3.0f},
    {"Gérard", "Gerard",
     "Morse retraité. Patient. Pourboires modestes. « Mouais. »",
     "Retired walrus. Patient. Modest tips. \"Meh.\"",
     Color{150, 100, 70, 255}, 0.8f, 1.3f, 1.0f, 3.0f},
    {"Roxane", "Roxane",
     "Renarde polaire influenceuse. Adore la vitesse (et les vues).",
     "Arctic fox influencer. Loves speed (and views).",
     Color{220, 120, 170, 255}, 1.0f, 1.0f, 2.2f, 3.0f},
    {"Le Yéti", "The Yeti",
     "Commande toujours la même chose. Ne laisse jamais de pourboire. Jamais ?",
     "Always orders the same thing. Never tips. Never?",
     Color{110, 150, 190, 255}, 1.5f, 1.1f, 1.0f, 1.6f},
    {"Les jumeaux", "The twins",
     "Lulu et Lili, bébés phoques. Commandent en cachette de leurs parents.",
     "Lulu and Lili, baby seals. Order behind their parents' back.",
     Color{120, 130, 150, 255}, 1.1f, 0.95f, 1.0f, 2.2f},
};

// [client][palier] — FR puis EN
static const char* ORDER[CUSTOMER_COUNT][customers::TIER_COUNT][2] = {
    {{"Un poisson-frites, vite : j'ai une visio dans dix minutes.", "Fish & chips, quick: I have a video call in ten minutes."},
     {"Le même que d'habitude. Micro coupé, je mange.", "The usual. Mic muted, I'm eating."},
     {"Mon chef croit que je suis en réunion. Soupe de krill, svp.", "My boss thinks I'm in a meeting. Krill soup, please."},
     {"Barnabé, j'ai mis ton nom dans mon rapport trimestriel.", "Barnaby, I put your name in my quarterly report."},
     {"Toute l'équipe veut te voir en visio. Apporte le maki !", "The whole team wants to see you on camera. Bring the maki!"}},
    {{"La même chose qu'hier. Et qu'avant-hier.", "Same as yesterday. And the day before."},
     {"Soupe de krill. Pas trop chaude, pas trop froide. Tiède.", "Krill soup. Not too hot, not too cold. Lukewarm."},
     {"Poisson-frites. Dans mon temps, on livrait à la nage.", "Fish & chips. In my day, we delivered by swimming."},
     {"Je t'ai gardé une place sur mon banc. Apporte deux soupes.", "Saved you a spot on my bench. Bring two soups."},
     {"Mon petit Barnabé, ta soupe de krill habituelle... pour moi.", "Little Barnaby, your usual krill soup... for me."}},
    {{"Coucou ! Un maki banquise, je filme l'unboxing.", "Hi! One ice-floe maki, I'm filming the unboxing."},
     {"Mes abonnés te réclament. Poisson-frites en direct !", "My followers want you. Fish & chips, live!"},
     {"Collab ? Livre-moi un krill et je te tague.", "Collab? Deliver me krill and I'll tag you."},
     {"Ta dernière livraison a fait un million de vues !", "Your last delivery hit a million views!"},
     {"On lance la chaîne « Barnabé & Roxane ». Premier épisode : maki.", "We're launching \"Barnaby & Roxane\". Episode one: maki."}},
    {{"Maki banquise.", "Ice-floe maki."},
     {"Maki banquise. Comme d'habitude.", "Ice-floe maki. As usual."},
     {"... Maki banquise.", "... Ice-floe maki."},
     {"Maki banquise. S'il te plaît.", "Ice-floe maki. Please."},
     {"Maki banquise, Barnabé. Prends ton temps. Enfin, pas trop.", "Ice-floe maki, Barnaby. Take your time. Well, not too much."}},
    {{"Chuuut ! Deux poissons-frites, papa dort.", "Shhh! Two fish & chips, dad's asleep."},
     {"On a cassé la tirelire. Soupe de krill pour deux !", "We broke the piggy bank. Krill soup for two!"},
     {"Lulu dit que tu es plus rapide qu'un orque.", "Lulu says you're faster than an orca."},
     {"On t'a dessiné ! Viens chercher le dessin (et livre le maki).", "We drew you! Come get the drawing (and bring the maki)."},
     {"Tonton Barnabé ! Maman dit que tu peux rester dîner.", "Uncle Barnaby! Mom says you can stay for dinner."}},
};

static const char* THANKS[CUSTOMER_COUNT][customers::TIER_COUNT][2] = {
    {{"Parfait, la caméra était coupée.", "Perfect, my camera was off."},
     {"Tu sauves ma journée de réunions.", "You're saving my meeting-filled day."},
     {"J'ai réservé ton créneau dans mon agenda.", "I booked your slot in my calendar."},
     {"Si tu cherches un poste, on recrute.", "If you're looking for a job, we're hiring."},
     {"Meilleur collègue que je n'ai jamais eu.", "Best colleague I've never had."}},
    {{"Mouais. C'est un compliment.", "Meh. That's a compliment."},
     {"Mouais mouais. C'est deux compliments.", "Meh meh. That's two compliments."},
     {"Pas mal. Ne le répète à personne.", "Not bad. Don't tell anyone."},
     {"Tu me rappelles moi, en plus poilu.", "You remind me of me, but hairier."},
     {"Tu es le petit-fils que je n'ai pas eu.", "You're the grandson I never had."}},
    {{"Trop mignon ! Je poste ça.", "So cute! Posting it."},
     {"Tu as fait réagir mes abonnés.", "My followers reacted to you."},
     {"Tagué, liké, partagé.", "Tagged, liked, shared."},
     {"Ta livraison est en tendance.", "Your delivery is trending."},
     {"Abonné à vie. Enfin, moi à toi.", "Subscribed for life. Me to you, I mean."}},
    {{"...", "..."},
     {"Hm.", "Hm."},
     {"Hmm. Bon.", "Hmm. Fine."},
     {"Le maki était correct. Pour un ours.", "The maki was fine. For a bear."},
     {"Merci, Barnabé.", "Thank you, Barnaby."}},
    {{"Merci ! Tu dis rien à papa, hein ?", "Thanks! Don't tell dad, okay?"},
     {"Lili veut un autographe.", "Lili wants an autograph."},
     {"On a gardé une frite pour toi. Elle est froide.", "We saved you a fry. It's cold."},
     {"Plus tard, on sera livreurs comme toi !", "When we grow up, we'll be couriers like you!"},
     {"Bisous de Lulu ! Et de Lili ! Et de maman !", "Kisses from Lulu! And Lili! And mom!"}},
};

static const char* FAIL[CUSTOMER_COUNT][2] = {
    {"Trop tard, la réunion a commencé. Je commande ailleurs.", "Too late, the meeting started. Ordering elsewhere."},
    {"Froid. Comme mon cœur. Comme mes nageoires.", "Cold. Like my heart. Like my flippers."},
    {"Contenu annulé. Mes abonnés n'aiment pas le poisson froid.", "Content cancelled. My followers hate cold fish."},
    {"Grrr.", "Grrr."},
    {"Papa s'est réveillé ! On annule, on annule !", "Dad woke up! Cancel, cancel!"},
};

static const char* TIER_UP[CUSTOMER_COUNT][customers::TIER_COUNT][2] = {
    {{"", ""},
     {"Pénélope t'a ajouté à ses contacts favoris.", "Penelope added you to her favourite contacts."},
     {"Pénélope a programmé une réunion récurrente « Barnabé ».", "Penelope set up a recurring \"Barnaby\" meeting."},
     {"Pénélope t'envoie des emojis poisson.", "Penelope sends you fish emojis."},
     {"Pénélope t'a nommé employé du mois. Elle ne travaille pas chez Ubear.", "Penelope named you employee of the month. She doesn't work at Ubear."}},
    {{"", ""},
     {"Gérard connaît ton prénom. Il ne l'utilise pas.", "Gerard knows your name. He doesn't use it."},
     {"Gérard t'a fait un signe de nageoire.", "Gerard waved a flipper at you."},
     {"Gérard t'a raconté sa jeunesse. Deux heures.", "Gerard told you about his youth. Two hours."},
     {"Gérard t'appelle « mon petit ». Les autres morses sont jaloux.", "Gerard calls you \"kiddo\". The other walruses are jealous."}},
    {{"", ""},
     {"Roxane te suit sur tous ses réseaux.", "Roxane follows you on every network."},
     {"Roxane a créé le hashtag #BarnabéFonce.", "Roxane created the hashtag #BarnabyRushes."},
     {"Roxane te propose un partenariat (payé en likes).", "Roxane offers you a partnership (paid in likes)."},
     {"Roxane et toi : le duo le plus vu de la banquise.", "You and Roxane: the most-watched duo on the ice."}},
    {{"", ""},
     {"Le yéti a grogné un peu moins fort.", "The yeti grunted a bit less loudly."},
     {"Le yéti a hoché la tête. Une fois.", "The yeti nodded. Once."},
     {"Le yéti a presque souri. Presque.", "The yeti almost smiled. Almost."},
     {"Le yéti t'a serré la patte. Elle est encore gelée.", "The yeti shook your paw. It's still frozen."}},
    {{"", ""},
     {"Les jumeaux t'attendent à la fenêtre.", "The twins wait for you at the window."},
     {"Les jumeaux ont nommé leur doudou « Barnabé ».", "The twins named their teddy \"Barnaby\"."},
     {"Les parents des jumeaux sont au courant. Ils approuvent.", "The twins' parents found out. They approve."},
     {"Tu es officiellement « tonton Barnabé ».", "You're officially \"Uncle Barnaby\"."}},
};

static const int THRESHOLDS[customers::TIER_COUNT] = {0, 3, 10, 25, cfg::YETI_PATIENCE};

const CustomerDef& customers::def(int id) { return DEFS[id]; }
const char* customers::name(int id) { return L(DEFS[id].nameFr, DEFS[id].nameEn); }
const char* customers::desc(int id) { return L(DEFS[id].descFr, DEFS[id].descEn); }

int customers::tier(int n)
{
    int t = 0;
    for (int i = 0; i < TIER_COUNT; i++) if (n >= THRESHOLDS[i]) t = i;
    return t;
}

int customers::tierThreshold(int t) { return THRESHOLDS[t < 0 ? 0 : (t >= TIER_COUNT ? TIER_COUNT - 1 : t)]; }

const char* customers::tierName(int t)
{
    static const char* fr[] = {"Inconnu", "Connaissance", "Habitué", "Ami", "Meilleur ami"};
    static const char* en[] = {"Stranger", "Acquaintance", "Regular", "Friend", "Best friend"};
    return L(fr[t], en[t]);
}

const char* customers::orderLine(int id, int t)  { return L(ORDER[id][t][0], ORDER[id][t][1]); }
const char* customers::thanksLine(int id, int t) { return L(THANKS[id][t][0], THANKS[id][t][1]); }
const char* customers::failLine(int id)          { return L(FAIL[id][0], FAIL[id][1]); }
const char* customers::tierUpLine(int id, int t) { return L(TIER_UP[id][t][0], TIER_UP[id][t][1]); }

const char* customers::yetiBigTipLine()
{
    return L("Cinquante livraisons. Tiens. Garde la monnaie. ... Ne le dis à personne.",
             "Fifty deliveries. Here. Keep the change. ... Don't tell anyone.");
}

float customers::tipMult(int id, int before)
{
    if (id == CU_YETI && before + 1 < cfg::YETI_PATIENCE) return 0.0f;
    return DEFS[id].tipMult;
}

// ---------------------------------------------------------------------------
// Portraits
// ---------------------------------------------------------------------------

static void eyes(Vector2 c, float r, float dx, float dy, float size, float time)
{
    float blink = std::fmod(time + c.x * 0.013f, 4.0f) < 0.12f ? 0.2f : 1.0f;
    for (int s = -1; s <= 1; s += 2) {
        Vector2 e = {c.x + s * dx * r, c.y + dy * r};
        DrawEllipse((int)e.x, (int)e.y, size * r, size * r * blink, Color{20, 20, 28, 255});
        if (blink > 0.5f) DrawCircleV({e.x + size * r * 0.3f, e.y - size * r * 0.3f}, size * r * 0.35f, WHITE);
    }
}

static void drawPenguin(Vector2 c, float r, float t)
{
    DrawCircleV(c, r * 0.78f, Color{32, 36, 48, 255});
    DrawEllipse((int)c.x, (int)(c.y + r * 0.12f), r * 0.55f, r * 0.5f, WHITE);
    eyes(c, r, 0.22f, -0.05f, 0.09f, t);
    DrawTriangle({c.x - r * 0.14f, c.y + r * 0.12f}, {c.x, c.y + r * 0.34f}, {c.x + r * 0.14f, c.y + r * 0.12f}, Color{250, 160, 40, 255});
    // casque de télétravail
    DrawRing(c, r * 0.8f, r * 0.9f, 200, 340, 24, Color{70, 70, 80, 255});
    DrawCircleV({c.x - r * 0.8f, c.y}, r * 0.14f, Color{90, 90, 100, 255});
    DrawCircleV({c.x + r * 0.8f, c.y}, r * 0.14f, Color{90, 90, 100, 255});
    DrawLineEx({c.x + r * 0.8f, c.y + r * 0.1f}, {c.x + r * 0.35f, c.y + r * 0.45f}, r * 0.05f, Color{90, 90, 100, 255});
}

static void drawWalrus(Vector2 c, float r, float t, bool shopkeeper)
{
    Color skin = shopkeeper ? Color{120, 90, 70, 255} : Color{160, 110, 80, 255};
    DrawCircleV(c, r * 0.8f, skin);
    eyes(c, r, 0.25f, -0.25f, 0.08f, t);
    DrawCircleV({c.x - r * 0.18f, c.y + r * 0.12f}, r * 0.24f, Color{215, 175, 140, 255});
    DrawCircleV({c.x + r * 0.18f, c.y + r * 0.12f}, r * 0.24f, Color{215, 175, 140, 255});
    DrawCircleV({c.x, c.y - r * 0.02f}, r * 0.1f, Color{60, 40, 30, 255});
    DrawTriangle({c.x - r * 0.28f, c.y + r * 0.3f}, {c.x - r * 0.2f, c.y + r * 0.78f}, {c.x - r * 0.12f, c.y + r * 0.3f}, Color{250, 245, 225, 255});
    DrawTriangle({c.x + r * 0.12f, c.y + r * 0.3f}, {c.x + r * 0.2f, c.y + r * 0.78f}, {c.x + r * 0.28f, c.y + r * 0.3f}, Color{250, 245, 225, 255});
    for (int i = 0; i < 3; i++) {
        DrawCircleV({c.x - r * (0.12f + i * 0.08f), c.y + r * 0.1f + i * r * 0.05f}, r * 0.025f, Color{90, 60, 40, 255});
        DrawCircleV({c.x + r * (0.12f + i * 0.08f), c.y + r * 0.1f + i * r * 0.05f}, r * 0.025f, Color{90, 60, 40, 255});
    }
    if (shopkeeper) {
        // casquette de commerçant
        DrawRectangleRounded({c.x - r * 0.5f, c.y - r * 0.85f, r * 1.0f, r * 0.35f}, 0.5f, 6, Color{40, 120, 80, 255});
        DrawRectangleRounded({c.x - r * 0.65f, c.y - r * 0.56f, r * 1.3f, r * 0.12f}, 0.5f, 6, Color{30, 90, 60, 255});
    }
}

static void drawFox(Vector2 c, float r, float t)
{
    Color fur = {245, 245, 250, 255};
    DrawTriangle({c.x - r * 0.7f, c.y - r * 0.2f}, {c.x - r * 0.55f, c.y - r * 0.95f}, {c.x - r * 0.15f, c.y - r * 0.55f}, fur);
    DrawTriangle({c.x + r * 0.15f, c.y - r * 0.55f}, {c.x + r * 0.55f, c.y - r * 0.95f}, {c.x + r * 0.7f, c.y - r * 0.2f}, fur);
    DrawTriangle({c.x - r * 0.58f, c.y - r * 0.3f}, {c.x - r * 0.52f, c.y - r * 0.75f}, {c.x - r * 0.28f, c.y - r * 0.5f}, Color{250, 180, 200, 255});
    DrawTriangle({c.x + r * 0.28f, c.y - r * 0.5f}, {c.x + r * 0.52f, c.y - r * 0.75f}, {c.x + r * 0.58f, c.y - r * 0.3f}, Color{250, 180, 200, 255});
    DrawCircleV(c, r * 0.62f, fur);
    DrawTriangle({c.x - r * 0.35f, c.y + r * 0.1f}, {c.x, c.y + r * 0.7f}, {c.x + r * 0.35f, c.y + r * 0.1f}, fur);
    eyes(c, r, 0.24f, -0.05f, 0.08f, t);
    DrawCircleV({c.x, c.y + r * 0.42f}, r * 0.08f, Color{30, 30, 30, 255});
    // téléphone qui filme
    float bob = std::sin(t * 3.0f) * r * 0.03f;
    DrawRectangleRounded({c.x + r * 0.45f, c.y + r * 0.2f + bob, r * 0.28f, r * 0.45f}, 0.3f, 4, Color{40, 40, 50, 255});
    DrawCircleV({c.x + r * 0.59f, c.y + r * 0.3f + bob}, r * 0.05f, Color{255, 70, 70, 255});
}

static void drawYeti(Vector2 c, float r, float t)
{
    Color fur = {235, 240, 248, 255};
    for (int i = 0; i < 10; i++) {
        float a = i * 0.628f;
        DrawCircleV({c.x + std::cos(a) * r * 0.62f, c.y + std::sin(a) * r * 0.62f}, r * 0.26f, fur);
    }
    DrawCircleV(c, r * 0.65f, fur);
    DrawEllipse((int)c.x, (int)(c.y + r * 0.08f), r * 0.42f, r * 0.38f, Color{120, 150, 185, 255});
    eyes(c, r, 0.17f, -0.02f, 0.07f, t);
    // sourcils froncés
    DrawLineEx({c.x - r * 0.3f, c.y - r * 0.2f}, {c.x - r * 0.08f, c.y - r * 0.1f}, r * 0.06f, Color{40, 50, 70, 255});
    DrawLineEx({c.x + r * 0.3f, c.y - r * 0.2f}, {c.x + r * 0.08f, c.y - r * 0.1f}, r * 0.06f, Color{40, 50, 70, 255});
    DrawLineEx({c.x - r * 0.14f, c.y + r * 0.28f}, {c.x + r * 0.14f, c.y + r * 0.28f}, r * 0.05f, Color{40, 50, 70, 255});
}

static void drawSealHead(Vector2 c, float r, float t)
{
    DrawCircleV(c, r, Color{170, 180, 195, 255});
    DrawCircleV({c.x, c.y + r * 0.3f}, r * 0.45f, Color{205, 212, 222, 255});
    eyes(c, r, 0.35f, -0.1f, 0.18f, t);
    DrawEllipse((int)c.x, (int)(c.y + r * 0.2f), r * 0.14f, r * 0.1f, Color{40, 40, 50, 255});
}

static void drawBearFace(Vector2 c, float r, float t)
{
    Color fur = {248, 248, 252, 255};
    DrawCircleV({c.x - r * 0.55f, c.y - r * 0.55f}, r * 0.25f, fur);
    DrawCircleV({c.x + r * 0.55f, c.y - r * 0.55f}, r * 0.25f, fur);
    DrawCircleV(c, r * 0.75f, fur);
    eyes(c, r, 0.25f, -0.12f, 0.08f, t);
    DrawEllipse((int)c.x, (int)(c.y + r * 0.22f), r * 0.28f, r * 0.2f, Color{230, 230, 236, 255});
    DrawEllipse((int)c.x, (int)(c.y + r * 0.14f), r * 0.12f, r * 0.08f, Color{30, 30, 36, 255});
}

void customers::drawPortrait(int who, Vector2 c, float r, float t)
{
    Color bg = who < CUSTOMER_COUNT ? DEFS[who].color
             : who == PORTRAIT_GASTON ? Color{60, 140, 100, 255}
             : Color{58, 128, 214, 255};
    DrawCircleV(c, r, bg);
    DrawCircleV(c, r * 0.92f, shade(bg, 1.18f));
    switch (who) {
    case CU_PENELOPE: drawPenguin(c, r, t); break;
    case CU_GERARD:   drawWalrus(c, r, t, false); break;
    case CU_ROXANE:   drawFox(c, r, t); break;
    case CU_YETI:     drawYeti(c, r, t); break;
    case CU_JUMEAUX:
        drawSealHead({c.x - r * 0.36f, c.y + r * 0.05f}, r * 0.42f, t);
        drawSealHead({c.x + r * 0.36f, c.y + r * 0.05f}, r * 0.42f, t + 1.7f);
        break;
    case PORTRAIT_GASTON: drawWalrus(c, r, t, true); break;
    default: drawBearFace(c, r, t); break;
    }
}
