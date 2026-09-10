#pragma once

#include <string>
#include <vector>

namespace Orbit {

struct WindowSnapshot {

    std::string title;

    double x = 0.0;
    double y = 0.0;

    double width = 0.0;
    double height = 0.0;

    bool floating = false;
};


struct WorkspaceSnapshot {

    int id = 0;

    std::string name;

    std::vector<WindowSnapshot> windows;
};


struct Snapshot {

    std::vector<WorkspaceSnapshot> workspaces;

    int windowCount() const {

        int count = 0;

        for (const auto& workspace : workspaces)
            count += static_cast<int>(
                workspace.windows.size()
            );

        return count;
    }
};


/*
 * Capture the current Hyprland state
 * into our own Orbit representation.
 */
Snapshot capture();

} // namespace Orbit
