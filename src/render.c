#include "render.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

const unsigned int render_colors[COLOR_COUNT] = {
    0x090d19, 0x040711, 0x25334f, 0x245068, 0x387e96, 0x6aacc1, 0x66f5dc,
    0xe2edf3, 0x8872c8, 0xf27cc7, 0xff6b81, 0xf7ce81, 0x91e8a1, 0x152639,
    0x101a2b, 0x8197b3
};

static void put(Screen *s, int x, int y, char ch, int fg, int bg)
{
    if (x < 0 || y < 0 || x >= s->width || y >= s->height) return;
    Cell *c = &s->cells[y * s->width + x];
    c->ch = (unsigned char)ch;
    c->fg = (unsigned char)fg;
    c->bg = (unsigned char)bg;
}

static void text(Screen *s, int x, int y, const char *value, int fg, int bg)
{
    for (int i = 0; value[i]; ++i) put(s, x + i, y, value[i], fg, bg);
}

static void center(Screen *s, int y, const char *value, int fg, int bg)
{
    text(s, (s->width - (int)strlen(value)) / 2, y, value, fg, bg);
}

static void rect(Screen *s, int x, int y, int w, int h, char ch, int fg, int bg)
{
    for (int yy = y; yy < y + h; ++yy)
        for (int xx = x; xx < x + w; ++xx) put(s, xx, yy, ch, fg, bg);
}

static void box(Screen *s, int x, int y, int w, int h, int color)
{
    rect(s, x, y, w, h, ' ', C_WHITE, C_PANEL);
    for (int xx = x + 1; xx < x + w - 1; ++xx) {
        put(s, xx, y, '-', color, C_PANEL);
        put(s, xx, y + h - 1, '-', color, C_PANEL);
    }
    for (int yy = y + 1; yy < y + h - 1; ++yy) {
        put(s, x, yy, '|', color, C_PANEL);
        put(s, x + w - 1, yy, '|', color, C_PANEL);
    }
    put(s, x, y, '+', color, C_PANEL);
    put(s, x + w - 1, y, '+', color, C_PANEL);
    put(s, x, y + h - 1, '+', color, C_PANEL);
    put(s, x + w - 1, y + h - 1, '+', color, C_PANEL);
}

static void world(Screen *s, const Game *g, int vx, int vy, int vw, int vh)
{
    double dir_x = cos(g->angle), dir_y = sin(g->angle);
    double plane_x = -dir_y * 0.66, plane_y = dir_x * 0.66;
    double focal = vw * 0.38;
    int horizon = vh / 2;
    for (int y = 0; y < vh; ++y) {
        for (int x = 0; x < vw; ++x) {
            char ch = ' ';
            int color = C_DIM;
            if (y > horizon) {
                double d = focal * 0.5 / (y - horizon);
                double camera = 2.0 * (x + 0.5) / vw - 1.0;
                double wx = g->x + d * (dir_x + plane_x * camera);
                double wy = g->y + d * (dir_y + plane_y * camera);
                double fx = wx - floor(wx), fy = wy - floor(wy);
                if (fx < 0.045 || fy < 0.045) { ch = '.'; color = C_WALL_DARK; }
                else if ((x + y * 3) % 7 == 0) { ch = '.'; color = C_FLOOR; }
            } else if (y < horizon && (x + y * 3) % 19 == 0) {
                ch = '.';
                color = C_FLOOR;
            }
            put(s, vx + x, vy + y, ch, color, C_BG);
        }
    }
    for (int x = 0; x < vw; ++x) {
        double camera = 2.0 * (x + 0.5) / vw - 1.0;
        RayHit hit = game_cast_ray(g, g->x, g->y, dir_x + plane_x * camera, dir_y + plane_y * camera);
        s->depth[x] = hit.distance;
        int line_h = (int)(focal / hit.distance);
        if (line_h < 1) line_h = 1;
        int start = horizon - line_h / 2, end = start + line_h;
        int color = hit.distance < 3.0 ? C_WALL_LIGHT : hit.distance < 6.0 ? C_WALL :
                    hit.distance < 11.0 ? C_WALL_DARK : C_DIM;
        if (hit.side && color == C_WALL_LIGHT) color = C_WALL;
        int seam = hit.texture_u < 0.025 || hit.texture_u > 0.975;
        for (int y = start < 0 ? 0 : start; y < end && y < vh; ++y) {
            double v = (double)(y - start) / line_h;
            char ch = hit.distance < 4.0 ? '#' : hit.distance < 8.0 ? '+' : ':';
            int fg = color;
            if (seam) { ch = '|'; fg = C_WALL_DARK; }
            if (v < 0.04 || v > 0.96) { ch = '='; fg = C_CYAN; }
            else if (fabs(v - 0.50) < 0.018) { ch = '-'; fg = C_WALL_DARK; }
            else if ((hit.map_x + hit.map_y) % 4 == 0 && hit.texture_u > 0.42 && hit.texture_u < 0.58) {
                ch = ':'; fg = C_PURPLE;
            } else if (((int)(hit.texture_u * 14) + (int)(v * 10)) % 5 == 0) ch = ':';
            put(s, vx + x, vy + y, ch, fg, C_BG);
        }
    }
}

