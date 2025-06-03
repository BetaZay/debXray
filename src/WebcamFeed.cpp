#include "WebcamFeed.h"
#include "Log.h"
#include <opencv2/opencv.hpp>

class WebcamFeed::Impl {
public:
    Impl(SDL_Renderer* renderer)
    : renderer(renderer), texture(nullptr), failed(true) {
        for (int i = 0; i < 5; ++i) {
            cap.open(i);
            if (cap.isOpened()) {
                logMessage("[+] Webcam opened: /dev/video" + std::to_string(i));
                cap.set(cv::CAP_PROP_FRAME_WIDTH, 320);
                cap.set(cv::CAP_PROP_FRAME_HEIGHT, 240);
                failed = false;
                break;
            }
        }

        if (failed) {
            logMessage("[-] Failed to open any webcam.");
        }
    }


    ~Impl() {
        if (texture) SDL_DestroyTexture(texture);
    }

    void update() {
        if (failed) return;

        cv::Mat frame;
        if (!cap.read(frame)) {
            logMessage("[-] Webcam capture failed — no frame returned.");
            failed = true;
            return;
        }

        cv::cvtColor(frame, frame, cv::COLOR_BGR2RGBA);

        if (!texture || frame.cols != lastWidth || frame.rows != lastHeight) {
            if (texture) SDL_DestroyTexture(texture);
            lastWidth = frame.cols;
            lastHeight = frame.rows;
            texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                                        SDL_TEXTUREACCESS_STREAMING,
                                        frame.cols, frame.rows);
        }

        SDL_UpdateTexture(texture, nullptr, frame.data, frame.cols * 4);
    }

    SDL_Texture* getTexture() const {
        return failed ? nullptr : texture;
    }

    bool isFailed() const {
        return failed;
    }

private:
    SDL_Renderer* renderer;
    cv::VideoCapture cap;
    SDL_Texture* texture;
    int lastWidth = 0, lastHeight = 0;
    bool failed;
};


WebcamFeed::WebcamFeed(SDL_Renderer* r) : impl(new Impl(r)) {}
WebcamFeed::~WebcamFeed() { delete impl; }
void WebcamFeed::update() { impl->update(); }
SDL_Texture* WebcamFeed::getTexture() const { return impl->getTexture(); }
bool WebcamFeed::isFailed() const { return impl->isFailed(); }