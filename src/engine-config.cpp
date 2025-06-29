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
 
#include "engine-config.h"
#include "engine-option.h"
#include "app-error.h"

std::unordered_map<std::string, std::string> EngineConfig::getOptions(const EngineOptions availableOptions) const {
    std::unordered_map<std::string, std::string> filteredOptions;
    for (const auto& option : availableOptions) {
		auto it = optionValues_.find(to_lowercase(option.name));
        if (it != optionValues_.end()) {
            filteredOptions[option.name] = it->second.value;
        }
    }
    return filteredOptions;
}

template<typename T>
inline constexpr bool always_false = false;

std::string EngineConfig::toString(const Value& value) {
    return std::visit([](auto&& v) -> std::string {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::string>) return v;
        else if constexpr (std::is_same_v<T, int>) return std::to_string(v);
		else if constexpr (std::is_same_v<T, float>) return std::to_string(v);
        else if constexpr (std::is_same_v<T, bool>) return v ? "true" : "false";
        else static_assert(always_false<T>, "Unsupported variant type");
        }, value);
}

void EngineConfig::setTimeControl(const std::string& tc) {
	if (tc.empty()) {
		throw AppError::makeInvalidParameters("Time control cannot be empty for engine " + getName());
	}
	try {
		tc_ = TimeControl::parse(tc);
	}
	catch (const std::exception& e) {
		throw AppError::makeInvalidParameters("Invalid time control format: " + tc + " for engine " + getName() + ". " + e.what());
	}
}

void EngineConfig::setTraceLevel(const std::string& level) {
	if (level == "none") {
		traceLevel_ = TraceLevel::none;
	}
	else if (level == "all") {
		traceLevel_ = TraceLevel::info;
	}
	else if (level == "command") {
		traceLevel_ = TraceLevel::command;
	}
	else {
		throw AppError::makeInvalidParameters("Unknown trace level: " + level + " for engine " + getName());
	}
}

void EngineConfig::setCommandLineOptions(const ValueMap& values, bool update) {
    std::unordered_set<std::string> seenKeys;

    for (const auto& [key, value] : values) {
        if (!seenKeys.insert(key).second)
            throw std::runtime_error("Duplicate key in engine options: " + key);
        if (update && std::holds_alternative<std::string>(value) && std::get<std::string>(value).empty())
            continue;
        if (key == "conf") continue;
        if (key == "ponder") setPonder(std::get<bool>(value));
        else if (key == "tc") setTimeControl(std::get<std::string>(value));
        else if (key == "gauntlet") setGauntlet(std::get<bool>(value));
        else if (key == "trace") setTraceLevel(std::get<std::string>(value));
        else if (key == "name") {
            if (!update) setName(std::get<std::string>(value));
        } else if (key == "cmd") setExecutablePath(std::get<std::string>(value));
        else if (key == "dir") setWorkingDirectory(std::get<std::string>(value));
        else if (key == "proto") {
            auto valueStr = std::get<std::string>(value);
            if (valueStr == "uci") protocol_ = EngineProtocol::Uci;
            else if (valueStr == "xboard") protocol_ = EngineProtocol::XBoard;
            else throw std::runtime_error("Unknown protocol: " + valueStr);
        }
        else if (key.starts_with("option.")) {
            setOptionValue(key.substr(7), toString(value));
        }
        else {
			throw AppError::makeInvalidParameters("Unknown engine option key: " + key + " for engine " + getName());
        }
    }
    if (!update) finalizeSetOptions();
}

void EngineConfig::finalizeSetOptions() {
    if (getExecutablePath().empty()) throw std::runtime_error("Missing required field: cmd");
    if (getName().empty()) setName(getExecutablePath());
    if (getWorkingDirectory().empty()) setWorkingDirectory(".");
    if (protocol_ == EngineProtocol::Unknown) protocol_ = EngineProtocol::Uci;
}

void EngineConfig::readHeader(std::istream& in) {
    std::string line;

    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        if (line.front() == '[' && line.back() == ']') {
            std::string name = line.substr(1, line.size() - 2);
            if (name.empty()) throw std::runtime_error("Empty section name");
            setName(name);
            return;
        }
        break;
    }

    throw std::runtime_error("Expected section header");
}

bool operator==(const EngineConfig& lhs, const EngineConfig& rhs) {
    return lhs.name_ == rhs.name_
        && lhs.executablePath_ == rhs.executablePath_
        && lhs.workingDirectory_ == rhs.workingDirectory_
        && lhs.tc_ == rhs.tc_
        && lhs.protocol_ == rhs.protocol_
        && lhs.ponder_ == rhs.ponder_
        // && lhs.gauntlet_ == rhs.gauntlet_ decided to not compare gauntlet setting
        && lhs.optionValues_ == rhs.optionValues_;
}

std::istream& operator>>(std::istream& in, EngineConfig& config) {
    config.readHeader(in);

    std::string line;
    std::unordered_set<std::string> seenKeys;

    while (in && in.peek() != '[' && std::getline(in, line)) {
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;

        auto pos = line.find('=');
        if (pos == std::string::npos) continue;

        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);

        if (!seenKeys.insert(key).second)
            throw std::runtime_error("Duplicate key: " + key);
        if (key == "name")
            throw std::runtime_error("name is set in the header and may not be set again");
        else if (key == "executablePath") config.setExecutablePath(value);
        else if (key == "workingDirectory") config.setWorkingDirectory(value);
		else if (key == "tc") config.setTimeControl(value);
		else if (key == "ponder") {
			if (value == "true" || value == "1" || value == "") config.setPonder(true);
			else if (value == "false" || value == "0") config.setPonder(false);
			else throw std::runtime_error("Invalid ponder value: " + value);
		}
        else if (key == "protocol") {
            if (value == "uci") config.protocol_ = EngineProtocol::Uci;
            else if (value == "xboard") config.protocol_ = EngineProtocol::XBoard;
            else throw std::runtime_error("Unknown protocol: " + value);
        }
        else {
            config.setOptionValue(key, value);
        }
    }

    config.finalizeSetOptions();
    return in;
}


std::ostream& operator<<(std::ostream& out, const EngineConfig& config) {
    if (!out) throw std::runtime_error("Invalid output stream");

    out << "[" << config.name_ << "]\n";
    out << "protocol=" << to_string(config.protocol_) << '\n';
    out << "executablePath=" << config.executablePath_ << '\n';
    out << "workingDirectory=" << config.workingDirectory_ << '\n';
    out << "tc=" << config.tc_.toPgnTimeControlString() << '\n';
	if (config.ponder_) out << "ponder=" << (config.ponder_ ? "true" : "false") << '\n';
    for (const auto& [_, value] : config.optionValues_) {
        out << value.originalName << "=" << value.value << '\n';
    }

    return out;
}

std::unordered_map<std::string, std::string> EngineConfig::toDisambiguationMap() const {
    std::unordered_map<std::string, std::string> result;

    if (!name_.empty())
        result["name"] = name_;

    result["protocol"] = to_string(protocol_);

    if (ponder_)
        result["ponder"] = "";

	if (gauntlet_)
		result["gauntlet"] = "";

    for (const auto& [_, value] : optionValues_) {
        result[value.originalName] = value.value;
    }

    return result;
}
