#include "Common.h"
#include "CCBot.h"
#include "JSONTools.h"
#include "Util.h"
#include "LadderInterface.h"
#include <cstdio>
#include <csignal>
#include <cstdlib>

#include "sc2utils/sc2_manage_process.h"
#include "sc2api/sc2_api.h"
#include <string>
#ifdef _WINDOWS
	#include "StackWalker.h"
	#include <direct.h>
	#define getcwd _getcwd // stupid MSFT "deprecation" warning
#else
	#include <stdio.h>
	#include <execinfo.h>
	#include <signal.h>
	#include <stdlib.h>
	#include <unistd.h>
#endif

class Human : public sc2::Agent {
public:
    void OnGameStart() final {
        Debug()->DebugTextOut("Human");
        Debug()->SendDebug();

    }

    void OnStep()
    {
        Control()->GetObservation();
    }

};

std::string getexepath()
{
#ifdef _WINDOWS
	char buffer[255];
	char *answer = getcwd(buffer, sizeof(buffer));
	std::string s_cwd;
	if (answer)
	{
		s_cwd = answer;
	}
	return s_cwd;
#else
	return "Executable path not available on Linux";
#endif
}

void handler(int sig) {
	time_t t;
	char buffer[80];
	time(&t);
	struct tm *timeinfo = localtime(&t);
	strftime(buffer, sizeof(buffer), "%d-%m-%Y %H:%M:%S", timeinfo);
	std::string str(buffer);
	std::cerr << str << std::endl;

	// print out all the frames to stderr
	fprintf(stderr, "Error: signal %d:\n", sig);

#ifdef _WINDOWS
	StackWalker sw;
	sw.ShowCallstack();
#else
	void *arr[30];
	size_t size;
	// get void*'s for all entries on the stack
	size = backtrace(arr, 30);
	backtrace_symbols_fd(arr, size, STDERR_FILENO);
#endif

	exit(1);
}

int main(int argc, char* argv[]) 
{
	signal(SIGABRT, handler);
#ifdef _WINDOWS
	signal(SIGABRT_COMPAT, handler);
	signal(SIGBREAK, handler);
#endif
	signal(SIGFPE, handler);
	signal(SIGILL, handler);
	signal(SIGINT, handler);
	signal(SIGSEGV, handler);
	signal(SIGTERM, handler);
	signal(SIG_ATOMIC_MAX, handler);
	signal(SIG_ATOMIC_MIN, handler);

	sc2::Coordinator coordinator;
    
	std::cout << "Current working directory: " << getexepath() << std::endl;

	std::string configPath = BotConfig().ConfigFileLocation;
    std::string config = JSONTools::ReadFile(configPath);
    if (config.length() == 0)
    {
        std::cerr << "Config file could not be found, and is required for starting the bot\n";
        std::cerr << "Please read the instructions and try again\n";
		std::cin.ignore();
        exit(-1);
    }

    std::ifstream file(configPath);
    json j;
    file >> j;

    /*if (parsingFailed)
    {
        std::cerr << "Config file could not be parsed, and is required for starting the bot\n";
        std::cerr << "Please read the instructions and try again\n";
        exit(-1);
    }*/

	std::string botVersion;
	bool connectToLadder = false;
    std::string botRaceString;
    std::string enemyRaceString;
    std::string mapString;
    int stepSize = 1;
    sc2::Difficulty enemyDifficulty = sc2::Difficulty::Easy;
    bool PlayVsItSelf = false;
    bool PlayerOneIsHuman = false;
	bool ForceStepMode = false;

    if (j.count("SC2API") && j["SC2API"].is_object())
    {
        const json & info = j["SC2API"];
		JSONTools::ReadString("BotVersion", info, botVersion);
		JSONTools::ReadBool("ConnectToLadder", info, connectToLadder);
        JSONTools::ReadString("BotRace", info, botRaceString);
        JSONTools::ReadString("EnemyRace", info, enemyRaceString);

        JSONTools::ReadString("MapFile", info, mapString);
		//Random map
		if (mapString.compare("Random") == 0)
		{
			if (j.count("RandomMaps") && j["RandomMaps"].is_array())
			{
				auto maps = j["RandomMaps"];
				srand(time(NULL));//Set random seed, otherwise the result is always the same
				int mapNumber = rand() % maps.size();
				auto map = maps.at(mapNumber);
				auto mapName = map.dump();//gets the string value in the json field
				mapString = mapName.substr(1, mapName.length() - 2);//Remove quotation marks from start and end of the map name

				Util::SetMapName(mapString);//Setting name in Util so we have access to it elsewhere
			}
		}

        JSONTools::ReadInt("StepSize", info, stepSize);
        JSONTools::ReadInt("EnemyDifficulty", info, enemyDifficulty);
		JSONTools::ReadBool("PlayAsHuman", info, PlayerOneIsHuman);
		JSONTools::ReadBool("ForceStepMode", info, ForceStepMode);
        JSONTools::ReadBool("PlayVsItSelf", info, PlayVsItSelf);
    }
    else
    {
        std::cerr << "Config file has no 'Game Info' object, required for starting the bot\n";
        std::cerr << "Please read the instructions and try again\n";
		std::cin.ignore();
        exit(-1);
    }

	if (connectToLadder)
	{
		bool loadSettings = false;
		JSONTools::ReadBool("LoadSettings", j["SC2API"], loadSettings);
		CCBot bot(botVersion);
		RunBot(argc, argv, &bot, sc2::Race::Terran, loadSettings);

		return 0;
	}
	
	// We need to load settings only when running in local
	if (!coordinator.LoadSettings(argc, argv))
	{
		std::cout << "Unable to find or parse settings." << std::endl;
		std::cin.ignore();
		return 1;
	}

    // Add the custom bot, it will control the players.
    CCBot bot(botVersion);
    CCBot bot2(botVersion);

    Human human_bot;

    sc2::PlayerSetup otherPlayer;
    sc2::PlayerSetup spectatingPlayer;
    if (PlayerOneIsHuman) {
        spectatingPlayer = CreateParticipant(Util::GetRaceFromString(enemyRaceString), &human_bot);
        otherPlayer = sc2::CreateParticipant(Util::GetRaceFromString(botRaceString), &bot);
    }
    else if (PlayVsItSelf)
    {
        spectatingPlayer = sc2::CreateParticipant(Util::GetRaceFromString(botRaceString), &bot);
        otherPlayer = sc2::CreateParticipant(Util::GetRaceFromString(botRaceString), &bot2);
    }
    else
    {
        spectatingPlayer = sc2::CreateParticipant(Util::GetRaceFromString(botRaceString), &bot);
        otherPlayer = sc2::CreateComputer(Util::GetRaceFromString(enemyRaceString), enemyDifficulty);
    }


    
    // WARNING: Bot logic has not been thorougly tested on step sizes > 1
    //          Setting this = N means the bot's onFrame gets called once every N frames
    //          The bot may crash or do unexpected things if its logic is not called every frame
    coordinator.SetStepSize(stepSize);
    coordinator.SetRealtime(PlayerOneIsHuman && !ForceStepMode);

    coordinator.SetParticipants({
        spectatingPlayer,
        otherPlayer
    });

    // Start the game.
    coordinator.LaunchStarcraft();
    coordinator.StartGame(mapString);
	
    // Step forward the game simulation.
    while (coordinator.Update())
    {
    }
    return 0;
}