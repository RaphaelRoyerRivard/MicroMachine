#include "UnitState.h"

UnitState::UnitState()
{

}

UnitState::UnitState(const sc2::Unit* unitPtr)
{
	m_previousHitPoints = unitPtr->health;
	m_previousShields = unitPtr->shield;
	m_previousEnergy = unitPtr->energy;

	m_hitPoints = unitPtr->health;
	m_shields = unitPtr->shield;
	m_energy = unitPtr->energy;

	unit = unitPtr;

	std::vector<int> temp(REMEMBER_X_LAST_DOMMAGE_TAKEN, 0);
	m_recentDamage = temp;
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

	int damage = m_previousShields - m_shields + m_previousHitPoints - m_hitPoints;
	m_damageTaken = damage > 0 ? damage : 0;

	m_totalRecentDamage -= m_recentDamage.at(REMEMBER_X_LAST_DOMMAGE_TAKEN - 1);
	std::rotate(m_recentDamage.begin(),
				m_recentDamage.end() - 1, // this will be the new first element
				m_recentDamage.end());
	m_recentDamage.at(0) = m_damageTaken;
	m_totalRecentDamage += m_damageTaken;
}

bool UnitState::WasUpdated() const
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

bool UnitState::WasAttacked() const
{
	return GetDamageTaken() > 0;
}

int UnitState::GetDamageTaken() const
{
	return m_damageTaken;
}

int UnitState::GetRecentDamageTaken() const
{
	return m_totalRecentDamage;
}

int UnitState::GetHealed() const
{
	int heal = m_shields - m_previousShields + m_hitPoints - m_previousHitPoints;
	return heal > 0 ? heal : 0;
}

bool UnitState::HadRecentTreats() const
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

sc2::UNIT_TYPEID UnitState::GetType() const
{
	return unit->unit_type;
}