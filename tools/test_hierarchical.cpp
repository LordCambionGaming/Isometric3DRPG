#include <raylib.h>
#include <raymath.h>
#include <iostream>
#include <vector>

// Transform a Transform struct to Matrix
Matrix TransformToMatrix(const Transform& t) {
    Matrix rot = QuaternionToMatrix(t.rotation);
    Matrix tra = MatrixTranslate(t.translation.x, t.translation.y, t.translation.z);
    Matrix sca = MatrixScale(t.scale.x, t.scale.y, t.scale.z);
    return MatrixMultiply(MatrixMultiply(sca, rot), tra);
}

void UpdateCustomAnimation(Model& model, const ModelAnimation& anim, int frame) {
    frame = frame % anim.frameCount;

    // 1. Compute animated world matrix for each bone via hierarchy!
    std::vector<Matrix> animWorld(model.boneCount);
    for (int b = 0; b < model.boneCount; b++) {
        Matrix local = TransformToMatrix(anim.framePoses[frame][b]);
        int parent = model.bones[b].parent;
        if (parent >= 0) {
            animWorld[b] = MatrixMultiply(local, animWorld[parent]);
        } else {
            animWorld[b] = local;
        }
    }

    // 2. Compute bind world matrix for each bone via hierarchy or model.bindPose
    // Let's see: what if bindWorld is also computed via hierarchy from framePoses[T-pose] or bindPose?
    // In Raylib, bindPose is ALREADY world space!
    std::vector<Matrix> bindWorld(model.boneCount);
    for (int b = 0; b < model.boneCount; b++) {
        bindWorld[b] = TransformToMatrix(model.bindPose[b]);
    }

    // 3. For each mesh, boneMatrix = inv(bindWorld) * animWorld
    for (int m = 0; m < model.meshCount; m++) {
        Mesh& mesh = model.meshes[m];
        if (!mesh.boneMatrices) continue;
        for (int b = 0; b < mesh.boneCount; b++) {
            Matrix invBind = MatrixInvert(bindWorld[b]);
            mesh.boneMatrices[b] = MatrixMultiply(invBind, animWorld[b]);
        }
    }
}

int main() {
    InitWindow(100, 100, "test");
    Model m = LoadModel("assets/models/characters/bases/human_male_base.glb");
    int animCount = 0;
    ModelAnimation* anims = LoadModelAnimations("assets/models/characters/bases/human_male_base.glb", &animCount);

    UpdateCustomAnimation(m, anims[1], 0);

    std::cout << "Custom animation evaluated!\n";

    UnloadModelAnimations(anims, animCount);
    UnloadModel(m);
    CloseWindow();
    return 0;
}
