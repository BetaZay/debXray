#include "DependencyManager.h"
#include "Log.h"
#include <unistd.h>
#include <cstdlib>
#include <vector>

bool isOnline() {
    return system("ping -c1 -W1 8.8.8.8 > /dev/null 2>&1") == 0;
}

bool isRoot() {
    return geteuid() == 0;
}

void installDependencies() {
    const std::vector<std::string> packages = { "curl", "screenfetch" };
    std::vector<std::string> missing;
    for (const auto& pkg : packages) {
        std::string checkCmd = "dpkg -s " + pkg + " > /dev/null 2>&1";
        if (system(checkCmd.c_str()) != 0) missing.push_back(pkg);
    }

    if (missing.empty()) {
        logMessage("[+] All dependencies already installed.");
        return;
    }

    std::string pkgList;
    for (const auto& pkg : missing) pkgList += pkg + " ";
    std::string command;

    if (isRoot())
        command = "apt-get update && apt-get install -y " + pkgList;
    else if (system("which pkexec > /dev/null 2>&1") == 0)
        command = "pkexec apt-get update && pkexec apt-get install -y " + pkgList;
    else
        command = "sudo apt-get update && sudo apt-get install -y " + pkgList;

    int status = system(command.c_str());
    logMessage(status == 0 ? "[+] Dependencies installed." : "[-] Failed to install some dependencies.");
}