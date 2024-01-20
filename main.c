/* -------------------------------------------------------------------------- */
/*                                  INCLUDES                                  */
/* -------------------------------------------------------------------------- */

#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>

#include "SDL.h"

/* -------------------------------------------------------------------------- */
/*                                    DATA                                    */
/* -------------------------------------------------------------------------- */

// Simulation configuration
struct config {
    int width;
    int height;
    float scale;
    int delay;
    uint32_t bg_color;
    uint32_t fg_color;
    uint32_t cp_color;
};

struct config C = {
    .width = 500,
    .height = 300,
    .scale = 3,
    .delay = 20,
    .bg_color = 0x000000FF,
    .fg_color = 0xFFFFFFFF,
    .cp_color = 0x444444FF
};

// Ghost cursor
struct cursor_pixel {
    int x;
    int y;
    int prev_x;
    int prev_y;
    bool prev_state;
    bool active;
};

struct cursor_pixel cp = {
    .x = 0,
    .y = 0,
    .prev_x = 0,
    .prev_y = 0,
    .prev_state = false,
    .active = false
};

// Program state
typedef enum {
    RUNNING,
    PAUSED,
    QUIT
} state_t;

SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;
SDL_Cursor *cursor = NULL;
state_t state = RUNNING;
bool *grid;
bool *next_gen;
size_t grid_size;
uint8_t bg_r, bg_g, bg_b, bg_a;
uint8_t fg_r, fg_g, fg_b, fg_a;

/* -------------------------------------------------------------------------- */
/*                                    UTILS                                   */
/* -------------------------------------------------------------------------- */

/**
 * Extract color components in RGBA format
 * @param color RGBA color
 * @param r Pointer to R component
 * @param g Pointer to G component
 * @param b Pointer to B component
 * @param a Pointer to A component
*/
void extract_color(uint32_t color, uint8_t *r, uint8_t *g, uint8_t *b, uint8_t *a) {
    *r = (color >> 24) & 0xFF;
    *g = (color >> 16) & 0xFF;
    *b = (color >>  8) & 0xFF;
    *a = (color >>  0) & 0xFF;
}

/* -------------------------------------------------------------------------- */
/*                                 SIMULATION                                 */
/* -------------------------------------------------------------------------- */

/**
 * Get cell state
 * @param arr Array to access
 * @param x X coordinate
 * @param y Y coordinate
 * @return Cell state
*/
bool get_cell(bool *arr, int x, int y) {
    return arr[x * C.height + y];
}

/**
 * Set cell state
 * @param arr Array to access
 * @param x X coordinate
 * @param y Y coordinate
 * @param val State to set
*/
void set_cell(bool *arr, int x, int y, bool val) {
    arr[x * C.height + y] = val;
}

/**
 * Check if cell lives or dies in the next iteration
 * @param x X coordinate
 * @param y Y coordinate
 * @return Whether cell lives
*/
bool is_alive(int x, int y) {
    int neighbors = 0;

    // Left
    if (x > 0 && get_cell(grid, x-1, y)) neighbors++;
    // Right
    if (x < C.width - 1 && get_cell(grid, x+1, y)) neighbors++;
    // Top
    if (y > 0 && get_cell(grid, x, y-1)) neighbors++;
    // Bottom
    if (y < C.height - 1 && get_cell(grid, x, y+1)) neighbors++;
    // Top-left
    if (x > 0 && y > 0 && get_cell(grid, x-1, y-1)) neighbors++;
    // Top-right
    if (x < C.width - 1 && y > 0 && get_cell(grid, x+1, y-1)) neighbors++;
    // Bottom-left
    if (x > 0 && y < C.height - 1 && get_cell(grid, x-1, y+1)) neighbors++;
    // Bottom-right
    if (x < C.width - 1 && y < C.height - 1 && get_cell(grid, x+1, y+1)) neighbors++;

    // (Alive) Less than two or more than 3 neighbors -> die
    if (get_cell(grid, x, y) && (neighbors < 2 || neighbors > 3)) return false;
    // (Alive) Two or three neighbors -> live
    if (get_cell(grid, x, y) && (neighbors == 2 || neighbors == 3)) return true;
    /// (Dead) Three neighbors -> live
    if (!get_cell(grid, x, y) && neighbors == 3) return true;

    return false;
}

/**
 * Clear grid data
*/
void clear_grid() {
    memset(grid, 0, grid_size);
}

/**
 * Fill grid with random data
*/
void fill_grid() {
    for (int i = 0; i < C.width; i++) {
        for (int j = 0; j < C.height; j++) {
            set_cell(grid, i, j, rand() % 2);
        }
    }
}

/**
 * Advance one step of the simulation
*/
void update_grid() {
    for (int i = 0; i < C.width; i++) {
        for (int j = 0; j < C.height; j++) {
            set_cell(next_gen, i, j, is_alive(i, j));
        }
    }

    memcpy(grid, next_gen, grid_size);
}

/* -------------------------------------------------------------------------- */
/*                                     SDL                                    */
/* -------------------------------------------------------------------------- */

