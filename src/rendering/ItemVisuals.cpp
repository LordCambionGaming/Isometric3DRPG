#include "rendering/ItemVisuals.h"
#include "assets/AssetManager.h"
#include <raymath.h>
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <string>

namespace {

constexpr int   kIconSize     = 64;      // lato della texture dell'icona
constexpr float kIconMargin   = 1.14f;   // aria intorno all'oggetto nell'icona
constexpr float kGripY        = -0.05f;  // centro del pugno nello spazio dell'osso mano (= GRIP_OFFSET nel generatore)

// Shader delle icone: indipendente dal sistema di luci del gioco (niente ora del giorno), una sola luce fissa.
// kStrength nel fragment shader regola il contrasto: 0 = icone piatte, 1 = molto contrastate.
const char* kIconVS = R"(#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;
uniform mat4 mvp;
uniform mat4 matNormal;
out vec2 fragTexCoord;
out vec4 fragColor;
out vec3 fragNormal;
void main() {
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    fragNormal = normalize(vec3(matNormal * vec4(vertexNormal, 0.0)));
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)";

const char* kIconFS = R"(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
out vec4 finalColor;
const float kStrength = 0.45;
void main() {
    vec4 base = texture(texture0, fragTexCoord) * colDiffuse * fragColor;
    float d = max(dot(normalize(fragNormal), normalize(vec3(-0.35, 0.65, 0.65))), 0.0);
    float l = (1.0 - kStrength) + kStrength * d * 1.25;
    finalColor = vec4(base.rgb * l, base.a);
}
)";

uint32_t HashKey(const char* s) {            // FNV-1a
    uint32_t h = 2166136261u;
    for (; s && *s; ++s) { h ^= static_cast<uint8_t>(*s); h *= 16777619u; }
    return h;
}

// Estensione del modello nella posa corrente (per i modelli animati: i vertici gia' deformati, non la posa di bind).
BoundingBox PosedBounds(const Model& model) {
    BoundingBox bb{ { FLT_MAX, FLT_MAX, FLT_MAX }, { -FLT_MAX, -FLT_MAX, -FLT_MAX } };
    for (int m = 0; m < model.meshCount; ++m) {
        const Mesh& mesh = model.meshes[m];
        const float* v = mesh.animVertices ? mesh.animVertices : mesh.vertices;
        if (!v) continue;
        for (int i = 0; i < mesh.vertexCount; ++i) {
            const float x = v[i * 3], y = v[i * 3 + 1], z = v[i * 3 + 2];
            bb.min.x = std::min(bb.min.x, x); bb.max.x = std::max(bb.max.x, x);
            bb.min.y = std::min(bb.min.y, y); bb.max.y = std::max(bb.max.y, y);
            bb.min.z = std::min(bb.min.z, z); bb.max.z = std::max(bb.max.z, z);
        }
    }
    if (bb.min.x > bb.max.x) bb = BoundingBox{ { -0.1f, -0.1f, -0.1f }, { 0.1f, 0.1f, 0.1f } };
    return bb;
}

float Axis(const Vector3& v, int i) { return i == 0 ? v.x : (i == 1 ? v.y : v.z); }

Vector3 AxisVec(int i, float sign) {
    return Vector3{ i == 0 ? sign : 0.0f, i == 1 ? sign : 0.0f, i == 2 ? sign : 0.0f };
}

// Matrice di rotazione con R * e_k = t_k per le tre coppie di vettori ortonormali (k = A, B, N).
Matrix RotationFromFrames(const Vector3 e[3], const Vector3 t[3]) {
    float r[3][3] = {};
    for (int k = 0; k < 3; ++k) {
        const float tv[3] = { t[k].x, t[k].y, t[k].z };
        const float ev[3] = { e[k].x, e[k].y, e[k].z };
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) r[i][j] += tv[i] * ev[j];
    }
    Matrix m = MatrixIdentity();
    m.m0 = r[0][0]; m.m4 = r[0][1]; m.m8  = r[0][2];
    m.m1 = r[1][0]; m.m5 = r[1][1]; m.m9  = r[1][2];
    m.m2 = r[2][0]; m.m6 = r[2][1]; m.m10 = r[2][2];
    return m;
}

} // namespace

