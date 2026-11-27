// ClothPhysics.h – real-time skirt cloth physics & collision guard for Raylib
#pragma once
#include <raylib.h>
#include <vector>

struct GarmentCollider {
    std::vector<Vector3> points;
    Matrix transform;
    BoundingBox aabb;
};

class ClothPhysics {
public:
    static std::vector<GarmentCollider> LoadColliders(const char* glbPath);
    static void AttachColliders(Model* model, const std::vector<GarmentCollider>& colliders);
    static bool TestCollision(const GarmentCollider& col, const BoundingBox& worldBox);

    // Real-time spring-damper cloth simulation + elastic body/leg & hand collision.
    static void UpdateSkirtCloth(Model* garment, const Model* bodyModel, float dt, Vector3 worldVel,
                                 float visualYaw, bool animUpdatedThisFrame,
                                 Vector3 leftHandModel, Vector3 rightHandModel, bool hasHands);

    static void UpdateCloth(Model* garment, float dt);
};

static inline BoundingBox ComputeAABB(const std::vector<Vector3>& pts) {
    BoundingBox bb = { { 1e30f, 1e30f, 1e30f }, { -1e30f, -1e30f, -1e30f } };
    for (const auto& p : pts) {
        if (p.x < bb.min.x) bb.min.x = p.x; if (p.x > bb.max.x) bb.max.x = p.x;
        if (p.y < bb.min.y) bb.min.y = p.y; if (p.y > bb.max.y) bb.max.y = p.y;
        if (p.z < bb.min.z) bb.min.z = p.z; if (p.z > bb.max.z) bb.max.z = p.z;
    }
    return bb;
}
