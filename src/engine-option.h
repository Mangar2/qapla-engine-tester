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

#include "app-error.h"
#include "string-helper.h"

#include <string>
#include <vector>
#include <optional>
#include <cassert>

namespace QaplaTester {

enum class RestartOption : std::uint8_t {
	EngineDecides,
	Always,
	Never
};

inline std::string to_string(RestartOption restart) {
	switch (restart) {
	case RestartOption::Always: { return "on"; }
	case RestartOption::Never: { return "off"; }
	default: { return "auto"; }
	}
}

inline RestartOption parseRestartOption(const std::string& value) {
	const auto lowerValue = QaplaHelpers::to_lowercase(value);
	if (lowerValue == "auto") { return RestartOption::EngineDecides; }
	if (lowerValue == "engine decides") { return RestartOption::EngineDecides; }
	if (lowerValue == "on") { return RestartOption::Always; }
	if (lowerValue == "always") { return RestartOption::Always; }
	if (lowerValue == "off") { return RestartOption::Never; }
	if (lowerValue == "never") { return RestartOption::Never; }
	AppError::throwOnInvalidOption({ "auto", "on", "off" }, value, "restart option");
	return RestartOption::EngineDecides;
}

enum class EngineProtocol : std::uint8_t {
	Uci,
	XBoard,
	NotSupported,
	Unknown
};

inline std::string to_string(EngineProtocol protocol) {
	switch (protocol) {
	case EngineProtocol::Uci: { return "uci"; }
	case EngineProtocol::XBoard: { return "xboard"; }
	case EngineProtocol::NotSupported: { return "not supported"; }
	default: { return "unknown"; }
	}
}

inline EngineProtocol parseEngineProtocol(const std::string& value) {
	const auto lowerValue = QaplaHelpers::to_lowercase(value);
	if (lowerValue == "uci") { return EngineProtocol::Uci; }
	if (lowerValue == "xboard") { return EngineProtocol::XBoard; }
	if (lowerValue == "not supported") { return EngineProtocol::NotSupported; }
	AppError::throwOnInvalidOption({ "uci", "xboard" }, value, "protocol option");
	return EngineProtocol::Uci;
}

/**
 * @brief Represents an option that can be set for a chess engine.
 */
struct EngineOption {
    enum class Type : std::uint8_t { File, Path, Check, Spin, Slider, Combo, Button, Save, Reset, String, Unknown };

    std::string name;
    Type type = Type::Unknown;
    std::string defaultValue;
    std::optional<int> min;
    std::optional<int> max;
    std::vector<std::string> vars;

	/**
	 * @brief Parses a string to determine the option type.
	 * @param typeStr The string representation of the type.
	 * @return The corresponding Type enum value.
	 */
	static EngineOption::Type parseType(const std::string& typeStr) {
		const auto lowerTypeStr = QaplaHelpers::to_lowercase(typeStr);
		if (lowerTypeStr == "button") { return EngineOption::Type::Button; }
		if (lowerTypeStr == "save") { return EngineOption::Type::Save; }
		if (lowerTypeStr == "reset") { return EngineOption::Type::Reset; }
		if (lowerTypeStr == "check") { return EngineOption::Type::Check; }
		if (lowerTypeStr == "string") { return EngineOption::Type::String; }
		if (lowerTypeStr == "file") { return EngineOption::Type::File; }
		if (lowerTypeStr == "path") { return EngineOption::Type::Path; }
		if (lowerTypeStr == "spin") { return EngineOption::Type::Spin; }
		// Using Spin as Slider is not yet implemented in the GUI
		if (lowerTypeStr == "slider") { return EngineOption::Type::Spin; }
		if (lowerTypeStr == "combo") { return EngineOption::Type::Combo; }
		return EngineOption::Type::Unknown;
	}

	/**
	 * @brief Converts a Type enum value to its string representation.
	 * @param type The Type enum value.
	 * @return The string representation of the type.
	 */
	static std::string to_string(Type type) {
		switch (type) {
		case Type::File: { return "file"; }
		case Type::Path: { return "path"; }
		case Type::Check: { return "check"; }
		case Type::Spin: { return "spin"; }
		case Type::Slider: { return "slider"; }
		case Type::Combo: { return "combo"; }
		case Type::Button: { return "button"; }
		case Type::Save: { return "save"; }
		case Type::Reset: { return "reset"; }
		case Type::String: { return "string"; }
		default: { return "unknown"; }
		}
	}

};

using EngineOptions = std::vector<EngineOption>;

} // namespace QaplaTester