// ---------------------------------------------------------------------------------------------------

ItemVisuals::ItemVisuals(AssetManager* assets, Shader worldShader)
    : m_assets(assets), m_worldShader(worldShader) {
    m_entries.resize(core::AllItems().size());
    m_iconShader = LoadShaderFromMemory(kIconVS, kIconFS);
    m_iconShaderOk = m_iconShader.id != 0;
    if (!m_iconShaderOk) TraceLog(LOG_WARNING, "[ITEMS] icon shader failed to compile: icons use the model's own shader");
}

ItemVisuals::~ItemVisuals() { Unload(); }

void ItemVisuals::Unload() {
    if (m_unloaded) return;
    m_unloaded = true;
    for (Entry& e : m_entries) {
        if (e.isPlaceholder) UnloadModel(e.placeholder);
        if (e.iconRT.id != 0) UnloadRenderTexture(e.iconRT);
        if (e.iconCustom.id != 0) UnloadTexture(e.iconCustom);
        e = Entry{};
    }
    if (m_iconShaderOk) UnloadShader(m_iconShader);
    m_iconShaderOk = false;
}

void ItemVisuals::ApplyShader(Model& m) {
    if (m_worldShader.id == 0) return;
    for (int i = 0; i < m.materialCount; ++i) m.materials[i].shader = m_worldShader;
}

ItemVisuals::Entry* ItemVisuals::Resolve(core::ItemId id) {
    if (m_unloaded || id < 0 || id >= static_cast<core::ItemId>(m_entries.size())) return nullptr;
    Entry& e = m_entries[static_cast<size_t>(id)];
    if (e.resolved) return &e;
    e.resolved = true;
    e.handLocal = MatrixIdentity();

    const core::ItemDef* def = core::GetItem(id);
    if (!def) return &e;

    // 1) modello vero: percorso della tabella, poi il nome con prefisso "weapon_"
    std::vector<std::string> candidates;
    if (def->model && *def->model) candidates.emplace_back(def->model);
    candidates.emplace_back(std::string("models/items/weapon_") + def->key + ".glb");

    std::string foundPath;
    if (m_assets) {
        for (const std::string& path : candidates) {
            Model* m = m_assets->LoadModel(path.c_str());
            if (m && m->meshCount > 0) { e.model = m; foundPath = path; break; }
        }
    }

    if (e.model) {
        ApplyShader(*e.model);
        if (e.model->boneCount > 0) {   // oggetto animato (l'arco): clip 0 = la sua animazione
            e.anims = m_assets->LoadAnimations(foundPath.c_str(), e.animCount);
            if (!e.anims || e.animCount <= 0 || e.anims[0].frameCount <= 0 ||
                !IsModelAnimationValid(*e.model, e.anims[0])) {
                TraceLog(LOG_WARNING, "[ITEMS] '%s': animation of '%s' unusable, rest pose only", def->key, foundPath.c_str());
                e.anims = nullptr;
                e.animCount = 0;
            }
        }
        TraceLog(LOG_INFO, "[ITEMS] '%s': model '%s'%s", def->key, foundPath.c_str(), e.anims ? " (animated)" : "");
    } else {
        MakePlaceholder(e, *def);
    }
    return &e;
}

