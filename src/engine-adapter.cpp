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


#include "engine-adapter.h"

namespace QaplaTester {

EngineAdapter::EngineAdapter(const std::filesystem::path& enginePath,
    const std::optional<std::filesystem::path>& workingDirectory, 
    const std::string& identifier)
    : process_(enginePath, workingDirectory, identifier), 
      identifier_(identifier) {
}

uint64_t EngineAdapter::writeCommand(const std::string& command) {
    if (terminating_) {
		// The engine is probably not running anymore, so we cannot write commands.
        return 0; 
    }
    std::scoped_lock lock(commandMutex_);
    logToEngine(command, TraceLevel::command);
    return process_.writeLine(command);
}

} // namespace QaplaTester
