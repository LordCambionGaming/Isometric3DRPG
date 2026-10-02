#include "ui/DiceRoller.h"
#include "ui/GothicTheme.h"
#include <rlgl.h>
#include <raymath.h>
#include <algorithm>
#include <cmath>

namespace ui {

namespace {

constexpr float kRadius        = 46.0f;    // raggio fisico / di presa
constexpr float kDrawScale     = 2.6f;     // lato della texture = kRadius * kDrawScale
constexpr float kMinThrowSpeed = 300.0f;
constexpr float kMaxThrowSpeed = 2600.0f;
constexpr float kFriction      = 2.2f;
constexpr float kBounce        = 0.70f;
constexpr float kStopSpeed     = 60.0f;
constexpr float kAlignSpeed    = 500.0f;   // sotto questa velocita' il dado inizia ad allinearsi
constexpr float kRollFactor    = 0.35f;    // quanto "rotola" rispetto alla velocita'

constexpr int   kCell   = 192;             // cella dell'atlas per faccia
constexpr int   kCols   = 5;
constexpr int   kRows   = 4;
constexpr float kTriR   = 86.0f;           // raggio del triangolo dentro la cella
constexpr int   kRTSize = 384;

const Vector3 kIdleSpin{ 0.35f, 0.8f, 0.15f };

const int kFaces[20][3] = {
    {0,11,5},{0,5,1},{0,1,7},{0,7,10},{0,10,11},
    {1,5,9},{5,11,4},{11,10,2},{10,7,6},{7,1,8},
    {3,9,4},{3,4,2},{3,2,6},{3,6,8},{3,8,9},
    {4,9,5},{2,4,11},{6,2,10},{8,6,7},{9,8,1}
};

float QDot(const Quaternion& a, const Quaternion& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

Quaternion SlerpShortest(Quaternion a, Quaternion b, float t) {
    if (QDot(a, b) < 0.0f) b = { -b.x, -b.y, -b.z, -b.w };
    return QuaternionNormalize(QuaternionSlerp(a, b, t));
}

// Rotolamento: dalla velocita' sullo schermo (y verso il basso) alla velocita' angolare
Vector3 Rolling(Vector2 v) {
    const float k = kRollFactor / kRadius;
    return { v.y * k, v.x * k, 0.0f };
}

// Vertici UV (in 0..1) del triangolo della faccia nell'atlas: 0=apice, 1=basso-sx, 2=basso-dx
Vector2 FaceUV(int face, int k) {
    const float W = (float)(kCell * kCols), H = (float)(kCell * kRows);
    const float cx = (face % kCols) * kCell + kCell / 2.0f;
    const float cy = (face / kCols) * kCell + kCell / 2.0f;
    Vector2 p;
    if (k == 0)      p = { cx, cy - kTriR };
    else if (k == 1) p = { cx - kTriR * 0.866f, cy + kTriR * 0.5f };
    else             p = { cx + kTriR * 0.866f, cy + kTriR * 0.5f };
    return { p.x / W, p.y / H };
}

unsigned char Clamp8(int v) { return (unsigned char)std::max(0, std::min(255, v)); }

void ImageThickLine(Image* img, Vector2 a, Vector2 b, int thick, Color c) {
    const Vector2 d = Vector2Subtract(b, a);
    const float len = Vector2Length(d);
    if (len < 1.0f) return;
    const Vector2 nrm = { -d.y / len, d.x / len };
    for (int i = -thick / 2; i <= thick / 2; ++i) {
        ImageDrawLine(img,
            (int)(a.x + nrm.x * i), (int)(a.y + nrm.y * i),
            (int)(b.x + nrm.x * i), (int)(b.y + nrm.y * i), c);
    }
}

// Linea tratteggiata: i "punti" del ricamo
void ImageDashedLine(Image* img, Vector2 a, Vector2 b, float dash, float gap, int thick, Color c) {
    const Vector2 d = Vector2Subtract(b, a);
    const float len = Vector2Length(d);
    if (len < 1.0f) return;
    const Vector2 dir = Vector2Scale(d, 1.0f / len);
    for (float t = 0.0f; t < len; t += dash + gap) {
        const float e = std::min(t + dash, len);
        ImageThickLine(img, Vector2Add(a, Vector2Scale(dir, t)),
                            Vector2Add(a, Vector2Scale(dir, e)), thick, c);
    }
}

// Punto a croce (cross-stitch)
void ImageCrossStitch(Image* img, Vector2 c, float s, Color col) {
    ImageThickLine(img, { c.x - s, c.y - s }, { c.x + s, c.y + s }, 2, col);
    ImageThickLine(img, { c.x - s, c.y + s }, { c.x + s, c.y - s }, 2, col);
}

} // namespace

// ---------------------------------------------------------------------------

DiceRoller::DiceRoller() {
    BuildGeometry();
    m_orient = QuaternionFromEuler(0.6f, 0.9f, 0.3f);
    m_target = m_orient;
    m_angVel = kIdleSpin;
}

DiceRoller::~DiceRoller() {
    if (m_ready && IsWindowReady()) {
        UnloadTexture(m_atlas);
        UnloadRenderTexture(m_rt);
    }
}

void DiceRoller::BuildGeometry() {
    const float t = (1.0f + std::sqrt(5.0f)) * 0.5f;
    const Vector3 raw[12] = {
        {-1, t, 0}, { 1, t, 0}, {-1,-t, 0}, { 1,-t, 0},
        { 0,-1, t}, { 0, 1, t}, { 0,-1,-t}, { 0, 1,-t},
        { t, 0,-1}, { t, 0, 1}, {-t, 0,-1}, {-t, 0, 1}
    };
    for (int i = 0; i < 12; ++i) m_v[i] = Vector3Normalize(raw[i]);

    for (int f = 0; f < 20; ++f) {
        for (int k = 0; k < 3; ++k) m_f[f][k] = kFaces[f][k];

        const Vector3 a = m_v[m_f[f][0]], b = m_v[m_f[f][1]], c = m_v[m_f[f][2]];
        const Vector3 n = Vector3Normalize(Vector3Scale(Vector3Add(Vector3Add(a, b), c), 1.0f / 3.0f));

        // garantisce ordine antiorario visto da fuori (serve per non specchiare i numeri)
        const Vector3 cr = Vector3CrossProduct(Vector3Subtract(b, a), Vector3Subtract(c, a));
        if (Vector3DotProduct(cr, n) < 0.0f) std::swap(m_f[f][1], m_f[f][2]);

        m_n[f] = n;
        m_number[f] = 0;
    }

    // numerazione: facce opposte sommano 21, come su un d20 vero
    int number = 1;
    for (int f = 0; f < 20; ++f) {
        if (m_number[f] != 0) continue;
        for (int g = f + 1; g < 20; ++g) {
            if (m_number[g] == 0 && Vector3DotProduct(m_n[f], m_n[g]) < -0.99f) {
                m_number[f] = number;
                m_number[g] = 21 - number;
                ++number;
                break;
            }
        }
    }
    for (int f = 0; f < 20; ++f) m_faceOfNumber[m_number[f]] = f;
}

void DiceRoller::BuildAtlas() {
    const Color obsidian = { 32, 29, 40, 255 };   // nero con riflesso viola
    Image img = GenImageColor(kCell * kCols, kCell * kRows, obsidian);
    const Font font = theme::DiceFont();

    const Color thread  = theme::kGold;           // filo d'oro
    const Color threadD = theme::kGoldDim;        // filo piu' scuro

    for (int f = 0; f < 20; ++f) {
        const int ox = (f % kCols) * kCell;
        const int oy = (f / kCols) * kCell;

        // nero, con leggerissima variazione per faccia
        const int var = ((f * 37) % 9) - 4;
        const Color base = { Clamp8(obsidian.r + var), Clamp8(obsidian.g + var),
                             Clamp8(obsidian.b + var), 255 };
        ImageDrawRectangle(&img, ox, oy, kCell, kCell, base);

        const Vector2 c = { ox + kCell / 2.0f, oy + kCell / 2.0f };

        // triangolo scalato attorno al centro della faccia
        auto tri = [&](float s, Vector2 out[3]) {
            const Vector2 v[3] = {
                { c.x, c.y - kTriR },
                { c.x - kTriR * 0.866f, c.y + kTriR * 0.5f },
                { c.x + kTriR * 0.866f, c.y + kTriR * 0.5f }
            };
            for (int i = 0; i < 3; ++i)
                out[i] = Vector2Add(c, Vector2Scale(Vector2Subtract(v[i], c), s));
        };

        Vector2 outer[3], mid[3], inner[3], orn[3];
        tri(1.00f, outer);
        tri(0.88f, mid);
        tri(0.76f, inner);
        tri(0.60f, orn);

        // 1) bordo: cordoncino dorato lungo lo spigolo della faccia
        for (int i = 0; i < 3; ++i) ImageThickLine(&img, outer[i], outer[(i + 1) % 3], 6, threadD);

        // 2) filo continuo sottile
        for (int i = 0; i < 3; ++i) ImageThickLine(&img, mid[i], mid[(i + 1) % 3], 2, thread);

        // 3) punti di cucitura tratteggiati
        for (int i = 0; i < 3; ++i)
            ImageDashedLine(&img, inner[i], inner[(i + 1) % 3], 7.0f, 5.0f, 2, threadD);

        // 4) ornamenti ai vertici: perla con anello
        for (int i = 0; i < 3; ++i) {
            ImageDrawCircle(&img, (int)orn[i].x, (int)orn[i].y, 3, thread);
            ImageDrawCircleLines(&img, (int)orn[i].x, (int)orn[i].y, 7, threadD);
        }

        // 5) punto a croce a meta' di ogni lato, verso l'interno
        for (int i = 0; i < 3; ++i) {
            const Vector2 m = Vector2Scale(Vector2Add(mid[i], mid[(i + 1) % 3]), 0.5f);
            const Vector2 toC = Vector2Normalize(Vector2Subtract(c, m));
            ImageCrossStitch(&img, Vector2Add(m, Vector2Scale(toC, 9.0f)), 3.0f, thread);
        }

        // 6) numero: font gotico, oro con ombra nera
        const int n = m_number[f];
        const char* txt = (n == 6 || n == 9) ? TextFormat("%d.", n) : TextFormat("%d", n);
        const float size = 72.0f;
        const float spacing = size / 14.0f;
        const Vector2 m = MeasureTextEx(font, txt, size, spacing);
        const Vector2 pos = { c.x - m.x / 2.0f, c.y - m.y / 2.0f + 3.0f };
        ImageDrawTextEx(&img, font, txt, { pos.x + 2, pos.y + 2 }, size, spacing, { 0, 0, 0, 255 });
        ImageDrawTextEx(&img, font, txt, pos, size, spacing, theme::kGoldBright);
    }

    m_atlas = LoadTextureFromImage(img);
    UnloadImage(img);
    SetTextureFilter(m_atlas, TEXTURE_FILTER_BILINEAR);
}

void DiceRoller::EnsureResources() {
    if (m_ready) return;
    BuildAtlas();
    m_rt = LoadRenderTexture(kRTSize, kRTSize);
    SetTextureFilter(m_rt.texture, TEXTURE_FILTER_BILINEAR);
    m_ready = true;
}

Vector2 DiceRoller::RestPos() const {
    return { m_bounds.x + m_bounds.width / 2.0f,
             m_bounds.y + m_bounds.height - kRadius - 20.0f };
}

void DiceRoller::Reset() {
    m_state = State::Idle;
    m_pos = RestPos();
    m_vel = { 0, 0 };
    m_angVel = kIdleSpin;
    m_orient = QuaternionFromEuler(0.6f, 0.9f, 0.3f);
    m_target = m_orient;
    m_hasResult = false;
    m_aligned = false;
}

Quaternion DiceRoller::TargetFor(int face) const {
    const Vector3 z = { 0, 0, 1 };
    const Vector3 n = m_n[face];

    // 1) porta la normale della faccia verso la camera (+z)
    const Quaternion q1 = QuaternionFromVector3ToVector3(n, z);

    // 2) ruota attorno a z finche' l'apice del numero punta in alto
    const Vector3 a = m_v[m_f[face][0]];
    Vector3 apex = Vector3Subtract(a, Vector3Scale(n, Vector3DotProduct(a, n)));
    apex = Vector3RotateByQuaternion(apex, q1);
    const float ang = std::atan2(apex.x, apex.y);
    const Quaternion q2 = QuaternionFromAxisAngle(z, ang);

    return QuaternionNormalize(QuaternionMultiply(q2, q1));
}

void DiceRoller::Integrate(float dt) {
    const float w = Vector3Length(m_angVel);
    if (w < 1e-4f) return;
    const Quaternion dq = QuaternionFromAxisAngle(Vector3Scale(m_angVel, 1.0f / w), w * dt);
    m_orient = QuaternionNormalize(QuaternionMultiply(dq, m_orient));
}

// ---------------------------------------------------------------------------

void DiceRoller::HandleInput() {
    const Vector2 mouse = GetMousePosition();
    const float dt = std::max(GetFrameTime(), 0.0001f);

    if (m_state == State::Idle || m_state == State::Settled) {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
            Vector2Distance(mouse, m_pos) <= kRadius * 1.15f) {
            m_state = State::Dragging;
            m_grab = Vector2Subtract(m_pos, mouse);
            m_vel = { 0, 0 };
            m_hasResult = false;
            m_aligned = false;
        }
    }
    else if (m_state == State::Dragging) {
        Vector2 target = Vector2Add(mouse, m_grab);
        target.x = std::max(m_bounds.x + kRadius, std::min(target.x, m_bounds.x + m_bounds.width  - kRadius));
        target.y = std::max(m_bounds.y + kRadius, std::min(target.y, m_bounds.y + m_bounds.height - kRadius));

        const Vector2 instVel = Vector2Scale(Vector2Subtract(target, m_pos), 1.0f / dt);
        m_vel = Vector2Lerp(m_vel, instVel, 0.35f);
        m_pos = target;

        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
            float speed = Vector2Length(m_vel);
            if (speed >= kMinThrowSpeed) {
                if (speed > kMaxThrowSpeed) {
                    m_vel = Vector2Scale(m_vel, kMaxThrowSpeed / speed);
                }
                // tiro VERO deciso qui; la faccia finale lo mostrera'
                m_pending = m_provider ? m_provider() : core::CombatRoll{};
                const int val = std::max(1, std::min(20, m_pending.rollValue));
                m_target = TargetFor(m_faceOfNumber[val]);
                m_state = State::Flying;
            } else {
                m_state = State::Idle;
            }
        }
    }
}

