#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "raylib.h"
#define Color Color

#include "common.h"

#include "audio.h"
void SetFlags(void *handle);

static void destroy_all(context **ctx_in) {

    if (!*ctx_in)
        return;
    
    context *ctx = *ctx_in;

	MTY_MutexDestroy(&ctx->lock);

    // Audio Processing
    if (ctx->processing) {
        free(ctx->processing);
    }
    
    // Visualisers
    if (ctx->vis_array) {
        for (uint32_t i = 0; i < ctx->vis_count; i++) {
            free(ctx->vis_array[i].band_data);
        }

        free(ctx->vis_array);
    }

    // Context
    free(ctx);

    *ctx_in = NULL;
}

static uint32_t init_all(context **ctx_out) {
    uint32_t e = 0;

    destroy_all(ctx_out);

    // Context
    context *ctx = *ctx_out = calloc(1, sizeof(context));
    
    // Visualisers
    ctx->vis_count = 4;
    ctx->vis_array = calloc(ctx->vis_count, sizeof(visualiser));

    // Settings

    uint32_t heights[4] = {180, 170, 160, 150};

    Color tmp = {38, 37, 36, 255};
    uint8_t opacities[4] = {63, 127, 191, 255};

    uint32_t bands[4] = {9, 17, 33, 65};
    
    for (uint32_t i = 0; i < ctx->vis_count ; i++) {
        uint32_t index = 4 - ctx->vis_count + i;
        tmp.a = opacities[index];

        ctx->vis_array[i].colour = tmp;
        ctx->vis_array[i].height = heights[index];
        ctx->vis_array[i].bands = bands[index];
    
        ctx->vis_array[i].width = 2560;
        ctx->vis_array[i].position_x = 0;
        ctx->vis_array[i].position_y = 1410;
        ctx->vis_array[i].freq_min = 20;
        ctx->vis_array[i].freq_max = 20000;
        ctx->vis_array[i].sensitivity = 35;
        calc_band_freqs(&ctx->vis_array[i]);
        ctx->vis_array[i].band_data = calloc(ctx->vis_array[i].bands, sizeof(float));
    }

    // Audio Processing
    ctx->processing = calloc(1, sizeof(audio_processing));
    ctx->processing->fft_ms = 100;

	ctx->lock = MTY_MutexCreate();

    return e;
}

void auto_resize(uint32_t *width, uint32_t *height) {
    uint32_t monitor = GetCurrentMonitor();
    uint32_t new_width = GetMonitorWidth(monitor);
    uint32_t new_height = GetMonitorHeight(monitor);

    bool changed = false;
    if (new_width != *width) {
        *width = new_width;
        changed = true;
    }
    if (new_height != *height) {
        *height = new_height;
        changed = true;
    }
    if (changed) {
        SetWindowSize(*width, *height+1);
        SetWindowPosition(0, 0);
    }
}

void draw_vis(struct visualiser vis) {
    float start_y = (float) vis.position_y;
    float width_step = (float) vis.width / (float) (vis.bands - 1);

    
    for (uint32_t i=0; i < vis.bands-1; i++) {
        // Cap
        Vector2 p1 = {width_step*(i+1), start_y - floorf(vis.band_data[i + 1]*vis.height)};
        Vector2 p2 = {width_step*i, start_y - floorf(vis.band_data[i]*vis.height)};
        Vector2 p3 = {width_step*i, start_y};
        DrawTriangle(p1, p2, p3, vis.colour);

        // Base
        Vector2 p4 = {width_step*(i+1), start_y - floorf(vis.band_data[i + 1]*vis.height)};
        Vector2 p5 = {width_step*i, start_y };
        Vector2 p6 = {width_step*(i+1), start_y};
        DrawTriangle(p4, p5, p6, vis.colour);
    }
    return;
}

void draw_main(void *opaque) {
    struct context *ctx = (struct context *) opaque;

    BeginDrawing();
    ClearBackground(BLANK);

	MTY_MutexLock(ctx->lock);
    for (uint32_t i=0; i<ctx->vis_count; i++) {
        draw_vis(ctx->vis_array[i]);
    }
	MTY_MutexUnlock(ctx->lock);

    EndDrawing();
}


uint32_t main(void) {
    context *ctx = NULL;
    init_all(&ctx);
    ctx->running = 1;
    MTY_Thread *thread = MTY_ThreadCreate((MTY_ThreadFunc) work_thread, ctx);

    SetConfigFlags(FLAG_WINDOW_UNDECORATED | FLAG_WINDOW_TRANSPARENT | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(0, 0, NULL);
    ctx->refresh_rate = 165;
    SetFlags(GetWindowHandle());

    uint32_t step = 0;
    while (!WindowShouldClose())
    {
        if (!step) {
            auto_resize(&ctx->resolution_w, &ctx->resolution_h);
        }
        step = (step+1) % ctx->refresh_rate;

        draw_main(ctx);
    }
    ctx->running = 0;

    while (ctx->processing->work_running) {
        WaitTime(1);
    }
    MTY_ThreadDestroy(&thread);
    destroy_all(&ctx);

    return 0;
}

int32_t WinMain(void)
{
	return main();
}