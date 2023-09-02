#include <cstdint>
#include <cstdio>
#include <cmath>
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <csignal>

#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <linux/input.h>

#include <thread>
#include <chrono>
using namespace std::chrono_literals;

#define UNREACHABLE assert(0 && "Unreachable")


enum class Color : uint32_t {
    Black = 0x000000,
    White = 0xFFFFFF,
    Cyan = 0x00FFFF,
    Yellow = 0xFFFF00,
    Pink = 0xFF00FF,
    Orange = 0xFF7F00,
    Blue = 0x0000FF,
    Green = 0x00FF00,
    Red = 0xFF0000,
};

template<
    size_t _Width=12,
    size_t _Height=22,
    size_t HorizontalStretch=2,
    size_t VerticalStretch=1
>
struct Screen {
    static constexpr size_t Width = _Width;
    static constexpr size_t Height = _Height;
    static constexpr size_t BufferSize = Width * HorizontalStretch * Height * VerticalStretch;

	Color _buffer[BufferSize];

    Screen() {
        ClearBuffer();
        ClearScreen();
    }

    void ClearBuffer() {
        for (size_t i = 0; i < BufferSize; ++i) {
            _buffer[i] = Color::Black;
        }
    }

    void SetPixel(size_t x, size_t y, Color color) {
        if (x < Width && y < Height) {
            for (size_t dy = 0; dy < VerticalStretch; ++dy) {
                for (size_t dx = 0; dx < HorizontalStretch; ++dx) {
                    size_t idx_x = x * HorizontalStretch + dx;
                    size_t idx_y = y * VerticalStretch + dy;
                    size_t idx_w = Width * HorizontalStretch;
                    _buffer[idx_x + idx_y * idx_w] = color;
                }
            }
        }
    }

    void ClearScreen() {
        puts("\x1b[2J"); // Clear screen, cursor to home
    }

    void RedrawScreen() {
        // Cursor to home, buffer
        puts("\x1b[H");

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
                uint32_t color = static_cast<uint32_t>(_buffer[x + y * Width * HorizontalStretch]);
                if (color != prevColor) {
                    SetColor(color);
                }
                // TODO: Coalesce prints
                putchar(' ');
            }
            putchar('\n');
        }
    }
};


struct termios original_termios;

void ResetTerminalMode() {
    tcsetattr(0, TCSANOW, &original_termios);
    puts("\e[?25h"); // Show cursor
}

void SetTerminalToRawMode() {
    puts("\e[?25l"); // Hide cursor

    // Save original mode to restore when done
    tcgetattr(0, &original_termios);
    atexit(ResetTerminalMode);

    // Set no echo and char buffered
    struct termios new_termios;
    memcpy(&new_termios, &original_termios, sizeof(new_termios));
    new_termios.c_lflag &= ~ICANON;
    new_termios.c_lflag &= ~ECHO;
    tcsetattr(0, TCSANOW, &new_termios);
}

void CustomExit(int) {
    ResetTerminalMode();
    exit(1);
}

namespace KeyPress {
    enum KeyPress : int {
        None = 0, Left, Right, Up, Down, Space, c, COUNT,
    };
}


using timepoint = std::chrono::milliseconds::rep;
volatile bool keepRunning = true;
volatile timepoint lastPress[KeyPress::COUNT];
volatile timepoint lastRelease[KeyPress::COUNT];

bool IsKeyPressed(KeyPress::KeyPress key) {
    return lastPress[key] > lastRelease[key];
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
        int len;
        while ((len = read(keyboard_fd, events, sizeof(events))) > 0) {
            len /= sizeof(events[0]);
            for (int i = 0; i < len; ++i) {

                struct input_event *event = &events[i];
                if (event->type == EV_KEY) {
                    // 0 = released
                    // 1 = pressed
                    // 2 = held
                    bool pressed = event->value != 0;
                    timepoint now = std::chrono::system_clock::now().time_since_epoch().count();

                    switch (event->code) {
                        case 105: (pressed ? lastPress : lastRelease)[KeyPress::Left]  = now; break;
                        case 106: (pressed ? lastPress : lastRelease)[KeyPress::Right] = now; break;
                        case 103: (pressed ? lastPress : lastRelease)[KeyPress::Up]    = now; break;
                        case 108: (pressed ? lastPress : lastRelease)[KeyPress::Down]  = now; break;
                        case 57:  (pressed ? lastPress : lastRelease)[KeyPress::Space] = now; break;
                        case 46:  (pressed ? lastPress : lastRelease)[KeyPress::c]     = now; break;
                    }
                }
                // Ignore all other event types
            }
        }
        // Don't kill cpu
        std::this_thread::sleep_for(1ms);
    }
}

