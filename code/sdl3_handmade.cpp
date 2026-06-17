#include <cstddef>
#include <cstdint>
#include <cstdlib>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_messagebox.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_video.h>

#define internal        static
#define local_persist   static
#define global_variable static

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t  i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

global_variable bool game_is_running;

global_variable SDL_Texture* bitmap_texture;
global_variable void* bitmap_pixels;
global_variable int bitmap_width;
global_variable int bitmap_height;
global_variable int bytes_per_pixel = 4;

internal void render_weird_gradient(int x_offset, int y_offset) {
    int pitch = bitmap_width * bytes_per_pixel;
    u8* row = (u8*)bitmap_pixels;
    for (int y = 0; y < bitmap_height; ++y) {
        u32* pixel = (u32*)row;
        for (int x = 0; x < bitmap_width; ++x) {
            // NOTE(orestis): Due to little endian architecture, if you
            // have RR GG BB XX 32 bits, you'll get
            //      XX BB GG RR
            // Apparently, the people doing Win32 bitmaps wanted the
            // values in the CPU register to be intuitive. They didn't care
            // about the padding bit, they wanted get RR GG BB in-register.
            // so they flipped the order around to BB GG RR XX, to get
            //                                     XX RR GG BB
            // I don't know if that's the same historical reason SDL3
            // works in the same manner.
            u8 blue = x + x_offset;
            u8 green = y + y_offset;
            *pixel++ = (green << 8) | blue;
        }
        row += pitch;
    }

    // NOTE(orestis): This is not how you typically use SDL3.
    // From the SDL_UpdateTexture docs:
    // > This is a fairly slow function, intended for use with static textures
    // > that do not change often. If the texture is intended to be updated often,
    // > it is preferred to create the texture as streaming and use the locking
    // > functions referenced below.
    // (refers to SDL_LockTexture and SDL_UnlockTexture)
    // When working with SDL_LockTexture, the SDL model is different from the
    // one Casey follows: we don't own the backbuffer, SDL does, and gives us a
    // pointer to the pixels and the pitch.
    // For now, I want to lean towards Handmade style: we own the backbuffer.
    bool ok = SDL_UpdateTexture(bitmap_texture, 0, bitmap_pixels, pitch);
    if (!ok) {
        SDL_Log("Could not update bitmap texture: %s", SDL_GetError());
    }
}

internal void resize_bitmap_texture(
    SDL_Renderer*     renderer,
    SDL_PixelFormat   format,
    SDL_TextureAccess access
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
        bitmap_width, bitmap_height
    );

    if (bitmap_pixels) {
        free(bitmap_pixels);
    }
    size_t bitmap_size = bitmap_width * bitmap_height * bytes_per_pixel;
    bitmap_pixels = malloc(bitmap_size);
    // TODO(orestis): Probably want to clear this to black
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
                0
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
    if (!window) {
        SDL_Log("Could not create window: %s", SDL_GetError());
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, 0);
    if (!renderer) {
        SDL_Log("Could not create renderer: %s", SDL_GetError());
        return 1;
    }

    if (!SDL_GetRenderOutputSize(renderer, &bitmap_width, &bitmap_height)) {
        SDL_Log("Could not get renderer output size: %s", SDL_GetError());
        return 1;
    }

    // Writes to global_variable bitmap_texture.
    resize_bitmap_texture(
        renderer,
        SDL_PIXELFORMAT_XRGB8888,
        SDL_TEXTUREACCESS_STREAMING
    );
    if (!bitmap_texture) {
        SDL_Log("Could not create texture: %s", SDL_GetError());
        return 1;
    }

    int x_offset = 0;
    int y_offset = 0;

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
                    resize_bitmap_texture(
                        renderer,
                        SDL_PIXELFORMAT_XRGB8888,
                        SDL_TEXTUREACCESS_STREAMING
                    );
                    if (!bitmap_texture) {
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
        render_weird_gradient(x_offset, y_offset);
        ++x_offset;

        if (!SDL_RenderClear(renderer)) {
            SDL_Log("Could not clear renderer: %s", SDL_GetError());
            break;
        }
        if (!SDL_RenderTexture(renderer, bitmap_texture, 0, 0)) {
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
