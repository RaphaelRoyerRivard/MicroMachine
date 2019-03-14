#pragma once
#include "Common.h"

class UnitState
{
	bool wasUpdated;
	int m_hitPoints;
	int m_previousHitPoints;
	int m_shields;
	int m_previousShields;
	int m_energy;
	int m_previousEnergy;
	int m_damageTaken;
	int m_totalRecentDamage = 0;
	
	const sc2::Unit* unit;

	static const int THREAT_CHECK_EVERY_X_FRAME = 5;
	static const int CONSIDER_X_LAST_THREAT_CHECK = 3;
	static const int REMEMBER_X_LAST_DOMMAGE_TAKEN = 24;//1 sec
	bool m_recentThreat[CONSIDER_X_LAST_THREAT_CHECK];
	std::vector<int> m_recentDamage;
public:

	UnitState();
	UnitState(CCHealth hitPoints, CCHealth shields, CCHealth energy, const sc2::Unit* unitPtr);
	void Reset();
	void Update();
	void Update(CCHealth hitPoints, CCHealth shields, CCHealth energy);
	bool WasUpdated();
	void UpdateThreat(bool hasThreat);

	bool WasAttacked();
	int GetDamageTaken();
	int GetRecentDamageTaken();
	int GetHealed();
	bool HadRecentTreats();
	sc2::UNIT_TYPEID GetType();
};