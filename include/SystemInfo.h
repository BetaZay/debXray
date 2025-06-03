#pragma once
#include <vector>
#include <string>

struct SystemInfo {
    std::string cpu;
    std::string gpu;
    std::string ram;
    std::string resolution;
    std::vector<std::string> pciDevices;
};

SystemInfo getSystemInfo();