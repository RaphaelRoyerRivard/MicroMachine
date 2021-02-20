#include "Unit.h"
#include "CCBot.h"

Unit::Unit()
    : m_bot(nullptr)
    , m_unit(nullptr)
    , m_unitID(0)
{

}

#ifdef SC2API
Unit::Unit(const sc2::Unit * unit, CCBot & bot)
    : m_bot(&bot)
    , m_unit(unit)
    , m_unitID(unit->tag)
    , m_unitType(unit->unit_type, bot)
{
    
}

const sc2::Unit * Unit::getUnitPtr() const
{
    return m_unit;
}

const sc2::UnitTypeID & Unit::getAPIUnitType() const
{
    BOT_ASSERT(isValid(), "Unit is not valid");
    return m_unit->unit_type;
}

#else
Unit::Unit(const BWAPI::Unit unit, CCBot & bot)
    : m_bot(&bot)
    , m_unit(unit)
    , m_unitID(unit->getID())
    , m_unitType(unit->getType(), bot)
{
    
}

const BWAPI::Unit Unit::getUnitPtr() const
{
    return m_unit;
}

const BWAPI::UnitType & Unit::getAPIUnitType() const
{
    BOT_ASSERT(isValid(), "Unit is not valid");
    return m_unit->getType();
}

#endif
bool Unit::operator < (const Unit & rhs) const
{
    return m_unit < rhs.m_unit;
}

bool Unit::operator == (const Unit & rhs) const
{
    return m_unit == rhs.m_unit;
}

const UnitType & Unit::getType() const
{
    return m_unitType;
}

bool Unit::hasAttribute(sc2::Attribute attribute) const
{
	const auto & unitData = getUnitData(m_unit->unit_type);
	return Util::Contains(attribute, unitData.attributes);
}

CCPosition Unit::getPosition() const
{
    BOT_ASSERT(isValid(), "Unit is not valid");
#ifdef SC2API
    return m_unit->pos;
#else
    return m_unit->getPosition();
#endif
}

CCTilePosition Unit::getTilePosition() const
{
    BOT_ASSERT(isValid(), "Unit is not valid");
#ifdef SC2API
    return Util::GetTilePosition(m_unit->pos);
#else
    return m_unit->getTilePosition();
#endif
}

float Unit::getHitPointsPercentage() const
{
	BOT_ASSERT(isValid(), "Unit is not valid");
	if (m_unit->health == 0 || m_unit->health_max == 0)
	{
		return 0;
	}
	return m_unit->health / m_unit->health_max * 100.f;
}

CCHealth Unit::getHitPoints() const
{
    BOT_ASSERT(isValid(), "Unit is not valid");
#ifdef SC2API
    return m_unit->health;
#else
    return m_unit->getHitPoints();
#endif
}

CCHealth Unit::getShields() const
{
    BOT_ASSERT(isValid(), "Unit is not valid");
#ifdef SC2API
    return m_unit->shield;
#else
    return m_unit->getShields();
#endif
}

CCHealth Unit::getEnergy() const
{
    BOT_ASSERT(isValid(), "Unit is not valid");
#ifdef SC2API
    return m_unit->energy;
#else
    return m_unit->getEnergy();
#endif
}

float Unit::getBuildProgress() const
{
    BOT_ASSERT(isValid(), "Unit is not valid");
    return m_unit->build_progress;
}

CCPlayer Unit::getPlayer() const
{
    BOT_ASSERT(isValid(), "Unit is not valid");

    if (m_unit->alliance == sc2::Unit::Alliance::Self) { return Players::Self; }
    else if (m_unit->alliance == sc2::Unit::Alliance::Enemy) { return Players::Enemy; }
    else { return Players::Neutral; }
}

sc2::Tag Unit::getTag() const
{
	return m_unit->tag;
}

int Unit::getTagAsInt() const
{
	BOT_ASSERT(isValid(), "Unit is not valid");
#ifdef SC2API
	int id = m_unit->tag;
#else
	int id = m_unit->getID();
#endif

	return id;
}

bool Unit::canAttackAir() const
{
	return Util::CanUnitAttackAir(m_unit, *m_bot);
}

bool Unit::canAttackGround() const
{
	return Util::CanUnitAttackGround(m_unit, *m_bot);
}

