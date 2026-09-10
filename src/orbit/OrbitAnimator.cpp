#include "OrbitAnimator.hpp"
#include <algorithm>
#include <cmath>

OrbitAnimator::OrbitAnimator() { reset(); }

void OrbitAnimator::reset() {
    m_cards.clear();
    m_ghosts.clear();
    m_angle = m_target = m_vel = 0;
    m_targetWS = m_prevWS = -1;
    m_visible = false;
    m_closeDone = false;
    m_openReq = false;
    m_flyScale = 1.0f;
    m_flyVel = 0;
    m_breath = 0;
    m_globalOp = 0;
    m_dimOp = 0;
    m_exitActive = false;
    m_exitProgress = 0;
    m_exitSpeed = 0;
    m_exitWS = -1;
}

double OrbitAnimator::shortest(double a, double b) const {
    double d = b - a;
    while (d > std::numbers::pi)  d -= 2 * std::numbers::pi;
    while (d < -std::numbers::pi) d += 2 * std::numbers::pi;
    return d;
}

int OrbitAnimator::indexOf(WORKSPACEID id) const {
    for (int i = 0; i < (int)m_cards.size(); ++i)
        if (m_cards[i].id == id) return i;
    return -1;
}

void OrbitAnimator::computeTarget() {
    int idx = indexOf(m_targetWS);
    if (idx < 0) return;
    const double focal = -std::numbers::pi / 2.0;
    m_target = m_angle + shortest(m_angle + m_cards[idx].baseAngle, focal);
}

void OrbitAnimator::setWorkspaces(
    const std::vector<WORKSPACEID>& ids, WORKSPACEID active
) {
    m_cards.clear();
    if (ids.empty()) return;

    const int n = (int)ids.size();
    const double step = 2.0 * std::numbers::pi / n;
    const double start = -std::numbers::pi / 2.0;

    for (int i = 0; i < n; ++i) {
        CardAnimState c;
        c.id = ids[i];
        c.baseAngle = start + step * i;
        c.currentAngle = c.baseAngle;
        c.currentScale = 0.0;
        c.currentOpacity = 0.0;
        c.spawnRadius = 0.0;
        c.active = (ids[i] == active);
        m_cards.push_back(c);
    }

    m_targetWS = active;
    m_prevWS = active;
    computeTarget();
    spawnParticles();
}

void OrbitAnimator::switchTo(WORKSPACEID id) {
    if (id == m_targetWS || indexOf(id) < 0) return;

    m_prevWS = m_targetWS;
    m_targetWS = id;
    computeTarget();

    /* ghost trail of old card */
    GhostTrail g;
    g.active = true;
    g.elapsed = 0;
    g.duration = 0.15f;
    g.id = m_prevWS;
    g.startScale = 1.0;
    g.startOp = 0.5;
    int prevIdx = indexOf(m_prevWS);
    g.angle = (prevIdx >= 0) ? m_cards[prevIdx].currentAngle : 0;
    m_ghosts.push_back(g);

    /* fly-through pulse */
    m_flyScale = 1.0f;
    m_flyVel = 0;

    for (auto& c : m_cards)
        c.active = (c.id == id);
}

void OrbitAnimator::spawnParticles() {
    m_particles.clear();
    m_particles.reserve(40);
    std::uniform_real_distribution<double> dx(0, m_sw), dy(0, m_sh);
    std::uniform_real_distribution<double> dv(-8, 8);
    std::uniform_real_distribution<float> ds(0.5f, 2.0f), da(0.06f, 0.18f);
    const char glyphs[] = {'.', '·', '*', '·'};
    for (int i = 0; i < 40; ++i) {
        Particle p;
        p.x = dx(m_rng); p.y = dy(m_rng);
        p.vx = dv(m_rng); p.vy = dv(m_rng) * 0.2;
        p.size = ds(m_rng); p.alpha = da(m_rng);
        m_particles.push_back(p);
    }
}

void OrbitAnimator::updateParticles(float dt) {
    for (auto& p : m_particles) {
        p.x += p.vx * dt;
        p.y += p.vy * dt;
        if (p.x < -10) p.x += m_sw + 20;
        if (p.x > m_sw + 10) p.x -= m_sw + 20;
        if (p.y < -10) p.y += m_sh + 20;
        if (p.y > m_sh + 10) p.y -= m_sh + 20;
    }
}

void OrbitAnimator::requestOpen() { m_openReq = true; }

