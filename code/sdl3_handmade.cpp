#include "SDL3/SDL_events.h"
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_messagebox.h>
#include <SDL3/SDL_video.h>

int main() {
    // SDL_Log and SDL_ShowSimpleMessageBox work
    // without initializing the video subsystem, and can work without
    // initialization
    bool ok = SDL_InitSubSystem(SDL_INIT_VIDEO);
    if (!ok) {
        SDL_Log("Could not initialize video subsystem: %s", SDL_GetError());
        ok = SDL_ShowSimpleMessageBox(
                SDL_MESSAGEBOX_ERROR,
                "Handmade Hero",
                "Fatal application error: Could not initialize video subsystem",
                nullptr);
        if (!ok) {
            SDL_Log("Could not show message box: %s", SDL_GetError());
        }
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("Handmade Hero", 1280, 720, SDL_WINDOW_RESIZABLE);
    if (window == nullptr) {
        SDL_Log("Could not create window: %s", SDL_GetError());
        return 1;
    }

    bool game_is_running = true;

    while (game_is_running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_EVENT_QUIT: {
                    game_is_running = false;
                    break;
                }
                case SDL_EVENT_WINDOW_RESIZED: {
                    SDL_Log("Window resized: %d x %d", event.window.data1, event.window.data2);
                    break;
                }
                default: {
                    SDL_Log("%d", event.type);
                    break;
                }
            }
        }
    }

    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
