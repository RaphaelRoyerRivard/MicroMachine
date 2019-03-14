#pragma once

#include "sc2api/sc2_api.h"
#include "Common.h"
#include "UnitState.h"
#include "Unit.h"

class CCBot;

class CombatAnalyzer {
	CCBot & m_bot;
	float overallDamage;
	float overallRatio;

	std::map<CCUnitID, UnitState> m_unitStates;
	std::map<sc2::UNIT_TYPEID, float> ratio;
	std::map<sc2::UNIT_TYPEID, float> totalDamage;
	std::map<sc2::UNIT_TYPEID, float> totalhealthLoss;
	std::map<sc2::Tag, Unit> enemies;
	std::map<sc2::UNIT_TYPEID, int> aliveEnemiesCountByType;
	std::map<sc2::UNIT_TYPEID, int> deadEnemiesCountByType;
	std::map<sc2::UNIT_TYPEID, int> deadCountByType;
	std::list<std::pair<CCPosition, uint32_t>> m_areasUnderDetection;

	void clearAreasUnderDetection();
	void UpdateTotalHealthLoss();
	void drawDamageHealthRatio();
	void drawAreasUnderDetection();
public:
	CombatAnalyzer(CCBot & bot);
	void onStart();
	void onFrame();
	void lowPriorityChecks();
	std::list<std::pair<CCPosition, uint32_t>> & GetAreasUnderDetection() { return m_areasUnderDetection; };
	void increaseTotalDamage(float damageDealt, sc2::UNIT_TYPEID unittype);
	void increaseTotalHealthLoss(float healthLoss, sc2::UNIT_TYPEID unittype);
	float GetRatio(sc2::UNIT_TYPEID type);
	void UpdateRatio();
	void checkUnitsState();
	void checkUnitState(Unit unit);
	void increaseDeadEnemy(sc2::UNIT_TYPEID type);
};