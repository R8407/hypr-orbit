#include "Geometry.hpp"

#include <numbers>

namespace Orbit {

OrbitLayout calculateLayout(
    const Snapshot& snapshot,
    int activeWorkspace,
    double centerX,
    double centerY
) {

    OrbitLayout layout;

    layout.center = {
        centerX,
        centerY
    };


    const std::size_t count =
        snapshot.workspaces.size();


    if (count == 0)
        return layout;


    /*
     * Circle radius.
     *
     * This will eventually become configurable.
     */
    layout.radius = 340.0;


    /*
     * Place workspace 1 at the top.
     *
     * -PI/2 = 12 o'clock.
     */
    constexpr double START_ANGLE =
        -std::numbers::pi / 2.0;


    const double angleStep =
        (2.0 * std::numbers::pi)
        / static_cast<double>(count);


    layout.nodes.reserve(count);


    for (
        std::size_t index = 0;
        index < count;
        ++index
    ) {

        const auto& workspace =
            snapshot.workspaces[index];


        const double angle =
            START_ANGLE
            + angleStep
            * static_cast<double>(index);


        WorkspaceNode node;


        node.id =
            workspace.id;


        node.name =
            workspace.name;


        node.angle =
            angle;


        node.position.x =
            centerX
            + std::cos(angle)
                * layout.radius;


        node.position.y =
            centerY
            + std::sin(angle)
                * layout.radius;


        node.active =
            workspace.id
            == activeWorkspace;


        /*
         * Active workspace is visually larger.
         */
        node.scale =
            node.active
                ? 1.15
                : 0.80;


        node.workspace =
            &workspace;


        layout.nodes.push_back(
            node
        );
    }


    return layout;
}

} // namespace Orbit
