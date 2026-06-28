#include <cstddef>
#include <cstdint>
#include <cstdlib>

#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_messagebox.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_scancode.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_gamepad.h>

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

struct PlatformLayer_OffscreenBuffer {
    SDL_Texture*    texture;
    void*           pixels;
    SDL_PixelFormat format;
    int             width;
    int             height;
    int             pitch;
    int             bytesPerPixel;
};

struct PlatformLayer_Audio {
    SDL_AudioSpec    spec;
    SDL_AudioStream* stream;
};

// TODO(orestis): These are global for now.
global_variable bool GAME_IS_RUNNING;
global_variable PlatformLayer_OffscreenBuffer GLOBAL_BACKBUFFER;
global_variable PlatformLayer_Audio GLOBAL_AUDIO;
global_variable SDL_Gamepad* GAMEPAD;
// NOTE(orestis): XInput recommended value for left thumb deadzone
// https://learn.microsoft.com/en-us/windows/win32/xinput/getting-started-with-xinput#dead-zone
global_variable int GAMEPAD_LEFT_THUMB_DEADZONE = 7849;

internal void RenderWeirdGradient(
    PlatformLayer_OffscreenBuffer* buffer,
    int xOffset, int yOffset
) {
    u8* row = (u8*)buffer->pixels;
    for (int y = 0; y < buffer->height; ++y) {
        // u32 writes can be faster than u8 writes.
        u32* pixel = (u32*)row;
        for (int x = 0; x < buffer->width; ++x) {
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
            u8 blue = x + xOffset;
            u8 green = y + yOffset;
            *pixel++ = (green << 8) | blue;
        }
        row += buffer->pitch;
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
    bool ok = SDL_UpdateTexture(buffer->texture, 0, buffer->pixels, buffer->pitch);
    if (!ok) {
        SDL_Log("Could not update bitmap texture: %s", SDL_GetError());
    }
}

internal void PlatformLayer_InitBackbuffer(
    PlatformLayer_OffscreenBuffer* buffer,
    int                            width,
    int                            height,
    SDL_PixelFormat                format
) {
    buffer->width = width;
    buffer->height = height;
    buffer->format = format;
    int bytesPerPixel = SDL_BYTESPERPIXEL(format);
    buffer->bytesPerPixel = bytesPerPixel;
    buffer->pitch = width * bytesPerPixel;
}

internal void PlatformLayer_ResizeBackBuffer(
    SDL_Renderer*                  renderer,
    SDL_TextureAccess              access,
    PlatformLayer_OffscreenBuffer* buffer
) {
    // TODO(orestis): Maybe don't free first, free after,
    // then free first if that fails.
    if (buffer->texture) {
        SDL_DestroyTexture(buffer->texture);
    }
    buffer->texture = SDL_CreateTexture(
        renderer,
        buffer->format,
        access,
        buffer->width, buffer->height
    );

    if (buffer->pixels) {
        free(buffer->pixels);
    }
    buffer->bytesPerPixel = SDL_BYTESPERPIXEL(buffer->format);
    size_t bitmap_size = buffer->width * buffer->height * buffer->bytesPerPixel;
    buffer->pitch = buffer->width * buffer->bytesPerPixel;
    buffer->pixels = malloc(bitmap_size);
}

internal void PlatformLayer_InitAudio(PlatformLayer_Audio* audio) {
    audio->spec = { SDL_AUDIO_S16, 2, 48000 };
    audio->stream = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
        &audio->spec,
        0,
        0
    );
    if (!audio->stream) {
        SDL_Log("Could not initialize audio stream: %s", SDL_GetError());
    }
}

