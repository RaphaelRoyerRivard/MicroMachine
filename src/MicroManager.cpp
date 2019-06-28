#include "MicroManager.h"
#include "CCBot.h"

const float MIN_RANGE_DIFFERENCE_FOR_TARGET = 2.f;

MicroManager::MicroManager(CCBot & bot)
    : m_bot(bot)
{
}

void MicroManager::setUnits(const std::vector<Unit> & u)
{
    m_units = u;
}

const std::vector<Unit> & MicroManager::getUnits() const
{
    return m_units;
}

void MicroManager::regroup(const CCPosition & regroupPosition) const
{
    // for each of the units we have
    for (auto & unit : m_units)
    {
        BOT_ASSERT(unit.isValid(), "null unit in MicroManager regroup");

        if (Util::DistSq(unit, regroupPosition) > 4 * 4)
        {
            // regroup it
			Micro::SmartMove(unit.getUnitPtr(), regroupPosition, m_bot);
        }
        else
        {
            //defend itself near the regroupPosition
			Micro::SmartAttackMove(unit.getUnitPtr(), regroupPosition, m_bot);
        }
    }
}

float MicroManager::getAverageSquadSpeed() const
{
	return Util::getAverageSpeedOfUnits(m_units, m_bot);
}

float MicroManager::getAverageTargetsSpeed() const
{
	return Util::getAverageSpeedOfUnits(m_targets, m_bot);
}

float MicroManager::getSquadPower() const
{
	return Util::GetUnitsPower(m_units, m_targets, m_bot);
}

float MicroManager::getTargetsPower(const std::vector<Unit>& units) const
{
	return Util::GetUnitsPower(m_targets, units, m_bot);
}

float MicroManager::getAttackPriority(const sc2::Unit * attacker, const sc2::Unit * target, bool filterHighRangeUnits) const
{
	BOT_ASSERT(target, "null unit in getAttackPriority");

	if (!UnitType::isTargetable(target->unit_type))
		return 0.f;

	if (m_bot.IsParasited(target))
		return 0.f;

	// Ignoring invisible creep tumors
	const uint32_t lastGameLoop = m_bot.GetGameLoop() - 1;
	if ((target->unit_type == sc2::UNIT_TYPEID::ZERG_CREEPTUMOR
		|| target->unit_type == sc2::UNIT_TYPEID::ZERG_CREEPTUMORBURROWED
		|| target->unit_type == sc2::UNIT_TYPEID::ZERG_CREEPTUMORQUEEN)
		&& target->last_seen_game_loop < lastGameLoop)
	{
		return 0.f;
	}

	const float attackerRange = Util::GetAttackRangeForTarget(attacker, target, m_bot);
	const float targetRange = Util::GetAttackRangeForTarget(target, attacker, m_bot);

	if (filterHighRangeUnits)
	{
		if (targetRange + MIN_RANGE_DIFFERENCE_FOR_TARGET > attackerRange)
			return 0.f;
	}

	const float unitDps = Util::GetDpsForTarget(attacker, target, m_bot);
	if (unitDps == 0.f)	//Do not attack targets on which we would do no damage
		return 0.f;

	const float healthValue = pow(target->health + target->shield, 0.5f);		//the more health a unit has, the less it is prioritized
	const float distance = Util::Dist(attacker->pos, target->pos) + attacker->radius + target->radius;
	float proximityValue = 1.f;
	if (distance > attackerRange)
		proximityValue = std::pow(0.9f, distance - attackerRange);	//the more far a unit is, the less it is prioritized

	float invisModifier = 1.f;
	if (target->cloak == sc2::Unit::CloakedDetected)
		invisModifier = 2.f;
	else if (target->is_burrowed && target->unit_type != sc2::UNIT_TYPEID::ZERG_ZERGLINGBURROWED)
		invisModifier = 2.f;

	Unit targetUnit(target, m_bot);
	if (targetUnit.getType().isCombatUnit() || targetUnit.getType().isWorker())
	{
		float targetDps = Util::GetDpsForTarget(target, attacker, m_bot);
		if (target->unit_type == sc2::UNIT_TYPEID::TERRAN_BUNKER)
		{
			//We manually reduce the dps of the bunker because it only serve as a shield, units will spawn out of it when destroyed
			targetDps = 5.f;
		}
		float workerBonus = 1.f;
		if (targetUnit.getType().isWorker() && m_order.getType() != SquadOrderTypes::Defend)
		{
			if (attacker->unit_type != sc2::UNIT_TYPEID::TERRAN_HELLION)
			{
				workerBonus = 2.f;
			}

			// Reduce priority for workers that are going in a refinery
			const auto enemyRace = m_bot.GetPlayerRace(Players::Enemy);
			const auto enemyRefineryType = UnitType::getEnemyRefineryType(enemyRace);
			for (auto & refinery : m_bot.GetKnownEnemyUnits(enemyRefineryType))
			{
				const float refineryDist = Util::DistSq(refinery, target->pos);
				if (refineryDist < 2.5 * 2.5)
				{
					const CCPosition facingVector = Util::getFacingVector(target);
					if (Dot2D(facingVector, refinery.getPosition() - target->pos) > 0.99f)
					{
						if (m_bot.Config().DrawHarassInfo)
							m_bot.Map().drawCircle(targetUnit.getPosition(), 0.5f, sc2::Colors::Red);
						workerBonus *= 0.5f;
					}
				}
			}
		}

		float nonThreateningModifier = targetDps == 0.f ? 0.5f : 1.f;								//targets that cannot hit our unit are less prioritized
		if (targetUnit.getType().isAttackingBuilding())
		{
			nonThreateningModifier = targetDps == 0.f ? 1.f : 0.5f;		//for buildings, we prefer targetting them with units that will not get attacked back
		}
		const float flyingDetectorModifier = target->is_flying && UnitType::isDetector(target->unit_type) ? 2.f : 1.f;
		const float minionModifier = target->unit_type == sc2::UNIT_TYPEID::PROTOSS_INTERCEPTOR ? 0.1f : 1.f;	//units that can be respawned should be less prioritized
		return (targetDps + unitDps - healthValue + proximityValue * 50) * workerBonus * nonThreateningModifier * minionModifier * invisModifier * flyingDetectorModifier;
	}

	return (proximityValue * 50 - healthValue) * invisModifier / 100.f;		//we do not want non combat buildings to have a higher priority than other units
}