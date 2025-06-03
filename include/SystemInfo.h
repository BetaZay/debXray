// ----------------------------------------
// SystemInfo.h
// ----------------------------------------
#pragma once
#include <vector>
#include <string>

struct DriveInfo {
    std::string name;    // e.g., /dev/sda
    std::string type;    // HDD, SSD, NVMe
    std::string tran;    // sata, usb, etc.
    std::string model;   // e.g., Samsung SSD
};

struct SystemInfo {
    bool isLaptop = false;
    bool hasNonUsbDrives = false;

    std::string model;          // Hardware model
    std::string serial;         // System serial number

    // CPU-related fields:
    std::string cpuBrand;       // e.g. "Intel(R) Core(TM) i7-"
    std::string cpuModel;       // e.g. "i7-8650U CPU @ 1.90GHz"
    std::string cpuSpeed;       // e.g. "1.90GHz"  (from lscpu)

    std::string physicalCPUs;   // e.g. "1" (number of sockets)

    std::string gpu;            // GPU info

    std::string ram;            // Rounded RAM in GB, e.g. "16 GB"
    std::string memoryType;     // e.g. "DDR4"

    std::string resolution;     // Screen resolution, e.g. "1920x1080"
    std::string screenSize;     // Diagonal in inches, e.g. "14.0\""

    std::string battery;        // "Excellent", "Good", "Poor", "N/A", or "None"

    std::vector<std::string> pciDevices;   // Filtered PCI devices
    std::vector<std::string> storageTypes; // e.g. {"NVMe","SATA","USB"}
    std::vector<DriveInfo> detectedDrives; // /dev/sdX, type, tran, model
};

SystemInfo getSystemInfo();
bool isAppleOrSurface(const std::string& model);
