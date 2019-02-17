#include "UnitInfoManager.h"
#include "Util.h"
#include "CCBot.h"
#include "Unit.h"

#include <sstream>

UnitInfoManager::UnitInfoManager(CCBot & bot)
    : m_bot(bot)
{

}

void UnitInfoManager::onStart()
{

}

void UnitInfoManager::onFrame()
{
    updateUnitInfo();
    drawUnitInformation(100, 100);
    drawSelectedUnitDebugInfo();
	drawUnitID();
}

void UnitInfoManager::updateUnitInfo()
{
    m_units[Players::Self].clear();
    m_units[Players::Enemy].clear();
    m_units[Players::Neutral].clear();

    for (auto & unit : m_bot.GetUnits())
    {
		if (!unit.isAlive())
		{
			continue;
		}
        updateUnit(unit);
        m_units[unit.getPlayer()].push_back(unit);     
    }

    // remove bad enemy units
    m_unitData[Players::Self].removeBadUnits();
    m_unitData[Players::Enemy].removeBadUnits();
    m_unitData[Players::Neutral].removeBadUnits();
}

const std::map<Unit, UnitInfo> & UnitInfoManager::getUnitInfoMap(CCPlayer player) const
{
    return getUnitData(player).getUnitInfoMap();
}

const std::vector<Unit> & UnitInfoManager::getUnits(CCPlayer player) const
{
    BOT_ASSERT(m_units.find(player) != m_units.end(), "Couldn't find player units: %d", player);

    return m_units.at(player);
}

//static std::string GetAbilityText(sc2::AbilityID ability_id) {
//    std::string str;
//    str += sc2::AbilityTypeToName(ability_id);
//    str += " (";
//    str += std::to_string(uint32_t(ability_id));
//    str += ")";
//    return str;
//}

void UnitInfoManager::drawSelectedUnitDebugInfo()
{
//#ifdef SC2API
//    const sc2::Unit * unit;
//    for (auto u : m_bot.Observation()->GetUnits()) 
//    {
//        if (u->is_selected && u->alliance == sc2::Unit::Self) {
//            unit = u;
//            break;
//        }
//    }
//
//    if (!unit) { return; }
//
//    auto query = m_bot.Query();
//    auto abilities = m_bot.Observation()->GetAbilityData();
//
//    std::string debug_txt;
//    debug_txt = UnitTypeToName(unit->unit_type);
//    if (debug_txt.length() < 1) 
//    {
//        debug_txt = "(Unknown name)";
//        assert(0);
//    }
//    debug_txt += " (" + std::to_string(unit->unit_type) + ")";
//        
//    sc2::AvailableAbilities available_abilities = query->GetAbilitiesForUnit(unit);
//    if (available_abilities.abilities.size() < 1) 
//    {
//        std::cout << "No abilities available for this unit" << std::endl;
//    }
//    else 
//    {
//        for (const sc2::AvailableAbility & available_ability : available_abilities.abilities) 
//        {
//            if (available_ability.ability_id >= abilities.size()) { continue; }
//
//            const sc2::AbilityData & ability = abilities[available_ability.ability_id];
//
//            debug_txt += GetAbilityText(ability.ability_id) + "\n";
//        }
//    }
//    m_bot.Map().drawText(unit->pos, debug_txt, CCColor(0, 255, 0));
//
//    // Show the direction of the unit.
//    sc2::Point3D p1; // Use this to show target distance.
//    {
//        const float length = 5.0f;
//        sc2::Point3D p0 = unit->pos;
//        p0.z += 0.1f; // Raise the line off the ground a bit so it renders more clearly.
//        p1 = unit->pos;
//        assert(unit->facing >= 0.0f && unit->facing < 6.29f);
//        p1.x += length * std::cos(unit->facing);
//        p1.y += length * std::sin(unit->facing);
//        m_bot.Map().drawLine(p0, p1, CCColor(255, 255, 0));
//    }
//
//    // Box around the unit.
//    {
//        sc2::Point3D p_min = unit->pos;
//        p_min.x -= 2.0f;
//        p_min.y -= 2.0f;
//        p_min.z -= 2.0f;
//        sc2::Point3D p_max = unit->pos;
//        p_max.x += 2.0f;
//        p_max.y += 2.0f;
//        p_max.z += 2.0f;
//        m_bot.Map().drawBox(p_min, p_max, CCColor(0, 0, 255));
//    }
//
//    // Sphere around the unit.
//    {
//        sc2::Point3D p = unit->pos;
//        p.z += 0.1f; // Raise the line off the ground a bit so it renders more clearly.
//        m_bot.Map().drawCircle(p, 1.25f, CCColor(255, 0, 255));
//    }
//
//    // Pathing query to get the target.
//    bool has_target = false;
//    sc2::Point3D target;
//    std::string target_info;
//    for (const sc2::UnitOrder& unit_order : unit->orders)
//    {
//        // TODO: Need to determine if there is a target point, no target point, or the target is a unit/snapshot.
//        target.x = unit_order.target_pos.x;
//        target.y = unit_order.target_pos.y;
//        target.z = p1.z;
//        has_target = true;
//
//        target_info = "Target:\n";
//        if (unit_order.target_unit_tag != 0x0LL) {
//            target_info += "Tag: " + std::to_string(unit_order.target_unit_tag) + "\n";
//        }
//        if (unit_order.progress != 0.0f && unit_order.progress != 1.0f) {
//            target_info += "Progress: " + std::to_string(unit_order.progress) + "\n";
//        }
//
//        // Perform the pathing query.
//        {
//            float distance = query->PathingDistance(unit->pos, target);
//            target_info += "\nPathing dist: " + std::to_string(distance);
//        }
//
//        break;
//    }
//
//    if (has_target)
//    {
//        sc2::Point3D p = target;
//        p.z += 0.1f; // Raise the line off the ground a bit so it renders more clearly.
//        m_bot.Map().drawCircle(target, 1.25f, CCColor(0, 0, 255));
//        m_bot.Map().drawText(p1, target_info, CCColor(255, 255, 0));
//    }
//#endif
}

