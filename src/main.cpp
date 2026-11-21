// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Bryan Lunduke
// See COPYING and NOTICE.

// Lunduke Civ: the SDL3 window. Everything is drawn with Cairo and Pango
// into one image, as in Lunduke City 2, and handed to SDL as a texture.

#include "app.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

ui::KeyEvent translate(const SDL_KeyboardEvent &e)
{
    ui::KeyEvent k;
    k.ctrl = (e.mod & SDL_KMOD_CTRL) != 0;
    k.shift = (e.mod & SDL_KMOD_SHIFT) != 0;
    const SDL_Keycode c = e.key;
    switch (c) {
    case SDLK_UP: k.key = ui::Key::Up; break;
    case SDLK_DOWN: k.key = ui::Key::Down; break;
    case SDLK_LEFT: k.key = ui::Key::Left; break;
    case SDLK_RIGHT: k.key = ui::Key::Right; break;
    case SDLK_HOME: k.key = ui::Key::UpLeft; break;
    case SDLK_PAGEUP: k.key = ui::Key::UpRight; break;
    case SDLK_END: k.key = ui::Key::DownLeft; break;
    case SDLK_PAGEDOWN: k.key = ui::Key::DownRight; break;
    case SDLK_RETURN:
    case SDLK_KP_ENTER: k.key = ui::Key::Enter; break;
    case SDLK_ESCAPE: k.key = ui::Key::Escape; break;
    case SDLK_SPACE: k.key = ui::Key::Space; break;
    case SDLK_BACKSPACE: k.key = ui::Key::Backspace; break;
    case SDLK_TAB: k.key = ui::Key::Tab; break;
    case SDLK_F1:
        k.key = ui::Key::Char;
        k.ch = '?';
        break;
    case SDLK_KP_PLUS:
    case SDLK_EQUALS:
    case SDLK_PLUS:
        k.key = ui::Key::Char;
        k.ch = '+';
        break;
    case SDLK_KP_MINUS:
    case SDLK_MINUS:
        k.key = ui::Key::Char;
        k.ch = '-';
        break;
    default:
        if (c >= SDLK_KP_1 && c <= SDLK_KP_9) {
            k.key = ui::Key::Char;
            k.ch = static_cast<char>('1' + (c - SDLK_KP_1));
        } else if ((c >= SDLK_A && c <= SDLK_Z) || (c >= SDLK_0 && c <= SDLK_9)) {
            k.key = ui::Key::Char;
            k.ch = static_cast<char>(c);
        } else if (c == SDLK_SLASH && k.shift) {
            k.key = ui::Key::Char;
            k.ch = '?';
        }
        break;
    }
    return k;
}

// Gamepads, as in Lunduke City 2: open every pad SDL offers, let the one
// last pressed lead, and name its buttons the way that pad prints them.
struct Pads {
    std::vector<SDL_Gamepad *> open;
    SDL_Gamepad *active = nullptr;
    bool deck = false;
    float lx = 0, ly = 0, rx = 0, ry = 0;

    void name_buttons(ui::App &app) const
    {
        bool ps = false, ps5 = false;
        if (active) {
            const SDL_GamepadType t = SDL_GetGamepadType(active);
            ps = t == SDL_GAMEPAD_TYPE_PS3 || t == SDL_GAMEPAD_TYPE_PS4 || t == SDL_GAMEPAD_TYPE_PS5 ||
                 SDL_GetGamepadButtonLabel(active, SDL_GAMEPAD_BUTTON_SOUTH) == SDL_GAMEPAD_BUTTON_LABEL_CROSS;
            ps5 = t == SDL_GAMEPAD_TYPE_PS5;
        }
        app.set_pad_glyphs(pad::glyphs_for(ps, deck && !ps, ps5), deck);
    }
    void add(SDL_JoystickID id, bool lead, ui::App &app)
    {
        for (SDL_Gamepad *p : open) {
            if (SDL_GetGamepadID(p) == id) {
                if (lead && active != p) {
                    active = p;
                    name_buttons(app);
                }
                return;
            }
        }
        if (SDL_Gamepad *p = SDL_OpenGamepad(id)) {
            open.push_back(p);
            if (lead || !active) {
                active = p;
                name_buttons(app);
            }
        }
    }
    void remove(SDL_JoystickID id, ui::App &app)
    {
        for (size_t i = 0; i < open.size(); ++i) {
            if (SDL_GetGamepadID(open[i]) != id) continue;
            if (active == open[i]) {
                active = nullptr;
                app.pad_unplugged();
            }
            SDL_CloseGamepad(open[i]);
            open.erase(open.begin() + static_cast<long>(i));
            break;
        }
        if (!active && !open.empty()) active = open.front();
        name_buttons(app);
    }
};

