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
#include "../opening/pgn-io.h"
#include "../base-elements/app-error.h"
#include "../base-elements/logger.h"
#include "../base-elements/ini-file.h"
#include "../opening/openings.h"
#include "../tournament/tournament.h"
#include "../sprt/sprt-manager.h"
#include "../sprt/sprt-tournament-file.h"
#include "../sprt/sprt-config-file.h"
#include "../config-file/opening-config.h"
#include "../config-file/pgn-config.h"
#include "../config-file/adjudication-config.h"
#include "../config-file/engine-config-file.h"
#include "../config-file/engine-global-config.h"
#include "../epd/epd-manager.h"
#include "../engine-handling/engine-worker-factory.h"
#include "../spsa/spsa-optimizer.h"

#include <format>
#include <fstream>

namespace QaplaTester::Settings {

QaplaSettings& QaplaSettings::instance() {
    static QaplaSettings instance;
    return instance;
}

void QaplaSettings::applyArguments(const std::vector<std::string>& args) {
    // Convert CLI arguments to ConfigData
    auto cliData = QaplaHelpers::ConfigData::fromArgv(args);
    
    // Check for settingsfile in cliglobal section
    auto cliglobalSections = cliData.getSectionList("cliglobal", "default");
    std::string settingsFile;
    if (cliglobalSections) {
        for (const auto& section : *cliglobalSections) {
            auto fileValue = section.getValue("settingsfile");
            if (fileValue) {
                settingsFile = *fileValue;
                break;
            }
        }
    }
    
    // If settings file specified, load and merge
    QaplaHelpers::ConfigData mergedData = cliData;
    if (!settingsFile.empty()) {
        std::ifstream file(settingsFile);
        if (!file.is_open()) {
            throw AppError::makeInvalidParameters("Failed to open settings file: " + settingsFile);
        }
        QaplaHelpers::ConfigData fileData;
        fileData.load(file);
        mergedData = fileData.merge(cliData);
    }
    
    // Parse the merged configuration
    Manager::instance().parseInput(mergedData);

    // Read options after all settings are registered and read.
    readLoggerConfig();
    readEngineOptions();
    readEngineGlobalConfig();
    readPgnOptions();
    readDrawAdjudicationConfig();
    readResignAdjudicationConfig();
    readOpenings();
    readTournamentConfig();
    readSprtConfig();
    readEpdConfig();
    readSPSAConfig();
}

const std::vector<std::string>& QaplaSettings::getArguments() const {
    return m_arguments;
}

const LoggerConfig* QaplaSettings::getLoggerConfig() const {
    return m_loggerConfig.get();
}

void QaplaSettings::applyLoggerConfig(const std::string& reportLogBaseName) const {
    if (!m_loggerConfig) {
        throw AppError::make("Logger configuration not initialized.");
    }
    LoggerConfig config = *m_loggerConfig;
    config.reportLogBaseName = reportLogBaseName;
    setLoggerConfig(config);
    auto loggingSetting = Settings::Manager::instance().getGroupInstance("logging");
    if (loggingSetting->get<bool>("engine")) {
        EngineLogger::engineLogger().setTraceLevel(TraceLevel::error, TraceLevel::info);
    } else {
        EngineLogger::engineLogger().setTraceLevel(TraceLevel::none, TraceLevel::none);
    }
    Logger::reportLogger().setTraceLevel(TraceLevel::result, TraceLevel::result);
}

std::vector<std::string> QaplaSettings::argvToVector(int argc, char* argv[]) {
    std::vector<std::string> result;
    for (int i = 0; i < argc; ++i) {
        result.emplace_back(argv[i]);
    }
    return result;
}

void QaplaSettings::init() {
    // Global settings
    Manager::instance().registerSetting("interactive", "Enables interactive mode", false, false, ValueType::Bool);
    Manager::instance().registerSetting("concurrency", "Maximal number of in parallel running engines", true, 10,
        ValueType::UInt);
    Manager::instance().registerSetting("rapid", "Ignores engine info output. Speeds up games with <10s total compute time",
        false, false, ValueType::Bool);
    Manager::instance().registerSetting("enginesfile", "Path to an ini file with engine configurations", false, "",
        ValueType::PathExists);
    Manager::instance().registerSetting("settingsfile", "Path to a settings file in INI-style format", false, std::string(""),
        ValueType::PathExists);

    // Engine group
    Manager::instance().registerGroup("engine", "Defines an engine configuration", false, Settings::getEngineKeys());

    // Logging group
    Manager::instance().registerGroup("logging", "Logger configuration", true, Settings::getLoggingKeys());

    // Each group
    Manager::instance().registerGroup("each", "Defines configuration options for all engines", false, Settings::getEachKeys());

    // EPD group
    Manager::instance().registerGroup("epd", "Configuration to run an epd testset against engines", false, Settings::getEpdKeys());

    // SPRT group
    Manager::instance().registerGroup("sprt", "Sequential Probability Ratio Test configuration", true, Settings::getSprtKeys());
    // Openings group
    Manager::instance().registerGroup("openings", "Defines how start positions are selected", true, Settings::getOpeningsKeys());

    // Test group
    Manager::instance().registerGroup("test", "Test the engine", true, Settings::getTestKeys());

    // PGN output group
    Manager::instance().registerGroup("pgnoutput", "PGN output settings", true, Settings::getPgnOutputKeys());

    // Tournament group
    Manager::instance().registerGroup("tournament", "Tournament setup and general parameters", true, Settings::getTournamentKeys());

    // Draw adjudication group
    Manager::instance().registerGroup("draw", "Draw adjudication settings", true, Settings::getDrawAdjudicationKeys());

    // Resign adjudication group
    Manager::instance().registerGroup("resign", "Resign adjudication settings", true, Settings::getResignAdjudicationKeys());
    // SPSA optimization group
    Manager::instance().registerGroup("spsa", "SPSA parameter optimization configuration", true, Settings::getSpsaKeys());

    // SPSA parameter value group
    Manager::instance().registerGroup("spsavalue", "Defines a single parameter to optimize with SPSA", false, Settings::getSpsaValueKeys());
}

void QaplaSettings::readLoggerConfig() {
    auto loggingSetting = Manager::instance().getGroupInstance("logging");
    
    std::string logPath = "./log";
    std::string logModeStr = "one";
    
    if (loggingSetting) {
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
}

void QaplaSettings::readEngineOptions() {
	EngineWorkerFactory::setSuppressInfoLines(Settings::Manager::instance().get<bool>("rapid"));
    std::string enginesFile = Settings::Manager::instance().get<std::string>("enginesfile");
    if (!enginesFile.empty()) {
        EngineWorkerFactory::getConfigManagerMutable().loadFromFile(enginesFile);
    }
    auto engineSettings = Settings::Manager::instance().getGroupInstances("engine");
	auto eachSetting = Settings::Manager::instance().getGroupInstance("each");
    auto loggingSetting = Manager::instance().getGroupInstance("logging");
        
	Settings::ValueMap eachOptions;
	if (eachSetting) {
		eachOptions = eachSetting->getValues();
	}

    for (const auto& engine : engineSettings) {
        // Merge global options with engine-specific options (engine options take precedence)
        Settings::ValueMap mergedOptions = engine.getValues();
        mergedOptions.insert(eachOptions.begin(), eachOptions.end());

        // Check if cmd or conf is specified
        auto cmdIt = mergedOptions.find("cmd");
        auto confIt = mergedOptions.find("conf");
        auto nameIt = mergedOptions.find("name");
        
        std::string cmd = (cmdIt != mergedOptions.end() && std::holds_alternative<std::string>(cmdIt->second)) 
            ? std::get<std::string>(cmdIt->second) : "";
        std::string conf = (confIt != mergedOptions.end() && std::holds_alternative<std::string>(confIt->second))
            ? std::get<std::string>(confIt->second) : "";
        std::string name = (nameIt != mergedOptions.end() && std::holds_alternative<std::string>(nameIt->second))
            ? std::get<std::string>(nameIt->second) : "";

        if (loggingSetting && !loggingSetting->get<bool>("engine")) {
            mergedOptions["trace"] = std::string("none");
        }

        EngineConfig config;

        if (!cmd.empty()) {
            config = EngineConfig::createFromValueMap(mergedOptions);
            EngineWorkerFactory::getActiveEnginesMutable().push_back(config);
        }
        else if (!conf.empty()) {
            auto engineConfig = EngineWorkerFactory::getConfigManager().getConfig(conf);
            if (!engineConfig) {
                throw AppError::makeInvalidParameters("Engine configuration '" + conf + "' not found.");
            }
            config = *engineConfig;
            config.setCommandLineOptions(mergedOptions, true);
            EngineWorkerFactory::getActiveEnginesMutable().push_back(config);
        }
        else {
            std::string engineName = name.empty() ? "" : " (for " + name + ")";
            throw AppError::makeInvalidParameters("No engine command or configuration provided"
                + engineName + ". Please specify either 'cmd' or 'conf'.");
        }
    }
    // Ensure that all active engines have different names
    EngineWorkerFactory::assignUniqueDisplayNames();
}

void QaplaSettings::readEngineGlobalConfig() {
    // This function creates m_engineGlobalConfig from [each] settings for GUI compatibility.
    // It is NOT used when loading engines from CLI parameters in readEngineOptions().
    // m_engineGlobalConfig is only applied when loading GUI-based tournament files via
    // setFromConfigData(), where it overrides individual engine settings.
    constexpr uint32_t defaultHashSizeMB = 32;
    auto eachSetting = Manager::instance().getGroupInstance("each");
    if (!eachSetting) {
        m_engineGlobalConfig = nullptr;
        return;
    }

    auto eachSettings = Settings::Manager::instance().getGroupInstances("each");
    const auto& each = *eachSetting;
    auto hashProvided = each.isKeyProvided("option.Hash");
    uint32_t hashValue = defaultHashSizeMB;
    if (hashProvided) {
        auto hashStr = each.get<std::string>("option.Hash");
        hashValue = QaplaHelpers::to_unsigned_int<uint32_t>(hashStr).value_or(defaultHashSizeMB);
    }

    m_engineGlobalConfig = std::make_unique<EngineGlobalConfig>(EngineGlobalConfig{
        .useGlobalHash = hashProvided,
        .hashSizeMB = hashValue,
        .useGlobalPonder = each.isKeyProvided("ponder"),
        .ponder = each.get<bool>("ponder"),
        .useGlobalTrace = each.isKeyProvided("trace"),
        .traceLevel = each.get<std::string>("trace"),
        .useGlobalRestart = each.isKeyProvided("restart"),
        .restart = each.get<std::string>("restart"),
        .timeControl = each.get<std::string>("tc")
    });
    
    applyEngineLoggingToGlobalConfig();
}

void QaplaSettings::readPgnOptions() {
    auto pgnOptionInstance = Manager::instance().getGroupInstance("pgnoutput");
    if (!pgnOptionInstance) {
        m_pgnOptions = nullptr;
        return;
    }

    const auto& pgn = *pgnOptionInstance;
    m_pgnOptions = std::make_unique<PgnSave::Options>(PgnSave::Options{
        .file = pgn.get<std::string>("file"),
        .append = pgn.get<bool>("append"),
        .onlyFinishedGames = pgn.get<bool>("fi"),
        .minimalTags = pgn.get<bool>("min"),
        .includeClock = pgn.get<bool>("clock"),
        .includeEval = pgn.get<bool>("eval"),
        .includePv = pgn.get<bool>("pv"),
        .includeDepth = pgn.get<bool>("depth")
    });
}

std::optional<PgnSave::Options> QaplaSettings::getPgnOptions() const {
    if (m_pgnOptions) {
        return *m_pgnOptions;
    }
    return std::nullopt;
}

void QaplaSettings::readDrawAdjudicationConfig() {
    auto draw = Manager::instance().getGroupInstance("draw");
    if (!draw) {
        m_drawConfig = nullptr;
        return;
    }

    m_drawConfig = std::make_unique<AdjudicationManager::DrawAdjudicationConfig>(AdjudicationManager::DrawAdjudicationConfig{
        .minFullMoves = draw->get<unsigned int>("movenumber"),
        .requiredConsecutiveMoves = draw->get<unsigned int>("movecount"),
        .centipawnThreshold = draw->get<int>("score"),
        .testOnly = draw->get<bool>("test")
    });
}

std::optional<AdjudicationManager::DrawAdjudicationConfig> QaplaSettings::getDrawAdjudicationConfig() const {
    if (m_drawConfig) {
        return *m_drawConfig;
    }
    return std::nullopt;
}

void QaplaSettings::readResignAdjudicationConfig() {
    auto resign = Manager::instance().getGroupInstance("resign");
    if (!resign) {
        m_resignConfig = nullptr;
        return;
    }

    m_resignConfig = std::make_unique<AdjudicationManager::ResignAdjudicationConfig>(AdjudicationManager::ResignAdjudicationConfig{
        .requiredConsecutiveMoves = resign->get<unsigned int>("movecount"),
        .centipawnThreshold = resign->get<int>("score"),
        .testOnly = resign->get<bool>("test")
    });
}

std::optional<AdjudicationManager::ResignAdjudicationConfig> QaplaSettings::getResignAdjudicationConfig() const {
    if (m_resignConfig) {
        return *m_resignConfig;
    }
    return std::nullopt;
}

void QaplaSettings::readOpenings() {
    auto opening = Manager::instance().getGroupInstance("openings");
    if (!opening) {
        m_openings = nullptr;
        return;
    }

    const auto pliesStr = opening->get<std::string>("plies");
    std::optional<int> plies;

    if (pliesStr != "all") {
        try {
            int val = std::stoi(pliesStr);
            if (val < 0) {
                throw AppError::makeInvalidParameters("Openings: Ply count must be at least 0, but got " + pliesStr);
            }
            plies = val - 1; // intern 0-basiert
        }
        catch (const std::exception&) {
            throw AppError::makeInvalidParameters(
                "Openings: Ply count must be a non-negative integer or \"all\", but got: \"" + pliesStr + "\"");
        }
    }

    auto openings = std::make_unique<Openings>(Openings{
        .file = opening->get<std::string>("file"),
        .order = opening->get<std::string>("order"),
        .plies = plies,
        .start = opening->get<unsigned int>("start"),
        .seed = opening->get<unsigned int>("srand"),
        .policy = opening->get<std::string>("policy")
    });

    if (openings->start < 1) {
        throw AppError::makeInvalidParameters("Openings: Start index must be at least 1, but got " +
            std::to_string(openings->start));
    }
    openings->start--; // 0-based

    if (openings->order != "sequential" && openings->order != "random") {
        throw AppError::makeInvalidParameters("Unsupported openings order: " + openings->order);
    }
    if (openings->policy != "default" && openings->policy != "encounter" && openings->policy != "round") {
        throw AppError::makeInvalidParameters("Unsupported openings policy: " + openings->policy);
    }

    m_openings = std::move(openings);
}

std::optional<Openings> QaplaSettings::getOpenings() const {
    if (!m_openings) {
        return std::nullopt;
    }
    return *m_openings;
}

void QaplaSettings::readTournamentConfig() {
    auto tournamentGroup = Manager::instance().getGroupInstance("tournament");
    if (!tournamentGroup) {
        m_tournamentConfig = nullptr;
        return;
    }

    // Tournament needs openings
    if (!m_openings) {
        m_tournamentConfig = nullptr;
        return;
    }

    m_tournamentConfig = std::make_unique<TournamentConfig>(TournamentConfig{
        .event = tournamentGroup->get<std::string>("event"),
        .type = tournamentGroup->get<std::string>("type"),
        .tournamentFilename = tournamentGroup->get<std::string>("resultfile"),
        .saveInterval = tournamentGroup->get<unsigned int>("saveinterval"),
        .games = tournamentGroup->get<unsigned int>("games"),
        .rounds = tournamentGroup->get<unsigned int>("rounds"),
        .repeat = tournamentGroup->get<unsigned int>("repeat"),
        .ratingInterval = tournamentGroup->get<unsigned int>("ratinginterval"),
        .outcomeInterval = tournamentGroup->get<unsigned int>("outcomeinterval"),
        .averageElo = tournamentGroup->get<int>("averageelo"),
        .noSwap = tournamentGroup->get<bool>("noswap"),
        .openings = *m_openings
    });
}

std::optional<TournamentConfig> QaplaSettings::getTournamentConfig() const {
    if (!m_tournamentConfig) {
        return std::nullopt;
    }
    return *m_tournamentConfig;
}

void QaplaSettings::readSprtConfig() {
    auto sprt = Manager::instance().getGroupInstance("sprt");
    if (!sprt) {
        m_sprtConfig = nullptr;
        return;
    }

    auto sprtFile = sprt->get<std::string>("file");
    if (!sprtFile.empty()) {
        QaplaHelpers::ConfigData configData;
        SprtTournamentFile::load(sprtFile, configData, "sprt-tournament");
        setFromConfigData(configData, "sprt-tournament");
        return;
    }

    // SPRT needs openings (unless montecarlo)
    auto isMontecarlo = sprt->get<bool>("montecarlo");
    if (!m_openings && !isMontecarlo && sprtFile.empty()) {
        m_sprtConfig = nullptr;
        return;
    }

    m_sprtConfig = std::make_unique<SprtConfig>(SprtConfig{
        .eloUpper = static_cast<float>(sprt->get<int>("eloUpper")),
        .eloLower = static_cast<float>(sprt->get<int>("eloLower")),
        .alpha = sprt->get<double>("alpha"),
        .beta = sprt->get<double>("beta"),
        .maxGames = sprt->get<unsigned int>("maxgames"),
        .model = sprt->get<std::string>("model"),
        .pentanomial = sprt->get<bool>("pentanomial"),
        .openings = m_openings ? *m_openings : Openings{}
    });

}

std::optional<SprtConfig> QaplaSettings::getSprtConfig() const {
    if (!m_sprtConfig) {
        return std::nullopt;
    }
    return *m_sprtConfig;
}

void QaplaSettings::readEpdConfig() {
    auto epdGroup = Manager::instance().getGroupInstance("epd");
    if (!epdGroup) {
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
    if (!m_epdConfig) {
        return std::nullopt;
    }
    return *m_epdConfig;
}

void QaplaSettings::readSPSAConfig() {
    auto spsaGroup = Manager::instance().getGroupInstance("spsa");
    if (!spsaGroup) {
        m_spsaConfig = nullptr;
        return;
    }

    // SPSA needs openings
    if (!m_openings) {
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
    m_spsaConfig->openingsFile = m_openings->file;
    
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
    if (!m_spsaConfig) {
        return std::nullopt;
    }
    return *m_spsaConfig;
}

void QaplaSettings::applyEngineLoggingToGlobalConfig() {
    if (!m_engineGlobalConfig) {
        return;
    }
    
    auto loggingSetting = Manager::instance().getGroupInstance("logging");
    if (loggingSetting && !loggingSetting->get<bool>("engine")) {
        m_engineGlobalConfig->useGlobalTrace = true;
        m_engineGlobalConfig->traceLevel = "none";
    }
}

void QaplaSettings::setFromConfigData(const QaplaHelpers::ConfigData& configData, const std::string& id) {
    // Apply SPRT configuration
    auto sprtConfig = SprtConfigFile::fromConfigData(configData, id);
    if (sprtConfig) {
        m_sprtConfig = std::make_unique<SprtConfig>(*sprtConfig);
    }

    // Apply Openings configuration
    auto openings = OpeningConfig::fromConfigData(configData, id);
    if (openings) {
        m_openings = std::make_unique<Openings>(*openings);
        if (m_sprtConfig) {
            m_sprtConfig->openings = *openings;
        }
    }

    // Apply PGN configuration
    auto pgnOptions = PgnConfig::fromConfigData(configData, id);
    if (pgnOptions) {
        m_pgnOptions = std::make_unique<PgnSave::Options>(*pgnOptions);
    }

    // Apply Draw Adjudication configuration
    auto drawConfig = AdjudicationConfig::fromDrawConfigData(configData, id);
    if (drawConfig) {
        m_drawConfig = std::make_unique<AdjudicationManager::DrawAdjudicationConfig>(*drawConfig);
    }

    // Apply Resign Adjudication configuration
    auto resignConfig = AdjudicationConfig::fromResignConfigData(configData, id);
    if (resignConfig) {
        m_resignConfig = std::make_unique<AdjudicationManager::ResignAdjudicationConfig>(*resignConfig);
    }

    // Load global engine configuration
    auto globalConfig = EngineGlobalConfigFile::fromConfigData(configData, id);
    if (globalConfig) {
        m_engineGlobalConfig = std::make_unique<EngineGlobalConfig>(*globalConfig);
    }
    
    applyEngineLoggingToGlobalConfig();
    
    // Load engine-specific configurations
    auto engineConfigs = EngineConfigFile::fromConfigData(configData, id);
    
    if (engineConfigs && !engineConfigs->empty()) {
        // Store ALL engine configurations (selected and non-selected)
        m_allEngineConfigurations = *engineConfigs;
        
        // Clear existing engines and add new ones
        EngineWorkerFactory::getActiveEnginesMutable().clear();
        
        for (auto& engineConfiguration : *engineConfigs) {
            // Only add engines that are selected for the tournament
            if (!engineConfiguration.selected) {
                continue;
            }
            
            // Apply global configuration to each engine if available
            if (globalConfig) {
                EngineGlobalConfigFile::applyGlobalConfig(engineConfiguration.config, *globalConfig);
            }
            
            EngineWorkerFactory::getActiveEnginesMutable().push_back(engineConfiguration.config);
        }
        
        EngineWorkerFactory::assignUniqueDisplayNames();
    }
}

const std::vector<EngineConfiguration>& QaplaSettings::getAllEngineConfigurations() const {
    return m_allEngineConfigurations;
}

QaplaHelpers::ConfigData QaplaSettings::getConfigData(const std::string& id) const {
    QaplaHelpers::ConfigData configData;

    // Convert Engine Global configuration back to sections
    if (m_engineGlobalConfig) {
        auto engineSections = EngineGlobalConfigFile::toEngineConfigSections(*m_engineGlobalConfig, id);
        configData.setSectionList("eachengine", id, engineSections);
        
        auto timeControlSections = EngineGlobalConfigFile::toTimeControlSections(*m_engineGlobalConfig, id);
        configData.setSectionList("timecontroloptions", id, timeControlSections);
    }

    if (m_openings) {
        auto sections = OpeningConfig::toSections(*m_openings, id);
        configData.setSectionList(OpeningConfig::getSectionName(), id, sections);
    }

    if (m_sprtConfig) {
        auto sections = SprtConfigFile::toSections(*m_sprtConfig, id);
        configData.setSectionList(SprtConfigFile::getSectionName(), id, sections);
    }

    if (m_pgnOptions) {
        auto sections = PgnConfig::toSections(*m_pgnOptions, id);
        configData.setSectionList(PgnConfig::getSectionName(), id, sections);
    }

    if (m_drawConfig) {
        auto sections = AdjudicationConfig::toDrawSections(*m_drawConfig, id);
        configData.setSectionList(AdjudicationConfig::getDrawSectionName(), id, sections);
    }

    if (m_resignConfig) {
        auto sections = AdjudicationConfig::toResignSections(*m_resignConfig, id);
        configData.setSectionList(AdjudicationConfig::getResignSectionName(), id, sections);
    }


    if (!m_allEngineConfigurations.empty()) {
        auto sections = EngineConfigFile::toSections(m_allEngineConfigurations, id);
        configData.setSectionList(EngineConfigFile::getSectionName(), id, sections);
    }

    return configData;
}

} // namespace QaplaTester::CliSettings


