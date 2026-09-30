#include "game.h"
#include "render.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int checks, failures;

#define CHECK(condition, name) do { \
    ++checks; \
    if (!(condition)) { ++failures; printf("FAIL: %s (line %d)\n", name, __LINE__); } \
} while (0)

static void empty_room(Game *g)
{
    game_init(g, 42);
    for (int y = 0; y < MAP_H; ++y)
        for (int x = 0; x < MAP_W; ++x)
            g->map[y][x] = (x == 0 || y == 0 || x == MAP_W - 1 || y == MAP_H - 1) ? '#' : '.';
    g->x = 2.5;
    g->y = 2.5;
    g->angle = 0.0;
    for (int i = 0; i < ENEMY_COUNT; ++i) {
        g->enemy[i].x = 25.5;
        g->enemy[i].y = 18.5;
        g->enemy[i].stun = 100000.0;
    }
    for (int i = 0; i < KEY_COUNT; ++i) { g->key[i].x = 20.5 + i; g->key[i].y = 18.5; }
    g->exit_x = 28;
    g->exit_y = 18;
    game_start(g);
}

static void generated_maps(void)
{
    for (uint32_t seed = 0; seed < 128; ++seed) {
        Game a, b;
        game_init(&a, seed);
        game_init(&b, seed);
        CHECK(memcmp(&a, &b, sizeof(a)) == 0, "seed is deterministic");
        int dist[MAP_H][MAP_W], border = 1, connected = 1, valid = 1;
        game_distances(&a, (int)a.x, (int)a.y, dist);
        for (int x = 0; x < MAP_W; ++x)
            if (!game_solid(&a, x, 0) || !game_solid(&a, x, MAP_H - 1)) border = 0;
        for (int y = 0; y < MAP_H; ++y)
            if (!game_solid(&a, 0, y) || !game_solid(&a, MAP_W - 1, y)) border = 0;
        for (int y = 1; y < MAP_H - 1; ++y)
            for (int x = 1; x < MAP_W - 1; ++x)
                if (!game_solid(&a, x, y) && dist[y][x] < 0) connected = 0;
        CHECK(border, "maze has closed borders");
        CHECK(connected, "every floor tile is connected");
        CHECK(game_can_stand(&a, a.x, a.y, 0.18), "safe spawn");
        CHECK(dist[a.exit_y][a.exit_x] > 0, "exit is reachable");
        for (int k = 0; k < KEY_COUNT; ++k) {
            if (dist[(int)a.key[k].y][(int)a.key[k].x] < 5) valid = 0;
            if ((int)a.key[k].x == a.exit_x && (int)a.key[k].y == a.exit_y) valid = 0;
            for (int j = 0; j < k; ++j)
                if (hypot(a.key[k].x - a.key[j].x, a.key[k].y - a.key[j].y) < 1.0) valid = 0;
        }
        CHECK(valid, "cores reachable, unique, separated from spawn and exit");
        for (int i = 0; i < ENEMY_COUNT; ++i)
            CHECK(dist[(int)a.enemy[i].y][(int)a.enemy[i].x] >= 12, "enemy starts away from player");
    }
}