typedef struct { double x, y, distance; int kind, index; } Sprite;

static void sprites(Screen *s, const Game *g, int vx, int vy, int vw, int vh)
{
    static const char *core[9] = {
        "    .    ", "   /|\\   ", "  /###\\  ", " <##O##> ", "  \\###/  ",
        "   \\|/   ", "    :    ", "   ===   ", "         "
    };
    static const char *sentinel[9] = {
        "   /\\    ", "  /##\\   ", " /#@@#\\  ", " |#VV#|  ", "  \\##/   ",
        "  /||\\   ", " / || \\  ", "   /\\    ", "  /  \\   "
    };
    static const char *gate[9] = {
        "+=======+", "| /   \\ |", "|:     :|", "|:  ^  :|", "|: /|\\ :|",
        "|:  |  :|", "|:     :|", "| \\   / |", "+=======+"
    };
    Sprite list[KEY_COUNT + ENEMY_COUNT + 1];
    int n = 0;
    for (int i = 0; i < KEY_COUNT; ++i)
        if (!g->key[i].collected) list[n++] = (Sprite){g->key[i].x, g->key[i].y, 0.0, 0, i};
    for (int i = 0; i < ENEMY_COUNT; ++i)
        list[n++] = (Sprite){g->enemy[i].x, g->enemy[i].y, 0.0, 1, i};
    list[n++] = (Sprite){g->exit_x + 0.5, g->exit_y + 0.5, 0.0, 2, 0};
    for (int i = 0; i < n; ++i) list[i].distance = hypot(list[i].x - g->x, list[i].y - g->y);
    for (int i = 0; i < n - 1; ++i) {
        for (int j = i + 1; j < n; ++j) {
            if (list[j].distance > list[i].distance) { Sprite t = list[i]; list[i] = list[j]; list[j] = t; }
        }
    }
    double dir_x = cos(g->angle), dir_y = sin(g->angle);
    for (int i = 0; i < n; ++i) {
        Sprite p = list[i];
        double dx = p.x - g->x, dy = p.y - g->y;
        double depth = dx * dir_x + dy * dir_y;
        if (depth < 0.18) continue;
        double side = -dx * dir_y + dy * dir_x;
        int cx = (int)(vw * 0.5 * (1.0 + side / (depth * 0.66)));
        int h = (int)(vw * 0.38 / depth * (p.kind == 0 ? 0.65 : 0.95));
        if (h < 2) h = 2;
        if (h > SCREEN_MAX_H * 4) h = SCREEN_MAX_H * 4;
        int w = h * 2, left = cx - w / 2, top = vh / 2 - h / 2;
        if (p.kind == 0) top += (int)(sin(g->elapsed * 3.0 + p.index) * 0.6);
        const char **art = p.kind == 0 ? core : p.kind == 1 ? sentinel : gate;
        int color = p.kind == 0 ? C_CYAN : p.kind == 1 ? C_RED : C_PINK;
        if (p.kind == 1 && g->enemy[p.index].stun > 0.0) color = C_YELLOW;
        if (p.kind == 2 && g->keys == KEY_COUNT) color = C_GREEN;
        for (int x = left < 0 ? 0 : left; x < left + w && x < vw; ++x) {
            if (depth >= s->depth[x]) continue;
            int tx = (x - left) * 9 / w;
            for (int y = top < 0 ? 0 : top; y < top + h && y < vh; ++y) {
                int ty = (y - top) * 9 / h;
                char ch = art[ty][tx];
                if (ch != ' ') put(s, vx + x, vy + y, ch, color, C_BG);
            }
        }
    }
}

