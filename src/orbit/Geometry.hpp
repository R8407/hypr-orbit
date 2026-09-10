#pragma once

#include "Snapshot.hpp"

#include <cmath>
#include <vector>

namespace Orbit {

struct Point {

    double x = 0.0;
    double y = 0.0;
};


struct WorkspaceNode {

    int id = 0;

    std::string name;

    Point position;

    double angle = 0.0;

    double scale = 1.0;

    bool active = false;

    const WorkspaceSnapshot* workspace = nullptr;
};


struct OrbitLayout {

    Point center;

    double radius = 320.0;

    std::vector<WorkspaceNode> nodes;
};


/*
 * Calculate the positions of all workspaces
 * around a circle.
 */
OrbitLayout calculateLayout(
    const Snapshot& snapshot,
    int activeWorkspace,
    double centerX,
    double centerY
);

} // namespace Orbit
