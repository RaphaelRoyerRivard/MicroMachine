#pragma once

#include "Common.h"
#include "UnitType.h"

class CCBot;

class Unit
{
    mutable CCBot * m_bot;
    CCUnitID    m_unitID;
    UnitType    m_unitType;

#ifdef SC2API
    const sc2::Unit * m_unit;
#else
    BWAPI::Unit m_unit;
#endif

public:

    Unit();

#ifdef SC2API
    Unit(const sc2::Unit * unit, CCBot & bot);
    const sc2::Unit * getUnitPtr() const;
    const sc2::UnitTypeID & getAPIUnitType() const;
#else
    Unit(const BWAPI::Unit unit, CCBot & bot);
    const BWAPI::Unit getUnitPtr() const;
    const BWAPI::UnitType & getAPIUnitType() const;
#endif

    bool operator < (const Unit & rhs) const;
    bool operator == (const Unit & rhs) const;
    bool operator != (const Unit & rhs) const;

    const UnitType & getType() const;
	bool hasAttribute(sc2::Attribute attribute) const;
    CCPosition getPosition() const;
    CCTilePosition getTilePosition() const;
	float getHitPointsPercentage() const;
    CCHealth getHitPoints() const;
    CCHealth getShields() const;
    CCHealth getEnergy() const;
    CCPlayer getPlayer() const;
	sc2::Tag getTag() const;
	int getTagAsInt() const;
    float getBuildProgress() const;
    int getWeaponCooldown() const;
	bool canAttackAir() const;
	bool canAttackGround() const;
    bool isCompleted() const;
    bool isBeingConstructed() const;
    bool isCloaked() const;
    bool isFlying() const;
	bool isLight() const;
	bool isArmored() const;
	bool isBiological() const;
	bool isMechanical() const;
	bool isPsionic() const;
	bool isMassive() const;
    bool isAlive() const;
    bool isPowered() const;
    bool isIdle() const;
    bool isBurrowed() const;
    bool isValid() const;
	bool isVisible() const;
    bool isTraining() const;
	bool isAddonTraining() const;
    bool isConstructing(const UnitType & type) const;
	bool isConstructingAnything() const;
	bool isCounterToUnit(const Unit& unit) const;
	bool isReturningCargo() const;
	sc2::Tag getAddonTag() const;
	bool isProducingAddon() const;
	bool isProductionBuildingIdle(bool underConstructionConsideredIdle, bool constructingAddonConsideredIdle) const;
	bool isMoving() const;

    void stop           () const;
	void cancel			() const;
    void attackUnit     (const Unit & target) const;
    void attackMove     (const CCPosition & targetPosition) const;
    void move           (const CCPosition & targetPosition) const;
    void move           (const CCTilePosition & targetTilePosition) const;
	void patrol			(const CCPosition & targetPosition) const;
	void patrol			(const CCTilePosition & targetPosition) const;
	void rightClick     (const Unit & target) const;
	void shiftRightClick(const Unit & target) const;
	void rightClick		(const sc2::Unit * target) const;
	void shiftRightClick(const sc2::Unit * target) const;
	void rightClick		(const CCPosition position) const;
	void shiftRightClick(const CCPosition position) const;
    void repair         (const Unit & target) const;
    void build          (const UnitType & buildingType, CCTilePosition pos) const;
    void buildTarget    (const UnitType & buildingType, const Unit & target) const;
	void harvestTarget	(const Unit & target) const;
    void train          (const UnitType & buildingType) const;
    void morph          (const UnitType & type) const;
	bool unloadUnit		(const sc2::Tag passengerTag) const;
	bool useAbility		(const sc2::ABILITY_ID abilityId) const;
	sc2::AvailableAbilities getAbilities() const;
	void getBuildingLimits(CCTilePosition & bottomLeft, CCTilePosition & topRight) const;
};
