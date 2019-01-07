#include "Common.h"
#include "CCBot.h"
#include "JSONTools.h"
#include "Util.h"
#include "LadderInterface.h"
#include <cstdio>
#include <csignal>
#include <cstdlib>
#include "StackWalker.h"

#ifdef SC2API

#include "sc2utils/sc2_manage_process.h"
#include "sc2api/sc2_api.h"
#include <string>
#ifdef _WIN32
	#include <direct.h>
	#define getcwd _getcwd // stupid MSFT "deprecation" warning
#elif
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
	char buffer[255];
	char *answer = getcwd(buffer, sizeof(buffer));
	std::string s_cwd;
	if (answer)
	{
		s_cwd = answer;
	}
	return s_cwd;
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

	StackWalker sw;
	sw.ShowCallstack();

	exit(1);
}

int main(int argc, char* argv[]) 
{
	signal(SIGABRT, handler);
	signal(SIGABRT_COMPAT, handler);
	signal(SIGBREAK, handler);
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

    if (j.count("SC2API") && j["SC2API"].is_object())
    {
        const json & info = j["SC2API"];
		JSONTools::ReadString("BotVersion", info, botVersion);
		JSONTools::ReadBool("ConnectToLadder", info, connectToLadder);
        JSONTools::ReadString("BotRace", info, botRaceString);
        JSONTools::ReadString("EnemyRace", info, enemyRaceString);
        JSONTools::ReadString("MapFile", info, mapString);
        JSONTools::ReadInt("StepSize", info, stepSize);
        JSONTools::ReadInt("EnemyDifficulty", info, enemyDifficulty);
        JSONTools::ReadBool("PlayAsHuman", info, PlayerOneIsHuman);
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
    coordinator.SetRealtime(PlayerOneIsHuman);
    //coordinator.SetRealtime(true);

    coordinator.SetParticipants({
        spectatingPlayer,
        otherPlayer
    });

    // Start the game.
    coordinator.LaunchStarcraft();
    coordinator.StartGame(mapString);

    // Step forward the game simulation.
    while (coordinator.Update() && !bot.shouldConceed())
    {
    }
    return 0;
}

#else

#include <BWAPI/Client.h>

void UAlbertaBot_BWAPIReconnect() 
{
    while(!BWAPI::BWAPIClient.connect())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds{ 1000 }); 
    }
}

void UAlbertaBot_PlayGame()
{
    CCBot bot;
    
    // The main game loop, which continues while we are connected to BWAPI and in a game
	while (BWAPI::BWAPIClient.isConnected() && BWAPI::Broodwar->isInGame()) 
    {
        // Handle each of the events that happened on this frame of the game
		for (const BWAPI::Event & e : BWAPI::Broodwar->getEvents()) 
        {
			switch (e.getType()) 
            {
                case BWAPI::EventType::MatchStart:      { bot.OnGameStart();                 break; }
			    case BWAPI::EventType::MatchFrame:      { bot.OnStep();                      break; }
                                                        
                /*case BWAPI::EventType::MatchEnd:        { m->onEnd(e.isWinner());            break; }
			    case BWAPI::EventType::UnitShow:        { m->onUnitShow(e.getUnit());        break; }
			    case BWAPI::EventType::UnitHide:        { m->onUnitHide(e.getUnit());        break; }
			    case BWAPI::EventType::UnitCreate:      { m->onUnitCreate(e.getUnit());      break; }
			    case BWAPI::EventType::UnitMorph:       { m->onUnitMorph(e.getUnit());       break; }
			    case BWAPI::EventType::UnitDestroy:     { m->onUnitDestroy(e.getUnit());     break; }
			    case BWAPI::EventType::UnitRenegade:    { m->onUnitRenegade(e.getUnit());    break; }
			    case BWAPI::EventType::UnitComplete:    { m->onUnitComplete(e.getUnit());    break; }
			    case BWAPI::EventType::SendText:        { m->onSendText(e.getText());        break; }*/
			}
		}

		BWAPI::BWAPIClient.update();
		if (!BWAPI::BWAPIClient.isConnected()) 
        {
			std::cout << "Disconnected\n";
			break;
		}
	}

    std::cout << "Game Over\n";
}

int main(int argc, char * argv[]) 
{
    bool exitIfStarcraftShutdown = true;

    size_t gameCount = 0;
	while (true)
    {
        // if we are not currently connected to BWAPI, try to reconnect
        if (!BWAPI::BWAPIClient.isConnected())
        {
            UAlbertaBot_BWAPIReconnect();
        }

        // if we have connected to BWAPI
        while (BWAPI::BWAPIClient.isConnected())
        {
            // wait for the game to start until the game starts
            std::cout << "Waiting for game start\n";
	        while (BWAPI::BWAPIClient.isConnected() && !BWAPI::Broodwar->isInGame()) 
            {
		        BWAPI::BWAPIClient.update();
	        }

            // Check to see if Starcraft shut down somehow
            if (BWAPI::BroodwarPtr == nullptr)
            {
                break;
            }

            // If we are successfully in a game, call the module to play the game
            if (BWAPI::Broodwar->isInGame())
            {
                std::cout << "Playing game " << gameCount++ << " on map " << BWAPI::Broodwar->mapFileName() << "\n";

                UAlbertaBot_PlayGame();
            }
        }

        if (exitIfStarcraftShutdown && !BWAPI::BWAPIClient.isConnected())
        {
            return 0;
        }
    }

	return 0;
}
#endif