#include "navigation/NavigationSystem.h"
#include "world/World.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace
{
    // Stable integer key for a tile coordinate.
    // The two 32-bit signed coordinates are packed into one 64-bit value.
    std::int64_t MakeTileKey(int x, int z)
    {
        return (static_cast<std::int64_t>(static_cast<std::uint32_t>(x)) << 32) |
               static_cast<std::uint32_t>(z);
    }

    bool IsTileInLoadedChunk(const World* world, int tileX, int tileZ)
    {
        if (!world) {
            return false;
        }

        int chunkX = 0;
        int chunkZ = 0;
        world->WorldToChunk(
            static_cast<float>(tileX) + 0.5f,
            static_cast<float>(tileZ) + 0.5f,
            chunkX,
            chunkZ
        );

        // Navigation must never use World's procedural fallback outside
        // the currently loaded navigation domain. Otherwise an unreachable
        // island can make A* explore the effectively infinite world.
        return const_cast<World*>(world)->GetChunk(chunkX, chunkZ) != nullptr;
    }

    struct OpenEntry
    {
        TileCoord coord;
        float g;
        float f;
        std::uint64_t order;
    };

    struct OpenEntryCompare
    {
        bool operator()(const OpenEntry& a, const OpenEntry& b) const
        {
            if (a.f != b.f) {
                return a.f > b.f;
            }

            // Deterministic tie-breaks.
            if (a.coord.x != b.coord.x) {
                return a.coord.x > b.coord.x;
            }

            if (a.coord.z != b.coord.z) {
                return a.coord.z > b.coord.z;
            }

            return a.order > b.order;
        }
    };
}

NavigationSystem::NavigationSystem()
    : m_world(nullptr)
    , m_maxStepHeight(1.5f)
{
}

void NavigationSystem::Initialize(World* world, float maxStepHeight)
{
    m_world = world;
    m_maxStepHeight = maxStepHeight;
}

void NavigationSystem::SetOccupiedTiles(const std::vector<Vector3>& mobPositions)
{
    m_occupiedTiles.clear();
    for (const auto& pos : mobPositions) {
        int tx = static_cast<int>(std::floor(pos.x));
        int tz = static_cast<int>(std::floor(pos.z));
        std::int64_t key = MakeTileKey(tx, tz);
        m_occupiedTiles.insert(key);
    }
}

void NavigationSystem::SetStructureBounds(const std::vector<Rectangle>& bounds)
{
    m_structureBounds = bounds;
}

bool NavigationSystem::IsTileBlocked(int tileX, int tileZ) const
{
    if (!m_world) {
        return true;
    }

    // Water and future obstacles are handled through World.
    const float tx = tileX + 0.5f;
    const float tz = tileZ + 0.5f;

    if (m_world->IsWaterAt(tx, tz)) {
        return true;
    }

    for (const Rectangle& bounds : m_structureBounds) {
        if (CheckCollisionPointRec({ tx, tz }, bounds)) return true;
    }

    return false;
}

bool NavigationSystem::IsWalkable(int tileX, int tileZ) const
{
    if (!m_world) {
        return false;
    }

    if (IsTileBlocked(tileX, tileZ)) {
        return false;
    }

    const float tileCenterX = tileX + 0.5f;
    const float tileCenterZ = tileZ + 0.5f;

    const float height = m_world->GetHeightAt(tileCenterX, tileCenterZ);

    return height >= 0.0f;
}

bool NavigationSystem::CanTraverse(
    int fromX,
    int fromZ,
    int toX,
    int toZ) const
{
    if (!m_world) {
        return false;
    }

    // Both tiles must be walkable.
    if (!IsWalkable(fromX, fromZ) || !IsWalkable(toX, toZ)) {
        return false;
    }

    if ((fromX != toX || fromZ != toZ) &&
        m_occupiedTiles.count(MakeTileKey(toX, toZ)) != 0) {
        return false;
    }

    const float fromCenterX = fromX + 0.5f;
    const float fromCenterZ = fromZ + 0.5f;
    const float toCenterX = toX + 0.5f;
    const float toCenterZ = toZ + 0.5f;

    const float fromHeight =
        m_world->GetHeightAt(fromCenterX, fromCenterZ);

    const float toHeight =
        m_world->GetHeightAt(toCenterX, toCenterZ);

    const float delta = toHeight - fromHeight;

    // ASCENT:
    // The maximum step height limits climbing only.
    if (delta > 0.0f) {
        return delta <= m_maxStepHeight;
    }

    // DESCENT:
    // Descending is always allowed.
    if (delta < 0.0f) {
        return true;
    }

    // SAME LEVEL.
    return true;
}