static void minimap(Screen *s, const Game *g, int x, int y)
{
    box(s, x, y, MAP_W + 2, MAP_H + 2, C_DIM);
    text(s, x + 2, y, " SECTOR SCAN ", C_CYAN, C_PANEL);
    for (int yy = 0; yy < MAP_H; ++yy) {
        for (int xx = 0; xx < MAP_W; ++xx) {
            char ch = g->seen[yy][xx] ? (game_solid(g, xx, yy) ? '#' : '.') : ' ';
            int color = game_solid(g, xx, yy) ? C_WALL_DARK : C_DIM;
            put(s, x + xx + 1, y + yy + 1, ch, color, C_PANEL);
        }
    }
    /* Objective beacons remain visible: a small one-day game should be navigable. */
    for (int i = 0; i < KEY_COUNT; ++i)
        if (!g->key[i].collected)
            put(s, x + (int)g->key[i].x + 1, y + (int)g->key[i].y + 1, '*', C_CYAN, C_PANEL);
    put(s, x + g->exit_x + 1, y + g->exit_y + 1, 'E', g->keys == KEY_COUNT ? C_GREEN : C_PINK, C_PANEL);
    for (int i = 0; i < ENEMY_COUNT; ++i) {
        int ex = (int)g->enemy[i].x, ey = (int)g->enemy[i].y;
        if (g->seen[ey][ex] && hypot(g->enemy[i].x - g->x, g->enemy[i].y - g->y) < 7.0)
            put(s, x + ex + 1, y + ey + 1, 'X', g->enemy[i].stun > 0.0 ? C_YELLOW : C_RED, C_PANEL);
    }
    int facing = ((int)floor((g->angle + PI / 4.0) / (PI / 2.0)) % 4 + 4) % 4;
    put(s, x + (int)g->x + 1, y + (int)g->y + 1, ">v<^"[facing], C_WHITE, C_PANEL);
    text(s, x + 1, y + MAP_H + 3, "* core   E gate   X sentinel", C_MUTED, C_BG);
}

static void title(Screen *s)
{
    static const char *logo[] = {
        "__     __ ___  ___ ____   ____    _    ____ _____",
        "\\ \\   / // _ \\|_ _|  _ \\ / ___|  / \\  / ___|_   _|",
        " \\ \\ / /| | | || || | | | |     / _ \\ \\___ \\ | |",
        "  \\ V / | |_| || || |_| | |___ / ___ \\ ___) || |",
        "   \\_/   \\___/|___|____/ \\____/_/   \\_\\____/ |_|"
    };
    int w = 66, h = 23, x = (s->width - w) / 2, y = (s->height - h) / 2;
    box(s, x, y, w, h, C_WALL_DARK);
    center(s, y + 1, "[ A S C I I   E X T R A C T I O N ]", C_MUTED, C_PANEL);
    for (int i = 0; i < 5; ++i) center(s, y + 3 + i, logo[i], i < 3 ? C_CYAN : C_WALL_LIGHT, C_PANEL);
    center(s, y + 9, "THE SIGNAL IS FADING. GET OUT WITH ALL THREE CORES.", C_WHITE, C_PANEL);
    center(s, y + 11, "Explore the maze. Collect cyan cores. Find the gate.", C_MUTED, C_PANEL);
    center(s, y + 12, "Sentinels track you. Your pulse buys three seconds.", C_MUTED, C_PANEL);
    center(s, y + 14, "W / S   Move          A / D   Turn", C_WHITE, C_PANEL);
    center(s, y + 15, "Q / E   Strafe        SHIFT   Sprint", C_WHITE, C_PANEL);
    center(s, y + 16, "SPACE   Pulse        TAB / M  Map", C_WHITE, C_PANEL);
    center(s, y + 17, "P / ESC Pause            R   Restart", C_WHITE, C_PANEL);
    center(s, y + 19, "[ ENTER ]  ENTER THE VOID", C_CYAN, C_PANEL);
    center(s, y + 21, "PURE C  /  PROCEDURAL MAZES  /  NO ENGINE", C_MUTED, C_PANEL);
}

static void overlay(Screen *s, const Game *g)
{
    int w = 62, h = 15, x = (s->width - w) / 2, y = (s->height - h) / 2;
    int color = g->mode == WON ? C_CYAN : g->mode == LOST ? C_RED : C_PURPLE;
    box(s, x, y, w, h, color);
    center(s, y + 2, g->mode == WON ? "E X T R A C T I O N   C O M P L E T E" :
           g->mode == LOST ? "S I G N A L   L O S T" : "S I G N A L   S U S P E N D E D", color, C_PANEL);
    if (g->mode == PAUSED) {
        center(s, y + 5, "The clock is paused. Take a breath.", C_WHITE, C_PANEL);
        center(s, y + 8, "[ ENTER / P / ESC ]  Resume", C_CYAN, C_PANEL);
        center(s, y + 10, "[ R ]  Same maze     [ N ]  New maze", C_MUTED, C_PANEL);
    } else {
        char line[100];
        center(s, y + 4, g->mode == WON ? "Three cores recovered. You made it back." :
               g->hp <= 0.0 ? "The sentinels caught your signal." : "The extraction window has closed.", C_WHITE, C_PANEL);
        snprintf(line, sizeof(line), "CORES %d/3    TIME %.0fs    PULSES %d", g->keys, g->elapsed, g->pulses_used);
        center(s, y + 6, line, C_MUTED, C_PANEL);
        snprintf(line, sizeof(line), "SECTOR SEED  %u", (unsigned)g->seed);
        center(s, y + 8, line, C_MUTED, C_PANEL);
        center(s, y + 10, "[ R / ENTER ]  Retry     [ N ]  New maze", C_CYAN, C_PANEL);
    }
    center(s, y + 12, "[ X ]  Quit", C_MUTED, C_PANEL);
}

