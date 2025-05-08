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
 * @author Volker B�hm
 * @copyright Copyright (c) 2025 Volker B�hm
 */

#pragma once

#include <string>
#include <string_view>

 /**
  * @brief Type of starting position for a game.
  */
enum class GameType {
    Classical,
    Chess960,
    Unknown
};

/**
 * @brief Represents the logical setup of a game start position.
 */
class GameStartPosition {
public:
    GameStartPosition() = default;

    GameStartPosition(GameType type, std::string fen = {})
        : type_(type), fen_(std::move(fen)) {
    }

    GameType type() const { return type_; }
    void setType(GameType type) { type_ = type; }

    const std::string& fen() const { return fen_; }
    void setFen(std::string fen) { fen_ = std::move(fen); }

    bool operator==(const GameStartPosition& other) const {
        return type_ == other.type_ && fen_ == other.fen_;
    }

private:
    GameType type_ = GameType::Classical;
    std::string fen_;
};
