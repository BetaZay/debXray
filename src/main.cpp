// debXray main.cpp (fully updated)

#include "SystemInfo.h"
#include "DependencyManager.h"
#include "Renderer.h"
#include "Log.h"
#include "WebcamFeed.h"
#include "json.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <imgui.h>
#include <backends/imgui_impl_sdl2.h>
#include <backends/imgui_impl_sdlrenderer2.h>
#include <unordered_set>
#include <string>
#include <array>
#include <curl/curl.h>

using json = nlohmann::json;
static std::unordered_set<SDL_Scancode> pressedScancodes;

json toJson(const SystemInfo &info)
{
    return {
        {"host_model", info.model},
        {"serial", info.serial},
        {"resolution", info.resolution},

        // CPU Info
        {"processor_brand", info.cpuBrand},
        {"cpu_model", info.cpuModel},
        {"cpu_speed", info.cpuSpeed},
        {"physical_cpus", info.physicalCPUs},

        // GPU
        {"gpu", info.gpu},

        // Memory
        {"memory_size", info.ram},
        {"memory_type", info.memoryType},

        // Battery condition as object with id
        {"battery_condition", {{"id", info.battery == "Excellent" ? 1 : info.battery == "Good" ? 2
                                                                    : info.battery == "Poor"   ? 3
                                                                    : info.battery == "None"   ? 4
                                                                                               : 0},
                               {"name", info.battery}}},

        // PCI devices and storage buses
        {"pci_devices", info.pciDevices},
        {"storage_types", info.storageTypes}};
}

void showBlockingWarning(const char *title, const char *message)
{
    SDL_Window *win = SDL_CreateWindow(
        title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        600, 300, SDL_WINDOW_SHOWN);
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    SDL_SetRenderDrawColor(ren, 30, 30, 30, 255);

    TTF_Init();
    TTF_Font *font = TTF_OpenFont(
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 18);
    SDL_Color textColor = {255, 255, 255, 255};

    SDL_Surface *surface = TTF_RenderText_Blended_Wrapped(
        font, message, textColor, 580);
    SDL_Texture *texture = SDL_CreateTextureFromSurface(ren, surface);

    SDL_Event e;
    bool quit = false;
    while (!quit)
    {
        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_QUIT ||
                (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_RETURN))
            {
                quit = true;
            }
        }
        SDL_RenderClear(ren);
        int w, h;
        SDL_QueryTexture(texture, NULL, NULL, &w, &h);
        SDL_Rect dst = {10, 100, w, h};
        SDL_RenderCopy(ren, texture, NULL, &dst);
        SDL_RenderPresent(ren);
    }

    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);
    TTF_CloseFont(font);
    TTF_Quit();
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
}

bool uploadSpecs(const json &payload)
{
    constexpr char URL[] = "https://cfk-sds.com/api/techline-upload";
    constexpr char KEY[] = "secureErase04!";

    CURL *c = curl_easy_init();
    if (!c)
        return false;

    struct curl_slist *hdrs = nullptr;
    hdrs = curl_slist_append(hdrs, "Content-Type: application/json");
    hdrs = curl_slist_append(
        hdrs, ("X-API-KEY: " + std::string(KEY)).c_str());

    curl_easy_setopt(c, CURLOPT_URL, URL);
    curl_easy_setopt(c, CURLOPT_HTTPHEADER, hdrs);
    std::string payloadStr = payload.dump();
    curl_easy_setopt(c, CURLOPT_POSTFIELDS, payloadStr.c_str());

    std::string resp;
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, +[](char *ptr, size_t sz, size_t nm, void *userdata) -> size_t
                     {
            auto* s = static_cast<std::string*>(userdata);
            s->append(ptr, sz * nm);
            return sz * nm; });
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &resp);

    CURLcode rc = curl_easy_perform(c);
    curl_slist_free_all(hdrs);
    curl_easy_cleanup(c);

    if (rc != CURLE_OK)
    {
        logMessage("Upload failed: " + std::string(curl_easy_strerror(rc)));
        return false;
    }

    // server returns {"status":"success|error","message":"…"}
    json r = json::parse(resp, nullptr, false);
    std::string status = r.value("status", "error");
    std::string message = r.value("message", "no message");
    logMessage("[upload] " + status + ": " + message);

    return status == "success";
}

