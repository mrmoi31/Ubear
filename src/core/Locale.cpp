#include "Locale.hpp"

static Lang g_lang = Lang::FR;

void setLang(Lang l) { g_lang = l; }
Lang getLang() { return g_lang; }

// L'ordre DOIT suivre l'enum Str.
static const char* FR[] = {
    "UBear", "Livré chaud, ou c'est l'ours qui paie.", "Jouer", "Boutique", "Options", "Retour", "Reprendre",
    "Terminer la journée", "Pause", "Pièces",
    "Récupère la commande au restaurant", "Livre la commande à l'igloo", "Commande récupérée !",
    "Livraison réussie !", "Plat froid : commande annulée", "Série", "Distance", "Pourboire",
    "Équipement", "Cosmétiques", "Acheter", "Équiper", "Équipé", "MAX", "Niv.", "Pas assez de pièces", "Gratuit", "Possédé",
    "Rollers", "Poussées plus puissantes.",
    "Freins", "Débloque un bouton de frein, de plus en plus efficace.",
    "GPS", "Distance, mini-carte, puis cible visible dans le brouillard.",
    "Sac isotherme", "Le plat refroidit plus lentement. Chaque niveau ajoute une commande à la tournée.",
    "Lest", "Réduit l'effet du vent et de la grêle.",
    "Pelage", "Chapeau", "Sac", "Rollers", "Traînée",
    "Aucun", "Original",
    "Polaire", "Brun", "Marron", "Panda", "Doré",
    "Bonnet rouge", "Bonnet vert", "Casquette Ubear", "Casquette livreur", "Haut-de-forme", "Couronne",
    "Sac bleu", "Sac rouge", "Sac vert", "Sac noir", "Sac jaune",
    "Rollers rouges", "Rollers bleus", "Rollers verts", "Rollers noirs", "Rollers jaunes",
    "Neige", "Étincelles", "Arc-en-ciel", "Bulles",
    "Musique", "Effets sonores", "Vibrations", "Langue : Français", "Effacer la sauvegarde", "Confirmer ?", "Sauvegarde effacée",
    "Fin de la journée", "Pièces gagnées", "Livraisons", "Meilleure série", "Record", "Nouveau record !", "Jour suivant", "Menu",
    "Touche la moitié GAUCHE :\nBarnabé tourne à gauche", "Touche la moitié DROITE :\nBarnabé tourne à droite",
    "Touche le coin HAUT-GAUCHE :\npatte avant gauche (tourne plus)", "Touche le coin HAUT-DROIT :\npatte avant droite",
    "Suis la flèche jusqu'au restaurant", "Livre à l'igloo avant que le plat refroidisse !", "Passer", "Tu es prêt. Bon service !",
    "Vent fort : Barnabé est une voile", "Brouillard : on n'y voit rien", "Grêle : ça ralentit", "Dans l'eau... ça rame !", "FREIN",
    "Nouvelle commande : 3 poissons-frites, sans frites.",
    "Nouvelle commande : soupe de krill, « bien chaude svp ».",
    "Nouvelle commande : maki banquise pour le yéti.",
    "Nouvelle commande : le morse veut « la même chose qu'hier ».",
    "Nouvelle commande : pingouin en télétravail, pressé.",
    "Le morse a dit « mouais ». C'est un compliment.", "Le pingouin a laissé un pourboire en coquillages.",
    "Le yéti n'a rien dit. Il a mangé.", "Le renard a filmé la livraison pour ses abonnés.",
    "Le pingouin a commandé une pizza ailleurs.", "Le client refuse un poisson qui a « re-nagé ».", "Le plat est devenu un glaçon.",
    "Chaleur", "Touche pour continuer", "Gaston le morse vend du matériel « reconditionné »",
};

static const char* EN[] = {
    "UBear", "Delivered hot, or the bear pays.", "Play", "Shop", "Settings", "Back", "Resume",
    "End the day", "Pause", "Coins",
    "Pick up the order at the restaurant", "Deliver the order to the igloo", "Order picked up!",
    "Delivered!", "Cold food: order cancelled", "Streak", "Distance", "Tip",
    "Equipment", "Cosmetics", "Buy", "Equip", "Equipped", "MAX", "Lv.", "Not enough coins", "Free", "Owned",
    "Skates", "More powerful pushes.",
    "Brakes", "Unlocks a brake button, stronger each level.",
    "GPS", "Distance, mini-map, then target visible through fog.",
    "Thermal bag", "Food cools down slower. Each level adds one order to a route.",
    "Ballast", "Reduces wind and hail effects.",
    "Fur", "Hat", "Bag", "Skates", "Trail",
    "None", "Original",
    "Polar", "Brown", "Chestnut", "Panda", "Golden",
    "Red beanie", "Green beanie", "Ubear cap", "Courier cap", "Top hat", "Crown",
    "Blue bag", "Red bag", "Green bag", "Black bag", "Yellow bag",
    "Red skates", "Blue skates", "Green skates", "Black skates", "Yellow skates",
    "Snow", "Sparks", "Rainbow", "Bubbles",
    "Music", "Sound FX", "Vibration", "Language: English", "Erase save", "Confirm?", "Save erased",
    "End of the day", "Coins earned", "Deliveries", "Best streak", "Best", "New record!", "Next day", "Menu",
    "Tap the LEFT half:\nBarnaby turns left", "Tap the RIGHT half:\nBarnaby turns right",
    "Tap the TOP-LEFT corner:\nfront left paw (turns more)", "Tap the TOP-RIGHT corner:\nfront right paw",
    "Follow the arrow to the restaurant", "Deliver to the igloo before the food gets cold!", "Skip", "You're ready. Have a good shift!",
    "Strong wind: Barnaby is now a sail", "Fog: can't see a thing", "Hail: slowing down", "In the water... slow going!", "BRAKE",
    "New order: 3 fish & chips, no chips.",
    "New order: krill soup, \"very hot please\".",
    "New order: ice-floe maki for the yeti.",
    "New order: the walrus wants \"same as yesterday\".",
    "New order: remote-working penguin, in a hurry.",
    "The walrus said \"meh\". That's a compliment.", "The penguin tipped in seashells.",
    "The yeti said nothing. He ate.", "The fox filmed the delivery for his followers.",
    "The penguin ordered pizza elsewhere.", "Customer refuses fish that \"swam again\".", "The dish became an ice cube.",
    "Heat", "Tap to continue", "Gaston the walrus sells \"refurbished\" gear",
};

static_assert(sizeof(FR) / sizeof(FR[0]) == (int)Str::COUNT, "FR: nombre de chaînes incorrect");
static_assert(sizeof(EN) / sizeof(EN[0]) == (int)Str::COUNT, "EN: nombre de chaînes incorrect");

const char* tr(Str s)
{
    int i = (int)s;
    if (i < 0 || i >= (int)Str::COUNT) return "?";
    return (g_lang == Lang::FR) ? FR[i] : EN[i];
}
