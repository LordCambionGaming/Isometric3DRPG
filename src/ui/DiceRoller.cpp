#include "ui/DiceRoller.h"
#include "ui/GothicTheme.h"
#include <rlgl.h>
#include <raymath.h>
#include <algorithm>
#include <cmath>

namespace ui {

namespace {

constexpr float kRadius        = 46.0f;    
constexpr float kDrawScale     = 2.6f;     
constexpr float kMinThrowSpeed = 300.0f;
constexpr float kMaxThrowSpeed = 2600.0f;
constexpr float kFriction      = 2.2f;
constexpr float kBounce        = 0.70f;
constexpr float kStopSpeed     = 60.0f;
constexpr float kAlignSpeed    = 500.0f;   
constexpr float kRollFactor    = 0.35f;    

constexpr int   kCell   = 192;             
constexpr float kTriR   = 86.0f;           
constexpr int   kRTSize = 384;

const Vector3 kIdleSpin{ 0.35f, 0.8f, 0.15f };

float QDot(const Quaternion& a, const Quaternion& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

Quaternion SlerpShortest(Quaternion a, Quaternion b, float t) {
    if (QDot(a, b) < 0.0f) b = { -b.x, -b.y, -b.z, -b.w };
    return QuaternionNormalize(QuaternionSlerp(a, b, t));
}

Vector3 Rolling(Vector2 v) {
    const float k = kRollFactor / kRadius;
    return { v.y * k, v.x * k, 0.0f };
}

unsigned char Clamp8(int v) { return (unsigned char)std::max(0, std::min(255, v)); }

void ImageThickLine(Image* img, Vector2 a, Vector2 b, int thick, Color c) {
    const Vector2 d = Vector2Subtract(b, a);
    const float len = Vector2Length(d);
    if (len < 1.0f) return;
    const Vector2 nrm = { -d.y / len, d.x / len };
    for (int i = -thick / 2; i <= thick / 2; ++i) {
        ImageDrawLine(img, (int)(a.x + nrm.x * i), (int)(a.y + nrm.y * i),
                           (int)(b.x + nrm.x * i), (int)(b.y + nrm.y * i), c);
    }
}

void ImageDashedLine(Image* img, Vector2 a, Vector2 b, float dash, float gap, int thick, Color c) {
    const Vector2 d = Vector2Subtract(b, a);
    const float len = Vector2Length(d);
    if (len < 1.0f) return;
    const Vector2 dir = Vector2Scale(d, 1.0f / len);
    for (float t = 0.0f; t < len; t += dash + gap) {
        const float e = std::min(t + dash, len);
        ImageThickLine(img, Vector2Add(a, Vector2Scale(dir, t)), Vector2Add(a, Vector2Scale(dir, e)), thick, c);
    }
}

void ImageCrossStitch(Image* img, Vector2 c, float s, Color col) {
    ImageThickLine(img, { c.x - s, c.y - s }, { c.x + s, c.y + s }, 2, col);
    ImageThickLine(img, { c.x - s, c.y + s }, { c.x + s, c.y - s }, 2, col);
}

} // namespace

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

void DiceRoller::SetSides(int sides) {
    if (m_sides == sides) return;
    m_sides = sides;
    if (m_ready && IsWindowReady()) {
        UnloadTexture(m_atlas);
        m_atlas.id = 0;
    }
    m_ready = false;
    BuildGeometry();
}

void DiceRoller::BuildGeometry() {
    m_v.clear(); 
    m_faces.clear();
    m_faceOfNumber.assign(m_sides + 1, 0);

    if (m_sides == 4) { // D4 (Tetraedro)
        m_v = {{1,1,1}, {1,-1,-1}, {-1,1,-1}, {-1,-1,1}};
        int f[4][3] = {{0,2,1}, {0,1,3}, {0,3,2}, {1,2,3}};
        for(auto& arr : f) m_faces.push_back({{arr[0], arr[1], arr[2]}, {0,0,0}, 0});
    }
    else if (m_sides == 6) { // D6 (Cubo)
        m_v = {{-1,-1,-1}, {1,-1,-1}, {1,1,-1}, {-1,1,-1}, {-1,-1,1}, {1,-1,1}, {1,1,1}, {-1,1,1}};
        int f[6][4] = {{0,3,2,1}, {5,6,7,4}, {1,2,6,5}, {4,7,3,0}, {3,7,6,2}, {4,0,1,5}};
        for(auto& arr : f) m_faces.push_back({{arr[0], arr[1], arr[2], arr[3]}, {0,0,0}, 0});
    }
    else if (m_sides == 8) { // D8 (Ottaedro)
        m_v = {{1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1}};
        int f[8][3] = {{0,4,2}, {2,4,1}, {1,4,3}, {3,4,0}, {0,2,5}, {2,1,5}, {1,3,5}, {3,0,5}};
        for(auto& arr : f) m_faces.push_back({{arr[0], arr[1], arr[2]}, {0,0,0}, 0});
    }
    else if (m_sides == 10) { // D10 (Trapezoedro Pentagonale)
        m_v.push_back({0, 1, 0}); m_v.push_back({0, -1, 0});
        for(int i=0; i<10; ++i) {
            float a = i * PI / 5.0f;
            float y = (i % 2 == 0) ? 0.2f : -0.2f;
            m_v.push_back({std::cos(a), y, std::sin(a)});
        }
        for(int i=0; i<10; ++i) {
            if (i % 2 == 0) m_faces.push_back({{0, 2+i, 2+(i+1)%10, 2+(i+9)%10}, {0,0,0}, 0});
            else m_faces.push_back({{1, 2+i, 2+(i+1)%10, 2+(i+9)%10}, {0,0,0}, 0});
        }
    }
    else if (m_sides == 12) { // D12 (Dodecaedro)
        const float p = 1.6180339887f, q = 1.0f/p;
        m_v = {{-1,-1,-1}, {1,-1,-1}, {1,1,-1}, {-1,1,-1}, {-1,-1,1}, {1,-1,1}, {1,1,1}, {-1,1,1},
               {0,-q,-p}, {0,q,-p}, {0,-q,p}, {0,q,p}, {-p,0,-q}, {p,0,-q}, {-p,0,q}, {p,0,q},
               {-q,-p,0}, {q,-p,0}, {-q,p,0}, {q,p,0}};
        int f[12][5] = {{9,8,1,13,2}, {8,9,3,12,0}, {10,11,7,14,4}, {11,10,5,15,6}, {12,14,7,18,3}, {14,12,0,16,4},
                        {15,13,1,17,5}, {13,15,6,19,2}, {16,17,1,8,0}, {17,16,4,10,5}, {18,19,2,9,3}, {19,18,7,11,6}};
        for(auto& arr : f) m_faces.push_back({{arr[0], arr[1], arr[2], arr[3], arr[4]}, {0,0,0}, 0});
    }
    else { // D20 (Icosaedro) - Default
        m_sides = 20;
        const float t = (1.0f + std::sqrt(5.0f)) * 0.5f;
        m_v = {{-1, t, 0}, { 1, t, 0}, {-1,-t, 0}, { 1,-t, 0}, { 0,-1, t}, { 0, 1, t}, { 0,-1,-t}, { 0, 1,-t}, { t, 0,-1}, { t, 0, 1}, {-t, 0,-1}, {-t, 0, 1}};
        int f[20][3] = {{0,11,5},{0,5,1},{0,1,7},{0,7,10},{0,10,11},{1,5,9},{5,11,4},{11,10,2},{10,7,6},{7,1,8},{3,9,4},{3,4,2},{3,2,6},{3,6,8},{3,8,9},{4,9,5},{2,4,11},{6,2,10},{8,6,7},{9,8,1}};
        for(auto& arr : f) m_faces.push_back({{arr[0], arr[1], arr[2]}, {0,0,0}, 0});
    }

    for(auto& v : m_v) v = Vector3Normalize(v); // normalizza per farlo rotolare fluido

    for (auto& face : m_faces) {
        Vector3 a = m_v[face.v[0]], b = m_v[face.v[1]], c = m_v[face.v[2]];
        Vector3 n = Vector3Normalize(Vector3CrossProduct(Vector3Subtract(b, a), Vector3Subtract(c, a)));
        
        Vector3 center = {0,0,0};
        for(int idx : face.v) center = Vector3Add(center, m_v[idx]);
        
        if (Vector3DotProduct(n, center) < 0) { // assicura orientamento all'esterno
            std::reverse(face.v.begin(), face.v.end());
            n = { -n.x, -n.y, -n.z };
        }
        face.n = n;
    }

    // Numerazione: le facce opposte (se parallele) sommano sides + 1
    int number = 1;
    for (int i = 0; i < m_sides; ++i) {
        if (m_faces[i].number != 0) continue;
        m_faces[i].number = number;
        for (int j = i + 1; j < m_sides; ++j) {
            if (m_faces[j].number == 0 && Vector3DotProduct(m_faces[i].n, m_faces[j].n) < -0.95f) {
                m_faces[j].number = m_sides + 1 - number;
                break;
            }
        }
        number++;
    }
    // Riempi eventuali buchi (es. il D4 non ha facce parallele opposte)
    number = 1;
    for (int i = 0; i < m_sides; ++i) {
        if (m_faces[i].number == 0) {
            while(std::find_if(m_faces.begin(), m_faces.end(), [&](const Polygon& p){return p.number == number;}) != m_faces.end()) number++;
            m_faces[i].number = number++;
        }
        m_faceOfNumber[m_faces[i].number] = i;
    }
}

Vector2 DiceRoller::FaceUV(int face, int k) const {
    int vCount = m_faces[face].v.size();
    const float W = (float)(kCell * m_atlasCols), H = (float)(kCell * m_atlasRows);
    const float cx = (face % m_atlasCols) * kCell + kCell / 2.0f;
    const float cy = (face / m_atlasCols) * kCell + kCell / 2.0f;
    float a = k * (2.0f * PI / vCount) - PI / 2.0f;
    Vector2 p = { cx + std::cos(a) * kTriR, cy + std::sin(a) * kTriR };
    return { p.x / W, p.y / H };
}

void DiceRoller::BuildAtlas() {
    m_atlasCols = std::ceil(std::sqrt(m_sides));
    m_atlasRows = std::ceil((float)m_sides / m_atlasCols);

    const Color obsidian = { 32, 29, 40, 255 };
    Image img = GenImageColor(kCell * m_atlasCols, kCell * m_atlasRows, obsidian);
    const Font font = theme::DiceFont();

    const Color thread  = theme::kGold;           
    const Color threadD = theme::kGoldDim;        

    for (int f = 0; f < m_sides; ++f) {
        const int ox = (f % m_atlasCols) * kCell;
        const int oy = (f / m_atlasCols) * kCell;
        const int var = ((f * 37) % 9) - 4;
        const Color base = { Clamp8(obsidian.r + var), Clamp8(obsidian.g + var), Clamp8(obsidian.b + var), 255 };
        ImageDrawRectangle(&img, ox, oy, kCell, kCell, base);

        const Vector2 c = { ox + kCell / 2.0f, oy + kCell / 2.0f };
        int vCount = m_faces[f].v.size();

        auto poly = [&](float s, std::vector<Vector2>& out) {
            for(int i = 0; i < vCount; ++i) {
                float a = i * (2.0f * PI / vCount) - PI / 2.0f;
                out.push_back({ c.x + std::cos(a) * kTriR * s, c.y + std::sin(a) * kTriR * s });
            }
        };

        std::vector<Vector2> outer, mid, inner, orn;
        poly(1.00f, outer); poly(0.88f, mid); poly(0.76f, inner); poly(0.60f, orn);

        for (int i = 0; i < vCount; ++i) {
            int next = (i + 1) % vCount;
            ImageThickLine(&img, outer[i], outer[next], 6, threadD);
            ImageThickLine(&img, mid[i], mid[next], 2, thread);
            ImageDashedLine(&img, inner[i], inner[next], 7.0f, 5.0f, 2, threadD);

            ImageDrawCircle(&img, (int)orn[i].x, (int)orn[i].y, 3, thread);
            ImageDrawCircleLines(&img, (int)orn[i].x, (int)orn[i].y, 7, threadD);

            const Vector2 m = Vector2Scale(Vector2Add(mid[i], mid[next]), 0.5f);
            const Vector2 toC = Vector2Normalize(Vector2Subtract(c, m));
            ImageCrossStitch(&img, Vector2Add(m, Vector2Scale(toC, 9.0f)), 3.0f, thread);
        }

        const int n = m_faces[f].number;
        const char* txt = (n == 6 || n == 9) ? TextFormat("%d.", n) : TextFormat("%d", n);
        const float size = 72.0f;
        const float spacing = size / 14.0f;
        const Vector2 mText = MeasureTextEx(font, txt, size, spacing);
        const Vector2 pos = { c.x - mText.x / 2.0f, c.y - mText.y / 2.0f + 3.0f };
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
    if (m_rt.id == 0) m_rt = LoadRenderTexture(kRTSize, kRTSize);
    SetTextureFilter(m_rt.texture, TEXTURE_FILTER_BILINEAR);
    m_ready = true;
}

Vector2 DiceRoller::RestPos() const {
    return { m_bounds.x + m_bounds.width / 2.0f, m_bounds.y + m_bounds.height - kRadius - 20.0f };
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
    const Vector3 n = m_faces[face].n;
    const Quaternion q1 = QuaternionFromVector3ToVector3(n, z);

    const Vector3 a = m_v[m_faces[face].v[0]];
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

void DiceRoller::HandleInput() {
    const Vector2 mouse = GetMousePosition();
    const float dt = std::max(GetFrameTime(), 0.0001f);

    if (m_state == State::Idle || m_state == State::Settled) {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && Vector2Distance(mouse, m_pos) <= kRadius * 1.15f) {
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
                if (speed > kMaxThrowSpeed) m_vel = Vector2Scale(m_vel, kMaxThrowSpeed / speed);
                
                m_pending = m_provider ? m_provider() : core::CombatRoll{};
                const int val = std::max(1, std::min(m_sides, m_pending.rollValue));
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

void DiceRoller::DrawDie3D() {
    Camera3D cam{};
    cam.position = { 0.0f, 0.0f, 4.0f };
    cam.target = { 0.0f, 0.0f, 0.0f };
    cam.up = { 0.0f, 1.0f, 0.0f };
    cam.fovy = 32.0f;
    cam.projection = CAMERA_PERSPECTIVE;

    const Vector3 keyDir  = Vector3Normalize({ -0.5f,  0.8f, 0.6f });
    const Vector3 fillDir = Vector3Normalize({  0.7f, -0.5f, 0.4f });

    BeginTextureMode(m_rt);
    ClearBackground(BLANK);
    BeginMode3D(cam);

    rlDisableBackfaceCulling();
    rlBegin(RL_TRIANGLES);
    rlSetTexture(m_atlas.id);

    for (size_t f = 0; f < m_faces.size(); ++f) {
        const Vector3 n = Vector3RotateByQuaternion(m_faces[f].n, m_orient);
        if (n.z < -0.2f) continue; 

        const float d1 = std::max(0.0f, Vector3DotProduct(n, keyDir));
        const float d2 = std::max(0.0f, Vector3DotProduct(n, fillDir));
        const float cr = 0.55f + 1.10f * d1 + 0.35f * d2;
        const float cg = 0.55f + 1.05f * d1 + 0.10f * d2;
        const float cb = 0.62f + 1.05f * d1 + 0.10f * d2;
        rlColor4ub(Clamp8((int)(cr * 255)), Clamp8((int)(cg * 255)), Clamp8((int)(cb * 255)), 255);

        // Triangolazione a ventaglio valida per QUALSIASI poligono (quadrato, pentagono...)
        for (size_t k = 1; k < m_faces[f].v.size() - 1; ++k) {
            Vector2 uv0 = FaceUV(f, 0);
            Vector3 p0 = Vector3RotateByQuaternion(m_v[m_faces[f].v[0]], m_orient);
            rlTexCoord2f(uv0.x, uv0.y); rlVertex3f(p0.x, p0.y, p0.z);

            Vector2 uvK = FaceUV(f, k);
            Vector3 pK = Vector3RotateByQuaternion(m_v[m_faces[f].v[k]], m_orient);
            rlTexCoord2f(uvK.x, uvK.y); rlVertex3f(pK.x, pK.y, pK.z);

            Vector2 uvK1 = FaceUV(f, k+1);
            Vector3 pK1 = Vector3RotateByQuaternion(m_v[m_faces[f].v[k+1]], m_orient);
            rlTexCoord2f(uvK1.x, uvK1.y); rlVertex3f(pK1.x, pK1.y, pK1.z);
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

    const Vector2 rest = RestPos();
    DrawRing(rest, kRadius + 12.0f, kRadius + 15.0f, 0.0f, 360.0f, 48, Fade(theme::kGold, 0.45f));

    float scale = 1.0f;
    if (m_state == State::Flying) {
        scale += 0.25f * std::min(Vector2Length(m_vel) / 1500.0f, 1.0f);
    } else if (m_state == State::Dragging) {
        scale = 1.12f;
    }
    const float size = kRadius * kDrawScale * scale;

    DrawEllipse((int)(m_pos.x + 8.0f * scale), (int)(m_pos.y + 14.0f * scale),
                kRadius * 0.95f, kRadius * 0.75f, Fade(BLACK, 0.45f));

    DrawDie3D();

    const Rectangle src = { 0.0f, 0.0f, (float)m_rt.texture.width, -(float)m_rt.texture.height };
    const Rectangle dst = { m_pos.x - size / 2.0f, m_pos.y - size / 2.0f, size, size };
    DrawTexturePro(m_rt.texture, src, dst, { 0.0f, 0.0f }, 0.0f, WHITE);
}

} // namespace ui