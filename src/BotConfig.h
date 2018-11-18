#pragma once

#include "Common.h"

class BotConfig
{

public:

    bool ConfigFileFound;
    bool ConfigFileParsed;
    std::string ConfigFileLocation;
        
    bool UsingAutoObserver;		
    
    std::string BotName;
    std::string Authors;
    bool PrintInfoOnStart;
    std::string BotMode;

    int SetLocalSpeed;
    int SetFrameSkip;
    bool UserInput;
    bool CompleteMapInformation;

    bool BatchReplayMode;
    int NbBatchReplay;
    
    std::string StrategyName;
    std::string ReadDir;
    std::string WriteDir;
    bool UseEnemySpecificStrategy;
    bool FoundEnemySpecificStrategy;

	bool AllowDebug;
	bool AllowKeyControl;
    bool DrawGameInfo;
    bool DrawTileInfo;
    bool DrawBaseLocationInfo;
    bool DrawWalkableSectors;
    bool DrawResourceInfo;
    bool DrawProductionInfo;
    bool DrawScoutInfo;
    bool DrawWorkerInfo;
    bool DrawModuleTimers;
    bool DrawReservedBuildingTiles;
    bool DrawBuildingInfo;
	bool DrawStartingRamp;
    bool DrawEnemyUnitInfo;
    bool DrawLastSeenTileInfo;
    bool DrawUnitTargetInfo;
    bool DrawSquadInfo;		
	bool DrawUnitPowerInfo;
	bool DrawFSMStateInfo;
	bool DrawHarassInfo;
	bool DrawMemoryInfo;
	bool DrawUnitID;
	bool DrawProfilingInfo;
    
    CCColor ColorLineTarget;
    CCColor ColorLineMineral;
    CCColor ColorUnitNearEnemy;
    CCColor ColorUnitNotNearEnemy;
    
    bool KiteWithRangedUnits;   
    float MaxTargetDistance;
    float MaxWorkerRepairDistance;
    bool ScoutHarassEnemy;
	bool AutoCompleteBuildOrder;
	bool NoScoutOn2PlayersMap;

    bool AlphaBetaPruning;
    int AlphaBetaDepth;
    int AlphaBetaMaxMilli;

    bool UCTCD;
    int UCTCDMaxMilli;
    float UCTCDK;
    int UCTCDMaxTraversals;
    bool UCTCDConsiderDistance;

    bool UnitOwnAgent;
    bool SkipOneFrame;
    bool ClosestEnemy;
    bool WeakestEnemy;
    bool HighestPriority;
    
    int WorkersPerRefinery;
    int BuildingSpacing;
    int PylonSpacing;
 
    BotConfig();

    void readConfigFile();
};