#pragma once

#include <cstdint>
#include <memory>
#include <thread>
#include <chrono>
using namespace std::chrono_literals;

struct Color {
    constexpr static uint32_t Black = 0x000000;
    constexpr static uint32_t White = 0xFFFFFF;
    constexpr static uint32_t Cyan = 0x00FFFF;
    constexpr static uint32_t Yellow = 0xFFFF00;
    constexpr static uint32_t Pink = 0xFF00FF;
    constexpr static uint32_t Orange = 0xFF7F00;
    // constexpr static uint32_t Blue = 0x0000FF;
    constexpr static uint32_t Blue = 0x3333FF;
    constexpr static uint32_t Green = 0x00FF00;
    constexpr static uint32_t Red = 0xFF0000;

    constexpr Color(uint32_t x=Black) : value{x} {}
    constexpr operator uint32_t() const { return value; }

    uint32_t value;

    Color DimColor(uint8_t tint) const {
        // This is almost definitely the wrong way to do this
        return
            (uint32_t(((value & 0xFF0000) >> 16) * tint / 256.0) << 16) |
            (uint32_t(((value & 0x00FF00) >> 8) * tint / 256.0) << 8) |
            (uint32_t(((value & 0x0000FF) >> 0) * tint / 256.0) << 0);
    }
};

namespace KeyPress {
    enum KeyPress : int {
        None = 0, Left, Right, Up, Down, Space, c, z, r, COUNT,
    };
}

using timepoint = std::chrono::system_clock::duration::rep;
inline volatile bool keepRunning = true;
inline volatile timepoint lastPress[KeyPress::COUNT];
inline volatile timepoint lastRelease[KeyPress::COUNT];

inline bool IsKeyPressed(KeyPress::KeyPress key) {
    return lastPress[key] > lastRelease[key];
}


template<size_t Width, size_t Height>
class Screen {
    struct Impl;

    std::unique_ptr<Impl> pimpl;

public:
    Screen();
    //     : Impl{std::make_unique<Impl>}
    // {
    //     ClearBuffer();
    //     ClearScreen();
    // }
    ~Screen();// = default;

    void ClearBuffer();
    void SetPixel(size_t x, size_t y, Color color);
    void ClearScreen();
    void RedrawScreen();
};


void InitializeScreen();
void DestroyScreen();
void ContinuouslyReadInput();
