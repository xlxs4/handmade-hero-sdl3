#include <SDL3/SDL_error.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_messagebox.h>

int main() {
    // TODO: Use SDL_ShowSimpleMessageBox to let the user know if
    // SDL_Init failed -- SDL_Log and SDL_ShowSimpleMessageBox work
    // without initializing the video subsystem, and can work without
    // initialization
    bool ok = SDL_ShowSimpleMessageBox(
        SDL_MESSAGEBOX_INFORMATION,
        "Handmade Hero",
        "This is Handmade Hero",
        nullptr);

    if (!ok) {
        SDL_Log("%s\n", SDL_GetError());
        return 1;
    }

    return 0;
}
