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

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <sstream>
#include <string>
#include <thread>

namespace QaplaHelpers {

/**
 * @brief A trace that can be read after the fact, and does not change what it observes.
 *
 * Written for a fault that vanished the moment it was traced: one line per event to stderr, with
 * a flush, and twenty-five runs went by without it. The same binary without the tracing froze on
 * the second. Writing to a stream synchronises threads -- and a fault that lives in the gap
 * between two threads is exactly what that hides.
 *
 * So this one writes nothing anywhere. Each event takes a timestamp, a slot in a fixed array and
 * an atomic increment: no lock, no allocation, no I/O, nothing another thread has to wait for.
 * What was recorded is read afterwards -- from a debugger on a frozen process, or through
 * dump() from code.
 *
 * Deliberately plain data, so a debugger can read it without calling anything:
 *
 *     (lldb) p QaplaHelpers::traceRing.entries_[100]
 *     (lldb) p QaplaHelpers::traceRing.next_
 *
 * The events are a ring: entry N lives at N % CAPACITY, and once it wraps, the oldest are gone.
 * next_ says how many were ever written, so the order can be reconstructed from the array.
 *
 * Not switched off in release builds, because that is where the faults worth tracing tend to be.
 * It costs an atomic increment and a handful of stores per event; leave the calls in only while
 * a hunt is on.
 */
class TraceRing {
public:
    /** @brief One event. Only trivially copyable fields, so a debugger can print it. */
    struct Entry {
        double millis = 0.0;          ///< Since the first event, in milliseconds
        uint32_t thread = 0;          ///< Small per-thread number, assigned in order of first use
        const char* event = nullptr;  ///< A string literal: what happened
        const void* subject = nullptr;///< Whom it happened to, usually `this`
        int64_t first = 0;            ///< Free numeric slot, meaning is the caller's
        int64_t second = 0;           ///< Free numeric slot, meaning is the caller's
        const char* detail = nullptr; ///< A string literal, or nullptr
    };

    /** @brief How many events are kept. The oldest are overwritten. */
    static constexpr size_t CAPACITY = 4096;

    /**
     * @brief Records one event.
     *
     * Safe from any thread and never blocks: the slot is claimed with one atomic increment, and
     * a slot being overwritten while it is read is a garbled line, not a crash. That trade is the
     * point -- a trace that locks would change the timing it is meant to show.
     *
     * @param event A string literal naming what happened. Not copied, so it has to outlive the
     *              trace -- a literal always does.
     * @param subject Whom it happened to, usually `this`.
     * @param first Free numeric slot.
     * @param second Free numeric slot.
     * @param detail A string literal with more, or nullptr.
     */
    void add(const char* event, const void* subject = nullptr, int64_t first = 0,
             int64_t second = 0, const char* detail = nullptr) noexcept {
        const uint64_t ticket = next_.fetch_add(1, std::memory_order_relaxed);
        Entry& entry = entries_[ticket % CAPACITY];
        entry.millis = millisSinceStart();
        entry.thread = threadNumber();
        entry.event = event;
        entry.subject = subject;
        entry.first = first;
        entry.second = second;
        entry.detail = detail;
    }

    /**
     * @brief The events, oldest first, one per line.
     * @param maxEntries At most this many of the most recent events.
     */
    [[nodiscard]] std::string dump(size_t maxEntries = CAPACITY) const {
        const uint64_t written = next_.load(std::memory_order_relaxed);
        const uint64_t available = written < CAPACITY ? written : CAPACITY;
        const uint64_t wanted = maxEntries < available ? maxEntries : available;

        std::ostringstream out;
        for (uint64_t index = written - wanted; index < written; ++index) {
            const Entry& entry = entries_[index % CAPACITY];
            if (entry.event == nullptr) {
                continue;
            }
            out << entry.millis << " t" << entry.thread << " " << entry.event;
            if (entry.subject != nullptr) {
                out << " subject=" << entry.subject;
            }
            out << " " << entry.first << " " << entry.second;
            if (entry.detail != nullptr) {
                out << " " << entry.detail;
            }
            out << "\n";
        }
        return out.str();
    }

    /** @brief Forgets everything recorded so far. */
    void clear() noexcept {
        next_.store(0, std::memory_order_relaxed);
        entries_ = {};
    }

    /** @brief How many events were ever recorded, including those already overwritten. */
    [[nodiscard]] uint64_t count() const noexcept {
        return next_.load(std::memory_order_relaxed);
    }

    // Public so that a debugger can read them on a process that is not running any more.
    std::array<Entry, CAPACITY> entries_{};
    std::atomic<uint64_t> next_{0};

private:
    [[nodiscard]] static double millisSinceStart() noexcept {
        static const auto origin = std::chrono::steady_clock::now();
        return std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - origin).count();
    }

    /** @brief A small number per thread, easier to read than a thread id and stable within a run. */
    [[nodiscard]] static uint32_t threadNumber() noexcept {
        static std::atomic<uint32_t> nextNumber{1};
        static thread_local uint32_t number = nextNumber.fetch_add(1, std::memory_order_relaxed);
        return number;
    }
};

/**
 * @brief The one trace of this process.
 *
 * A named global rather than a function-local static, so that a debugger can find it by name in
 * an optimised build, where an accessor would have been inlined away.
 */
inline TraceRing traceRing;

/** @brief Shorthand for traceRing.add(). */
inline void trace(const char* event, const void* subject = nullptr, int64_t first = 0,
                  int64_t second = 0, const char* detail = nullptr) noexcept {
    traceRing.add(event, subject, first, second, detail);
}

} // namespace QaplaHelpers
