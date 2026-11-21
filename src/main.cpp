// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Bryan Lunduke
// See COPYING and NOTICE.

// Lunduke Civ: the SDL3 window. Everything is drawn with Cairo and Pango
// into one image, as in Lunduke City 2, and handed to SDL as a texture.

#include "app.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <cstdio>
#include <cstring>
#include <string>

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

} // namespace

int main(int argc, char **argv)
{
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

    bool dirty = true;
    bool text_on = false;
    while (!app.quit_requested()) {
        SDL_Event e;
        if (SDL_WaitEventTimeout(&e, 60)) {
            do {
                switch (e.type) {
                case SDL_EVENT_QUIT: return SDL_Quit(), 0;
                case SDL_EVENT_KEY_DOWN: dirty |= app.key(translate(e.key)); break;
                case SDL_EVENT_TEXT_INPUT: dirty |= app.text_input(e.text.text); break;
                case SDL_EVENT_MOUSE_BUTTON_DOWN:
                    dirty |= app.mouse_down(e.button.x, e.button.y, e.button.button == SDL_BUTTON_RIGHT ? 3 : 1);
                    break;
                case SDL_EVENT_MOUSE_MOTION: dirty |= app.mouse_move(e.motion.x, e.motion.y); break;
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
    if (surface) cairo_surface_destroy(surface);
    if (tex) SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
