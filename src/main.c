#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_timer.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define SAMPLE_RATE 44100
#define AUDIO_BUFFER 2048

static const double melody_notes[] = {
    164.81, 196.00, 220.00, 246.94,
    220.00, 196.00, 164.81, 146.83,
    164.81, 220.00, 261.63, 293.66,
    261.63, 220.00, 196.00, 164.81,
    130.81, 164.81, 196.00, 220.00,
    196.00, 164.81, 146.83, 130.81,
    146.83, 174.61, 196.00, 220.00,
    246.94, 220.00, 196.00, 164.81
};
#define MELODY_LEN (sizeof(melody_notes) / sizeof(melody_notes[0]))

static const double bass_notes[] = {
    82.41, 82.41, 110.00, 110.00,
    98.00, 98.00, 82.41, 82.41,
    73.42, 73.42, 98.00, 98.00,
    82.41, 82.41, 110.00, 82.41
};
#define BASS_LEN (sizeof(bass_notes) / sizeof(bass_notes[0]))

typedef struct {
    double melody_phase;
    double bass_phase;
    double arp_phase;
    int sample_pos;
    int note_duration;
} AudioState;

static AudioState audio_state = {0};

static void audio_callback(void *userdata, Uint8 *stream, int len)
{
    (void)userdata;
    Sint16 *samples = (Sint16 *)stream;
    int num_samples = len / sizeof(Sint16);

    int samples_per_note = SAMPLE_RATE / 6;

    for (int i = 0; i < num_samples; i++) {
        int note_idx = audio_state.sample_pos / samples_per_note;
        int melody_idx = note_idx % MELODY_LEN;
        int bass_idx = (note_idx / 2) % BASS_LEN;

        double melody_freq = melody_notes[melody_idx];
        double bass_freq = bass_notes[bass_idx];

        int pos_in_note = audio_state.sample_pos % samples_per_note;
        double envelope = 1.0;
        if (pos_in_note < samples_per_note / 20) {
            envelope = (double)pos_in_note / (samples_per_note / 20);
        } else if (pos_in_note > samples_per_note * 3 / 4) {
            envelope = 1.0 - (double)(pos_in_note - samples_per_note * 3 / 4) / (samples_per_note / 4);
        }

        double melody_sample = sin(audio_state.melody_phase) * 0.3;
        melody_sample += sin(audio_state.melody_phase * 2) * 0.1;

        double bass_sample = sin(audio_state.bass_phase) * 0.25;
        bass_sample += (fmod(audio_state.bass_phase, 2 * M_PI) < M_PI ? 0.1 : -0.1) * 0.15;

        double arp_freq = melody_freq * (1 + (note_idx % 3));
        double arp_sample = sin(audio_state.arp_phase) * 0.08;

        double mixed = (melody_sample * envelope + bass_sample + arp_sample) * 0.6;
        samples[i] = (Sint16)(mixed * 20000);

        audio_state.melody_phase += 2 * M_PI * melody_freq / SAMPLE_RATE;
        audio_state.bass_phase += 2 * M_PI * bass_freq / SAMPLE_RATE;
        audio_state.arp_phase += 2 * M_PI * arp_freq / SAMPLE_RATE;

        if (audio_state.melody_phase > 2 * M_PI) audio_state.melody_phase -= 2 * M_PI;
        if (audio_state.bass_phase > 2 * M_PI) audio_state.bass_phase -= 2 * M_PI;
        if (audio_state.arp_phase > 2 * M_PI) audio_state.arp_phase -= 2 * M_PI;

        audio_state.sample_pos++;
    }
}

typedef struct {
    SDL_Texture *tex;
    int orig_w, orig_h;
} Icon;

typedef struct {
    double x, y, z;
} Vec3;

static const Vec3 merkaba_verts[8] = {
    { 1,  1,  1}, { 1, -1, -1}, {-1,  1, -1}, {-1, -1,  1},
    {-1, -1, -1}, {-1,  1,  1}, { 1, -1,  1}, { 1,  1, -1}
};

static const int merkaba_edges[12][2] = {
    {0,1}, {0,2}, {0,3}, {1,2}, {1,3}, {2,3},
    {4,5}, {4,6}, {4,7}, {5,6}, {5,7}, {6,7}
};

static Vec3 rotate_point(Vec3 p, double ax, double ay, double az)
{
    double cx = cos(ax), sx = sin(ax);
    double cy = cos(ay), sy = sin(ay);
    double cz = cos(az), sz = sin(az);

    double y1 = p.y * cx - p.z * sx;
    double z1 = p.y * sx + p.z * cx;
    double x2 = p.x * cy + z1 * sy;
    double z2 = -p.x * sy + z1 * cy;
    double x3 = x2 * cz - y1 * sz;
    double y3 = x2 * sz + y1 * cz;

    return (Vec3){x3, y3, z2};
}

static void project_point(Vec3 p, double scale, int cx, int cy, int *sx, int *sy)
{
    double z_offset = 4.0;
    double perspective = z_offset / (z_offset + p.z);
    *sx = cx + (int)(p.x * scale * perspective);
    *sy = cy + (int)(p.y * scale * perspective);
}

