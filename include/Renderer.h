#pragma once
#include "SystemInfo.h"
#include <SDL2/SDL.h>

SDL_Window* createWindow(bool windowed);
SDL_Renderer* createRenderer(SDL_Window* window);
void renderSystemInfo(SDL_Renderer* renderer, const SystemInfo& info);