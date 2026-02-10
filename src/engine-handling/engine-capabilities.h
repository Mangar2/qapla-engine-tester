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
#include "engine-capability.h"
#include "engine-option.h"

#include "../base-elements/ini-file.h"

#include <unordered_map>
#include <string>
#include <ostream>
#include <optional>
#include <atomic>
#include <vector>
#include <memory>
#include <functional>
#include <format>
#include <mutex>
#include <condition_variable>

namespace QaplaTester {
    class EngineConfig;
    class EngineWorker;
}

namespace QaplaConfiguration {
    /**
     * @class EngineCapabilities
     * @brief Manages a collection of EngineCapability objects, providing methods to save, add, and retrieve capabilities.
     */
    class EngineCapabilities {
    public:
        using NotificationCallback = std::function<void(const std::string& message, const std::string& type)>;
        using McpNotificationCallback = std::function<void(const std::string& message)>;

        /**
         * @brief Registers a callback for notifications (e.g., snackbar in the GUI).
         * @param callback The callback function to register.
         */
        static void setNotificationCallback(NotificationCallback callback) {
            notificationCallback_ = std::move(callback);
        }

        /**
         * @brief Saves all engine capabilities to a stream in INI format.
         * @param out The output stream to write the data to.
         */
        void save(std::ostream& out) const {
            for (const auto& [key, capability] : capabilities_) {
                capability.save(out);
            }
        }

        /**
         * @brief Adds or replaces an EngineCapability in the collection.
         * @param capability The EngineCapability to add or replace.
         */
        void addOrReplace(const EngineCapability& capability) {
            auto key = makeKey(capability.getPath(), capability.getProtocol());
            capabilities_[key] = capability;
        }

        /**
         * @brief Adds or replaces an EngineCapability using a key-value map.
         * @param keyValueMap A map containing key-value pairs to initialize the EngineCapability.
         * @throws std::invalid_argument if the map is invalid or missing required keys.
         */
        void addOrReplace(const QaplaHelpers::IniFile::Section& section) {
            EngineCapability capability = EngineCapability::createFromSection(section);
            addOrReplace(capability);
        }

        /**
         * @brief Deletes an EngineCapability based on its path and protocol.
         * @param path The path to the engine executable.
         * @param protocol The protocol used by the engine.
         */
        void deleteCapability(const std::string& path, QaplaTester::EngineProtocol protocol) {
            auto key = makeKey(path, protocol);
            capabilities_.erase(key);
        }

        /**
         * @brief Retrieves an EngineCapability based on its path and protocol.
         * @param path The path to the engine executable.
         * @param protocol The protocol used by the engine.
         * @return An optional EngineCapability if found.
         */
        std::optional<EngineCapability> getCapability(const std::string& path, QaplaTester::EngineProtocol protocol) const {
            auto key = makeKey(path, protocol);
            auto it = capabilities_.find(key);
            if (it != capabilities_.end()) {
                return it->second;
            } 
            return std::nullopt;
        }

        /**
         * @brief Checks if any capability exists for a given path.
         * @param path The path to check for capabilities.
         * @param protocol The protocol used by the engine.
         * @return True if any capability exists for path and protocol, false otherwise.
         */
        bool hasAnyCapability(const std::string& path, QaplaTester::EngineProtocol protocol) const {
            return std::ranges::any_of(capabilities_, [&](const auto& pair) {
                return pair.second.getPath() == path && pair.second.getProtocol() == protocol;
            });
        }

        /**
         * @brief Detects missing engine configurations.
         *
         * This function attempts to start engines using the file paths and reads their parameters
         * (e.g., supported options, protocol, and other metadata) as well as their names for
         * configurations where the engine name is not set.
         *
         * The detection process follows this sequence:
         * 1. Try to start engines with UCI protocol
         * 2. For failed engines, retry with XBoard protocol
         * 3. Mark remaining failed engines as NotSupported
         */
        void autoDetect();

        /**
         * @brief Synchronously detects missing engine configurations.
         */
        void autoDetectSync();

        /**
         * @brief Waits for the completion of any ongoing engine detection.
         */
        void waitForDetection() const;

        /**
         * @brief Shuts down engine detection activities, waiting for completion if necessary.
         */
        void shutdown() const;

        /**
         * @brief Checks if detection is currently in progress.
         * @return True if detection is ongoing, false otherwise.
         */
        bool isDetecting() const {
            return detecting_.load();
        }

        /**
         * @brief Checks if all configured engines have been detected.
         * @return True if all engines have capabilities, false otherwise.
         */
        bool areAllEnginesDetected() const;

    private:
        /**
         * @brief Collects all engine configurations that don't have capabilities yet.
         * @return Vector of engine configurations that need capability detection.
         */
        std::vector<QaplaTester::EngineConfig> collectMissingCapabilities() const;

        /**
         * @brief Attempts to detect engines using the specified protocol.
         * This method tries to start engines with the given protocol, stores successful
         * detections, and returns the list of failed configurations.
         * @param configs Vector of engine configurations to detect.
         * @param protocol Pprotocol to use for detection; if not provided, uses the protocol from each config.
         * @return Vector of configurations that failed to start with the specified protocol.
         */
        std::vector<QaplaTester::EngineConfig> detectWithProtocol(
            std::vector<QaplaTester::EngineConfig>& configs, 
            std::optional<QaplaTester::EngineProtocol> protocol);

        /**
         * @brief Stores capabilities for successfully detected engines.
         * Updates the config manager and adds capabilities to the collection.
         * @param engines Vector of successfully started engines.
         */
        void storeCapabilities(const std::vector<std::unique_ptr<QaplaTester::EngineWorker>>& engines);

        /**
         * @brief Marks engines as not supported if they failed both UCI and XBoard detection.
         * @param failedConfigs Vector of configurations that failed detection.
         */
        static void markAsNotSupported(const std::vector<QaplaTester::EngineConfig>& failedConfigs);

        /**
         * @brief Creates a unique key for the unordered_map based on path and protocol.
         * @param path The path to the engine executable.
         * @param protocol The protocol used by the engine.
         * @return A string key combining path and protocol.
         */
        static std::string makeKey(const std::string& path, QaplaTester::EngineProtocol protocol) {
            return std::format("{}|{}", path, to_string(protocol));
        }

        inline static NotificationCallback notificationCallback_;
        std::unordered_map<std::string, EngineCapability> capabilities_; ///< Stores EngineCapability objects indexed by a unique key.
        std::atomic<bool> detecting_{false}; ///< True, if detection is currently in progress.
        mutable std::mutex detectionMutex_; ///< Mutex for synchronization of engine detection.
        mutable std::condition_variable detectionCv_; ///< Condition variable to notify about completion of engine detection.
    };

}