int main(int argc, char *argv[])
{
    bool windowed = false;
    bool showModal = false;
    std::string modalTitle;
    std::string modalMessage;

    for (int i = 1; i < argc; ++i)
    {
        if (std::string(argv[i]) == "--windowed")
            windowed = true;
    }

    // ── Clear previous log ──
    clearLog();

    if (isOnline())
        installDependencies();
    else
        logMessage("[-] No network connection.");

    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        logMessage("SDL_Init failed: " + std::string(SDL_GetError()));
        return 1;
    }

    SDL_Window *window = createWindow(windowed);
    SDL_Renderer *renderer = createRenderer(window);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);

    WebcamFeed webcam(renderer);
    SystemInfo info = getSystemInfo();

    // if any non-USB drive detected (and not Apple/Surface) → block
    if (info.hasNonUsbDrives && !isAppleOrSurface(info.model))
    {
        logMessage("[-] Internal drive(s) detected.");
        modalTitle = "Non-USB Drive Detected";
        modalMessage =
            "This system has internal (non-USB) drives connected.\n\n"
            "Please shut down and remove them.";
        showModal = true;
    }

    SDL_Event e;
    bool running = true;
    while (running)
    {
        while (SDL_PollEvent(&e))
        {
            ImGui_ImplSDL2_ProcessEvent(&e);
            if (e.type == SDL_QUIT ||
                (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE))
            {
                running = false;
            }
        }

        // ── Update key states ──
        const Uint8 *keys = SDL_GetKeyboardState(nullptr);
        for (int sc = SDL_SCANCODE_A; sc <= SDL_SCANCODE_Z; ++sc)
            if (keys[sc])
                pressedScancodes.insert((SDL_Scancode)sc);
        for (int sc = SDL_SCANCODE_1; sc <= SDL_SCANCODE_0; ++sc)
            if (keys[sc])
                pressedScancodes.insert((SDL_Scancode)sc);
        SDL_Scancode extras[] = {
            SDL_SCANCODE_MINUS, SDL_SCANCODE_EQUALS,
            SDL_SCANCODE_LEFTBRACKET, SDL_SCANCODE_RIGHTBRACKET,
            SDL_SCANCODE_BACKSLASH, SDL_SCANCODE_SEMICOLON,
            SDL_SCANCODE_APOSTROPHE, SDL_SCANCODE_COMMA,
            SDL_SCANCODE_PERIOD, SDL_SCANCODE_SLASH};
        for (SDL_Scancode sc : extras)
            if (keys[sc])
                pressedScancodes.insert(sc);

        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        // ── Main UI Window (fullscreen) ──
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin(
            "debXray Main", nullptr,
            ImGuiWindowFlags_NoDecoration |
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoTitleBar |
                ImGuiWindowFlags_MenuBar);

        // ── Menu Bar ──
        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("System"))
            {
                if (ImGui::MenuItem("Upload Specs"))
                {
                    if (uploadSpecs(toJson(info)))
                    {
                        logMessage("[+] Specs uploaded manually.");
                    }
                    else
                    {
                        showBlockingWarning("Upload Failed", "Specs upload failed — please try again or contact support.");
                    }
                }

                if (ImGui::MenuItem("Shutdown"))
                {
                    logMessage("[*] Shutting down via menu...");
                    system("sudo shutdown -h now");
                }

                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        ImVec2 avail = ImGui::GetContentRegionAvail();
        float halfWidth = avail.x * 0.5f;
        float halfHeight = avail.y * 0.5f;

        //
        // ─── ROW 1 ───
        //

        ImGui::BeginGroup(); // ── Top-Left: System Info ──
        ImGui::BeginChild("SystemInfoBox", ImVec2(halfWidth, halfHeight), true);
        ImGui::Text("System Info");
        ImGui::Separator();
        ImGui::Text("Form Factor: %s", info.isLaptop ? "Laptop" : "Desktop");
        ImGui::Text("Model: %s", info.model.c_str());
        ImGui::Text("Serial: %s", info.serial.c_str());
        ImGui::Text("CPU: %s - %s @ %sGHz",
                    info.cpuBrand.c_str(),
                    info.cpuModel.c_str(),
                    info.cpuSpeed.c_str());
        ImGui::Text("GPU: %s", info.gpu.c_str());
        ImGui::Text("RAM: %s %s",
                    info.ram.c_str(),
                    info.memoryType.c_str());
        ImGui::Text("Screen: %s", info.resolution.c_str());
        ImGui::SameLine();
        ImGui::Text("(%s)", info.screenSize.c_str());
        ImGui::Text("Battery: %s", info.battery.c_str());

        if (!info.pciDevices.empty())
        {
            ImGui::Separator();
            ImGui::Text("PCI Devices:");
            for (const auto &device : info.pciDevices)
            {
                ImGui::BulletText("%s", device.c_str());
            }
        }

        if (!info.storageTypes.empty())
        {
            ImGui::Separator();
            ImGui::Text("Drive Support:");
            for (const auto &type : info.storageTypes)
                ImGui::BulletText("%s", type.c_str());
        }

        if (!info.detectedDrives.empty())
        {
            ImGui::Separator();
            ImGui::Text("Drives:");
            for (const auto &d : info.detectedDrives)
            {
                ImGui::BulletText(
                    "%s (%s, %s, %s)",
                    d.name.c_str(),
                    d.tran.c_str(),
                    d.type.c_str(),
                    d.model.c_str());
            }
        }

        ImGui::EndChild();
        ImGui::EndGroup();

        ImGui::SameLine();

        ImGui::BeginGroup(); // ── Top-Right: Webcam ──
        ImGui::BeginChild("WebcamBox", ImVec2(halfWidth, halfHeight), true);
        ImGui::Text("Webcam Preview");
        webcam.update();

        if (webcam.isFailed())
        {
            ImGui::Spacing();
            ImGui::TextColored(
                ImVec4(0.8f, 0.1f, 0.1f, 1.0f),
                "No webcam detected.");
        }
        else
        {
            SDL_Texture *tex = webcam.getTexture();
            if (tex)
            {
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
        ImGui::BeginChild("LogViewerBox", ImVec2(halfWidth, halfHeight - 5), true);
        ImGui::Text("Logs");
        ImGui::Separator();
        ImGui::BeginChild(
            "LogScroll", ImVec2(0, 0), false,
            ImGuiWindowFlags_AlwaysVerticalScrollbar);
        auto logs = readLogTail(100);
        for (const auto &line : logs)
        {
            ImGui::TextUnformatted(line.c_str());
        }
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 5.0f)
            ImGui::SetScrollHereY(1.0f);
        ImGui::EndChild();
        ImGui::EndChild();
        ImGui::EndGroup();

        ImGui::SameLine();

        ImGui::BeginGroup(); // ── Bottom-Right: Keyboard Tester ──
        ImGui::BeginChild("KeyboardBox", ImVec2(halfWidth, halfHeight - 5), true);
        ImGui::Text("Keyboard Test");
        ImGui::Separator();

        // Button layout (same as before)
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        const float totalWidth = ImGui::GetContentRegionAvail().x;
        const int maxKeysInRow = 13;
        float btnSize = (totalWidth - spacing * (maxKeysInRow - 1)) / maxKeysInRow;
        ImVec2 size(btnSize, btnSize);

        auto drawRow = [&](const char *chars, const SDL_Scancode *scancodes, int count)
        {
            float rowWidth = count * size.x + (count - 1) * spacing;
            float offsetX = (ImGui::GetContentRegionAvail().x - rowWidth) * 0.5f;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);

            for (int i = 0; i < count; ++i)
            {
                if (pressedScancodes.count(scancodes[i]))
                    ImGui::PushStyleColor(
                        ImGuiCol_Button,
                        ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
                ImGui::Button(std::string(1, chars[i]).c_str(), size);
                if (pressedScancodes.count(scancodes[i]))
                    ImGui::PopStyleColor();
                if (i < count - 1)
                    ImGui::SameLine();
            }
            ImGui::NewLine();
        };

        // Define rows
        const char *row1 = "1234567890-=";
        SDL_Scancode row1Sc[] = {
            SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_3, SDL_SCANCODE_4,
            SDL_SCANCODE_5, SDL_SCANCODE_6, SDL_SCANCODE_7, SDL_SCANCODE_8,
            SDL_SCANCODE_9, SDL_SCANCODE_0, SDL_SCANCODE_MINUS, SDL_SCANCODE_EQUALS};
        const char *row2 = "QWERTYUIOP[]\\";
        SDL_Scancode row2Sc[] = {
            SDL_SCANCODE_Q, SDL_SCANCODE_W, SDL_SCANCODE_E, SDL_SCANCODE_R,
            SDL_SCANCODE_T, SDL_SCANCODE_Y, SDL_SCANCODE_U, SDL_SCANCODE_I,
            SDL_SCANCODE_O, SDL_SCANCODE_P, SDL_SCANCODE_LEFTBRACKET,
            SDL_SCANCODE_RIGHTBRACKET, SDL_SCANCODE_BACKSLASH};
        const char *row3 = "ASDFGHJKL;'";
        SDL_Scancode row3Sc[] = {
            SDL_SCANCODE_A, SDL_SCANCODE_S, SDL_SCANCODE_D, SDL_SCANCODE_F,
            SDL_SCANCODE_G, SDL_SCANCODE_H, SDL_SCANCODE_J, SDL_SCANCODE_K,
            SDL_SCANCODE_L, SDL_SCANCODE_SEMICOLON, SDL_SCANCODE_APOSTROPHE};
        const char *row4 = "ZXCVBNM,./";
        SDL_Scancode row4Sc[] = {
            SDL_SCANCODE_Z, SDL_SCANCODE_X, SDL_SCANCODE_C, SDL_SCANCODE_V,
            SDL_SCANCODE_B, SDL_SCANCODE_N, SDL_SCANCODE_M, SDL_SCANCODE_COMMA,
            SDL_SCANCODE_PERIOD, SDL_SCANCODE_SLASH};

        // Draw rows
        drawRow(row1, row1Sc, IM_ARRAYSIZE(row1Sc));
        drawRow(row2, row2Sc, IM_ARRAYSIZE(row2Sc));
        drawRow(row3, row3Sc, IM_ARRAYSIZE(row3Sc));
        drawRow(row4, row4Sc, IM_ARRAYSIZE(row4Sc));

        static std::unordered_set<SDL_Scancode> allKeysRequired;
        if (allKeysRequired.empty())
        {
            allKeysRequired.insert(std::begin(row1Sc), std::end(row1Sc));
            allKeysRequired.insert(std::begin(row2Sc), std::end(row2Sc));
            allKeysRequired.insert(std::begin(row3Sc), std::end(row3Sc));
            allKeysRequired.insert(std::begin(row4Sc), std::end(row4Sc));
        }
        bool allPressed = true;
        for (SDL_Scancode sc : allKeysRequired)
        {
            if (!pressedScancodes.count(sc))
            {
                allPressed = false;
                break;
            }
        }
        if (allPressed)
        {
            ImGui::Spacing();
            ImGui::TextColored(
                ImVec4(0.1f, 0.8f, 0.1f, 1.0f),
                "All keys have been tested!");
        }

        ImGui::EndChild();
        ImGui::EndGroup();
        ImGui::End(); // End of Main Window

        //
        // ── Blocking Modal (if needed) ──
        //
        if (showModal)
        {
            // Process events for the modal only
            while (SDL_PollEvent(&e))
            {
                ImGui_ImplSDL2_ProcessEvent(&e);

                if (e.type == SDL_QUIT)
                {
                    running = false;
                }
                else if (e.type == SDL_KEYDOWN)
                {
                    showModal = false; // Hide modal
                    // If "No Drives" modal, upload specs now:
                    if (info.detectedDrives.empty())
                    {
                        if (uploadSpecs(toJson(info)))
                        {
                            logMessage("[+] Specs uploaded successfully.");
                        }
                        else
                        {
                            showBlockingWarning(
                                "Upload Failed",
                                "Specs upload failed — please try again or contact support.");
                        }
                    }
                }
            }

            ImGui::SetNextWindowPos(ImVec2(0, 0));
            ImGui::SetNextWindowSize(io.DisplaySize);
            ImGui::Begin(
                "BlockingModal", nullptr,
                ImGuiWindowFlags_NoDecoration |
                    ImGuiWindowFlags_NoMove |
                    ImGuiWindowFlags_NoResize |
                    ImGuiWindowFlags_NoCollapse |
                    ImGuiWindowFlags_NoTitleBar);

            ImVec2 center = ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
            ImGui::SetCursorPos(ImVec2(center.x - 150, center.y - 50));

            ImGui::BeginGroup();
            ImGui::TextColored(
                ImVec4(1.0f, 0.2f, 0.2f, 1.0f),
                "%s", modalTitle.c_str());
            ImGui::Spacing();
            ImGui::TextWrapped("%s", modalMessage.c_str());
            ImGui::Spacing();
            ImGui::Text("Press Enter to continue");
            ImGui::EndGroup();
            ImGui::End();
        }

        // ── Render Normal UI ──
        ImGui::Render();
        SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
        SDL_RenderClear(renderer);
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);
    }

    // ── Cleanup ──
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
