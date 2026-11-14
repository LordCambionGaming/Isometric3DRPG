#include <raylib.h>
#include <raymath.h>
#include <iostream>
#include <vector>

int main() {
    InitWindow(100, 100, "test");
    Model m = LoadModel("assets/models/characters/bases/human_male_base.glb");
    int animCount = 0;
    ModelAnimation* anims = LoadModelAnimations("assets/models/characters/bases/human_male_base.glb", &animCount);

    std::cout << "Original mesh[2] vertex 0:\n";
    float origV[3] = { m.meshes[2].vertices[0], m.meshes[2].vertices[1], m.meshes[2].vertices[2] };
    std::cout << "  v0 = (" << origV[0] << ", " << origV[1] << ", " << origV[2] << ")\n";

    // How many vertices in mesh 2?
    std::cout << "Mesh 2 vertex count = " << m.meshes[2].vertexCount << "\n";
    for (int i = 0; i < 5; i++) {
        std::cout << "v[" << i << "] = (" << m.meshes[2].vertices[i*3] << ", " << m.meshes[2].vertices[i*3+1] << ", " << m.meshes[2].vertices[i*3+2] << ")\n";
    }

    UnloadModelAnimations(anims, animCount);
    UnloadModel(m);
    CloseWindow();
    return 0;
}
