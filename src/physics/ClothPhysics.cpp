#include "ClothPhysics.h"
#include <raymath.h>
#include <rlgl.h>
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <string>

static std::unordered_map<std::string, std::vector<GarmentCollider>> s_colliderCache;

struct SkirtClothState {
    float swayX = 0.0f;
    float swayZ = 0.0f;
    float velX  = 0.0f;
    float velZ  = 0.0f;
    float time  = 0.0f;
    std::vector<std::vector<Vector3>> prevOffsets;
};

static std::unordered_map<Model*, SkirtClothState> s_clothStates;

std::vector<GarmentCollider> ClothPhysics::LoadColliders(const char* glbPath) {
    if (!glbPath) return {};
    auto it = s_colliderCache.find(glbPath);
    if (it != s_colliderCache.end()) return it->second;
    s_colliderCache[glbPath] = {};
    return {};
}

void ClothPhysics::AttachColliders(Model* model, const std::vector<GarmentCollider>& colliders) {
    (void)model;
    (void)colliders;
}

bool ClothPhysics::TestCollision(const GarmentCollider& col, const BoundingBox& worldBox) {
    if (col.aabb.max.x < worldBox.min.x || col.aabb.min.x > worldBox.max.x) return false;
    if (col.aabb.max.y < worldBox.min.y || col.aabb.min.y > worldBox.max.y) return false;
    if (col.aabb.max.z < worldBox.min.z || col.aabb.min.z > worldBox.max.z) return false;
    return true;
}

void ClothPhysics::UpdateCloth(Model* garment, float dt) {
    UpdateSkirtCloth(garment, nullptr, dt, Vector3{ 0.0f, 0.0f, 0.0f }, 0.0f, false,
                     Vector3{ 0,0,0 }, Vector3{ 0,0,0 }, false);
}

