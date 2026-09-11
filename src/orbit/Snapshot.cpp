#include "Snapshot.hpp"

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/desktop/Workspace.hpp>
#include <hyprland/src/desktop/view/Window.hpp>
#include <hyprland/src/state/WorkspaceState.hpp>
#include <hyprland/src/desktop/state/WindowState.hpp>

#include <algorithm>

namespace Orbit {

Snapshot capture() {

    Snapshot snapshot;

    if (!g_pCompositor)
        return snapshot;


    /*
     * Get current workspaces.
     */
    auto workspaces =
        State::workspaceState()->workspacesCopy();


    /*
     * Deterministic workspace ordering.
     */
    std::sort(
        workspaces.begin(),
        workspaces.end(),

        [](const PHLWORKSPACE& a,
           const PHLWORKSPACE& b) {

            if (!a)
                return false;

            if (!b)
                return true;

            return a->m_id < b->m_id;
        }
    );


    /*
     * Capture every normal workspace.
     */
    for (const auto& workspace :
         workspaces) {

        if (!workspace)
            continue;


        if (workspace->m_isSpecialWorkspace)
            continue;


        WorkspaceSnapshot ws;

        ws.id =
            workspace->m_id;

        ws.name =
            workspace->m_name;


        /*
         * Capture windows.
         */
        for (const auto& window :
             Desktop::windowState()->windows()) {

            if (!window)
                continue;


            if (!window->m_isMapped)
                continue;


            if (!window->m_workspace)
                continue;


            if (
                window
                    ->m_workspace
                    ->m_isSpecialWorkspace
            ) {
                continue;
            }


            if (
                window
                    ->m_workspace
                    ->m_id
                != ws.id
            ) {
                continue;
            }


            WindowSnapshot win;


            win.title =
                window->m_title;


            const auto position =
                window->m_reportedPosition;


            const auto size =
                window->m_reportedSize;


            win.x =
                position.x;

            win.y =
                position.y;


            win.width =
                size.x;

            win.height =
                size.y;


            win.floating =
                window->m_isFloating;


            ws.windows.push_back(
                std::move(win)
            );
        }


        snapshot.workspaces.push_back(
            std::move(ws)
        );
    }


    return snapshot;
}

} // namespace Orbit