/**
 * Initialize SDL subsystems and components
 * @return Whether initialization was successful
*/
bool sdl_init() {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "Unable to initialize SDL subsystems: '%s'\n", SDL_GetError());
        return false;
    }

    window = SDL_CreateWindow("Game of Life", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, C.width * C.scale, C.height * C.scale, 0);
    if (!window) {
        fprintf(stderr, "Unable to create window: '%s'\n", SDL_GetError());
        return false;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        fprintf(stderr, "Unable to create renderer: '%s'\n", SDL_GetError());
        return false;
    }

    if (SDL_RenderSetScale(renderer, C.scale, C.scale) != 0) {
        fprintf(stderr, "Unable to set render scale: '%s'\n", SDL_GetError());
        return false;
    }

    cursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_CROSSHAIR);
    if (!cursor) {
        fprintf(stderr, "Unable to create cursor: '%s'\n", SDL_GetError());
        return false;
    }

    SDL_SetCursor(cursor);

    return true;
}

/**
 * Clear screen pixels
*/
void clear_screen() {
    SDL_SetRenderDrawColor(renderer, bg_r, bg_g, bg_b, bg_a);
    SDL_RenderClear(renderer);
}

/**
 * Draw current grid state to the screen
*/
void update_screen() {
    clear_screen();
    SDL_SetRenderDrawColor(renderer, fg_r, fg_g, fg_b, fg_a);

    for (int i = 0; i < C.width; i++) {
        for (int j = 0; j < C.height; j++) {
            if (get_cell(grid, i, j)) SDL_RenderDrawPoint(renderer, i, j);
        }
    }

    SDL_RenderPresent(renderer);
    SDL_Delay(C.delay);
}

/**
 * Draw single pixel to the screen
 * @param x X coordinate
 * @param y Y coordinate
 * @param color Pixel color
*/
void draw_pixel(int x, int y, uint32_t color) {
    uint8_t r, g, b, a;
    extract_color(color, &r, &g, &b, &a);
    SDL_SetRenderDrawColor(renderer, r, g, b, a);

    SDL_RenderDrawPoint(renderer, x, y);
    SDL_RenderPresent(renderer);
}

/**
 * SDL event handler
*/
void handle_events() {
    SDL_Event event;
    int x, y;

    static bool draw_flag = false;
    static bool erase_flag = false;

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                // Quit simulation
                state = QUIT;
                return;
            
            case SDL_KEYDOWN:
                switch (event.key.keysym.sym) {
                    case SDLK_ESCAPE:
                        // Quit simulation
                        state = QUIT;
                        return;
                    
                    case SDLK_SPACE:
                        // Toggle pause
                        state = (state == RUNNING) ? PAUSED : RUNNING;
                        break;

                    case SDLK_c:
                        // Clear grid
                        clear_grid();
                        update_screen();
                        break;
                    
                    case SDLK_r:
                        // Randomize grid
                        fill_grid();
                        update_screen();
                        break;
                }
                break;
            
            case SDL_MOUSEBUTTONDOWN:
                // Get click coordinates
                x = event.button.x / (uint32_t)C.scale;
                y = event.button.y / (uint32_t)C.scale;
                
                if (event.button.button == SDL_BUTTON_LEFT) {
                    // Draw pixel and set draw flag
                    set_cell(grid, x, y, true);
                    draw_pixel(x, y, C.fg_color);
                    draw_flag = true;
                } else if (event.button.button == SDL_BUTTON_RIGHT) {
                    // Erase pixel and set erase flag
                    set_cell(grid, x, y, false);
                    draw_pixel(x, y, C.bg_color);
                    erase_flag = true;
                }

                break;
            
            case SDL_MOUSEBUTTONUP:
                // Unset draw and erase flags
                if (event.button.button == SDL_BUTTON_LEFT) draw_flag = false;
                if (event.button.button == SDL_BUTTON_RIGHT) erase_flag = false;

                break;
            
            case SDL_MOUSEMOTION:
                // Get cursor coordinates
                x = event.motion.x / (uint32_t)C.scale;
                y = event.motion.y / (uint32_t)C.scale;

                if (draw_flag) {
                    // Draw trail
                    set_cell(grid, x, y, true);
                    draw_pixel(x, y, C.fg_color);
                }

                if (erase_flag) {
                    // Erase trail
                    set_cell(grid, x, y, false);
                    draw_pixel(x, y, C.bg_color);
                }

                break;
            
            case SDL_WINDOWEVENT:
                // Toggle ghost cursor depending on window mouse focus
                if (event.window.event == SDL_WINDOWEVENT_ENTER && !cp.active) cp.active = true;
                if (event.window.event == SDL_WINDOWEVENT_LEAVE && cp.active) cp.active = false;

                break;
        }
    }
}