int main() {
    bool ok = SDL_SetAppMetadata("Handmade Hero", "0.0.1", "com.example.handmade");
    if (!ok) {
        SDL_Log("Could not set app metadata: %s", SDL_GetError());
    }
    // NOTE(orestis): SDL_Log and SDL_ShowSimpleMessageBox work
    // without initializing the video subsystem, and can work without
    // initialization.
    ok = SDL_InitSubSystem(SDL_INIT_VIDEO);
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

    ok = SDL_InitSubSystem(SDL_INIT_AUDIO);
    if (!ok) {
        SDL_Log("Could not initialize audio subsystem: %s", SDL_GetError());
    }

    ok = SDL_InitSubSystem(SDL_INIT_GAMEPAD);
    if (!ok) {
        SDL_Log("Could not initialize gamepad subsystem: %s", SDL_GetError());
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

    {
        int width;
        int height;
        if (!SDL_GetRenderOutputSize(renderer, &width, &height)) {
            SDL_Log("Could not get renderer output size: %s", SDL_GetError());
            return 1;
        }
        PlatformLayer_InitBackbuffer(
            &GLOBAL_BACKBUFFER,
            width, height,
            SDL_PIXELFORMAT_XRGB8888
        );
    }

    // Writes to global_variable bitmap_texture.
    PlatformLayer_ResizeBackBuffer(
        renderer,
        SDL_TEXTUREACCESS_STREAMING,
        &GLOBAL_BACKBUFFER
    );
    if (!GLOBAL_BACKBUFFER.texture) {
        SDL_Log("Could not create texture: %s", SDL_GetError());
        return 1;
    }

    PlatformLayer_InitAudio(&GLOBAL_AUDIO);

    int xOffset = 0;
    int yOffset = 0;

    GAME_IS_RUNNING = true;
    while (GAME_IS_RUNNING) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_EVENT_QUIT: {
                    GAME_IS_RUNNING = false;
                    break;
                }
                // NOTE(orestis):
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
                    ok = SDL_GetRenderOutputSize(renderer, &GLOBAL_BACKBUFFER.width, &GLOBAL_BACKBUFFER.height);
                    if (!ok) {
                        SDL_Log("Could not get renderer output size: %s", SDL_GetError());
                    }
                    SDL_Log("Window resized. New pixel size: %d x %d", GLOBAL_BACKBUFFER.width, GLOBAL_BACKBUFFER.height);
                    PlatformLayer_ResizeBackBuffer(
                        renderer,
                        SDL_TEXTUREACCESS_STREAMING,
                        &GLOBAL_BACKBUFFER
                    );
                    if (!GLOBAL_BACKBUFFER.texture) {
                        SDL_Log("Could not create texture: %s", SDL_GetError());
                    }
                    break;
                }
                case SDL_EVENT_GAMEPAD_ADDED: {
                    if (!GAMEPAD) {
                        GAMEPAD = SDL_OpenGamepad(event.gdevice.which);
                        if (!GAMEPAD) {
                            SDL_Log("Could not open gamepad: %s", SDL_GetError());
                        }
                    }
                    break;
                }
                case SDL_EVENT_GAMEPAD_REMOVED: {
                    if (GAMEPAD && (SDL_GetGamepadID(GAMEPAD) == event.gdevice.which)) {
                        SDL_CloseGamepad(GAMEPAD);
                        GAMEPAD = 0;
                    }
                    break;
                }
            }
        }
        // NOTE(orestis): From the SDL3 docs:
        // > The pointer returned is a pointer to an internal SDL array. 
        // > It will be valid for the whole lifetime of the application
        // > and should not be freed by the caller.
        const bool* keyStates = SDL_GetKeyboardState(0);
        if (keyStates[SDL_SCANCODE_W]) {
            --yOffset;
        }
        if (keyStates[SDL_SCANCODE_A]) {
            --xOffset;
        }
        if (keyStates[SDL_SCANCODE_S]) {
            ++yOffset;
        }
        if (keyStates[SDL_SCANCODE_D]) {
            ++xOffset;
        }

        if (GAMEPAD) {
            i16 axisX = SDL_GetGamepadAxis(GAMEPAD, SDL_GAMEPAD_AXIS_LEFTX);
            i16 axisY = SDL_GetGamepadAxis(GAMEPAD, SDL_GAMEPAD_AXIS_LEFTY);
            if (SDL_abs(axisX) > GAMEPAD_LEFT_THUMB_DEADZONE) {
                xOffset += axisX >> 12;
            }
            if (SDL_abs(axisY) > GAMEPAD_LEFT_THUMB_DEADZONE) {
                yOffset += axisY >> 12;
            }
        }

        RenderWeirdGradient(&GLOBAL_BACKBUFFER, xOffset, yOffset);

        if (!SDL_RenderClear(renderer)) {
            SDL_Log("Could not clear renderer: %s", SDL_GetError());
            break;
        }
        if (!SDL_RenderTexture(renderer, GLOBAL_BACKBUFFER.texture, 0, 0)) {
            SDL_Log("Could not copy texture to backbuffer: %s", SDL_GetError());
            break;
        }
        if (!SDL_RenderPresent(renderer)) {
            SDL_Log("Could not update the screen with new rendering: %s", SDL_GetError());
        }
    }

    // NOTE(orestis): We don't free on shutdown. We let the OS reclaim everything
    // at process exit instead of freeing stuff one by one.
    SDL_Quit();
    return 0;
}