bool Unit::isCompleted() const
{
    BOT_ASSERT(isValid(), "Unit is not valid");
#ifdef SC2API
    return m_unit->build_progress >= 1.0f;
#else
    return m_unit->isCompleted();
#endif
}

bool Unit::isTraining() const
{
    return m_unit->orders.size() > 0;
}


bool Unit::isAddonTraining() const
{
	BOT_ASSERT(isValid(), "Unit is not valid");
#ifdef SC2API
	return m_unit->orders.size() > 1;	
#else
	return false;
#endif
}

bool Unit::isBeingConstructed() const
{
    BOT_ASSERT(isValid(), "Unit is not valid");
#ifdef SC2API
    return !isCompleted() && m_unit->build_progress > 0.0f;
#else
    return m_unit->isBeingConstructed();
#endif
}

sc2::Tag Unit::getAddonTag() const
{
	BOT_ASSERT(isValid(), "Unit is not valid");
	return m_unit->add_on_tag;
}

bool Unit::isProducingAddon() const
{
	if (isValid() && !isIdle())
	{
		const auto & orders = getUnitPtr()->orders;
		if (orders[0].ability_id == sc2::ABILITY_ID::BUILD_TECHLAB || orders[0].ability_id == sc2::ABILITY_ID::BUILD_REACTOR)
			return true;
	}
	return false;
}

bool Unit::isProductionBuildingIdle(bool underConstructionConsideredIdle, bool constructingAddonConsideredIdle) const
{
	if(getType().isBuilding())
	{
		//Check if this building is idle
		if (isIdle() || (constructingAddonConsideredIdle && isProducingAddon()))
		{
			if (underConstructionConsideredIdle || isCompleted())
				return true;
		}
	}
	return false;
}

bool Unit::isMoving() const
{
	const auto & orders = getUnitPtr()->orders;
	for (const auto & order : orders)
	{
		if (order.ability_id == sc2::ABILITY_ID::MOVE)
			return true;
	}
	return false;
}

int Unit::getWeaponCooldown() const
{
    BOT_ASSERT(isValid(), "Unit is not valid");
#ifdef SC2API
    return (int)m_unit->weapon_cooldown;
#else
    return std::max(m_unit->getGroundWeaponCooldown(), m_unit->getAirWeaponCooldown());
#endif
}

bool Unit::isCloaked() const
{
    BOT_ASSERT(isValid(), "Unit is not valid");
    return m_unit->cloak != sc2::Unit::NotCloaked && m_unit->cloak != sc2::Unit::CloakedUnknown;
}

bool Unit::isFlying() const
{
    BOT_ASSERT(isValid(), "Unit is not valid");
#ifdef SC2API
    return m_unit->is_flying;
#else
    return m_unit->isFlying();
#endif
}

bool Unit::isLight() const
{
	switch ((sc2::UNIT_TYPEID)getAPIUnitType())
	{
		case sc2::UNIT_TYPEID::TERRAN_SCV:
		case sc2::UNIT_TYPEID::PROTOSS_PROBE:
		case sc2::UNIT_TYPEID::ZERG_DRONE:
		case sc2::UNIT_TYPEID::PROTOSS_ZEALOT:
		case sc2::UNIT_TYPEID::PROTOSS_SENTRY:
		case sc2::UNIT_TYPEID::PROTOSS_HIGHTEMPLAR:
		case sc2::UNIT_TYPEID::PROTOSS_DARKTEMPLAR:
		case sc2::UNIT_TYPEID::PROTOSS_OBSERVER:
		case sc2::UNIT_TYPEID::PROTOSS_OBSERVERSIEGEMODE:
		case sc2::UNIT_TYPEID::PROTOSS_PHOENIX:
		case sc2::UNIT_TYPEID::PROTOSS_ORACLE:
		case sc2::UNIT_TYPEID::PROTOSS_INTERCEPTOR:
		case sc2::UNIT_TYPEID::PROTOSS_ADEPT:
		case sc2::UNIT_TYPEID::TERRAN_MARINE:
		case sc2::UNIT_TYPEID::TERRAN_REAPER:
		case sc2::UNIT_TYPEID::TERRAN_HELLION:
		case sc2::UNIT_TYPEID::TERRAN_RAVEN:
		case sc2::UNIT_TYPEID::TERRAN_BANSHEE:
		case sc2::UNIT_TYPEID::TERRAN_HELLIONTANK:
		case sc2::UNIT_TYPEID::TERRAN_WIDOWMINE:
		case sc2::UNIT_TYPEID::TERRAN_WIDOWMINEBURROWED:
		case sc2::UNIT_TYPEID::TERRAN_POINTDEFENSEDRONE:			
		case sc2::UNIT_TYPEID::ZERG_LARVA:
		case sc2::UNIT_TYPEID::ZERG_ZERGLING:
		case sc2::UNIT_TYPEID::ZERG_BANELINGBURROWED:
		case sc2::UNIT_TYPEID::ZERG_HYDRALISK:
		case sc2::UNIT_TYPEID::ZERG_HYDRALISKBURROWED:
		case sc2::UNIT_TYPEID::ZERG_BROODLING:
		case sc2::UNIT_TYPEID::ZERG_CHANGELING:
		case sc2::UNIT_TYPEID::ZERG_INFESTEDTERRANSEGG:
		case sc2::UNIT_TYPEID::ZERG_INFESTORTERRAN:
		case sc2::UNIT_TYPEID::ZERG_MUTALISK:
		case sc2::UNIT_TYPEID::ZERG_LOCUSTMP:
		case sc2::UNIT_TYPEID::ZERG_LOCUSTMPFLYING:
			return true;
		default:
			return false;
	}
}