static void mechanics(void)
{
    Game g;
    Controls idle = {0}, forward = {1.0, 0.0, 0.0, 0, 0};
    empty_room(&g);
    RayHit hit = game_cast_ray(&g, 2.5, 2.5, -1.0, 0.0);
    CHECK(fabs(hit.distance - 1.5) < 1e-9 && hit.side == 0, "horizontal DDA distance");
    hit = game_cast_ray(&g, 2.5, 2.5, 0.0, -1.0);
    CHECK(fabs(hit.distance - 1.5) < 1e-9 && hit.side == 1, "vertical DDA distance");
    hit = game_cast_ray(&g, 2.5, 2.5, 1.0, 1.0);
    CHECK(fabs(hit.distance - 17.5) < 1e-9, "diagonal DDA parameter");
    CHECK(game_line_clear(&g, 2.5, 2.5, 8.5, 2.5), "unobstructed line of sight");
    g.map[2][4] = '#';
    CHECK(!game_line_clear(&g, 2.5, 2.5, 8.5, 2.5), "wall blocks line of sight");
    for (int i = 0; i < 100; ++i) game_update(&g, forward, 0.1);
    CHECK(g.x < 3.83 && g.x > 3.5, "movement stops at wall with body radius");
    CHECK(game_can_stand(&g, g.x, g.y, 0.18), "collision never leaves body inside wall");
    CHECK(!game_can_stand(&g, -0.1, 2.5, 0.18), "negative coordinates are solid");
    empty_room(&g);
    game_update(&g, forward, 0.1);
    double straight = hypot(g.x - 2.5, g.y - 2.5);
    empty_room(&g);
    Controls diagonal = {1.0, 1.0, 0.0, 0, 0};
    game_update(&g, diagonal, 0.1);
    CHECK(fabs(hypot(g.x - 2.5, g.y - 2.5) - straight) < 1e-9, "diagonal speed normalized");
    g.mode = PAUSED;
    double before = g.remaining, px = g.x;
    game_update(&g, forward, 0.1);
    CHECK(g.remaining == before && g.x == px, "pause freezes gameplay and timer");
    empty_room(&g);
    g.key[0].x = g.x;
    g.key[0].y = g.y;
    g.hp = 50;
    game_update(&g, idle, 0.05);
    CHECK(g.keys == 1 && g.key[0].collected && g.hp == 65, "core pickup restores health");
    CHECK(g.remaining > RUN_SECONDS, "core pickup extends time");
    game_update(&g, idle, 0.05);
    CHECK(g.keys == 1, "core cannot be collected twice");
    g.x = g.exit_x + 0.5;
    g.y = g.exit_y + 0.5;
    game_update(&g, idle, 0.01);
    CHECK(g.mode == PLAYING, "gate stays locked without all cores");
    g.keys = KEY_COUNT;
    game_update(&g, idle, 0.01);
    CHECK(g.mode == WON, "all cores unlock extraction");
    empty_room(&g);
    g.remaining = 0.01;
    game_update(&g, idle, 0.1);
    CHECK(g.mode == LOST && g.remaining == 0.0, "timeout loses cleanly");
    empty_room(&g);
    g.enemy[0] = (Enemy){3.0, 2.5, 0.0, 0.0};
    game_update(&g, idle, 0.01);
    CHECK(g.hp == 86.0, "enemy contact damages player");
    game_update(&g, idle, 0.01);
    CHECK(g.hp == 86.0, "contact damage has cooldown");
    g.hp = 1.0;
    g.enemy[0].attack_cooldown = 0.0;
    game_update(&g, idle, 0.01);
    CHECK(g.mode == LOST && g.hp == 0.0, "zero health loses cleanly");
    empty_room(&g);
    g.enemy[0] = (Enemy){3.5, 2.5, 0.0, 0.0};
    Controls pulse = {0.0, 0.0, 0.0, 0, 1};
    game_update(&g, pulse, 0.05);
    CHECK(g.enemy[0].stun > 2.9 && g.enemy[0].x > 4.5, "pulse stuns and pushes visible enemy");
    CHECK(g.pulses_used == 1 && g.pulse_cooldown > 6.9, "pulse consumes cooldown");
    game_update(&g, pulse, 0.05);
    CHECK(g.pulses_used == 1, "pulse cannot repeat during cooldown");
    empty_room(&g);
    g.map[2][4] = '#';
    g.enemy[0] = (Enemy){5.5, 2.5, 0.0, 0.0};
    game_update(&g, pulse, 0.01);
    CHECK(g.enemy[0].stun == 0.0, "pulse cannot pass through wall");
    double timer = g.remaining;
    game_update(&g, forward, NAN);
    game_update(&g, forward, -1.0);
    CHECK(g.remaining == timer, "invalid timestep ignored");
}

