#include "SystemInfo.h"
#include "DependencyManager.h"
#include "Renderer.h"
#include "Log.h"
#include "WebcamFeed.h"

#include <SDL2/SDL.h>
#include <imgui.h>
#include <backends/imgui_impl_sdl2.h>
#include <backends/imgui_impl_sdlrenderer2.h>
#include <unordered_set>
#include <string>
#include <array>

static std::unordered_set<SDL_Scancode> pressedScancodes;

int main(int argc, char* argv[]) {
    bool windowed = false;
    for (int i = 1; i < argc; ++i)
        if (std::string(argv[i]) == "--windowed") windowed = true;

    // ── Clear previous log ──
    clearLog();
    
    if (isOnline()) installDependencies();
    else logMessage("[-] No network connection.");

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        logMessage("SDL_Init failed: " + std::string(SDL_GetError()));
        return 1;
    }

    SDL_Window* window = createWindow(windowed);
    SDL_Renderer* renderer = createRenderer(window);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);

    WebcamFeed webcam(renderer);
    SystemInfo info = getSystemInfo();

    SDL_Event e;
    bool running = true;
    while (running) {
        while (SDL_PollEvent(&e)) {
            ImGui_ImplSDL2_ProcessEvent(&e);
            if (e.type == SDL_QUIT || (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE))
                running = false;
        }

        // ── Update key states ──
        const Uint8* keys = SDL_GetKeyboardState(nullptr);
        for (int sc = SDL_SCANCODE_A; sc <= SDL_SCANCODE_Z; ++sc)
            if (keys[sc]) pressedScancodes.insert((SDL_Scancode)sc);
        for (int sc = SDL_SCANCODE_1; sc <= SDL_SCANCODE_0; ++sc)
            if (keys[sc]) pressedScancodes.insert((SDL_Scancode)sc);
        SDL_Scancode extras[] = {
            SDL_SCANCODE_MINUS, SDL_SCANCODE_EQUALS,
            SDL_SCANCODE_LEFTBRACKET, SDL_SCANCODE_RIGHTBRACKET,
            SDL_SCANCODE_BACKSLASH, SDL_SCANCODE_SEMICOLON,
            SDL_SCANCODE_APOSTROPHE, SDL_SCANCODE_COMMA,
            SDL_SCANCODE_PERIOD, SDL_SCANCODE_SLASH
        };
        for (SDL_Scancode sc : extras)
            if (keys[sc]) pressedScancodes.insert(sc);

        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("debXray Main", nullptr,
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoTitleBar);

        ImVec2 avail = ImGui::GetContentRegionAvail();
float halfWidth  = avail.x * 0.5f;
float halfHeight = avail.y * 0.5f;

//
// ─── ROW 1 ───
//
ImGui::BeginGroup(); // ── Top-Left: System Info ──
ImGui::BeginChild("SystemInfoBox", ImVec2(halfWidth, halfHeight), true);
ImGui::Text("System Info");
ImGui::Separator();
ImGui::Text("CPU: %s", info.cpu.c_str());
ImGui::Text("GPU: %s", info.gpu.c_str());
ImGui::Text("RAM: %s", info.ram.c_str());
ImGui::Text("Resolution: %s", info.resolution.c_str());
ImGui::EndChild();
ImGui::EndGroup();

ImGui::SameLine();

ImGui::BeginGroup(); // ── Top-Right: Webcam ──
ImGui::BeginChild("WebcamBox", ImVec2(halfWidth, halfHeight), true);
ImGui::Text("Webcam Preview");
webcam.update();

if (webcam.isFailed()) {
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.8f, 0.1f, 0.1f, 1.0f), "No webcam detected.");
} else {
    SDL_Texture* tex = webcam.getTexture();
    if (tex) {
        ImVec2 size = ImGui::GetContentRegionAvail();
        ImGui::Image((ImTextureID)tex, size);
    }
}
ImGui::EndChild();
ImGui::EndGroup();

//
// ─── ROW 2 ───
//
ImGui::BeginGroup(); // ── Bottom-Left: Logs ──
ImGui::BeginChild("LogViewerBox", ImVec2(halfWidth, halfHeight), true);
ImGui::Text("Logs");
ImGui::Separator();
ImGui::BeginChild("LogScroll", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);
auto logs = readLogTail(100);
for (const auto& line : logs) {
    ImGui::TextUnformatted(line.c_str());
}
if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 5.0f)
    ImGui::SetScrollHereY(1.0f);
ImGui::EndChild();
ImGui::EndChild();
ImGui::EndGroup();

