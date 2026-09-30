#include "game.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static uint32_t random_next(Game *g)
{
    uint32_t x = g->rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    g->rng = x;
    return x;
}

static int random_int(Game *g, int n)
{
    return (int)(random_next(g) % (uint32_t)n);
}

int game_solid(const Game *g, int x, int y)
{
    return x < 0 || y < 0 || x >= MAP_W || y >= MAP_H || g->map[y][x] == '#';
}

int game_can_stand(const Game *g, double x, double y, double radius)
{
    return !game_solid(g, (int)floor(x - radius), (int)floor(y - radius)) &&
           !game_solid(g, (int)floor(x + radius), (int)floor(y - radius)) &&
           !game_solid(g, (int)floor(x - radius), (int)floor(y + radius)) &&
           !game_solid(g, (int)floor(x + radius), (int)floor(y + radius));
}

/* Grid DDA. With camera-plane rays this distance is already fish-eye corrected. */
RayHit game_cast_ray(const Game *g, double x, double y, double dx, double dy)
{
    RayHit hit = {64.0, 0.0, 0, 0, 0};
    int mx = (int)floor(x), my = (int)floor(y);
    int sx = dx < 0.0 ? -1 : 1, sy = dy < 0.0 ? -1 : 1;
    double ddx = fabs(dx) < 1e-12 ? 1e30 : fabs(1.0 / dx);
    double ddy = fabs(dy) < 1e-12 ? 1e30 : fabs(1.0 / dy);
    double next_x = (dx < 0.0 ? x - mx : mx + 1.0 - x) * ddx;
    double next_y = (dy < 0.0 ? y - my : my + 1.0 - y) * ddy;
    if (fabs(dx) < 1e-12 && fabs(dy) < 1e-12) return hit;
    for (int i = 0; i < MAP_W + MAP_H + 8; ++i) {
        if (next_x < next_y) {
            mx += sx;
            hit.distance = next_x;
            next_x += ddx;
            hit.side = 0;
        } else {
            my += sy;
            hit.distance = next_y;
            next_y += ddy;
            hit.side = 1;
        }
        if (game_solid(g, mx, my)) break;
    }
    hit.map_x = mx;
    hit.map_y = my;
    double u = hit.side ? x + hit.distance * dx : y + hit.distance * dy;
    hit.texture_u = u - floor(u);
    if (hit.distance < 0.001) hit.distance = 0.001;
    return hit;
}

int game_line_clear(const Game *g, double ax, double ay, double bx, double by)
{
    double dx = bx - ax, dy = by - ay;
    double length = hypot(dx, dy);
    if (length < 0.001) return 1;
    RayHit hit = game_cast_ray(g, ax, ay, dx / length, dy / length);
    return hit.distance + 0.01 >= length;
}

void game_distances(const Game *g, int x, int y, int dist[MAP_H][MAP_W])
{
    static const int dx[4] = {1, 0, -1, 0};
    static const int dy[4] = {0, 1, 0, -1};
    int qx[MAP_W * MAP_H], qy[MAP_W * MAP_H];
    int head = 0, tail = 0;
    for (int yy = 0; yy < MAP_H; ++yy)
        for (int xx = 0; xx < MAP_W; ++xx) dist[yy][xx] = -1;
    if (game_solid(g, x, y)) return;
    dist[y][x] = 0;
    qx[tail] = x;
    qy[tail++] = y;
    while (head < tail) {
        int cx = qx[head], cy = qy[head++];
        for (int i = 0; i < 4; ++i) {
            int nx = cx + dx[i], ny = cy + dy[i];
            if (game_solid(g, nx, ny) || dist[ny][nx] >= 0) continue;
            dist[ny][nx] = dist[cy][cx] + 1;
            qx[tail] = nx;
            qy[tail++] = ny;
        }
    }
}

