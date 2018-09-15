#include "MetaTypeEnum.h"
#include "Util.h"
#include "CCBot.h"

//TERRAN
	//Buildings
MetaType MetaTypeEnum::TechLab;
MetaType MetaTypeEnum::Reactor;
MetaType MetaTypeEnum::Barracks;
MetaType MetaTypeEnum::BarracksTechLab;
MetaType MetaTypeEnum::BarracksReactor;
MetaType MetaTypeEnum::Factory;
MetaType MetaTypeEnum::FactoryTechLab;
MetaType MetaTypeEnum::FactoryReactor;
MetaType MetaTypeEnum::Starport;
MetaType MetaTypeEnum::StarportTechLab;
MetaType MetaTypeEnum::StarportReactor;
MetaType MetaTypeEnum::CommandCenter;
MetaType MetaTypeEnum::OrbitalCommand;
MetaType MetaTypeEnum::Refinery;
	//Units
MetaType MetaTypeEnum::Marine;
MetaType MetaTypeEnum::Marauder;
MetaType MetaTypeEnum::Reaper;
MetaType MetaTypeEnum::Hellion;
MetaType MetaTypeEnum::Banshee;
	//Upgrades
MetaType MetaTypeEnum::TerranInfantryWeaponsLevel1;
MetaType MetaTypeEnum::TerranInfantryWeaponsLevel2;
MetaType MetaTypeEnum::TerranInfantryWeaponsLevel3;
MetaType MetaTypeEnum::TerranInfantryArmorsLevel1;
MetaType MetaTypeEnum::TerranInfantryArmorsLevel2;
MetaType MetaTypeEnum::TerranInfantryArmorsLevel3;
MetaType MetaTypeEnum::TerranVehicleWeaponsLevel1;
MetaType MetaTypeEnum::TerranVehicleWeaponsLevel2;
MetaType MetaTypeEnum::TerranVehicleWeaponsLevel3;
MetaType MetaTypeEnum::TerranShipWeaponsLevel1;
MetaType MetaTypeEnum::TerranShipWeaponsLevel2;
MetaType MetaTypeEnum::TerranShipWeaponsLevel3;
MetaType MetaTypeEnum::TerranVehicleAndShipArmorsLevel1;
MetaType MetaTypeEnum::TerranVehicleAndShipArmorsLevel2;
MetaType MetaTypeEnum::TerranVehicleAndShipArmorsLevel3;

//PROTOSS
	//Buildings
	//Units
	//Upgrades
MetaType MetaTypeEnum::ProtossGroundWeaponsLevel1;
MetaType MetaTypeEnum::ProtossGroundWeaponsLevel2;
MetaType MetaTypeEnum::ProtossGroundWeaponsLevel3;
MetaType MetaTypeEnum::ProtossGroundArmorsLevel1;
MetaType MetaTypeEnum::ProtossGroundArmorsLevel2;
MetaType MetaTypeEnum::ProtossGroundArmorsLevel3;
MetaType MetaTypeEnum::ProtossAirWeaponsLevel1;
MetaType MetaTypeEnum::ProtossAirWeaponsLevel2;
MetaType MetaTypeEnum::ProtossAirWeaponsLevel3;
MetaType MetaTypeEnum::ProtossAirArmorsLevel1;
MetaType MetaTypeEnum::ProtossAirArmorsLevel2;
MetaType MetaTypeEnum::ProtossAirArmorsLevel3;
MetaType MetaTypeEnum::ProtossShieldsLevel1;
MetaType MetaTypeEnum::ProtossShieldsLevel2;
MetaType MetaTypeEnum::ProtossShieldsLevel3;

//ZERG
	//Buildings
	//Units
	//Upgrades
MetaType MetaTypeEnum::ZergMeleeWeaponsLevel1;
MetaType MetaTypeEnum::ZergMeleeWeaponsLevel2;
MetaType MetaTypeEnum::ZergMeleeWeaponsLevel3;
MetaType MetaTypeEnum::ZergMissileWeaponsLevel1;
MetaType MetaTypeEnum::ZergMissileWeaponsLevel2;
MetaType MetaTypeEnum::ZergMissileWeaponsLevel3;
MetaType MetaTypeEnum::ZergGroundArmorsLevel1;
MetaType MetaTypeEnum::ZergGroundArmorsLevel2;
MetaType MetaTypeEnum::ZergGroundArmorsLevel3;
MetaType MetaTypeEnum::ZergFlyerWeaponsLevel1;
MetaType MetaTypeEnum::ZergFlyerWeaponsLevel2;
MetaType MetaTypeEnum::ZergFlyerWeaponsLevel3;
MetaType MetaTypeEnum::ZergFlyerArmorsLevel1;
MetaType MetaTypeEnum::ZergFlyerArmorsLevel2;
MetaType MetaTypeEnum::ZergFlyerArmorsLevel3;

