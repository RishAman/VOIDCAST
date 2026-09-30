#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static HANDLE console_in, console_out;
static DWORD old_in_mode, old_out_mode;
static int active;
static volatile LONG interrupted;
static LARGE_INTEGER frequency;
static HWND game_window;
static WCHAR old_title[512];
static int have_old_title;
static double last_key_time[256];
static unsigned char was_down[256];
static int use_event_input;

int platform_open_window(void)
{
    WCHAR executable[MAX_PATH], command[MAX_PATH + 4];
    DWORD n = GetModuleFileNameW(NULL, executable, MAX_PATH);
    if (!n || n >= MAX_PATH) return 0;
    if (swprintf(command, MAX_PATH + 4, L"\"%ls\"", executable) < 0) return 0;
    STARTUPINFOW startup;
    PROCESS_INFORMATION process;
    memset(&startup, 0, sizeof(startup));
    memset(&process, 0, sizeof(process));
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USECOUNTCHARS;
    startup.dwXCountChars = 128;
    startup.dwYCountChars = 42;
    /* Only this process and its new child inherit this environment change. */
    SetEnvironmentVariableA("TERM_PROGRAM", NULL);
    if (!CreateProcessW(executable, command, NULL, NULL, FALSE, CREATE_NEW_CONSOLE,
                        NULL, NULL, &startup, &process)) {
        fprintf(stderr, "Could not open game terminal (Windows error %lu).\n", GetLastError());
        return 0;
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return 1;
}

static BOOL WINAPI on_control(DWORD event)
{
    if (event == CTRL_C_EVENT || event == CTRL_BREAK_EVENT || event == CTRL_CLOSE_EVENT) {
        InterlockedExchange(&interrupted, 1);
        return TRUE;
    }
    return FALSE;
}

static int output(const char *buffer, size_t size)
{
    while (size) {
        DWORD written = 0;
        if (!WriteFile(console_out, buffer, (DWORD)size, &written, NULL) || !written) return 0;
        buffer += written;
        size -= written;
    }
    return 1;
}

int platform_init(void)
{
    console_in = GetStdHandle(STD_INPUT_HANDLE);
    console_out = GetStdHandle(STD_OUTPUT_HANDLE);
    if (!GetConsoleMode(console_in, &old_in_mode) || !GetConsoleMode(console_out, &old_out_mode)) {
        fprintf(stderr, "VOIDCAST needs an interactive Windows terminal. Run .\\voidcast.exe in VS Code or Windows Terminal.\n");
        return 0;
    }
    DWORD mode = (old_in_mode | ENABLE_EXTENDED_FLAGS | ENABLE_WINDOW_INPUT) &
                 ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_QUICK_EDIT_MODE | ENABLE_MOUSE_INPUT);
    if (!SetConsoleMode(console_in, mode)) return 0;
    if (!SetConsoleMode(console_out, old_out_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) {
        SetConsoleMode(console_in, old_in_mode);
        fprintf(stderr, "This console does not support ANSI color. Use Windows Terminal or VS Code.\n");
        return 0;
    }
    QueryPerformanceFrequency(&frequency);
    HWND native_window = GetConsoleWindow();
    game_window = IsWindowVisible(native_window) ? native_window : GetForegroundWindow();
    /* VS Code's integrated terminal has no separate foreground HWND. Only consume
       console events there, so typing in an editor cannot control the game. */
    const char *term = getenv("TERM_PROGRAM");
    use_event_input = term && strcmp(term, "vscode") == 0;
    for (int i = 0; i < 256; ++i) last_key_time[i] = -100.0;
    SetConsoleCtrlHandler(on_control, TRUE);
    have_old_title = GetConsoleTitleW(old_title, 512) > 0;
    SetConsoleTitleW(L"VOIDCAST - ASCII Extraction");
    active = 1;
    atexit(platform_shutdown);
    static const char setup[] = "\x1b[?1049h\x1b[?25l\x1b[2J\x1b[H";
    if (!output(setup, sizeof(setup) - 1)) { platform_shutdown(); return 0; }
    return 1;
}

void platform_shutdown(void)
{
    if (!active) return;
    static const char cleanup[] = "\x1b[0m\x1b[?25h\x1b[?1049l";
    output(cleanup, sizeof(cleanup) - 1);
    SetConsoleMode(console_in, old_in_mode);
    SetConsoleMode(console_out, old_out_mode);
    if (have_old_title) SetConsoleTitleW(old_title);
    SetConsoleCtrlHandler(on_control, FALSE);
    active = 0;
}

void platform_size(int *width, int *height)
{
    CONSOLE_SCREEN_BUFFER_INFO info;
    *width = 119;
    *height = 36;
    if (GetConsoleScreenBufferInfo(console_out, &info)) {
        /* Reserve the last column to avoid terminal autowrap / accidental scrolling. */
        *width = info.srWindow.Right - info.srWindow.Left;
        *height = info.srWindow.Bottom - info.srWindow.Top + 1;
    }
}

double platform_time(void)
{
    LARGE_INTEGER now, hz;
    QueryPerformanceCounter(&now);
    if (!frequency.QuadPart) { QueryPerformanceFrequency(&hz); frequency = hz; }
    return (double)now.QuadPart / (double)frequency.QuadPart;
}

void platform_sleep(unsigned milliseconds) { Sleep(milliseconds); }
int platform_interrupted(void) { return InterlockedCompareExchange(&interrupted, 0, 0) != 0; }

Input platform_input(void)
{
    Input result = {0};
    int pressed[256] = {0};
    DWORD count;
    double now = platform_time();
    while (GetNumberOfConsoleInputEvents(console_in, &count) && count) {
        INPUT_RECORD events[64];
        DWORD read_count = 0;
        if (!ReadConsoleInputW(console_in, events, count > 64 ? 64 : count, &read_count)) break;
        for (DWORD i = 0; i < read_count; ++i) {
            if (events[i].EventType != KEY_EVENT) continue;
            KEY_EVENT_RECORD *k = &events[i].Event.KeyEvent;
            unsigned key = k->wVirtualKeyCode;
            if (key >= 256) continue;
            if (k->bKeyDown) {
                /* A console key event is evidence that this window owns input.
                   This also handles a newly created terminal taking focus late. */
                if (!use_event_input) game_window = GetForegroundWindow();
                if (!was_down[key] || now - last_key_time[key] > 0.24) pressed[key] = 1;
                last_key_time[key] = now;
                was_down[key] = 1;
                if (k->dwControlKeyState & SHIFT_PRESSED) last_key_time[VK_SHIFT] = now;
            } else if (!use_event_input) was_down[key] = 0;
        }
    }
    int down[256] = {0};
    if (use_event_input) {
        for (int i = 0; i < 256; ++i) {
            down[i] = now - last_key_time[i] < 0.14;
            if (!down[i]) was_down[i] = 0;
        }
    } else if (GetForegroundWindow() == game_window) {
        for (int i = 0; i < 256; ++i)
            down[i] = (GetAsyncKeyState(i) & 0x8000) != 0 || now - last_key_time[i] < 0.04;
    }
    result.controls.forward = (down['W'] || down[VK_UP]) - (down['S'] || down[VK_DOWN]);
    result.controls.turn = (down['D'] || down[VK_RIGHT]) - (down['A'] || down[VK_LEFT]);
    result.controls.strafe = down['E'] - down['Q'];
    result.controls.sprint = down[VK_SHIFT];
    result.controls.pulse = pressed[VK_SPACE];
    result.enter = pressed[VK_RETURN];
    result.pause = pressed['P'] || pressed[VK_ESCAPE];
    result.restart = pressed['R'];
    result.new_maze = pressed['N'];
    result.map = pressed[VK_TAB] || pressed['M'];
    result.quit = pressed['X'] || (pressed['C'] && (down[VK_CONTROL] || down[VK_LCONTROL]));
    return result;
}

int platform_present(const Screen *s)
{
    /* One batch per frame; color escapes are emitted only when a run changes. */
    static char buffer[SCREEN_MAX_W * SCREEN_MAX_H * 48 + 2048];
    size_t used = 0;
    int fg = -1, bg = -1;
    used += (size_t)snprintf(buffer + used, sizeof(buffer) - used, "\x1b[H");
    for (int y = 0; y < s->height; ++y) {
        if (y) used += (size_t)snprintf(buffer + used, sizeof(buffer) - used, "\x1b[%d;1H", y + 1);
        for (int x = 0; x < s->width; ++x) {
            Cell c = s->cells[y * s->width + x];
            if (c.fg != fg) {
                unsigned rgb = render_colors[c.fg];
                used += (size_t)snprintf(buffer + used, sizeof(buffer) - used,
                    "\x1b[38;2;%u;%u;%um", (rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255);
                fg = c.fg;
            }
            if (c.bg != bg) {
                unsigned rgb = render_colors[c.bg];
                used += (size_t)snprintf(buffer + used, sizeof(buffer) - used,
                    "\x1b[48;2;%u;%u;%um", (rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255);
                bg = c.bg;
            }
            buffer[used++] = (char)c.ch;
        }
        used += (size_t)snprintf(buffer + used, sizeof(buffer) - used, "\x1b[K");
    }
    return output(buffer, used);
}

/* Optional deterministic captures use the exact same Cell buffer as the terminal.
   GDI only rasterizes text for BMP export; gameplay rendering itself is ASCII. */
int render_save_bmp(const Screen *s, const char *path)
{
    int width = s->width * 10, height = s->height * 20;
    BITMAPINFO info;
    memset(&info, 0, sizeof(info));
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void *pixels = NULL;
    HDC dc = CreateCompatibleDC(NULL);
    if (!dc) return 0;
    HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &pixels, NULL, 0);
    HFONT font = CreateFontA(-18, 10, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET,
                            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                            FIXED_PITCH | FF_MODERN, "Consolas");
    if (!bitmap || !font) {
        if (bitmap) DeleteObject(bitmap);
        if (font) DeleteObject(font);
        DeleteDC(dc);
        return 0;
    }
    HGDIOBJ old_bitmap = SelectObject(dc, bitmap), old_font = SelectObject(dc, font);
    SetBkMode(dc, OPAQUE);
    for (int y = 0; y < s->height; ++y) {
        for (int x = 0; x < s->width; ++x) {
            Cell c = s->cells[y * s->width + x];
            unsigned f = render_colors[c.fg], b = render_colors[c.bg];
            SetTextColor(dc, RGB((f >> 16) & 255, (f >> 8) & 255, f & 255));
            SetBkColor(dc, RGB((b >> 16) & 255, (b >> 8) & 255, b & 255));
            RECT r = {x * 10, y * 20, (x + 1) * 10, (y + 1) * 20};
            char ch = (char)c.ch;
            ExtTextOutA(dc, r.left, r.top, ETO_OPAQUE | ETO_CLIPPED, &r, &ch, 1, NULL);
        }
    }
    GdiFlush();
    BITMAPFILEHEADER header;
    memset(&header, 0, sizeof(header));
    header.bfType = 0x4d42;
    header.bfOffBits = sizeof(header) + sizeof(BITMAPINFOHEADER);
    DWORD bytes = (DWORD)width * (DWORD)height * 4;
    header.bfSize = header.bfOffBits + bytes;
    FILE *file = fopen(path, "wb");
    int ok = 0;
    if (file) {
        ok = fwrite(&header, sizeof(header), 1, file) == 1 &&
             fwrite(&info.bmiHeader, sizeof(BITMAPINFOHEADER), 1, file) == 1 &&
             fwrite(pixels, bytes, 1, file) == 1;
        if (fclose(file) != 0) ok = 0;
    }
    SelectObject(dc, old_font);
    SelectObject(dc, old_bitmap);
    DeleteObject(font);
    DeleteObject(bitmap);
    DeleteDC(dc);
    return ok;
}
