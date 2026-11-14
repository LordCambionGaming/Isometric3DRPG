#include <raylib.h>
#include <iostream>

int main() {
    InitWindow(100, 100, "test");
    Model m = LoadModel("assets/models/entities/player_animated.glb");
    int animCount = 0;
    ModelAnimation* anims = LoadModelAnimations("assets/models/entities/player_animated.glb", &animCount);

    std::cout << "player_animated boneCount: " << m.boneCount << "\n";
    for (int i = 0; i < std::min(m.boneCount, 4); i++) {
        Vector3 t = m.bindPose[i].translation;
        Quaternion r = m.bindPose[i].rotation;
        Vector3 s = m.bindPose[i].scale;
        std::cout << "Bone[" << i << "] '" << m.bones[i].name << "' parent=" << m.bones[i].parent << "\n";
        std::cout << "  bindPose T=(" << t.x << ", " << t.y << ", " << t.z << ")\n";
        std::cout << "  bindPose R=(" << r.x << ", " << r.y << ", " << r.z << ", " << r.w << ")\n";
        std::cout << "  bindPose S=(" << s.x << ", " << s.y << ", " << s.z << ")\n";
    }

    if (animCount > 0 && anims[0].frameCount > 0) {
        for (int i = 0; i < std::min(anims[0].boneCount, 4); i++) {
            Vector3 t = anims[0].framePoses[0][i].translation;
            Quaternion r = anims[0].framePoses[0][i].rotation;
            Vector3 s = anims[0].framePoses[0][i].scale;
            std::cout << "Anim 0 Bone[" << i << "] '" << anims[0].bones[i].name << "'\n";
            std::cout << "  framePoses[0] T=(" << t.x << ", " << t.y << ", " << t.z << ")\n";
            std::cout << "  framePoses[0] R=(" << r.x << ", " << r.y << ", " << r.z << ", " << r.w << ")\n";
            std::cout << "  framePoses[0] S=(" << s.x << ", " << s.y << ", " << s.z << ")\n";
        }
    }

    UnloadModelAnimations(anims, animCount);
    UnloadModel(m);
    CloseWindow();
    return 0;
}
