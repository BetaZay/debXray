#pragma once
#include <string>
#include <vector>

void clearLog();
void logMessage(const std::string& message);
std::vector<std::string> readLogTail(int maxLines = 100);