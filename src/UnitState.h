#pragma once
#include "Common.h"

class UnitState
{
	bool wasUpdated;
	CCHealth m_hitPoints;
	CCHealth m_previousHitPoints;
	CCHealth m_shields;
	CCHealth m_previousShields;
	CCHealth m_energy;
	CCHealth m_previousEnergy;

	static const int THREAT_CHECK_EVERY_X_FRAME = 5;
	static const int CONSIDER_X_LAST_THREAT_CHECK = 3;
	bool m_recentThreat[CONSIDER_X_LAST_THREAT_CHECK];
public:

	UnitState();
	UnitState(CCHealth hitPoints, CCHealth shields, CCHealth energy);
	void Reset();
	void Update();
	void Update(CCHealth hitPoints, CCHealth shields, CCHealth energy);
	bool WasUpdated();
	void UpdateThreat(bool hasThreat);

	bool WasAttacked();
	int GetDamageTaken();
	int GetHealed();
	bool HadRecentTreats();
};