void render_game(Screen *s, const Game *g, int width, int height, double fps)
{
    s->width = width < 1 ? 1 : width > SCREEN_MAX_W ? SCREEN_MAX_W : width;
    s->height = height < 1 ? 1 : height > SCREEN_MAX_H ? SCREEN_MAX_H : height;
    rect(s, 0, 0, s->width, s->height, ' ', C_WHITE, C_BG);
    if (s->width < 72 || s->height < 26) {
        center(s, s->height / 2 - 1, "VOIDCAST // TERMINAL TOO SMALL", C_CYAN, C_BG);
        center(s, s->height / 2 + 1, "Resize to at least 73 columns x 26 rows.", C_WHITE, C_BG);
        center(s, s->height / 2 + 3, "Use a smaller font, or maximize the terminal.", C_MUTED, C_BG);
        return;
    }
    int sidebar = g->show_map && s->width >= 116 && s->height >= 32;
    int vw = sidebar ? s->width - 37 : s->width - 4, vh = s->height - 9;
    world(s, g, 2, 4, vw, vh);
    sprites(s, g, 2, 4, vw, vh);
    if (g->pulse_flash > 0.0) {
        double phase = 1.0 - g->pulse_flash / 0.60;
        double radius = phase * vw * 0.6;
        for (int y = 0; y < vh; ++y)
            for (int x = 0; x < vw; ++x)
                if (fabs(hypot(x - vw * 0.5, (y - vh * 0.5) * 2.0) - radius) < 1.2)
                    put(s, x + 2, y + 4, '.', C_CYAN, C_BG);
    }
    if (g->hurt_flash > 0.0) {
        for (int y = 4; y < vh + 4; ++y) {
            put(s, 1, y, '|', C_RED, C_BG);
            put(s, vw + 2, y, '|', C_RED, C_BG);
        }
    }
    put(s, 2 + vw / 2, 4 + vh / 2, '+', C_MUTED, C_BG);
    if (sidebar) minimap(s, g, s->width - 34, 4);
    else if (g->show_map && g->mode != TITLE) {
        /* On narrower terminals TAB toggles this overlay to expose the whole scene. */
        minimap(s, g, s->width - 35, 3);
    }
    rect(s, 0, 0, s->width, 3, ' ', C_WHITE, C_PANEL);
    text(s, 2, 1, "V O I D C A S T", C_CYAN, C_PANEL);
    text(s, 21, 1, "/  EXTRACTION PROTOCOL", C_MUTED, C_PANEL);
    char line[140];
    snprintf(line, sizeof(line), "SEED %u  /  %02.0f FPS", (unsigned)g->seed, fps);
    text(s, s->width - (int)strlen(line) - 2, 1, line, C_MUTED, C_PANEL);
    rect(s, 0, s->height - 5, s->width, 5, ' ', C_WHITE, C_PANEL);
    int seconds = (int)ceil(g->remaining);
    snprintf(line, sizeof(line), "HP %3.0f  [", g->hp);
    text(s, 2, s->height - 4, line, g->hp < 30 ? C_RED : C_CYAN, C_PANEL);
    for (int i = 0; i < 10; ++i)
        put(s, 11 + i, s->height - 4, g->hp > i * 10 ? '=' : '.', g->hp < 30 ? C_RED : C_CYAN, C_PANEL);
    text(s, 21, s->height - 4, "]", C_CYAN, C_PANEL);
    snprintf(line, sizeof(line), "CORES %d/3   TIME %02d:%02d", g->keys, seconds / 60, seconds % 60);
    text(s, 26, s->height - 4, line, g->remaining < 30.0 ? C_RED : C_WHITE, C_PANEL);
    if (g->pulse_cooldown > 0.0) snprintf(line, sizeof(line), "PULSE %.1fs", g->pulse_cooldown);
    else snprintf(line, sizeof(line), "PULSE READY");
    text(s, 55, s->height - 4, line, g->pulse_cooldown > 0.0 ? C_MUTED : C_CYAN, C_PANEL);
    if (g->message_time > 0.0) text(s, 2, s->height - 3, g->message, C_YELLOW, C_PANEL);
    else text(s, 2, s->height - 3, "Recover all cores (*) and reach the gate (E). Cores restore health and time.", C_MUTED, C_PANEL);
    text(s, 2, s->height - 2, "W/S move  A/D turn  Q/E strafe  SPACE pulse  TAB map  P pause  X quit", C_MUTED, C_PANEL);
    if (g->mode == TITLE) title(s);
    if (g->mode == PAUSED || g->mode == WON || g->mode == LOST) overlay(s, g);
}