bool Unit::isArmored() const
{

	switch ((sc2::UNIT_TYPEID)getAPIUnitType())
	{
		case sc2::UNIT_TYPEID::PROTOSS_STALKER:
		case sc2::UNIT_TYPEID::PROTOSS_IMMORTAL:
		case sc2::UNIT_TYPEID::PROTOSS_COLOSSUS:
		case sc2::UNIT_TYPEID::PROTOSS_WARPPRISM:
		case sc2::UNIT_TYPEID::PROTOSS_WARPPRISMPHASING:
		case sc2::UNIT_TYPEID::PROTOSS_VOIDRAY:
		case sc2::UNIT_TYPEID::PROTOSS_CARRIER:
		case sc2::UNIT_TYPEID::PROTOSS_MOTHERSHIP:
		case sc2::UNIT_TYPEID::PROTOSS_TEMPEST:
		case sc2::UNIT_TYPEID::PROTOSS_ORACLE:
		case sc2::UNIT_TYPEID::PROTOSS_DISRUPTOR:
		case sc2::UNIT_TYPEID::TERRAN_MARAUDER:
		case sc2::UNIT_TYPEID::TERRAN_SIEGETANK:
		case sc2::UNIT_TYPEID::TERRAN_SIEGETANKSIEGED:
		case sc2::UNIT_TYPEID::TERRAN_THOR:
		case sc2::UNIT_TYPEID::TERRAN_THORAP:
		case sc2::UNIT_TYPEID::TERRAN_VIKINGASSAULT:
		case sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER:
		case sc2::UNIT_TYPEID::TERRAN_MEDIVAC:
		case sc2::UNIT_TYPEID::TERRAN_BATTLECRUISER:
		case sc2::UNIT_TYPEID::TERRAN_CYCLONE:
		case sc2::UNIT_TYPEID::TERRAN_LIBERATOR:
		case sc2::UNIT_TYPEID::TERRAN_LIBERATORAG:
		case sc2::UNIT_TYPEID::TERRAN_AUTOTURRET:
		case sc2::UNIT_TYPEID::ZERG_ROACH:
		case sc2::UNIT_TYPEID::ZERG_ROACHBURROWED:
		case sc2::UNIT_TYPEID::ZERG_INFESTOR:
		case sc2::UNIT_TYPEID::ZERG_INFESTORBURROWED:
		case sc2::UNIT_TYPEID::ZERG_ULTRALISK:
		case sc2::UNIT_TYPEID::ZERG_ULTRALISKBURROWED:
		case sc2::UNIT_TYPEID::ZERG_OVERLORD:
		case sc2::UNIT_TYPEID::ZERG_OVERSEER:
		case sc2::UNIT_TYPEID::ZERG_CORRUPTOR:
		case sc2::UNIT_TYPEID::ZERG_BROODLORD:
		case sc2::UNIT_TYPEID::ZERG_SWARMHOSTMP:
		case sc2::UNIT_TYPEID::ZERG_SWARMHOSTBURROWEDMP:
		case sc2::UNIT_TYPEID::ZERG_VIPER:
		case sc2::UNIT_TYPEID::ZERG_LURKERMP:
		case sc2::UNIT_TYPEID::ZERG_LURKERMPBURROWED:
		case sc2::UNIT_TYPEID::ZERG_NYDUSCANAL:
		case sc2::UNIT_TYPEID::ZERG_NYDUSNETWORK:
			return true;
		default:
			return false;
	}
}

