#include "UnitState.h"

UnitState::UnitState()
{

}

UnitState::UnitState(CCHealth hitPoints, CCHealth shields, CCHealth energy)
{
	m_previousHitPoints = hitPoints;
	m_previousShields = shields;
	m_previousEnergy = energy;

	m_hitPoints = hitPoints;
	m_shields = shields;
	m_energy = energy;
}

void UnitState::Reset()
{
	wasUpdated = false;
}

void UnitState::Update()
{
	wasUpdated = true;
}

void UnitState::Update(CCHealth hitPoints, CCHealth shields, CCHealth energy)
{
	wasUpdated = true;

	m_previousHitPoints = m_hitPoints;
	m_previousShields = m_shields;
	m_previousEnergy = m_energy;

	m_hitPoints = hitPoints;
	m_shields = shields;
	m_energy = energy;
}

bool UnitState::WasUpdated()
{
	return wasUpdated;
}

void UnitState::UpdateThreat(bool hasThreat)
{
	for (int i = 0; i < CONSIDER_X_LAST_THREAT_CHECK - 1; i++)
	{
		m_recentThreat[i] = m_recentThreat[i + 1];
	}
	m_recentThreat[CONSIDER_X_LAST_THREAT_CHECK - 1] = hasThreat;
}

bool UnitState::WasAttacked()
{
	return GetDamageTaken() > 0;
}

int UnitState::GetDamageTaken()
{
	int damage = m_previousShields - m_shields + m_previousHitPoints - m_hitPoints;
	return damage > 0 ? damage : 0;
}

int UnitState::GetHealed()
{
	int heal = m_shields - m_previousShields + m_hitPoints - m_previousHitPoints;
	return heal > 0 ? heal : 0;
}

bool UnitState::HadRecentTreats()
{
	for (int i = 0; i < CONSIDER_X_LAST_THREAT_CHECK; i++)
	{
		if (m_recentThreat[i])
		{
			return true;
		}
	}
	return false;
}