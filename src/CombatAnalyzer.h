#pragma once

#include "sc2api/sc2_api.h"
#include "Common.h"
#include "UnitState.h"

class CCBot;

class CombatAnalyzer {
	CCBot & m_bot;

	std::map<CCUnitID, UnitState> m_unitStates;
	std::map<sc2::UNIT_TYPEID, float> totalDamage;
	std::map<sc2::UNIT_TYPEID, float> totalhealthLoss;

	void drawDamageHealthRatio();
public:
	CombatAnalyzer(CCBot & bot);
	void onStart();
	void onFrame();
	void UpdateTotalHealthLoss();
	void increaseTotalDamage(float damageDealt, sc2::UNIT_TYPEID unittype);
	void increaseTotalHealthLoss(float healthLoss, sc2::UNIT_TYPEID unittype);
	void checkUnitsState();
};