bool pad_button(SDL_GamepadButton b, ui::PadButton &out)
{
    switch (b) {
    case SDL_GAMEPAD_BUTTON_SOUTH: out = ui::PadButton::A; return true;
    case SDL_GAMEPAD_BUTTON_EAST: out = ui::PadButton::B; return true;
    case SDL_GAMEPAD_BUTTON_WEST: out = ui::PadButton::X; return true;
    case SDL_GAMEPAD_BUTTON_NORTH: out = ui::PadButton::Y; return true;
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: out = ui::PadButton::LB; return true;
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: out = ui::PadButton::RB; return true;
    case SDL_GAMEPAD_BUTTON_BACK: out = ui::PadButton::View; return true;
    case SDL_GAMEPAD_BUTTON_START: out = ui::PadButton::Menu; return true;
    case SDL_GAMEPAD_BUTTON_DPAD_UP: out = ui::PadButton::Up; return true;
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN: out = ui::PadButton::Down; return true;
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT: out = ui::PadButton::Left; return true;
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: out = ui::PadButton::Right; return true;
    case SDL_GAMEPAD_BUTTON_LEFT_STICK: out = ui::PadButton::L3; return true;
    case SDL_GAMEPAD_BUTTON_RIGHT_STICK: out = ui::PadButton::R3; return true;
    default: return false;
    }
}

} // namespace

