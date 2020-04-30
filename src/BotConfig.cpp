#include "BotConfig.h"
#include "JSONTools.h"
#include <iostream>

BotConfig::BotConfig()
{
    ConfigFileFound = true;
    ConfigFileParsed = true;
    ConfigFileLocation = "BotConfig.txt";
    BotName = "MicroMachine";
    Authors = "David Churchill, Raphael Royer-Rivard, Antoine Theberge, Benjamin Ross, Jean-Philippe Croteau, Francois Genest";
    PrintInfoOnStart = false;
    StrategyName = "Protoss_ZealotRush";
    ReadDir = "read/";
    WriteDir = "write/";
    UseEnemySpecificStrategy = false;
    FoundEnemySpecificStrategy = false;
    UsingAutoObserver = false;

    SetLocalSpeed = 10;
    SetFrameSkip = 0;
    UserInput = true;
    CompleteMapInformation = false;

    BatchReplayMode = false;
    NbBatchReplay = 1;

    DrawProductionInfo = true;
    DrawTileInfo = false;
    DrawWalkableSectors = false;
	DrawBuildableSectors = false;
	DrawBuildable = true;
    DrawScoutInfo = false;
    DrawResourceInfo = false;
    DrawWorkerInfo = false;
    DrawModuleTimers = false;
    DrawReservedBuildingTiles = false;
    DrawBuildingInfo = false;
	DrawStartingRamp = false;
	DrawWall = false;
    DrawEnemyUnitInfo = false;
    DrawUnitTargetInfo = false;
	DrawSquadInfo = false;
	DrawUnitPowerInfo = false;
	DrawHarassInfo = false;
	DrawMemoryInfo = false;
	DrawUnitID = false;
	DrawProfilingInfo = false;
	DrawInfluenceMaps = false;
	DrawBlockedTiles = false;
	DrawRepairStation = false;
	DrawDamageHealthRatio = false;
	DrawRangedUnitActions = false;
	DrawResourcesProximity = false;
	DrawCombatInformation = false;
	TimeControl = false;

    KiteWithRangedUnits = true;
    ScoutHarassEnemy = true;
    MaxTargetDistance = 25.0f;
    MaxWorkerRepairDistance = 20.0f;

    ColorLineTarget = CCColor(255, 255, 255);
    ColorLineMineral = CCColor(0, 128, 128);
    ColorUnitNearEnemy = CCColor(255, 0, 0);
    ColorUnitNotNearEnemy = CCColor(0, 255, 0);

    WorkersPerRefinery = 3;
    BuildingSpacing = 1;
    PylonSpacing = 3;
	SelectStartingBuildBasedOnHistory = false;

    AlphaBetaDepth = 6;
    AlphaBetaMaxMilli = 100;
    UnitOwnAgent = false;
}

