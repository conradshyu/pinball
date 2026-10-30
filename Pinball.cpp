#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace {
constexpr int kWidth = 1020;
constexpr int kHeight = 760;
constexpr UINT_PTR kTimerId = 1;
constexpr double kPi = 3.14159265358979323846;

struct Vec2 {
    double x{};
    double y{};
};

struct Bumper {
    Vec2 position;
    int points;
    COLORREF color;
    double flash{};
};

struct Ball {
    Vec2 position;
    Vec2 velocity;
    bool inPlunger{true};
};

HWND g_window{};
HDC g_backBuffer{};
HBITMAP g_backBitmap{};
HGDIOBJ g_oldBitmap{};
Ball g_ball;
std::vector<Bumper> g_bumpers;
bool g_leftHeld{};
bool g_rightHeld{};
bool g_plungerHeld{};
bool g_started{};
bool g_gameOver{};
bool g_tilted{};
bool g_centerPost{};
int g_score{};
int g_highScore{};
int g_ballNumber{1};
int g_extraBalls{};
int g_missionProgress{};
int g_missionsCompleted{};
double g_fuel{100.0};
int g_skillLane{};
int g_tiltWarnings{};
double g_plungerPower{};
double g_tiltTime{};
double g_postTime{};
double g_messageTime{};
std::wstring g_message;

const wchar_t* RankName(int rank) {
    static const wchar_t* ranks[] = {
        L"CADET", L"ENSIGN", L"LIEUTENANT", L"LT. COMMANDER", L"COMMANDER",
        L"CAPTAIN", L"COMMODORE", L"ADMIRAL", L"FLEET ADMIRAL"
    };
    return ranks[std::clamp(rank, 0, 8)];
}

int CurrentRank() {
    return std::min(g_missionsCompleted, 8);
}

void SetMessage(const std::wstring& text, double seconds = 2.2) {
    g_message = text;
    g_messageTime = seconds;
}

void AddScore(int points) {
    g_score += points;
    g_highScore = std::max(g_highScore, g_score);
}

void ResetBall() {
    g_ball = {{570.0, 625.0}, {0.0, 0.0}, true};
    g_plungerPower = 0.0;
    g_plungerHeld = false;
    g_fuel = 100.0;
}

void StartGame() {
    g_score = 0;
    g_ballNumber = 1;
    g_extraBalls = 0;
    g_missionProgress = 0;
    g_missionsCompleted = 0;
    g_fuel = 100;
    g_tiltWarnings = 0;
    g_tilted = false;
    g_gameOver = false;
    g_started = true;
    ResetBall();
    SetMessage(L"SPACE FLEET: LAUNCH YOUR FIRST MISSION");
}

void AdvanceMission() {
    ++g_missionProgress;
    AddScore(2500);
    if (g_missionProgress >= 3) {
        g_missionProgress = 0;
        if (g_missionsCompleted < 8) {
            ++g_missionsCompleted;
            SetMessage(std::wstring(L"PROMOTION! ") + RankName(CurrentRank()));
        } else {
            SetMessage(L"MISSION COMPLETE - EXTRA BALL!");
        }
        ++g_extraBalls;
    } else {
        SetMessage(L"MISSION OBJECTIVE COMPLETE");
    }
}

void DrainBall() {
    if (g_extraBalls > 0) {
        --g_extraBalls;
        ResetBall();
        SetMessage(L"REPLAY AWARDED - EXTRA BALL");
        return;
    }
    if (g_ballNumber >= 3) {
        g_gameOver = true;
        g_started = false;
        SetMessage(L"GAME OVER - PRESS ENTER TO PLAY AGAIN", 30.0);
    } else {
        ++g_ballNumber;
        ResetBall();
        SetMessage(L"BALL SAVED - PREPARE TO LAUNCH");
    }
}