// passing in a unit type of 0 returns a count of all units
size_t UnitInfoManager::getUnitTypeCount(CCPlayer player, UnitType type, bool completed, bool ignoreState) const
{
	sc2::UNIT_TYPEID typeID = type.getAPIUnitType();
	if (ignoreState)
	{
		//TODO PROTOSS AND ZERG NOT IMPLEMENTED
		switch (typeID)
		{
			case sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOT:
			case sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOTLOWERED:
				return m_bot.GetUnitCount(sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOT, completed) + m_bot.GetUnitCount(sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOTLOWERED, completed);
			case sc2::UNIT_TYPEID::TERRAN_BARRACKS:
			case sc2::UNIT_TYPEID::TERRAN_BARRACKSFLYING:
				return m_bot.GetUnitCount(sc2::UNIT_TYPEID::TERRAN_BARRACKS, completed) + m_bot.GetUnitCount(sc2::UNIT_TYPEID::TERRAN_BARRACKSFLYING, completed);
			case sc2::UNIT_TYPEID::TERRAN_FACTORY:
			case sc2::UNIT_TYPEID::TERRAN_FACTORYFLYING:
				return m_bot.GetUnitCount(sc2::UNIT_TYPEID::TERRAN_FACTORY, completed) + m_bot.GetUnitCount(sc2::UNIT_TYPEID::TERRAN_FACTORYFLYING, completed);
			case sc2::UNIT_TYPEID::TERRAN_STARPORT:
			case sc2::UNIT_TYPEID::TERRAN_STARPORTFLYING:
				return m_bot.GetUnitCount(sc2::UNIT_TYPEID::TERRAN_STARPORT, completed) + m_bot.GetUnitCount(sc2::UNIT_TYPEID::TERRAN_STARPORTFLYING, completed);
			case sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER:
			case sc2::UNIT_TYPEID::TERRAN_COMMANDCENTERFLYING:
				return m_bot.GetUnitCount(sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER, completed) + m_bot.GetUnitCount(sc2::UNIT_TYPEID::TERRAN_COMMANDCENTERFLYING, completed);
			case sc2::UNIT_TYPEID::TERRAN_ORBITALCOMMAND:
			case sc2::UNIT_TYPEID::TERRAN_ORBITALCOMMANDFLYING:
				return m_bot.GetUnitCount(sc2::UNIT_TYPEID::TERRAN_ORBITALCOMMAND, completed) + m_bot.GetUnitCount(sc2::UNIT_TYPEID::TERRAN_ORBITALCOMMANDFLYING, completed);
			case sc2::UNIT_TYPEID::TERRAN_HELLION:
			case sc2::UNIT_TYPEID::TERRAN_HELLIONTANK:
				return m_bot.GetUnitCount(sc2::UNIT_TYPEID::TERRAN_HELLION, completed) + m_bot.GetUnitCount(sc2::UNIT_TYPEID::TERRAN_HELLIONTANK, completed);
			case sc2::UNIT_TYPEID::TERRAN_LIBERATOR:
			case sc2::UNIT_TYPEID::TERRAN_LIBERATORAG:
				return m_bot.GetUnitCount(sc2::UNIT_TYPEID::TERRAN_LIBERATOR, completed) + m_bot.GetUnitCount(sc2::UNIT_TYPEID::TERRAN_LIBERATORAG, completed);
			case sc2::UNIT_TYPEID::TERRAN_SIEGETANK:
			case sc2::UNIT_TYPEID::TERRAN_SIEGETANKSIEGED:
				return m_bot.GetUnitCount(sc2::UNIT_TYPEID::TERRAN_SIEGETANK, completed) + m_bot.GetUnitCount(sc2::UNIT_TYPEID::TERRAN_SIEGETANKSIEGED, completed);
			case sc2::UNIT_TYPEID::TERRAN_THOR:
			case sc2::UNIT_TYPEID::TERRAN_THORAP:
				return m_bot.GetUnitCount(sc2::UNIT_TYPEID::TERRAN_THOR, completed) + m_bot.GetUnitCount(sc2::UNIT_TYPEID::TERRAN_THORAP, completed);
			case sc2::UNIT_TYPEID::TERRAN_WIDOWMINE:
			case sc2::UNIT_TYPEID::TERRAN_WIDOWMINEBURROWED:
				return m_bot.GetUnitCount(sc2::UNIT_TYPEID::TERRAN_WIDOWMINE, completed) + m_bot.GetUnitCount(sc2::UNIT_TYPEID::TERRAN_WIDOWMINEBURROWED, completed);
			case sc2::UNIT_TYPEID::TERRAN_VIKINGASSAULT:
			case sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER:
				return m_bot.GetUnitCount(sc2::UNIT_TYPEID::TERRAN_VIKINGASSAULT, completed) + m_bot.GetUnitCount(sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER, completed);
			default:
				return m_bot.GetUnitCount(typeID, completed);
		}
	}
	else
	{
		return m_bot.GetUnitCount(typeID, completed);
	}
}

