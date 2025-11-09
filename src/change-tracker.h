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

#include <cstdint>
#include <utility>

namespace QaplaTester {

class ChangeTracker {

public:
    ChangeTracker() = default;
    ChangeTracker([[maybe_unused]] const ChangeTracker& tracker) 
    :id_(nextId_++)
    {}

    void trackModification() {
        modificationCnt_++;
        updateCnt_++;
    }

    void trackUpdate() {
        updateCnt_++;
    }

    /**
     * @brief tracks the modification instead of copying the value from other
     *
     * @param other
     * @return ChangeTracker&
     */
    ChangeTracker& operator=(const ChangeTracker& other) {
        if (this != &other) {
            trackModification();
        }
        return *this;
    }

    /**
     * @brief checks if the modification or update count has changed
     *
     * @param other
     * @return std::pair<bool, bool>, first: modification changed, second: update changed
     */
    [[nodiscard]] std::pair<bool, bool> checkModification(const ChangeTracker& other) const {
        return { modificationCnt_ != other.modificationCnt_ || id_ != other.id_, 
            updateCnt_ != other.updateCnt_ || id_ != other.id_ };
    }

    /**
     * @brief updates the modification and update count from other without tracking
     *
     * @param other
     */
    void updateFrom(const ChangeTracker& other) {
        modificationCnt_ = other.modificationCnt_;
        updateCnt_ = other.updateCnt_;
        id_ = other.id_;
    }

    void clear() {
        modificationCnt_ = 0;
        updateCnt_ = 0;
    }

private:
    static inline int64_t nextId_ = 1;
    int64_t id_ = nextId_++;
    int64_t modificationCnt_ = 1;
    int64_t updateCnt_ = 1;
};

} // namespace QaplaTester