/**
 * Draw ghost cursor
*/
void draw_cursor_pixel() {
    if (!cp.active) {
        // Erase ghost cursor when mouse focus is lost
        draw_pixel(cp.prev_x, cp.prev_y, cp.prev_state ? C.fg_color : C.bg_color);
        return;
    }

    // Get cursor coordinates
    SDL_GetMouseState(&cp.x, &cp.y);
    cp.x = cp.x / (uint32_t)C.scale;
    cp.y = cp.y / (uint32_t)C.scale;

    // Draw ghost cursor over background only
    if (!get_cell(grid, cp.x, cp.y)) draw_pixel(cp.x, cp.y, C.cp_color);

    // Reset previous pixel to its corresponding color
    if (cp.x != cp.prev_x || cp.y != cp.prev_y) draw_pixel(cp.prev_x, cp.prev_y, cp.prev_state ? C.fg_color : C.bg_color);

    // Update previous pixel information
    cp.prev_x = cp.x;
    cp.prev_y = cp.y;
    cp.prev_state = get_cell(grid, cp.x, cp.y);
}

/**
 * Destroy SDL components and quit SDL
*/
void sdl_cleanup() {
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_FreeCursor(cursor);
    SDL_Quit();
}

/* -------------------------------------------------------------------------- */
/*                                  ARGUMENTS                                 */
/* -------------------------------------------------------------------------- */

/**
 * Set up simulation config from args
 * @param argc Number of args
 * @param argv Args list
 * @return Whether setup was successful
*/
bool set_config(int argc, char **argv) {
    int opt;
    while ((opt = getopt(argc, argv, ":w:h:s:d:b:f:c:")) != -1) {
        switch (opt) {
            case 'w':
                // Width
                C.width = atoi(optarg);
                if (C.width <= 0) {
                    fprintf(stderr, "Invalid width value\n");
                    return false;
                }
                break;

            case 'h':
                // Height
                C.height = atoi(optarg);
                if (C.height <= 0) {
                    fprintf(stderr, "Invalid height value\n");
                    return false;
                }
                break;

            case 's':
                // Scale
                C.scale = atof(optarg);
                if (C.scale <= 0) {
                    fprintf(stderr, "Invalid scale value\n");
                    return false;
                }
                break;

            case 'd':
                // Delay
                C.delay = atoi(optarg);
                if (C.delay < 0) {
                    fprintf(stderr, "Invalid delay value\n");
                    return false;
                }
                break;
            
            case 'b':
                // Background color
                C.bg_color = (uint32_t)strtoul(optarg, NULL, 16);
                break;
            
            case 'f':
                // Foreground color
                C.fg_color = (uint32_t)strtoul(optarg, NULL, 16);
                break;
            
            case 'c':
                // Ghost cursor color
                C.cp_color = (uint32_t)strtoul(optarg, NULL, 16);
                break;

            case ':':
                fprintf(stderr, "Option requires a value\n");
                return false;

            case '?':
                fprintf(stderr, "Unknown option\n");
                return false;
        }
    }

    // Args
    int args_len = 0;
    int arg_pos;
    for (; optind < argc; optind++) {
        if (args_len == 0) arg_pos = optind;
        args_len++;
    }

    if (args_len == 1 && strcmp(argv[arg_pos], "help") == 0) {
        printf("Usage: game-of-life [-w WIDTH] [-h HEIGHT] [-s SCALE] [-d DELAY] [-b BG_COLOR] [-f FG_COLOR] [-c CP_COLOR]\n");
        printf("\n");
        printf("Default values:\n");
        printf("  WIDTH -- 500\n");
        printf("  HEIGHT -- 300\n");
        printf("  SCALE -- 3\n");
        printf("  DELAY -- 20\n");
        printf("  BG_COLOR -- 000000FF\n");
        printf("  FG_COLOR -- FFFFFFFF\n");
        printf("  CP_COLOR -- 444444FF\n");
        exit(0);
    }

    return true;
}

/* -------------------------------------------------------------------------- */
/*                                    MAIN                                    */
/* -------------------------------------------------------------------------- */

/**
 * Application entry point
 * @param argc Number of args
 * @param argv Args list
 * @return Exit code
*/
int main(int argc, char **argv) {
    if (!set_config(argc, argv)) return EXIT_FAILURE;
    if (!sdl_init()) {
        sdl_cleanup();
        return EXIT_FAILURE;
    }

    // Initialize random number generation
    srand(time(NULL));

    // Initialize grids
    grid_size = C.width * C.height * sizeof(bool);
    grid = malloc(grid_size);
    next_gen = malloc(grid_size);
    memset(grid, 0, grid_size);
    memset(next_gen, 0, grid_size);

    // Get background and foreground color components
    extract_color(C.bg_color, &bg_r, &bg_g, &bg_b, &bg_a);
    extract_color(C.fg_color, &fg_r, &fg_g, &fg_b, &fg_a);

    // Main loop
    while (state != QUIT) {
        handle_events();

        if (state == PAUSED) {
            // Only draw ghost cursor when paused
            draw_cursor_pixel();
            continue;
        }

        update_grid();
        update_screen();
    }

    sdl_cleanup();

    return EXIT_SUCCESS;
}