#include "Presets.h"

namespace swelleq {

namespace {
constexpr auto B = BandType::bell, LS = BandType::lowShelf, HS = BandType::highShelf,
               LC = BandType::lowCut, HC = BandType::highCut, N = BandType::notch;
}

// Réglages de départ : à ajuster à l'oreille selon la source. Les coupes restent douces pour ne rien abîmer.
const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> bank {
        { "", "Neutre", "Tout à plat : double-cliquez sur la courbe pour ajouter une bande.", 0.0f, {} },

        // ---- Voix
        { "Voix", "Rap · présence", "Voix rap devant le beat : nettoie le bas, fait ressortir l'articulation.", 0.0f,
          { { LC, 90, 0, 0.71f, 1 }, { B, 240, -2.5f, 1.2f }, { B, 3200, 2.5f, 0.9f }, { B, 6800, -1.5f, 5.0f }, { HS, 11000, 2.5f, 0.71f } } },
        { "Voix", "Rap · chaud", "Plus de poitrine, aigus adoucis : voix grave, couplets posés.", 0.0f,
          { { LC, 70, 0, 0.71f, 1 }, { B, 160, 1.5f, 0.9f }, { B, 450, -2.0f, 1.4f }, { B, 2500, 1.5f, 0.8f }, { HS, 9000, -1.5f, 0.71f } } },
        { "Voix", "Chant · air", "Chant pop / RnB : de l'air et de la brillance sans dureté.", 0.0f,
          { { LC, 100, 0, 0.71f, 1 }, { B, 300, -2.0f, 1.0f }, { B, 1100, -1.0f, 1.5f }, { B, 4500, 1.5f, 1.0f }, { HS, 12000, 3.5f, 0.6f } } },
        { "Voix", "Chant · clarté", "Voix chantée qui se perd dans le mix : retire la boue, ouvre le haut-médium.", 0.0f,
          { { LC, 110, 0, 0.71f, 1 }, { B, 350, -3.0f, 1.1f }, { B, 2800, 2.0f, 1.0f }, { B, 5500, 1.0f, 2.0f }, { HS, 10000, 2.0f, 0.71f } } },
        { "Voix", "Voix · anti-boue", "Prise de son dans une pièce : retire le grave et le carton.", 0.0f,
          { { LC, 120, 0, 0.71f, 2 }, { B, 220, -3.0f, 1.5f }, { B, 500, -2.5f, 1.8f }, { B, 900, -1.5f, 2.0f } } },
        { "Voix", "Voix · douce", "Calme les sifflantes et les aigus agressifs d'un micro brillant.", 0.0f,
          { { LC, 80, 0, 0.71f, 1 }, { B, 3500, -1.5f, 1.2f }, { B, 7000, -3.0f, 4.0f }, { HS, 12000, -2.0f, 0.71f } } },
        { "Voix", "Ad-libs · radio", "Effet téléphone / radio pour les ad-libs et les voix de fond.", 2.0f,
          { { LC, 450, 0, 0.9f, 2 }, { B, 1500, 4.0f, 1.0f }, { HC, 5000, 0, 0.9f, 2 } } },
        { "Voix", "Chœurs · derrière", "Fait reculer les doublures et les chœurs derrière la voix lead.", 0.0f,
          { { LC, 180, 0, 0.71f, 1 }, { B, 2500, -2.5f, 0.8f }, { HS, 9000, 1.5f, 0.71f }, { HC, 15000, 0, 0.71f, 0 } } },

        // ---- Kick
        { "Kick", "Kick · punch", "Kick qui tape : bas tenu, carton retiré, attaque mise en avant.", 0.0f,
          { { LC, 30, 0, 0.71f, 2 }, { B, 60, 3.0f, 1.2f }, { B, 350, -4.0f, 1.5f }, { B, 4000, 3.0f, 1.0f } } },
        { "Kick", "Kick · click", "Pour que le kick s'entende sur petits haut-parleurs.", 0.0f,
          { { LC, 35, 0, 0.71f, 1 }, { B, 250, -2.5f, 1.4f }, { B, 2500, 3.5f, 1.4f }, { HS, 8000, 1.5f, 0.71f } } },
        { "Kick", "Kick · rond", "Kick avec une 808 : laisse la place au sub, garde le corps.", 0.0f,
          { { LC, 45, 0, 0.71f, 2 }, { B, 90, 2.0f, 1.0f }, { B, 400, -3.0f, 1.2f }, { HC, 10000, 0, 0.71f, 1 } } },

        // ---- 808 & basse
        { "808 & basse", "808 · sub", "Sub plus profond et propre, sans grondement inaudible.", 0.0f,
          { { LC, 28, 0, 0.71f, 3 }, { LS, 60, 2.0f, 0.71f }, { B, 220, -3.0f, 1.0f }, { HC, 6000, 0, 0.71f, 1 } } },
        { "808 & basse", "808 · présence", "Ajoute les harmoniques pour entendre la 808 sur téléphone.", 0.0f,
          { { LC, 30, 0, 0.71f, 2 }, { B, 900, 3.0f, 0.9f }, { B, 2000, 1.5f, 1.2f } } },
        { "808 & basse", "Basse · définie", "Basse électrique ou synthé : bas stable, attaque des notes.", 0.0f,
          { { LC, 35, 0, 0.71f, 2 }, { B, 80, 1.5f, 1.0f }, { B, 300, -2.5f, 1.3f }, { B, 1200, 2.0f, 1.2f }, { HC, 8000, 0, 0.71f, 1 } } },

        // ---- Batterie
        { "Batterie", "Snare · corps", "Caisse claire plus épaisse avec du claquant.", 0.0f,
          { { LC, 90, 0, 0.71f, 1 }, { B, 200, 2.5f, 1.2f }, { B, 700, -2.0f, 1.8f }, { B, 5000, 2.0f, 1.0f } } },
        { "Batterie", "Hi-hats · doux", "Charleston moins agressif, débarrassé du grave inutile.", 0.0f,
          { { LC, 350, 0, 0.71f, 2 }, { B, 6000, -2.0f, 1.5f }, { HS, 12000, -1.5f, 0.71f } } },
        { "Batterie", "Percussions · brillantes", "Percussions, shakers, congas : relief et clarté.", 0.0f,
          { { LC, 150, 0, 0.71f, 1 }, { B, 400, -2.0f, 1.2f }, { HS, 8000, 2.5f, 0.71f } } },

        // ---- Instruments
        { "Instruments", "Guitare acoustique", "Moins de grave de caisse, plus de cordes.", 0.0f,
          { { LC, 90, 0, 0.71f, 1 }, { B, 200, -2.5f, 1.2f }, { B, 3000, 1.5f, 1.0f }, { HS, 10000, 2.0f, 0.71f } } },
        { "Instruments", "Piano · clair", "Piano ou Rhodes qui laisse respirer la voix.", 0.0f,
          { { LC, 60, 0, 0.71f, 1 }, { B, 300, -2.0f, 1.0f }, { B, 2500, -1.5f, 1.0f }, { HS, 9000, 1.5f, 0.71f } } },
        { "Instruments", "Pads · en retrait", "Nappes et cordes : creuse la place de la voix.", 0.0f,
          { { LC, 150, 0, 0.71f, 1 }, { B, 1800, -3.0f, 0.7f }, { HC, 12000, 0, 0.71f, 1 } } },

        // ---- Bus
        { "Bus", "Bus batterie", "Cohésion de la batterie : poids, moins de boue, éclat.", 0.0f,
          { { LC, 25, 0, 0.71f, 1 }, { LS, 80, 1.5f, 0.71f }, { B, 400, -1.5f, 1.0f }, { HS, 10000, 1.5f, 0.71f } } },
        { "Bus", "Beat sous la voix", "Creuse doucement le beat là où vit la voix.", 0.0f,
          { { B, 300, -1.0f, 0.9f }, { B, 2800, -2.5f, 0.7f } } },
        { "Bus", "Bus voix", "Groupe de voix : propre en bas, ouvert en haut.", 0.0f,
          { { LC, 100, 0, 0.71f, 1 }, { B, 400, -1.5f, 1.0f }, { HS, 10000, 2.0f, 0.71f } } },

        // ---- Master
        { "Master", "Master · doux", "Finition légère : sub nettoyé, un peu de poids et d'air.", 0.0f,
          { { LC, 22, 0, 0.71f, 0 }, { LS, 90, 1.0f, 0.71f }, { B, 300, -1.0f, 0.8f }, { HS, 12000, 1.5f, 0.71f } } },
        { "Master", "Master · brillant", "Mix un peu terne : ouvre le haut sans durcir.", 0.0f,
          { { LC, 22, 0, 0.71f, 0 }, { B, 3500, 1.0f, 0.6f }, { HS, 10000, 2.5f, 0.6f } } },
        { "Master", "Master · chaud", "Mix trop brillant : bascule vers le chaud.", 0.0f,
          { { LC, 22, 0, 0.71f, 0 }, { LS, 150, 1.5f, 0.6f }, { B, 4000, -1.0f, 0.7f }, { HS, 9000, -1.5f, 0.6f } } },
        { "Master", "Master · club", "Plus de sub et de kick pour les sound-systems.", 0.0f,
          { { LC, 25, 0, 0.71f, 1 }, { B, 50, 2.0f, 1.0f }, { B, 250, -1.5f, 1.0f }, { HS, 12000, 1.0f, 0.71f } } },

        // ---- Correction
        { "Correction", "Bourdonnement 50 Hz", "Supprime le ronflement secteur (Europe, Afrique) et ses harmoniques.", 0.0f,
          { { N, 50, 0, 12.0f }, { N, 100, 0, 12.0f }, { N, 150, 0, 12.0f }, { N, 200, 0, 12.0f } } },
        { "Correction", "Bourdonnement 60 Hz", "Supprime le ronflement secteur (Amérique) et ses harmoniques.", 0.0f,
          { { N, 60, 0, 12.0f }, { N, 120, 0, 12.0f }, { N, 180, 0, 12.0f }, { N, 240, 0, 12.0f } } },
        { "Correction", "Coupe-bas propre", "Retire seulement le grondement sous 40 Hz.", 0.0f,
          { { LC, 40, 0, 0.71f, 2 } } },
    };
    return bank;
}

} // namespace swelleq
