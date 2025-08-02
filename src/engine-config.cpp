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
#include "string-helper.h"
#include <string>
#include <filesystem>

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
        else if constexpr (std::is_same_v<T, unsigned int>) return std::to_string(v);
		else if constexpr (std::is_same_v<T, double>) return std::to_string(v);
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
        AppError::throwOnInvalidOption({ "none", "all", "command" }, level, 
			"Invalid trace level for engine " + getName() + ". Supported levels are: none, all, command.");
	}
}

void EngineConfig::setProtocol(const std::string& proto) {
    if (proto == "uci") protocol_ = EngineProtocol::Uci;
    else if (proto == "xboard") protocol_ = EngineProtocol::XBoard;
    else {
		AppError::throwOnInvalidOption({ "uci", "xboard" }, proto, "Unknown protocol");
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
        }
        else if (key == "cmd") setCmd(std::get<std::string>(value));
        else if (key == "dir") setDir(std::get<std::string>(value));
        else if (key == "restart") restart_ = parseRestartOption(std::get<std::string>(value));
        else if (key == "proto") setProtocol(std::get<std::string>(value));
        else if (key.starts_with("option.")) setOptionValue(key.substr(7), toString(value));
        else {
            AppError::throwOnInvalidOption(
                { "name", "cmd", "dir", "tc", "ponder", "gauntlet", "trace", "restart", "proto", "option."},
                key, 
				"Invalid engine option key: " + key + 
                ". Supported keys are: name, cmd, dir, tc, ponder, gauntlet, trace, restart, proto, option.[name] ."
            );
        }
    }
    if (!update) finalizeSetOptions();
}

void EngineConfig::warnOnNameMismatch(const std::string& fileName, const std::string& engineName) const {
    const std::string normName = to_lowercase(to_alphanum(engineName));
    const std::string normFile = to_lowercase(to_alphanum(fileName));

    const size_t len = std::min(normName.size(), normFile.size());
    if (len <= 2) return;

    if (normName.find(normFile) != std::string::npos || normFile.find(normName) != std::string::npos)
        return;

    const size_t dist = levenshteinDistance(normName, normFile);
    
    if (dist > 0 && dist < 3) {
        std::cerr << "Warning: Engine name '" << getName()
            << "' and command filename '" << fileName
            << "' appear mismatched.\n";
    }
}

void EngineConfig::finalizeSetOptions() {
    if (getCmd().empty()) throw std::runtime_error("Missing required field: cmd");
    std::string fileName = std::filesystem::path(getCmd()).filename().string();
    warnOnNameMismatch(fileName, getName());
    if (getName().empty()) setName(fileName);
    if (getDir().empty()) setDir(".");
    if (protocol_ == EngineProtocol::Unknown) protocol_ = EngineProtocol::Uci;
}


bool operator==(const EngineConfig& lhs, const EngineConfig& rhs) {
    return lhs.name_ == rhs.name_
        && lhs.cmd_ == rhs.cmd_
        && lhs.dir_ == rhs.dir_
        && lhs.tc_ == rhs.tc_
        && lhs.protocol_ == rhs.protocol_
        && lhs.ponder_ == rhs.ponder_
        // && lhs.gauntlet_ == rhs.gauntlet_ decided to not compare gauntlet setting
        && lhs.optionValues_ == rhs.optionValues_;
}

std::istream& operator>>(std::istream& in, EngineConfig& config) {

    std::string line;
    std::unordered_set<std::string> seenKeys;
    
	auto sectionHeader = readSectionHeader(in);
    if (!sectionHeader) return in;
    if (*sectionHeader != "engine") {
		throw AppError::makeInvalidParameters("Invalid section header, expected [engine], got: " + 
            (sectionHeader ? *sectionHeader : "none"));
    }

    while (in && in.peek() != '[' && std::getline(in, line)) {
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;

        auto kv = parseKeyValue(line);
        if (!kv)
        {
            throw AppError::makeInvalidParameters("Invalid setting in line " + line 
                + "'. Expected 'key=value' format.");
        }
        const auto& [key, value] = *kv;

        if (!seenKeys.insert(key).second)
            throw std::runtime_error("Duplicate key: " + key);
        if (key == "name") config.setName(value);
        else if (key == "cmd") config.setCmd(value);
        else if (key == "dir") config.setDir(value);
        else if (key == "tc") config.setTimeControl(value);
        else if (key == "ponder") {
            if (value == "true" || value == "1" || value == "") config.setPonder(true);
            else if (value == "false" || value == "0") config.setPonder(false);
            else throw std::runtime_error("Invalid ponder value: " + value);
        }
        else if (key == "restart") config.restart_ = parseRestartOption(value);
        else if (key == "proto") config.setProtocol(value);
        else {
            config.setOptionValue(key, value);
        }
    }

    config.finalizeSetOptions();
    return in;
}

std::ostream& operator<<(std::ostream& out, const EngineConfig& config) {
    if (!out) throw std::runtime_error("Invalid output stream");

    out << "[engine]\n";
	out << "name=" << config.name_ << '\n';
    out << "cmd=" << config.cmd_ << '\n';
    out << "dir=" << config.dir_ << '\n';
    out << "restart=" << to_string(config.restart_) << '\n';
    out << "proto=" << to_string(config.protocol_) << '\n';
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

    result["proto"] = to_string(protocol_);

    if (ponder_) result["ponder"] = "";
	if (gauntlet_) result["gauntlet"] = "";

    for (const auto& [_, value] : optionValues_) {
        result[value.originalName] = value.value;
    }

    return result;
}
