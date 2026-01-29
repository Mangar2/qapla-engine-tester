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

#include "settings-manager.h"
#include "../opening/pgn-save.h"
#include "../game-manager/adjudication-manager.h"
#include "../config/engine-config.h"
#include "../config/engine-global-config.h"
#include "../base-elements/logger.h"

#include <string>
#include <vector>
#include <optional>
#include <memory>

namespace QaplaTester {
    struct Openings;
    struct TournamentConfig;
    struct SprtConfig;
    struct EpdConfig;
    struct SPSAConfig;

}

namespace QaplaHelpers {
    class ConfigData;
}

namespace QaplaTester::Settings {

/**
 * @brief Settings management class for Qapla Engine Tester
 * 
 * This class manages command-line arguments and initializes all CLI settings.
 * It uses the singleton pattern with a static instance() method.
 */
class QaplaSettings {
public:
    /**
     * @brief Gets the static instance of QaplaSettings
     * @return Reference to the static instance
     */
    [[nodiscard]] static QaplaSettings& instance();

    /**
     * @brief Sets the command-line arguments
     * @param args Vector of command-line arguments
     */
    void applyArguments(const std::vector<std::string>& args);

    /**
     * @brief Gets the stored command-line arguments
     * @return Const reference to the arguments vector
     */
    [[nodiscard]] const std::vector<std::string>& getArguments() const;

    /**
     * @brief Initializes all CLI settings by registering them with the Manager
     * 
     * This method registers all settings and groups used by the application,
     * including global settings, engine configurations, test settings, etc.
     */
    static void init();

    /**
     * @brief Gets the PGN output options
     * @return Optional containing PGN options if configured, nullopt otherwise
     */
    [[nodiscard]] std::optional<PgnSave::Options> getPgnOptions() const;

    /**
     * @brief Gets the draw adjudication options
     * @return Optional containing draw adjudication config if configured, nullopt otherwise
     */
    [[nodiscard]] std::optional<AdjudicationManager::DrawAdjudicationConfig> getDrawAdjudicationConfig() const;

    /**
     * @brief Gets the resign adjudication options
     * @return Optional containing resign adjudication config if configured, nullopt otherwise
     */
    [[nodiscard]] std::optional<AdjudicationManager::ResignAdjudicationConfig> getResignAdjudicationConfig() const;

    /**
     * @brief Gets the openings configuration
     * @return Optional containing openings config if configured, nullopt otherwise
     */
    [[nodiscard]] std::optional<Openings> getOpenings() const;

    /**
     * @brief Gets the tournament configuration
     * @return Optional containing tournament config if configured, nullopt otherwise
     */
    [[nodiscard]] std::optional<TournamentConfig> getTournamentConfig() const;

    /**
     * @brief Gets the SPRT configuration
     * @return Optional containing SPRT config if configured, nullopt otherwise
     */
    [[nodiscard]] std::optional<SprtConfig> getSprtConfig() const;

    /**
     * @brief Gets the EPD configuration
     * @return Optional containing EPD config if configured, nullopt otherwise
     */
    [[nodiscard]] std::optional<EpdConfig> getEpdConfig() const;

    /**
     * @brief Gets the SPSA configuration
     * @return Optional containing SPSA config if configured, nullopt otherwise
     */
    [[nodiscard]] std::optional<SPSAConfig> getSPSAConfig() const;

    /**
     * @brief Gets all engine configurations (including non-selected ones)
     * @return Vector of all engine configurations with selection status
     */
    [[nodiscard]] const std::vector<EngineConfiguration>& getAllEngineConfigurations() const;

    /**
     * @brief Applies logger configuration with specified report base name
     * @param reportLogBaseName Base name for report log files
     */
    void applyLoggerConfig(const std::string& reportLogBaseName) const;

    /**
     * @brief Gets the logger configuration
     * @return Pointer to logger config if configured, nullptr otherwise
     */
    [[nodiscard]] const LoggerConfig* getLoggerConfig() const;