static void render_merkaba(SDL_Renderer *renderer, int cx, int cy, int size, double angle)
{
    double ax = angle * 0.7;
    double ay = angle;
    double az = angle * 0.3;
    double scale = size * 0.35;

    Vec3 rotated[8];
    int screen_x[8], screen_y[8];

    for (int i = 0; i < 8; i++) {
        rotated[i] = rotate_point(merkaba_verts[i], ax, ay, az);
        project_point(rotated[i], scale, cx, cy, &screen_x[i], &screen_y[i]);
    }

    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    for (int i = 0; i < 12; i++) {
        int a = merkaba_edges[i][0];
        int b = merkaba_edges[i][1];
        SDL_RenderDrawLine(renderer, screen_x[a], screen_y[a], screen_x[b], screen_y[b]);
    }
}

#define NUM_MENU_ITEMS 6

static const char *menu_icon_paths[NUM_MENU_ITEMS] = {
    "assets/the_world.png",
    "assets/browser.png",
    "assets/mail.png",
    "assets/news.png",
    "assets/bbs.png",
    "assets/audio.png"
};

static const char *splash_path = "assets/splash.png";
static const char *back_path   = "assets/back.png";

static Icon splash_icon = {NULL, 0, 0};
static Icon back_icon  = {NULL, 0, 0};
static Icon menu_icons[NUM_MENU_ITEMS] = {{0}};

enum { SPLASH, MENU, CONTENT } state = SPLASH;
static int selected_index = -1;

static int win_w = 800, win_h = 600;
static double cube_angle = 0.0;
static int cube_cx = 400, cube_cy = 300, cube_size = 200;

static SDL_Rect splash_dst = {0};
static SDL_Rect selected_dst = {0};
static SDL_Rect back_dst = {0};
static SDL_Rect menu_dst[NUM_MENU_ITEMS] = {{0}};

static Icon load_icon(SDL_Renderer *renderer, const char *path)
{
    SDL_Texture *t = IMG_LoadTexture(renderer, path);
    if (!t) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
            "Failed to load %s: %s", path, IMG_GetError());
        return (Icon){NULL, 0, 0};
    }
    int w, h;
    SDL_QueryTexture(t, NULL, NULL, &w, &h);
    return (Icon){t, w, h};
}

static void update_positions(void);

int main(void)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init error: %s", SDL_GetError());
        return 1;
    }

    SDL_AudioSpec want, have;
    SDL_AudioDeviceID audio_dev;
    SDL_memset(&want, 0, sizeof(want));
    want.freq = SAMPLE_RATE;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = AUDIO_BUFFER;
    want.callback = audio_callback;

    audio_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (audio_dev != 0) {
        SDL_PauseAudioDevice(audio_dev, 0);
    }

    SDL_Window *window = SDL_CreateWindow("Altimit OS",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        800, 600, SDL_WINDOW_RESIZABLE);
    if (!window) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Window creation failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Renderer creation failed: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    if ((IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) == 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "IMG_Init PNG failed: %s", IMG_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    splash_icon = load_icon(renderer, splash_path);
    back_icon  = load_icon(renderer, back_path);

    for (int i = 0; i < NUM_MENU_ITEMS; i++) {
        menu_icons[i] = load_icon(renderer, menu_icon_paths[i]);
    }

    update_positions();

    Uint64 splash_start = SDL_GetTicks64();

    SDL_Event event;
    int running = 1;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = 0;
            } else if (event.type == SDL_WINDOWEVENT &&
                       event.window.event == SDL_WINDOWEVENT_RESIZED) {
                win_w = event.window.data1;
                win_h = event.window.data2;
                update_positions();
            } else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                int mx = event.button.x;
                int my = event.button.y;

                if (state == SPLASH) {
                    state = MENU;
                    update_positions();
                } else if (state == MENU) {
                    for (int i = 0; i < NUM_MENU_ITEMS; i++) {
                        if (menu_icons[i].tex &&
                            mx >= menu_dst[i].x && mx < menu_dst[i].x + menu_dst[i].w &&
                            my >= menu_dst[i].y && my < menu_dst[i].y + menu_dst[i].h) {
                            selected_index = i;
                            state = CONTENT;
                            update_positions();
                            break;
                        }
                    }
                } else if (state == CONTENT && back_icon.tex &&
                           mx >= back_dst.x && mx < back_dst.x + back_dst.w &&
                           my >= back_dst.y && my < back_dst.y + back_dst.h) {
                    state = MENU;
                    update_positions();
                }
            }
        }

        if (state == SPLASH && SDL_GetTicks64() - splash_start > 10000) {
            state = MENU;
            update_positions();
        }

        cube_angle += 0.002;
        if (cube_angle > 2 * M_PI) cube_angle -= 2 * M_PI;

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        if (state == SPLASH && splash_icon.tex) {
            SDL_RenderCopy(renderer, splash_icon.tex, NULL, &splash_dst);
        } else if (state == MENU) {
            render_merkaba(renderer, cube_cx, cube_cy, cube_size, cube_angle);
            for (int i = 0; i < NUM_MENU_ITEMS; i++) {
                if (menu_icons[i].tex) {
                    SDL_RenderCopy(renderer, menu_icons[i].tex, NULL, &menu_dst[i]);
                }
            }
        } else if (state == CONTENT && selected_index >= 0 && menu_icons[selected_index].tex) {
            SDL_RenderCopy(renderer, menu_icons[selected_index].tex, NULL, &selected_dst);
            if (back_icon.tex) {
                SDL_RenderCopy(renderer, back_icon.tex, NULL, &back_dst);
            }
        }

        SDL_RenderPresent(renderer);
    }

    if (splash_icon.tex) SDL_DestroyTexture(splash_icon.tex);
    if (back_icon.tex) SDL_DestroyTexture(back_icon.tex);
    for (int i = 0; i < NUM_MENU_ITEMS; i++) {
        if (menu_icons[i].tex) SDL_DestroyTexture(menu_icons[i].tex);
    }

    if (audio_dev != 0) {
        SDL_CloseAudioDevice(audio_dev);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    SDL_Quit();
    return 0;
}

