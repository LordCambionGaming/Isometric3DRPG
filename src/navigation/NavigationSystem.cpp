#include "navigation/NavigationSystem.h"
#include "world/World.h"
#include <cmath>
#include <algorithm>
#include <iostream>

NavigationSystem::NavigationSystem()
    : m_world(nullptr)
    , m_maxStepHeight(1.5f)
{
}

void NavigationSystem::Initialize(World* world, float maxStepHeight) {
    m_world = world;
    m_maxStepHeight = maxStepHeight;
}

bool NavigationSystem::IsTileBlocked(int tileX, int tileZ) const {
    // STUB: Future integration with procedural trees/structures/occupancy.
    // Currently no blocking obstacles exist in the world.
    return false;
}

bool NavigationSystem::IsWalkable(int tileX, int tileZ) const {
    if (!m_world) return false;

    // Blocked tiles (obstacles) are never walkable
    if (IsTileBlocked(tileX, tileZ)) return false;

    // For now, all tiles are walkable (no obstacles yet)
    // Future: check for obstacles, structures, water depth, etc.
    float tileCenterX = tileX + 0.5f;
    float tileCenterZ = tileZ + 0.5f;
    float height = m_world->GetHeightAt(tileCenterX, tileCenterZ);

    // Basic validation: height exists
    return height >= 0.0f;
}

bool NavigationSystem::CanTraverse(int fromX, int fromZ, int toX, int toZ) const {
    if (!m_world) return false;

    // Check both tiles are walkable
    if (!IsWalkable(fromX, fromZ) || !IsWalkable(toX, toZ)) {
        return false;
    }

    // Get heights
    float fromCenterX = fromX + 0.5f;
    float fromCenterZ = fromZ + 0.5f;
    float toCenterX = toX + 0.5f;
    float toCenterZ = toZ + 0.5f;

    float fromHeight = m_world->GetHeightAt(fromCenterX, fromCenterZ);
    float toHeight = m_world->GetHeightAt(toCenterX, toCenterZ);

    // Check step height constraint
    float delta = std::abs(toHeight - fromHeight);
    return delta <= m_maxStepHeight;
}

float NavigationSystem::Heuristic(int x1, int z1, int x2, int z2) const {
    // Manhattan distance for 4-directional movement
    return static_cast<float>(std::abs(x2 - x1) + std::abs(z2 - z1));
}

void NavigationSystem::GetNeighbors(const TileCoord& tile, std::vector<TileCoord>& neighbors) const {
    neighbors.clear();

    // 4-directional movement: North, South, East, West
    const int dx[] = {0, 0, 1, -1};
    const int dz[] = {1, -1, 0, 0};

    for (int i = 0; i < 4; ++i) {
        int nx = tile.x + dx[i];
        int nz = tile.z + dz[i];

        // Check if traversal is valid
        if (CanTraverse(tile.x, tile.z, nx, nz)) {
            neighbors.push_back(TileCoord(nx, nz));
        }
    }
}

std::vector<TileCoord> NavigationSystem::ReconstructPath(
    const std::vector<Node>& allNodes,
    const TileCoord& start,
    const TileCoord& target) const
{
    std::vector<TileCoord> path;

    // Find target node
    const Node* targetNode = nullptr;
    for (const auto& node : allNodes) {
        if (node.coord == target) {
            targetNode = &node;
            break;
        }
    }

    if (!targetNode || !targetNode->hasParent) {
        // No path found or target is start
        if (targetNode && target == start) {
            path.push_back(start);
        }
        return path;
    }

    // Reconstruct path backwards
    TileCoord current = target;
    while (true) {
        path.push_back(current);

        // Find current node
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

    // Reverse to get path from start to target
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
        std::cout << "[NAV] FindPath failed: World not initialized" << std::endl;
        return std::vector<TileCoord>();
    }

    TileCoord start(startX, startZ);
    TileCoord target(targetX, targetZ);

    // Check if start and target are valid
    if (!IsWalkable(startX, startZ)) {
        std::cout << "[NAV] FindPath failed: Start tile (" << startX << "," << startZ << ") not walkable" << std::endl;
        return std::vector<TileCoord>();
    }

    if (!IsWalkable(targetX, targetZ)) {
        std::cout << "[NAV] FindPath failed: Target tile (" << targetX << "," << targetZ << ") not walkable" << std::endl;
        return std::vector<TileCoord>();
    }

    // If start == target
    if (start == target) {
        std::cout << "[NAV] FindPath: Start == Target, returning single-tile path" << std::endl;
        return std::vector<TileCoord>{start};
    }

    // A* algorithm
    std::vector<Node> openSet;
    std::vector<Node> closedSet;
    std::vector<Node> allNodes; // Track all nodes for path reconstruction

    // Initialize start node
    Node startNode(start);
    startNode.g = 0.0f;
    startNode.h = Heuristic(start.x, start.z, target.x, target.z);
    startNode.f = startNode.g + startNode.h;
    openSet.push_back(startNode);
    allNodes.push_back(startNode);

    while (!openSet.empty()) {
        // Find node with lowest f in open set
        auto minIt = std::min_element(openSet.begin(), openSet.end(),
            [](const Node& a, const Node& b) { return a.f < b.f; });

        Node current = *minIt;
        openSet.erase(minIt);

        // Check if we reached target
        if (current.coord == target) {
            std::cout << "[NAV] FindPath: Path found from (" << startX << "," << startZ
                      << ") to (" << targetX << "," << targetZ << ")" << std::endl;
            return ReconstructPath(allNodes, start, target);
        }

        closedSet.push_back(current);

        // Get neighbors
        std::vector<TileCoord> neighbors;
        GetNeighbors(current.coord, neighbors);

        for (const auto& neighborCoord : neighbors) {
            // Check if in closed set
            bool inClosed = false;
            for (const auto& closed : closedSet) {
                if (closed.coord == neighborCoord) {
                    inClosed = true;
                    break;
                }
            }
            if (inClosed) continue;

            // Calculate tentative g
            float tentativeG = current.g + 1.0f; // Cost = 1.0 per move

            // Find if neighbor already in open set
            Node* openNeighbor = nullptr;
            for (auto& node : openSet) {
                if (node.coord == neighborCoord) {
                    openNeighbor = &node;
                    break;
                }
            }

            if (openNeighbor) {
                // Update if this path is better
                if (tentativeG < openNeighbor->g) {
                    openNeighbor->g = tentativeG;
                    openNeighbor->f = openNeighbor->g + openNeighbor->h;
                    openNeighbor->parent = current.coord;
                    openNeighbor->hasParent = true;

                    // Update in allNodes too
                    for (auto& node : allNodes) {
                        if (node.coord == neighborCoord) {
                            node.g = tentativeG;
                            node.f = node.g + node.h;
                            node.parent = current.coord;
                            node.hasParent = true;
                            break;
                        }
                    }
                }
            } else {
                // Add new node to open set
                Node newNode(neighborCoord);
                newNode.g = tentativeG;
                newNode.h = Heuristic(neighborCoord.x, neighborCoord.z, target.x, target.z);
                newNode.f = newNode.g + newNode.h;
                newNode.parent = current.coord;
                newNode.hasParent = true;

                openSet.push_back(newNode);
                allNodes.push_back(newNode);
            }
        }
    }

    // No path found
    std::cout << "[NAV] FindPath failed: No path from (" << startX << "," << startZ
              << ") to (" << targetX << "," << targetZ << ")" << std::endl;
    return std::vector<TileCoord>();
}
