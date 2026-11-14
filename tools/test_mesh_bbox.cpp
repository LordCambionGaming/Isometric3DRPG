#include <raylib.h>
#include <iostream>

int main() {
    InitWindow(100, 100, "test");
    Model m = LoadModel("assets/models/characters/bases/human_male_base.glb");
    BoundingBox bb = GetMeshBoundingBox(m.meshes[2]);
    std::cout << "Mesh 2 (basemesh_male) bbox:\n";
    std::cout << "  min = (" << bb.min.x << ", " << bb.min.y << ", " << bb.min.z << ")\n";
    std::cout << "  max = (" << bb.max.x << ", " << bb.max.y << ", " << bb.max.z << ")\n";
    std::cout << "  size = (" << (bb.max.x - bb.min.x) << ", " << (bb.max.y - bb.min.y) << ", " << (bb.max.z - bb.min.z) << ")\n";

    UnloadModel(m);
    CloseWindow();
    return 0;
}
