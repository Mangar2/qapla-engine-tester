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
#include "../config/engine-config.h"
#include "../epd/epd-manager.h"
#include "../engine-handling/engine-worker-factory.h"
#include "../spsa/spsa-optimizer.h"
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
    auto settingsFile = Manager::instance().get<std::string>("settingsfile");
    if (!settingsFile.empty()) {
        std::ifstream file(settingsFile);
        if (!file.is_open()) {
            throw AppError::makeInvalidParameters(std::format("Failed to open settings file: {}", settingsFile));
        }
        QaplaHelpers::ConfigData fileData;
        fileData.load(file);
        configData_.push_back(fileData);
        // Refresh manager to see if mcp or other files are there
        Manager::instance().parseInput(fileData, false);
    }

    // 3. Add CLI arguments to configData_ (CLI overrides settings file)
    configData_.push_back(cliData);

    // 4. Apply mcp environment layer if needed (MCP layer overrides CLI)
    bool isMcp = Manager::instance().get<bool>("mcp");
    if (isMcp) {
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
        if (isMcp) {
            throw AppError::makeInvalidParameters("Continuing a tournament/SPRT run from file is not supported in MCP mode.");
        }
        
        if (!sprtFile.empty()) {
            std::ifstream file(sprtFile);
            if (file.is_open()) {
                QaplaHelpers::ConfigData sprtData;
                sprtData.load(file);
                configData_.push_back(sprtData);
                Manager::instance().parseInput(sprtData, true); // Overwrite to find tournament file if nested
            }
        }
        
        if (!tournamentFile.empty()) {
            std::ifstream file(tournamentFile);
            if (file.is_open()) {
                QaplaHelpers::ConfigData tourneyData;
                tourneyData.load(file);
                configData_.push_back(tourneyData);
            }
        }
    }

    if (isMcp) {
        Mcp::McpServer::initialize();
    }

    // Initial apply to populate all internal members
    applyConfig(std::nullopt);

    // 6. Initialize engines only once
    setEngineConfig(Manager::instance(), "engine");
}

