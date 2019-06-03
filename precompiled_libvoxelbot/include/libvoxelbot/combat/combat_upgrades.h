#pragma once
#include <bitset>
#include "sc2api/sc2_interfaces.h"

struct CombatUpgrades {
	std::bitset<90> upgrades;

	struct iterator {
		const CombatUpgrades& parent;
		size_t index = 0;
		iterator(const CombatUpgrades& parent, size_t index): parent(parent), index(index) {
			while(this->index < parent.upgrades.size() && !parent.upgrades[this->index]) this->index++;
		}
        iterator operator++() {
			index++;
			while(index < parent.upgrades.size() && !parent.upgrades[index]) index++;
			return *this;
		}
        bool operator!=(const iterator & other) const {
			return index != other.index;
		}
        sc2::UPGRADE_ID operator*() const;
	};

	CombatUpgrades();

	CombatUpgrades(std::initializer_list<sc2::UPGRADE_ID> upgrades) {
		for (auto u : upgrades) add(u);
	}

	bool hasUpgrade(sc2::UPGRADE_ID upgrade) const;

	uint64_t hash() const {
		return std::hash<std::bitset<90>>()(upgrades);
	}

	void add(sc2::UPGRADE_ID upgrade);

	void combine(const CombatUpgrades& other) {
		upgrades |= other.upgrades;
	}

	void remove(const CombatUpgrades& other) {
		upgrades &= ~other.upgrades;
	}

	iterator begin() const { return iterator(*this, 0 ); }
	iterator end() const { return iterator(*this, upgrades.size() ); }
};