void BotConfig::readConfigFile()
{
    std::string config = JSONTools::ReadFile(ConfigFileLocation);
    if (config.length() == 0)
    {
        std::cerr << "Error: Config File Not Found or is Empty\n";
        std::cerr << "Config Filename: " << ConfigFileLocation << "\n";
        std::cerr << "The bot will not run without its configuration file\n";
        std::cerr << "Please check that the file exists and is not empty. Incomplete paths are relative to the bot .exe file\n";
        std::cerr << "You can change the config file location in Config::ConfigFile::ConfigFileLocation\n";
        ConfigFileFound = false;
        return;
    }

    std::ifstream file(ConfigFileLocation);
    json j;
    file >> j;

    /*if (parsingFailed)
    {
        std::cerr << "Error: Config File Found, but could not be parsed\n";
        std::cerr << "Config Filename: " << ConfigFileLocation << "\n";
        std::cerr << "The bot will not run without its configuration file\n";
        std::cerr << "Please check that the file exists, is not empty, and is valid JSON. Incomplete paths are relative to the bot .exe file\n";
        std::cerr << "You can change the config file location in Config::ConfigFile::ConfigFileLocation\n";
        ConfigFileParsed = false;
        return;
    }*/

    // Parse the Bot Info
    if (j.count("Bot Info") && j["Bot Info"].is_object())
    {
        const json & info = j["Bot Info"];
        JSONTools::ReadString("BotName", info, BotName);
        JSONTools::ReadString("Authors", info, Authors);
        JSONTools::ReadBool("PrintInfoOnStart", info, PrintInfoOnStart);
    }

    // Parse the Micro Options
    if (j.count("Micro") && j["Micro"].is_object())
    {
        const json & micro = j["Micro"];
        JSONTools::ReadBool("UnitOwnAgent", micro, UnitOwnAgent);
        JSONTools::ReadBool("SkipOneFrame", micro, SkipOneFrame);
        JSONTools::ReadBool("KiteWithRangedUnits", micro, KiteWithRangedUnits);
        JSONTools::ReadInt("MaxTargetDistance", micro, MaxTargetDistance);
        JSONTools::ReadInt("MaxWorkerRepairDistance", micro, MaxWorkerRepairDistance);
        JSONTools::ReadBool("ScoutHarassEnemy", micro, ScoutHarassEnemy);
        JSONTools::ReadBool("ClosestEnemy", micro, ClosestEnemy);
        JSONTools::ReadBool("WeakestEnemy", micro, WeakestEnemy);
		JSONTools::ReadBool("HighestPriority", micro, HighestPriority);
		JSONTools::ReadBool("EnableMultiThreading", micro, EnableMultiThreading);
		JSONTools::ReadBool("TournamentMode", micro, TournamentMode);
		JSONTools::ReadString("StarCraft2Version", micro, StarCraft2Version);
    }

    if (j.count("UCTConsideringDurations") && j["UCTConsideringDurations"].is_object())
    {
        const json & uctcd = j["UCTConsideringDurations"];
        JSONTools::ReadBool("UCTCD", uctcd, UCTCD);
        JSONTools::ReadInt("UCTCDMaxMilli", uctcd, UCTCDMaxMilli);
        JSONTools::ReadFloat("UCTCDK", uctcd, UCTCDK);
        JSONTools::ReadInt("UCTCDMaxTraversals", uctcd, UCTCDMaxTraversals);
        JSONTools::ReadBool("UCTCDConsiderDistance", uctcd, UCTCDConsiderDistance);
    }
    // Parse the AlphaBeta Options
    if (j.count("AlphaBeta") && j["AlphaBeta"].is_object())
    {
        const json & abcd = j["AlphaBeta"];
        JSONTools::ReadBool("AlphaBetaPruning", abcd, AlphaBetaPruning);
        JSONTools::ReadInt("AlphaBetaDepth", abcd, AlphaBetaDepth);
        JSONTools::ReadInt("AlphaBetaMaxMilli", abcd, AlphaBetaMaxMilli);
    }

    // Parse the BWAPI Options
    if (j.count("BWAPI") && j["BWAPI"].is_object())
    {
        const json & bwapi = j["BWAPI"];
        JSONTools::ReadInt("SetLocalSpeed", bwapi, SetLocalSpeed);
        JSONTools::ReadInt("SetFrameSkip", bwapi, SetFrameSkip);
        JSONTools::ReadBool("UserInput", bwapi, UserInput);
        JSONTools::ReadBool("CompleteMapInformation", bwapi, CompleteMapInformation);
    }

    // Parse the SC2 Options
    if (j.count("SC2API") && j["SC2API"].is_object())
    {
        const json & sc2api = j["SC2API"];
        JSONTools::ReadBool("BatchReplayMode", sc2api, BatchReplayMode);
        JSONTools::ReadInt("NbBatchReplay", sc2api, NbBatchReplay);
    }

    // Parse the Macro Options
    if (j.count("Macro") && j["Macro"].is_object())
    {
        const json & macro = j["Macro"];
        JSONTools::ReadInt("BuildingSpacing", macro, BuildingSpacing);
        JSONTools::ReadInt("PylongSpacing", macro, PylonSpacing);
        JSONTools::ReadInt("WorkersPerRefinery", macro, WorkersPerRefinery);
		JSONTools::ReadBool("SelectStartingBuildBasedOnHistory", macro, SelectStartingBuildBasedOnHistory);
		JSONTools::ReadBool("PrintGreetingMessage", macro, PrintGreetingMessage);
		JSONTools::ReadBool("RandomProxyLocation", macro, RandomProxyLocation);
    }

    // Parse the Debug Options
    if (j.count("Debug") && j["Debug"].is_object())
    {
        const json & debug = j["Debug"];
		const json & info = j["SC2API"];
		JSONTools::ReadBool("AllowDebug", debug, AllowDebug);
		if (AllowDebug)
		{
			JSONTools::ReadBool("AllowKeyControl", debug, AllowKeyControl);

			JSONTools::ReadBool("PlayAsHuman", info, IsRealTime);
			JSONTools::ReadBool("DrawTileInfo", debug, DrawTileInfo);
			JSONTools::ReadBool("DrawBaseLocationInfo", debug, DrawBaseLocationInfo);
			JSONTools::ReadBool("DrawBaseTiles", debug, DrawBaseTiles);
			JSONTools::ReadBool("DrawStartingRamp", debug, DrawStartingRamp);
			JSONTools::ReadBool("DrawWall", debug, DrawWall);
			JSONTools::ReadBool("DrawWalkableSectors", debug, DrawWalkableSectors);
			JSONTools::ReadBool("DrawBuildableSectors", debug, DrawBuildableSectors);
			JSONTools::ReadBool("DrawBuildable", debug, DrawBuildable);
			JSONTools::ReadBool("DrawResourceInfo", debug, DrawResourceInfo);
			JSONTools::ReadBool("DrawWorkerInfo", debug, DrawWorkerInfo);
			JSONTools::ReadBool("DrawProductionInfo", debug, DrawProductionInfo);
			JSONTools::ReadBool("DrawScoutInfo", debug, DrawScoutInfo);
			JSONTools::ReadBool("DrawSquadInfo", debug, DrawSquadInfo);
			JSONTools::ReadBool("DrawBuildingInfo", debug, DrawBuildingInfo);
			JSONTools::ReadBool("DrawModuleTimers", debug, DrawModuleTimers);
			JSONTools::ReadBool("DrawEnemyUnitInfo", debug, DrawEnemyUnitInfo);
			JSONTools::ReadBool("DrawUnitTargetInfo", debug, DrawUnitTargetInfo);
			JSONTools::ReadBool("DrawUnitPowerInfo", debug, DrawUnitPowerInfo);
			JSONTools::ReadBool("DrawReservedBuildingTiles", debug, DrawReservedBuildingTiles);
			JSONTools::ReadBool("DrawHarassInfo", debug, DrawHarassInfo);
			JSONTools::ReadBool("DrawMemoryInfo", debug, DrawMemoryInfo);
			JSONTools::ReadBool("DrawUnitID", debug, DrawUnitID);
			JSONTools::ReadBool("DrawProfilingInfo", debug, DrawProfilingInfo);
			JSONTools::ReadBool("DrawInfluenceMaps", debug, DrawInfluenceMaps);
			JSONTools::ReadBool("DrawBlockedTiles", debug, DrawBlockedTiles);
			JSONTools::ReadBool("DrawRepairStation", debug, DrawRepairStation);
			JSONTools::ReadBool("DrawDamageHealthRatio", debug, DrawDamageHealthRatio);
			JSONTools::ReadBool("DrawRangedUnitActions", debug, DrawRangedUnitActions);
			JSONTools::ReadBool("DrawResourcesProximity", debug, DrawResourcesProximity);
			JSONTools::ReadBool("DrawCombatInformation", debug, DrawCombatInformation);
			JSONTools::ReadBool("DrawPathfindingTiles", debug, DrawPathfindingTiles);
			JSONTools::ReadBool("DrawBuildingBase", debug, DrawBuildingBase);
			JSONTools::ReadBool("TimeControl", debug, TimeControl);
		}
    }

    // Parse the Module Options
    if (j.count("Modules") && j["Modules"].is_object())
    {
        const json & module = j["Modules"];

        JSONTools::ReadBool("UseAutoObserver", module, UsingAutoObserver);
    }
}
