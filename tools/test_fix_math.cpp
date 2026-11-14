#include <raylib.h>
#include <raymath.h>
#include <iostream>
#include <vector>

void FixMixamoModelAndAnimations(Model& model, ModelAnimation* anims, int animCount) {
    Matrix invRig = MatrixMultiply(MatrixScale(100.0f, 100.0f, 100.0f), MatrixRotateX(-90.0f * DEG2RAD));

    for (int m = 0; m < model.meshCount; m++) {
        Mesh& mesh = model.meshes[m];
        for (int v = 0; v < mesh.vertexCount; v++) {
            Vector3 pos = { mesh.vertices[v*3], mesh.vertices[v*3+1], mesh.vertices[v*3+2] };
            pos = Vector3Transform(pos, invRig);
            mesh.vertices[v*3] = pos.x;
            mesh.vertices[v*3+1] = pos.y;
            mesh.vertices[v*3+2] = pos.z;
        }
        UpdateMeshBuffer(mesh, 0, mesh.vertices, mesh.vertexCount * 3 * sizeof(float), 0);
    }

    // 2. Transform bindPose by invRig
    Quaternion invRot = QuaternionFromAxisAngle(Vector3{ 1.0f, 0.0f, 0.0f }, -90.0f * DEG2RAD);
    for (int b = 0; b < model.boneCount; b++) {
        model.bindPose[b].translation = Vector3Transform(model.bindPose[b].translation, invRig);
        model.bindPose[b].rotation = QuaternionMultiply(invRot, model.bindPose[b].rotation);
        model.bindPose[b].scale = Vector3{ 1.0f, 1.0f, 1.0f };
    }
}

int main() {
    InitWindow(100, 100, "test");
    Model m = LoadModel("assets/models/characters/bases/human_male_base.glb");
    int animCount = 0;
    ModelAnimation* anims = LoadModelAnimations("assets/models/characters/bases/human_male_base.glb", &animCount);

    FixMixamoModelAndAnimations(m, anims, animCount);

    UpdateModelAnimation(m, anims[1], 0);

    // Let's check animVertices of mesh 2!
    BoundingBox animBB = { 0 };
    animBB.min = Vector3{ m.meshes[2].animVertices[0], m.meshes[2].animVertices[1], m.meshes[2].animVertices[2] };
    animBB.max = animBB.min;
    for (int v = 1; v < m.meshes[2].vertexCount; v++) {
        Vector3 p = { m.meshes[2].animVertices[v*3], m.meshes[2].animVertices[v*3+1], m.meshes[2].animVertices[v*3+2] };
        animBB.min = Vector3Min(animBB.min, p);
        animBB.max = Vector3Max(animBB.max, p);
    }

    std::cout << "ANIMATED Mesh 2 BBOX:\n";
    std::cout << "  min = (" << animBB.min.x << ", " << animBB.min.y << ", " << animBB.min.z << ")\n";
    std::cout << "  max = (" << animBB.max.x << ", " << animBB.max.y << ", " << animBB.max.z << ")\n";
    std::cout << "  size = (" << (animBB.max.x - animBB.min.x) << ", " << (animBB.max.y - animBB.min.y) << ", " << (animBB.max.z - animBB.min.z) << ")\n";

    UnloadModelAnimations(anims, animCount);
    UnloadModel(m);
    CloseWindow();
    return 0;
}