void QaplaSettings::applyConfig(std::optional<QaplaHelpers::ConfigData> configData) {
    Manager::instance().clearValues();
    
    // Apply initial config vector (later entries override earlier ones)
    for (const auto& cfg : configData_) {
        Manager::instance().parseInput(cfg, true);
    }
    
    // Apply dynamic configuration if provided
    if (configData.has_value()) {
        Manager::instance().parseInput(*configData, true);
    }
    
    // Validate all settings for completeness
    Manager::instance().validateCompleteness();

    // Check concurrency is not zero
    if (Manager::instance().get<unsigned int>("concurrency") == 0) {
        throw AppError::makeInvalidParameters("Concurrency must be at least 1.");
    }

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
    
    if (loggingSetting.has_value()) {
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
    // Global settings
    Manager::instance().registerSetting({
        .name = "interactive", 
        .description = "Enables interactive mode", 
        .isRequired = false, 
        .defaultValue = false, 
        .type = ValueType::Bool
    });

    Manager::instance().registerSetting({
        .name = "mcp", 
        .description = "Enables Model Context Protocol (MCP) server mode", 
        .isRequired = false, 
        .defaultValue = false, 
        .type = ValueType::Bool
    });
    
    Manager::instance().registerSetting({
        .name = "concurrency", 
        .description = "Maximal number of in parallel running engines", 
        .isRequired = true, 
        .defaultValue = 10,
        .type = ValueType::UInt
    });
    
    Manager::instance().registerSetting({
        .name = "rapid", 
        .description = "Enables rapid mode (suppresses engine info lines)",
        .isRequired = false, 
        .defaultValue = false, 
        .type = ValueType::Bool
    });

    Manager::instance().registerSetting({
        .name = "engines", 
        .description = "Comma separated list of engine names to use", 
        .isRequired = false, 
        .defaultValue = std::string(""),
        .type = ValueType::String
    });

    Manager::instance().registerSetting({
        .name = "enginesfile", 
        .description = "Path to an ini file with engine configurations", 
        .isRequired = false, 
        .defaultValue = "",
        .type = ValueType::PathExists
    });

    Manager::instance().registerSetting({
        .name = "settingsfile", 
        .description = "Path to a settings file in INI-style format", 
        .isRequired = false, 
        .defaultValue = std::string(""),
        .type = ValueType::PathExists
    });

    Manager::instance().registerGroup({
        .name = "engine", 
        .description = "Defines an engine configuration", 
        .unique = false, 
        .primaryKey = {"name", "conf"},
        .keys = Settings::getEngineKeys()}
    );

    // Logging group
    Manager::instance().registerGroup({
        .name = "logging", 
        .description = "Logger configuration", 
        .unique = true, 
        .keys = Settings::getLoggingKeys()
    });

    // Each group
    Manager::instance().registerGroup({
        .name = "each", 
        .description = "Defines configuration options for all engines", 
        .unique = true, 
        .keys = Settings::getEachKeys()
    });

    // EPD group
    Manager::instance().registerGroup({
        .name = "epd", 
        .description = "Configuration to run an epd testset against engines", 
        .longDescription = R"(Runs an EPD (Extended Position Description) testset.
Each engine analyzes a set of positions and its performance is measured by how many 'best moves' it finds within the time limit.
Results are reported as a success rate and compared against a minimum threshold.)",
        .unique = true, 
        .keys = Settings::getEpdKeys()
    });

    // SPRT group
    Manager::instance().registerGroup({
        .name = "sprt", 
        .description = "Sequential Probability Ratio Test configuration", 
        .longDescription = R"(Runs a Sequential Probability Ratio Test (SPRT) between two engines. 
SPRT is an efficient method to determine if one engine is stronger than another with statistical confidence.

Typical SPRT configurations:
- **Small Improvement**: alpha=0.05, beta=0.05, eloupper=5, elolower=0. Checks if engine 1 is at least 5 Elo stronger.
- **Strong Improvement**: alpha=0.05, beta=0.05, eloupper=10, elolower=0. Checks if engine 1 is at least 10 Elo stronger.
- **Regression Testing**: alpha=0.05, beta=0.05, eloupper=0, elolower=-5. Checks if engine 1 is at least not more than 5 Elo weaker.

The test stops as soon as H0 (no difference or weaker) or H1 (stronger) is accepted.)",
        .unique = true, 
        .keys = Settings::getSprtKeys()
    });
    // Openings group
    Manager::instance().registerGroup({
        .name = "openings", 
        .description = "Defines how start positions are selected", 
        .unique = true, 
        .keys = Settings::getOpeningsKeys()
    });

    // Test group
    Manager::instance().registerGroup({
        .name = "test", 
        .description = "Test the engine", 
        .unique = true, 
        .keys = Settings::getTestKeys()
    });

    // PGN output group
    Manager::instance().registerGroup({
        .name = "pgnoutput", 
        .description = "PGN output settings", 
        .unique = true, 
        .keys = Settings::getPgnOutputKeys()
    });

    // Tournament group
    Manager::instance().registerGroup({
        .name = "tournament", 
        .description = "Tournament setup and general parameters", 
        .longDescription = R"(Runs a tournament between multiple engines.
Pairings are generated based on the tournament type (e.g., round-robin or gauntlet).
Engines play against each other with color swapping and opening variations.)",
        .unique = true, 
        .keys = Settings::getTournamentKeys()
    });

    // Draw adjudication group
    Manager::instance().registerGroup({
        .name = "draw", 
        .description = "Draw adjudication settings", 
        .unique = true, 
        .keys = Settings::getDrawAdjudicationKeys()
    });

    // Resign adjudication group
    Manager::instance().registerGroup({
        .name = "resign", 
        .description = "Resign adjudication settings", 
        .unique = true, 
        .keys = Settings::getResignAdjudicationKeys()
    });

    // SPSA optimization group
    Manager::instance().registerGroup({
        .name = "spsa", 
        .description = "SPSA parameter optimization configuration", 
        .longDescription = R"(Optimizes engine parameters using the Simultaneous Perturbation Stochastic Approximation (SPSA) algorithm.
Parameters are perturbed in multiple iterations to find the optimal values that maximize playing strength.
Requires defining specific parameters to optimize using the 'spsavalue' group.)",
        .unique = true, 
        .keys = Settings::getSpsaKeys()
    });

    // SPSA parameter value group
    Manager::instance().registerGroup({
        .name = "spsavalue", 
        .description = "Defines a single parameter to optimize with SPSA", 
        .unique = false, 
        .keys = Settings::getSpsaValueKeys()
    });
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