static int walk_to(Game *g, int tx, int ty)
{
    static const int dx[4] = {1, 0, -1, 0};
    static const int dy[4] = {0, 1, 0, -1};
    int dist[MAP_H][MAP_W];
    game_distances(g, tx, ty, dist);
    for (int step = 0; step < MAP_W * MAP_H; ++step) {
        if (g->mode == WON) return tx == g->exit_x && ty == g->exit_y;
        int x = (int)g->x, y = (int)g->y;
        int nx = x, ny = y, best = dist[y][x];
        if (x == tx && y == ty) return 1;
        for (int i = 0; i < 4; ++i) {
            int xx = x + dx[i], yy = y + dy[i];
            if (game_solid(g, xx, yy) || dist[yy][xx] < 0) continue;
            if (dist[yy][xx] < best) { best = dist[yy][xx]; nx = xx; ny = yy; }
        }
        if (nx == x && ny == y) return 0;
        int reached = 0;
        for (int sub = 0; sub < 50; ++sub) {
            double vx = nx + 0.5 - g->x, vy = ny + 0.5 - g->y, len = hypot(vx, vy);
            if (len < 0.001 || g->mode == WON) { reached = 1; break; }
            if (g->mode != PLAYING) return 0;
            g->angle = atan2(vy, vx);
            Controls move = {1.0, 0.0, 0.0, 0, 0};
            game_update(g, move, fmin(0.04, len / 2.5));
        }
        if (!reached) return 0;
    }
    return 0;
}

static void enemy_navigation(void)
{
    for (uint32_t seed = 1; seed <= 16; ++seed) {
        Game g;
        game_init(&g, seed);
        game_start(&g);
        int dist[MAP_H][MAP_W], found = 0;
        game_distances(&g, (int)g.x, (int)g.y, dist);
        for (int y = 1; y < MAP_H - 1 && !found; ++y) {
            for (int x = 1; x < MAP_W - 1; ++x) {
                if (dist[y][x] == 15) {
                    g.enemy[0] = (Enemy){x + 0.5, y + 0.5, 0.0, 0.0};
                    found = 1;
                    break;
                }
            }
        }
        for (int i = 1; i < ENEMY_COUNT; ++i) g.enemy[i].stun = 100000.0;
        Controls idle = {0};
        int safe = 1;
        for (int i = 0; i < 1200 && g.hp == 100.0; ++i) {
            game_update(&g, idle, 0.05);
            if (!game_can_stand(&g, g.enemy[0].x, g.enemy[0].y, 0.16)) safe = 0;
        }
        CHECK(found && g.hp < 100.0, "sentinel navigates maze corners to reach player");
        CHECK(safe, "sentinel never enters wall");
    }
}

static void complete_runs(void)
{
    for (uint32_t seed = 1; seed <= 12; ++seed) {
        Game g;
        game_init(&g, seed);
        game_start(&g);
        for (int i = 0; i < ENEMY_COUNT; ++i) g.enemy[i].stun = 100000.0;
        for (int i = 0; i < KEY_COUNT; ++i)
            CHECK(walk_to(&g, (int)g.key[i].x, (int)g.key[i].y), "walk real movement to core");
        CHECK(g.keys == KEY_COUNT, "full route collects all three cores");
        CHECK(walk_to(&g, g.exit_x, g.exit_y), "walk real movement to exit");
        CHECK(g.mode == WON, "full playable run reaches victory");
    }
}

static void rendering(void)
{
    static Screen s;
    static const int sizes[][2] = {{1, 1}, {71, 25}, {72, 26}, {115, 31}, {128, 42}, {200, 70}, {300, 100}};
    Game g;
    game_init(&g, 42);
    for (int mode = TITLE; mode <= LOST; ++mode) {
        g.mode = (GameMode)mode;
        for (unsigned i = 0; i < sizeof(sizes) / sizeof(sizes[0]); ++i) {
            memset(&s, 0, sizeof(s));
            render_game(&s, &g, sizes[i][0], sizes[i][1], 60.0);
            int valid = s.width <= SCREEN_MAX_W && s.height <= SCREEN_MAX_H;
            for (int j = 0; j < s.width * s.height; ++j) {
                Cell c = s.cells[j];
                if (c.ch < 32 || c.ch > 126 || c.fg >= COLOR_COUNT || c.bg >= COLOR_COUNT) valid = 0;
            }
            CHECK(valid, "render stays printable ASCII and within palette at all sizes");
        }
    }
}

int run_tests(void)
{
    puts("VOIDCAST // deterministic C test suite");
    generated_maps();
    mechanics();
    complete_runs();
    enemy_navigation();
    rendering();
    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
