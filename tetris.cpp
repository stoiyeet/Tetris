#include <cstdint>
#include <cstdio>
#include <cmath>
#include <cassert>
#include <cstring>
#include <cstdlib>

#include <thread>
#include <chrono>
#include <random>
using namespace std::chrono_literals;

// #include "platform_terminal_linux.hpp"
#include "platform_sdl.hpp"


#define UNREACHABLE assert(0 && "Unreachable")

long randlong(long min, long max) {
    static std::random_device rd;
    static std::mt19937 rng(rd());

    std::uniform_int_distribution<long> dist(min, max);
    return dist(rng);
}

void ShuffleArray(auto* arr, size_t N) {
    for (size_t i = N-1; i >= 1; --i) {
        size_t j = static_cast<size_t>(randlong(0, static_cast<long>(i)));
        std::swap(arr[i], arr[j]);
    }
}


template<int8_t Width=10, int8_t Height=20>
struct Tetris {
    static constexpr auto DAS = std::chrono::system_clock::duration(133ms).count();
    static constexpr auto ARR = std::chrono::system_clock::duration(10ms).count();

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

        Mino GetMino(size_t i) const {
            Mino mino{px, py};
            Mino diff = rotations[type][rotation][i];
            mino.x += diff.x;
            mino.y += diff.y;
            return mino;
        }

    };

    Color PieceColor(Tetromino::Type type) const {
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

    bool InBounds(int8_t x, int8_t y) const {
        // NOTE: Don't check for y >= 0 here
        return y < Height && x >= 0 && x < Width;
    }

    bool HitWall(int8_t x, int8_t y) const {
        return !InBounds(x, y) ||
            (y >= 0 && board[y][x] != Tetromino::Type::None);
    }

    bool PieceHitWall(Tetromino piece, int8_t dx = 0, int8_t dy = 0) const {
        for (size_t i = 0; i < 4; ++i) {
            int8_t x = piece.GetMino(i).x + dx;
            int8_t y = piece.GetMino(i).y + dy;
            if (HitWall(x, y)) {
                return true;
            }
        }
        return false;
    }

    void Rotate(Tetromino& piece, bool clockwise) const {
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
    }

    int8_t DistanceFromFloor(Tetromino piece) const {
        int8_t dy = 0;
        while (!PieceHitWall(piece, 0, dy)) {
            dy += 1;
        }
        dy -= 1;
        return dy;
    }

    void PlacePiece(Tetromino& piece) {
        // TODO: Game over
        for (size_t i = 0; i < 4; ++i) {
            int8_t x = piece.GetMino(i).x;
            int8_t y = piece.GetMino(i).y;
            if (y >= 0 && InBounds(x, y)) {
                board[y][x] = piece.type;
            }
        }
        piece = Tetromino{NextFromBag()};
        alreadySwapped = false;
        ClearLines();
    }

    void SwapHold() {
        if (alreadySwapped) {
            return;
        }
        alreadySwapped = true;
        if (holdType == Tetromino::Type::None) {
            holdType = currentPiece.type;
            currentPiece = Tetromino{NextFromBag()};
        }
        else {
            typename Tetromino::Type oldType = currentPiece.type;
            currentPiece = Tetromino{holdType};
            holdType = oldType;
        }
    }

    void HardDrop() {
        currentPiece.py += DistanceFromFloor(currentPiece);
        PlacePiece(currentPiece);
    }

    void ClearLines() {
        for (int8_t y = Height-1; y >= 0; --y) {
            bool rowFull = true;
            for (int8_t x = 0; x < Width; ++x) {
                if (board[y][x] == Tetromino::Type::None) {
                    rowFull = false;
                    break;
                }
            }

            if (rowFull) {
                for (int8_t above = y; above > 0; --above) {
                    for (int8_t x = 0; x < Width; ++x) {
                        board[above][x] = board[above-1][x];
                    }
                }
                for (int8_t x = 0; x < Width; ++x) {
                    board[0][x] = Tetromino::Type::None;
                }
                ++y;
            }
        }
    }

    Tetromino::Type NextFromBag() {
        typename Tetromino::Type result = pieceQueue[pieceQueueTop++];
        if (pieceQueueTop >= 7) {
            pieceQueueTop = 0;
            memcpy(pieceQueue, pieceQueue+7, 7*sizeof(pieceQueue[0]));
            for (size_t i = 7; i < 14; ++i) {
                pieceQueue[i] = static_cast<Tetromino::Type>(i-7+1);
            }
            ShuffleArray(pieceQueue+7, 7);
        }
        return result;
    }

    Tetris() {
        pieceQueueTop = 0;
        for (size_t i = 0; i < 7; ++i) {
            pieceQueue[i] = static_cast<Tetromino::Type>(i+1);
        }
        for (size_t i = 7; i < 14; ++i) {
            pieceQueue[i] = static_cast<Tetromino::Type>(i-7+1);
        }
        ShuffleArray(pieceQueue, 7);
        ShuffleArray(pieceQueue+7, 7);

        currentPiece = Tetromino{NextFromBag()};
    }

    void Update(timepoint now) {
        static timepoint lastUpdate = 0;

        if (lastUpdate + ARR <= now) {
            lastUpdate = now;
        }
        else {
            return;
        }

        static bool holdLatch = false;
        static bool dropLatch = false;
        static bool leftLatch = false;
        static bool rightLatch = false;
        static bool upLatch = false;
        static bool zLatch = false;

        bool right = IsKeyPressed(KeyPress::Right);
        bool rightDAS = right && lastPress[KeyPress::Right] + DAS < now;
        bool rightFirstPress = false;
        if (right && !rightLatch) {
            rightLatch = true;
            rightFirstPress = true;
        }
        else if (!right) {
            rightLatch = false;
        }
        bool rightPress = rightFirstPress || rightDAS;

        bool left = IsKeyPressed(KeyPress::Left);
        bool leftDAS = left && lastPress[KeyPress::Left] + DAS < now;
        bool leftFirstPress = false;
        if (left && !leftLatch) {
            leftLatch = true;
            leftFirstPress = true;
        }
        else if (!left) {
            leftLatch = false;
        }
        bool leftPress = leftFirstPress || leftDAS;


        bool up = IsKeyPressed(KeyPress::Up);
        bool upFirstPress = false;
        if (up && !upLatch) {
            upLatch = true;
            upFirstPress = true;
        }
        else if (!up) {
            upLatch = false;
        }

        bool z = IsKeyPressed(KeyPress::z);
        bool zFirstPress = false;
        if (z && !zLatch) {
            zLatch = true;
            zFirstPress = true;
        }
        else if (!z) {
            zLatch = false;
        }

        bool downPress = IsKeyPressed(KeyPress::Down);

        bool hold = IsKeyPressed(KeyPress::c);
        bool holdFirstPress = false;
        if (hold && !holdLatch) {
            holdLatch = true;
            holdFirstPress = true;
        }
        else if (!hold) {
            holdLatch = false;
        }

        bool drop = IsKeyPressed(KeyPress::Space);
        bool dropFirstPress = false;
        if (drop && !dropLatch) {
            dropLatch = true;
            dropFirstPress = true;
        }
        else if (!drop) {
            dropLatch = false;
        }


        if (upFirstPress) {
            Rotate(currentPiece, true);
        }
        if (zFirstPress) {
            Rotate(currentPiece, false);
        }

        if (holdFirstPress) {
            SwapHold();
        }

        if (dropFirstPress) {
            HardDrop();
        }

        int8_t dx = rightPress - leftPress;
        int8_t dy = downPress;
        if (!PieceHitWall(currentPiece, dx, 0)) {
            currentPiece.px += dx;
        }
        if (!PieceHitWall(currentPiece, 0, dy)) {
            currentPiece.py += dy;
        }
    }

    void DrawPiece(Tetromino piece, Color color, int8_t dx, int8_t dy) {
        if (piece.type == Tetromino::Type::None) {
            return;
        }

        for (size_t i = 0; i < 4; ++i) {
            int8_t minoX = piece.GetMino(i).x + dx;
            int8_t minoY = piece.GetMino(i).y + dy;
            screen.SetPixel(static_cast<size_t>(minoX), static_cast<size_t>(minoY), color);
        }
    }

    void Draw() {
        screen.ClearBuffer();
        for (size_t y = 0; y <= Height+1; ++y) {
            screen.SetPixel(0, y, Color::White);
            screen.SetPixel(Width+1, y, Color::White);
        }
        for (size_t x = 0; x <= Width+1; ++x) {
            screen.SetPixel(x, 0, Color::White);
            screen.SetPixel(x, Height+1, Color::White);
        }
        for (size_t y = 0; y < Height; ++y) {
            for (size_t x = 0; x < Width; ++x) {
                typename Tetromino::Type type = board[y][x];
                screen.SetPixel(x+1, y+1, PieceColor(type));
            }
        }

        // Next piece queue
        for (int8_t top = 0; top < 5; ++top) {
            Tetromino nextPiece{.type=pieceQueue[pieceQueueTop+static_cast<size_t>(top)], .rotation=0, .px=0, .py=0};
            Color nextColor = PieceColor(nextPiece.type);
            DrawPiece(nextPiece, nextColor, 14, 2 + top * 3);
        }

        // Hold piece
        Tetromino holdPiece{.type=holdType, .rotation=0, .px=0, .py=0};
        Color holdColor = PieceColor(holdPiece.type);
        DrawPiece(holdPiece, holdColor, 14, 20);

        // Ghost piece
        int8_t distFromFloor = DistanceFromFloor(currentPiece);
        Color minoColor = PieceColor(currentPiece.type);
        Color ghostColor = minoColor.DimColor(127);
        DrawPiece(currentPiece, ghostColor, 1, 1 + distFromFloor);

        // Current piece
        DrawPiece(currentPiece, minoColor, 1, 1);
    }

    Screen<18, 22> screen;
    size_t pieceQueueTop;
    Tetromino::Type pieceQueue[14];
    Tetromino::Type board[Height][Width]{};
    Tetromino currentPiece;
    Tetromino::Type holdType = Tetromino::Type::None;
    bool alreadySwapped = false;
};

int main(void) {
    InitializeScreen();
    Tetris game;

    std::thread inputThread(ContinuouslyReadInput);

    // This should be consistent with NES tetris
    // At 60fps, the fastest tapping should be 30hz (alternating pressing and releasing each frame)
    static constexpr size_t Framerate = 60;
    static constexpr timepoint Timestep = 1000 / Framerate;

    timepoint lastRedraw = 0;
    while (keepRunning) {
        // TODO: Timer
        // TODO: Score counter

        // Check time diff, if large enough redraw
        timepoint now = std::chrono::system_clock::now().time_since_epoch().count();
        game.Update(now);
        if (now > lastRedraw + Timestep) {
            game.Draw();
            game.screen.RedrawScreen();
        }

        // std::this_thread::sleep_for(Timestep);
        std::this_thread::sleep_for(1ms);
    }

    // If the game loop breaks somehow, clean up and exit
    inputThread.join();
    DestroyScreen();
    return 0;
}

// TODO:
//   Game over + restart
//   Timer
//   Line/score counter
//   Cross-platform
//   Online multiplayer

