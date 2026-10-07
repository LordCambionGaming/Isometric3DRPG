#pragma once
#include <raylib.h>
#include <vector>
#include "core/Items.h"

class AssetManager;

// Lato visivo del catalogo oggetti (core/Items.h): modello 3D, icona dell'inventario e disegno in mano.
//
// Tutto e' guidato dalla tabella degli oggetti, nessun codice per singola arma:
//   * il modello si cerca in ItemDef::model ("models/items/<key>.glb"), poi in "models/items/weapon_<key>.glb";
//   * se il file non c'e' (o non si carica) l'oggetto riceve un PLACEHOLDER: una forma semplice, con forma e
//     colore ricavati dalla chiave dell'oggetto (sempre uguali tra una partita e l'altra);
//   * l'icona e' lo stesso modello renderizzato una volta sola in una texture, di profilo e a 45 gradi come
//     in Minecraft. Se ItemDef::icon punta a un'immagine esistente, vince quella.
// Aggiungere un'arma = una riga in Items.cpp + il file .glb nella cartella. Nient'altro.
//
// Convenzione dei modelli (vedi generate_weapons.py): i vertici sono nello spazio dell'osso della mano
// (origine = giunto Hand, impugnatura nel pugno): si agganciano all'osso con trasformazione identita'.
// I modelli rigged (l'arco) hanno la clip 0 sincronizzata con la clip del giocatore.
//
// Tutte le chiamate che toccano OpenGL (GetIcon, DrawIcon, DrawInHand) vanno fatte a finestra aperta e, per le
// icone, FUORI da BeginMode3D/BeginTextureMode.
class ItemVisuals {
public:
    ItemVisuals(AssetManager* assets, Shader worldShader);
    ~ItemVisuals();
    ItemVisuals(const ItemVisuals&) = delete;
    ItemVisuals& operator=(const ItemVisuals&) = delete;

    // Libera texture, placeholder e shader. Idempotente. Va chiamata PRIMA di chiudere la finestra.
    void Unload();

    // true se per questo oggetto non e' stato trovato il modello e si usa un placeholder.
    bool IsPlaceholder(core::ItemId id);

    // Icona (generata al primo uso). nullptr solo per un id non valido.
    const Texture2D* GetIcon(core::ItemId id);
    // Disegna l'icona dentro dest (2D). Non fa nulla per un id non valido.
    void DrawIcon(core::ItemId id, Rectangle dest, Color tint = WHITE);

    // Disegna l'oggetto in 3D. handToWorld porta lo spazio dell'osso della mano nel mondo.
    // clipPhase: 0..1 = fase della clip dell'oggetto (arco: stessa fase della clip Bow del giocatore),
    //            < 0 = posa di riposo (frame 0).
    void DrawInHand(core::ItemId id, const Matrix& handToWorld, float clipPhase = -1.0f);

    // true per gli oggetti che si tengono sempre nella mano sinistra (archi).
    static bool UsesLeftHand(core::ItemId id);

private:
    struct Entry {
        bool resolved = false;
        bool isPlaceholder = false;
        Model* model = nullptr;          // reale: di proprieta' di AssetManager. Placeholder: &placeholder
        Model placeholder{};             // valido solo se isPlaceholder
        Matrix handLocal{};              // trasformazione locale prima di quella della mano
        ModelAnimation* anims = nullptr; // non posseduto; solo i modelli con ossa e animazioni
        int animCount = 0;
        int appliedFrame = -1;
        bool iconTried = false;
        bool iconFlip = false;           // le render texture sono capovolte in verticale
        RenderTexture2D iconRT{};
        Texture2D iconCustom{};
        const Texture2D* icon = nullptr;
    };

    Entry* Resolve(core::ItemId id);
    void MakePlaceholder(Entry& e, const core::ItemDef& def);
    void BuildIcon(Entry& e, const core::ItemDef& def);
    void ApplyShader(Model& m);

    AssetManager* m_assets = nullptr;
    Shader m_worldShader{};
    Shader m_iconShader{};
    bool m_iconShaderOk = false;
    bool m_unloaded = false;
    std::vector<Entry> m_entries;        // una per oggetto, dimensione fissa: i puntatori restano validi
};
