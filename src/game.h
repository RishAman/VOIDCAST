#ifndef VOIDCAST_GAME_H
#define VOIDCAST_GAME_H

#include <stdint.h>

#define MAP_W 31
#define MAP_H 21
#define KEY_COUNT 3
#define ENEMY_COUNT 3
#define PI 3.14159265358979323846
#define RUN_SECONDS 240.0

typedef enum { TITLE, PLAYING, PAUSED, WON, LOST } GameMode;

typedef struct {
    double x, y, stun, attack_cooldown;
} Enemy;

typedef struct {
    double x, y;
    int collected;
} Key;

typedef struct {
    double forward, strafe, turn;
    int sprint, pulse;
} Controls;

typedef struct {
    char map[MAP_H][MAP_W];
    unsigned char seen[MAP_H][MAP_W];
    double x, y, angle;
    double hp, remaining, elapsed, pulse_cooldown, pulse_flash, hurt_flash;
    double message_time;
    int keys, exit_x, exit_y, show_map, pulses_used;
    uint32_t seed, rng;
    GameMode mode;
    Key key[KEY_COUNT];
    Enemy enemy[ENEMY_COUNT];
    char message[100];
} Game;

typedef struct {
    double distance, texture_u;
    int side, map_x, map_y;
} RayHit;

void game_init(Game *g, uint32_t seed);
void game_start(Game *g);
void game_update(Game *g, Controls input, double dt);
int game_solid(const Game *g, int x, int y);
int game_can_stand(const Game *g, double x, double y, double radius);
int game_line_clear(const Game *g, double ax, double ay, double bx, double by);
void game_distances(const Game *g, int x, int y, int dist[MAP_H][MAP_W]);
RayHit game_cast_ray(const Game *g, double x, double y, double dx, double dy);

#endif
