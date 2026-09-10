#pragma once

#include <cmath>
#include <numbers>
#include <vector>
#include <random>

struct CardAnimState {
    long long id        = -1;
    double baseAngle    = 0.0;
    double currentAngle = 0.0;
    double currentScale = 0.0;
    double currentOpacity = 0.0;
    double spawnRadius  = 0.0;
    bool   active       = false;
};

struct GhostTrail {
    bool   active   = false;
    float  elapsed  = 0.0f;
    float  duration = 0.15f;
    double angle    = 0.0;
    WORKSPACEID id  = -1;
    double startScale = 1.0;
    double startOp    = 0.5;
};

struct Particle {
    double x = 0, y = 0, vx = 0, vy = 0;
    float size = 1, alpha = 0.3f;
};

class OrbitAnimator {
public:
    OrbitAnimator();

    void setWorkspaces(const std::vector<WORKSPACEID>& ids, WORKSPACEID active);
    void switchTo(WORKSPACEID id);
    void requestOpen();
    void requestClose();
    void tick(float dt);
    void reset();

    bool overlayVisible() const { return m_visible; }
    bool closeDone() const { return m_closeDone; }
    void clearCloseDone() { m_closeDone = false; }

    float globalOpacity() const { return m_globalOp; }
    float dimOpacity() const { return m_dimOp; }

    double flyScale() const { return m_flyScale; }

    const std::vector<CardAnimState>& cards() const { return m_cards; }
    const std::vector<GhostTrail>& ghosts() const { return m_ghosts; }
    const std::vector<Particle>& particles() const { return m_particles; }
    double breathPhase() const { return m_breath; }

    void setScreen(double w, double h) { m_sw = w; m_sh = h; }

    /* exit portal */
    bool exitActive() const { return m_exitActive; }
    float exitProgress() const { return m_exitProgress; }
    WORKSPACEID exitSelected() const { return m_exitWS; }

private:
    std::vector<CardAnimState> m_cards;
    std::vector<GhostTrail> m_ghosts;
    std::vector<Particle> m_particles;

    double m_angle = 0;
    double m_target = 0;
    double m_vel = 0;

    static constexpr double K = 280.0;
    static constexpr double D = 0.70;

    WORKSPACEID m_targetWS = -1;
    WORKSPACEID m_prevWS = -1;
    bool m_visible = false;
    bool m_closeDone = false;
    bool m_openReq = false;

    float m_flyScale = 1.0f;
    float m_flyVel = 0.0f;

    float m_breath = 0;
    float m_globalOp = 0;
    float m_dimOp = 0;

    /* exit */
    bool m_exitActive = false;
    float m_exitProgress = 0;
    float m_exitSpeed = 0;
    WORKSPACEID m_exitWS = -1;

    /* particles */
    std::mt19937 m_rng{77};
    double m_sw = 1920, m_sh = 1080;

    double shortest(double a, double b) const;
    int indexOf(WORKSPACEID id) const;
    void computeTarget();
    void spawnParticles();
    void updateParticles(float dt);
};
