
#include "oss-tools.h"

#ifdef _WIN32
#include <windows.h>
#include <vector>

int getPhysicalCoreCount() {
    DWORD length = 0;
    // Erster Aufruf ermittelt notwendige Buffer-Größe
    if (GetLogicalProcessorInformation(nullptr, &length) == FALSE && GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
        std::vector<SYSTEM_LOGICAL_PROCESSOR_INFORMATION> buffer(length / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION));
        
        if (GetLogicalProcessorInformation(buffer.data(), &length) != FALSE) {
            int count = 0;
            for (const auto& info : buffer) {
                if (info.Relationship == RelationProcessorCore) {
                    count++;
                }
            }
            return count;
        }
    }
    return 0;
}

// --- APPLE Implementation ---
#elif defined(__APPLE__)
#include <sys/sysctl.h>
#include <cstddef>

int getPhysicalCoreCount() {
    int count = 0;
    size_t size = sizeof(count);
    if (sysctlbyname("hw.physicalcpu", &count, &size, nullptr, 0) == 0) {
        return count;
    }
    return 0;
}

// --- LINUX Implementation ---
#elif defined(__linux__)
#include <fstream>
#include <set>
#include <utility>
#include <string>

int getPhysicalCoreCount() {
    std::ifstream file("/proc/cpuinfo");
    if (!file.is_open()) {
        return 0;
    }

    std::set<std::pair<int, int>> core_identifiers;
    int current_phys_id = -1;
    std::string line;

    while (std::getline(file, line)) {
        if (line.find("physical id") == 0) {
            const size_t pos = line.find(':');
            if (pos != std::string::npos) {
                current_phys_id = std::stoi(line.substr(pos + 1));
            }
        } else if (line.find("core id") == 0) {
            const size_t pos = line.find(':');
            if (pos != std::string::npos && current_phys_id != -1) {
                const int current_core_id = std::stoi(line.substr(pos + 1));
                core_identifiers.insert({current_phys_id, current_core_id});
            }
        }
    }

    // Fallback für Architekturen ohne detaillierte Topologie-Infos (z.B. ARM)
    if (core_identifiers.empty()) {
        file.clear();
        file.seekg(0);
        while (std::getline(file, line)) {
            if (line.find("cpu cores") == 0) {
                const size_t pos = line.find(':');
                if (pos != std::string::npos) {
                    return std::stoi(line.substr(pos + 1));
                }
            }
        }
        return 0;
    }
    return static_cast<int>(core_identifiers.size());
}

// --- FALLBACK (Other OS) ---
#else
int getPhysicalCoreCount() {
    return 0;
}
#endif