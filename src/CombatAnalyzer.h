#pragma once

#include "sc2api/sc2_api.h"
#include "Common.h"
#include "UnitState.h"
#include "Unit.h"
#include <list>

class CCBot;

class CombatAnalyzer {
	CCBot & m_bot;
	uint32_t m_lastLowPriorityFrame = 0;
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
	std::set<const sc2::Unit *> enemyPickedUpUnits;

	bool m_enemyHasCombatAirUnit = false;

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

	int getUnitUpgradeArmor(const sc2::Unit* unit);
	int getUnitUpgradeWeapon(const sc2::Unit* unit);

	void clearAreasUnderDetection();
	void UpdateTotalHealthLoss();
	void drawDamageHealthRatio();
	void drawAreasUnderDetection();
public:
	int selfTerranBioArmor = 0;
	int selfTerranBioWeapon = 0;
	int selfTerranGroundMechWeapon = 0;
	int selfTerranAirMechWeapon = 0;
	int selfTerranMechArmor = 0;

	int selfProtossGroundArmor = 0;
	int selfProtossGroundWeapon = 0;
	int selfProtossAirArmor = 0;
	int selfProtossAirWeapon = 0;
	int selfProtossShield = 0;

	int selfZergGroundArmor = 0;
	int selfZergGroundMeleeWeapon = 0;
	int selfZergGroundRangedWeapon = 0;
	int selfZergAirArmor = 0;
	int selfZergAirWeapon = 0;

	int opponentTerranBioArmor = 0;
	int opponentTerranBioWeapon = 0;
	int opponentTerranGroundMechWeapon = 0;
	int opponentTerranAirMechWeapon = 0;
	int opponentTerranMechArmor = 0;

	int opponentProtossGroundArmor = 0;
	int opponentProtossGroundWeapon = 0;
	int opponentProtossAirArmor = 0;
	int opponentProtossAirWeapon = 0;
	int opponentProtossShield = 0;

	int opponentZergGroundArmor = 0;
	int opponentZergGroundMeleeWeapon = 0;
	int opponentZergGroundRangedWeapon = 0;
	int opponentZergAirArmor = 0;
	int opponentZergAirWeapon = 0;

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
	const UnitState & getUnitState(const sc2::Unit * unit);
	void increaseDeadEnemy(sc2::UNIT_TYPEID type);
	bool shouldProduceGroundAntiGround();
	bool shouldProduceGroundAntiAir();
	bool shouldProduceAirAntiGround();
	bool shouldProduceAirAntiAir();
	bool enemyHasCombatAirUnit() const { return m_enemyHasCombatAirUnit; }
	void detectUpgrades(Unit & unit, UnitState & state);
	void detectTechs(Unit & unit, UnitState & state);
};