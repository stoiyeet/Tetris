#pragma once

#include <SDL2/SDL_keycode.h>
#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <memory>

#include <SDL2/SDL.h>

#include "platform.hpp"


#define SDL_CHECK_CODE(x) SDLCheckCode(x, __FILE__, __LINE__)
#define SDL_CHECK_PTR(p) SDLCheckPtr(p, __FILE__, __LINE__)
#define SDL_ERROR_HERE() SDLError(__FILE__, __LINE__)

#define PANIC_HERE(kind, msg) PANIC(kind, msg, __FILE__, __LINE__)
#define PANIC(kind, msg, file, line) (fprintf(stderr, "%s:%d: " kind " ERROR: %s\n", file, line, msg), exit(1))

static void SDLError(const char* file, int line) {
    PANIC("SDL", SDL_GetError(), file, line);
}

static int SDLCheckCode(int code, const char* file, int line) {
    if (code < 0)
        SDLError(file, line);
    return code;
}

template<typename T>
static T* SDLCheckPtr(T* ptr, const char* file, int line) {
    if (ptr == NULL)
        SDLError(file, line);
    return ptr;
}

static void PollEvents() {
    for (SDL_Event e; SDL_PollEvent(&e);) {
        switch (e.type) {
            case SDL_QUIT: {
                keepRunning = false;
            } break;

            case SDL_KEYDOWN:
            case SDL_KEYUP: {
                bool pressed = e.type == SDL_KEYDOWN;
                SDL_Keycode code = e.key.keysym.sym;
                // SDL_Keymod mod = static_cast<SDL_Keymod>(e.key.keysym.mod);

                timepoint now = std::chrono::system_clock::now().time_since_epoch().count();

                KeyPress::KeyPress key;
                switch (code) {
                    case SDLK_LEFT:  key = KeyPress::Left;  break;
                    case SDLK_RIGHT: key = KeyPress::Right; break;
                    case SDLK_UP:    key = KeyPress::Up;    break;
                    case SDLK_DOWN:  key = KeyPress::Down;  break;
                    case SDLK_SPACE: key = KeyPress::Space; break;
                    case SDLK_c:     key = KeyPress::c;     break;
                    case SDLK_r:     key = KeyPress::r;     break;
                    case SDLK_z:     key = KeyPress::z;     break;
                    default:         key = KeyPress::None;  break;
                }

                if (key == KeyPress::None) {
                    break;
                }

                if (pressed) {
                    bool isFirstPress = lastPress[key] <= lastRelease[key];
                    if (isFirstPress) {
                        lastPress[key] = now;
                    }
                }
                else {
                    lastRelease[key] = now;
                }
            } break;
        }
    }
}


static constexpr size_t Scale = 50;

template<size_t Width, size_t Height>
struct Screen<Width, Height>::Impl {
	Color buffer[Width * Height];

    SDL_Window* window;
    SDL_Renderer* renderer;
};

template<size_t Width, size_t Height>
void Screen<Width, Height>::ClearBuffer() {
    for (size_t i = 0; i < Width * Height; ++i) {
        pimpl->buffer[i] = Color::Black;
    }
}

template<size_t Width, size_t Height>
void Screen<Width, Height>::SetPixel(size_t x, size_t y, Color color) {
    if (x < Width && y < Height) {
        pimpl->buffer[x + y * Width] = color;
    }
}

template<size_t Width, size_t Height>
void Screen<Width, Height>::ClearScreen() {
    SDL_SetRenderDrawColor(pimpl->renderer, 0, 0, 0, 0xFF);
    SDL_RenderClear(pimpl->renderer);
}

template<size_t Width, size_t Height>
void Screen<Width, Height>::RedrawScreen() {
    PollEvents();

    uint32_t prevColor;
    auto SetColor = [&prevColor, this](uint32_t color) {
        SDL_SetRenderDrawColor(pimpl->renderer,
               (color & 0xFF0000) >> 16,
               (color & 0x00FF00) >> 8,
               (color & 0x0000FF) >> 0,
               255);
        prevColor = color;
    };
    SetColor(0);

    for (size_t y = 0; y < Height; ++y) {
        for (size_t x = 0; x < Width; ++x) {
            uint32_t color = pimpl->buffer[x + y * Width];

            if (color != prevColor) {
                SetColor(color);
            }

            SDL_Rect r{.x=static_cast<int>(x*Scale), .y=static_cast<int>(y*Scale),.w=Scale,.h=Scale};
            SDL_RenderFillRect(pimpl->renderer, &r);
        }
    }

    SDL_RenderPresent(pimpl->renderer);
}


template<size_t Width, size_t Height>
Screen<Width, Height>::Screen()
    : pimpl{std::make_unique<Impl>()}
{
    pimpl->window = SDL_CHECK_PTR(SDL_CreateWindow(
        "Tetris",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        Width * Scale, Height * Scale,
        0));

    pimpl->renderer = SDL_CHECK_PTR(SDL_CreateRenderer(
        pimpl->window,
        -1,
        SDL_RENDERER_ACCELERATED));
}

template<size_t Width, size_t Height>
Screen<Width, Height>::~Screen() = default;


void ContinuouslyReadInput() {
    while (keepRunning) {
        PollEvents();
        // Don't kill cpu
        std::this_thread::sleep_for(1ms);
    }
}

void InitializeScreen() {
    SDL_CHECK_CODE(SDL_Init(SDL_INIT_VIDEO));
}

void DestroyScreen() {
    SDL_Quit();
}