void NewBackBuffer(int width, int height) {
    if (g_backBuffer) {
        SelectObject(g_backBuffer, g_oldBitmap);
        DeleteObject(g_backBitmap);
        DeleteDC(g_backBuffer);
    }
    HDC screen = GetDC(g_window);
    g_backBuffer = CreateCompatibleDC(screen);
    g_backBitmap = CreateCompatibleBitmap(screen, std::max(width, 1), std::max(height, 1));
    g_oldBitmap = SelectObject(g_backBuffer, g_backBitmap);
    ReleaseDC(g_window, screen);
}

void Fill(HDC dc, int left, int top, int right, int bottom, COLORREF color) {
    RECT rect{left, top, right, bottom};
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(dc, &rect, brush);
    DeleteObject(brush);
}

void Text(HDC dc, int x, int y, const std::wstring& value, int size, COLORREF color,
          bool bold = false, int width = 0) {
    HFONT font = CreateFontW(size, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Arial");
    HGDIOBJ oldFont = SelectObject(dc, font);
    SetTextColor(dc, color);
    SetBkMode(dc, TRANSPARENT);
    RECT rect{x, y, width ? x + width : kWidth, y + size + 8};
    DrawTextW(dc, value.c_str(), static_cast<int>(value.size()), &rect,
              DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS);
    SelectObject(dc, oldFont);
    DeleteObject(font);
}

void CenterText(HDC dc, int x, int y, int width, const std::wstring& value, int size,
                COLORREF color, bool bold = false) {
    HFONT font = CreateFontW(size, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Arial");
    HGDIOBJ oldFont = SelectObject(dc, font);
    SetTextColor(dc, color);
    SetBkMode(dc, TRANSPARENT);
    RECT rect{x, y, x + width, y + size + 10};
    DrawTextW(dc, value.c_str(), static_cast<int>(value.size()), &rect,
              DT_CENTER | DT_TOP | DT_SINGLELINE);
    SelectObject(dc, oldFont);
    DeleteObject(font);
}

void Disk(HDC dc, int x, int y, int radius, COLORREF color, COLORREF rim = RGB(235, 244, 255)) {
    HPEN pen = CreatePen(PS_SOLID, 2, rim);
    HBRUSH brush = CreateSolidBrush(color);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    HGDIOBJ oldBrush = SelectObject(dc, brush);
    Ellipse(dc, x - radius, y - radius, x + radius, y + radius);
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(pen);
    DeleteObject(brush);
}

void Segment(HDC dc, int x1, int y1, int x2, int y2, COLORREF color, int thickness) {
    HPEN pen = CreatePen(PS_SOLID, thickness, color);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    MoveToEx(dc, x1, y1, nullptr);
    LineTo(dc, x2, y2);
    SelectObject(dc, oldPen);
    DeleteObject(pen);
}

void RoundBox(HDC dc, int left, int top, int right, int bottom, int radius,
              COLORREF fill, COLORREF border) {
    HPEN pen = CreatePen(PS_SOLID, 2, border);
    HBRUSH brush = CreateSolidBrush(fill);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    HGDIOBJ oldBrush = SelectObject(dc, brush);
    RoundRect(dc, left, top, right, bottom, radius, radius);
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(pen);
    DeleteObject(brush);
}

void DrawStarfield(HDC dc) {
    for (int i = 0; i < 80; ++i) {
        int x = 70 + (i * 193 + 37) % 575;
        int y = 55 + (i * 127 + 11) % 640;
        COLORREF color = i % 4 == 0 ? RGB(76, 182, 232) : RGB(43, 82, 127);
        Fill(dc, x, y, x + (i % 5 == 0 ? 3 : 2), y + (i % 5 == 0 ? 3 : 2), color);
    }
}

void DrawTable(HDC dc) {
    Fill(dc, 0, 0, kWidth, kHeight, RGB(7, 11, 25));
    DrawStarfield(dc);
    RoundBox(dc, 42, 28, 678, 730, 26, RGB(19, 34, 57), RGB(56, 121, 177));
    RoundBox(dc, 54, 40, 666, 718, 20, RGB(9, 23, 42), RGB(24, 93, 147));

    // Field rails and the narrow plunger lane.
    Segment(dc, 85, 72, 85, 579, RGB(37, 168, 225), 5);
    Segment(dc, 85, 579, 125, 664, RGB(37, 168, 225), 5);
    Segment(dc, 625, 72, 625, 573, RGB(37, 168, 225), 5);
    Segment(dc, 625, 573, 594, 664, RGB(37, 168, 225), 5);
    Segment(dc, 548, 76, 548, 657, RGB(229, 151, 59), 4);
    Segment(dc, 568, 76, 568, 657, RGB(72, 109, 153), 2);
    Segment(dc, 85, 72, 548, 72, RGB(37, 168, 225), 5);

    // Launch ramp, orbit, target banks, and illuminated lanes.
    RoundBox(dc, 112, 93, 300, 137, 12, RGB(20, 72, 107), RGB(66, 197, 237));
    CenterText(dc, 114, 100, 184, L"LAUNCH RAMP", 16, RGB(206, 245, 255), true);
    RoundBox(dc, 338, 85, 492, 122, 10, RGB(49, 39, 78), RGB(164, 121, 238));
    CenterText(dc, 340, 92, 150, L"HYPERSPACE", 15, RGB(228, 206, 255), true);
    for (int i = 0; i < 3; ++i) {
        COLORREF lane = i == g_skillLane ? RGB(255, 211, 69) : RGB(35, 91, 126);
        Disk(dc, 124 + i * 36, 166, 8, lane, RGB(125, 216, 255));
    }
    Text(dc, 96, 182, L"SKILL LANES", 12, RGB(111, 190, 225), true);

    for (int i = 0; i < 5; ++i) {
        int x = 330 + i * 42;
        RoundBox(dc, x, 165, x + 28, 196, 6, i < g_missionProgress ? RGB(50, 193, 230) : RGB(22, 46, 70),
                 RGB(67, 155, 194));
        CenterText(dc, x, 171, 28, std::to_wstring(i + 1), 13, RGB(220, 248, 255), true);
    }
    Text(dc, 338, 203, L"MISSION TARGETS", 12, RGB(130, 195, 220), true);

    // Rollover lights and ramp guides.
    for (int i = 0; i < 5; ++i) {
        Disk(dc, 116 + i * 26, 258, 7, (i < g_missionProgress) ? RGB(63, 209, 250) : RGB(20, 60, 92));
    }
    Segment(dc, 115, 310, 245, 218, RGB(38, 89, 130), 7);
    Segment(dc, 245, 218, 300, 223, RGB(38, 89, 130), 7);
    Segment(dc, 485, 218, 533, 300, RGB(112, 66, 176), 7);
    Segment(dc, 533, 300, 533, 430, RGB(112, 66, 176), 7);
    RoundBox(dc, 90, 328, 177, 367, 9, RGB(24, 68, 84), RGB(63, 184, 197));
    CenterText(dc, 91, 336, 85, L"FUEL", 16, RGB(156, 238, 223), true);
    RoundBox(dc, 476, 325, 557, 365, 9, RGB(42, 32, 71), RGB(154, 111, 222));
    CenterText(dc, 478, 333, 77, L"WORMHOLE", 11, RGB(217, 192, 255), true);

    // Bumpers, animated pulse rings, and side kickers.
    for (const auto& bumper : g_bumpers) {
        if (bumper.flash > 0.0) {
            Disk(dc, static_cast<int>(bumper.position.x), static_cast<int>(bumper.position.y), 30,
                 RGB(61, 140, 180), RGB(127, 232, 255));
        }
        Disk(dc, static_cast<int>(bumper.position.x), static_cast<int>(bumper.position.y), 21,
             bumper.flash > 0.0 ? RGB(239, 167, 61) : bumper.color, RGB(194, 229, 246));
        Disk(dc, static_cast<int>(bumper.position.x), static_cast<int>(bumper.position.y), 11,
             RGB(24, 48, 73), RGB(244, 208, 112));
    }
    RoundBox(dc, 89, 425, 124, 480, 12, RGB(54, 111, 132), RGB(124, 221, 240));
    CenterText(dc, 90, 438, 33, L"K", 20, RGB(238, 255, 255), true);
    RoundBox(dc, 588, 432, 621, 488, 12, RGB(82, 57, 134), RGB(193, 163, 248));
    CenterText(dc, 589, 445, 31, L"K", 20, RGB(245, 236, 255), true);

    // Outlane guards, center post, and flippers.
    Segment(dc, 135, 536, 264, 606, RGB(35, 139, 184), 5);
    Segment(dc, 570, 536, 448, 606, RGB(35, 139, 184), 5);
    RoundBox(dc, 245, 642, 453, 660, 9,
             g_centerPost ? RGB(229, 181, 61) : RGB(35, 58, 76),
             g_centerPost ? RGB(255, 237, 129) : RGB(77, 121, 146));
    Segment(dc, 286, 620, g_leftHeld && !g_tilted ? 356 : 348, g_leftHeld && !g_tilted ? 584 : 610,
            RGB(69, 206, 255), 15);
    Segment(dc, 420, 620, g_rightHeld && !g_tilted ? 352 : 360, g_rightHeld && !g_tilted ? 584 : 610,
            RGB(249, 119, 90), 15);
    Disk(dc, static_cast<int>(g_ball.position.x), static_cast<int>(g_ball.position.y), 9,
         RGB(245, 249, 255), RGB(97, 185, 255));

    Text(dc, 231, 278, L"ATTACK", 12, RGB(205, 127, 76), true);
    Text(dc, 374, 278, L"ENGINE", 12, RGB(113, 207, 159), true);
    Text(dc, 480, 278, L"SATELLITE", 12, RGB(181, 147, 226), true);
    Text(dc, 85, 680, L"Z / X  NUDGE", 12, RGB(117, 177, 207), true);
    Text(dc, 450, 680, L"SPACE  PLUNGER", 12, RGB(117, 177, 207), true);
}

void DrawHud(HDC dc) {
    RoundBox(dc, 704, 28, 1000, 730, 22, RGB(13, 20, 36), RGB(51, 101, 151));
    CenterText(dc, 718, 46, 268, L"SPACE FLEET", 25, RGB(100, 215, 255), true);
    CenterText(dc, 718, 79, 268, L"PINBALL COMMAND", 13, RGB(137, 164, 195), true);
    Segment(dc, 728, 110, 976, 110, RGB(39, 74, 106), 2);
    Text(dc, 734, 126, L"SCORE", 13, RGB(127, 165, 202), true);
    Text(dc, 734, 146, std::to_wstring(g_score), 31, RGB(246, 220, 108), true);
    Text(dc, 734, 187, L"HIGH SCORE", 12, RGB(127, 165, 202), true);
    Text(dc, 734, 205, std::to_wstring(g_highScore), 20, RGB(219, 236, 251), true);
    Segment(dc, 728, 242, 976, 242, RGB(39, 74, 106), 2);

    Text(dc, 734, 259, L"RANK", 12, RGB(127, 165, 202), true);
    Text(dc, 734, 279, RankName(CurrentRank()), 21, RGB(255, 207, 93), true);
    Text(dc, 734, 313, L"PROMOTION LIGHTS", 12, RGB(127, 165, 202), true);
    for (int i = 0; i < 8; ++i) {
        Disk(dc, 746 + (i % 4) * 51, 347 + (i / 4) * 35, 8,
             i < g_missionsCompleted ? RGB(77, 199, 255) : RGB(24, 53, 78), RGB(81, 151, 190));
    }
    Text(dc, 734, 402, L"MISSION", 12, RGB(127, 165, 202), true);
    const wchar_t* mission = g_missionsCompleted == 0 ? L"LAUNCH TRAINING" :
                             g_missionsCompleted < 3 ? L"TARGET PRACTICE" :
                             g_missionsCompleted < 6 ? L"COMBAT PATROL" : L"FLEET COMMAND";
    Text(dc, 734, 420, mission, 17, RGB(162, 222, 243), true);
    Text(dc, 734, 451, L"FUEL", 12, RGB(127, 165, 202), true);
    Fill(dc, 734, 472, 956, 488, RGB(27, 46, 65));
    Fill(dc, 734, 472, 734 + (222 * static_cast<int>(g_fuel) / 100), 488,
         g_fuel > 30 ? RGB(59, 200, 167) : RGB(238, 100, 83));
    Text(dc, 734, 493, std::to_wstring(static_cast<int>(g_fuel)) + L"%", 12,
         RGB(177, 220, 222), true);
    Segment(dc, 728, 523, 976, 523, RGB(39, 74, 106), 2);
    Text(dc, 734, 539, L"PLAYER  1", 13, RGB(127, 165, 202), true);
    Text(dc, 734, 559, L"BALL", 12, RGB(127, 165, 202), true);
    Text(dc, 812, 554, std::to_wstring(g_ballNumber) + L" / 3", 19, RGB(245, 240, 218), true);
    Text(dc, 734, 589, L"REPLAYS", 12, RGB(127, 165, 202), true);
    Text(dc, 812, 585, std::to_wstring(g_extraBalls), 18, RGB(245, 240, 218), true);
    Text(dc, 734, 623, L"MISSION OBJECTIVES", 12, RGB(127, 165, 202), true);
    Text(dc, 734, 645, L"TARGETS", 13, RGB(202, 220, 240), true);
    for (int i = 0; i < 3; ++i) {
        Disk(dc, 886 + i * 24, 655, 7, i < g_missionProgress ? RGB(255, 208, 82) : RGB(24, 53, 78));
    }
    CenterText(dc, 719, 690, 266, L"ENTER: NEW GAME   ESC: QUIT", 11, RGB(97, 129, 161), true);
}

void DrawOverlays(HDC dc) {
    if (!g_started) {
        RoundBox(dc, 142, 286, 607, 470, 20, RGB(7, 17, 32), RGB(62, 160, 207));
        CenterText(dc, 158, 307, 432, g_gameOver ? L"MISSION COMPLETE" : L"SPACE FLEET PINBALL",
                   27, RGB(103, 221, 255), true);
        CenterText(dc, 167, 352, 414,
                   g_gameOver ? L"Your fleet record is ready." : L"Launch your ball. Earn your rank.",
                   16, RGB(219, 232, 244));
        CenterText(dc, 167, 393, 414, L"ENTER TO START", 18, RGB(255, 218, 105), true);
    }
    if (g_messageTime > 0.0 && !g_message.empty()) {
        RoundBox(dc, 147, 497, 604, 535, 10, RGB(8, 24, 42), RGB(62, 151, 192));
        CenterText(dc, 153, 505, 445, g_message, 14, RGB(232, 244, 250), true);
    }
    if (g_tilted) {
        RoundBox(dc, 255, 390, 477, 435, 10, RGB(56, 18, 22), RGB(255, 93, 66));
        CenterText(dc, 262, 400, 208, L"TILT - FLIPPERS DISABLED", 14, RGB(255, 173, 132), true);
    }
}

void DrawScene() {
    if (!g_backBuffer) return;
    SetMapMode(g_backBuffer, MM_ANISOTROPIC);
    SetWindowExtEx(g_backBuffer, kWidth, kHeight, nullptr);
    RECT client{};
    GetClientRect(g_window, &client);
    SetViewportExtEx(g_backBuffer, std::max(client.right, 1L), std::max(client.bottom, 1L), nullptr);
    SetViewportOrgEx(g_backBuffer, 0, 0, nullptr);
    DrawTable(g_backBuffer);
    DrawHud(g_backBuffer);
    DrawOverlays(g_backBuffer);
}

Vec2 FlipperA(bool left) {
    if (left) return {g_leftHeld && !g_tilted ? 356.0 : 348.0, g_leftHeld && !g_tilted ? 584.0 : 610.0};
    return {g_rightHeld && !g_tilted ? 352.0 : 360.0, g_rightHeld && !g_tilted ? 584.0 : 610.0};
}

void CollideWithFlipper(Vec2 pivot, Vec2 tip, bool active) {
    Vec2 segment{tip.x - pivot.x, tip.y - pivot.y};
    double lengthSquared = segment.x * segment.x + segment.y * segment.y;
    double t = ((g_ball.position.x - pivot.x) * segment.x +
                (g_ball.position.y - pivot.y) * segment.y) / lengthSquared;
    t = std::clamp(t, 0.0, 1.0);
    Vec2 nearest{pivot.x + t * segment.x, pivot.y + t * segment.y};
    Vec2 delta{g_ball.position.x - nearest.x, g_ball.position.y - nearest.y};
    double distance = std::sqrt(delta.x * delta.x + delta.y * delta.y);
    if (distance < 15.0 && g_ball.velocity.y > -1.0) {
        double nx = distance > 0.001 ? delta.x / distance : 0.0;
        double ny = distance > 0.001 ? delta.y / distance : -1.0;
        g_ball.position.x = nearest.x + nx * 15.0;
        g_ball.position.y = nearest.y + ny * 15.0;
        double dot = g_ball.velocity.x * nx + g_ball.velocity.y * ny;
        g_ball.velocity.x -= 1.8 * dot * nx;
        g_ball.velocity.y -= 1.8 * dot * ny;
        if (active) {
            g_ball.velocity.y = -10.5;
            g_ball.velocity.x += (pivot.x < 340.0 ? 2.8 : -2.8);
        }
    }
}

void UpdateGame(double dt) {
    if (g_messageTime > 0.0) g_messageTime = std::max(0.0, g_messageTime - dt);
    if (g_tilted) {
        g_tiltTime -= dt;
        if (g_tiltTime <= 0.0) {
            g_tilted = false;
            g_tiltWarnings = 0;
        }
    }
    if (g_centerPost) {
        g_postTime -= dt;
        if (g_postTime <= 0.0) g_centerPost = false;
    }
    for (auto& bumper : g_bumpers) bumper.flash = std::max(0.0, bumper.flash - dt);
    if (!g_started || g_gameOver) return;
    if (g_plungerHeld && g_ball.inPlunger) g_plungerPower = std::min(1.0, g_plungerPower + dt * 0.62);
    if (g_ball.inPlunger) return;

    g_ball.velocity.y += 15.5 * dt;
    g_ball.position.x += g_ball.velocity.x * dt * 60.0;
    g_ball.position.y += g_ball.velocity.y * dt * 60.0;
    g_ball.velocity.x *= 0.999;

    if (g_ball.position.x < 98.0) {
        g_ball.position.x = 98.0;
        g_ball.velocity.x = std::abs(g_ball.velocity.x) * 0.78;
    }
    if (g_ball.position.x > 610.0 && g_ball.position.y > 105.0) {
        g_ball.position.x = 610.0;
        g_ball.velocity.x = -std::abs(g_ball.velocity.x) * 0.78;
    }
    if (g_ball.position.y < 86.0) {
        g_ball.position.y = 86.0;
        g_ball.velocity.y = std::abs(g_ball.velocity.y) * 0.7;
    }
    if (g_ball.position.y < 160.0 && g_ball.position.x > 548.0) {
        g_ball.position.x = 520.0;
        g_ball.velocity.x = -3.0;
        g_ball.velocity.y = 3.0;
        AddScore(500);
    }

    for (auto& bumper : g_bumpers) {
        Vec2 delta{g_ball.position.x - bumper.position.x, g_ball.position.y - bumper.position.y};
        double distance = std::sqrt(delta.x * delta.x + delta.y * delta.y);
        if (distance < 30.0 && distance > 0.01) {
            double nx = delta.x / distance;
            double ny = delta.y / distance;
            g_ball.position.x = bumper.position.x + nx * 30.0;
            g_ball.position.y = bumper.position.y + ny * 30.0;
            double toward = g_ball.velocity.x * nx + g_ball.velocity.y * ny;
            if (toward < 0.0) {
                g_ball.velocity.x -= 2.2 * toward * nx;
                g_ball.velocity.y -= 2.2 * toward * ny;
            }
            g_ball.velocity.y -= 2.4;
            bumper.flash = 0.2;
            AddScore(bumper.points);
            if (g_fuel < 100.0) g_fuel = std::min(100.0, g_fuel + 5.0);
            if (g_ball.position.y < 340.0 && g_missionProgress < 3) AdvanceMission();
        }
    }

    Vec2 wormholeDelta{g_ball.position.x - 518.0, g_ball.position.y - 345.0};
    if (wormholeDelta.x * wormholeDelta.x + wormholeDelta.y * wormholeDelta.y < 28.0 * 28.0) {
        g_ball.position = {304.0, 445.0};
        g_ball.velocity = {-2.0, -5.0};
        AddScore(3000);
        if (g_extraBalls == 0) {
            ++g_extraBalls;
            SetMessage(L"WORMHOLE REPLAY - EXTRA BALL");
        } else {
            SetMessage(L"HYPERSPACE TRANSIT +3000");
        }
    }

    CollideWithFlipper({286.0, 620.0}, FlipperA(true), g_leftHeld);
    CollideWithFlipper({420.0, 620.0}, FlipperA(false), g_rightHeld);
    if (g_centerPost && g_ball.position.y > 625.0 && g_ball.position.y < 658.0 &&
        g_ball.position.x > 250.0 && g_ball.position.x < 448.0) {
        g_ball.velocity.y = -8.0;
        g_ball.velocity.x += (g_ball.position.x < 349.0 ? -1.2 : 1.2);
    }
    if (g_ball.position.y < 175.0 && g_ball.position.x > 100.0 && g_ball.position.x < 230.0 &&
        g_skillLane >= 0) {
        AddScore(5000 + static_cast<int>(g_plungerPower * 5000.0));
        g_skillLane = (g_skillLane + 1) % 3;
        SetMessage(L"SKILL SHOT! PRECISION BONUS");
        g_ball.velocity.x = 5.0;
        g_ball.velocity.y = 2.0;
    }
    const double fuelBeforeDrain = g_fuel;
    g_fuel = std::max(0.0, g_fuel - (g_ball.position.y < 360.0 ? 0.0 : 12.0 * dt));
    if (fuelBeforeDrain > 0.0 && g_fuel == 0.0)
        SetMessage(L"FUEL DEPLETED - MISSION ABORTED");
    if (g_fuel == 0.0 && g_ball.position.y < 500.0) g_ball.velocity.y += 5.0 * dt;
    if (g_ball.position.y > 710.0) DrainBall();
}

void Nudge(double direction) {
    if (!g_started || g_gameOver || g_ball.inPlunger || g_tilted) return;
    if (++g_tiltWarnings >= 4) {
        g_tilted = true;
        g_tiltTime = 3.0;
        g_leftHeld = false;
        g_rightHeld = false;
        SetMessage(L"TILT WARNING - TABLE LOCKED");
        return;
    }
    g_ball.velocity.x += direction * 3.2;
    g_ball.velocity.y -= 1.6;
    SetMessage(g_tiltWarnings >= 3 ? L"CAUTION: TILT SENSITIVE" : L"TABLE NUDGED", 0.7);
}

void LaunchBall() {
    if (!g_started || g_gameOver || !g_ball.inPlunger) return;
    const double power = std::max(0.28, g_plungerPower);
    g_ball.inPlunger = false;
    g_ball.velocity = {-0.4 + (g_skillLane - 1) * 0.35, -(8.5 + power * 8.0)};
    if (power > 0.78) {
        AddScore(5000);
        g_skillLane = (g_skillLane + 1) % 3;
        SetMessage(L"PERFECT LAUNCH - SKILL SHOT!");
    } else {
        SetMessage(L"LAUNCH!");
    }
}

void KeyDown(WPARAM key) {
    if (key == VK_RETURN && !g_started) {
        StartGame();
        return;
    }
    if (key == VK_ESCAPE) {
        DestroyWindow(g_window);
        return;
    }
    if (!g_started) return;
    if (key == VK_LEFT || key == 'A') g_leftHeld = true;
    if (key == VK_RIGHT || key == 'D') g_rightHeld = true;
    if (key == VK_SPACE) g_plungerHeld = true;
    if (key == 'Z') Nudge(-1.0);
    if (key == 'X') Nudge(1.0);
    if (key == 'C') {
        g_centerPost = true;
        g_postTime = 5.0;
        AddScore(250);
        SetMessage(L"CENTER POST ACTIVATED");
    }
}

void KeyUp(WPARAM key) {
    if (key == VK_LEFT || key == 'A') g_leftHeld = false;
    if (key == VK_RIGHT || key == 'D') g_rightHeld = false;
    if (key == VK_SPACE) {
        g_plungerHeld = false;
        LaunchBall();
    }
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        g_window = window;
        g_bumpers = {
            {{270.0, 295.0}, 100, RGB(177, 76, 58)},
            {{370.0, 295.0}, 150, RGB(53, 151, 112)},
            {{470.0, 295.0}, 200, RGB(116, 79, 171)}
        };
        SetTimer(window, kTimerId, 16, nullptr);
        return 0;
    case WM_SIZE:
        NewBackBuffer(LOWORD(lParam), HIWORD(lParam));
        return 0;
    case WM_TIMER:
        UpdateGame(1.0 / 60.0);
        DrawScene();
        InvalidateRect(window, nullptr, FALSE);
        return 0;
    case WM_KEYDOWN:
        if ((lParam & (1LL << 30)) == 0) KeyDown(wParam);
        return 0;
    case WM_KEYUP:
        KeyUp(wParam);
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(window, &paint);
        if (g_backBuffer) {
            DrawScene();
            RECT client{};
            GetClientRect(window, &client);
            SetStretchBltMode(dc, HALFTONE);
            StretchBlt(dc, 0, 0, client.right, client.bottom, g_backBuffer, 0, 0,
                       kWidth, kHeight, SRCCOPY);
        }
        EndPaint(window, &paint);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_DESTROY:
        KillTimer(window, kTimerId);
        if (g_backBuffer) {
            SelectObject(g_backBuffer, g_oldBitmap);
            DeleteObject(g_backBitmap);
            DeleteDC(g_backBuffer);
            g_backBuffer = nullptr;
            g_backBitmap = nullptr;
        }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    const wchar_t className[] = L"SpaceFleetPinballWindow";
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    windowClass.lpszClassName = className;
    if (!RegisterClassExW(&windowClass)) return 1;

    RECT rect{0, 0, kWidth, kHeight};
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
    HWND window = CreateWindowExW(0, className, L"Space Fleet Pinball",
                                  WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                                  rect.right - rect.left, rect.bottom - rect.top,
                                  nullptr, nullptr, instance, nullptr);
    if (!window) return 1;
    ShowWindow(window, showCommand);
    UpdateWindow(window);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}
