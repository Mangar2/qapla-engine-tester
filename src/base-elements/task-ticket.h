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
 * @copyright Copyright (c) 2026 Volker Böhm
 */

#pragma once

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <utility>

namespace QaplaHelpers {

/**
 * @brief A receipt for one request, which its sender can wait on.
 *
 * Every asker gets a ticket of their own, so nobody's answer can be handed to somebody else. That
 * is the difference to a promise living on the thing being asked: such a promise has to be
 * replaced for the next request, and replacing it under a waiter is how a wait becomes endless.
 *
 * A ticket is done exactly once and stays done. Waiting on a ticket that is already done returns
 * at once.
 */
class TaskTicket {
public:
    /** @brief Marks the request as carried out. Repeated calls do nothing. */
    void markDone() noexcept {
        {
            std::scoped_lock lock(mutex_);
            if (done_) {
                return;
            }
            done_ = true;
        }
        changed_.notify_all();
    }

    /** @brief Blocks until the request has been carried out. */
    void wait() {
        std::unique_lock lock(mutex_);
        changed_.wait(lock, [this] { return done_; });
    }

    /**
     * @brief Blocks until the request has been carried out, or the time is up.
     * @return true if it was carried out, false on timeout.
     */
    template <typename Rep, typename Period>
    [[nodiscard]] bool waitFor(std::chrono::duration<Rep, Period> limit) {
        std::unique_lock lock(mutex_);
        return changed_.wait_for(lock, limit, [this] { return done_; });
    }

    [[nodiscard]] bool isDone() const noexcept {
        std::scoped_lock lock(mutex_);
        return done_;
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable changed_;
    bool done_ = false;
};

using TaskTicketPtr = std::shared_ptr<TaskTicket>;

/**
 * @brief Travels with the request and marks its ticket done when it goes away.
 *
 * However it goes away: carried out, or thrown away unprocessed. That is the point -- a request
 * that is discarded is finished as far as its sender is concerned, and every place that discards
 * one would otherwise have to remember to say so. A queue that is emptied, an event dropped
 * because there is nothing to apply it to: the holder dies with it and the waiter is released.
 *
 * Held by the request, never by the waiter -- the waiter keeps the ticket instead. If the waiter
 * held a holder, it would be waiting for something only it could release.
 */
class TicketHolder {
public:
    explicit TicketHolder(TaskTicketPtr ticket) noexcept : ticket_(std::move(ticket)) {}

    TicketHolder(const TicketHolder&) = delete;
    TicketHolder& operator=(const TicketHolder&) = delete;
    TicketHolder(TicketHolder&&) = default;
    TicketHolder& operator=(TicketHolder&&) = default;

    ~TicketHolder() {
        if (ticket_) {
            ticket_->markDone();
        }
    }

    /** @brief Marks it done now, without waiting for this holder to be destroyed. */
    void markDone() noexcept {
        if (ticket_) {
            ticket_->markDone();
        }
    }

private:
    TaskTicketPtr ticket_;
};

using TicketHolderPtr = std::shared_ptr<TicketHolder>;

/** @brief A fresh ticket and the holder that will mark it done. */
[[nodiscard]] inline std::pair<TaskTicketPtr, TicketHolderPtr> makeTicket() {
    auto ticket = std::make_shared<TaskTicket>();
    return {ticket, std::make_shared<TicketHolder>(ticket)};
}

} // namespace QaplaHelpers
