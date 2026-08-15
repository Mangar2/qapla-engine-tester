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
#include <string_view>
#include <filesystem>
#include <iostream>
#include <unordered_set>
#include <vector>
#include <stdexcept>
#include <set>

namespace QaplaTester {

namespace {

/**
 * @brief Parses the boolean spellings an ini file or the settings manager can deliver.
 * @return The parsed value, or std::nullopt if the text is not a boolean.
 */
[[nodiscard]] std::optional<bool> parseConfigBool(const std::string& value) {
    if (value == "true" || value == "1" || value.empty()) { return true; }
    if (value == "false" || value == "0") { return false; }
    return std::nullopt;
}

/**
 * @brief Reads and writes a single configuration key as text.
 */
struct KeyAccessor {
    void (*set)(EngineConfig&, const std::string&);
    std::string (*get)(const EngineConfig&);
};

/// Keys of the engine section that are not properties of a configuration: "conf" selects a
/// template and is resolved before a configuration exists, "id" states which run a section
/// belongs to and is stamped on by the file writer.
[[nodiscard]] bool isConfigOwnedKey(const std::string& keyName) {
    return keyName != "conf" && keyName != "id";
}

/**
 * @brief Returns the central parameter definition of the engine section.
 */
[[nodiscard]] const Settings::GroupDefinition& engineDefinition() {
    const auto& groupDefinitions = Settings::Manager::instance().getGroupDefinitions();
    const auto definition = groupDefinitions.find("engine");
    if (definition == groupDefinitions.end()) {
        throw AppError::make("No parameter definition for engine sections available.");
    }
    return definition->second;
}

/**
 * @brief Returns the prefix the definition uses for freely named entries.
 *
 * The definition spells such a family as "<prefix>.[name]"; UCI options are the only one.
 * Taking the prefix from there rather than writing "option." into the code is what keeps
 * reading, writing and validating in step.
 */
[[nodiscard]] const std::string& optionKeyPrefix() {
    static const std::string prefix = [] {
        constexpr std::string_view placeholder = "[name]";
        for (const auto& keyName : engineDefinition().getKeyNames()) {
            if (keyName.ends_with(placeholder)) {
                return keyName.substr(0, keyName.size() - placeholder.size());
            }
        }
        throw AppError::make("Engine parameter definition has no key for named options.");
    }();
    return prefix;
}

using AccessorMap = QaplaHelpers::StableMap<std::string, KeyAccessor>;

/**
 * @brief Fails if the accessors and the central parameter definition have drifted apart.
 *
 * A key defined centrally but not accessible here would be silently dropped when a
 * configuration is saved; a key accessible here but undefined centrally would produce files
 * that no longer load. Both are programming errors and are reported as soon as an engine
 * configuration is used, not once a state file turns out to be unreadable.
 */
void verifyAccessorsMatchDefinition(const AccessorMap& accessors) {
    const auto& definition = engineDefinition();
    for (const auto& keyName : definition.getKeyNames()) {
        if (keyName.starts_with(optionKeyPrefix()) || !isConfigOwnedKey(keyName)) { continue; }
        if (!accessors.contains(keyName)) {
            throw AppError::make(std::format(
                "Engine parameter '{}' is defined centrally but cannot be read or written by "
                "EngineConfig. Add it to the accessor table in engine-config.cpp.", keyName));
        }
    }
    for (const auto& [keyName, _] : accessors) {
        if (!definition.keys.contains(keyName)) {
            throw AppError::make(std::format(
                "EngineConfig reads and writes '{}', but the central parameter definition does "
                "not know that key, so files containing it cannot be loaded.", keyName));
        }
    }
}

/**
 * @brief Maps every configuration key of the engine section to its accessor.
 *
 * Key names are those of the central parameter definition, which registers them in lower
 * case. This table is the single vocabulary shared by setValue(), setCommandLineOptions()
 * and toSection() - none of them may recognize a key of its own.
 */
[[nodiscard]] const AccessorMap& keyAccessors() {
    static const AccessorMap accessors = {
        { "name", { [](EngineConfig& config, const std::string& value) { config.setName(value); },
                    [](const EngineConfig& config) -> std::string { return config.getName(); } } },
        { "originalname", { [](EngineConfig& config, const std::string& value) { config.setReportedName(value); },
                    [](const EngineConfig& config) -> std::string { return config.getReportedName(); } } },
        { "author", { [](EngineConfig& config, const std::string& value) { config.setAuthor(value); },
                    [](const EngineConfig& config) -> std::string { return config.getAuthor(); } } },
        { "cmd", { [](EngineConfig& config, const std::string& value) { config.setCmd(value); },
                    [](const EngineConfig& config) -> std::string { return config.getCmd(); } } },
        { "dir", { [](EngineConfig& config, const std::string& value) { config.setDir(value); },
                    [](const EngineConfig& config) -> std::string { return config.getDir(); } } },
        { "args", { [](EngineConfig& config, const std::string& value) { config.setArgs(value); },
                    [](const EngineConfig& config) -> std::string { return config.getArgs(); } } },
        { "proto", { [](EngineConfig& config, const std::string& value) { config.setProtocol(value); },
                    [](const EngineConfig& config) -> std::string { return to_string(config.getProtocol()); } } },
        { "tc", { [](EngineConfig& config, const std::string& value) { config.setTimeControl(value); },
                    [](const EngineConfig& config) -> std::string {
                        return config.getTimeControl().toPgnTimeControlString(); } } },
        { "trace", { [](EngineConfig& config, const std::string& value) { config.setTraceLevel(value); },
                    [](const EngineConfig& config) -> std::string { return to_string(config.getTraceLevel()); } } },
        { "restart", { [](EngineConfig& config, const std::string& value) {
                        config.setRestartOption(parseRestartOption(value)); },
                    [](const EngineConfig& config) -> std::string { return to_string(config.getRestartOption()); } } },
        { "ponder", { [](EngineConfig& config, const std::string& value) {
                        if (const auto parsed = parseConfigBool(value)) { config.setPonder(*parsed); } },
                    [](const EngineConfig& config) -> std::string {
                        return config.isPonderEnabled() ? "true" : "false"; } } },
        { "gauntlet", { [](EngineConfig& config, const std::string& value) {
                        if (const auto parsed = parseConfigBool(value)) { config.setGauntlet(*parsed); } },
                    [](const EngineConfig& config) -> std::string {
                        return config.isGauntlet() ? "true" : "false"; } } },
        { "whitepov", { [](EngineConfig& config, const std::string& value) {
                        if (const auto parsed = parseConfigBool(value)) { config.setScoreFromWhitePov(*parsed); } },
                    [](const EngineConfig& config) -> std::string {
                        return config.isScoreFromWhitePov() ? "true" : "false"; } } },
        { "selected", { [](EngineConfig& config, const std::string& value) {
                        if (const auto parsed = parseConfigBool(value)) { config.setSelected(*parsed); } },
                    [](const EngineConfig& config) -> std::string {
                        return config.isSelected() ? "true" : "false"; } } }
    };
    static const bool verified = [] { verifyAccessorsMatchDefinition(accessors); return true; }();
    (void)verified;
    return accessors;
}

} // namespace

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

void EngineConfig::setCommandLineOptions(const ValueMap& values, bool update) {
    std::unordered_set<std::string> seenKeys;
    const auto& accessors = keyAccessors();

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
        // An update refines an existing configuration; its name is the one it is known under.
        if (update && key == "name") { continue; }

        const std::string text = toString(value);
        if (key.starts_with(optionKeyPrefix())) {
            setOptionValue(key.substr(optionKeyPrefix().size()), text);
            continue;
        }

        const auto accessor = accessors.find(key);
        if (accessor == accessors.end()) {
            AppError::throwOnInvalidOption(engineDefinition().getKeyNames(), key, "Invalid engine option key");
            throw AppError::makeInvalidParameters("Invalid engine option key: " + key);
        }
        accessor->second.set(*this, text);
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

void  EngineConfig::setValue(const std::string& key, const std::string& value) {
    static const std::set<std::string> internalKeys = { "id" };
    // Recognized keys and their spelling come from the central parameter definition, which
    // registers them in lower case, so the key is matched case-insensitively here as well.
    const std::string lowerKey = QaplaHelpers::to_lowercase(key);

    // Counterpart of toSection(), which spells UCI options with the prefix the parameter
    // definition requires. Without stripping it here, reading back what was written yields an
    // option literally named "option.Hash": the engine never receives the setting, and the next
    // save writes "option.option.Hash".
    if (lowerKey.starts_with(optionKeyPrefix())) {
        setOptionValue(key.substr(optionKeyPrefix().size()), value);
        return;
    }

    const auto& accessors = keyAccessors();
    if (const auto accessor = accessors.find(lowerKey); accessor != accessors.end()) {
        accessor->second.set(*this, value);
        return;
    }

    if (internalKeys.contains(lowerKey)) {
        internalKeys_[lowerKey] = value;
    }
    else {
        // A key the definition does not know is a UCI option written without the prefix - engine
        // configuration files have always spelled options that way. The original casing is kept,
        // since real UCI option names are case-sensitive.
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
    // Counterpart of setValue(), served by the same accessor table, so that whatever this class
    // writes it can also read back. Keys the configuration does not own ("conf" selects a
    // template, "id" is stamped on by the file writer) yield no value.
    const auto& accessors = keyAccessors();
    const auto accessor = accessors.find(QaplaHelpers::to_lowercase(key));
    if (accessor == accessors.end()) {
        return std::nullopt;
    }
    return accessor->second.get(*this);
}

QaplaHelpers::IniFile::Section EngineConfig::toSection(const std::string& sectionName) const {
    QaplaHelpers::IniFile::Section section;
    section.name = sectionName;

    // Which keys exist and how they are spelled is owned by the central parameter definition -
    // the same one that validates these sections when they are read back. Deriving the key names
    // here instead of listing them locally is what keeps writing and reading in step: a writer
    // with its own idea of the format is exactly how state files carrying UCI options became
    // unreadable ("Unknown parameter in section engine: 'hash'").
    for (const auto& keyName : engineDefinition().getKeyNames()) {
        // The "<prefix>.[name]" key stands for a family of freely named entries - UCI options are
        // the only one. Their names are case sensitive, so the name the engine reported is used.
        if (keyName.starts_with(optionKeyPrefix())) {
            for (const auto& [_, option] : optionValues_) {
                section.addEntry(optionKeyPrefix() + option.originalName, option.value);
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
        // Don't save the reported name as it is discovered from the engine executable. The key
        // is compared in lower case: the definition registers every key that way.
        if (keyName == "originalname") { continue; }

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