static void generate_map(Game *g)
{
    static const int dx[4] = {2, 0, -2, 0};
    static const int dy[4] = {0, 2, 0, -2};
    int stack_x[MAP_W * MAP_H], stack_y[MAP_W * MAP_H], top = 0;
    memset(g->map, '#', sizeof(g->map));
    stack_x[0] = stack_y[0] = 1;
    g->map[1][1] = '.';
    while (top >= 0) {
        int x = stack_x[top], y = stack_y[top], choices[4], count = 0;
        for (int i = 0; i < 4; ++i) {
            int nx = x + dx[i], ny = y + dy[i];
            if (nx > 0 && ny > 0 && nx < MAP_W - 1 && ny < MAP_H - 1 &&
                g->map[ny][nx] == '#') choices[count++] = i;
        }
        if (!count) { --top; continue; }
        int dir = choices[random_int(g, count)];
        int nx = x + dx[dir], ny = y + dy[dir];
        g->map[y + dy[dir] / 2][x + dx[dir] / 2] = '.';
        g->map[ny][nx] = '.';
        stack_x[++top] = nx;
        stack_y[top] = ny;
    }
    /* Small rooms and loops give the player routes around pursuing sentinels. */
    for (int i = 0; i < 12; ++i) {
        int x = 1 + 2 * random_int(g, (MAP_W - 3) / 2);
        int y = 1 + 2 * random_int(g, (MAP_H - 3) / 2);
        for (int yy = y; yy < y + 3; ++yy)
            for (int xx = x; xx < x + 3; ++xx) g->map[yy][xx] = '.';
    }
    for (int i = 0; i < 65; ++i) {
        int x = 1 + random_int(g, MAP_W - 2), y = 1 + random_int(g, MAP_H - 2);
        if ((!game_solid(g, x - 1, y) && !game_solid(g, x + 1, y)) ||
            (!game_solid(g, x, y - 1) && !game_solid(g, x, y + 1))) g->map[y][x] = '.';
    }
    /* An open starting chamber makes the first frame and movement readable. */
    for (int y = 1; y <= 3; ++y)
        for (int x = 1; x <= 5; ++x) g->map[y][x] = '.';
}

static void set_message(Game *g, const char *message)
{
    snprintf(g->message, sizeof(g->message), "%s", message);
    g->message_time = 3.5;
}

static void reveal(Game *g)
{
    int px = (int)g->x, py = (int)g->y;
    for (int y = py - 6; y <= py + 6; ++y) {
        for (int x = px - 6; x <= px + 6; ++x) {
            if (x < 0 || y < 0 || x >= MAP_W || y >= MAP_H) continue;
            if (hypot(x + 0.5 - g->x, y + 0.5 - g->y) > 6.0) continue;
            /* A ray can reveal the near face of a wall, but not what's behind it. */
            double dx = x + 0.5 - g->x, dy = y + 0.5 - g->y;
            double d = hypot(dx, dy);
            if (d < 0.01) { g->seen[y][x] = 1; continue; }
            RayHit hit = game_cast_ray(g, g->x, g->y, dx / d, dy / d);
            if (hit.distance >= d || (hit.map_x == x && hit.map_y == y)) g->seen[y][x] = 1;
        }
    }
}

