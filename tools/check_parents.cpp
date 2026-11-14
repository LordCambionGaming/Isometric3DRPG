#include <raylib.h>
#include <raymath.h>
#include <iostream>
#include <vector>

int main() {
    InitWindow(100, 100, "test");
    Model m = LoadModel("assets/models/characters/bases/human_male_base.glb");
    int animCount = 0;
    ModelAnimation* anims = LoadModelAnimations("assets/models/characters/bases/human_male_base.glb", &animCount);

    std::cout << "--- PARENT CHAIN OF BONES ---\n";
    for (int i = 0; i < 10; i++) {
        std::cout << "Bone[" << i << "] '" << m.bones[i].name << "' parent=" << m.bones[i].parent;
        if (m.bones[i].parent >= 0) std::cout << " ('" << m.bones[m.bones[i].parent].name << "')";
        std::cout << "\n";
    }

    UnloadModelAnimations(anims, animCount);
    UnloadModel(m);
    CloseWindow();
    return 0;
}
