/**
 * @license
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * @author Volker Böhm
 * @copyright Copyright (c) 2025 Volker Böhm
 */

 #include "oss-tools.h"

#ifdef _WIN32
#include <windows.h>
#include <vector>

namespace QaplaHelpers {


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

}

// --- APPLE Implementation ---
#elif defined(__APPLE__)
#include <sys/sysctl.h>
#include <cstddef>

namespace QaplaHelpers {

int getPhysicalCoreCount() {
    int count = 0;
    size_t size = sizeof(count);
    if (sysctlbyname("hw.physicalcpu", &count, &size, nullptr, 0) == 0) {
        return count;
    }
    return 0;
}

}

// --- LINUX Implementation ---
#elif defined(__linux__)
#include <fstream>
#include <set>
#include <utility>
#include <string>

namespace QaplaHelpers {

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

}


// --- FALLBACK (Other OS) ---
#else

namespace QaplaHelpers {

int getPhysicalCoreCount() {
    return 0;
}

}


#endif

