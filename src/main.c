#include "game.h"
#include "platform.h"
#include "render.h"

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int run_tests(void);
int platform_open_window(void);

static void usage(void)
{
    puts("VOIDCAST // A pure-C ASCII extraction game for Windows\n"
         "\nUsage: voidcast.exe [options]\n"
         "  --window           Open the game in its own terminal (smooth held keys)\n"
         "  --seed NUMBER      Play a reproducible maze (0..4294967295)\n"
         "  --self-test        Run deterministic engine + rendering checks\n"
         "  --capture FILE.bmp Export a gameplay frame without opening a terminal\n"
         "  --title            Show the title screen in a capture\n"
         "  --demo             Non-interactive rotating camera\n"
         "  --frames NUMBER    Exit after this many rendered frames\n"
         "  --help             Show this help\n"
         "\nW/S move, A/D or arrows turn, Q/E strafe, Shift sprint, Space pulse.\n"
         "Tab/M map, P/Esc pause, R retry, N new maze, X quit.\n"
         "Collect all 3 cyan cores (*) and step into the magenta gate (E).\n"
         "Use an English keyboard layout. Recommended terminal: 128 x 42 or larger.");
}

static int number(const char *value, unsigned long *out)
{
    char *end = NULL;
    if (!*value || *value == '-') return 0;
    errno = 0;
    unsigned long n = strtoul(value, &end, 10);
    if (errno || *end || n > UINT32_MAX) return 0;
    *out = n;
    return 1;
}

int main(int argc, char **argv)
{
    uint32_t seed = (uint32_t)time(NULL);
    int demo = 0, title_capture = 0, frames = 0, new_window = 0;
    const char *capture = NULL;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0) { usage(); return 0; }
        if (strcmp(argv[i], "--self-test") == 0) return run_tests();
        if (strcmp(argv[i], "--window") == 0) { new_window = 1; continue; }
        if (strcmp(argv[i], "--demo") == 0) { demo = 1; continue; }
        if (strcmp(argv[i], "--title") == 0) { title_capture = 1; continue; }
        if (strcmp(argv[i], "--capture") == 0 && i + 1 < argc) { capture = argv[++i]; continue; }
        if ((strcmp(argv[i], "--seed") == 0 || strcmp(argv[i], "--frames") == 0) && i + 1 < argc) {
            int is_seed = strcmp(argv[i], "--seed") == 0;
            unsigned long n;
            if (!number(argv[++i], &n) || (!is_seed && (n == 0 || n > INT_MAX))) {
                fprintf(stderr, "Invalid numeric argument: %s\n", argv[i]);
                return 2;
            }
            if (is_seed) seed = (uint32_t)n;
            else frames = (int)n;
            continue;
        }
        fprintf(stderr, "Unknown or incomplete option: %s\nUse --help for options.\n", argv[i]);
        return 2;
    }
    if (new_window) {
        if (argc != 2) { fprintf(stderr, "Use --window by itself; pass other options to the game directly.\n"); return 2; }
        return platform_open_window() ? 0 : 1;
    }
    Game game;
    static Screen screen;
    game_init(&game, seed);
    if (capture) {
        if (!title_capture) {
            game_start(&game);
            game.angle = 0.20;
        }
        render_game(&screen, &game, 128, 42, 60.0);
        if (!render_save_bmp(&screen, capture)) {
            fprintf(stderr, "Could not write capture: %s\n", capture);
            return 1;
        }
        printf("Saved %s (1280 x 840)\n", capture);
        return 0;
    }
    if (!platform_init()) return 1;
    if (demo) game_start(&game);
    double previous = platform_time(), fps = 60.0;
    int rendered = 0, ok = 1;
    while (!platform_interrupted()) {
        double start = platform_time(), dt = start - previous;
        previous = start;
        if (dt > 0.10) dt = 0.10;
        if (dt > 0.004) fps = fps * 0.96 + 0.04 / dt;
        Input input = platform_input();
        if (input.quit) break;
        if (input.map) game.show_map = !game.show_map;
        if (input.new_maze) {
            game_init(&game, game.seed + 1u);
            game_start(&game);
        } else if (input.restart || (input.enter && (game.mode == WON || game.mode == LOST))) {
            game_init(&game, game.seed);
            game_start(&game);
        } else if (input.enter && game.mode == TITLE) game_start(&game);
        else if (input.pause || (input.enter && game.mode == PAUSED)) {
            if (game.mode == PLAYING) game.mode = PAUSED;
            else if (game.mode == PAUSED) game.mode = PLAYING;
        }
        int width, height;
        platform_size(&width, &height);
        if (demo) {
            game.angle += dt * 0.25;
            game.elapsed += dt;
        } else if (width >= 72 && height >= 26) game_update(&game, input.controls, dt);
        render_game(&screen, &game, width, height, fps);
        if (!platform_present(&screen)) { ok = 0; break; }
        if (frames && ++rendered >= frames) break;
        double budget = 1.0 / 60.0 - (platform_time() - start);
        if (budget > 0.001) platform_sleep((unsigned)(budget * 1000.0));
    }
    platform_shutdown();
    return ok ? 0 : 1;
}