// Placeholder "a caso" ma stabile: forma e colore vengono dall'hash della chiave dell'oggetto.
void ItemVisuals::MakePlaceholder(Entry& e, const core::ItemDef& def) {
    const uint32_t h = HashKey(def.key);
    Mesh mesh{};
    switch (h % 5) {
        case 0:  mesh = GenMeshCube(0.18f, 0.18f, 0.18f); break;
        case 1:  mesh = GenMeshSphere(0.11f, 14, 14); break;
        case 2:  mesh = GenMeshCylinder(0.07f, 0.26f, 12); break;
        case 3:  mesh = GenMeshCone(0.10f, 0.28f, 12); break;
        default: mesh = GenMeshCube(0.08f, 0.32f, 0.08f); break;
    }
    e.placeholder = LoadModelFromMesh(mesh);
    const Color col = ColorFromHSV(static_cast<float>((h >> 8) % 360), 0.55f, 0.85f);
    e.placeholder.materials[0].maps[MATERIAL_MAP_ALBEDO].color = col;
    ApplyShader(e.placeholder);
    e.model = &e.placeholder;
    e.isPlaceholder = true;
    e.handLocal = MatrixTranslate(0.0f, kGripY, 0.0f);   // al centro del pugno
    TraceLog(LOG_WARNING, "[ITEMS] '%s': no model found (%s) -> placeholder", def.key,
             (def.model && *def.model) ? def.model : "no path in the table");
}

bool ItemVisuals::IsPlaceholder(core::ItemId id) {
    Entry* e = Resolve(id);
    return e && e->isPlaceholder;
}

bool ItemVisuals::UsesLeftHand(core::ItemId id) {
    return core::IsWeapon(id) && core::ResolveWeapon(id).grip == core::WeaponGrip::Bow;
}

// ---------------------------------------------------------------------------------------------------
// Icona

void ItemVisuals::BuildIcon(Entry& e, const core::ItemDef& def) {
    e.iconTried = true;

    // Immagine fornita a mano: vince sul render. Il percorso e' relativo alla working directory o ad assets/.
    if (def.icon && *def.icon) {
        const std::string tries[2] = { def.icon, std::string("assets/") + def.icon };
        for (const std::string& p : tries) {
            if (FileExists(p.c_str())) {
                e.iconCustom = LoadTexture(p.c_str());
                if (e.iconCustom.id != 0) {
                    SetTextureFilter(e.iconCustom, TEXTURE_FILTER_BILINEAR);
                    e.icon = &e.iconCustom;
                    e.iconFlip = false;
                    return;
                }
            }
        }
    }

    if (!e.model) return;

    // Posa di riposo (arco: freccia nascosta, corda a riposo) prima di misurare e disegnare.
    if (e.anims && e.appliedFrame != 0) {
        UpdateModelAnimation(*e.model, e.anims[0], 0);
        e.appliedFrame = 0;
    }

    // Asse lungo A -> diagonale in alto a destra; asse piu' sottile N -> verso la camera (vista di profilo).
    const BoundingBox bb = PosedBounds(*e.model);
    const Vector3 ext = { bb.max.x - bb.min.x, bb.max.y - bb.min.y, bb.max.z - bb.min.z };
    const Vector3 ctr = { (bb.min.x + bb.max.x) * 0.5f, (bb.min.y + bb.max.y) * 0.5f, (bb.min.z + bb.max.z) * 0.5f };
    int A = 0, N = 0;
    for (int i = 1; i < 3; ++i) {
        if (Axis(ext, i) > Axis(ext, A)) A = i;
        if (Axis(ext, i) < Axis(ext, N)) N = i;
    }
    if (A == N) { A = 0; N = 2; }                                  // forma regolare (cubo, sfera): asse qualsiasi
    // La punta e' dal lato dove il modello si allontana dall'origine (impugnatura): spade e pugnali puntano in alto a destra.
    const float signA = (Axis(ctr, A) >= 0.0f) ? 1.0f : -1.0f;
    const Vector3 eA = AxisVec(A, signA);
    const Vector3 eN = AxisVec(N, 1.0f);
    const Vector3 eB = Vector3CrossProduct(eN, eA);
    const float s = 0.70710678f;
    const Vector3 frameSrc[3] = { eA, eB, eN };
    const Vector3 frameDst[3] = { Vector3{ s, s, 0.0f }, Vector3{ -s, s, 0.0f }, Vector3{ 0.0f, 0.0f, 1.0f } };
    const Matrix rot = RotationFromFrames(frameSrc, frameDst);
    const Matrix xf = MatrixMultiply(MatrixTranslate(-ctr.x, -ctr.y, -ctr.z), rot);

    // Dimensione della vista: semi-estensione massima dei vertici del box ruotato.
    float half = 0.01f;
    for (int i = 0; i < 8; ++i) {
        const Vector3 c = { (i & 1) ? bb.max.x : bb.min.x, (i & 2) ? bb.max.y : bb.min.y, (i & 4) ? bb.max.z : bb.min.z };
        const Vector3 p = Vector3Transform(c, xf);
        half = std::max(half, std::max(std::fabs(p.x), std::fabs(p.y)));
    }

    Camera3D cam{};
    cam.position = Vector3{ 0.0f, 0.0f, 10.0f };
    cam.target = Vector3{ 0.0f, 0.0f, 0.0f };
    cam.up = Vector3{ 0.0f, 1.0f, 0.0f };
    cam.fovy = half * 2.0f * kIconMargin;       // proiezione ortografica: fovy = altezza della vista
    cam.projection = CAMERA_ORTHOGRAPHIC;

    e.iconRT = LoadRenderTexture(kIconSize, kIconSize);
    SetTextureFilter(e.iconRT.texture, TEXTURE_FILTER_BILINEAR);

    // Shader delle icone al posto di quello del mondo, solo per questo disegno.
    std::vector<Shader> saved(static_cast<size_t>(e.model->materialCount));
    for (int i = 0; i < e.model->materialCount; ++i) {
        saved[static_cast<size_t>(i)] = e.model->materials[i].shader;
        if (m_iconShaderOk) e.model->materials[i].shader = m_iconShader;
    }
    const Matrix savedTransform = e.model->transform;
    e.model->transform = xf;

    BeginTextureMode(e.iconRT);
    ClearBackground(BLANK);
    BeginMode3D(cam);
    DrawModel(*e.model, Vector3{ 0.0f, 0.0f, 0.0f }, 1.0f, WHITE);
    EndMode3D();
    EndTextureMode();

    e.model->transform = savedTransform;
    for (int i = 0; i < e.model->materialCount; ++i) e.model->materials[i].shader = saved[static_cast<size_t>(i)];

    e.icon = &e.iconRT.texture;
    e.iconFlip = true;
}

