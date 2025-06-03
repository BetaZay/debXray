// ----------------------------------------
// SystemInfo.cpp
// ----------------------------------------
#include "SystemInfo.h"
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <array>
#include <cmath>
#include <algorithm>
#include <unordered_set>
#include <filesystem>
#include <Log.h>       // for logMessage(...)
#include <regex>

// Helper: run a shell pipeline and capture stdout (trimmed)
static std::string runCommand(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    FILE* pipe = popen(cmd, "r");
    if (!pipe) return "";
    while (fgets(buffer.data(), buffer.size(), pipe)) {
        result += buffer.data();
    }
    pclose(pipe);
    // Trim trailing whitespace/newlines:
    if (!result.empty()) {
        result.erase(result.find_last_not_of(" \n\r\t") + 1);
    }
    return result;
}

bool isAppleOrSurface(const std::string& model) {
    std::string lower = model;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    return lower.find("mac") != std::string::npos
        || lower.find("apple") != std::string::npos
        || lower.find("surface") != std::string::npos;
}

SystemInfo getSystemInfo() {
    SystemInfo info;

    // We’ll reuse this variable in multiple loops below:
    std::string line;

    // ── 1) Determine form factor
    std::string chassis = runCommand("sudo dmidecode -s chassis-type 2>/dev/null");
    std::string lowerChassis = chassis;
    std::transform(lowerChassis.begin(), lowerChassis.end(), lowerChassis.begin(), ::tolower);
    if (lowerChassis.find("laptop")   != std::string::npos ||
        lowerChassis.find("notebook") != std::string::npos ||
        lowerChassis.find("portable") != std::string::npos ||
        lowerChassis.find("tablet")   != std::string::npos)
    {
        info.isLaptop = true;
    }

    // ── 2) Detect storage‐bus types
    std::unordered_set<std::string> supported;
    {
        std::istringstream lspciLines(runCommand(
            "lspci | grep -iE 'SATA|SAS|NVMe|RAID|IDE|SCSI|AHCI|controller'"
        ));
        while (std::getline(lspciLines, line)) {
            if (line.find("SATA")  != std::string::npos || line.find("AHCI") != std::string::npos)
                supported.insert("SATA");
            if (line.find("SAS")   != std::string::npos) supported.insert("SAS");
            if (line.find("NVMe")  != std::string::npos) supported.insert("NVMe");
            if (line.find("IDE")   != std::string::npos) supported.insert("IDE");
            if (line.find("SCSI")  != std::string::npos) supported.insert("SCSI");
            if (line.find("RAID")  != std::string::npos) supported.insert("RAID");
        }
        if (std::filesystem::exists("/sys/class/nvme")) {
            supported.insert("NVMe");
        }

        std::istringstream lsblkLines(runCommand("lsblk -dno TRAN 2>/dev/null | sort -u"));
        while (std::getline(lsblkLines, line)) {
            if (line.find("sata") != std::string::npos) supported.insert("SATA");
            if (line.find("sas")  != std::string::npos) supported.insert("SAS");
            if (line.find("nvme") != std::string::npos) supported.insert("NVMe");
            if (line.find("usb")  != std::string::npos) supported.insert("USB");
            if (line.find("ide")  != std::string::npos) supported.insert("IDE");
            if (line.find("scsi") != std::string::npos) supported.insert("SCSI");
            if (line.find("spi")  != std::string::npos) supported.insert("SPI");
        }
    }
    info.storageTypes.assign(supported.begin(), supported.end());
    std::sort(info.storageTypes.begin(), info.storageTypes.end());

    // ── 3) Host Model & Serial
    info.model  = runCommand("hostnamectl | grep 'Hardware Model' | awk -F': ' '{print $2}'");
    info.serial = runCommand("sudo dmidecode -s system-serial-number 2>/dev/null");
    if (info.serial.empty()) info.serial = "Unavailable";

    // ── 4) CPU brand, model, speed, physical sockets
{
    std::string modelLine = runCommand("lscpu | awk -F: '/Model name/ {print $2}'");
    modelLine.erase(0, modelLine.find_first_not_of(" \t\n\r"));
    info.cpuModel = "Unknown";
    info.cpuBrand = "Unknown";
    info.cpuSpeed = "Unknown";

    if (!modelLine.empty()) {
        // 1. Extract speed
        std::smatch speedMatch;
        std::regex speedRegex(R"(@\s*([0-9.]+)GHz)");
        if (std::regex_search(modelLine, speedMatch, speedRegex)) {
            info.cpuSpeed = speedMatch[1].str(); // "2.90"
            modelLine = modelLine.substr(0, speedMatch.position());
            modelLine.erase(modelLine.find_last_not_of(" \t\n\r") + 1);
        }

        // 2. Extract model number (e.g., 10700, 5800X)
        std::smatch modelMatch;
        std::regex modelRegex(R"((\d{4,5}[A-Za-z]{0,2}))");
        if (std::regex_search(modelLine, modelMatch, modelRegex)) {
            std::string fullModel = modelMatch[1].str();  // e.g., "i7-10700"
            size_t dash = fullModel.find('-');
            if (dash != std::string::npos && dash + 1 < fullModel.size()) {
                info.cpuModel = fullModel.substr(dash + 1); // keeps only "10700"
            } else {
                info.cpuModel = fullModel;
            }

            // 3. Extract brand by trimming before model
            if (modelMatch.position() > 0) {
                std::string brand = modelLine.substr(0, modelMatch.position());

                // Cleanup: remove (R), (TM), "CPU", "Processor", etc.
                brand = std::regex_replace(brand, std::regex(R"(\(.*?\)|CPU|Processor)", std::regex::icase), "");
                brand.erase(std::unique(brand.begin(), brand.end(),
                    [](char a, char b) { return std::isspace(a) && std::isspace(b); }), brand.end());
                brand.erase(brand.find_last_not_of(" \t\n\r") + 1);

                if (!brand.empty() && brand.back() == '-') {
                    brand.pop_back();
                }

                info.cpuBrand = brand;
            }
        } else {
            info.cpuModel = modelLine;
            info.cpuBrand = "Unknown";
        }
    }

    // 4. Socket count
    std::string sockets = runCommand("lscpu | grep 'Socket(s):' | awk '{print $2}'");
    sockets.erase(sockets.find_last_not_of(" \n\r\t") + 1);
    info.physicalCPUs = sockets.empty() ? "Unknown" : sockets;
}

    // ── 5) GPU Info
    info.gpu = runCommand("screenfetch -nN | grep 'GPU:' | head -n1 | awk -F': ' '{print $2}'");
    if (info.gpu.empty()) {
        // fallback if screenfetch not installed
        info.gpu = runCommand("lspci | grep -i 'VGA' | head -n1 | awk -F': ' '{print $3}'");
    }

    // ── 6) RAM Info (rounded to nearest even >8GB, or nearest int ≤8GB)
    {
        std::string memKbStr = runCommand("grep MemTotal /proc/meminfo | awk '{print $2}'");
        long memKb = memKbStr.empty() ? 0 : std::stol(memKbStr);
        int memGb = 0;
        if (memKb > 0) {
            double rawGb = memKb / 1048576.0;
            if (rawGb > 8.0) {
                int half = static_cast<int>(std::ceil(rawGb / 2.0));
                memGb = half * 2;
            } else {
                memGb = static_cast<int>(std::round(rawGb));
            }
        }
        info.ram = std::to_string(memGb) + " GB";

        // 6b) memoryType via dmidecode
        std::string memType = runCommand(
            "sudo dmidecode -t memory | grep 'Type:' | grep -v 'Unknown\\|Error' | sort | uniq | awk '{print $2}'"
        );
        memType.erase(memType.find_last_not_of(" \n\r\t") + 1);
        info.memoryType = memType.empty() ? "Unknown" : memType;
    }

    // ── 7) Screen resolution + physical size
    info.resolution = runCommand("xdpyinfo | grep dimensions | awk '{print $2}'");
    if (info.resolution.empty()) {
        info.resolution = "Unknown";
    }

    {
        std::string edidOutput = runCommand(
            "xrandr --verbose | grep -m1 -A5 ' connected' | grep -Eo '[0-9]+mm x [0-9]+mm'"
        );
        int widthMM = 0, heightMM = 0;
        if (!edidOutput.empty() &&
            sscanf(edidOutput.c_str(), "%dmm x %dmm", &widthMM, &heightMM) == 2 &&
            widthMM > 0 && heightMM > 0)
        {
            double wIn = widthMM / 25.4;
            double hIn = heightMM / 25.4;
            double diag = std::sqrt(wIn*wIn + hIn*hIn);
            std::ostringstream oss;
            oss.precision(1);
            oss << std::fixed << diag << "\"";
            info.screenSize = oss.str();
        } else {
            info.screenSize = "Unknown";
        }
    }

    // ── 8) Battery condition
    {
        std::string batteryPath = runCommand("upower -e | grep BAT");
        if (!batteryPath.empty()) {
            batteryPath.erase(batteryPath.find_last_not_of(" \n\r\t") + 1);
            std::string full   = runCommand(
                ("upower -i " + batteryPath + " | grep 'energy-full:' | awk '{print $2}'").c_str()
            );
            std::string design = runCommand(
                ("upower -i " + batteryPath + " | grep 'energy-full-design:' | awk '{print $2}'").c_str()
            );
            if (!full.empty() && !design.empty() && design != "0") {
                float health = std::stof(full) / std::stof(design) * 100.0f;
                if (health >= 90.0f)      info.battery = "Excellent";
                else if (health >= 70.0f) info.battery = "Good";
                else                      info.battery = "Poor";
            } else {
                info.battery = "N/A";
            }
        } else {
            info.battery = "None";
        }
    }

    // ── 9) PCI Devices (filtered)
    {
        std::istringstream pciSs(runCommand(
            "lspci -mm | grep -iv -E 'bridge|host|system peripheral|processing controller|usb|communication|memory controller|cavs' | "
            "grep -i -E 'VGA|Network|Ethernet|Audio|Non-Volatile' | awk -F'\"' '{print $6}' | sort -u"
        ));
        while (std::getline(pciSs, line)) {
            if (!line.empty()) info.pciDevices.push_back(line);
        }
    }

    // ── 10) Detected drives (flag non‐USB, log individually)
    {
        bool nonUsb = false;
        std::istringstream driveLines(runCommand(
            "lsblk -dn -o NAME,TRAN,ROTA,MODEL | awk '$1 !~ /^(loop|sr)/ && $2 != \"\"'"
        ));
        while (std::getline(driveLines, line)) {
            std::istringstream ls(line);
            std::string name, tran, rota, model;
            if (!(ls >> name >> tran >> rota)) continue;
            std::getline(ls, model);
            model = model.empty() ? "Unknown" : model.substr(model.find_first_not_of(" \t"));
            std::string type;
            if (name.rfind("nvme", 0) == 0)    type = "NVMe";
            else if (rota == "0")             type = "SSD";
            else if (rota == "1")             type = "HDD";
            else                               type = "Unknown";

            DriveInfo d{ "/dev/" + name, type, tran, model };
            info.detectedDrives.push_back(d);

            std::string msg = "[*] Drive detected: " + d.name +
                              " (Protocol: " + tran +
                              ", Type: " + type +
                              ", Model: " + model + ")";
            logMessage(msg);

            if (tran != "usb") nonUsb = true;
        }
        info.hasNonUsbDrives = nonUsb;
        if (nonUsb) logMessage("[-] Warning: One or more non-USB drives detected.");
        else        logMessage("[+] No non-USB drives detected.");
    }

    return info;
}
