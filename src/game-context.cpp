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

#include "game-context.h"
#include "engine-worker-factory.h"

namespace QaplaTester {

GameContext::GameContext() = default;

GameContext::~GameContext() = default;

void GameContext::updateEngineNames()
{
    auto *white = getWhite();
    auto *black = getBlack();
    const std::string whiteName = white != nullptr && white->getEngine() != nullptr ? 
        white->getEngine()->getConfig().getName() : "";
    const std::string blackName = black != nullptr && black->getEngine() != nullptr ? 
        black->getEngine()->getConfig().getName() : "";
    gameRecord_.setWhiteEngineName(whiteName);
    gameRecord_.setBlackEngineName(blackName);
}

void GameContext::playerRestartEngine(PlayerContext* player, bool differentThread)
{
    player->restartEngine(differentThread);
    if (eventCallback_)
    {
        player->getEngine()->setEventSink(eventCallback_);
    }
    bool isWhite = getWhite()->getIdentifier() == player->getIdentifier();
    player->newGame(gameRecord_, isWhite);
}

void GameContext::tearDown()
{
    std::scoped_lock lock(engineMutex_);
    players_.clear();
}

void GameContext::initPlayers(std::vector<std::unique_ptr<EngineWorker>> engines)
{
    {
        std::scoped_lock lock(engineMutex_);
        players_.clear();
        bool isWhite = !switchedSide_;
        for (auto &engine : engines)
        {
            if (eventCallback_)
            {
                engine->setEventSink(eventCallback_);
            }
            auto player = std::make_unique<PlayerContext>();
            player->setEngine(std::move(engine));
            player->setStartPosition(gameRecord_);
            player->setTimeControl(gameRecord_, isWhite);
            // it does not matter for engines with index > 1 if its white or not as it will never play a move
            isWhite = switchedSide_;
            players_.emplace_back(std::move(player));
        }
    }
    updateEngineNames();
    newGame();
}

void GameContext::ensureStarted()
{
    std::scoped_lock lock(engineMutex_);
    for (auto &player : players_)
    {
        if (player->getEngine()->isStopped())
        {
            playerRestartEngine(player.get(), true);
        }
    }
}

void GameContext::restartPlayer(const std::string &id)
{
    std::scoped_lock lock(engineMutex_);
    for (auto &player : players_)
    {
        if (player->getIdentifier() == id)
        {
            playerRestartEngine(player.get(), true);
        }
    }
}

void GameContext::stopEngine(const std::string &id)
{
    std::scoped_lock lock(engineMutex_);
    for (auto &player : players_)
    {
        if (player->getIdentifier() == id)
        {
            player->stopEngine();
        }
    }
}

void GameContext::setTimeControl(const TimeControl &timeControl)
{
    {
        std::scoped_lock lock(gameRecordMutex_);
        gameRecord_.setTimeControl(timeControl, timeControl);
    }
    {
        std::scoped_lock lock(engineMutex_);
        for (size_t i = 0; i < players_.size(); ++i)
        {
            bool isWhite = (i == 0 && !switchedSide_) || (i == 1 && switchedSide_);
            players_[i]->setTimeControl(gameRecord_, isWhite);
        }
    }
}

void GameContext::setTimeControls(const std::vector<TimeControl> &timeControls, bool informEngines)
{
    {
        std::scoped_lock lock(gameRecordMutex_);
        if (timeControls.size() >= 2)
        {
            gameRecord_.setTimeControl(timeControls[0], timeControls[1]);
        }
        else if (timeControls.size() == 1)
        {
            gameRecord_.setTimeControl(timeControls[0], timeControls[0]);
        }
    }
    if (!informEngines) {
        return;
    }
    {
        std::scoped_lock lock(engineMutex_);
        for (size_t i = 0; i < players_.size(); ++i)
        {
            bool isWhite = (i == 0 && !switchedSide_) || (i == 1 && switchedSide_);
            players_[i]->setTimeControl(gameRecord_, isWhite);
        }
    }
}

void GameContext::newGame()
{
    ensureStarted();
    for (size_t i = 0; i < players_.size(); ++i)
    {
        bool isWhite = (i == 0 && !switchedSide_) || (i == 1 && switchedSide_);
        players_[i]->newGame(gameRecord_, isWhite);
    }
}

void GameContext::setPosition(bool useStartPosition, const std::string &fen,
                              const std::optional<std::vector<std::string>>& playedMoves)
{
    cancelCompute();
    {
        std::scoped_lock lock(gameRecordMutex_);
        GameState gameState;
        gameState.setFen(useStartPosition, fen);
        gameRecord_.setStartPosition(useStartPosition, fen, gameState.isWhiteToMove(), gameState.getStartHalfmoves());
        updateEngineNames();

        if (playedMoves)
        {
            for (const auto &move : *playedMoves)
            {
                MoveRecord moveRecord(gameRecord_.nextMoveIndex(), "#gui");
                moveRecord.original = move;
                moveRecord.lan_ = move;
                gameRecord_.addMove(moveRecord);
            }
        }
    }

    for (auto &player : players_)
    {
        player->setStartPosition(gameRecord_);
    }
}

void GameContext::setPosition(const GameRecord &gameRecord)
{
    cancelCompute();
    {
        std::scoped_lock lock(gameRecordMutex_);
        gameRecord_ = gameRecord;
        updateEngineNames();
    }

    for (auto &player : players_)
    {
        player->setStartPosition(gameRecord_);
    }
}

void GameContext::setNextMoveIndex(uint32_t moveIndex)
{
    cancelCompute();
    {
        std::scoped_lock lock(gameRecordMutex_);
        if (moveIndex <= gameRecord_.history().size())
        {
            gameRecord_.setNextMoveIndex(moveIndex);
        }
        else {
            // We ignore moveIndex out of bounds as this may happen e.g. when selecting the game result
            // in the move list view.
            return;
        }
    }

    for (auto &player : players_)
    {
        player->setStartPosition(gameRecord_);
    }
}

void GameContext::doMove(const MoveRecord& move)
{
    cancelCompute(true);
    assert (!move.move.isEmpty());
    {
        std::scoped_lock lock(gameRecordMutex_);
        gameRecord_.addMove(move);
    }

    for (auto &player : players_)
    {
        player->doMove(move.move);
    }
}

size_t GameContext::getPlayerCount() const
{
    return players_.size();
}

PlayerContext *GameContext::player(size_t index)
{
    if (index >= players_.size())
    {
        return nullptr;
    }
    return players_[index].get();
}

PlayerContext *GameContext::getWhite()
{
    if (players_.empty())
    {
        return nullptr;
    }
    return players_[(switchedSide_ ? 1 : 0) % players_.size()].get();
}

const PlayerContext *GameContext::getWhite() const
{
    if (players_.empty())
    {
        return nullptr;
    }
    return players_[(switchedSide_ ? 1 : 0) % players_.size()].get();
}

PlayerContext *GameContext::getBlack()
{
    if (players_.size() < 2)
    {
        return getWhite();
    }
    return players_[(switchedSide_ ? 0 : 1) % players_.size()].get();
}

const PlayerContext *GameContext::getBlack() const
{
    if (players_.empty())
    {
        return nullptr;
    }
    return players_[(switchedSide_ ? 0 : 1) % players_.size()].get();
}

void GameContext::setSideSwitched(bool switched)
{
    switchedSide_ = switched;
}

bool GameContext::isSideSwitched() const
{
    return switchedSide_;
}

void GameContext::setEventCallback(std::function<void(EngineEvent &&)> callback)
{
    eventCallback_ = std::move(callback);
    for (auto &player : players_)
    {
        if (player->getEngine() != nullptr)
        {
            player->getEngine()->setEventSink(eventCallback_);
        }
    }
}

const GameRecord &GameContext::gameRecord() const
{
    return gameRecord_;
}

void GameContext::withGameRecord(const std::function<void(const GameRecord &)> &accessFn) const
{
    std::scoped_lock lock(gameRecordMutex_);
    accessFn(gameRecord_);
}

std::tuple<GameEndCause, GameResult> GameContext::checkGameResult()
{

    for (auto &player : players_)
    {
        auto [pcause, presult] = player->getGameResult();
        if (presult != GameResult::Unterminated)
        {
            std::scoped_lock lock(gameRecordMutex_);
            gameRecord_.setGameEnd(pcause, presult);
            break;
        }
    }

    return gameRecord_.getGameResult();
}

bool GameContext::checkForTimeoutsAndRestart()
{
    if (!eventCallback_)
    {
        throw AppError::make("GameContext::checkForTimeoutsAndRestart; No event callback set.");
    }

    bool restarted = false;
    for (auto &player : players_)
    {
        if (player->checkEngineTimeout())
        {
            restarted = true;
            player->getEngine()->setEventSink(eventCallback_);
        }
    }
    return restarted;
}

PlayerContext *GameContext::findPlayerByEngineId(const std::string &identifier)
{
    for (auto &player : players_)
    {
        if (player->getEngine()->getIdentifier() == identifier)
        {
            return player.get();
        }
    }
    return nullptr;
}

void GameContext::restartIfConfigured()
{
    std::scoped_lock lock(engineMutex_);
    for (auto &player : players_)
    {
        if (player->getEngine() == nullptr)
        {
            continue;
        }

        if (player->getEngine()->getConfig().getRestartOption() == RestartOption::Always)
        {
            playerRestartEngine(player.get(), false);
        }
    }
}

void GameContext::cancelCompute(bool keepPondering)
{
    std::scoped_lock lock(engineMutex_);
    for (auto &player : players_)
    {
        if (player->isPondering() && keepPondering) {
            continue;
        }
        player->cancelCompute();
    }
}

MoreRecords GameContext::getMoveInfos() const
{
    MoreRecords infos;
    for (const auto &player : players_)
    {
        infos.emplace_back(player->getCurrentMoveCopy());
    }
    return infos;
}

void GameContext::withMoveRecord(std::function<void(const MoveRecord&, uint32_t)> accessFn) const {
    uint32_t index = 0;
    std::scoped_lock lock(engineMutex_);
    for (const auto &player : players_)
    {
        uint32_t adjustedIndex = index;
        if (isSideSwitched() && index < 2)
        {
            adjustedIndex = 1 - index;
        }
        index++;
        player->withCurrentMove([&](const MoveRecord& record) {
            accessFn(record, adjustedIndex);
        });
    }
}

EngineRecords GameContext::mkEngineRecords() const
{
    EngineRecords records;
    records.resize(players_.size());
    uint32_t index = 0;

    for (const auto &player : players_)
    {
        uint32_t adjustedIndex = index;
        if (switchedSide_ && index == 0)
        {
            adjustedIndex = 1;
        }
        if (switchedSide_ && index == 1)
        {
            adjustedIndex = 0;
        }
        index++;

        if (player->getEngine() == nullptr)
        {
            EngineRecord record = {
                .identifier{},
                .config{},
                .supportedOptions{},
                .status = EngineRecord::Status::NotStarted,
                .memoryUsageB = 0};
            records[adjustedIndex] = record;
            continue;
        }
        auto *engine = player->getEngine();
        EngineRecord record = {
            .identifier = engine->getIdentifier(),
            .config = engine->getConfig(),
            .supportedOptions = engine->getSupportedOptions(),
            .memoryUsageB = engine->getEngineMemoryUsage()
        };
        switch (engine->workerState())
        {
        case EngineWorker::WorkerState::notStarted:
            record.status = EngineRecord::Status::NotStarted;
            break;
        case EngineWorker::WorkerState::starting:
            record.status = EngineRecord::Status::Starting;
            break;
        case EngineWorker::WorkerState::running:
            record.status = EngineRecord::Status::Running;
            break;
        case EngineWorker::WorkerState::stopped:
            record.status = EngineRecord::Status::NotStarted;
            break;
        default:
            record.status = EngineRecord::Status::Error;
            break;
        }
        records[adjustedIndex] = record;
    }
    return records;
}

} // namespace QaplaTester
