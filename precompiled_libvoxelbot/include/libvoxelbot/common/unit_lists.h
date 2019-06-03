#pragma once
#include "sc2api/sc2_interfaces.h"
#include <libvoxelbot/buildorder/optimizer.h>
#include <libvoxelbot/utilities/predicates.h>

struct AvailableUnitTypes {
    std::vector<BuildOrderItem> index2item;
    std::vector<int> type2index;
    std::vector<int> upgrade2index;
    std::map<int, int> arbitraryType2index;
  private:
    std::vector<sc2::UNIT_TYPEID> unitTypes;
  public:

    size_t size() const {
        return index2item.size();
    }

    const std::vector<sc2::UNIT_TYPEID>& getUnitTypes() const {
        return unitTypes;
    }


    AvailableUnitTypes(std::initializer_list<BuildOrderItem> types);

    int getIndex (sc2::UPGRADE_ID upgrade) const {
        assert((int)upgrade < (int)upgrade2index.size());
        auto res = upgrade2index[(int)upgrade];
        assert(res != -1);
        return res;
    }

    int getIndex (sc2::UNIT_TYPEID unit) const {
        assert((int)unit < (int)type2index.size());
        auto res = type2index[(int)unit];
        assert(res != -1);
        return res;
    }

    int getIndexMaybe (sc2::UPGRADE_ID unit) const {
        auto it = arbitraryType2index.find((int)unit);
        if (it != arbitraryType2index.end()) {
            return it->second;
        }
        return -1;
    }

    int getIndexMaybe (sc2::UNIT_TYPEID unit) const {
        // Fast path
        if ((int)unit < (int)type2index.size()) {
            return type2index[(int)unit];
        }
        // Slow path
        auto it = arbitraryType2index.find((int)unit);
        if (it != arbitraryType2index.end()) {
            return it->second;
        }
        return -1;
    }

    // Note: does not work for upgrades
    bool contains (sc2::UNIT_TYPEID unit) const {
        return getIndexMaybe(unit) != -1;
    }

    // Reasonable maximum for the unit/upgrade in an army composition.
    // This is primarily used to prevent upgrade counts higher than possible
    int armyCompositionMaximum(int index) const {
        assert(index < (int)index2item.size());
        if (!index2item[index].isUnitType()) {
            // This is an upgrade
            if (isUpgradeWithLevels(index2item[index].upgradeID())) {
                // Enumerable upgrade, there are 3 upgrade levels
                // TODO: Build order execution is bugged at the moment, only the first level can be executed
                return 1;
            } else {
                return 1;
            }
        }

        // Clamp maximum unit counts to something reasonable
        // In part to avoid giving the neural network too large input which might cause odd output
        return 20;
    }

    sc2::UNIT_TYPEID getUnitType(int index) const {
        assert(index < (int)index2item.size());
        if (!index2item[index].isUnitType()) return sc2::UNIT_TYPEID::INVALID;
        return index2item[index].typeID();
    }

    bool canBeChronoBoosted (int index) const;

    BuildOrderItem getBuildOrderItem (int index) const {
        assert(index < (int)index2item.size());
        return index2item[index];
    }

    BuildOrderItem getBuildOrderItem (const GeneUnitType item) const {
        auto itm = index2item[item.type];
        itm.chronoBoosted = item.chronoBoosted;
        return itm;
    }

    GeneUnitType getGeneItem (const BuildOrderItem item) const {
        return GeneUnitType(arbitraryType2index.at((int)item.rawType()), item.chronoBoosted);
    }

    friend int remapAvailableUnitIndex(int index, const AvailableUnitTypes& from, const AvailableUnitTypes& to) {
        assert(index < (int)from.index2item.size());
        return to.arbitraryType2index.at((int)from.index2item[index].rawType());
    }
};


enum class UnitCategory {
    Economic,
    ArmyCompositionOptions,
    BuildOrderOptions,
};

const AvailableUnitTypes& getAvailableUnitsForRace (sc2::Race race);
const AvailableUnitTypes& getAvailableUnitsForRace (sc2::Race race, UnitCategory category);

extern const AvailableUnitTypes availableUpgrades;