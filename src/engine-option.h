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
#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cassert>
#include "app-error.h"

enum class RestartOption {
	EngineDecides,
	Always,
	Never
};

inline std::string to_string(RestartOption restart) {
	switch (restart) {
	case RestartOption::Always: return "on";
	case RestartOption::Never: return "off";
	default: return "auto";
	}
}

inline RestartOption parseRestartOption(const std::string& value) {
	if (value == "auto") return RestartOption::EngineDecides;
	if (value == "on") return RestartOption::Always;
	if (value == "off") return RestartOption::Never;
	AppError::throwOnInvalidOption({ "auto", "on", "off" }, value, "restart option");	
	return RestartOption::EngineDecides;
}

enum class EngineProtocol {
	Uci,
	XBoard,
	Unknown
};

inline std::string to_string(EngineProtocol protocol) {
	switch (protocol) {
	case EngineProtocol::Uci: return "uci";
	case EngineProtocol::XBoard: return "xboard";
	default: return "unknown";
	}
}

 /**
  * Represents an option that can be set for a chess engine.
  */
struct EngineOption {
    enum class Type { File, Path, Check, Spin, Slider, Combo, Button, String, Unknown };

    std::string name;
    Type type = Type::Unknown;
    std::string defaultValue;
    std::optional<int> min;
    std::optional<int> max;
    std::vector<std::string> vars;
};

using EngineOptions = std::vector<EngineOption>;

inline EngineOption::Type parseOptionType(const std::string& typeStr) {
	if (typeStr == "button" || typeStr == "save" || typeStr == "reset") return EngineOption::Type::Button;
	if (typeStr == "check")  return EngineOption::Type::Check;
	if (typeStr == "string") return EngineOption::Type::String;
	if (typeStr == "file")   return EngineOption::Type::File;
	if (typeStr == "path")   return EngineOption::Type::Path;
	if (typeStr == "spin")   return EngineOption::Type::Spin;
	if (typeStr == "slider") return EngineOption::Type::Spin;
	if (typeStr == "combo")  return EngineOption::Type::Combo;
	return EngineOption::Type::Unknown;
}