bool Unit::isBiological() const
{

	switch ((sc2::UNIT_TYPEID)getAPIUnitType())
	{
	case sc2::UNIT_TYPEID::TERRAN_SCV:
	case sc2::UNIT_TYPEID::ZERG_DRONE:
		case sc2::UNIT_TYPEID::PROTOSS_ZEALOT:
		case sc2::UNIT_TYPEID::PROTOSS_HIGHTEMPLAR:
		case sc2::UNIT_TYPEID::PROTOSS_DARKTEMPLAR:
		case sc2::UNIT_TYPEID::PROTOSS_ADEPT:
		case sc2::UNIT_TYPEID::TERRAN_MARINE:
		case sc2::UNIT_TYPEID::TERRAN_MARAUDER:
		case sc2::UNIT_TYPEID::TERRAN_REAPER:
		case sc2::UNIT_TYPEID::TERRAN_GHOST:
		case sc2::UNIT_TYPEID::TERRAN_HELLIONTANK:
		case sc2::UNIT_TYPEID::ZERG_QUEEN:
		case sc2::UNIT_TYPEID::ZERG_QUEENBURROWED:
		case sc2::UNIT_TYPEID::ZERG_ZERGLING:
		case sc2::UNIT_TYPEID::ZERG_ZERGLINGBURROWED:
		case sc2::UNIT_TYPEID::ZERG_BANELING:
		case sc2::UNIT_TYPEID::ZERG_BANELINGBURROWED:
		case sc2::UNIT_TYPEID::ZERG_ROACH:
		case sc2::UNIT_TYPEID::ZERG_ROACHBURROWED:
		case sc2::UNIT_TYPEID::ZERG_HYDRALISK:
		case sc2::UNIT_TYPEID::ZERG_HYDRALISKBURROWED:
		case sc2::UNIT_TYPEID::ZERG_INFESTOR:
		case sc2::UNIT_TYPEID::ZERG_INFESTORBURROWED:
		case sc2::UNIT_TYPEID::ZERG_ULTRALISK:
		case sc2::UNIT_TYPEID::ZERG_OVERLORD:
		case sc2::UNIT_TYPEID::ZERG_OVERSEER:
		case sc2::UNIT_TYPEID::ZERG_MUTALISK:
		case sc2::UNIT_TYPEID::ZERG_CORRUPTOR:
		case sc2::UNIT_TYPEID::PROTOSS_STALKER:
		case sc2::UNIT_TYPEID::ZERG_BROODLORD:
		case sc2::UNIT_TYPEID::ZERG_VIPER:
		case sc2::UNIT_TYPEID::ZERG_SWARMHOSTMP:
		case sc2::UNIT_TYPEID::ZERG_SWARMHOSTBURROWEDMP:
		case sc2::UNIT_TYPEID::ZERG_RAVAGER:
		case sc2::UNIT_TYPEID::ZERG_LURKERMP:
		case sc2::UNIT_TYPEID::ZERG_LURKERMPBURROWED:
		case sc2::UNIT_TYPEID::ZERG_BROODLING:
		case sc2::UNIT_TYPEID::ZERG_EGG:
		case sc2::UNIT_TYPEID::ZERG_BANELINGCOCOON:
		case sc2::UNIT_TYPEID::ZERG_BROODLORDCOCOON:
		case sc2::UNIT_TYPEID::ZERG_RAVAGERCOCOON:
		case sc2::UNIT_TYPEID::ZERG_TRANSPORTOVERLORDCOCOON:
			return true;
		default:
			return false;
	}
}

