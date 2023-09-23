#pragma once

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <memory>

#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <linux/input.h>
#include <csignal>

#include "platform.hpp"

static constexpr size_t HorizontalStretch = 2;
static constexpr size_t VerticalStretch = 1;

template<size_t Width, size_t Height>
struct Screen<Width, Height>::Impl {
    static constexpr size_t BufferSize = Width * HorizontalStretch * Height * VerticalStretch;

	Color buffer[BufferSize];
};

template<size_t Width, size_t Height>
void Screen<Width, Height>::ClearBuffer() {
    for (size_t i = 0; i < pimpl->BufferSize; ++i) {
        pimpl->buffer[i] = Color::Black;
    }
}

template<size_t Width, size_t Height>
void Screen<Width, Height>::SetPixel(size_t x, size_t y, Color color) {
    if (x < Width && y < Height) {
        for (size_t dy = 0; dy < VerticalStretch; ++dy) {
            for (size_t dx = 0; dx < HorizontalStretch; ++dx) {
                size_t idx_x = x * HorizontalStretch + dx;
                size_t idx_y = y * VerticalStretch + dy;
                size_t idx_w = Width * HorizontalStretch;
                pimpl->buffer[idx_x + idx_y * idx_w] = color;
            }
        }
    }
}

template<size_t Width, size_t Height>
void Screen<Width, Height>::ClearScreen() {
    printf("\x1b[2J"); // Clear screen, cursor to home
}

template<size_t Width, size_t Height>
void Screen<Width, Height>::RedrawScreen() {
    // Cursor to home, buffer
    printf("\x1b[H");

    uint32_t prevColor;
    auto SetColor = [&prevColor](uint32_t color) {
        printf("\x1b[48;2;%d;%d;%dm",
               (color & 0xFF0000) >> 16,
               (color & 0x00FF00) >> 8,
               (color & 0x0000FF) >> 0);
        prevColor = color;
    };
    SetColor(0);

    for (size_t y = 0; y < Height*VerticalStretch; ++y) {
        for (size_t x = 0; x < Width*HorizontalStretch; ++x) {
            uint32_t color = pimpl->buffer[x + y * Width * HorizontalStretch];
            if (color != prevColor) {
                SetColor(color);
            }
            // TODO: Coalesce prints
            putchar(' ');
        }
        putchar('\n');
    }

    // Reset color
    printf("\033[m");
}


template<size_t Width, size_t Height>
Screen<Width, Height>::Screen()
    : pimpl{std::make_unique<Impl>()}
{
    ClearBuffer();
    ClearScreen();
}

template<size_t Width, size_t Height>
Screen<Width, Height>::~Screen() = default;


static struct termios original_termios;

static void ResetTerminalMode() {
    tcsetattr(0, TCSANOW, &original_termios);
    puts("\e[?25h"); // Show cursor
}

static void SetTerminalToRawMode() {
    puts("\e[?25l"); // Hide cursor

    // Save original mode to restore when done
    tcgetattr(0, &original_termios);
    atexit(ResetTerminalMode);

    // Set no echo and char buffered
    struct termios new_termios;
    memcpy(&new_termios, &original_termios, sizeof(new_termios));
    new_termios.c_lflag &= static_cast<tcflag_t>(~ICANON);
    new_termios.c_lflag &= static_cast<tcflag_t>(~ECHO);
    tcsetattr(0, TCSANOW, &new_termios);
}

static void CustomExit(int) {
    ResetTerminalMode();
    exit(1);
}


void ContinuouslyReadInput() {
    // FIXME: This is bad
    int keyboard_fd = open("/dev/input/event2", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (keyboard_fd < 0) {
        perror("Error opening device");
        exit(1);
    }

    struct input_event events[32];
    while (true) {
        ssize_t len;
        while ((len = read(keyboard_fd, events, sizeof(events))) > 0) {
            len /= sizeof(events[0]);
            for (size_t i = 0; i < static_cast<size_t>(len); ++i) {

                struct input_event *event = &events[i];
                if (event->type == EV_KEY) {
                    // 0 = released
                    // 1 = pressed
                    // 2 = held
                    if (event->value == 2) {
                        continue;
                    }
                    bool pressed = event->value != 0;
                    timepoint now = std::chrono::system_clock::now().time_since_epoch().count();

                    switch (event->code) {
                        case 105: (pressed ? lastPress : lastRelease)[KeyPress::Left]  = now; break;
                        case 106: (pressed ? lastPress : lastRelease)[KeyPress::Right] = now; break;
                        case 103: (pressed ? lastPress : lastRelease)[KeyPress::Up]    = now; break;
                        case 108: (pressed ? lastPress : lastRelease)[KeyPress::Down]  = now; break;
                        case 57:  (pressed ? lastPress : lastRelease)[KeyPress::Space] = now; break;
                        case 46:  (pressed ? lastPress : lastRelease)[KeyPress::c]     = now; break;
                        case 19:  (pressed ? lastPress : lastRelease)[KeyPress::r]     = now; break;
                        case 44:  (pressed ? lastPress : lastRelease)[KeyPress::z]     = now; break;
                    }
                }
                // Ignore all other event types
            }
        }
        // Don't kill cpu
        std::this_thread::sleep_for(1ms);
    }
}

void InitializeScreen() {
    // Handlers for gracefully exiting
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = CustomExit;
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGKILL, &action, NULL);
    sigaction(SIGINT, &action, NULL);

    SetTerminalToRawMode();
}

void DestroyScreen() {
    ResetTerminalMode();
}
