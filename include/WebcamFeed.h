#pragma once
#include <SDL2/SDL.h>

class WebcamFeed {
public:
    explicit WebcamFeed(SDL_Renderer* r);
    ~WebcamFeed();

    void update();
    SDL_Texture* getTexture() const;
    bool isFailed() const;

private:
    class Impl;
    Impl* impl;
};