bool Unit::isMechanical() const
{

	switch ((sc2::UNIT_TYPEID)getAPIUnitType())
	{
		case sc2::UNIT_TYPEID::PROTOSS_STALKER:
		case sc2::UNIT_TYPEID::PROTOSS_SENTRY:
		case sc2::UNIT_TYPEID::PROTOSS_IMMORTAL:
		case sc2::UNIT_TYPEID::PROTOSS_COLOSSUS:
		case sc2::UNIT_TYPEID::PROTOSS_OBSERVER:
		case sc2::UNIT_TYPEID::PROTOSS_OBSERVERSIEGEMODE:
		case sc2::UNIT_TYPEID::PROTOSS_WARPPRISM:
		case sc2::UNIT_TYPEID::PROTOSS_WARPPRISMPHASING:
		case sc2::UNIT_TYPEID::PROTOSS_PHOENIX:
		case sc2::UNIT_TYPEID::PROTOSS_VOIDRAY:
		case sc2::UNIT_TYPEID::PROTOSS_CARRIER:
		case sc2::UNIT_TYPEID::PROTOSS_INTERCEPTOR:
		case sc2::UNIT_TYPEID::PROTOSS_MOTHERSHIP:
		case sc2::UNIT_TYPEID::PROTOSS_ORACLE:
		case sc2::UNIT_TYPEID::PROTOSS_TEMPEST:
		case sc2::UNIT_TYPEID::PROTOSS_DISRUPTOR:
		case sc2::UNIT_TYPEID::TERRAN_HELLION:
		case sc2::UNIT_TYPEID::TERRAN_HELLIONTANK:
		case sc2::UNIT_TYPEID::TERRAN_VIKINGASSAULT:
		case sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER:
		case sc2::UNIT_TYPEID::TERRAN_MEDIVAC:
		case sc2::UNIT_TYPEID::TERRAN_RAVEN:
		case sc2::UNIT_TYPEID::TERRAN_BANSHEE:
		case sc2::UNIT_TYPEID::TERRAN_SIEGETANK:
		case sc2::UNIT_TYPEID::TERRAN_SIEGETANKSIEGED:
		case sc2::UNIT_TYPEID::TERRAN_THOR:
		case sc2::UNIT_TYPEID::TERRAN_THORAP:
		case sc2::UNIT_TYPEID::TERRAN_BATTLECRUISER:
		case sc2::UNIT_TYPEID::TERRAN_CYCLONE:
		case sc2::UNIT_TYPEID::TERRAN_LIBERATOR:
		case sc2::UNIT_TYPEID::TERRAN_LIBERATORAG:
		case sc2::UNIT_TYPEID::TERRAN_POINTDEFENSEDRONE:
			return true;
		default:
			return false;
	}
}

bool Unit::isPsionic() const
{

	switch ((sc2::UNIT_TYPEID)getAPIUnitType())
	{
		case sc2::UNIT_TYPEID::PROTOSS_SENTRY:
		case sc2::UNIT_TYPEID::PROTOSS_HIGHTEMPLAR:
		case sc2::UNIT_TYPEID::PROTOSS_DARKTEMPLAR:
		case sc2::UNIT_TYPEID::PROTOSS_ARCHON:
		case sc2::UNIT_TYPEID::PROTOSS_WARPPRISM:
		case sc2::UNIT_TYPEID::PROTOSS_WARPPRISMPHASING:
		case sc2::UNIT_TYPEID::PROTOSS_MOTHERSHIP:
		case sc2::UNIT_TYPEID::PROTOSS_ORACLE:
		case sc2::UNIT_TYPEID::TERRAN_GHOST:
		case sc2::UNIT_TYPEID::ZERG_QUEEN:
		case sc2::UNIT_TYPEID::ZERG_QUEENBURROWED:
		case sc2::UNIT_TYPEID::ZERG_INFESTOR:
		case sc2::UNIT_TYPEID::ZERG_INFESTORBURROWED:
		case sc2::UNIT_TYPEID::ZERG_VIPER:
			return true;
		default:
			return false;
	}
}

