#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/event/EventBus.hpp>
#include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/render/Texture.hpp>
#include <hyprland/src/render/pass/RectPassElement.hpp>
#include <hyprland/src/render/pass/BorderPassElement.hpp>
#include <hyprland/src/render/pass/SurfacePassElement.hpp>
#include <hyprland/src/render/pass/TexPassElement.hpp>
#include <hyprland/src/render/pass/RendererHintsPassElement.hpp>
#include <hyprland/src/render/pass/Pass.hpp>
#include <hyprland/src/managers/PointerManager.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/desktop/view/WLSurface.hpp>
#include <hyprland/src/protocols/core/Compositor.hpp>

#include <cairo/cairo.h>

#include <cmath>
#include <cstring>
#include <numbers>
#include <string>
#include <vector>
#include <unordered_map>

namespace {

HANDLE g_pluginHandle = nullptr;
bool g_visible = false;
int g_selected = 0;
double g_time = 0;

CHyprSignalListener g_renderHook;
CHyprSignalListener g_mouseHook;
CHyprSignalListener g_mouseMoveHook;
CHyprSignalListener g_keyHook;

struct Card {
    WORKSPACEID id = -1;
    double angle = 0;
};

struct Particle {
    double orbitR, speed, size, phase;
    CHyprColor color;
};

struct WindowInfo {
    std::string cls;
    std::string title;
};

std::vector<Card> g_cards;
std::vector<Particle> g_particles;

constexpr double ORBIT_R = 300.0;
constexpr double CARD_R  = 90.0;
constexpr double TAU     = 2.0 * std::numbers::pi;

/* ============================================================
 * Side panel text cache
 * ============================================================ */

SP<CTexture> g_panelTex;
int g_panelWsID = -1;
int g_panelSel = -1;
double g_panelW = 0, g_panelH = 0;

constexpr int PANEL_MAX_W = 320;
constexpr int LINE_H      = 28;
constexpr int PAD         = 16;
constexpr int HEADER_H    = 40;

SP<CTexture> renderTextToTexture(const std::vector<std::string>& lines, double& outW, double& outH) {

    int maxChars = 0;
    for (auto& l : lines)
        maxChars = std::max(maxChars, (int)l.size());

    int w = std::min(PANEL_MAX_W, std::max(200, maxChars * 10 + PAD * 2));
    int h = (int)lines.size() * LINE_H + PAD * 2;

    cairo_surface_t* surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
    cairo_t* cr = cairo_create(surf);

    /* background */
    cairo_set_source_rgba(cr, 0.08, 0.08, 0.12, 0.85);
    cairo_paint(cr);

    /* border */
    cairo_set_source_rgba(cr, 0.3, 0.5, 0.8, 0.4);
    cairo_set_line_width(cr, 1.0);
    cairo_rectangle(cr, 0.5, 0.5, w - 1, h - 1);
    cairo_stroke(cr);

    /* text */
    cairo_select_font_face(cr, "monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 14);

    for (int i = 0; i < (int)lines.size(); ++i) {
        double y = PAD + i * LINE_H + 18;

        if (i == 0) {
            /* header line — brighter */
            cairo_set_source_rgba(cr, 0.5, 0.8, 1.0, 0.95);
            cairo_set_font_size(cr, 16);
        } else if (lines[i].empty()) {
            /* separator */
            cairo_set_source_rgba(cr, 0.3, 0.4, 0.6, 0.3);
            cairo_set_line_width(cr, 1.0);
            cairo_move_to(cr, PAD, y - 8);
            cairo_line_to(cr, w - PAD, y - 8);
            cairo_stroke(cr);
            continue;
        } else if (lines[i][0] == ' ') {
            /* window entry — dimmer */
            cairo_set_source_rgba(cr, 0.7, 0.75, 0.85, 0.8);
            cairo_set_font_size(cr, 13);
        } else {
            /* section label */
            cairo_set_source_rgba(cr, 0.4, 0.6, 0.9, 0.7);
            cairo_set_font_size(cr, 12);
        }

        cairo_move_to(cr, PAD, y);
        cairo_show_text(cr, lines[i].c_str());
    }

    cairo_destroy(cr);
    cairo_surface_flush(surf);

    uint8_t* data = cairo_image_surface_get_data(surf);
    int stride = cairo_image_surface_get_stride(surf);

    CSharedPointer<CTexture> tex(new CTexture(DRM_FORMAT_ARGB8888, data, stride, Vector2D{(double)w, (double)h}, true));

    outW = w;
    outH = h;

    cairo_surface_destroy(surf);
    return tex;
}

void rebuildPanelTexture(WORKSPACEID wsID) {
    if (!g_pCompositor) return;
    auto ws = g_pCompositor->getWorkspaceByID(wsID);
    if (!ws) return;

    std::vector<std::string> lines;

    /* header */
    lines.push_back("Workspace " + std::to_string(wsID) + "  [" + ws->m_name + "]");

    /* separator */
    lines.push_back("");

    /* collect windows */
    std::vector<WindowInfo> wins;
    for (auto& w : g_pCompositor->m_windows) {
        if (!w || w->m_workspace != ws || !w->m_isMapped) continue;
        wins.push_back({w->m_class, w->m_title});
    }

    if (wins.empty()) {
        lines.push_back("  (no windows)");
    } else {
        lines.push_back("Windows (" + std::to_string(wins.size()) + "):");
        for (auto& wi : wins) {
            std::string cls = wi.cls.empty() ? "?" : wi.cls;
            std::string ttl = wi.title;
            if (ttl.size() > 36) ttl = ttl.substr(0, 33) + "...";
            if (ttl.empty()) ttl = "(untitled)";
            lines.push_back("  " + cls + " — " + ttl);
        }
    }

    g_panelTex = renderTextToTexture(lines, g_panelW, g_panelH);
    g_panelWsID = wsID;
}

void addRect(CBox b, CHyprColor c, int round = 0) {
    CRectPassElement::SRectData d; d.color = c; d.box = b; d.round = round; d.roundingPower = 2.0f;
    g_pHyprRenderer->m_renderPass.add(makeUnique<CRectPassElement>(d));
}

void addBorder(CBox b, CHyprColor c, int s, int round = 0) {
    CBorderPassElement::SBorderData d;
    d.box = b; d.grad1 = c; d.round = round; d.roundingPower = 2.0f; d.a = 1; d.borderSize = s;
    g_pHyprRenderer->m_renderPass.add(makeUnique<CBorderPassElement>(d));
}

void renderWindowMini(PHLWINDOW w, PHLMONITOR m, CBox t, const Time::steady_tp& ti) {
    if (!w || !m || !w->m_isMapped || !w->wlSurface() || !w->wlSurface()->resource()) return;
    auto oP = w->m_realPosition->value();
    auto oS = w->m_realSize->value();
    float sc = t.w / std::max((float)oS.x * m->m_scale, 5.0f);
    if (sc <= 0 || t.w <= 0) return;

    Vector2D tr = t.pos() / sc - (oP + w->m_floatingOffset - m->m_position) * m->m_scale;

    SRenderModifData mod; mod.enabled = true;
    mod.modifs.push_back({SRenderModifData::RMOD_TYPE_TRANSLATE, std::any(tr)});
    mod.modifs.push_back({SRenderModifData::RMOD_TYPE_SCALE, std::any(sc)});
    g_pHyprRenderer->m_renderPass.add(makeUnique<CRendererHintsPassElement>(
        CRendererHintsPassElement::SData{.renderModif = mod}));

    CSurfacePassElement::SRenderData rd;
    rd.pMonitor = m; rd.when = ti;
    rd.pos = oP + w->m_floatingOffset;
    rd.w = std::max(oS.x, 5.0); rd.h = std::max(oS.y, 5.0);
    rd.surface = w->wlSurface()->resource(); rd.pWindow = w;
    rd.decorate = false; rd.blur = false; rd.alpha = 1; rd.fadeAlpha = 1;
    rd.clipBox = t; rd.squishOversized = true;
    rd.dontRound = w->m_fullscreenState.internal != FSMODE_NONE;
    rd.rounding = rd.dontRound ? 0 : (int)(w->rounding() * sc * m->m_scale);
    rd.roundingPower = w->roundingPower();

    w->wlSurface()->resource()->breadthfirst(
        [&rd, &w](SP<CWLSurfaceResource> s, const Vector2D& off, void*) {
            if (!s || !s->m_current.texture || s->m_current.size.x < 1) return;
            rd.localPos = off; rd.texture = s->m_current.texture;
            rd.surface = s; rd.mainSurface = (s == w->wlSurface()->resource());
            g_pHyprRenderer->m_renderPass.add(makeUnique<CSurfacePassElement>(rd));
        }, nullptr);

    g_pHyprRenderer->m_renderPass.add(makeUnique<CRendererHintsPassElement>(
        CRendererHintsPassElement::SData{.renderModif = SRenderModifData()}));
}

void initParticles() {
    g_particles.clear();
    CHyprColor colors[] = {
        CHyprColor(0.9, 0.4, 0.3, 0.6),
        CHyprColor(0.3, 0.6, 0.9, 0.6),
        CHyprColor(0.9, 0.8, 0.3, 0.6),
        CHyprColor(0.4, 0.8, 0.5, 0.6),
        CHyprColor(0.7, 0.4, 0.8, 0.6),
        CHyprColor(0.9, 0.6, 0.3, 0.6),
    };
    for (int i = 0; i < 12; ++i) {
        double sz = 3.0 + (i % 4) * 2.0;
        g_particles.push_back({
            .orbitR = 160.0 + (i % 3) * 80.0,
            .speed = 0.3 + (i % 4) * 0.15,
            .size = sz,
            .phase = TAU * i / 12.0,
            .color = colors[i % 6]
        });
    }
}

void buildCards() {
    g_cards.clear();
    auto ws = g_pCompositor->getWorkspacesCopy();
    for (auto& w : ws)
        if (w && !w->m_isSpecialWorkspace) g_cards.push_back({w->m_id, 0});
    std::sort(g_cards.begin(), g_cards.end(), [](auto& a, auto& b){ return a.id < b.id; });
    int n = g_cards.size();
    for (int i = 0; i < n; ++i)
        g_cards[i].angle = -TAU/4 + TAU*i/n;
}

void open() {
    if (g_visible) return;
    buildCards();
    if (g_cards.empty()) return;
    auto m = g_pCompositor->getMonitorFromCursor();
    if (!m) return;
    g_selected = 0;
    for (int i = 0; i < (int)g_cards.size(); ++i)
        if (g_cards[i].id == m->activeWorkspaceID()) { g_selected = i; break; }
    g_visible = true;
    g_panelTex = nullptr;
    g_panelWsID = -1;
    /* damage all monitors to force immediate render */
    for (auto& mon : g_pCompositor->m_monitors)
        if (mon && mon->m_enabled) {
            g_pHyprRenderer->damageMonitor(mon);
            g_pCompositor->scheduleFrameForMonitor(mon);
        }
}

void close() {
    if (!g_visible) return;
    g_visible = false;
    g_panelTex = nullptr;
    g_panelWsID = -1;
    for (auto& m : g_pCompositor->m_monitors)
        if (m && m->m_enabled) g_pHyprRenderer->damageMonitor(m);
}

void select() {
    if (g_selected < 0 || g_selected >= (int)g_cards.size()) return;
    HyprlandAPI::invokeHyprctlCommand("dispatch", "workspace " + std::to_string(g_cards[g_selected].id));
    close();
}

SDispatchResult dispatchOrbit(std::string args) {
    auto f = args.find_first_not_of(" \t\r\n");
    if (f == std::string::npos) args.clear(); else args.erase(0, f);
    if (args.empty() || args == "toggle") { g_visible ? close() : open(); return {false,true,""}; }
    if (args == "open") { open(); return {false,true,""}; }
    if (args == "close") { close(); return {false,true,""}; }
    return {false,false,""};
}

bool hitTest(double mx, double my, int& idx, PHLMONITOR mon) {
    if (!mon) return false;
    double cx = mon->m_size.x / 2.0, cy = mon->m_size.y / 2.0;
    for (int i = 0; i < (int)g_cards.size(); ++i) {
        double px = cx + std::cos(g_cards[i].angle) * ORBIT_R;
        double py = cy + std::sin(g_cards[i].angle) * ORBIT_R;
        if (std::hypot(mx-px, my-py) <= CARD_R * 1.2) { idx = i; return true; }
    }
    return false;
}

void renderOnMonitor(PHLMONITOR mon) {
    if (!mon || !g_visible) return;
    auto ti = Time::steadyNow();
    g_time += 1.0 / 60.0;

    double cx = mon->m_size.x / 2.0, cy = mon->m_size.y / 2.0;

    g_pHyprOpenGL->m_renderData.clipBox = {{0, 0}, mon->m_transformedSize};

    /* dim overlay */
    addRect({0.0, 0.0, mon->m_size.x, mon->m_size.y}, CHyprColor(0, 0, 0, 0.5));

    /* orbiting planets */
    for (auto& p : g_particles) {
        double a = p.phase + g_time * p.speed;
        double px = cx + std::cos(a) * p.orbitR;
        double py = cy + std::sin(a) * p.orbitR;
        double r = p.size;
        int round = (int)r;
        addRect({px - r, py - r, r * 2, r * 2}, p.color, round);
        addBorder({px - r - 1, py - r - 1, r * 2 + 2, r * 2 + 2},
                  CHyprColor(p.color.r, p.color.g, p.color.b, 0.2), 1, round + 1);
    }

    /* faint orbit ring */
    addBorder({cx - ORBIT_R, cy - ORBIT_R, ORBIT_R * 2, ORBIT_R * 2},
              CHyprColor(0.3, 0.5, 0.8, 0.12), 1);

    /* workspace cards */
    for (int i = 0; i < (int)g_cards.size(); ++i) {
        auto& c = g_cards[i];
        bool sel = (i == g_selected);
        double sc = sel ? 1.1 : 0.85;
        double r = CARD_R * sc;
        double px = cx + std::cos(c.angle) * ORBIT_R;
        double py = cy + std::sin(c.angle) * ORBIT_R;
        CBox box = {px-r, py-r, r*2, r*2};

        addRect(box, sel ? CHyprColor(0.2, 0.35, 0.6, 0.4) : CHyprColor(0.1, 0.1, 0.15, 0.5));

        auto ws = g_pCompositor->getWorkspaceByID(c.id);
        if (ws) for (auto& w : g_pCompositor->m_windows)
            if (w && w->m_workspace == ws && w->m_isMapped)
                renderWindowMini(w, mon, box, ti);

        addBorder(box, sel ? CHyprColor(0.4, 0.7, 1.0, 0.9) : CHyprColor(1,1,1,0.1), sel ? 3 : 1);

        if (sel) {
            double gr = r + 4;
            addBorder({px-gr, py-gr, gr*2, gr*2}, CHyprColor(0.3, 0.6, 1.0, 0.3), 2);
        }
    }

    /* side panel — workspace metadata */
    if (g_selected >= 0 && g_selected < (int)g_cards.size()) {
        WORKSPACEID wsID = g_cards[g_selected].id;

        if (g_panelTex == nullptr || g_panelWsID != wsID || g_panelSel != g_selected)
            rebuildPanelTexture(wsID);

        if (g_panelTex && g_panelW > 0 && g_panelH > 0) {
            double panelX = mon->m_size.x - g_panelW - 20;
            double panelY = (mon->m_size.y - g_panelH) / 2.0;

            CTexPassElement::SRenderData td;
            td.tex = g_panelTex;
            td.box = {panelX, panelY, g_panelW, g_panelH};
            td.a = 0.95f;
            td.clipBox = {{0, 0}, mon->m_transformedSize};
            g_pHyprRenderer->m_renderPass.add(makeUnique<CTexPassElement>(td));
        }
    }

    g_pHyprRenderer->damageMonitor(mon);
    g_pCompositor->scheduleFrameForMonitor(mon);
}

} // namespace

extern "C" EXPORT std::string pluginAPIVersion() { return HYPRLAND_API_VERSION; }

extern "C" EXPORT PLUGIN_DESCRIPTION_INFO pluginInit(HANDLE h) {
    g_pluginHandle = h;
    initParticles();

    g_renderHook = Event::bus()->m_events.render.stage.listen([](eRenderStage s) {
        if (s != RENDER_POST_WINDOWS) return;
        auto mon = g_pHyprOpenGL->m_renderData.pMonitor.lock();
        if (!mon) for (auto& m : g_pCompositor->m_monitors) if (m && m->m_enabled) { mon = m; break; }
        if (mon) renderOnMonitor(mon);
    });

    g_mouseHook = Event::bus()->m_events.input.mouse.button.listen(
        [](const IPointer::SButtonEvent& e, Event::SCallbackInfo& info) {
            if (!g_visible || e.button != BTN_LEFT || e.state != WL_POINTER_BUTTON_STATE_PRESSED) return;
            info.cancelled = true;
            auto mon = g_pCompositor->getMonitorFromCursor();
            if (!mon) return;
            auto pos = g_pPointerManager->position();
            double mx = pos.x - mon->m_position.x;
            double my = pos.y - mon->m_position.y;
            int hit = -1;
            if (hitTest(mx, my, hit, mon)) { g_selected = hit; select(); }
        });

    g_mouseMoveHook = Event::bus()->m_events.input.mouse.move.listen(
        [](const Vector2D& cursorPos, Event::SCallbackInfo& info) {
            if (!g_visible) return;
            auto mon = g_pCompositor->getMonitorFromCursor();
            if (!mon) return;
            double mx = cursorPos.x - mon->m_position.x;
            double my = cursorPos.y - mon->m_position.y;
            int hit = -1;
            if (hitTest(mx, my, hit, mon)) g_selected = hit;
        });

    g_keyHook = Event::bus()->m_events.input.keyboard.key.listen(
        [](const IKeyboard::SKeyEvent& e, Event::SCallbackInfo& info) {
            if (!g_visible || e.state != WL_KEYBOARD_KEY_STATE_PRESSED) return;
            int n = g_cards.size();
            if (n == 0) { if (e.keycode == KEY_ESC) { close(); info.cancelled = true; } return; }
            if (e.keycode == KEY_ESC) { close(); info.cancelled = true; return; }
            if (e.keycode == KEY_LEFT)  { g_selected = (g_selected - 1 + n) % n; info.cancelled = true; return; }
            if (e.keycode == KEY_RIGHT) { g_selected = (g_selected + 1) % n; info.cancelled = true; return; }
            if (e.keycode == KEY_ENTER || e.keycode == KEY_KPENTER) { select(); info.cancelled = true; }
        });

    HyprlandAPI::addDispatcherV2(h, "hypr-orbit", dispatchOrbit);
    return {"hypr-orbit", "Fast workspace switcher", "LuX404", "0.9.0"};
}

extern "C" EXPORT void pluginExit() {
    g_renderHook.reset(); g_mouseHook.reset(); g_mouseMoveHook.reset(); g_keyHook.reset();
    if (g_pluginHandle) HyprlandAPI::removeDispatcher(g_pluginHandle, "hypr-orbit");
    g_cards.clear(); g_particles.clear(); g_visible = false; g_panelTex = nullptr; g_pluginHandle = nullptr;
}