void ClothPhysics::UpdateSkirtCloth(Model* garment, const Model* bodyModel, float dt, Vector3 worldVel,
                                    float visualYaw, bool animUpdatedThisFrame,
                                    Vector3 leftHandModel, Vector3 rightHandModel, bool hasHands) {
    if (!garment || garment->meshCount <= 0 || dt <= 0.0f) return;
    dt = std::min(dt, 0.05f);

    SkirtClothState& st = s_clothStates[garment];
    st.time += dt;

    // Convert world velocity into model-local horizontal coordinates
    // Model faces +Z when visualYaw == 0.
    const float c = std::cos(visualYaw);
    const float s = std::sin(visualYaw);
    const float localVelX =  worldVel.x * c - worldVel.z * s;
    const float localVelZ =  worldVel.x * s + worldVel.z * c;
    const float speed = std::sqrt(localVelX * localVelX + localVelZ * localVelZ);
    const float speedRatio = std::min(speed / 4.5f, 1.0f);

    // Damped spring-mass pendulum: trails opposite to movement, bounces & settles on stop
    const float targetX = std::clamp(-localVelX * 0.045f, -0.16f, 0.16f);
    const float targetZ = std::clamp(-localVelZ * 0.045f, -0.16f, 0.16f);
    constexpr float kStiffness = 42.0f;
    constexpr float kDamping   = 6.8f;

    const float accX = (targetX - st.swayX) * kStiffness - st.velX * kDamping;
    const float accZ = (targetZ - st.swayZ) * kStiffness - st.velZ * kDamping;
    st.velX  += accX * dt;
    st.velZ  += accZ * dt;
    st.swayX += st.velX * dt;
    st.swayZ += st.velZ * dt;

    if (static_cast<int>(st.prevOffsets.size()) != garment->meshCount) {
        st.prevOffsets.assign(static_cast<size_t>(garment->meshCount), {});
    }

    // Only run cloth physics on garments that span from waist (Y > 0.85) down to a long skirt (Y < 0.50).
    // Boots (maxY < 0.45) and shorts (minY > 0.65) will NEVER trigger cloth physics.
    bool hasLowSkirt = false;
    bool hasWaist    = false;
    for (int m = 0; m < garment->meshCount; ++m) {
        const Mesh& mesh = garment->meshes[m];
        if (!mesh.vertices) continue;
        for (int vi = 0; vi < mesh.vertexCount; ++vi) {
            const float vy = mesh.vertices[vi * 3 + 1];
            if (vy < 0.50f && vy > 0.12f) hasLowSkirt = true;
            if (vy > 0.85f) hasWaist = true;
            if (hasLowSkirt && hasWaist) break;
        }
        if (hasLowSkirt && hasWaist) break;
    }
    if (!hasLowSkirt || !hasWaist) return;

    const float swayMag = std::sqrt(st.swayX * st.swayX + st.swayZ * st.swayZ);

    // Collect animated body vertices in the leg/pelvis region (Y in [0.08, 0.98], |X| < 0.35)
    // so the skirt stretches elastically over knees, thighs, and calves in real time.
    std::vector<Vector3> bodyLegVerts;
    if (bodyModel && bodyModel->meshCount > 0) {
        int bestMesh = 0;
        int bestCount = 0;
        for (int bmIdx = 0; bmIdx < bodyModel->meshCount; ++bmIdx) {
            if (bodyModel->meshes[bmIdx].vertexCount > bestCount) {
                bestCount = bodyModel->meshes[bmIdx].vertexCount;
                bestMesh = bmIdx;
            }
        }
        const Mesh& bMesh = bodyModel->meshes[bestMesh];
        const float* bv = bMesh.animVertices ? bMesh.animVertices : bMesh.vertices;
        if (bv) {
            bodyLegVerts.reserve(512);
            for (int bi = 0; bi < bMesh.vertexCount; ++bi) {
                const float bx = bv[bi * 3 + 0];
                const float by = bv[bi * 3 + 1];
                const float bz = bv[bi * 3 + 2];
                if (by >= 0.08f && by <= 0.98f && std::fabs(bx) < 0.35f) {
                    bodyLegVerts.push_back(Vector3{ bx, by, bz });
                }
            }
        }
    }

    for (int m = 0; m < garment->meshCount; ++m) {
        Mesh& mesh = garment->meshes[m];
        if (!mesh.vertices || mesh.vertexCount <= 0) continue;

        // Skinned models write posed vertices into animVertices; static models use vertices.
        float* targetVerts = mesh.animVertices ? mesh.animVertices : mesh.vertices;
        auto& prev = st.prevOffsets[static_cast<size_t>(m)];
        if (static_cast<int>(prev.size()) != mesh.vertexCount) {
            prev.assign(static_cast<size_t>(mesh.vertexCount), Vector3{ 0.0f, 0.0f, 0.0f });
        }

        bool anyChanged = false;
        for (int vi = 0; vi < mesh.vertexCount; ++vi) {
            const float restX = mesh.vertices[vi * 3 + 0];
            const float restY = mesh.vertices[vi * 3 + 1];
            const float restZ = mesh.vertices[vi * 3 + 2];

            // Undo previous frame's cloth offset if Raylib didn't re-run skinning this frame
            if (!animUpdatedThisFrame) {
                targetVerts[vi * 3 + 0] -= prev[vi].x;
                targetVerts[vi * 3 + 1] -= prev[vi].y;
                targetVerts[vi * 3 + 2] -= prev[vi].z;
            }

            // Only simulate cloth on skirt vertices below the waist (restY < 0.96)
            // and outside the boots/shorts (skirts have wider radius > 0.14 or restY > 0.14)
            if (restY >= 0.96f || restY < 0.12f) {
                prev[vi] = Vector3{ 0.0f, 0.0f, 0.0f };
                continue;
            }

            const float flex = std::clamp((0.96f - restY) / 0.81f, 0.0f, 1.0f);
            const float w = flex * flex;

            const float radLen = std::sqrt(restX * restX + restZ * restZ);
            const float rx = (radLen > 1e-4f) ? (restX / radLen) : 0.0f;
            const float rz = (radLen > 1e-4f) ? (restZ / radLen) : 0.0f;
            const float ang = std::atan2(restZ, restX);

            // Fabric breeze & walk-cycle ripple
            const float ripple = std::sin(st.time * 3.8f + ang * 3.0f + restY * 4.5f)
                               * (0.006f + 0.014f * speedRatio) * w;
            const float flare = speedRatio * 0.028f * w;

            float ox = st.swayX * w + rx * (flare + ripple);
            float oz = st.swayZ * w + rz * (flare + ripple);
            float oy = swayMag * 0.06f * w; // hem lifts slightly as it swings

            // Leg collision guard: prevent cloth from being pushed inward past min radius
            const float minR = 0.155f + 0.12f * flex;
            float curX = targetVerts[vi * 3 + 0] + ox;
            float curY = targetVerts[vi * 3 + 1] + oy;
            float curZ = targetVerts[vi * 3 + 2] + oz;
            const float newR = std::sqrt(curX * curX + curZ * curZ);
            if (newR < minR && newR > 1e-4f) {
                const float push = (minR - newR) / newR;
                ox += curX * push;
                oz += curZ * push;
                curX = targetVerts[vi * 3 + 0] + ox;
                curZ = targetVerts[vi * 3 + 2] + oz;
            }

            // Hand collision: indent skirt fabric inward around the player's hands so hands NEVER clip inside!
            if (hasHands) {
                const Vector3 hands[2] = { leftHandModel, rightHandModel };
                constexpr float kHandRadius = 0.115f;
                for (int hi = 0; hi < 2; ++hi) {
                    const float dxH = curX - hands[hi].x;
                    const float dyH = curY - hands[hi].y;
                    const float dzH = curZ - hands[hi].z;
                    const float distSq = dxH * dxH + dyH * dyH + dzH * dzH;
                    if (distSq < kHandRadius * kHandRadius) {
                        const float dist = std::sqrt(distSq);
                        const float pen = (kHandRadius - dist);
                        const float rLen = std::sqrt(curX * curX + curZ * curZ);
                        if (rLen > 1e-4f) {
                            ox -= (curX / rLen) * pen * 1.15f;
                            oz -= (curZ / rLen) * pen * 1.15f;
                            curX = targetVerts[vi * 3 + 0] + ox;
                            curZ = targetVerts[vi * 3 + 2] + oz;
                        }
                    }
                }
            }

            // Elastic body/leg collision: push skirt vertices outward smoothly whenever
            // thighs, knees, or calves press against the fabric, stretching it like real cloth.
            if (!bodyLegVerts.empty()) {
                constexpr float kLegInfluenceRadius = 0.13f;
                constexpr float kClothClearance     = 0.042f;
                float bestPushX = 0.0f;
                float bestPushZ = 0.0f;
                float maxPush   = 0.0f;
                for (const Vector3& bp : bodyLegVerts) {
                    const float dyB = std::fabs(bp.y - curY);
                    if (dyB > 0.10f) continue;
                    const float dxB = curX - bp.x;
                    const float dzB = curZ - bp.z;
                    const float dXZ = std::sqrt(dxB * dxB + dzB * dzB);
                    if (dXZ > kLegInfluenceRadius) continue;

                    // Compare radial distance from torso axis (0, curY, 0):
                    // if body vertex is further out (or within clearance) than cloth vertex, stretch cloth outward!
                    const float bRad = std::sqrt(bp.x * bp.x + bp.z * bp.z);
                    const float cRad = std::sqrt(curX * curX + curZ * curZ);
                    const float requiredRad = bRad + kClothClearance;
                    if (cRad < requiredRad && bRad > 1e-4f) {
                        const float yFalloff = 1.0f - (dyB / 0.10f);
                        const float xzFalloff = 1.0f - (dXZ / kLegInfluenceRadius);
                        const float stretch = (requiredRad - cRad) * (0.55f + 0.45f * yFalloff * xzFalloff);
                        if (stretch > maxPush && cRad > 1e-4f) {
                            maxPush = stretch;
                            bestPushX = (curX / cRad) * stretch;
                            bestPushZ = (curZ / cRad) * stretch;
                        }
                    }
                }
                ox += bestPushX;
                oz += bestPushZ;
            }

            targetVerts[vi * 3 + 0] += ox;
            targetVerts[vi * 3 + 1] += oy;
            targetVerts[vi * 3 + 2] += oz;
            prev[vi] = Vector3{ ox, oy, oz };
            anyChanged = true;
        }

        if (anyChanged && mesh.vboId && mesh.vboId[0] != 0) {
            rlUpdateVertexBuffer(mesh.vboId[0], targetVerts,
                                 mesh.vertexCount * 3 * static_cast<int>(sizeof(float)), 0);
        }
    }
}

