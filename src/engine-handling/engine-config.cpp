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
#include "../base-elements/app-error.h"
#include "../base-elements/string-helper.h"
#include "../cli/settings-manager.h"

#include <string>
#include <filesystem>
#include <iostream>
#include <unordered_set>
#include <vector>
#include <stdexcept>
#include <set>

namespace QaplaTester {

std::unordered_map<std::string, std::string> EngineConfig::getOptions(const EngineOptions& availableOptions) const {
    std::unordered_map<std::string, std::string> filteredOptions;
    for (const auto& option : availableOptions) {
		auto it = optionValues_.find(QaplaHelpers::to_lowercase(option.name));
        if (it != optionValues_.end()) {
            filteredOptions[option.name] = it->second.value;
        }
    }
    return filteredOptions;
}

template<typename T>
inline constexpr bool always_false = false;

std::string EngineConfig::toString(const Value& value) {
    return std::visit([](auto&& v) -> std::string 
        {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, std::string>) { return v; }
            else if constexpr (std::is_same_v<T, int> || std::is_same_v<T, unsigned int> || std::is_same_v<T, double>) { 
                return std::to_string(v); 
            }
            else if constexpr (std::is_same_v<T, bool>) { return v ? "true" : "false"; }
            else {
                static_assert(always_false<T>, "Unsupported variant type");
            }
        }, value
    );
}

void EngineConfig::setTimeControl(const std::string& tc) {
	if (tc.empty()) {
		return;
	}
	try {
		tc_ = TimeControl::parse(tc);
	}
	catch (const std::exception& e) {
		throw AppError::makeInvalidParameters("Invalid time control format: " + tc + " for engine " + getName() + ". " + e.what());
	}
}

void EngineConfig::setTraceLevel(const std::string& level) {

    const auto lowercaseLevel = QaplaHelpers::to_lowercase(level);
    if (lowercaseLevel == "none") {
        traceLevel_ = TraceLevel::none;
    }
    else if (lowercaseLevel == "all") {
        traceLevel_ = TraceLevel::info;
    }
    else if (lowercaseLevel == "command") {
        traceLevel_ = TraceLevel::command;
    }
    else
    {
        AppError::throwOnInvalidOption({ "none", "all", "command" }, level, 
            "Invalid trace level for engine " + getName());
    }
}

