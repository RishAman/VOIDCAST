# VOIDCAST

VOIDCAST is a 3D-style ASCII maze exploration game for Windows, written entirely in **C11**. It uses raycasting to draw the world with characters and color in a terminal, without a game engine or external libraries.

Collect **three cyan energy cores** and reach the magenta extraction gate before time runs out. Red sentinels pursue you through the corridors; use an energy pulse to knock them back and stun them temporarily.

## Play in VS Code

1. Open this `C project` folder in VS Code.
2. Press **Ctrl+Shift+B** to run the `VOIDCAST: Build & Play` task.
3. The game should open in a separate terminal window. Press **Enter** to start.
4. Use an English keyboard layout, and click the game window before using the controls.

The compiler path is configured for the original development machine: `C:\msys64\ucrt64\bin\gcc.exe`. On another machine, update the paths in `.vscode/tasks.json` and `.vscode/c_cpp_properties.json`. These JSON files configure the editor only; all game and test code is in `.c` and `.h` files.

A terminal of at least **128 columns × 42 rows** is recommended. If the game displays `TERMINAL TOO SMALL`, enlarge the window or reduce the font size. The game timer pauses while the terminal is too small. Close an existing game window before rebuilding because Windows locks a running `.exe`. This project has multiple source files, so use the supplied task to compile them together.

To play inside VS Code instead, press **Ctrl+Shift+P**, choose `Tasks: Run Task`, then select `VOIDCAST: Play in VS Code terminal`. Expand the terminal panel enough to fit the game. Holding a movement key in the integrated terminal may briefly pause because of the system's key-repeat delay; the separate window opened by `Ctrl+Shift+B` reads key states for smoother movement. If a separate window does not appear, the integrated-terminal task is a useful fallback.

## Controls

| Key | Action |
|---|---|
| W / S or Up / Down arrows | Move forward / backward |
| A / D or Left / Right arrows | Turn |
| Q / E | Strafe left / right |
| Hold Shift while moving | Sprint |
| Space | Energy pulse: knock back visible enemies within 5 cells and stun them for 3 seconds |
| Tab or M | Toggle the map |
| P or Esc | Pause / resume |
| R | Restart the same maze |
| N | Start a new maze |
| X | Quit |

## Rules

- You start with 100 health and four minutes.
- A cyan `*` on the map marks an energy core. Walk close to it to collect it automatically.
- Each core restores 15 health (up to 100) and adds 15 seconds.
- `E` marks the extraction gate; it turns green after you collect all three cores.
- A red `X` marks a sentinel. Contact costs 14 health per attack.
- The energy pulse has a seven-second cooldown and cannot pass through walls.
- The map is revealed as you explore, but core and gate locations are visible from the start.
- The seed displayed in the HUD lets you reproduce the same maze.

## Build manually

Open a terminal in the project folder and run:

```powershell
gcc -std=c11 -O2 -g -Wall -Wextra -Wpedantic src/main.c src/game.c src/render.c src/platform_win.c src/tests.c -o voidcast.exe -lm -lgdi32 -luser32
.\voidcast.exe --window
```

Alternatively, run `.\voidcast.exe --seed 42` to play maze seed 42 in the current terminal. The program uses the Windows API for keyboard input, timing, and console output; GDI is used only for exporting BMP images. The game itself is pure C, but these platform interfaces mean the current version supports **Windows only**.

## Source layout

| File | Purpose |
|---|---|
| `src/main.c` | Program entry point, command-line options, game loop, and state transitions |
| `src/game.c` / `src/game.h` | Maze generation, movement and collision, BFS enemy tracking, energy cores, pulse, and win/loss rules |
| `src/render.c` / `src/render.h` | Sprites, colors, map, HUD, menus, and rendering using raycasting |
| `src/platform_win.c` / `src/platform.h` | Keyboard input, timing, ANSI output, and terminal restoration on exit |
| `src/tests.c` | Automated tests written in C |
| `.vscode/` | Build, IntelliSense, and run-task configuration |

To modify the game, start with `src/game.h` for map dimensions, the number of cores and enemies, and the starting time. Some HUD text and artwork assume three cores, so update those as well if you change the count. Colors can be adjusted in `render_colors` in `src/render.c`.

## Test and export images

```powershell
.\voidcast.exe --self-test
.\voidcast.exe --help
.\voidcast.exe --seed 42 --capture gameplay.bmp
.\voidcast.exe --seed 42 --title --capture title.bmp
```

You can also run the `VOIDCAST: Test` task in VS Code. The test suite covers 128 maze seeds, collision, raycasting, core collection, timing, pausing, the energy pulse, win/loss conditions, paths through 12 mazes, enemy pursuit in 16 mazes, and different screen sizes. The path-to-goal tests freeze enemies so that goal reachability can be checked independently of combat difficulty.

Sample images in `captures/` are generated by the same renderer as the game and convert its characters to BMP pixels. They are useful for checking colors and layout, but they are not screenshots of the terminal window.

## What this project practices

`struct`, two-dimensional arrays, pointers, organizing `.c` and `.h` files, camera math, DFS maze generation, BFS pathfinding, DDA raycasting, collision detection, frame timing, state machines, and deterministic tests.

## Team credits

- Karee yu nai team
