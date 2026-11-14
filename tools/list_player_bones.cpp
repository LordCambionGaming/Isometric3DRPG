#include <raylib.h>
#include <iostream>

int main() {
    InitWindow(100, 100, "test");
    Model m = LoadModel("assets/models/entities/player_animated.glb");
    std::cout << "All 17 bones of player_animated:\n";
    for (int i = 0; i < m.boneCount; i++) {
        std::cout << "  [" << i << "] '" << m.bones[i].name << "' parent=" << m.bones[i].parent << "\n";
    }
    UnloadModel(m);
    CloseWindow();
    return 0;
}
