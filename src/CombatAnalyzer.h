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

	//Stats for each unit types
	std::map<sc2::UNIT_TYPEID, int> lightCountByType;
	int totalGroundLightCount = 0;
	int totalAirLightCount = 0;

	std::map<sc2::UNIT_TYPEID, int> armoredCountByType;
	int totalGroundArmoredCount = 0;
	int totalAirArmoredCount = 0;

	std::map<sc2::UNIT_TYPEID, int> bioCountByType;
	int totalGroundBioCount = 0;
	int totalAirBioCount = 0;

	std::map<sc2::UNIT_TYPEID, int> mechCountByType;
	int totalGroundMechCount = 0;
	int totalAirMechCount = 0;

	std::map<sc2::UNIT_TYPEID, int> psiCountByType;
	int totalGroundPsiCount = 0;
	int totalAirPsiCount = 0;

	std::map<sc2::UNIT_TYPEID, int> massiveCountByType;
	int totalGroundMassiveCount = 0;
	int totalAirMassiveCount = 0;

	int totalKnownWorkerCount = 0;
	int totalGroundUnitsCount = 0;
	int totalAirUnitsCount = 0;

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
	bool shouldProduceGroundAntiGround();
	bool shouldProduceGroundAntiAir();
	bool shouldProduceAirAntiGround();
	bool shouldProduceAirAntiAir();
};