void UnitInfoManager::drawUnitInformation(float x,float y) const
{
    if (!m_bot.Config().DrawEnemyUnitInfo)
    {
        return;
    }

    std::stringstream ss;

    // TODO: move this to unitData

    //// for each unit in the queue
    //for (auto & kv : m_)
    //{
    //    int numUnits =      m_unitData.at(Players::Self).getNumUnits(t);
    //    int numDeadUnits =  m_unitData.at(Players::Enemy).getNumDeadUnits(t);

    //    // if there exist units in the vector
    //    if (numUnits > 0)
    //    {
    //        ss << numUnits << "   " << numDeadUnits << "   " << sc2::UnitTypeToName(t) << "\n";
    //    }
    //}
    //
    //for (auto & kv : getUnitData(Players::Enemy).getUnitInfoMap())
    //{
    //    m_bot.Map().drawCircle(kv.second.lastPosition, 0.5f);
    //    m_bot.Map().drawText(kv.second.lastPosition, sc2::UnitTypeToName(kv.second.type));
    //}
    //
}

void UnitInfoManager::drawUnitID()
{
	if (!m_bot.Config().DrawUnitID)
	{
		return;
	}
	for (auto & unit : getUnits(Players::Self))
	{
		m_bot.Map().drawText(unit.getPosition(), std::to_string(unit.getTag()) + " (" + std::to_string(unit.getPosition().x) + ", " + std::to_string(unit.getPosition().y) + ")");
	}
}

void UnitInfoManager::updateUnit(const Unit & unit)
{
    m_unitData[unit.getPlayer()].updateUnit(unit);
}

// is the unit valid?
bool UnitInfoManager::isValidUnit(const Unit & unit)
{
    if (!unit.isValid()) { return false; }

    // if it's a weird unit, don't bother
    if (unit.getType().isEgg() || unit.getType().isLarva())
    {
        return false;
    }

    // if the position isn't valid throw it out
    if (!m_bot.Map().isValidPosition(unit.getPosition()))
    {
        return false;
    }
    
    // s'all good baby baby
    return true;
}

void UnitInfoManager::getNearbyForce(std::vector<UnitInfo> & unitInfo, CCPosition p, int player, float radius) const
{
    bool hasBunker = false;
    // for each unit we know about for that player
    for (const auto & kv : getUnitData(player).getUnitInfoMap())
    {
        const UnitInfo & ui(kv.second);

        // if it's a combat unit we care about
        // and it's finished! 
        if (ui.type.isCombatUnit() && Util::DistSq(ui.lastPosition, p) <= radius * radius)
        {
            // add it to the vector
            unitInfo.push_back(ui);
        }
    }
}

const UnitData & UnitInfoManager::getUnitData(CCPlayer player) const
{
    return m_unitData.find(player)->second;
}
