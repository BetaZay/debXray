#include "SystemInfo.h"
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <array>

static std::string runCommand(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    FILE* pipe = popen(cmd, "r");
    if (!pipe) return "N/A";
    while (fgets(buffer.data(), buffer.size(), pipe)) {
        result += buffer.data();
    }
    pclose(pipe);
    return result;
}

SystemInfo getSystemInfo() {
    SystemInfo info;
    info.cpu = runCommand("lscpu | grep 'Model name' | awk -F: '{print $2}'");
    info.gpu = runCommand("lspci | grep -i 'vga\\|3d'");
    info.ram = runCommand("free -h | grep Mem | awk '{print $2}'");
    info.resolution = runCommand("xdpyinfo | grep dimensions | awk '{print $2}'");
    std::istringstream ss(runCommand("lspci"));
    std::string line;
    while (std::getline(ss, line)) {
        if (!line.empty()) info.pciDevices.push_back(line);
    }
    return info;
}