bool Unit::isMassive() const
{

	switch ((sc2::UNIT_TYPEID)getAPIUnitType())
	{
		case sc2::UNIT_TYPEID::TERRAN_THOR:
		case sc2::UNIT_TYPEID::TERRAN_THORAP:
		case sc2::UNIT_TYPEID::TERRAN_BATTLECRUISER:
		case sc2::UNIT_TYPEID::ZERG_ULTRALISK:
		case sc2::UNIT_TYPEID::ZERG_BROODLORD:
		case sc2::UNIT_TYPEID::PROTOSS_ARCHON:
		case sc2::UNIT_TYPEID::PROTOSS_COLOSSUS:
		case sc2::UNIT_TYPEID::PROTOSS_CARRIER:
		case sc2::UNIT_TYPEID::PROTOSS_MOTHERSHIP:
		case sc2::UNIT_TYPEID::PROTOSS_TEMPEST:
			return true;
		default:
			return false;
	}
}

bool Unit::isAlive() const
{
    BOT_ASSERT(isValid(), "Unit is not valid");
#ifdef SC2API
	return m_unit->is_alive;
#else
    return m_unit->getHitPoints() > 0;
#endif
}

bool Unit::isPowered() const
{
    BOT_ASSERT(isValid(), "Unit is not valid");
#ifdef SC2API
    return m_unit->is_powered;
#else
    return m_unit->isPowered();
#endif
}

bool Unit::isIdle() const
{
    BOT_ASSERT(isValid(), "Unit is not valid");
#ifdef SC2API
    return m_unit->orders.empty();
#else
    return m_unit->isIdle() && !m_unit->isMoving() && !m_unit->isGatheringGas() && !m_unit->isGatheringMinerals();
#endif
}

bool Unit::isBurrowed() const
{
    BOT_ASSERT(isValid(), "Unit is not valid");
#ifdef SC2API
    return m_unit->is_burrowed;
#else
    return m_unit->isBurrowed();
#endif
}

bool Unit::isValid() const
{
    return m_unit != nullptr;
}

bool Unit::isVisible() const
{
	return m_unit->display_type == sc2::Unit::DisplayType::Visible;
}

void Unit::stop() const
{
    BOT_ASSERT(isValid(), "Unit is not valid");
	m_bot->Actions()->UnitCommand(m_unit, sc2::ABILITY_ID::STOP);
}

void Unit::cancel() const
{
	BOT_ASSERT(isValid(), "Unit is not valid");
	BOT_ASSERT(this->getType().isBuilding(), "Doesn't handle units right now.");
	m_bot->Actions()->UnitCommand(m_unit, sc2::ABILITY_ID::CANCEL_LAST);
}

void Unit::attackUnit(const Unit & target) const
{
    BOT_ASSERT(isValid(), "Unit is not valid");
    BOT_ASSERT(target.isValid(), "Target is not valid");
#ifdef SC2API
    m_bot->Actions()->UnitCommand(m_unit, sc2::ABILITY_ID::ATTACK, target.getUnitPtr());
#else
    m_unit->attack(target.getUnitPtr());
#endif
}

void Unit::attackMove(const CCPosition & targetPosition) const
{
    BOT_ASSERT(isValid(), "Unit is not valid");
#ifdef SC2API
    m_bot->Actions()->UnitCommand(m_unit, sc2::ABILITY_ID::ATTACK, targetPosition);
#else
    m_unit->attack(targetPosition);
#endif
}

void Unit::move(const CCPosition & targetPosition) const
{
    BOT_ASSERT(isValid(), "Unit is not valid");
#ifdef SC2API
    m_bot->Actions()->UnitCommand(m_unit, sc2::ABILITY_ID::MOVE, targetPosition);
#else
    m_unit->move(targetPosition);
#endif
}

void Unit::move(const CCTilePosition & targetPosition) const
{
    BOT_ASSERT(isValid(), "Unit is not valid");
#ifdef SC2API
    m_bot->Actions()->UnitCommand(m_unit, sc2::ABILITY_ID::MOVE, CCPosition((float)targetPosition.x, (float)targetPosition.y));
#else
    m_unit->move(CCPosition(targetPosition));
#endif
}

void Unit::patrol(const CCPosition & targetPosition) const
{
	BOT_ASSERT(isValid(), "Unit is not valid");
	m_bot->Actions()->UnitCommand(m_unit, sc2::ABILITY_ID::PATROL, targetPosition);
}