void EngineConfig::setProtocol(const std::string& proto) {
	protocol_ = parseEngineProtocol(proto);
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void EngineConfig::setCommandLineOptions(const ValueMap& values, bool update) {
    std::unordered_set<std::string> seenKeys;

    for (const auto& [key, value] : values) {
        if (!seenKeys.insert(key).second) {
            throw std::runtime_error("Duplicate key in engine options: " + key);
        }
        if (update && std::holds_alternative<std::string>(value) && std::get<std::string>(value).empty()) {
            continue;
        }
        // "selected" is a GUI-only flag (see selected_). Deliberately not adopted here: an engine
        // configured on the command line or in a settings file is active by definition, and taking
        // over a stale "selected=false" from an older state file would propagate it on the next save.
        if (key == "conf" || key == "id" || key == "author" || key == "selected") { continue; }
        if (key == "ponder") { setPonder(std::get<bool>(value)); }
        else if (key == "originalname") { originalName_ = std::get<std::string>(value); }
        else if (key == "tc") { setTimeControl(std::get<std::string>(value)); }
        else if (key == "gauntlet") { setGauntlet(std::get<bool>(value)); }
        else if (key == "whitepov") { setScoreFromWhitePov(std::get<bool>(value)); }
        else if (key == "trace") { setTraceLevel(std::get<std::string>(value)); }
        else if (key == "name") {
            if (!update) { setName(std::get<std::string>(value)); }
        }
        else if (key == "cmd") { setCmd(std::get<std::string>(value)); }
        else if (key == "dir") { setDir(std::get<std::string>(value)); }
        else if (key == "args") { setArgs(std::get<std::string>(value)); }
        else if (key == "restart") { restart_ = parseRestartOption(std::get<std::string>(value)); }
        else if (key == "proto") { setProtocol(std::get<std::string>(value)); }
        else if (key.starts_with("option.")) { setOptionValue(key.substr(7), toString(value)); }
        else {
            AppError::throwOnInvalidOption(
                { "name", "cmd", "dir", "args", "tc", "ponder", "gauntlet", "whitepov", "trace", "restart", "proto", "option."},
                key, 
				"Invalid engine option key" 
            );
        }
    }
    if (!update) { finalizeSetOptions(); }
}

void EngineConfig::warnOnNameMismatch(const std::string& fileName, const std::string& engineName) const {
    const std::string normName = QaplaHelpers::to_lowercase(QaplaHelpers::to_alphanum(engineName));
    const std::string normFile = QaplaHelpers::to_lowercase(QaplaHelpers::to_alphanum(fileName));

    const size_t len = std::min(normName.size(), normFile.size());
    if (len <= 2) { return; }

    if (normName.find(normFile) != std::string::npos || normFile.find(normName) != std::string::npos) {
        return;
    }

    const size_t dist = QaplaHelpers::levenshteinDistance(normName, normFile);
    
    if (dist > 0 && dist < 3) {
        std::cerr << "Warning: Engine name '" << getName()
            << "' and command filename '" << fileName
            << "' appear mismatched.\n";
    }
}

void EngineConfig::finalizeSetOptions() {
    if (getCmd().empty()) { throw std::runtime_error("Missing required field: cmd"); }
    std::string fileName = std::filesystem::path(getCmd()).filename().string();
    warnOnNameMismatch(fileName, getName());
    if (getName().empty()) { setName(fileName); }
    if (getDir().empty()) { setDir("."); }
    if (protocol_ == EngineProtocol::Unknown) { protocol_ = EngineProtocol::Uci; }
}

bool operator==(const EngineConfig& lhs, const EngineConfig& rhs) {
    return lhs.name_ == rhs.name_
        && lhs.author_ == rhs.author_
        && lhs.cmd_ == rhs.cmd_
        && lhs.dir_ == rhs.dir_
        && lhs.args_ == rhs.args_
        && lhs.tc_ == rhs.tc_
        && lhs.protocol_ == rhs.protocol_
        && lhs.ponder_ == rhs.ponder_
        && lhs.scoreFromWhitePov_ == rhs.scoreFromWhitePov_
        // && lhs.gauntlet_ == rhs.gauntlet_ decided to not compare gauntlet setting
        && lhs.optionValues_ == rhs.optionValues_;
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void  EngineConfig::setValue(const std::string& key, const std::string& value) {
    static const std::set<std::string> internalKeys = { "id" };
    // Recognized keys are matched case-insensitively (consistent with Settings::Manager
    // and with EngineConfig::toSection(), which writes e.g. "originalName" in mixed case).
    // Unrecognized keys fall through to setOptionValue() with their original casing intact,
    // since real UCI option names are case-sensitive.
    const std::string lowerKey = QaplaHelpers::to_lowercase(key);
    if (lowerKey == "name") { setName(value); }
    else if (lowerKey == "originalname") { originalName_ = value; }
    else if (lowerKey == "author") { setAuthor(value); }
    else if (lowerKey == "cmd") { setCmd(value); }
    else if (lowerKey == "dir") { setDir(value); }
    else if (lowerKey == "args") { setArgs(value); }
    else if (lowerKey == "tc") { setTimeControl(value); }
    else if (lowerKey == "gauntlet") {
        if (value == "true" || value == "1" || value.empty()) { setGauntlet(true); }
        else if (value == "false" || value == "0") { setGauntlet(false); }
    }
    else if (lowerKey == "ponder") {
        if (value == "true" || value == "1" || value.empty()) { setPonder(true); }
        else if (value == "false" || value == "0") { setPonder(false); }
    }
    else if (lowerKey == "whitepov") {
        if (value == "true" || value == "1" || value.empty()) { setScoreFromWhitePov(true); }
        else if (value == "false" || value == "0") { setScoreFromWhitePov(false); }
    }
    else if (lowerKey == "selected") {
        if (value == "true" || value == "1" || value.empty()) { setSelected(true); }
        else if (value == "false" || value == "0") { setSelected(false); }
    }
    else if (lowerKey == "trace") { setTraceLevel(value); }
    else if (lowerKey == "restart") { restart_ = parseRestartOption(value); }
    else if (lowerKey == "proto") { setProtocol(value); }
    else if (internalKeys.contains(lowerKey)) {
        internalKeys_[lowerKey] = value;
    }
    else {
        setOptionValue(key, value);
    }
}

EngineConfig EngineConfig::createFromSection(const QaplaHelpers::IniFile::Section& section) {

    EngineConfig config;
    std::unordered_set<std::string> seenKeys;

    for (const auto& [key, value] : section.entries) {
        if (!seenKeys.insert(key).second) {
            throw std::runtime_error("Duplicate key: " + key);
        }
        config.setValue(key, value);
    }

    config.finalizeSetOptions();
    return config;
}

std::optional<std::string> EngineConfig::getValue(const std::string& key) const {
    // Counterpart of setValue(): same key vocabulary, same case-insensitive matching, so that
    // whatever this class writes it can also read back. Keys the configuration does not own
    // ("conf" selects a template, "id" is stamped on by the file writer) yield no value.
    const std::string lowerKey = QaplaHelpers::to_lowercase(key);
    if (lowerKey == "name") { return name_; }
    if (lowerKey == "originalname") { return originalName_; }
    if (lowerKey == "author") { return author_; }
    if (lowerKey == "cmd") { return cmd_; }
    if (lowerKey == "dir") { return dir_; }
    if (lowerKey == "args") { return args_; }
    if (lowerKey == "proto") { return to_string(protocol_); }
    if (lowerKey == "trace") { return to_string(traceLevel_); }
    if (lowerKey == "restart") { return to_string(restart_); }
    if (lowerKey == "tc") { return tc_.toPgnTimeControlString(); }
    if (lowerKey == "ponder") { return ponder_ ? "true" : "false"; }
    if (lowerKey == "gauntlet") { return gauntlet_ ? "true" : "false"; }
    if (lowerKey == "whitepov") { return scoreFromWhitePov_ ? "true" : "false"; }
    if (lowerKey == "selected") { return selected_ ? "true" : "false"; }
    return std::nullopt;
}

QaplaHelpers::IniFile::Section EngineConfig::toSection(const std::string& sectionName) const {
    QaplaHelpers::IniFile::Section section;
    section.name = sectionName;

    // Which keys exist and how they are spelled is owned by the central parameter definition -
    // the same one that validates these sections when they are read back. Deriving the key names
    // here instead of listing them locally is what keeps writing and reading in step: a writer
    // with its own idea of the format is exactly how state files carrying UCI options became
    // unreadable ("Unknown parameter in section engine: 'hash'").
    const auto& groupDefinitions = Settings::Manager::instance().getGroupDefinitions();
    const auto definition = groupDefinitions.find("engine");
    if (definition == groupDefinitions.end()) {
        throw AppError::make("No parameter definition for engine sections available.");
    }

    constexpr std::string_view namePlaceholder = "[name]";
    for (const auto& keyName : definition->second.getKeyNames()) {
        // A "<prefix>.[name]" key stands for a family of freely named entries - UCI options are
        // the only one. Their names are case sensitive, so the name the engine reported is used.
        if (keyName.ends_with(namePlaceholder)) {
            const std::string prefix = keyName.substr(0, keyName.size() - namePlaceholder.size());
            for (const auto& [_, option] : optionValues_) {
                section.addEntry(prefix + option.originalName, option.value);
            }
            continue;
        }

        const auto value = getValue(keyName);
        if (!value || value->empty()) { continue; }

        // Skip default boolean values to keep config clean
        if ((keyName == "ponder" || keyName == "whitepov" || keyName == "gauntlet") && *value == "false") {
            continue;
        }
        // "selected" defaults to true, so only a deselected engine needs to record it. Keeping the
        // key out of the common case means tournament/SPRT state files - which contain the active
        // engines only - carry no selection state at all, and both the GUI and the CLI can read
        // "engine section present" as "engine takes part".
        if (keyName == "selected" && *value == "true") {
            continue;
        }
        // Don't save originalName as it is discovered from the engine executable
        if (keyName == "originalName") { continue; }

        section.addEntry(keyName, *value);
    }
    return section;
}

std::unordered_map<std::string, std::string> EngineConfig::toDisambiguationMap() const {
    std::unordered_map<std::string, std::string> result;

    if (!name_.empty()) {
        result["name"] = name_;
    }

    if (!author_.empty()) {
        result["author"] = author_;
    }

    if (!args_.empty()) {
        result["args"] = args_;
    }

    result["proto"] = to_string(protocol_);

    if (ponder_) { result["ponder"] = ""; }
	if (gauntlet_) { result["gauntlet"] = ""; }
	if (scoreFromWhitePov_) { result["whitepov"] = ""; }

    for (const auto& [_, value] : optionValues_) {
        result[value.originalName] = value.value;
    }

    return result;
}

} // namespace QaplaTester
