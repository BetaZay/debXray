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

    std::string model;             // Hardware model
    std::string serial;            // System serial number
    std::string cpu;               // CPU brand/model
    std::string gpu;               // GPU info
    std::string ram;               // Rounded RAM in GB
    std::string resolution;        // Screen resolution
    std::string screenSize;        // Diagonal screen size in inches (e.g. "14.0\"")
    std::string battery;           // Battery health description

    std::vector<std::string> pciDevices;   // Filtered PCI devices
    std::vector<std::string> storageTypes; // e.g., {"SATA", "NVMe", "SAS"}
    std::vector<DriveInfo> detectedDrives; // /dev/sdX, type, tran, model
};

SystemInfo getSystemInfo();
bool isAppleOrSurface(const std::string& model);