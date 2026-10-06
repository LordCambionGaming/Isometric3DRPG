#include "ui/DiceGeometry.h"
#include <raymath.h>
#include <algorithm>
#include <cmath>
#include <vector>

namespace ui {

namespace {

// Facce dell'icosaedro (d20): identiche al DiceRoller originale, cosi' il d20 resta com'era.
const int kIcosaFaces[20][3] = {
    {0,11,5},{0,5,1},{0,1,7},{0,7,10},{0,10,11},
    {1,5,9},{5,11,4},{11,10,2},{10,7,6},{7,1,8},
    {3,9,4},{3,4,2},{3,2,6},{3,6,8},{3,8,9},
    {4,9,5},{2,4,11},{6,2,10},{8,6,7},{9,8,1}
};

void NormalizeToUnitRadius(DiceShape& s) {
    float maxLen = 0.0f;
    for (int i = 0; i < s.vertexCount; ++i) maxLen = std::max(maxLen, Vector3Length(s.v[i]));
    if (maxLen <= 0.0f) return;
    for (int i = 0; i < s.vertexCount; ++i) s.v[i] = Vector3Scale(s.v[i], 1.0f / maxLen);
}

Vector3 Centroid(const DiceShape& s, const DiceFace& f) {
    Vector3 c{ 0, 0, 0 };
    for (int k = 0; k < f.count; ++k) c = Vector3Add(c, s.v[f.v[k]]);
    return Vector3Scale(c, 1.0f / (float)f.count);
}

// Dato un faccia con vertici gia' ordinati antiorari e normale impostata: calcola up, contorno 2D, inRadius.
void FinishFace(const DiceShape& s, DiceFace& f, bool upThroughEdge = false) {
    const Vector3 c = Centroid(s, f);
    if (upThroughEdge) {
        // "alto" = verso il punto medio del primo lato: un quadrato resta dritto invece di sembrare un rombo
        const Vector3 mid = Vector3Scale(Vector3Add(s.v[f.v[0]], s.v[f.v[1]]), 0.5f);
        f.up = Vector3Normalize(Vector3Subtract(mid, c));
    } else {
        f.up = Vector3Normalize(Vector3Subtract(s.v[f.v[0]], c));
    }
    const Vector3 right = Vector3CrossProduct(f.up, f.normal);   // destra visto da fuori (x destra, y su, z verso chi guarda)

    Vector2 p[5];
    for (int k = 0; k < f.count; ++k) {
        const Vector3 d = Vector3Subtract(s.v[f.v[k]], c);
        p[k] = { Vector3DotProduct(d, right), Vector3DotProduct(d, f.up) };   // y verso l'alto
    }

    // baricentro dell'area (per il pentagono/triangolo/quadrato regolari coincide con la media dei vertici;
    // per l'aquilone del d10 e' piu' vicino al lato largo, ed e' li' che sta bene il numero)
    float area2 = 0.0f, cx = 0.0f, cy = 0.0f;
    for (int k = 0; k < f.count; ++k) {
        const Vector2 a = p[k], b = p[(k + 1) % f.count];
        const float cr = a.x * b.y - b.x * a.y;
        area2 += cr;
        cx += (a.x + b.x) * cr;
        cy += (a.y + b.y) * cr;
    }
    if (std::fabs(area2) > 1e-9f) {
        cx /= 3.0f * area2;
        cy /= 3.0f * area2;
        for (int k = 0; k < f.count; ++k) { p[k].x -= cx; p[k].y -= cy; }
    }

    float maxD = 0.0f;
    for (int k = 0; k < f.count; ++k) maxD = std::max(maxD, std::sqrt(p[k].x * p[k].x + p[k].y * p[k].y));
    const float sc = (maxD > 1e-9f) ? kFaceFitRadius / maxD : 1.0f;

    f.inRadius = 1e9f;
    for (int k = 0; k < f.count; ++k) {
        f.poly[k] = { p[k].x * sc, -p[k].y * sc };                // coordinate immagine: y verso il basso
        const Vector2 a = { p[k].x * sc, p[k].y * sc };
        const Vector2 b = { p[(k + 1) % f.count].x * sc, p[(k + 1) % f.count].y * sc };
        const float edge = std::sqrt((b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y));
        if (edge > 1e-6f) f.inRadius = std::min(f.inRadius, std::fabs(a.x * b.y - b.x * a.y) / edge);
    }
}

void BuildIcosahedron(DiceShape& s) {
    const float t = (1.0f + std::sqrt(5.0f)) * 0.5f;
    const Vector3 raw[12] = {
        {-1, t, 0}, { 1, t, 0}, {-1,-t, 0}, { 1,-t, 0},
        { 0,-1, t}, { 0, 1, t}, { 0,-1,-t}, { 0, 1,-t},
        { t, 0,-1}, { t, 0, 1}, {-t, 0,-1}, {-t, 0, 1}
    };
    s.vertexCount = 12;
    for (int i = 0; i < 12; ++i) s.v[i] = Vector3Normalize(raw[i]);

    s.faceCount = 20;
    for (int f = 0; f < 20; ++f) {
        DiceFace& F = s.face[f];
        F.count = 3;
        for (int k = 0; k < 3; ++k) F.v[k] = kIcosaFaces[f][k];

        const Vector3 a = s.v[F.v[0]], b = s.v[F.v[1]], c = s.v[F.v[2]];
        const Vector3 n = Vector3Normalize(Vector3Scale(Vector3Add(Vector3Add(a, b), c), 1.0f / 3.0f));

        // garantisce ordine antiorario visto da fuori (serve per non specchiare i numeri)
        const Vector3 cr = Vector3CrossProduct(Vector3Subtract(b, a), Vector3Subtract(c, a));
        if (Vector3DotProduct(cr, n) < 0.0f) std::swap(F.v[1], F.v[2]);

        F.normal = n;
        F.number = 0;
        FinishFace(s, F);
    }
}

// Costruisce le facce di un poliedro convesso centrato nell'origine cercando i piani "di appoggio":
// un piano per tre vertici, con tutti gli altri vertici dallo stesso lato, e' una faccia.
void BuildFromHull(DiceShape& s, bool squaresUpThroughEdge = false) {
    const int n = s.vertexCount;
    std::vector<std::vector<int>> seen;
    s.faceCount = 0;

    for (int i = 0; i < n; ++i)
    for (int j = i + 1; j < n; ++j)
    for (int k = j + 1; k < n; ++k) {
        Vector3 nrm = Vector3CrossProduct(Vector3Subtract(s.v[j], s.v[i]), Vector3Subtract(s.v[k], s.v[i]));
        const float len = Vector3Length(nrm);
        if (len < 1e-6f) continue;
        nrm = Vector3Scale(nrm, 1.0f / len);
        const float d = Vector3DotProduct(nrm, s.v[i]);

        int pos = 0, neg = 0;
        std::vector<int> onPlane;
        for (int m = 0; m < n; ++m) {
            const float e = Vector3DotProduct(nrm, s.v[m]) - d;
            if (e > 1e-4f) ++pos;
            else if (e < -1e-4f) ++neg;
            else onPlane.push_back(m);
        }
        if (pos > 0 && neg > 0) continue;          // non e' un piano di appoggio
        if (onPlane.size() < 3) continue;

        std::vector<int> key = onPlane;
        std::sort(key.begin(), key.end());
        if (std::find(seen.begin(), seen.end(), key) != seen.end()) continue;
        seen.push_back(key);

        if (s.faceCount >= 20 || onPlane.size() > 5) continue;

        const Vector3 outward = (pos > 0) ? Vector3Negate(nrm) : nrm;

        Vector3 c{ 0, 0, 0 };
        for (int m : onPlane) c = Vector3Add(c, s.v[m]);
        c = Vector3Scale(c, 1.0f / (float)onPlane.size());

        // ordina antiorario visto da fuori: assi (a, b) con a x b = outward
        const Vector3 a = Vector3Normalize(Vector3Subtract(s.v[onPlane[0]], c));
        const Vector3 b = Vector3CrossProduct(outward, a);
        std::sort(onPlane.begin(), onPlane.end(), [&](int p, int q) {
            const Vector3 dp = Vector3Subtract(s.v[p], c), dq = Vector3Subtract(s.v[q], c);
            return std::atan2(Vector3DotProduct(dp, b), Vector3DotProduct(dp, a)) <
                   std::atan2(Vector3DotProduct(dq, b), Vector3DotProduct(dq, a));
        });

        // la "punta" (v[0]) e' il vertice piu' vicino al centro del dado: il polo dell'aquilone del d10, che sta
        // sull'asse di simmetria della faccia. Nei solidi regolari i vertici sono tutti uguali e vale il primo.
        int best = 0;
        for (size_t q = 1; q < onPlane.size(); ++q) {
            if (Vector3Length(s.v[onPlane[q]]) < Vector3Length(s.v[onPlane[best]]) - 1e-4f) best = (int)q;
        }
        std::rotate(onPlane.begin(), onPlane.begin() + best, onPlane.end());

        DiceFace& F = s.face[s.faceCount++];
        F = DiceFace{};
        F.count = (int)onPlane.size();
        for (int q = 0; q < F.count; ++q) F.v[q] = onPlane[q];
        F.normal = outward;
        FinishFace(s, F, squaresUpThroughEdge && F.count == 4);
    }
}

void BuildTetrahedron(DiceShape& s) {
    const Vector3 raw[4] = { {1,1,1}, {1,-1,-1}, {-1,1,-1}, {-1,-1,1} };
    s.vertexCount = 4;
    for (int i = 0; i < 4; ++i) s.v[i] = raw[i];
    NormalizeToUnitRadius(s);
    BuildFromHull(s);
}

void BuildCube(DiceShape& s) {
    s.vertexCount = 8;
    int n = 0;
    for (int x = -1; x <= 1; x += 2)
        for (int y = -1; y <= 1; y += 2)
            for (int z = -1; z <= 1; z += 2) s.v[n++] = { (float)x, (float)y, (float)z };
    NormalizeToUnitRadius(s);
    BuildFromHull(s, true);
}

void BuildOctahedron(DiceShape& s) {
    const Vector3 raw[6] = { {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1} };
    s.vertexCount = 6;
    for (int i = 0; i < 6; ++i) s.v[i] = raw[i];
    NormalizeToUnitRadius(s);
    BuildFromHull(s);
}

// Trapezoedro pentagonale: 2 poli + anello di 10 vertici a zig-zag. Le 10 facce sono aquiloni piani.
void BuildTrapezohedron(DiceShape& s) {
    const double pi = 3.14159265358979323846;
    const double h = 0.105;                                   // sfalsamento dell'anello (raggio anello = 1)
    const double s1 = std::sin(pi / 5.0), s2 = std::sin(2.0 * pi / 5.0);
    const double H = h * (2.0 * s1 + s2) / (2.0 * s1 - s2);   // altezza dei poli che rende le facce piane

    s.vertexCount = 12;
    s.v[0] = { 0.0f, 0.0f, (float)H };
    s.v[1] = { 0.0f, 0.0f, (float)-H };
    for (int k = 0; k < 10; ++k) {
        const double a = k * pi / 5.0;
        s.v[2 + k] = { (float)std::cos(a), (float)std::sin(a), (float)((k % 2 == 0) ? h : -h) };
    }
    NormalizeToUnitRadius(s);
    BuildFromHull(s);
}

void BuildDodecahedron(DiceShape& s) {
    const float p = (1.0f + std::sqrt(5.0f)) * 0.5f, q = 1.0f / p;
    s.vertexCount = 20;
    int n = 0;
    for (int x = -1; x <= 1; x += 2)
        for (int y = -1; y <= 1; y += 2)
            for (int z = -1; z <= 1; z += 2) s.v[n++] = { (float)x, (float)y, (float)z };
    for (int a = -1; a <= 1; a += 2)
        for (int b = -1; b <= 1; b += 2) {
            s.v[n++] = { 0.0f, a * q, b * p };
            s.v[n++] = { a * q, b * p, 0.0f };
            s.v[n++] = { b * p, 0.0f, a * q };
        }
    NormalizeToUnitRadius(s);
    BuildFromHull(s);
}

// Numerazione: sui dadi con facce opposte (tutti tranne il d4) le facce opposte sommano sides+1,
// come su un dado vero. Il d4 non ha facce opposte: numeri in ordine.
void Number(DiceShape& s) {
    for (int f = 0; f < s.faceCount; ++f) s.face[f].number = 0;

    if (s.sides == 4) {
        for (int f = 0; f < s.faceCount; ++f) s.face[f].number = f + 1;
    } else {
        int number = 1;
        for (int f = 0; f < s.faceCount; ++f) {
            if (s.face[f].number != 0) continue;
            for (int g = f + 1; g < s.faceCount; ++g) {
                if (s.face[g].number == 0 && Vector3DotProduct(s.face[f].normal, s.face[g].normal) < -0.99f) {
                    s.face[f].number = number;
                    s.face[g].number = s.sides + 1 - number;
                    ++number;
                    break;
                }
            }
        }
    }
    for (int f = 0; f < s.faceCount; ++f) s.faceOfNumber[s.face[f].number] = f;
}

} // namespace

bool IsSupportedDie(int sides) {
    return sides == 4 || sides == 6 || sides == 8 || sides == 10 || sides == 12 || sides == 20;
}

DiceShape BuildDiceShape(int sides) {
    DiceShape s;
    if (!IsSupportedDie(sides)) sides = 20;
    s.sides = sides;
    switch (sides) {
        case 4:  BuildTetrahedron(s);   break;
        case 6:  BuildCube(s);          break;
        case 8:  BuildOctahedron(s);    break;
        case 10: BuildTrapezohedron(s); break;
        case 12: BuildDodecahedron(s);  break;
        default: BuildIcosahedron(s);   break;
    }
    Number(s);
    return s;
}

} // namespace ui