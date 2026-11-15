#include "Assets.hpp"
#include "../game/Catalog.hpp"

#include <vector>

// Les calques (chapeaux, sacs, rollers) sont surtout transparents.
// Les mipmaps les effacent sur certains GPU mobiles, donc on les charge nets.
static Texture2D loadSmooth(const char* path, bool mipmaps = true)
{
    Texture2D t = LoadTexture(path);
    if (t.id != 0) {
        if (mipmaps) {
            GenTextureMipmaps(&t);
            SetTextureFilter(t, TEXTURE_FILTER_TRILINEAR);
        } else {
            SetTextureFilter(t, TEXTURE_FILTER_BILINEAR);
        }
    } else {
        TraceLog(LOG_WARNING, "ASSETS: texture introuvable: %s", path);
    }
    return t;
}

void Assets::load()
{
    body       = loadSmooth("Body.png");
    arrow      = loadSmooth("Fleche.png");
    restaurant = loadSmooth("igloos/hub.png");
    igloo      = loadSmooth("igloos/base.png");
    static const char* HOME_FILES[CUSTOMER_COUNT] = {
        "igloos/pingouin.png", "igloos/morse.png", "igloos/renard.png",
        "igloos/yeti.png", "igloos/phoques.png",
    };
    for (int i = 0; i < CUSTOMER_COUNT; i++) homes[i] = loadSmooth(HOME_FILES[i]);
    logo       = loadSmooth("Logo.png");
    wind       = loadSmooth("Wind.png");
    rain       = loadSmooth("Rain.png");
    fog[0]     = loadSmooth("Fog1.png");
    fog[1]     = loadSmooth("Fog2.png");
    fog[2]     = loadSmooth("Fog3.png");
    particle   = loadSmooth("Particle.png");
    iceTex     = loadSmooth("BanquiseBlanche.png");
    blizzard   = loadSmooth("Blizzard.png");

    Image lightImg = GenImageGradientRadial(256, 256, 0.35f, Color{255, 255, 255, 0}, Color{255, 255, 255, 255});
    light = LoadTextureFromImage(lightImg);
    UnloadImage(lightImg);
    SetTextureFilter(light, TEXTURE_FILTER_BILINEAR);

    // Police : ASCII + Latin-1 (accents) + quelques signes typographiques.
    std::vector<int> cps;
    for (int c = 32; c < 127; c++) cps.push_back(c);
    for (int c = 160; c < 256; c++) cps.push_back(c);
    cps.push_back(0x2019); // ’
    cps.push_back(0x20AC); // €
    cps.push_back(0x2026); // …
    cps.push_back(0x2014); // —
    cps.push_back(0x2022); // •
    font = LoadFontEx("Rushon_Ground.ttf", 64, cps.data(), (int)cps.size());
    if (font.texture.id == 0) font = GetFontDefault();
    else SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);

    bodyImage_ = LoadImage("Body.png");
    for (int i = 0; i < COSMETIC_COUNT; i++) {
        if (!COSMETICS[i].file) continue;
        Image img = LoadImage(COSMETICS[i].file);
        if (!img.data) { TraceLog(LOG_WARNING, "ASSETS: image introuvable: %s", COSMETICS[i].file); continue; }
        // Tous les calques sont étirés sur le corps : on les ramène à sa taille dès le
        // chargement (sacs/rollers en 1207x1464 → ~100 Mo de RAM sinon sur mobile).
        ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
        if (bodyImage_.data && (img.width != bodyImage_.width || img.height != bodyImage_.height))
            ImageResize(&img, bodyImage_.width, bodyImage_.height);
        images_.emplace(COSMETICS[i].file, img);
    }

    if (!IsAudioDeviceReady()) InitAudioDevice();
    audioReady = IsAudioDeviceReady();
    if (audioReady) {
        music = LoadMusicStream("Music.mp3");
        music.looping = true;
        coin = LoadSound("Coin.mp3");
        zip  = LoadSound("Zip.mp3");
    }
}

const Image* Assets::findImage(const std::string& file) const
{
    if (file.empty()) return bodyImage_.data ? &bodyImage_ : nullptr;
    auto it = images_.find(file);
    if (it == images_.end() || !it->second.data) return nullptr;
    return &it->second;
}

Texture2D Assets::compose(const std::string& baseFile, const std::string& skate,
                          const std::string& bag, const std::string& hat)
{
    const Image* base = findImage(baseFile);
    if (!base) base = findImage("");
    if (!base) return {};
    Image canvas = ImageCopy(*base);
    for (const std::string& layer : {skate, bag, hat}) {
        const Image* ov = layer.empty() ? nullptr : findImage(layer);
        if (!ov) continue;
        ImageDraw(&canvas, *ov, {0, 0, (float)ov->width, (float)ov->height},
                  {0, 0, (float)canvas.width, (float)canvas.height}, WHITE);
    }
    Texture2D tex = LoadTextureFromImage(canvas);
    if (tex.id) {
        GenTextureMipmaps(&tex);
        SetTextureFilter(tex, TEXTURE_FILTER_TRILINEAR);
        SetTextureWrap(tex, TEXTURE_WRAP_CLAMP);
    }
    UnloadImage(canvas);
    return tex;
}

