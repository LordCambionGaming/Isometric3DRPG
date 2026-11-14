#include <raylib.h>
#include <raymath.h>
#include <iostream>
#include <vector>

// Let's implement correct hierarchical animation evaluation:
// In glTF:
// For each bone:
//   localMatrix = TRS(framePose.t, framePose.r, framePose.s)
//   worldMatrix = parent ? (parentWorldMatrix * localMatrix) : localMatrix
//
// Then boneMatrix = worldMatrix * inverseBindMatrix
//
// Let's verify this!
int main() {
    InitWindow(100, 100, "test");
    Model m = LoadModel("assets/models/characters/bases/human_male_base.glb");
    int animCount = 0;
    ModelAnimation* anims = LoadModelAnimations("assets/models/characters/bases/human_male_base.glb", &animCount);

    // Let's inspect inverseBindMatrices from GLB!
    // Wait, does Raylib store inverseBindMatrices anywhere?
    // Let's check raylib structs!

    UnloadModelAnimations(anims, animCount);
    UnloadModel(m);
    CloseWindow();
    return 0;
}