float NavigationSystem::CalculateMovementCost(
    int fromX,
    int fromZ,
    int toX,
    int toZ) const
{
    if (!m_world) {
        return 1.0f;
    }

    const float fromCenterX = fromX + 0.5f;
    const float fromCenterZ = fromZ + 0.5f;
    const float toCenterX = toX + 0.5f;
    const float toCenterZ = toZ + 0.5f;

    const float fromHeight =
        m_world->GetHeightAt(fromCenterX, fromCenterZ);

    const float toHeight =
        m_world->GetHeightAt(toCenterX, toCenterZ);

    const float delta = toHeight - fromHeight;

    // Movement cost rules:
    //
    // Same level = 1
    // Descent    = 1
    // Ascent     = 3
    //
    // CanTraverse() is responsible for rejecting
    // ascents that exceed m_maxStepHeight.

    if (delta > 0.01f) {
        return 3.0f;
    }

    if (delta < -0.01f) {
        return 1.0f;
    }

    return 1.0f;
}

float NavigationSystem::Heuristic(
    int x1,
    int z1,
    int x2,
    int z2) const
{
    // Manhattan distance is admissible because every movement
    // costs at least 1 and movement is strictly 4-directional.
    //
    // It remains valid with the asymmetric costs:
    // same level = 1, descent = 1, ascent = 3.
    return static_cast<float>(
        std::abs(x1 - x2) +
        std::abs(z1 - z2)
    );
}

void NavigationSystem::GetNeighbors(
    const TileCoord& tile,
    std::vector<TileCoord>& neighbors) const
{
    neighbors.clear();
    neighbors.reserve(4);

    // 4-directional movement only:
    // North, South, East, West.
    const int dx[] = {0, 0, 1, -1};
    const int dz[] = {1, -1, 0, 0};

    for (int i = 0; i < 4; ++i) {
        const int nx = tile.x + dx[i];
        const int nz = tile.z + dz[i];

        if (CanTraverse(tile.x, tile.z, nx, nz)) {
            neighbors.emplace_back(nx, nz);
        }
    }
}

std::vector<TileCoord> NavigationSystem::ReconstructPath(
    const std::vector<Node>& allNodes,
    const TileCoord& start,
    const TileCoord& target) const
{
    std::vector<TileCoord> path;

    const Node* targetNode = nullptr;

    for (const auto& node : allNodes) {
        if (node.coord == target) {
            targetNode = &node;
            break;
        }
    }

    if (!targetNode || !targetNode->hasParent) {
        if (targetNode && target == start) {
            path.push_back(start);
        }

        return path;
    }

    TileCoord current = target;

    while (true) {
        path.push_back(current);

        const Node* currentNode = nullptr;

        for (const auto& node : allNodes) {
            if (node.coord == current) {
                currentNode = &node;
                break;
            }
        }

        if (!currentNode || !currentNode->hasParent) {
            break;
        }

        if (currentNode->parent == start) {
            path.push_back(start);
            break;
        }

        current = currentNode->parent;
    }

    std::reverse(path.begin(), path.end());

    return path;
}