void DiceRoller::Update(float dt) {
    switch (m_state) {
    case State::Idle:
        m_pos = Vector2Lerp(m_pos, RestPos(), std::min(1.0f, dt * 10.0f));
        m_angVel = Vector3Lerp(m_angVel, kIdleSpin, std::min(1.0f, dt * 3.0f));
        Integrate(dt);
        break;

    case State::Dragging: {
        const Vector3 want = Vector3Add(Rolling(m_vel), Vector3Scale(kIdleSpin, 0.5f));
        m_angVel = Vector3Lerp(m_angVel, want, std::min(1.0f, dt * 10.0f));
        Integrate(dt);
        break;
    }

    case State::Flying: {
        m_pos = Vector2Add(m_pos, Vector2Scale(m_vel, dt));
        m_vel = Vector2Scale(m_vel, std::max(0.0f, 1.0f - kFriction * dt));

        const float l = m_bounds.x + kRadius,  r = m_bounds.x + m_bounds.width  - kRadius;
        const float t = m_bounds.y + kRadius,  b = m_bounds.y + m_bounds.height - kRadius;
        if (m_pos.x < l) { m_pos.x = l; m_vel.x = -m_vel.x * kBounce; }
        if (m_pos.x > r) { m_pos.x = r; m_vel.x = -m_vel.x * kBounce; }
        if (m_pos.y < t) { m_pos.y = t; m_vel.y = -m_vel.y * kBounce; }
        if (m_pos.y > b) { m_pos.y = b; m_vel.y = -m_vel.y * kBounce; }

        // il dado rotola in base alla velocita'; quando rallenta si allinea alla faccia del tiro
        const float speed = Vector2Length(m_vel);
        const float k = (speed < kAlignSpeed) ? 1.0f - speed / kAlignSpeed : 0.0f;
        m_angVel = Vector3Scale(Rolling(m_vel), 1.0f - k);
        Integrate(dt);
        if (k > 0.0f) {
            m_orient = SlerpShortest(m_orient, m_target, std::min(1.0f, (1.5f + 10.0f * k * k) * dt));
        }

        if (speed < kStopSpeed) {
            m_state = State::Settled;
            m_vel = { 0, 0 };
            m_settleTime = 0.0f;
        }
        break;
    }

    case State::Settled:
        m_orient = SlerpShortest(m_orient, m_target, std::min(1.0f, 12.0f * dt));
        m_settleTime += dt;
        if (!m_aligned && (std::fabs(QDot(m_orient, m_target)) > 0.9995f || m_settleTime > 1.0f)) {
            m_orient = m_target;
            m_aligned = true;
            m_hasResult = true;
        }
        break;
    }
}