void OrbitAnimator::requestClose() {
    if (!m_visible) return;
    m_exitActive = true;
    m_exitProgress = 0;
    m_exitSpeed = 5.0f;
    m_exitWS = m_targetWS;
    m_visible = false;
}

void OrbitAnimator::tick(float dt) {
    if (dt <= 0 || dt > 0.1f) dt = 1.0f / 60.0f;

    /* --- open: instant dim, cards spawn from center --- */
    if (m_openReq && !m_visible) {
        m_visible = true;
        m_closeDone = false;
        m_openReq = false;
        m_exitActive = false;
        m_globalOp = 1.0f;
        m_dimOp = 0.6f;

        for (auto& c : m_cards) {
            c.spawnRadius = 0.0;
            c.currentScale = 0.0;
            c.currentOpacity = 0.0;
        }
    }

    /* --- dim layer --- */
    if (m_visible) {
        if (m_dimOp < 0.6f) {
            m_dimOp += 6.0f * dt;
            if (m_dimOp > 0.6f) m_dimOp = 0.6f;
        }
    } else if (m_exitActive) {
        /* collapse phase: dim fades with progress */
        m_exitProgress += m_exitSpeed * dt;
        if (m_exitProgress >= 1.0f) {
            m_exitProgress = 1.0f;
            m_exitActive = false;
            m_globalOp = 0;
            m_dimOp = 0;
            m_closeDone = true;
        } else {
            m_dimOp = 0.6f * (1.0f - m_exitProgress);
            m_globalOp = 1.0f - m_exitProgress * m_exitProgress;
        }
    }

    if (!m_visible && !m_exitActive && m_globalOp < 0.01f) {
        m_closeDone = true;
        return;
    }

    /* --- breath --- */
    m_breath += dt * 2.5f;

    /* --- spring orbit rotation --- */
    {
        double disp = m_target - m_angle;
        m_vel += disp * K * dt;
        m_vel *= std::pow(D, dt * 60.0);
        m_angle += m_vel * dt;
    }

    /* --- fly-through overshoot --- */
    {
        float target = 1.0f;
        if (m_flyScale < 1.22f && m_flyVel >= 0)
            target = 1.22f;
        else if (m_flyScale > 1.0f)
            target = 1.0f;

        float diff = target - m_flyScale;
        m_flyVel += diff * 400.0f * dt;
        m_flyVel *= std::pow(0.55f, dt * 60.0f);
        m_flyScale += m_flyVel * dt;
    }

    /* --- ghost trails --- */
    for (auto& g : m_ghosts) {
        if (!g.active) continue;
        g.elapsed += dt;
        if (g.elapsed >= g.duration) g.active = false;
    }
    m_ghosts.erase(
        std::remove_if(m_ghosts.begin(), m_ghosts.end(),
            [](const GhostTrail& g) { return !g.active; }),
        m_ghosts.end()
    );

    /* --- per-card spawn spring --- */
    const double ORBIT = 340.0;

    for (auto& c : m_cards) {
        /* spring spawnRadius toward target */
        double targetR = ORBIT;
        double targetS = c.active ? 1.0 : 0.82;
        double targetO = c.active ? 1.0 : 0.7;

        if (m_exitActive) {
            /* collapse back to center */
            double collapseT = m_exitProgress;
            targetR = ORBIT * (1.0 - collapseT);
            targetS = (c.active ? 1.0 : 0.82) * (1.0 - collapseT);
            targetO = (c.active ? 1.0 : 0.7) * (1.0 - collapseT);
        }

        double dR = targetR - c.spawnRadius;
        double dS = targetS - c.currentScale;
        double dO = targetO - c.currentOpacity;

        double springK = 300.0;
        double dampF = std::pow(0.68, dt * 60.0);

        c.spawnRadius += dR * springK * dt;
        c.spawnRadius *= dampF;

        c.currentScale += dS * springK * dt;
        c.currentScale *= dampF;

        c.currentOpacity += dO * springK * dt;
        c.currentOpacity *= dampF;

        /* clamp */
        if (c.spawnRadius < 0.01) c.spawnRadius = 0;
        if (c.currentScale < 0.001) c.currentScale = 0;
        if (c.currentOpacity < 0.005) c.currentOpacity = 0;

        c.currentAngle = c.baseAngle + m_angle;
    }

    /* --- particles --- */
    updateParticles(dt);
}

double OrbitAnimator::getRingBreath() const {
    return 0.5 + 0.5 * std::sin(m_breath);
}