void Unit::patrol(const CCTilePosition & targetPosition) const
{
	BOT_ASSERT(isValid(), "Unit is not valid");
	m_bot->Actions()->UnitCommand(m_unit, sc2::ABILITY_ID::PATROL, CCPosition((float)targetPosition.x, (float)targetPosition.y));
}

void Unit::rightClick(const Unit & target) const
{
	BOT_ASSERT(isValid(), "Unit is not valid");
#ifdef SC2API
	m_bot->Actions()->UnitCommand(m_unit, sc2::ABILITY_ID::SMART, target.getUnitPtr());
#else
	m_unit->rightClick(target.getUnitPtr());
#endif
}

void Unit::shiftRightClick(const Unit & target) const
{
	BOT_ASSERT(isValid(), "Unit is not valid");
#ifdef SC2API
	m_bot->Actions()->UnitCommand(m_unit, sc2::ABILITY_ID::SMART, target.getUnitPtr(), true);
#else
	m_unit->rightClick(target.getUnitPtr());
#endif
}

void Unit::rightClick(const CCPosition position) const
{
	m_bot->Actions()->UnitCommand(m_unit, sc2::ABILITY_ID::SMART, sc2::Point2D(position.x, position.y));
}

void Unit::shiftRightClick(const CCPosition position) const
{
	m_bot->Actions()->UnitCommand(m_unit, sc2::ABILITY_ID::SMART, sc2::Point2D(position.x, position.y), true);
}

void Unit::repair(const Unit & target) const
{
    BOT_ASSERT(isValid(), "Unit is not valid");
#ifdef SC2API
    m_bot->Actions()->UnitCommand(m_unit, sc2::ABILITY_ID::EFFECT_REPAIR, target.getUnitPtr());
#else
    rightClick(target);
#endif
}

void Unit::build(const UnitType & buildingType, CCTilePosition pos) const
{
    //BOT_ASSERT(m_bot->Map().isConnected(getTilePosition(), pos), ("Error: Build Position is not connected to worker (build pos:[" + std::to_string(pos.x) + ", " + std::to_string(pos.y) + "], unit pos:[" + std::to_string(getTilePosition().x) + ", " + std::to_string(getTilePosition().y) + "], building type:" + buildingType.getName() + ")").c_str());
	
	BOT_ASSERT(isValid(), "Unit is not valid");
#ifdef SC2API
    m_bot->Actions()->UnitCommand(m_unit, m_bot->Data(buildingType).buildAbility, Util::GetPosition(pos));
#else
    m_unit->build(buildingType.getAPIUnitType(), pos);
#endif
}

void Unit::buildTarget(const UnitType & buildingType, const Unit & target) const
{
    BOT_ASSERT(isValid(), "Unit is not valid");
#ifdef SC2API
    m_bot->Actions()->UnitCommand(m_unit, m_bot->Data(buildingType).buildAbility, target.getUnitPtr());
#else
    BOT_ASSERT(false, "buildTarget shouldn't be called for BWAPI bots");
#endif
}

void Unit::harvestTarget(const Unit & target) const
{
	BOT_ASSERT(isValid(), "Unit is not valid");
#ifdef SC2API
	m_bot->Actions()->UnitCommand(m_unit, sc2::ABILITY_ID::HARVEST_GATHER, target.getUnitPtr());
#else
	BOT_ASSERT(false, "buildTarget shouldn't be called for BWAPI bots");
#endif
}

void Unit::train(const UnitType & type) const
{
    BOT_ASSERT(isValid(), "Unit is not valid");
#ifdef SC2API
    m_bot->Actions()->UnitCommand(m_unit, m_bot->Data(type).buildAbility);
#else
    m_unit->train(type.getAPIUnitType());
#endif
}

void Unit::morph(const UnitType & type) const
{
    BOT_ASSERT(isValid(), "Unit is not valid");
#ifdef SC2API
    m_bot->Actions()->UnitCommand(m_unit, m_bot->Data(type).buildAbility);
#else
    m_unit->morph(type.getAPIUnitType());
#endif
}

sc2::AvailableAbilities Unit::getAbilities() const
{
	return m_bot->Query()->GetAbilitiesForUnit(m_unit);
}

/*
 * This method queries the game to check if the ability is available.
 * Since queries are slow, this method should not be used on every frame.
 */