bool DiceRoller::ConsumeResult(core::CombatRoll& out) {
    if (!m_hasResult) return false;
    out = m_pending;
    m_hasResult = false;
    return true;
}

// ---------------------------------------------------------------------------

void DiceRoller::DrawDie3D() {
    Camera3D cam{};
    cam.position = { 0.0f, 0.0f, 4.0f };
    cam.target = { 0.0f, 0.0f, 0.0f };
    cam.up = { 0.0f, 1.0f, 0.0f };
    cam.fovy = 32.0f;
    cam.projection = CAMERA_PERSPECTIVE;

    // luce principale (alto-sinistra) e luce rossastra di riempimento (basso-destra)
    const Vector3 keyDir  = Vector3Normalize({ -0.5f,  0.8f, 0.6f });
    const Vector3 fillDir = Vector3Normalize({  0.7f, -0.5f, 0.4f });

    BeginTextureMode(m_rt);
    ClearBackground(BLANK);
    BeginMode3D(cam);

    rlDisableBackfaceCulling();
    // ATTENZIONE: rlBegin() cambia modo (QUADS -> TRIANGLES) e riporta la texture
    // a quella bianca di default. rlSetTexture va quindi chiamata DOPO rlBegin.
    rlBegin(RL_TRIANGLES);
    rlSetTexture(m_atlas.id);

    for (int f = 0; f < 20; ++f) {
        const Vector3 n = Vector3RotateByQuaternion(m_n[f], m_orient);
        if (n.z < -0.2f) continue;               // faccia sul retro, inutile disegnarla

        const float d1 = std::max(0.0f, Vector3DotProduct(n, keyDir));
        const float d2 = std::max(0.0f, Vector3DotProduct(n, fillDir));
        // luci piu' alte: su un dado nero servono per far leggere volume e oro
        const float cr = 0.55f + 1.10f * d1 + 0.35f * d2;
        const float cg = 0.55f + 1.05f * d1 + 0.10f * d2;
        const float cb = 0.62f + 1.05f * d1 + 0.10f * d2;
        rlColor4ub(Clamp8((int)(cr * 255)), Clamp8((int)(cg * 255)), Clamp8((int)(cb * 255)), 255);

        for (int k = 0; k < 3; ++k) {
            const Vector3 p = Vector3RotateByQuaternion(m_v[m_f[f][k]], m_orient);
            const Vector2 uv = FaceUV(f, k);
            rlTexCoord2f(uv.x, uv.y);
            rlVertex3f(p.x, p.y, p.z);
        }
    }

    rlEnd();
    rlSetTexture(0);
    rlEnableBackfaceCulling();

    EndMode3D();
    EndTextureMode();
}

void DiceRoller::Render() {
    EnsureResources();

    // anello di partenza
    const Vector2 rest = RestPos();
    DrawRing(rest, kRadius + 12.0f, kRadius + 15.0f, 0.0f, 360.0f, 48, Fade(theme::kGold, 0.45f));

    float scale = 1.0f;
    if (m_state == State::Flying) {
        scale += 0.25f * std::min(Vector2Length(m_vel) / 1500.0f, 1.0f);   // finta altezza
    } else if (m_state == State::Dragging) {
        scale = 1.12f;
    }
    const float size = kRadius * kDrawScale * scale;

    // ombra sul feltro
    DrawEllipse((int)(m_pos.x + 8.0f * scale), (int)(m_pos.y + 14.0f * scale),
                kRadius * 0.95f, kRadius * 0.75f, Fade(BLACK, 0.45f));

    DrawDie3D();

    const Rectangle src = { 0.0f, 0.0f, (float)m_rt.texture.width, -(float)m_rt.texture.height };
    const Rectangle dst = { m_pos.x - size / 2.0f, m_pos.y - size / 2.0f, size, size };
    DrawTexturePro(m_rt.texture, src, dst, { 0.0f, 0.0f }, 0.0f, WHITE);
}

} // namespace ui