void game_init(Game *g, uint32_t seed)
{
    memset(g, 0, sizeof(*g));
    g->seed = seed;
    g->rng = seed ? seed : 0x9e3779b9u;
    g->x = 2.5;
    g->y = 2.5;
    g->hp = 100.0;
    g->remaining = RUN_SECONDS;
    g->show_map = 1;
    g->mode = TITLE;
    generate_map(g);
    int dist[MAP_H][MAP_W], farthest = -1;
    game_distances(g, (int)g->x, (int)g->y, dist);
    for (int y = 1; y < MAP_H - 1; ++y) {
        for (int x = 1; x < MAP_W - 1; ++x) {
            if (dist[y][x] > farthest) {
                farthest = dist[y][x];
                g->exit_x = x;
                g->exit_y = y;
            }
        }
    }
    /* Farthest-point placement spreads objectives across the connected map. */
    for (int k = 0; k < KEY_COUNT; ++k) {
        double best = -1.0;
        int bx = 1, by = 1;
        for (int y = 1; y < MAP_H - 1; ++y) {
            for (int x = 1; x < MAP_W - 1; ++x) {
                if (dist[y][x] < 5 || (x == g->exit_x && y == g->exit_y)) continue;
                double score = hypot(x + 0.5 - g->x, y + 0.5 - g->y);
                double exit_distance = hypot((double)(x - g->exit_x), (double)(y - g->exit_y));
                if (exit_distance < score) score = exit_distance;
                for (int j = 0; j < k; ++j) {
                    double d = hypot(x + 0.5 - g->key[j].x, y + 0.5 - g->key[j].y);
                    if (d < score) score = d;
                }
                score += (double)random_int(g, 100) / 500.0;
                if (score > best) { best = score; bx = x; by = y; }
            }
        }
        g->key[k].x = bx + 0.5;
        g->key[k].y = by + 0.5;
    }
    for (int e = 0; e < ENEMY_COUNT; ++e) {
        int bx = g->exit_x, by = g->exit_y;
        for (int tries = 0; tries < 500; ++tries) {
            int x = 1 + random_int(g, MAP_W - 2), y = 1 + random_int(g, MAP_H - 2);
            if (dist[y][x] < 12) continue;
            int separated = 1;
            for (int j = 0; j < e; ++j)
                if (hypot(x + 0.5 - g->enemy[j].x, y + 0.5 - g->enemy[j].y) < 4.0)
                    separated = 0;
            if (separated) { bx = x; by = y; break; }
        }
        g->enemy[e].x = bx + 0.5;
        g->enemy[e].y = by + 0.5;
    }
    set_message(g, "Find 3 cyan cores. Reach the magenta gate. Keep moving.");
    reveal(g);
}

void game_start(Game *g)
{
    if (g->mode == TITLE) g->mode = PLAYING;
}

static void move_entity(const Game *g, double *x, double *y, double dx, double dy, double radius)
{
    /* Substeps prevent tunnelling through walls, including diagonal corners. */
    int steps = (int)ceil(fmax(fabs(dx), fabs(dy)) / 0.08);
    if (steps < 1) steps = 1;
    for (int i = 0; i < steps; ++i) {
        double nx = *x + dx / steps, ny = *y + dy / steps;
        if (game_can_stand(g, nx, *y, radius)) *x = nx;
        if (game_can_stand(g, *x, ny, radius)) *y = ny;
    }
}

static void update_enemies(Game *g, double dt)
{
    static const int dx[4] = {1, 0, -1, 0};
    static const int dy[4] = {0, 1, 0, -1};
    int dist[MAP_H][MAP_W];
    game_distances(g, (int)g->x, (int)g->y, dist);
    for (int i = 0; i < ENEMY_COUNT; ++i) {
        Enemy *e = &g->enemy[i];
        e->attack_cooldown = fmax(0.0, e->attack_cooldown - dt);
        if (e->stun > 0.0) { e->stun = fmax(0.0, e->stun - dt); continue; }
        double distance = hypot(g->x - e->x, g->y - e->y);
        int ex = (int)e->x, ey = (int)e->y;
        if (dist[ey][ex] < 0 || dist[ey][ex] > 18) continue;
        double tx = ex + 0.5, ty = ey + 0.5;
        if (dist[ey][ex] == 0) {
            tx = g->x;
            ty = g->y;
        } else {
            int best = dist[ey][ex];
            int direction = -1;
            for (int j = 0; j < 4; ++j) {
                int nx = ex + dx[j], ny = ey + dy[j];
                if (game_solid(g, nx, ny) || dist[ny][nx] < 0) continue;
                if (dist[ny][nx] < best) { best = dist[ny][nx]; direction = j; }
            }
            /* Align to the corridor center before turning. A point-only sight ray
               can otherwise guide a finite-radius enemy into a wall corner. */
            if (direction >= 0) {
                if (dx[direction]) {
                    tx = fabs(e->y - ty) > 0.01 ? e->x : ex + dx[direction] + 0.5;
                } else {
                    ty = fabs(e->x - tx) > 0.01 ? e->y : ey + dy[direction] + 0.5;
                }
            }
        }
        double vx = tx - e->x, vy = ty - e->y, len = hypot(vx, vy);
        double speed = (0.80 + 0.09 * g->keys) * dt;
        if (len > 0.01 && distance > 0.48) {
            double step = fmin(len, speed);
            move_entity(g, &e->x, &e->y, vx / len * step, vy / len * step, 0.16);
        }
        if (distance < 0.80 && e->attack_cooldown <= 0.0 &&
            game_line_clear(g, e->x, e->y, g->x, g->y)) {
            g->hp = fmax(0.0, g->hp - 14.0);
            g->hurt_flash = 0.35;
            e->attack_cooldown = 1.0;
            set_message(g, "SENTINEL CONTACT! Press SPACE to stun nearby enemies.");
        }
    }
}