    // Delete copy constructor and assignment operator
    QaplaSettings(const QaplaSettings&) = delete;
    QaplaSettings& operator=(const QaplaSettings&) = delete;

private:
    /**
     * @brief Private constructor for singleton pattern
     */
    QaplaSettings() = default;

    /**
     * @brief Converts argc/argv into a vector of strings
     * @param argc Argument count
     * @param argv Argument values
     * @return Vector of argument strings
     */
    [[nodiscard]] static std::vector<std::string> argvToVector(int argc, char* argv[]);

    /**
     * @brief Applies settings from a configuration file
     * @param settingsFile Path to the settings file
     * @param strict When true, unknown sections throw errors. When false, unknown sections are silently ignored.
     */
    void applySettingsFromFile(std::string_view settingsFile, bool strict = true);

    /**
     * @brief Reads logger configuration from CLI settings
     */
    void setLoggerConfiguration();

    /**
     * @brief Reads engine options from CLI settings
     * @param manager The settings manager to read from
     * @param groupName The group instance name
     */
    static void setEngineConfig(Settings::Manager& manager, const std::string& groupName);

    /**
     * @brief Reads PGN options from settings manager
     * @param manager The settings manager to read from
     * @param groupName The group instance name
     */
    void setPgnConfig(Settings::Manager& manager, const std::string& groupName);

    /**
     * @brief Reads draw adjudication options from settings manager
     * @param manager The settings manager to read from
     * @param groupName The group instance name
     */
    void setDrawAdjudicationConfig(Settings::Manager& manager, const std::string& groupName);

    /**
     * @brief Reads resign adjudication options from settings manager
     * @param manager The settings manager to read from
     * @param groupName The group instance name
     */
    void setResignAdjudicationConfig(Settings::Manager& manager, const std::string& groupName);

    /**
     * @brief Reads openings configuration from settings manager
     * @param manager The settings manager to read from
     * @param groupName The group instance name
     */
    void setOpenings(Settings::Manager& manager, const std::string& groupName);

    /**
     * @brief Reads tournament configuration from CLI settings
     */
    void setTournamentConfig();

    /**
     * @brief Loads SPRT configuration, checking for file-based config first
     */
    void loadSprtConfig();

    /**
     * @brief Reads SPRT configuration from settings manager
     * @param manager The settings manager to read from
     * @param groupName The group instance name
     */
    void setSprtConfig(Settings::Manager& manager, const std::string& groupName);

    /**
     * @brief Reads EPD configuration from CLI settings
     */
    void setEpdConfig();

    /**
     * @brief Reads SPSA configuration from CLI settings
     */
    void setSPSAConfig();

    /**
     * @brief Applies all configurations found in ConfigData
     * @param configData The configuration data to apply
     * @param id Identifier for the configuration sections
     */
    void setFromConfigData(const QaplaHelpers::ConfigData& configData, const std::string& id);

    std::vector<std::string> m_arguments; ///< Stored command-line arguments
    std::unique_ptr<PgnSave::Options> m_pgnOptions; ///< PGN output options
    std::unique_ptr<AdjudicationManager::DrawAdjudicationConfig> m_drawConfig; ///< Draw adjudication config
    std::unique_ptr<AdjudicationManager::ResignAdjudicationConfig> m_resignConfig; ///< Resign adjudication config
    std::unique_ptr<Openings> m_openings; ///< Openings configuration
    std::unique_ptr<TournamentConfig> m_tournamentConfig; ///< Tournament configuration
    std::unique_ptr<SprtConfig> m_sprtConfig; ///< SPRT configuration
    std::unique_ptr<EpdConfig> m_epdConfig; ///< EPD configuration
    std::unique_ptr<SPSAConfig> m_spsaConfig; ///< SPSA configuration
    std::unique_ptr<LoggerConfig> m_loggerConfig; ///< Logger configuration
    std::vector<EngineConfiguration> m_allEngineConfigurations; ///< All engine configurations (selected and non-selected)
};

} // namespace CliSettings
