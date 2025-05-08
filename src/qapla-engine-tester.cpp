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

#include "engine-process.h"
#include <iostream>
#include <thread>

int main() {
    try {
        EngineProcess engine(
            "C:\\Development\\qapla-engine-tester\\Qapla0.3.0-win-x86.exe"
        );

        std::cout << "Engine gestartet.\n";

        engine.writeLine("uci");

        while (true) {
            auto line = engine.readLine(std::chrono::milliseconds(1000));
            if (!line) {
                std::cerr << "Timeout beim Warten auf UCI-Antwort.\n";
                break;
            }

            std::cout << "[ENGINE] " << *line << '\n';

            if (*line == "uciok") break;
        }

        // engine.writeLine("quit");

        // Warte kurz auf Prozessende
        for (int i = 0; i < 10; ++i) {
            if (!engine.isRunning()) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        if (engine.isRunning()) {
            std::cerr << "Engine lebt noch - wird nun beendet.\n";
            engine.terminate();
        }
        else {
            std::cout << "Engine hat sich korrekt beendet.\n";
        }

    }
    catch (const std::exception& ex) {
        std::cerr << "Fehler: " << ex.what() << '\n';
        return 1;
    }

    return 0;
}