static Texture2D iconFromOverlay(const Image& img)
{
    Rectangle b = GetImageAlphaBorder(img, 0.08f);
    if (b.width < 2.0f || b.height < 2.0f)
        b = {0, 0, (float)img.width, (float)img.height};
    Image crop = ImageFromImage(img, b);
    Texture2D tex = LoadTextureFromImage(crop);
    UnloadImage(crop);
    if (tex.id) SetTextureFilter(tex, TEXTURE_FILTER_BILINEAR);
    return tex;
}

static std::string equippedFile(const SaveData& s, CosCat cat)
{
    const CosmeticDef* d = findCosmetic(s.equipped[cat]);
    if (!d || !d->file) return {};
    return d->file;
}

void Assets::syncLooks(const SaveData& s)
{
    if (looks_.empty()) {
        looks_.emplace("fur|", compose("", "", "", ""));
        for (int i = 0; i < COSMETIC_COUNT; i++) {
            const CosmeticDef& c = COSMETICS[i];
            if (c.cat == COS_SKIN) {
                std::string file = c.file ? c.file : "";
                looks_.emplace("fur|" + file, compose(file, "", "", ""));
            } else if (c.file) {
                const Image* img = findImage(c.file);
                if (img) looks_.emplace("icon|" + std::string(c.file), iconFromOverlay(*img));
            }
        }
    }

    std::string skin = equippedFile(s, COS_SKIN);
    std::string skate = equippedFile(s, COS_SKATE);
    std::string bag = equippedFile(s, COS_BAG);
    if (bag.empty()) bag = "cosmetics/bags/bag_bleu.png";
    std::string hat = equippedFile(s, COS_HAT);
    std::string key = skin + "|" + skate + "|" + bag + "|" + hat;
    if (key != gameplayKey_ || gameplay_.id == 0) {
        if (gameplay_.id) UnloadTexture(gameplay_);
        gameplay_ = compose(skin, skate, bag, hat);
        gameplayKey_ = key;
    }
}

void Assets::syncShopPreview(const SaveData& s)
{
    std::string skin = equippedFile(s, COS_SKIN);
    std::string skate = equippedFile(s, COS_SKATE);
    std::string bag = equippedFile(s, COS_BAG);
    std::string hat = equippedFile(s, COS_HAT);
    std::string key = "shop|" + skin + "|" + skate + "|" + bag + "|" + hat;
    if (key == shopPreviewKey_ && shopPreview_.id != 0) return;
    if (shopPreview_.id) UnloadTexture(shopPreview_);
    shopPreview_ = compose(skin, skate, bag, hat);
    shopPreviewKey_ = key;
}

const Texture2D* Assets::look(const std::string& key) const
{
    auto it = looks_.find(key);
    if (it == looks_.end() || it->second.id == 0) return nullptr;
    return &it->second;
}

void Assets::unload()
{
    for (auto& kv : cosmetics_) UnloadTexture(kv.second);
    cosmetics_.clear();
    for (auto& kv : looks_) UnloadTexture(kv.second);
    looks_.clear();
    if (gameplay_.id) UnloadTexture(gameplay_);
    gameplay_ = {};
    gameplayKey_.clear();
    if (shopPreview_.id) UnloadTexture(shopPreview_);
    shopPreview_ = {};
    shopPreviewKey_.clear();
    for (auto& kv : images_) UnloadImage(kv.second);
    images_.clear();
    if (bodyImage_.data) UnloadImage(bodyImage_);
    bodyImage_ = {};

    UnloadTexture(body); UnloadTexture(arrow);
    UnloadTexture(restaurant); UnloadTexture(igloo); UnloadTexture(logo);
    for (auto& h : homes) if (h.id) UnloadTexture(h);
    UnloadTexture(wind); UnloadTexture(rain); UnloadTexture(particle); UnloadTexture(iceTex);
    UnloadTexture(blizzard); UnloadTexture(light);
    for (auto& f : fog) UnloadTexture(f);
    if (font.texture.id != GetFontDefault().texture.id) UnloadFont(font);
    if (audioReady) {
        UnloadMusicStream(music);
        UnloadSound(coin);
        UnloadSound(zip);
        CloseAudioDevice();
    }
}

const Texture2D& Assets::homeFor(int customer) const
{
    if (customer >= 0 && customer < CUSTOMER_COUNT && homes[customer].id) return homes[customer];
    return igloo;
}

const Texture2D* Assets::cosmetic(const std::string& file)
{
    if (file.empty()) return nullptr;
    auto it = cosmetics_.find(file);
    if (it != cosmetics_.end()) return &it->second;
    Texture2D t = loadSmooth(file.c_str(), false);
    if (t.id == 0) return nullptr;
    auto res = cosmetics_.emplace(file, t);
    return &res.first->second;
}

void Assets::playSfx(const Sound& s, float volume, float pitch) const
{
    if (!audioReady || sfxVolume <= 0.001f) return;
    SetSoundVolume(s, volume * sfxVolume);
    SetSoundPitch(s, pitch);
    PlaySound(s);
}