int main(int argc, char **argv)
{
#ifdef _WIN32
    // Render text through fontconfig and the fonts shipped next to the exe
    // (etc/fonts, share/fonts) so Windows matches the Linux look and metrics.
    // Setting PANGOCAIRO_BACKEND=win32 beforehand restores native fonts.
    if (std::getenv("PANGOCAIRO_BACKEND") == nullptr) {
        _putenv_s("PANGOCAIRO_BACKEND", "fc");
    }
#endif
    std::string load_path;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--version")) {
            std::printf("Lunduke Civ %s\n", LUNDUKE_CIV_VERSION);
            return 0;
        }
        if (!std::strcmp(argv[i], "--help")) {
            std::printf("Usage: lunduke-civ [SAVED-GAME]\n");
            return 0;
        }
        load_path = argv[i];
    }
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    // Gamepads are optional: play on without them if SDL can't offer any.
    const bool have_pads = SDL_InitSubSystem(SDL_INIT_GAMEPAD);
    Pads pads;
    if (const char *deck = std::getenv("SteamDeck")) pads.deck = deck[0] == '1';
    if (const char *hint = SDL_GetHint("SteamDeck")) pads.deck = pads.deck || hint[0] == '1';
    SDL_Window *win = SDL_CreateWindow("Lunduke Civ", 1280, 800, SDL_WINDOW_RESIZABLE);
    if (!win) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_SetWindowMinimumSize(win, 1024, 700);
    SDL_Renderer *ren = SDL_CreateRenderer(win, nullptr);
    SDL_Texture *tex = nullptr;
    cairo_surface_t *surface = nullptr;
    int sw = 0, sh = 0;

    ui::App app;
    if (char *pref = SDL_GetPrefPath("Lunduke", "Lunduke Civ")) {
        app.set_save_dir(pref);
        SDL_free(pref);
    }
    std::string err;
    if (load_path.empty() || !app.load_file(load_path, &err)) {
        if (!err.empty()) std::fprintf(stderr, "%s\n", err.c_str());
        app.open_new_game_dialog();
    }

    pads.name_buttons(app);
    (void)have_pads;
    bool dirty = true;
    bool text_on = false;
    while (!app.quit_requested()) {
        SDL_Event e;
        if (SDL_WaitEventTimeout(&e, app.pad_busy() ? 16 : 60)) {
            do {
                switch (e.type) {
                case SDL_EVENT_QUIT: return SDL_Quit(), 0;
                case SDL_EVENT_KEY_DOWN:
                    app.pointer_used();
                    dirty |= app.key(translate(e.key));
                    break;
                case SDL_EVENT_GAMEPAD_ADDED: pads.add(e.gdevice.which, false, app); break;
                case SDL_EVENT_GAMEPAD_REMOVED:
                    pads.remove(e.gdevice.which, app);
                    dirty = true;
                    break;
                case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
                case SDL_EVENT_GAMEPAD_BUTTON_UP: {
                    pads.add(e.gbutton.which, true, app);
                    ui::PadButton b;
                    if (pad_button(static_cast<SDL_GamepadButton>(e.gbutton.button), b))
                        dirty |= app.pad_button(b, e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN, SDL_GetTicks()) || true;
                    break;
                }
                case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
                    pads.add(e.gaxis.which, true, app);
                    const float v = std::max(-1.0f, std::min(1.0f, e.gaxis.value / 32767.0f));
                    switch (e.gaxis.axis) {
                    case SDL_GAMEPAD_AXIS_LEFTX: pads.lx = v; break;
                    case SDL_GAMEPAD_AXIS_LEFTY: pads.ly = v; break;
                    case SDL_GAMEPAD_AXIS_RIGHTX: pads.rx = v; break;
                    case SDL_GAMEPAD_AXIS_RIGHTY: pads.ry = v; break;
                    case SDL_GAMEPAD_AXIS_LEFT_TRIGGER:
                    case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER: {
                        // Triggers zoom, once per pull.
                        static bool held[2] = {false, false};
                        const int i = e.gaxis.axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER;
                        if (v > 0.6f && !held[i]) {
                            held[i] = true;
                            dirty |= app.pad_button(i ? ui::PadButton::RT : ui::PadButton::LT, true, SDL_GetTicks());
                        } else if (v < 0.3f) {
                            held[i] = false;
                        }
                        break;
                    }
                    default: break;
                    }
                    dirty |= app.pad_sticks(pads.lx, pads.ly, pads.rx, pads.ry, SDL_GetTicks());
                    break;
                }
                case SDL_EVENT_TEXT_INPUT: dirty |= app.text_input(e.text.text); break;
                case SDL_EVENT_MOUSE_BUTTON_DOWN:
                    dirty |= app.mouse_down(e.button.x, e.button.y, e.button.button == SDL_BUTTON_RIGHT ? 3 : 1);
                    break;
                case SDL_EVENT_MOUSE_MOTION:
                    if (std::fabs(e.motion.xrel) + std::fabs(e.motion.yrel) > 2) app.pointer_used();
                    dirty |= app.mouse_move(e.motion.x, e.motion.y);
                    break;
                case SDL_EVENT_MOUSE_WHEEL: {
                    float mx = 0, my = 0;
                    SDL_GetMouseState(&mx, &my);
                    dirty |= app.wheel(mx, my, e.wheel.y);
                    break;
                }
                case SDL_EVENT_WINDOW_RESIZED:
                case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                case SDL_EVENT_WINDOW_EXPOSED: dirty = true; break;
                default: break;
                }
            } while (SDL_PollEvent(&e));
        }
        dirty |= app.tick(SDL_GetTicks());
        if (app.wants_text() != text_on) {
            text_on = app.wants_text();
            if (text_on) {
                SDL_StartTextInput(win);
            } else {
                SDL_StopTextInput(win);
            }
            dirty = true;
        }
        if (text_on) dirty |= app.tick(SDL_GetTicks()) || true;
        if (!dirty) continue;
        int w = 0, h = 0;
        SDL_GetWindowSizeInPixels(win, &w, &h);
        if (w != sw || h != sh || !surface) {
            if (surface) cairo_surface_destroy(surface);
            if (tex) SDL_DestroyTexture(tex);
            sw = w;
            sh = h;
            surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, w, h);
            tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, w, h);
        }
        cairo_t *cr = cairo_create(surface);
        app.render(cr, w, h);
        cairo_destroy(cr);
        cairo_surface_flush(surface);
        SDL_UpdateTexture(tex, nullptr, cairo_image_surface_get_data(surface), cairo_image_surface_get_stride(surface));
        SDL_RenderTexture(ren, tex, nullptr, nullptr);
        SDL_RenderPresent(ren);
        dirty = false;
    }
    for (SDL_Gamepad *p : pads.open) SDL_CloseGamepad(p);
    if (surface) cairo_surface_destroy(surface);
    if (tex) SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
