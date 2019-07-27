#pragma once
#include <vector>
#include "sc2api/sc2_interfaces.h"

struct BuildState;

static const int UPGRADE_ID_OFFSET = 1000000;
struct BuildOrderItem {
  public:
    // union ish. It is assumed that unit type ids are pretty small (which they are)
    sc2::UNIT_TYPEID internalType = sc2::UNIT_TYPEID::INVALID;
    bool chronoBoosted = false;

    bool isUnitType() const {
        return (int)internalType < UPGRADE_ID_OFFSET;
    }

    sc2::UNIT_TYPEID rawType() const {
        return internalType;
    }

    sc2::UNIT_TYPEID typeID() const {
        assert((int)internalType < UPGRADE_ID_OFFSET);
        return internalType;
    }

    sc2::UPGRADE_ID upgradeID() const {
        return (sc2::UPGRADE_ID)((int)internalType - UPGRADE_ID_OFFSET);
    }

    BuildOrderItem() {}
    explicit BuildOrderItem(sc2::UPGRADE_ID upgrade, bool chronoBoosted = false) : internalType((sc2::UNIT_TYPEID)((int)upgrade + UPGRADE_ID_OFFSET)), chronoBoosted(chronoBoosted) {}
    explicit BuildOrderItem(sc2::UNIT_TYPEID type) : internalType(type), chronoBoosted(false) {}
    explicit BuildOrderItem(sc2::UNIT_TYPEID type, bool chronoBoosted) : internalType(type), chronoBoosted(chronoBoosted) {}

    bool operator==(const BuildOrderItem& other) const {
        return internalType == other.internalType && chronoBoosted == other.chronoBoosted;
    }

    bool operator!=(const BuildOrderItem& other) const {
        return internalType != other.internalType || chronoBoosted != other.chronoBoosted;
    }

    std::string name() const {
        if (isUnitType()) return sc2::UnitTypeToName(typeID());
        else return sc2::UpgradeIDToName(upgradeID());
    }
};

enum class BuildOrderPrintMode {
    Brief,
    Detailed
};

struct BuildOrder {
    std::vector<BuildOrderItem> items;

    BuildOrder() {}

    BuildOrder(std::initializer_list<BuildOrderItem> order) {
        for (auto tp : order) {
            items.push_back(tp);
        }
    }
    
    BuildOrder(std::initializer_list<sc2::UNIT_TYPEID> order) {
        for (auto tp : order) {
            items.push_back(BuildOrderItem(tp));
        }
    }

    explicit BuildOrder (const std::vector<sc2::UNIT_TYPEID>& order) : items(order.size()) {
        for (size_t i = 0; i < order.size(); i++) {
            items[i] = BuildOrderItem(order[i]);
        }
    }

    inline size_t size() const noexcept {
        return items.size();
    }

    inline BuildOrderItem& operator[] (int index) {
        return items[index];
    }

    inline const BuildOrderItem& operator[] (int index) const {
        return items[index];
    }

    std::vector<BuildOrderItem>::const_iterator begin() const { return items.begin(); }
	std::vector<BuildOrderItem>::const_iterator end() const { return items.end(); }
    std::vector<BuildOrderItem>::iterator begin() { return items.begin(); }
	std::vector<BuildOrderItem>::iterator end() { return items.end(); }

    std::string toString();
    std::string toString(BuildState initialState, BuildOrderPrintMode mode = BuildOrderPrintMode::Brief);
};


struct BuildOrderState {
    std::shared_ptr<const BuildOrder> buildOrder;
    int buildIndex = 0;
    sc2::UNIT_TYPEID lastChronoUnit = sc2::UNIT_TYPEID::INVALID;

    BuildOrderState (std::shared_ptr<const BuildOrder> buildOrder) : buildOrder(buildOrder) {}
};

void printBuildOrder(const std::vector<sc2::UNIT_TYPEID>& buildOrder);
void printBuildOrder(const BuildOrder& buildOrder);
