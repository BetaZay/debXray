#include "Log.h"
#include <fstream>
#include <ctime>
#include <deque>

void clearLog() {
    std::ofstream logClear("/tmp/debxray.log", std::ios::trunc);
    if (!logClear.is_open()) {
        fprintf(stderr, "Failed to clear /tmp/debxray.log\n");
    }
}

void logMessage(const std::string& message) {
    std::ofstream logFile("/tmp/debxray.log", std::ios::app);
    if (!logFile.is_open()) return;
    std::time_t now = std::time(nullptr);
    char timeStr[100];
    std::strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
    logFile << "[" << timeStr << "] " << message << std::endl;
}

std::vector<std::string> readLogTail(int maxLines) {
    std::ifstream file("/tmp/debxray.log");
    if (!file.is_open()) return {};

    std::deque<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        if (lines.size() >= maxLines) lines.pop_front();
        lines.push_back(line);
    }
    return {lines.begin(), lines.end()};
}