void ClothPhysics::UpdateHairCloth(Model* hairModel, const Model* bodyModel, float dt, Vector3 worldVel,
                                   float visualYaw, bool animUpdatedThisFrame) {
    if (!hairModel || hairModel->meshCount <= 0 || dt <= 0.0f) return;
    dt = std::min(dt, 0.05f);

    // Find top of hair (crown) in rest space so scalp stays pinned while hanging strands sway
    float maxRestY = -1e30f;
    float minRestY =  1e30f;
    for (int m = 0; m < hairModel->meshCount; ++m) {
        const Mesh& mesh = hairModel->meshes[m];
        if (!mesh.vertices) continue;
        for (int vi = 0; vi < mesh.vertexCount; ++vi) {
            const float vy = mesh.vertices[vi * 3 + 1];
            if (vy > maxRestY) maxRestY = vy;
            if (vy < minRestY) minRestY = vy;
        }
    }
    constexpr float kPinnedDepth = 0.185f; // top 18.5cm stays anchored to the skull
    if (maxRestY - minRestY <= kPinnedDepth + 0.02f) {
        return; // short hairstyle (crop / side_part / tied_back): nothing hangs down to sway
    }

    SkirtClothState& st = s_clothStates[hairModel];
    st.time += dt;

    const float c = std::cos(visualYaw);
    const float s = std::sin(visualYaw);
    const float localVelX =  worldVel.x * c - worldVel.z * s;
    const float localVelZ =  worldVel.x * s + worldVel.z * c;
    const float speed = std::sqrt(localVelX * localVelX + localVelZ * localVelZ);
    const float speedRatio = std::min(speed / 4.5f, 1.0f);

    // Hair is lighter and slightly bouncier than a heavy skirt
    const float targetX = std::clamp(-localVelX * 0.032f, -0.13f, 0.13f);
    const float targetZ = std::clamp(-localVelZ * 0.032f, -0.13f, 0.13f);
    constexpr float kStiffness = 58.0f;
    constexpr float kDamping   = 6.2f;

    const float accX = (targetX - st.swayX) * kStiffness - st.velX * kDamping;
    const float accZ = (targetZ - st.swayZ) * kStiffness - st.velZ * kDamping;
    st.velX  += accX * dt;
    st.velZ  += accZ * dt;
    st.swayX += st.velX * dt;
    st.swayZ += st.velZ * dt;

    if (static_cast<int>(st.prevOffsets.size()) != hairModel->meshCount) {
        st.prevOffsets.assign(static_cast<size_t>(hairModel->meshCount), {});
    }

    // Collect animated upper-body vertices (back, shoulders, neck, head) so hair never clips into body
    std::vector<Vector3> upperBodyVerts;
    if (bodyModel && bodyModel->meshCount > 0) {
        int bestMesh = 0;
        int bestCount = 0;
        for (int bmIdx = 0; bmIdx < bodyModel->meshCount; ++bmIdx) {
            if (bodyModel->meshes[bmIdx].vertexCount > bestCount) {
                bestCount = bodyModel->meshes[bmIdx].vertexCount;
                bestMesh = bmIdx;
            }
        }
        const Mesh& bMesh = bodyModel->meshes[bestMesh];
        const float* bv = bMesh.animVertices ? bMesh.animVertices : bMesh.vertices;
        if (bv) {
            upperBodyVerts.reserve(512);
            for (int bi = 0; bi < bMesh.vertexCount; ++bi) {
                const float bx = bv[bi * 3 + 0];
                const float by = bv[bi * 3 + 1];
                const float bz = bv[bi * 3 + 2];
                if (by >= 1.12f && by <= maxRestY + 0.05f && std::fabs(bx) < 0.30f) {
                    upperBodyVerts.push_back(Vector3{ bx, by, bz });
                }
            }
        }
    }

    const float swayMag = std::sqrt(st.swayX * st.swayX + st.swayZ * st.swayZ);

    for (int m = 0; m < hairModel->meshCount; ++m) {
        Mesh& mesh = hairModel->meshes[m];
        if (!mesh.vertices || mesh.vertexCount <= 0) continue;

        float* targetVerts = mesh.animVertices ? mesh.animVertices : mesh.vertices;
        auto& prev = st.prevOffsets[static_cast<size_t>(m)];
        if (static_cast<int>(prev.size()) != mesh.vertexCount) {
            prev.assign(static_cast<size_t>(mesh.vertexCount), Vector3{ 0.0f, 0.0f, 0.0f });
        }

        bool anyChanged = false;
        for (int vi = 0; vi < mesh.vertexCount; ++vi) {
            const float restX = mesh.vertices[vi * 3 + 0];
            const float restY = mesh.vertices[vi * 3 + 1];
            const float restZ = mesh.vertices[vi * 3 + 2];

            if (!animUpdatedThisFrame) {
                targetVerts[vi * 3 + 0] -= prev[vi].x;
                targetVerts[vi * 3 + 1] -= prev[vi].y;
                targetVerts[vi * 3 + 2] -= prev[vi].z;
            }

            const float drop = maxRestY - restY;
            if (drop < kPinnedDepth) {
                prev[vi] = Vector3{ 0.0f, 0.0f, 0.0f };
                continue;
            }

            const float flex = std::clamp((drop - kPinnedDepth) / 0.30f, 0.0f, 1.0f);
            const float w = flex * flex;

            const float ang = std::atan2(restZ, restX);
            const float ripple = std::sin(st.time * 4.5f + ang * 2.0f + restY * 6.0f)
                               * (0.004f + 0.010f * speedRatio) * w;

            float ox = st.swayX * w + std::cos(ang) * ripple;
            float oz = st.swayZ * w + std::sin(ang) * ripple;
            float oy = swayMag * 0.04f * w;

            // Body/neck/shoulder collision: push hair outward if it gets too close to skin
            if (!upperBodyVerts.empty()) {
                float curX = targetVerts[vi * 3 + 0] + ox;
                float curY = targetVerts[vi * 3 + 1] + oy;
                float curZ = targetVerts[vi * 3 + 2] + oz;
                constexpr float kHairClearance = 0.022f;
                constexpr float kInfluence     = 0.095f;
                float bestPushX = 0.0f, bestPushY = 0.0f, bestPushZ = 0.0f;
                float maxPush = 0.0f;
                for (const Vector3& bp : upperBodyVerts) {
                    const float dy = curY - bp.y;
                    if (std::fabs(dy) > 0.075f) continue;
                    const float dx = curX - bp.x;
                    const float dz = curZ - bp.z;
                    const float d2 = dx * dx + dy * dy + dz * dz;
                    if (d2 > kInfluence * kInfluence) continue;

                    // Compare radial distance from torso/neck spine axis (0, curY, 0.02)
                    const float bRad = std::sqrt(bp.x * bp.x + (bp.z - 0.02f) * (bp.z - 0.02f));
                    const float cRad = std::sqrt(curX * curX + (curZ - 0.02f) * (curZ - 0.02f));
                    const float reqRad = bRad + kHairClearance;
                    if (cRad < reqRad && cRad > 1e-4f) {
                        const float push = reqRad - cRad;
                        if (push > maxPush) {
                            maxPush = push;
                            bestPushX = (curX / cRad) * push;
                            bestPushZ = ((curZ - 0.02f) / cRad) * push;
                            bestPushY = (dy > 0.0f ? 0.15f : 0.0f) * push;
                        }
                    }
                }
                ox += bestPushX;
                oy += bestPushY;
                oz += bestPushZ;
            }

            targetVerts[vi * 3 + 0] += ox;
            targetVerts[vi * 3 + 1] += oy;
            targetVerts[vi * 3 + 2] += oz;
            prev[vi] = Vector3{ ox, oy, oz };
            anyChanged = true;
        }

        if (anyChanged && mesh.vboId && mesh.vboId[0] != 0) {
            rlUpdateVertexBuffer(mesh.vboId[0], targetVerts,
                                 mesh.vertexCount * 3 * static_cast<int>(sizeof(float)), 0);
        }
    }
}
