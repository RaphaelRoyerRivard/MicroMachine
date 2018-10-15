#pragma once
#include "Common.h"

class CCBot;
class MetaType;

class MetaTypeEnum
{
	public:
		//TERRAN
			//Buildings
		static MetaType TechLab;
		static MetaType Reactor;
		static MetaType Barracks;
		static MetaType BarracksTechLab;
		static MetaType BarracksReactor;
		static MetaType Factory;
		static MetaType FactoryTechLab;
		static MetaType FactoryReactor;
		static MetaType Starport;
		static MetaType StarportTechLab;
		static MetaType StarportReactor;
		static MetaType CommandCenter;
		static MetaType OrbitalCommand;
		static MetaType PlanetaryFortress;
		static MetaType SupplyDepot;
		static MetaType Refinery;
		static MetaType EngineeringBay;
		static MetaType MissileTurret;
			//Units
		static MetaType Marine;
		static MetaType Marauder;
		static MetaType Reaper;
		static MetaType Hellion;
		static MetaType Banshee;
		static MetaType Viking;
			//Upgrades
		static MetaType TerranInfantryWeaponsLevel1;
		static MetaType TerranInfantryWeaponsLevel2;
		static MetaType TerranInfantryWeaponsLevel3;
		static MetaType TerranInfantryArmorsLevel1;
		static MetaType TerranInfantryArmorsLevel2;
		static MetaType TerranInfantryArmorsLevel3;
		static MetaType TerranVehicleWeaponsLevel1;
		static MetaType TerranVehicleWeaponsLevel2;
		static MetaType TerranVehicleWeaponsLevel3;
		static MetaType TerranShipWeaponsLevel1;
		static MetaType TerranShipWeaponsLevel2;
		static MetaType TerranShipWeaponsLevel3;
		static MetaType TerranVehicleAndShipArmorsLevel1;
		static MetaType TerranVehicleAndShipArmorsLevel2;
		static MetaType TerranVehicleAndShipArmorsLevel3;
		static MetaType Stimpack;
		static MetaType InfernalPreIgniter;

		//PROTOSS
			//Buildings
			//Units
		static MetaType Adept;
			//Upgrades
		static MetaType ProtossGroundWeaponsLevel1;
		static MetaType ProtossGroundWeaponsLevel2;
		static MetaType ProtossGroundWeaponsLevel3;
		static MetaType ProtossGroundArmorsLevel1;
		static MetaType ProtossGroundArmorsLevel2;
		static MetaType ProtossGroundArmorsLevel3;
		static MetaType ProtossAirWeaponsLevel1;
		static MetaType ProtossAirWeaponsLevel2;
		static MetaType ProtossAirWeaponsLevel3;
		static MetaType ProtossAirArmorsLevel1;
		static MetaType ProtossAirArmorsLevel2;
		static MetaType ProtossAirArmorsLevel3;
		static MetaType ProtossShieldsLevel1;
		static MetaType ProtossShieldsLevel2;
		static MetaType ProtossShieldsLevel3;

		//ZERG
			//Buildings
			//Units
			//Upgrades
		static MetaType ZergMeleeWeaponsLevel1;
		static MetaType ZergMeleeWeaponsLevel2;
		static MetaType ZergMeleeWeaponsLevel3;
		static MetaType ZergMissileWeaponsLevel1;
		static MetaType ZergMissileWeaponsLevel2;
		static MetaType ZergMissileWeaponsLevel3;
		static MetaType ZergGroundArmorsLevel1;
		static MetaType ZergGroundArmorsLevel2;
		static MetaType ZergGroundArmorsLevel3;
		static MetaType ZergFlyerWeaponsLevel1;
		static MetaType ZergFlyerWeaponsLevel2;
		static MetaType ZergFlyerWeaponsLevel3;
		static MetaType ZergFlyerArmorsLevel1;
		static MetaType ZergFlyerArmorsLevel2;
		static MetaType ZergFlyerArmorsLevel3;

		static void Initialize(CCBot & m_bot);
};