ImGui::SameLine();

ImGui::BeginGroup(); // ── Bottom-Right: Keyboard Tester ──
ImGui::BeginChild("KeyboardBox", ImVec2(halfWidth, halfHeight), true);
ImGui::Text("Keyboard Test");
ImGui::Separator();

// Button layout (same as you had before)
const float spacing = ImGui::GetStyle().ItemSpacing.x;
const float totalWidth = ImGui::GetContentRegionAvail().x;
const int maxKeysInRow = 13;
float btnSize = (totalWidth - spacing * (maxKeysInRow - 1)) / maxKeysInRow;
ImVec2 size(btnSize, btnSize);

auto drawRow = [&](const char* chars, const SDL_Scancode* scancodes, int count) {
    float rowWidth = count * size.x + (count - 1) * spacing;
    float offsetX = (ImGui::GetContentRegionAvail().x - rowWidth) * 0.5f;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);

    for (int i = 0; i < count; ++i) {
        if (pressedScancodes.count(scancodes[i]))
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
        ImGui::Button(std::string(1, chars[i]).c_str(), size);
        if (pressedScancodes.count(scancodes[i]))
            ImGui::PopStyleColor();
        if (i < count - 1)
            ImGui::SameLine();
    }
    ImGui::NewLine();
};

// Define rows
const char* row1 = "1234567890-=";
SDL_Scancode row1Sc[] = { SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_3, SDL_SCANCODE_4, SDL_SCANCODE_5, SDL_SCANCODE_6, SDL_SCANCODE_7, SDL_SCANCODE_8, SDL_SCANCODE_9, SDL_SCANCODE_0, SDL_SCANCODE_MINUS, SDL_SCANCODE_EQUALS };
const char* row2 = "QWERTYUIOP[]\\";
SDL_Scancode row2Sc[] = { SDL_SCANCODE_Q, SDL_SCANCODE_W, SDL_SCANCODE_E, SDL_SCANCODE_R, SDL_SCANCODE_T, SDL_SCANCODE_Y, SDL_SCANCODE_U, SDL_SCANCODE_I, SDL_SCANCODE_O, SDL_SCANCODE_P, SDL_SCANCODE_LEFTBRACKET, SDL_SCANCODE_RIGHTBRACKET, SDL_SCANCODE_BACKSLASH };
const char* row3 = "ASDFGHJKL;'";
SDL_Scancode row3Sc[] = { SDL_SCANCODE_A, SDL_SCANCODE_S, SDL_SCANCODE_D, SDL_SCANCODE_F, SDL_SCANCODE_G, SDL_SCANCODE_H, SDL_SCANCODE_J, SDL_SCANCODE_K, SDL_SCANCODE_L, SDL_SCANCODE_SEMICOLON, SDL_SCANCODE_APOSTROPHE };
const char* row4 = "ZXCVBNM,./";
SDL_Scancode row4Sc[] = { SDL_SCANCODE_Z, SDL_SCANCODE_X, SDL_SCANCODE_C, SDL_SCANCODE_V, SDL_SCANCODE_B, SDL_SCANCODE_N, SDL_SCANCODE_M, SDL_SCANCODE_COMMA, SDL_SCANCODE_PERIOD, SDL_SCANCODE_SLASH };

// Draw rows
drawRow(row1, row1Sc, IM_ARRAYSIZE(row1Sc));
drawRow(row2, row2Sc, IM_ARRAYSIZE(row2Sc));
drawRow(row3, row3Sc, IM_ARRAYSIZE(row3Sc));
drawRow(row4, row4Sc, IM_ARRAYSIZE(row4Sc));

static std::unordered_set<SDL_Scancode> allKeysRequired;
if (allKeysRequired.empty()) {
    allKeysRequired.insert(std::begin(row1Sc), std::end(row1Sc));
    allKeysRequired.insert(std::begin(row2Sc), std::end(row2Sc));
    allKeysRequired.insert(std::begin(row3Sc), std::end(row3Sc));
    allKeysRequired.insert(std::begin(row4Sc), std::end(row4Sc));
}
bool allPressed = true;
for (SDL_Scancode sc : allKeysRequired)
    if (!pressedScancodes.count(sc)) {
        allPressed = false;
        break;
    }
if (allPressed) {
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.1f, 0.8f, 0.1f, 1.0f), "All keys have been tested!");
}

ImGui::EndChild();
ImGui::EndGroup();
        ImGui::End();      // Main

        ImGui::Render();
        SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
        SDL_RenderClear(renderer);
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);
    }

    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