Screen screen;

template<int8_t Width=10, int8_t Height=20>
struct Tetris {

    struct Tetromino {
        struct Mino {
            int8_t x, y;
        };

        enum Type : uint8_t {
            None = 0, I, J, L, O, S, T, Z
        };

        Type type;
        int8_t rotation = 0;
        int8_t px = 4;
        int8_t py = 0;

        // rotations[type][rotation][mino]
        static constexpr Mino rotations[][4][4] = {
            [Type::I] = {
                { {-1, 0}, { 0, 0}, {+1, 0}, {+2, 0} },
                { { 0,-1}, { 0, 0}, { 0,+1}, { 0,+2} },
                { {+1, 0}, { 0, 0}, {-1, 0}, {-2, 0} },
                { { 0,+1}, { 0, 0}, { 0,-1}, { 0,-2} },
            },
            [Type::J] = {
                { {-1,-1}, {-1, 0}, { 0, 0}, {+1, 0} },
                { {+1,-1}, { 0,-1}, { 0, 0}, { 0,+1} },
                { {+1,+1}, {+1, 0}, { 0, 0}, {-1, 0} },
                { {-1,+1}, { 0,+1}, { 0, 0}, { 0,-1} },
            },
            [Type::L] = {
                { {+1,-1}, {-1, 0}, { 0, 0}, {+1, 0} },
                { {+1,+1}, { 0,-1}, { 0, 0}, { 0,+1} },
                { {-1,+1}, {+1, 0}, { 0, 0}, {-1, 0} },
                { {-1,-1}, { 0,+1}, { 0, 0}, { 0,-1} },
            },
            [Type::O] = {
                { { 0, 0}, { 0,-1}, {+1, 0}, {+1,-1} },
                { { 0, 0}, {+1, 0}, { 0,+1}, {+1,+1} },
                { { 0, 0}, { 0,+1}, {-1, 0}, {-1,+1} },
                { { 0, 0}, {-1, 0}, { 0,-1}, {-1,-1} },
            },
            [Type::S] = {
                { {-1, 0}, { 0, 0}, { 0,-1}, {+1,-1} },
                { { 0,-1}, { 0, 0}, {+1, 0}, {+1,+1} },
                { {+1, 0}, { 0, 0}, { 0,+1}, {-1,+1} },
                { { 0,+1}, { 0, 0}, {-1, 0}, {-1,-1} },
            },
            [Type::T] = {
                { { 0, 0}, {-1, 0}, { 0,-1}, {+1, 0} },
                { { 0, 0}, { 0,-1}, {+1, 0}, { 0,+1} },
                { { 0, 0}, {+1, 0}, { 0,+1}, {-1, 0} },
                { { 0, 0}, { 0,+1}, {-1, 0}, { 0,-1} },
            },
            [Type::Z] = {
                { {-1,-1}, { 0,-1}, { 0, 0}, {+1, 0} },
                { {+1,-1}, {+1, 0}, { 0, 0}, { 0,+1} },
                { {+1,+1}, { 0,+1}, { 0, 0}, {-1, 0} },
                { {-1,+1}, {-1, 0}, { 0, 0}, { 0,-1} },
            },
        };