void game_update(Game *g, Controls input, double dt)
{
    if (g->mode != PLAYING || dt <= 0.0 || !isfinite(dt)) return;
    dt = fmin(dt, 0.10);
    g->elapsed += dt;
    g->remaining = fmax(0.0, g->remaining - dt);
    g->pulse_cooldown = fmax(0.0, g->pulse_cooldown - dt);
    g->pulse_flash = fmax(0.0, g->pulse_flash - dt);
    g->hurt_flash = fmax(0.0, g->hurt_flash - dt);
    g->message_time = fmax(0.0, g->message_time - dt);
    g->angle = remainder(g->angle + input.turn * 1.85 * dt, 2.0 * PI);
    double f = input.forward, s = input.strafe, length = hypot(f, s);
    if (length > 1.0) { f /= length; s /= length; }
    double speed = input.sprint ? 3.8 : 2.5;
    double dx = (cos(g->angle) * f - sin(g->angle) * s) * speed * dt;
    double dy = (sin(g->angle) * f + cos(g->angle) * s) * speed * dt;
    move_entity(g, &g->x, &g->y, dx, dy, 0.18);
    if (input.pulse && g->pulse_cooldown <= 0.0) {
        g->pulse_cooldown = 7.0;
        g->pulse_flash = 0.60;
        ++g->pulses_used;
        int hits = 0;
        for (int i = 0; i < ENEMY_COUNT; ++i) {
            Enemy *e = &g->enemy[i];
            double d = hypot(e->x - g->x, e->y - g->y);
            if (d < 5.0 && game_line_clear(g, g->x, g->y, e->x, e->y)) {
                e->stun = 3.0;
                if (d > 0.01)
                    move_entity(g, &e->x, &e->y, (e->x - g->x) / d * 1.2,
                                (e->y - g->y) / d * 1.2, 0.16);
                ++hits;
            }
        }
        set_message(g, hits ? "PULSE RELEASED // Sentinels stunned for 3 seconds." : "PULSE RELEASED // No sentinels in line of sight.");
    }
    for (int k = 0; k < KEY_COUNT; ++k) {
        if (!g->key[k].collected && hypot(g->x - g->key[k].x, g->y - g->key[k].y) < 0.60) {
            g->key[k].collected = 1;
            ++g->keys;
            g->hp = fmin(100.0, g->hp + 15.0);
            g->remaining += 15.0;
            set_message(g, g->keys == KEY_COUNT ? "ALL CORES ONLINE // Find the magenta extraction gate!" : "CORE ACQUIRED // +15 health / +15 seconds.");
        }
    }
    if (hypot(g->x - (g->exit_x + 0.5), g->y - (g->exit_y + 0.5)) < 0.65) {
        if (g->keys == KEY_COUNT) { g->mode = WON; return; }
        set_message(g, "GATE LOCKED // Collect all 3 cyan cores first.");
    }
    update_enemies(g, dt);
    reveal(g);
    if (g->hp <= 0.0 || g->remaining <= 0.0) g->mode = LOST;
}