static void update_positions(void)
{
    const int edge_buffer = 40;

    int available_w = win_w - 2 * edge_buffer;
    int available_h = win_h - 2 * edge_buffer;
    if (available_w < 100) available_w = 100;
    if (available_h < 100) available_h = 100;

    /* splash */
    if (splash_icon.tex && splash_icon.orig_w > 0 && splash_icon.orig_h > 0) {
        double ratio = fmin((double)available_w / splash_icon.orig_w,
                            (double)available_h / splash_icon.orig_h) * 0.9;
        int rw = (int)(splash_icon.orig_w * ratio);
        int rh = (int)(splash_icon.orig_h * ratio);
        splash_dst.x = (win_w - rw) / 2;
        splash_dst.y = (win_h - rh) / 2;
        splash_dst.w = rw;
        splash_dst.h = rh;
    }

    /* menu - vertical stack on left */
    if (state == MENU) {
        int base_item_size = 80;
        int base_item_spacing = 20;
        int total_height = NUM_MENU_ITEMS * base_item_size + (NUM_MENU_ITEMS - 1) * base_item_spacing;

        double stack_scale = fmin(1.0, (double)available_h / total_height);
        int item_size = (int)(base_item_size * stack_scale);
        int item_spacing = (int)(base_item_spacing * stack_scale);

        int stack_height = NUM_MENU_ITEMS * item_size + (NUM_MENU_ITEMS - 1) * item_spacing;
        int stack_x = edge_buffer;
        int stack_y = (win_h - stack_height) / 2;

        int menu_right_edge = stack_x + item_size;

        for (int i = 0; i < NUM_MENU_ITEMS; i++) {
            if (!menu_icons[i].tex) continue;

            int item_y = stack_y + i * (item_size + item_spacing);

            double scale = fmin((double)item_size / menu_icons[i].orig_w,
                                (double)item_size / menu_icons[i].orig_h);
            int rw = (int)(menu_icons[i].orig_w * scale + 0.5);
            int rh = (int)(menu_icons[i].orig_h * scale + 0.5);

            menu_dst[i].x = stack_x;
            menu_dst[i].y = item_y + (item_size - rh) / 2;
            menu_dst[i].w = rw;
            menu_dst[i].h = rh;

            if (stack_x + rw > menu_right_edge) {
                menu_right_edge = stack_x + rw;
            }
        }

        int cube_buffer = 40;
        int cube_left = menu_right_edge + cube_buffer;
        int cube_right = win_w - edge_buffer;
        int cube_top = edge_buffer;
        int cube_bottom = win_h - edge_buffer;

        int cube_area_w = cube_right - cube_left;
        int cube_area_h = cube_bottom - cube_top;

        cube_cx = cube_left + cube_area_w / 2;
        cube_cy = cube_top + cube_area_h / 2;
        cube_size = (int)(fmin(cube_area_w, cube_area_h) * 0.8);
        if (cube_size < 50) cube_size = 50;
    }

    /* content screen */
    if (state == CONTENT && selected_index >= 0 && menu_icons[selected_index].tex) {
        int ow = menu_icons[selected_index].orig_w;
        int oh = menu_icons[selected_index].orig_h;
        int target_w = ow / 2;
        int target_h = oh / 2;

        double content_scale = fmin(1.0, fmin((double)available_w / target_w,
                                              (double)(available_h - 100) / target_h));
        selected_dst.w = (int)(target_w * content_scale);
        selected_dst.h = (int)(target_h * content_scale);
        selected_dst.x = (win_w - selected_dst.w) / 2;
        selected_dst.y = edge_buffer + 60;

        if (back_icon.tex) {
            int back_w = back_icon.orig_w;
            int back_h = back_icon.orig_h;
            if (back_w > available_w / 4) {
                double back_scale = (double)(available_w / 4) / back_w;
                back_w = (int)(back_w * back_scale);
                back_h = (int)(back_h * back_scale);
            }
            back_dst.x = win_w - back_w - edge_buffer;
            back_dst.y = win_h - back_h - edge_buffer;
            back_dst.w = back_w;
            back_dst.h = back_h;
        }
    }
}