std::vector<TileCoord> NavigationSystem::FindPath(
    int startX,
    int startZ,
    int targetX,
    int targetZ) const
{
    if (!m_world) {
        std::cout
            << "[NAV] FindPath failed: World not initialized"
            << std::endl;

        return {};
    }

    const TileCoord start(startX, startZ);
    const TileCoord target(targetX, targetZ);

    // Validate start.
    if (!IsWalkable(startX, startZ)) {
        std::cout
            << "[NAV] FindPath failed: Start tile ("
            << startX << "," << startZ
            << ") not walkable"
            << std::endl;

        return {};
    }

    // Validate target.
    if (!IsWalkable(targetX, targetZ)) {
        std::cout
            << "[NAV] FindPath failed: Target tile ("
            << targetX << "," << targetZ
            << ") not walkable"
            << std::endl;

        return {};
    }

    // Navigation is intentionally restricted to currently loaded chunks.
    //
    // World::GetHeightAt() and World::IsWaterAt() have a deterministic
    // procedural fallback for unloaded chunks. That is useful for world
    // queries, but it makes an unconstrained path search effectively
    // infinite when the target is on a disconnected island.
    //
    // Clicked/visible tiles are already inside the loaded world, so this
    // restriction does not affect normal local navigation. It guarantees
    // that an unreachable target produces a finite, safe "no path".
    if (!IsTileInLoadedChunk(m_world, start.x, start.z) ||
        !IsTileInLoadedChunk(m_world, target.x, target.z)) {

        std::cout
            << "[NAV] FindPath failed: Start or target is outside "
            << "the loaded navigation domain"
            << std::endl;

        return {};
    }

    // Already there.
    if (start == target) {
        return {start};
    }

    // ---------------------------------------------------------------------
    // Optimized A*
    //
    // The previous implementation was effectively Dijkstra because the
    // heuristic was zero. It also performed repeated linear scans of
    // openSet, closedSet and allNodes.
    //
    // This implementation uses:
    //   - real Manhattan heuristic
    //   - priority_queue for the open set
    //   - hash maps for best g-scores and parents
    //   - lazy queue invalidation instead of linear removal/update
    //   - search restricted to currently loaded chunks, so disconnected
    //     islands terminate with "no path" instead of exploring forever
    //
    // Gameplay rules remain unchanged:
    //   - 4 directions
    //   - water blocked
    //   - max ascent = m_maxStepHeight
    //   - ascent cost = 3
    //   - descent cost = 1
    //   - same-level cost = 1
    // ---------------------------------------------------------------------

    using GScoreMap = std::unordered_map<std::int64_t, float>;
    using ParentMap = std::unordered_map<std::int64_t, TileCoord>;

    std::priority_queue<
        OpenEntry,
        std::vector<OpenEntry>,
        OpenEntryCompare
    > openSet;

    GScoreMap gScore;
    ParentMap parent;
    std::unordered_set<std::int64_t> closedSet;

    // Reserve a reasonable amount for typical local paths without
    // imposing a fixed search radius.
    gScore.reserve(1024);
    parent.reserve(1024);
    closedSet.reserve(1024);

    const std::int64_t startKey = MakeTileKey(start.x, start.z);
    const std::int64_t targetKey = MakeTileKey(target.x, target.z);

    gScore[startKey] = 0.0f;

    std::uint64_t pushOrder = 0;

    openSet.push(OpenEntry{
        start,
        0.0f,
        Heuristic(start.x, start.z, target.x, target.z),
        pushOrder++
    });

    // Fixed neighbor order preserves deterministic path selection.
    const int dx[] = {0, 0, 1, -1};
    const int dz[] = {1, -1, 0, 0};

    while (!openSet.empty()) {
        const OpenEntry currentEntry = openSet.top();
        openSet.pop();

        const TileCoord current = currentEntry.coord;
        const std::int64_t currentKey =
            MakeTileKey(current.x, current.z);

        auto currentGIt = gScore.find(currentKey);
        if (currentGIt == gScore.end()) {
            continue;
        }

        // The queue can contain stale entries after a cheaper route to
        // the same tile is discovered. Ignore those entries.
        if (currentEntry.g > currentGIt->second) {
            continue;
        }

        if (closedSet.find(currentKey) != closedSet.end()) {
            continue;
        }

        // Target reached with the optimal A* cost.
        if (current == target) {
            std::vector<TileCoord> path;
            path.reserve(
                static_cast<std::size_t>(
                    Heuristic(start.x, start.z, target.x, target.z) + 1.0f
                )
            );

            TileCoord node = target;

            while (node != start) {
                path.push_back(node);

                const std::int64_t key =
                    MakeTileKey(node.x, node.z);

                auto parentIt = parent.find(key);
                if (parentIt == parent.end()) {
                    // Defensive failure: an incomplete parent chain
                    // must never produce a partial movement path.
                    return {};
                }

                node = parentIt->second;
            }

            path.push_back(start);
            std::reverse(path.begin(), path.end());

            return path;
        }

        closedSet.insert(currentKey);

        for (int i = 0; i < 4; ++i) {
            const int nx = current.x + dx[i];
            const int nz = current.z + dz[i];

            if (!CanTraverse(current.x, current.z, nx, nz)) {
                continue;
            }

            // Never expand into unloaded chunks. This is the critical
            // guard that makes disconnected islands terminate correctly.
            if (!IsTileInLoadedChunk(m_world, nx, nz)) {
                continue;
            }

            const TileCoord neighbor(nx, nz);
            const std::int64_t neighborKey =
                MakeTileKey(nx, nz);

            if (closedSet.find(neighborKey) != closedSet.end()) {
                continue;
            }

            const float moveCost =
                CalculateMovementCost(
                    current.x,
                    current.z,
                    nx,
                    nz
                );

            const float tentativeG =
                currentGIt->second + moveCost;

            auto neighborGIt = gScore.find(neighborKey);

            if (neighborGIt == gScore.end() ||
                tentativeG < neighborGIt->second) {

                gScore[neighborKey] = tentativeG;
                parent[neighborKey] = current;

                const float h =
                    Heuristic(nx, nz, target.x, target.z);

                openSet.push(OpenEntry{
                    neighbor,
                    tentativeG,
                    tentativeG + h,
                    pushOrder++
                });
            }
        }
    }

    std::cout
        << "[NAV] FindPath failed: No path from ("
        << startX << "," << startZ
        << ") to ("
        << targetX << "," << targetZ
        << ")"
        << std::endl;

    return {};
}
