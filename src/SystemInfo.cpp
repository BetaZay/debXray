#include "SystemInfo.h"
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <array>
#include <cmath>
#include <ranges>
#include <algorithm> 
#include <unordered_set>
#include <filesystem>
#include <Log.h>

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

bool isAppleOrSurface(const std::string& model) {
    std::string lower = model;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    return lower.find("mac") != std::string::npos ||
           lower.find("apple") != std::string::npos ||
           lower.find("surface") != std::string::npos;
}

SystemInfo getSystemInfo() {
    SystemInfo info;

    std::string chassis = runCommand("sudo dmidecode -s chassis-type 2>/dev/null");
    chassis.erase(chassis.find_last_not_of(" \n\r\t") + 1); // trim
    std::string lowerChassis = chassis;
    std::transform(lowerChassis.begin(), lowerChassis.end(), lowerChassis.begin(), ::tolower);

    if (lowerChassis.find("laptop") != std::string::npos ||
        lowerChassis.find("notebook") != std::string::npos ||
        lowerChassis.find("portable") != std::string::npos ||
        lowerChassis.find("tablet") != std::string::npos) {
        info.isLaptop = true;
    } else {
        info.isLaptop = false;
    }

    // ── Drive Type Detection ──
    std::unordered_set<std::string> supported;

    std::istringstream lspciLines(runCommand("lspci | grep -iE 'SATA|SAS|NVMe|RAID|IDE|SCSI|AHCI|controller'"));
    std::string line;
    while (std::getline(lspciLines, line)) {
        if (line.find("SATA") != std::string::npos || line.find("AHCI") != std::string::npos) supported.insert("SATA");
        if (line.find("SAS")  != std::string::npos) supported.insert("SAS");
        if (line.find("NVMe") != std::string::npos) supported.insert("NVMe");
        if (line.find("IDE")  != std::string::npos) supported.insert("IDE");
        if (line.find("SCSI") != std::string::npos) supported.insert("SCSI");
        if (line.find("RAID") != std::string::npos) supported.insert("RAID");
    }

    if (std::filesystem::exists("/sys/class/nvme")) {
        supported.insert("NVMe");
    }

    std::istringstream lsblkLines(runCommand("lsblk -dno TRAN 2>/dev/null | sort -u"));
    while (std::getline(lsblkLines, line)) {
        if (line.find("sata") != std::string::npos) supported.insert("SATA");
        if (line.find("sas") != std::string::npos) supported.insert("SAS");
        if (line.find("nvme") != std::string::npos) supported.insert("NVMe");
        if (line.find("usb") != std::string::npos) supported.insert("USB");
        if (line.find("ide") != std::string::npos) supported.insert("IDE");
        if (line.find("scsi") != std::string::npos) supported.insert("SCSI");
        if (line.find("spi") != std::string::npos) supported.insert("SPI");
    }

    // Copy into info.storageTypes
    info.storageTypes.assign(supported.begin(), supported.end());
    std::sort(info.storageTypes.begin(), info.storageTypes.end());

    // Host Model
    info.model = runCommand("hostnamectl | grep 'Hardware Model' | awk -F': ' '{print $2}'");

    // Serial Number
    info.serial = runCommand("sudo dmidecode -s system-serial-number 2>/dev/null");
    if (info.serial.empty()) info.serial = "Unavailable";

    // CPU Info
    std::string cpuLine = runCommand("screenfetch -nN | grep 'CPU:' | head -n1 | awk -F': ' '{print $2}'");
    info.cpu = cpuLine.empty() ? runCommand("lscpu | grep 'Model name' | awk -F: '{print $2}'") : cpuLine;

    // GPU Info
    info.gpu = runCommand("screenfetch -nN | grep 'GPU:' | head -n1 | awk -F': ' '{print $2}'");

    // RAM Info (rounded logic)
    std::string memKbStr = runCommand("grep MemTotal /proc/meminfo | awk '{print $2}'");
    long memKb = memKbStr.empty() ? 0 : std::stol(memKbStr);
    int memGb = 0;
    if (memKb > 0) {
        double rawGb = memKb / 1048576.0;
        if (rawGb > 8) {
            int half = static_cast<int>(std::ceil(rawGb / 2.0));
            memGb = half * 2;
        } else {
            memGb = static_cast<int>(std::round(rawGb));
        }
    }
    info.ram = std::to_string(memGb) + " GB";

    // Resolution
    info.resolution = runCommand("xdpyinfo | grep dimensions | awk '{print $2}'");

    // Screen Size (diagonal in inches, from EDID)
    std::string edidOutput = runCommand("xrandr --verbose | grep -m1 -A5 ' connected' | grep -Eo '[0-9]+mm x [0-9]+mm'");
    int widthMM = 0, heightMM = 0;
    if (!edidOutput.empty()) {
        if (sscanf(edidOutput.c_str(), "%dmm x %dmm", &widthMM, &heightMM) == 2 && widthMM > 0 && heightMM > 0) {
            double widthIn = widthMM / 25.4;
            double heightIn = heightMM / 25.4;
            double diagonal = std::sqrt(widthIn * widthIn + heightIn * heightIn);
            std::ostringstream oss;
            oss.precision(1);
            oss << std::fixed << diagonal << "\"";
            info.screenSize = oss.str();
        } else {
            info.screenSize = "Unknown";
        }
    } else {
        info.screenSize = "Unknown";
    }

    // Battery Condition
    std::string battery = runCommand("upower -e | grep BAT");
    if (!battery.empty()) {
        battery.erase(battery.find_last_not_of(" \n\r\t") + 1); // Trim newline
        std::string cmdFull = "upower -i " + battery + " | grep 'energy-full:' | awk '{print $2}'";
        std::string cmdDesign = "upower -i " + battery + " | grep 'energy-full-design:' | awk '{print $2}'";

        std::string energyFull = runCommand(cmdFull.c_str());
        std::string energyDesign = runCommand(cmdDesign.c_str());

        if (!energyFull.empty() && !energyDesign.empty() && energyDesign != "0") {
            float health = std::stof(energyFull) / std::stof(energyDesign) * 100;
            if (health >= 90) info.battery = "Excellent";
            else if (health >= 70) info.battery = "Good";
            else info.battery = "Poor";
        } else {
            info.battery = "N/A";
        }
    } else {
        info.battery = "None";
    }

    // PCI Devices (filtered)
    std::istringstream ss(runCommand(
        "lspci -mm | grep -iv -E 'bridge|host|system peripheral|processing controller|usb|communication|memory controller|cavs' | "
        "grep -i -E 'VGA|Network|Ethernet|Audio|Non-Volatile' | awk -F'\"' '{print $6}' | sort -u"
    ));

    while (std::getline(ss, line)) {
        if (!line.empty()) info.pciDevices.push_back(line);
    }

    if (!info.storageTypes.empty()) {
        std::string driveList = "[+] Detected drive types: ";
        for (const auto& t : info.storageTypes) driveList += t + " ";
        logMessage(driveList);
    } else {
        logMessage("[-] No drive types detected.");
    }

    std::istringstream driveLines(runCommand(
    "lsblk -dn -o NAME,TRAN,ROTA,MODEL | awk '$1 !~ /^(loop|sr)/ && $2 != \"\"'"
));

bool nonUsbDetected = false;
while (std::getline(driveLines, line)) {
    std::istringstream lineStream(line);
    std::string name, tran, rota, model;
    if (!(lineStream >> name >> tran >> rota)) continue;
    std::getline(lineStream, model); // Rest of the line is model (may contain spaces)
    model = model.empty() ? "Unknown" : model.substr(model.find_first_not_of(" \t"));

    std::string type;
    if (name.rfind("nvme", 0) == 0) type = "NVMe";
    else if (rota == "0") type = "SSD";
    else if (rota == "1") type = "HDD";
    else type = "Unknown";

    DriveInfo d { "/dev/" + name, type, tran, model };
    info.detectedDrives.push_back(d);

    // Optional logging
    std::string msg = "[*] Drive detected: " + d.name +
                      " (Protocol: " + tran +
                      ", Type: " + type +
                      ", Model: " + model + ")";
    logMessage(msg);

    // Flag non-usb if needed
    if (tran != "usb") nonUsbDetected = true;
}

info.hasNonUsbDrives = nonUsbDetected;
if (nonUsbDetected) {
    logMessage("[-] Warning: One or more non-USB drives detected.");
} else {
    logMessage("[+] No non-USB drives detected.");
}

    return info;
}