void MetaTypeEnum::Initialize(CCBot & m_bot)
{
	//TERRAN
		//Buildings
	MetaTypeEnum::TechLab = MetaType("TechLab", m_bot);
	MetaTypeEnum::Reactor = MetaType("Reactor", m_bot);
	MetaTypeEnum::Barracks = MetaType("Barracks", m_bot);
	MetaTypeEnum::BarracksTechLab = MetaType("BarracksTechLab", m_bot);
	MetaTypeEnum::BarracksReactor = MetaType("BarracksReactor", m_bot);
	MetaTypeEnum::Factory = MetaType("Factory", m_bot);
	MetaTypeEnum::FactoryTechLab = MetaType("FactoryTechLab", m_bot);
	MetaTypeEnum::FactoryReactor = MetaType("FactoryReactor", m_bot);
	MetaTypeEnum::Starport = MetaType("Starport", m_bot);
	MetaTypeEnum::StarportTechLab = MetaType("StarportTechLab", m_bot);
	MetaTypeEnum::StarportReactor = MetaType("StarportReactor", m_bot);
	MetaTypeEnum::CommandCenter = MetaType("CommandCenter", m_bot);
	MetaTypeEnum::OrbitalCommand = MetaType("OrbitalCommand", m_bot);
	MetaTypeEnum::Refinery = MetaType("Refinery", m_bot);
		//Units
	MetaTypeEnum::Marine = MetaType("Marine", m_bot);
	MetaTypeEnum::Marauder = MetaType("Marauder", m_bot);
	MetaTypeEnum::Reaper = MetaType("Reaper", m_bot);
	MetaTypeEnum::Hellion = MetaType("Hellion", m_bot);
	MetaTypeEnum::Banshee = MetaType("Banshee", m_bot);
		//Upgrades
	MetaTypeEnum::TerranInfantryWeaponsLevel1 = MetaType("TerranInfantryWeaponsLevel1", m_bot);
	MetaTypeEnum::TerranInfantryWeaponsLevel2 = MetaType("TerranInfantryWeaponsLevel2", m_bot);
	MetaTypeEnum::TerranInfantryWeaponsLevel3 = MetaType("TerranInfantryWeaponsLevel3", m_bot);
	MetaTypeEnum::TerranInfantryArmorsLevel1 = MetaType("TerranInfantryArmorsLevel1", m_bot);
	MetaTypeEnum::TerranInfantryArmorsLevel2 = MetaType("TerranInfantryArmorsLevel2", m_bot);
	MetaTypeEnum::TerranInfantryArmorsLevel3 = MetaType("TerranInfantryArmorsLevel3", m_bot);
	MetaTypeEnum::TerranVehicleWeaponsLevel1 = MetaType("TerranVehicleWeaponsLevel1", m_bot);
	MetaTypeEnum::TerranVehicleWeaponsLevel2 = MetaType("TerranVehicleWeaponsLevel2", m_bot);
	MetaTypeEnum::TerranVehicleWeaponsLevel3 = MetaType("TerranVehicleWeaponsLevel3", m_bot);
	MetaTypeEnum::TerranShipWeaponsLevel1 = MetaType("TerranShipWeaponsLevel1", m_bot);
	MetaTypeEnum::TerranShipWeaponsLevel2 = MetaType("TerranShipWeaponsLevel2", m_bot);
	MetaTypeEnum::TerranShipWeaponsLevel3 = MetaType("TerranShipWeaponsLevel3", m_bot);
	MetaTypeEnum::TerranVehicleAndShipArmorsLevel1 = MetaType("TerranVehicleAndShipArmorsLevel1", m_bot);
	MetaTypeEnum::TerranVehicleAndShipArmorsLevel2 = MetaType("TerranVehicleAndShipArmorsLevel2", m_bot);
	MetaTypeEnum::TerranVehicleAndShipArmorsLevel3 = MetaType("TerranVehicleAndShipArmorsLevel3", m_bot);

	//PROTOSS
		//Buildings
		//Units
		//Upgrades
	MetaTypeEnum::ProtossGroundWeaponsLevel1 = MetaType("ProtossGroundWeaponsLevel1", m_bot);
	MetaTypeEnum::ProtossGroundWeaponsLevel2 = MetaType("ProtossGroundWeaponsLevel2", m_bot);
	MetaTypeEnum::ProtossGroundWeaponsLevel3 = MetaType("ProtossGroundWeaponsLevel3", m_bot);
	MetaTypeEnum::ProtossGroundArmorsLevel1 = MetaType("ProtossGroundArmorsLevel1", m_bot);
	MetaTypeEnum::ProtossGroundArmorsLevel2 = MetaType("ProtossGroundArmorsLevel2", m_bot);
	MetaTypeEnum::ProtossGroundArmorsLevel3 = MetaType("ProtossGroundArmorsLevel3", m_bot);
	MetaTypeEnum::ProtossAirWeaponsLevel1 = MetaType("ProtossAirWeaponsLevel1", m_bot);
	MetaTypeEnum::ProtossAirWeaponsLevel2 = MetaType("ProtossAirWeaponsLevel2", m_bot);
	MetaTypeEnum::ProtossAirWeaponsLevel3 = MetaType("ProtossAirWeaponsLevel3", m_bot);
	MetaTypeEnum::ProtossAirArmorsLevel1 = MetaType("ProtossAirArmorsLevel1", m_bot);
	MetaTypeEnum::ProtossAirArmorsLevel2 = MetaType("ProtossAirArmorsLevel2", m_bot);
	MetaTypeEnum::ProtossAirArmorsLevel3 = MetaType("ProtossAirArmorsLevel3", m_bot);
	MetaTypeEnum::ProtossShieldsLevel1 = MetaType("ProtossShieldsLevel1", m_bot);
	MetaTypeEnum::ProtossShieldsLevel2 = MetaType("ProtossShieldsLevel2", m_bot);
	MetaTypeEnum::ProtossShieldsLevel3 = MetaType("ProtossShieldsLevel3", m_bot);

	//ZERG
		//Buildings
		//Units
		//Upgrades
	MetaTypeEnum::ZergMeleeWeaponsLevel1 = MetaType("ZergMeleeWeaponsLevel1", m_bot);
	MetaTypeEnum::ZergMeleeWeaponsLevel2 = MetaType("ZergMeleeWeaponsLevel2", m_bot);
	MetaTypeEnum::ZergMeleeWeaponsLevel3 = MetaType("ZergMeleeWeaponsLevel3", m_bot);
	MetaTypeEnum::ZergMissileWeaponsLevel1 = MetaType("ZergMissileWeaponsLevel1", m_bot);
	MetaTypeEnum::ZergMissileWeaponsLevel2 = MetaType("ZergMissileWeaponsLevel2", m_bot);
	MetaTypeEnum::ZergMissileWeaponsLevel3 = MetaType("ZergMissileWeaponsLevel3", m_bot);
	MetaTypeEnum::ZergGroundArmorsLevel1 = MetaType("ZergGroundArmorsLevel1", m_bot);
	MetaTypeEnum::ZergGroundArmorsLevel2 = MetaType("ZergGroundArmorsLevel2", m_bot);
	MetaTypeEnum::ZergGroundArmorsLevel3 = MetaType("ZergGroundArmorsLevel3", m_bot);
	MetaTypeEnum::ZergFlyerWeaponsLevel1 = MetaType("ZergFlyerWeaponsLevel1", m_bot);
	MetaTypeEnum::ZergFlyerWeaponsLevel2 = MetaType("ZergFlyerWeaponsLevel2", m_bot);
	MetaTypeEnum::ZergFlyerWeaponsLevel3 = MetaType("ZergFlyerWeaponsLevel3", m_bot);
	MetaTypeEnum::ZergFlyerArmorsLevel1 = MetaType("ZergFlyerArmorsLevel1", m_bot);
	MetaTypeEnum::ZergFlyerArmorsLevel2 = MetaType("ZergFlyerArmorsLevel2", m_bot);
	MetaTypeEnum::ZergFlyerArmorsLevel3 = MetaType("ZergFlyerArmorsLevel3", m_bot);
}