const Texture2D* ItemVisuals::GetIcon(core::ItemId id) {
    Entry* e = Resolve(id);
    if (!e) return nullptr;
    if (!e->iconTried) {
        const core::ItemDef* def = core::GetItem(id);
        if (def) BuildIcon(*e, *def);
    }
    return e->icon;
}

void ItemVisuals::DrawIcon(core::ItemId id, Rectangle dest, Color tint) {
    const Texture2D* tex = GetIcon(id);
    if (!tex) return;
    const Entry& e = m_entries[static_cast<size_t>(id)];
    const Rectangle src = { 0.0f, 0.0f, static_cast<float>(tex->width),
                            e.iconFlip ? -static_cast<float>(tex->height) : static_cast<float>(tex->height) };
    DrawTexturePro(*tex, src, dest, Vector2{ 0.0f, 0.0f }, 0.0f, tint);
}

// ---------------------------------------------------------------------------------------------------
// In mano

void ItemVisuals::DrawInHand(core::ItemId id, const Matrix& handToWorld, float clipPhase) {
    Entry* e = Resolve(id);
    if (!e || !e->model) return;

    if (e->anims) {
        int frame = 0;
        if (clipPhase >= 0.0f) {
            frame = static_cast<int>(clipPhase * static_cast<float>(e->anims[0].frameCount));
            frame = std::max(0, std::min(frame, e->anims[0].frameCount - 1));
        }
        if (frame != e->appliedFrame) {
            UpdateModelAnimation(*e->model, e->anims[0], frame);
            e->appliedFrame = frame;
        }
    }
    e->model->transform = MatrixMultiply(e->handLocal, handToWorld);
    DrawModel(*e->model, Vector3{ 0.0f, 0.0f, 0.0f }, 1.0f, WHITE);
    e->model->transform = MatrixIdentity();
}