        // offsets[rotation][offset]
        static constexpr Mino JLSTZoffsets[4][5] {
            { { 0, 0}, { 0, 0}, { 0, 0}, { 0, 0}, { 0, 0} },
            { { 0, 0}, {+1, 0}, {+1,+1}, { 0,-2}, {+1,-2} },
            { { 0, 0}, { 0, 0}, { 0, 0}, { 0, 0}, { 0, 0} },
            { { 0, 0}, {-1, 0}, {-1,+1}, { 0,-2}, {-1,-2} },
        };
        static constexpr Mino Ioffsets[4][5] {
            { { 0, 0}, {-1, 0}, {+2, 0}, {-1, 0}, {+2, 0} },
            { {-1, 0}, { 0, 0}, { 0, 0}, { 0,-1}, { 0,+2} },
            { {-1,-1}, {+1,-1}, {-2,-1}, {+1, 0}, {-2, 0} },
            { { 0,-1}, { 0,-1}, { 0,-1}, { 0,+1}, { 0,-2} },
        };
        static constexpr Mino Ooffsets[4][5] {
            { { 0, 0}, { 0, 0}, { 0, 0}, { 0, 0}, { 0, 0} },
            { { 0,+1}, { 0,+1}, { 0,+1}, { 0,+1}, { 0,+1} },
            { {-1,+1}, {-1,+1}, {-1,+1}, {-1,+1}, {-1,+1} },
            { {-1, 0}, {-1, 0}, {-1, 0}, {-1, 0}, {-1, 0} },
        };

        Mino GetMino(size_t i) {
            Mino mino{px, py};
            Mino diff = rotations[type][rotation][i];
            mino.x += diff.x;
            mino.y += diff.y;
            return mino;
        }

    };

    Color PieceColor(Tetromino::Type type) {
        switch (type) {
            case Tetromino::Type::I: return Color::Cyan;
            case Tetromino::Type::J: return Color::Blue;
            case Tetromino::Type::L: return Color::Orange;
            case Tetromino::Type::O: return Color::Yellow;
            case Tetromino::Type::S: return Color::Green;
            case Tetromino::Type::T: return Color::Pink;
            case Tetromino::Type::Z: return Color::Red;
            case Tetromino::Type::None: return Color::Black;
            default: break;
        }
        UNREACHABLE;
        return {};
    }

    bool InBounds(int8_t x, int8_t y) {
        // NOTE: Don't check for y >= 0 here
        return y < Height && x >= 0 && x < Width;
    }

    bool HitWall(int8_t x, int8_t y) {
        return !InBounds(x, y) ||
            (y >= 0 && board[y][x] != Tetromino::Type::None);
    }

    bool PieceHitWall(Tetromino piece, int8_t dx = 0, int8_t dy = 0) {
        for (size_t i = 0; i < 4; ++i) {
            int8_t x = piece.GetMino(i).x + dx;
            int8_t y = piece.GetMino(i).y + dy;
            if (HitWall(x, y)) {
                return true;
            }
        }
        return false;
    }

    void Rotate(Tetromino& piece, bool clockwise) {
        int8_t oldRotation = piece.rotation;

        if (clockwise) {
            if (++piece.rotation >= 4) {
                piece.rotation = 0;
            }
        }
        else {
            if (--piece.rotation < 0) {
                piece.rotation = 3;
            }
        }

        // https://tetris.wiki/Super_Rotation_System
        // The positions are commonly described as a sequence of ( x, y) kick values representing
        // translations relative to basic rotation; a convention of positive x rightwards, positive
        // y upwards is used, e.g. (-1,+2) would indicate a kick of 1 cell left and 2 cells up.
        // For offset pair in wall kick
        //   try to rotate with offset
        //   if ok then done
        typename Tetromino::Mino const (*offsets)[4][5];
        switch (piece.type) {
            case Tetromino::Type::I: offsets = &Tetromino::Ioffsets; break;
            case Tetromino::Type::O: offsets = &Tetromino::Ooffsets; break;
            case Tetromino::Type::J:
            case Tetromino::Type::L:
            case Tetromino::Type::S:
            case Tetromino::Type::T:
            case Tetromino::Type::Z:
                offsets = &Tetromino::JLSTZoffsets;
                break;
            case Tetromino::Type::None:
            default: UNREACHABLE;
        }

        bool didRotate = false;
        // Try all wallkick offsets
        for (size_t offset = 0; offset < 5; ++offset) {
            typename Tetromino::Mino oldOffset = (*offsets)[oldRotation][offset];
            typename Tetromino::Mino newOffset = (*offsets)[piece.rotation][offset];
            int8_t dx = oldOffset.x - newOffset.x;
            int8_t dy = oldOffset.y - newOffset.y;
            if (!PieceHitWall(piece, dx, dy)) {
                didRotate = true;
                piece.px += dx;
                piece.py += dy;
                break;
            }
        }

        if (!didRotate) {
            piece.rotation = oldRotation;
        }
        assert(didRotate);
    }

