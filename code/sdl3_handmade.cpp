#include "SDL3/SDL_events.h"
#include "SDL3/SDL_pixels.h"
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_messagebox.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_video.h>

#define internal static
#define local_persist static
#define global_variable static

global_variable bool game_is_running;

global_variable SDL_Texture* bitmap_texture;

internal void create_or_resize_bitmap_texture(
    SDL_Renderer* renderer,
    SDL_PixelFormat format,
    SDL_TextureAccess access,
    int width, int height
) {
    // TODO(orestis): Maybe don't free first, free after,
    // then free first if that fails.
    if (bitmap_texture) {
        SDL_DestroyTexture(bitmap_texture);
    }
    bitmap_texture = SDL_CreateTexture(
        renderer,
        format,
        access,
        width, height
    );
}

int main() {
    // SDL_Log and SDL_ShowSimpleMessageBox work
    // without initializing the video subsystem, and can work without
    // initialization.
    bool ok = SDL_InitSubSystem(SDL_INIT_VIDEO);
    if (!ok) {
        SDL_Log("Could not initialize video subsystem: %s", SDL_GetError());
        ok = SDL_ShowSimpleMessageBox(
                SDL_MESSAGEBOX_ERROR,
                "Handmade Hero",
                "Fatal application error: Could not initialize video subsystem",
                nullptr
        );
        if (!ok) {
            SDL_Log("Could not show message box: %s", SDL_GetError());
        }
        return 1;
    }


    SDL_Window* window = SDL_CreateWindow(
        "Handmade Hero",
        1280, 720,
        SDL_WINDOW_RESIZABLE
    );
    if (window == nullptr) {
        SDL_Log("Could not create window: %s", SDL_GetError());
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (renderer == nullptr) {
        SDL_Log("Could not create renderer: %s", SDL_GetError());
        return 1;
    }

    int bitmap_width, bitmap_height;
    if (!SDL_GetRenderOutputSize(renderer, &bitmap_width, &bitmap_height)) {
        SDL_Log("Could not get renderer output size: %s", SDL_GetError());
        return 1;
    }

    // Writes to global_variable bitmap_texture.
    create_or_resize_bitmap_texture(
        renderer,
        SDL_PIXELFORMAT_XRGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        bitmap_width, bitmap_height
    );
    if (bitmap_texture == nullptr) {
        SDL_Log("Could not create texture: %s", SDL_GetError());
        return 1;
    }

    game_is_running = true;
    while (game_is_running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_EVENT_QUIT: {
                    game_is_running = false;
                    break;
                }
                // https://wiki.libsdl.org/SDL3/README-highdpi
                // In SDL3, there's a difference between points and pixels:
                //
                // > The window size is now distinct from the window pixel size,
                // > and the ratio between the two is the window pixel density.
                // > You can query the window pixel size using
                // > SDL_GetWindowSizeInPixels(), and when this changes you get an
                // > SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED event. You are guaranteed
                // > to get a SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED event when a
                // > window is created and resized [...]
                //
                // So we use SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED, and we also use
                // SDL_GetRenderOutputSize to get the true output size in pixels,
                // since we're following Casey's approach of working with our own
                // backbuffer.
                case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: {
                    ok = SDL_GetRenderOutputSize(renderer, &bitmap_width, &bitmap_height);
                    if (!ok) {
                        SDL_Log("Could not get renderer output size: %s", SDL_GetError());
                    }
                    SDL_Log("Window resized. New pixel size: %d x %d", bitmap_width, bitmap_height);
                    create_or_resize_bitmap_texture(
                        renderer,
                        SDL_PIXELFORMAT_XRGB8888,
                        SDL_TEXTUREACCESS_STREAMING,
                        bitmap_width, bitmap_height
                    );
                    if (bitmap_texture == nullptr) {
                        // TODO(orestis): How and where should I handle this error?
                        SDL_Log("Could not create texture: %s", SDL_GetError());
                    }
                    break;
                }
                // No need to log anything for now.
                // default: {
                //     SDL_Log("%d", event.type);
                //     break;
                // }
            }
        }

        if (!SDL_RenderClear(renderer)) {
            SDL_Log("Could not clear renderer: %s", SDL_GetError());
            break;
        }
        if (!SDL_RenderTexture(renderer, bitmap_texture, nullptr, nullptr)) {
            SDL_Log("Could not copy texture to backbuffer: %s", SDL_GetError());
            break;
        }
        if (!SDL_RenderPresent(renderer)) {
            SDL_Log("Could not update the screen with new rendering: %s", SDL_GetError());
        }
    }

    // We don't free on shutdown. We let the OS reclaim everything
    // at process exit instead of freeing stuff one by one.
    SDL_Quit();
    return 0;
}
