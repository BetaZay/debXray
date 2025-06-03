#include "Renderer.h"
#include <SDL2/SDL.h>

SDL_Window* createWindow(bool windowed) {
    if (windowed) {
        return SDL_CreateWindow("debXray (Windowed)", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                1280, 720, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    }
    return SDL_CreateWindow("debXray", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                            0, 0, SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_BORDERLESS);
}

SDL_Renderer* createRenderer(SDL_Window* window) {
    return SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
}

void renderSystemInfo(SDL_Renderer*, const SystemInfo&) {
    // No-op with ImGui now rendering
}