bool Unit::useAbility(const sc2::ABILITY_ID abilityId) const
{
	BOT_ASSERT(isValid(), "Unit is not valid");

	if (abilityId == sc2::ABILITY_ID::EFFECT_STIM)
	{
		sc2::BUFF_ID stimpack = m_unitType.getAPIUnitType() == sc2::UNIT_TYPEID::TERRAN_MARINE ? sc2::BUFF_ID::STIMPACK : sc2::BUFF_ID::STIMPACKMARAUDER;
		if(std::find(m_unit->buffs.begin(), m_unit->buffs.end(), stimpack) != m_unit->buffs.end())
		{
			// Stimpack already in use
			return false;
		}
	}

	auto query = m_bot->Query();
	auto abilities = m_bot->Observation()->GetAbilityData();
	sc2::AvailableAbilities available_abilities = query->GetAbilitiesForUnit(m_unit);
	for (const sc2::AvailableAbility & available_ability : available_abilities.abilities)
	{
		if (available_ability.ability_id >= abilities.size()) { continue; }
		const sc2::AbilityData & ability = abilities[available_ability.ability_id];
		if (ability.ability_id == abilityId) {
			m_bot->GetCommandMutex().lock();
			Micro::SmartAbility(m_unit, ability.ability_id, *m_bot);
			m_bot->GetCommandMutex().unlock();
			return true;
		}
	}

	return false;
}

bool Unit::isConstructing(const UnitType & type) const
{
#ifdef SC2API
	BOT_ASSERT(isValid(), "Cannot check if unit is constructing because unit ptr is null");

    sc2::AbilityID buildAbility = m_bot->Data(type).buildAbility;
    return (getUnitPtr()->orders.size() > 0) && (getUnitPtr()->orders[0].ability_id == buildAbility);
#else
    return m_unit->isConstructing();
#endif
}

bool Unit::isConstructingAnything() const
{
#ifdef SC2API
	BOT_ASSERT(isValid(), "Cannot check if unit is constructing because unit ptr is null");
	auto orders = getUnitPtr()->orders;
	if (orders.size() > 0)
	{
		auto tag = getTag();
		for (auto & b : m_bot->Buildings().getBuildings())
		{
			if (b.builderUnit.isValid() && b.builderUnit.getTag() == tag && orders.at(0).ability_id != sc2::ABILITY_ID::PATROL)
			{
				return true;
			}
		}
		return false;
	}

	return false;
#else
	return m_unit->isConstructing();
#endif
}

bool Unit::isCounterToUnit(const Unit& unit) const
{
	const bool canAttack = unit.isFlying() ? Util::CanUnitAttackAir(m_unit, *m_bot) : Util::CanUnitAttackGround(m_unit, *m_bot);
	if (!canAttack)
		return false;
	const bool isAttacked = isFlying() ? Util::CanUnitAttackAir(unit.getUnitPtr(), *m_bot) : Util::CanUnitAttackGround(unit.getUnitPtr(), *m_bot);
	return !isAttacked;
}

bool Unit::isReturningCargo() const
{
	return m_bot->Workers().getWorkerData().isReturningCargo(*this);
}

void Unit::getBuildingLimits(CCTilePosition & bottomLeft, CCTilePosition & topRight) const
{
	const CCTilePosition centerTile = Util::GetTilePosition(getPosition());
	const int size = floor(getUnitPtr()->radius * 2);
	const int flooredHalfSize = floor(size / 2.f);
	const int ceiledHalfSize = ceil(size / 2.f);
	int minX = centerTile.x - flooredHalfSize;
	const int maxX = centerTile.x + ceiledHalfSize;
	int minY = centerTile.y - flooredHalfSize;
	const int maxY = centerTile.y + ceiledHalfSize;

	//special cases
	if (getType().isAttackingBuilding())
	{
		//attacking buildings have a smaller radius, so we must increase the min to cover more tiles
		minX = centerTile.x - ceiledHalfSize;
		minY = centerTile.y - ceiledHalfSize;
	}
	else if (getType().isMineral())
	{
		//minerals are rectangles instead of squares
		minY = centerTile.y;
	}

	bottomLeft = CCTilePosition(minX, minY);
	topRight = CCTilePosition(maxX, maxY);
}