void QaplaSettings::setEngineConfig(Settings::Manager& manager, const std::string& groupName) {
	m_rapid = manager.get<bool>("rapid");
    GameManagerPool::getInstance().setRapid(m_rapid);
	EngineWorkerFactory::setRapid(m_rapid);
    auto enginesFile = manager.get<std::string>("enginesfile");
    if (!enginesFile.empty()) {
        EngineWorkerFactory::getConfigManagerMutable().loadFromFile(enginesFile);
    }

    auto engineNamesStr = manager.get<std::string>("engines");
    if (!engineNamesStr.empty()) {
        applyEngineList(engineNamesStr);
        return;
    }

    auto engineSettings = manager.getGroupInstances(groupName);
	auto eachSetting = manager.getGroupInstance("each");
    auto loggingSetting = manager.getGroupInstance("logging");

    for (const auto& engine : engineSettings) {
        applyEngineInstance(engine, eachSetting.has_value() ? &(*eachSetting) : nullptr, 
            loggingSetting.has_value() ? &(*loggingSetting) : nullptr);
    }
    // Name conflicts would cause ambiguity in tournament results
    EngineWorkerFactory::assignUniqueDisplayNames();
}

void QaplaSettings::applyEngineList(const std::string& engineNamesStr) {
    auto engineNames = QaplaHelpers::split(engineNamesStr, ',');
    for (auto& name : engineNames) {
        name = QaplaHelpers::trim(name);
        if (name.empty()) {
            continue;
        }
        const auto* engineConfig = EngineWorkerFactory::getConfigManager().getConfig(name);
        if (engineConfig == nullptr) {
            throw AppError::makeInvalidParameters("Engine configuration '" + name + "' not found.");
        }
        EngineWorkerFactory::getActiveEnginesMutable().push_back(*engineConfig);
    }
    EngineWorkerFactory::assignUniqueDisplayNames();
}

void QaplaSettings::applyEngineInstance(const Settings::GroupInstance& instance,
                                         const Settings::GroupInstance* eachSetting,
                                         const Settings::GroupInstance* loggingSetting) {
    // instance.mergeWithDefaults(each) ensures per-engine settings take precedence over global [each] defaults
    Settings::GroupInstance mergedInstance = (eachSetting != nullptr) 
        ? instance.mergeWithDefaults(*eachSetting) 
        : instance;

    auto cmd = mergedInstance.get<std::string>("cmd");
    auto conf = mergedInstance.get<std::string>("conf");
    auto name = mergedInstance.get<std::string>("name");

    // Logging is configured per engine, requiring global logging settings to be applied individually
    Settings::ValueMap finalOptions = mergedInstance.getValues();
    if (loggingSetting != nullptr && !loggingSetting->get<bool>("engine")) {
        finalOptions["trace"] = std::string("none");
    }

    if (!cmd.empty()) {
        // Using executable path (cmd) to create a new EngineConfig from scratch
        auto config = EngineConfig::createFromValueMap(finalOptions);
        EngineWorkerFactory::getActiveEnginesMutable().push_back(config);
    }
    else if (!conf.empty()) {
        // Using named configuration reference (conf), loading it and overlaying command-line options
        const auto* engineConfig = EngineWorkerFactory::getConfigManager().getConfig(conf);
        if (engineConfig == nullptr) {
            throw AppError::makeInvalidParameters("Engine configuration '" + conf + "' not found.");
        }
        auto config = *engineConfig;
        config.setCommandLineOptions(finalOptions, true);
        EngineWorkerFactory::getActiveEnginesMutable().push_back(config);
    }
    else {
        std::string engineName = name.empty() ? "" : " (for " + name + ")";
        throw AppError::makeInvalidParameters("No engine command or configuration provided"
            + engineName + ". Please specify either 'cmd' or 'conf'.");
    }
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
    m_spsaConfig->openingsSeed = spsaGroup->get<unsigned int>("seed");
    m_spsaConfig->swapColors = !spsaGroup->get<bool>("noswap");
    
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

void QaplaSettings::setFromConfigData(const QaplaHelpers::ConfigData& configData, const std::string& /*id*/) {
    Settings::Manager::instance().parseInput(configData, false);
}

const std::vector<EngineConfiguration>& QaplaSettings::getAllEngineConfigurations() const {
    return m_allEngineConfigurations;
}

} // namespace QaplaTester::CliSettings


