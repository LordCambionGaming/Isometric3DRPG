#pragma once
#include <raylib.h>

namespace ui {

// Solo matematica (nessuna chiamata a raylib/OpenGL): geometria dei dadi poliedrici.
// Tutti i dadi sono costruiti con la stessa convenzione, cosi' DiceRoller li disegna tutti allo stesso modo.

// Ogni faccia sta in una cella dell'atlas: il vertice piu' lontano dal centro cade a questa distanza (pixel).
constexpr float kFaceFitRadius = 86.0f;

struct DiceFace {
    int     count = 0;                       // vertici della faccia: 3 (d4/d8/d20), 4 (d6/d10), 5 (d12)
    int     v[5] = { 0, 0, 0, 0, 0 };        // indici dei vertici, antiorari visti da fuori; v[0] e' la "punta"
    Vector3 normal{ 0, 0, 1 };               // normale uscente (unitaria)
    Vector3 up{ 0, 1, 0 };                   // direzione "alto" del numero, nel piano della faccia (centro -> v[0])
    Vector2 poly[5] = {};                    // contorno 2D in pixel dell'atlas rispetto al centro cella (y verso il basso)
    float   inRadius = 0.0f;                 // distanza dal centro al lato piu' vicino (per dimensionare il numero)
    int     number = 0;                      // numero scritto sulla faccia (1..sides)
};

struct DiceShape {
    int      sides = 0;
    int      vertexCount = 0;
    Vector3  v[20] = {};                     // vertici, raggio massimo = 1
    int      faceCount = 0;
    DiceFace face[20];
    int      faceOfNumber[21] = {};          // numero -> indice faccia
};

bool IsSupportedDie(int sides);              // 4, 6, 8, 10, 12, 20
DiceShape BuildDiceShape(int sides);         // sides non supportato -> d20

} // namespace ui