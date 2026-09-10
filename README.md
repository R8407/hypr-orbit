# hypr-orbit

## Demo
https://github.com/user-attachments/assets/382227c5-4033-4f3e-a5e2-14cd0d020c39

A radial workspace navigator plugin for [Hyprland](https://hyprland.org).

## TL;DR

```bash
git clone https://github.com/R8407/hypr-orbit.git
cd hypr-orbit
make
hyprctl plugin load ./hypr-orbit.so
hyprctl dispatch hypr-orbit toggle
```

Bind a key to `hyprctl dispatch hypr-orbit toggle` and press it to open the orbit. Navigate with arrow keys or mouse, press Enter/Click to switch.

## What It Is

hypr-orbit is a Hyprland plugin that replaces the traditional numbered workspace list with a visual radial navigator. Workspaces are arranged in a circle around your screen center. Each workspace is displayed as a live thumbnail card showing its actual windows. You navigate between them using your keyboard, mouse, or both.

The plugin hooks directly into Hyprland's render pipeline using the official plugin API (v0.54.3). It renders an overlay on top of your desktop using Cairo for text and Hyprland's native render pass system for surfaces, borders, and textures.

## Features

- **Radial workspace layout** -- workspaces orbit around a central point on your screen
- **Live window thumbnails** -- each card renders actual workspace contents, not static images
- **Side panel** -- displays workspace metadata (ID, name, window count, window titles) for the highlighted card
- **Particle effects** -- animated orbiting particles for visual flair
- **Keyboard navigation** -- Arrow keys to cycle, Enter to select, Escape to close
- **Mouse navigation** -- hover to highlight, click to select
- **Single dispatcher** -- one command: `hyprctl dispatch hypr-orbit toggle`
- **Non-invasive** -- hooks into render events, doesn't modify workspace state until you select

## How It Works (Internals)

The plugin registers a custom Hyprland dispatcher (`hypr-orbit`) and hooks into four event buses:

| Hook | Purpose |
|---|---|
| `render.stage` (RENDER_POST_WINDOWS) | Draws the overlay after all windows are rendered |
| `input.mouse.button` | Handles click-to-select on cards |
| `input.mouse.move` | Handles hover highlighting |
| `input.keyboard.key` | Handles arrow keys, Enter, Escape |

**Rendering pipeline:**
1. On each frame (when visible), the plugin iterates all non-special workspaces via `g_pCompositor->getWorkspacesCopy()`
2. Each workspace gets a `Card` struct with an angle calculated as `(-pi/2) + (2*pi * i / n)` where `n` is the total workspace count
3. Cards are positioned at `(cx + cos(angle) * ORBIT_R, cy + sin(angle) * ORBIT_R)` where `ORBIT_R = 300px`
4. Each card renders its workspace's mapped windows as scaled-down thumbnails using `CSurfacePassElement` with `SRenderModifData` for scale/translate transforms
5. A Cairo-rendered side panel shows workspace info as a texture (`CTexture`) composited via `CTexPassElement`
6. 12 animated particles orbit at varying radii and speeds for visual polish

**Dispatcher commands:**
- `toggle` / (no args) -- open or close the orbit
- `open` -- force open
- `close` -- force close

Closing the orbit with a card selected switches to that workspace via `HyprlandAPI::invokeHyprctlCommand("dispatch", "workspace <id>")`.

## Requirements

- Hyprland 0.54.3 (plugin API v0.54.3)
- `g++` with C++26 support
- `pkg-config` with `hyprland` and `cairo` development files

## Installation

### Build from source

```bash
git clone https://github.com/R8407/hypr-orbit.git
cd hypr-orbit
make
```

### Install system-wide

```bash
sudo make install
```

This installs to `/usr/local/lib/hyprland/hypr-orbit.so`.

### Load the plugin

```bash
hyprctl plugin load /usr/local/lib/hyprland/hypr-orbit.so
```

Or for local testing:

```bash
hyprctl plugin load "$PWD/hypr-orbit.so"
```

### Unload

```bash
hyprctl plugin unload hypr-orbit
```

## Usage

### Dispatcher

```
hyprctl dispatch hypr-orbit          # toggle open/close
hyprctl dispatch hypr-orbit toggle   # same as above
hyprctl dispatch hypr-orbit open     # force open
hyprctl dispatch hypr-orbit close    # force close
```

### Keybinding (hyprland.conf)

```ini
bind = SUPER, TAB, exec, hyprctl dispatch hypr-orbit toggle
```

### Navigation

When the orbit is open:

| Input | Action |
|---|---|
| Left Arrow | Previous workspace |
| Right Arrow | Next workspace |
| Enter / Keypad Enter | Switch to selected workspace |
| Escape | Close orbit |
| Mouse hover | Highlight card under cursor |
| Left Click | Select and switch |

## Auto-load on startup

Add to your `hyprland.conf`:

```ini
exec-once = hyprctl plugin load /usr/local/lib/hyprland/hypr-orbit.so
```

## hyprpm (Hyprland Plugin Manager)

```bash
hyprpm add https://github.com/R8407/hypr-orbit
hyprpm enable hypr-orbit
```

## License

MIT
