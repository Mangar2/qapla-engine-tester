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

#include "qapla-settings.h"
#include "engine-settings-helper.h"

#include "settings-manager.h"
#include "settings-definitions.h"
#include "../base-elements/app-error.h"
#include "../base-elements/logger.h"
#include "../base-elements/ini-file.h"
#include "../opening/openings.h"
#include "../tournament/tournament.h"
#include "../tournament/tournament-config.h"
#include "../sprt/sprt-manager.h"
#include "../sprt/sprt-tournament-file.h"
#include "../sprt/sprt-config.h"
#include "../config/opening-config.h"
#include "../config/pgn-config.h"
#include "../config/adjudication-config.h"
#include "../epd/epd-manager.h"
#include "../spsa/spsa-optimizer.h"
#include "../clop/clop-optimizer.h"
#include "../mcp/mcp-server.h"

#include <fstream>

namespace QaplaTester::Settings {

QaplaSettings& QaplaSettings::instance() {
    static QaplaSettings instance;
    return instance;
}

void QaplaSettings::initializeConfigs(const std::vector<std::string>& args) {
    configData_.clear();
    
    // 1. Convert CLI arguments to ConfigData to find other configurations
    auto cliData = QaplaHelpers::ConfigData::fromArgv(args);
    
    // Temporarily apply to find settings file and mcp flag
    Manager::instance().clearValues();
    Manager::instance().parseInput(cliData, false);

    // 2. Load settings file if provided (Settings file is applied before CLI to allow CLI overrides)
    loadFromFile(Manager::instance().get<std::string>("settingsfile"), true, false);

    // 3. Load engines file. CLI overrides this, but this file overrides settings file.
    loadFromFile(Manager::instance().get<std::string>("enginesfile"), true, false, "config");

    // 4. Add CLI arguments to configData_ (CLI overrides settings file and engines file)
    configData_.push_back(cliData);

    // 5. Apply mcp environment layer if needed (MCP layer overrides CLI)
    if (Settings::Manager::instance().get<bool>("mcp")) {
        QaplaHelpers::ConfigData mcpEnvLayer;
        QaplaHelpers::IniFile::Section loggingSection;
        loggingSection.name = "logging";
        loggingSection.addEntry("trace", "none");
        mcpEnvLayer.addSection(loggingSection);
        configData_.push_back(mcpEnvLayer);
    }

    // 5. Load tournament/sprt config files (to continue a run - these override everything)
    auto sprtGroup = Manager::instance().getGroupInstance("sprt");
    std::string sprtFile = sprtGroup.has_value() ? sprtGroup->get<std::string>("file") : "";
    
    auto tournamentGroup = Manager::instance().getGroupInstance("tournament");
    std::string tournamentFile = tournamentGroup.has_value() ? tournamentGroup->get<std::string>("file") : "";

    if (!sprtFile.empty() || !tournamentFile.empty()) {
        if (Settings::Manager::instance().get<bool>("mcp")) {
            throw AppError::makeInvalidParameters("Continuing a tournament/SPRT run from file is not supported in MCP mode.");
        }
        
        loadFromFile(sprtFile, false, false);
        loadFromFile(tournamentFile, false, false);
    }

    if (Settings::Manager::instance().get<bool>("mcp")) {
        Mcp::McpServer::initialize();
    }

    // Initial apply to populate all internal members
    applyConfig(std::nullopt);

    // 6. Initialize engines only once
    m_rapid = Helper::applyEngineSettings(Manager::instance(), "engine");
}

void QaplaSettings::loadFromFile(const std::string& fileName, bool throwOnError, bool overwrite, std::optional<std::string> id) {
    if (fileName.empty()) {
        return;
    }
    std::ifstream file(fileName);
    if (!file.is_open()) {
        if (throwOnError) {
            throw AppError::makeInvalidParameters(std::format("Failed to open file: {}", fileName));
        }
        return;
    }
    QaplaHelpers::ConfigData fileData;
    fileData.load(file);
    if (id.has_value()) {
        fileData.setKeyInAllSections("id", *id);
    }
    configData_.push_back(fileData);
    Manager::instance().parseInput(fileData, overwrite);
}

void QaplaSettings::applyConfig(std::optional<QaplaHelpers::ConfigData> configData) {
    
    // Apply initial config vector (later entries override earlier ones)
    /*
    // to be tested, if this is really no longer needed.
    for (const auto& cfg : configData_) {
        Manager::instance().parseInput(cfg, false);
    }
    */
    
    // Apply dynamic configuration if provided
    if (configData.has_value()) {
        Manager::instance().parseInput(*configData, true);
    }
    
    // Validate all settings for completeness
    Manager::instance().validateCompleteness();

    setLoggerConfiguration();
    setPgnConfig(Manager::instance(), "pgnoutput");
    setDrawAdjudicationConfig(Manager::instance(), "draw");
    setResignAdjudicationConfig(Manager::instance(), "resign");
    setOpenings(Manager::instance(), "openings");
    // Must be after openings
    setSprtConfig(Manager::instance(), "sprt");
    setTournamentConfig(Manager::instance(), "tournament");
    setEpdConfig();
    setSPSAConfig();
    setCLOPConfig();
}

const std::vector<std::string>& QaplaSettings::getArguments() const {
    return m_arguments;
}

const LoggerConfig* QaplaSettings::getLoggerConfig() const {
    return m_loggerConfig.get();
}

void QaplaSettings::applyLoggerConfig(const std::string& reportLogBaseName) const {
    if (m_loggerConfig == nullptr) {
        throw AppError::make("Logger configuration not initialized.");
    }
    LoggerConfig config = *m_loggerConfig;
    config.reportLogBaseName = reportLogBaseName;
    setLoggerConfig(config);
    
    auto loggingSetting = Settings::Manager::instance().getGroupInstance("logging");
    TraceLevel reportLevel = TraceLevel::result;
    TraceLevel mcpLevel = TraceLevel::none;
    EngineLogger::engineLogger().setTraceLevel(TraceLevel::error, TraceLevel::info); 
    
    if (loggingSetting) {
        if (loggingSetting->get<bool>("engine")) {
            EngineLogger::engineLogger().setTraceLevel(TraceLevel::error, TraceLevel::info);
        } else {
            EngineLogger::engineLogger().setTraceLevel(TraceLevel::none, TraceLevel::none);
        }

        auto trace = loggingSetting->get<std::string>("trace");
        if (trace == "none") {
            reportLevel = TraceLevel::none;
        } else if (trace == "all") {
            reportLevel = TraceLevel::info;
        } else if (trace == "result") {
            reportLevel = TraceLevel::result;
        }

        auto mcpTrace = loggingSetting->get<std::string>("mcp");
        if (mcpTrace == "none") {
            mcpLevel = TraceLevel::none;
        } else if (mcpTrace == "all") {
            mcpLevel = TraceLevel::info;
        } else if (mcpTrace == "result") {
            mcpLevel = TraceLevel::result;
        }
    }
    
    Logger::reportLogger().setTraceLevel(reportLevel, TraceLevel::info, mcpLevel);
}

std::vector<std::string> QaplaSettings::argvToVector(int argc, char* argv[]) { // NOLINT(modernize-avoid-c-arrays)
    std::vector<std::string> result;
    result.reserve(static_cast<std::size_t>(argc));
    for (int i = 0; i < argc; ++i) {
        result.emplace_back(argv[i]);
    }
    return result;
}

void QaplaSettings::init() {
   initSettings();
}

void QaplaSettings::setLoggerConfiguration() {
    auto loggingSetting = Manager::instance().getGroupInstance("logging");
    
    std::string logPath = "./log";
    std::string logModeStr = "one";
    
    if (loggingSetting.has_value()) {
        logPath = loggingSetting->get<std::string>("path");
        logModeStr = loggingSetting->get<std::string>("mode");
    }
    
    LogFileStrategy logMode = (logModeStr == "each") ? LogFileStrategy::perEngine : LogFileStrategy::global;

    m_loggerConfig = std::make_unique<LoggerConfig>(LoggerConfig{
        .logPath = logPath,
        .reportLogBaseName = "report",
        .engineLogBaseName = "engine",
        .engineLogStrategy = logMode
    });
    setLoggerConfig(*m_loggerConfig);
}

void QaplaSettings::setPgnConfig(Settings::Manager& manager, const std::string& groupName) {
    auto pgnOptionInstance = manager.getGroupInstance(groupName);
    if (!pgnOptionInstance.has_value()) {
        m_pgnOptions = nullptr;
        return;
    }

    m_pgnOptions = std::make_unique<PgnSave::Options>(
        PgnConfig::fromManager(manager, groupName));
}

std::optional<PgnSave::Options> QaplaSettings::getPgnOptions() const {
    if (m_pgnOptions != nullptr) {
        return *m_pgnOptions;
    }
    return std::nullopt;
}

void QaplaSettings::setDrawAdjudicationConfig(Settings::Manager& manager, const std::string& groupName) {
    auto draw = manager.getGroupInstance(groupName);
    if (!draw.has_value()) {
        m_drawConfig = nullptr;
        return;
    }

    m_drawConfig = std::make_unique<AdjudicationManager::DrawAdjudicationConfig>(
        AdjudicationConfig::fromDrawManager(manager, groupName));
}

std::optional<AdjudicationManager::DrawAdjudicationConfig> QaplaSettings::getDrawAdjudicationConfig() const {
    if (m_drawConfig != nullptr) {
        return *m_drawConfig;
    }
    return std::nullopt;
}

void QaplaSettings::setResignAdjudicationConfig(Settings::Manager& manager, const std::string& groupName) {
    auto resign = manager.getGroupInstance(groupName);
    if (!resign.has_value()) {
        m_resignConfig = nullptr;
        return;
    }

    m_resignConfig = std::make_unique<AdjudicationManager::ResignAdjudicationConfig>(
        AdjudicationConfig::fromResignManager(manager, groupName));
}

std::optional<AdjudicationManager::ResignAdjudicationConfig> QaplaSettings::getResignAdjudicationConfig() const {
    if (m_resignConfig != nullptr) {
        return *m_resignConfig;
    }
    return std::nullopt;
}

void QaplaSettings::setOpenings(Settings::Manager& manager, const std::string& groupName) {
    auto opening = manager.getGroupInstance(groupName);
    if (!opening.has_value()) {
        m_openings = nullptr;
        return;
    }

    m_openings = std::make_unique<Openings>(
        OpeningConfig::fromManager(manager, groupName));
}

std::optional<Openings> QaplaSettings::getOpenings() const {
    if (m_openings == nullptr) {
        return std::nullopt;
    }
    return *m_openings;
}

void QaplaSettings::setTournamentConfig(Settings::Manager& manager, const std::string& groupName) {
    auto tournament = manager.getGroupInstance(groupName);
    if (!tournament.has_value()) {
        m_tournamentConfig = nullptr;
        return;
    }

    m_tournamentConfig = std::make_unique<TournamentConfig>(
        TournamentConfigFile::fromManager(manager, groupName));
    
    if (m_openings != nullptr) {
        m_tournamentConfig->openings = *m_openings;
    }
}

std::optional<TournamentConfig> QaplaSettings::getTournamentConfig() const {
    if (m_tournamentConfig == nullptr) {
        return std::nullopt;
    }
    return *m_tournamentConfig;
}

void QaplaSettings::setSprtConfig(Settings::Manager& manager, const std::string& groupName) {
    auto sprt = manager.getGroupInstance(groupName);
    if (!sprt.has_value()) {
        m_sprtConfig = nullptr;
        return;
    }

    m_sprtConfig = std::make_unique<SprtConfig>(
        SprtConfigFile::fromManager(manager, groupName));
    
    if (m_openings != nullptr) {
        m_sprtConfig->openings = *m_openings;
    }
}

std::optional<SprtConfig> QaplaSettings::getSprtConfig() const {
    if (m_sprtConfig == nullptr) {
        return std::nullopt;
    }
    return *m_sprtConfig;
}

void QaplaSettings::setEpdConfig() {
    auto epdGroup = Manager::instance().getGroupInstance("epd");
    if (!epdGroup.has_value()) {
        m_epdConfig = nullptr;
        return;
    }

    m_epdConfig = std::make_unique<EpdConfig>(EpdConfig{
        .file = epdGroup->get<std::string>("file"),
        .maxTime = epdGroup->get<unsigned int>("maxtime"),
        .minTime = epdGroup->get<unsigned int>("mintime"),
        .seenPlies = epdGroup->get<unsigned int>("seenplies"),
        .depth = epdGroup->get<unsigned int>("depth"),
        .nodes = epdGroup->get<unsigned int>("nodes"),
        .minSuccess = epdGroup->get<unsigned int>("minsuccess")
    });
}

std::optional<EpdConfig> QaplaSettings::getEpdConfig() const {
    if (m_epdConfig == nullptr) {
        return std::nullopt;
    }
    return *m_epdConfig;
}

void QaplaSettings::setSPSAConfig() {
    auto spsaGroup = Manager::instance().getGroupInstance("spsa");
    if (!spsaGroup.has_value()) {
        m_spsaConfig = nullptr;
        return;
    }

    m_spsaConfig = std::make_unique<SPSAConfig>();
    
    // Read basic SPSA configuration
    
    m_spsaConfig->maxActivePairs = spsaGroup->get<unsigned int>("activepairs");
    m_spsaConfig->learningRate = spsaGroup->get<double>("learningrate");
    m_spsaConfig->gamesPerPair = spsaGroup->get<unsigned int>("gamesperpair");
    m_spsaConfig->iterations = spsaGroup->get<unsigned int>("iterations");
    m_spsaConfig->outcomeInterval = spsaGroup->get<unsigned int>("outcomeinterval");
    m_spsaConfig->openingsSeed = spsaGroup->get<unsigned int>("seed");
    
    if (m_openings != nullptr) {
        m_spsaConfig->openingsFile = m_openings->file;
    }
    
    // Read all spsavalue groups to build parameter list
    auto spsaValueGroups = Manager::instance().getGroupInstances("spsavalue");
    for (const auto& valueGroup : spsaValueGroups) {
        SPSAParameterConfig param;
        param.name = valueGroup.get<std::string>("name");
        param.defaultValue = valueGroup.get<double>("default");
        param.minValue = valueGroup.get<double>("min");
        param.maxValue = valueGroup.get<double>("max");
        param.c = valueGroup.get<double>("step");
        
        m_spsaConfig->parameters.push_back(param);
    }

    // Validate that at least one parameter is defined
    if (m_spsaConfig->parameters.empty()) {
        m_spsaConfig = nullptr;
    }
}

std::optional<SPSAConfig> QaplaSettings::getSPSAConfig() const {
    if (m_spsaConfig == nullptr) {
        return std::nullopt;
    }
    return *m_spsaConfig;
}

void QaplaSettings::setCLOPConfig() {
    auto clopGroup = Manager::instance().getGroupInstance("clop");
    if (!clopGroup.has_value()) {
        m_clopConfig = nullptr;
        return;
    }

    m_clopConfig = std::make_unique<CLOPConfig>();
    m_clopConfig->maxActivePairs = clopGroup->get<unsigned int>("activepairs");
    m_clopConfig->samples = clopGroup->get<unsigned int>("samples");
    m_clopConfig->gamesPerSample = clopGroup->get<unsigned int>("gamespersample");
    m_clopConfig->warmupSamples = clopGroup->get<unsigned int>("warmupsamples");
    m_clopConfig->outcomeInterval = clopGroup->get<unsigned int>("outcomeinterval");
    m_clopConfig->trace = clopGroup->get<bool>("trace");
    m_clopConfig->maxWeightIterations = clopGroup->get<unsigned int>("maxweightiterations");
    m_clopConfig->h = clopGroup->get<double>("h");
    m_clopConfig->priorVariance = clopGroup->get<double>("priorvariance");
    m_clopConfig->openingsSeed = clopGroup->get<unsigned int>("seed");

    if (m_openings != nullptr) {
        m_clopConfig->openingsFile = m_openings->file;
    }

    auto clopValueGroups = Manager::instance().getGroupInstances("clopvalue");
    for (const auto& valueGroup : clopValueGroups) {
        CLOPParameterConfig parameter;
        parameter.name = valueGroup.get<std::string>("name");
        parameter.minValue = valueGroup.get<double>("min");
        parameter.maxValue = valueGroup.get<double>("max");
        m_clopConfig->parameters.push_back(parameter);
    }

    if (m_clopConfig->parameters.empty()) {
        m_clopConfig = nullptr;
    }
}

std::optional<CLOPConfig> QaplaSettings::getCLOPConfig() const {
    if (m_clopConfig == nullptr) {
        return std::nullopt;
    }
    return *m_clopConfig;
}

void QaplaSettings::setFromConfigData(const QaplaHelpers::ConfigData& configData, const std::string& /*id*/) {
    Settings::Manager::instance().parseInput(configData, false);
}

} // namespace QaplaTester::Settings