    void HardDrop(Tetromino& piece) {
        // TODO: Check for game over
        int dy = 0;
        while (!PieceHitWall(piece, 0, dy)) {
            dy += 1;
        }
        dy -= 1;
        for (size_t i = 0; i < 4; ++i) {
            int8_t x = piece.GetMino(i).x;
            int8_t y = piece.GetMino(i).y + dy;
            if (InBounds(x, y)) {
                board[y][x] = piece.type;
            }
        }
        piece = Tetromino{static_cast<Tetromino::Type>(static_cast<int>(piece.type) % static_cast<int>(Tetromino::Type::Z) + 1)};
    }


    Tetromino::Type board[Height][Width]{};
    Tetromino currentPiece = Tetromino{Tetromino::Type::I};

    void Update() {
        bool rightPress = IsKeyPressed(KeyPress::Right);
        bool leftPress = IsKeyPressed(KeyPress::Left);
        bool upPress = IsKeyPressed(KeyPress::Up);
        bool downPress = IsKeyPressed(KeyPress::Down);
        bool rotate = IsKeyPressed(KeyPress::c);
        bool drop = IsKeyPressed(KeyPress::Space);

        static bool rotateLatch = false;
        static bool dropLatch = false;

        bool rotateFirstPress = false;
        if (rotate && !rotateLatch) {
            rotateLatch = true;
            rotateFirstPress = true;
        }
        else if (!rotate) {
            rotateLatch = false;
        }

        bool dropFirstPress = false;
        if (drop && !dropLatch) {
            dropLatch = true;
            dropFirstPress = true;
        }
        else if (!drop) {
            dropLatch = false;
        }

        if (rotateFirstPress) {
            Rotate(currentPiece, true);
        }
        else if (dropFirstPress) {
            HardDrop(currentPiece);
        }
        else {
            int dx = rightPress - leftPress;
            int dy = downPress - upPress;
            if (!PieceHitWall(currentPiece, dx, 0)) {
                currentPiece.px += dx;
            }
            if (!PieceHitWall(currentPiece, 0, dy)) {
                currentPiece.py += dy;
            }
        }
    }

    void Draw() {
        screen.ClearBuffer();
        for (size_t y = 0; y < screen.Height; ++y) {
            screen.SetPixel(0, y, Color::White);
            screen.SetPixel(screen.Width-1, y, Color::White);
        }
        for (size_t x = 1; x < screen.Width-1; ++x) {
            screen.SetPixel(x, 0, Color::White);
            screen.SetPixel(x, screen.Height-1, Color::White);
        }
        for (size_t y = 0; y < Height; ++y) {
            for (size_t x = 0; x < Width; ++x) {
                typename Tetromino::Type type = board[y][x];
                screen.SetPixel(x+1, y+1, PieceColor(type));
            }
        }
        // TODO: Ghost piece
        for (size_t i = 0; i < 4; ++i) {
            int8_t minoX = currentPiece.GetMino(i).x + 1;
            int8_t minoY = currentPiece.GetMino(i).y + 1;
            screen.SetPixel(minoX, minoY, PieceColor(currentPiece.type));
        }
    }
};

int main(void) {
    // Handlers for gracefully exiting
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = CustomExit;
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGKILL, &action, NULL);
    sigaction(SIGINT, &action, NULL);

    SetTerminalToRawMode();
    std::thread inputThread(ContinuouslyReadInput);

    Tetris game;

    // This should be consistent with NES tetris
    // At 60fps, the fastest tapping should be 30hz (alternating pressing and releasing each frame)
    static constexpr size_t Framerate = 60;
    static constexpr auto Timestep = std::chrono::microseconds(1000000 / Framerate);

    // Press   -> Immediately move and start DAS
    // Tick    -> If pressed, check DAS/ARR then move if ready
    // Release -> Stop pressing
    while (keepRunning) {
        game.Update();
        game.Draw();
        screen.RedrawScreen();

        std::this_thread::sleep_for(Timestep);
    }

    // If the game loop breaks somehow, clean up and exit
    inputThread.join();
    ResetTerminalMode();
	return 0;
}

// TODO:
//   Game over + restart
//   DAS + ARR
//   Ghost piece
//   Hold piece
//   Next piece(s)
//   7-bag
//   Timer
//   Line/score counter
//   Cross-platform (don't read from device directly)

