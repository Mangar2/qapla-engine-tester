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

#include <vector>
#include <unordered_map>
#include <stdexcept>
#include <initializer_list>

namespace QaplaHelpers {

/**
 * @brief Map that preserves insertion order from initializer list.
 * Provides map-like interface with O(1) lookup while maintaining order.
 */
template<typename K, typename V>
class StableMap {
public:
	using value_type = std::pair<K, V>;
	using iterator = typename std::vector<value_type>::iterator;
	using const_iterator = typename std::vector<value_type>::const_iterator;

	StableMap() = default;

	StableMap(std::initializer_list<value_type> init) {
		items_.reserve(init.size());
		for (const auto& item : init) {
			index_[item.first] = items_.size();
			items_.push_back(item);
		}
	}

	[[nodiscard]] const V& at(const K& key) const {
		const auto it = index_.find(key);
		if (it == index_.end()) {
			throw std::out_of_range("Key not found in OrderedMap");
		}
		return items_[it->second].second;
	}

	[[nodiscard]] V& at(const K& key) {
		const auto it = index_.find(key);
		if (it == index_.end()) {
			throw std::out_of_range("Key not found in OrderedMap");
		}
		return items_[it->second].second;
	}

	[[nodiscard]] bool contains(const K& key) const {
		return index_.contains(key);
	}

	[[nodiscard]] const_iterator find(const K& key) const {
		const auto it = index_.find(key);
		if (it == index_.end()) {
			return items_.end();
		}
		return items_.begin() + it->second;
	}

	[[nodiscard]] iterator find(const K& key) {
		const auto it = index_.find(key);
		if (it == index_.end()) {
			return items_.end();
		}
		return items_.begin() + it->second;
	}

	[[nodiscard]] const_iterator begin() const { return items_.begin(); }
	[[nodiscard]] const_iterator end() const { return items_.end(); }
	[[nodiscard]] iterator begin() { return items_.begin(); }
	[[nodiscard]] iterator end() { return items_.end(); }

	[[nodiscard]] size_t size() const { return items_.size(); }
	[[nodiscard]] bool empty() const { return items_.empty(); }

	V& operator[](const K& key) {
		const auto it = index_.find(key);
		if (it != index_.end()) {
			return items_[it->second].second;
		}
		index_[key] = items_.size();
		items_.emplace_back(key, V{});
		return items_.back().second;
	}

private:
	std::vector<value_type> items_;
	std::unordered_map<K, size_t> index_;
};

} // namespace QaplaHelpers
