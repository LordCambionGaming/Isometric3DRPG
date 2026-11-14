#include <raylib.h>
#include <iostream>

int main() {
    InitWindow(100, 100, "test");
    Model m = LoadModel("assets/models/villagers/blacksmith.glb");
    int animCount = 0;
    ModelAnimation* anims = LoadModelAnimations("assets/models/villagers/blacksmith.glb", &animCount);

    std::cout << "blacksmith boneCount: " << m.boneCount << "\n";
    for (int i = 0; i < std::min(m.boneCount, 4); i++) {
        std::cout << "Bone[" << i << "] '" << m.bones[i].name << "' parent=" << m.bones[i].parent << "\n";
    }

    std::cout << "AnimCount: " << animCount << "\n";
    if (animCount > 0) {
        for (int i = 0; i < animCount; i++) {
            std::cout << "  anim[" << i << "] name='" << anims[i].name << "' frames=" << anims[i].frameCount << "\n";
        }
    }

    UnloadModelAnimations(anims, animCount);
    UnloadModel(m);
    CloseWindow();
    return 0;
}
