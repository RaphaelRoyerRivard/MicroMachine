#include "simulator.h"
#include "../buildorder/build_state.h"

using namespace std;
using namespace sc2;

int combatWinner(const CombatPredictor& predictor, const CombatState& state) {
    return predictor.predict_engage(state).state.owner_with_best_outcome();
}

const static double PI = 3.141592653589793238462643383279502884;

// static void createState(DebugInterface* debug, const CombatState& state, float offset = 0) {
//     if (!debug) return;
//     for (auto& u : state.units) {
//         debug->DebugCreateUnit(u.type, Point2D(40 + 20 * (u.owner == 1 ? -1 : 1), 20 + offset), u.owner);
//     }
// }

void unitTestSurround() {
    // One unit can be surrounded by 6 melee units and attacked by all 6 at the same time
    assert(maxSurround(pow(unitRadius(UNIT_TYPEID::TERRAN_MARINE), 2) * PI * 1, 1).maxAttackersPerDefender == 6);
    assert(maxSurround(pow(unitRadius(UNIT_TYPEID::TERRAN_MARINE), 2) * PI * 1, 1).maxMeleeAttackers == 6);

    // Two units can be surrounded by 8 melee units, but each one can only be attacked by at most 4
    assert(maxSurround(pow(unitRadius(UNIT_TYPEID::TERRAN_MARINE), 2) * PI * 2, 2).maxAttackersPerDefender == 4);
    assert(maxSurround(pow(unitRadius(UNIT_TYPEID::TERRAN_MARINE), 2) * PI * 2, 2).maxMeleeAttackers == 8);

    // Two units can be surrounded by 9 melee units, but each one can only be attacked by at most 3
    assert(maxSurround(pow(unitRadius(UNIT_TYPEID::TERRAN_MARINE), 2) * PI * 3, 3).maxAttackersPerDefender == 3);
    assert(maxSurround(pow(unitRadius(UNIT_TYPEID::TERRAN_MARINE), 2) * PI * 3, 3).maxMeleeAttackers == 9);

    assert(maxSurround(pow(unitRadius(UNIT_TYPEID::TERRAN_MARINE), 2) * PI * 4, 4).maxAttackersPerDefender == 3);
    assert(maxSurround(pow(unitRadius(UNIT_TYPEID::TERRAN_MARINE), 2) * PI * 4, 4).maxMeleeAttackers == 10);

    // One thor can be attacked by 10 melee units at a time.
    // This seems to be slightly incorrect, the real number is only 9, but it's approximately correct at least
    assert(maxSurround(pow(unitRadius(UNIT_TYPEID::TERRAN_THOR), 2) * PI * 1, 1).maxAttackersPerDefender == 10);
    assert(maxSurround(pow(unitRadius(UNIT_TYPEID::TERRAN_THOR), 2) * PI * 1, 1).maxMeleeAttackers == 10);
}

int main() {
    initMappings();
    CombatPredictor predictor;
    predictor.init();
    unitTestSurround();

    assert(combatWinner(predictor, {{
		makeUnit(1, UNIT_TYPEID::TERRAN_VIKINGFIGHTER),
        makeUnit(2, UNIT_TYPEID::PROTOSS_COLOSSUS),
	}}) == 1);

	assert(combatWinner(predictor, {{
		makeUnit(1, UNIT_TYPEID::PROTOSS_PYLON),
		makeUnit(1, UNIT_TYPEID::PROTOSS_PHOTONCANNON),
		makeUnit(1, UNIT_TYPEID::PROTOSS_SHIELDBATTERY),
		makeUnit(1, UNIT_TYPEID::PROTOSS_SHIELDBATTERY),
		makeUnit(1, UNIT_TYPEID::PROTOSS_SHIELDBATTERY),
		makeUnit(2, UNIT_TYPEID::TERRAN_MEDIVAC),
		makeUnit(2, UNIT_TYPEID::TERRAN_MARINE),
	}}) == 1);

	// Medivacs are pretty good (this is a very narrow win though)
	assert(combatWinner(predictor, {{
		makeUnit(1, UNIT_TYPEID::TERRAN_MARINE),
		makeUnit(1, UNIT_TYPEID::TERRAN_MEDIVAC),
		makeUnit(2, UNIT_TYPEID::TERRAN_MARINE),
		makeUnit(2, UNIT_TYPEID::TERRAN_MARINE),
	}}) == 1);

	// 1 marine wins against 1 zergling
	assert(combatWinner(predictor, {{
		CombatUnit(1, UNIT_TYPEID::TERRAN_MARINE, 50, false),
		CombatUnit(2, UNIT_TYPEID::ZERG_ZERGLING, 35, false),
	}}) == 1);

	// Symmetric
	assert(combatWinner(predictor, {{
		CombatUnit(2, UNIT_TYPEID::TERRAN_MARINE, 50, false),
		CombatUnit(1, UNIT_TYPEID::ZERG_ZERGLING, 35, false),
	}}) == 2);

	// 4 marines win against 4 zerglings
	assert(combatWinner(predictor, {{
		CombatUnit(1, UNIT_TYPEID::TERRAN_MARINE, 50, false),
		CombatUnit(1, UNIT_TYPEID::TERRAN_MARINE, 50, false),
		CombatUnit(1, UNIT_TYPEID::TERRAN_MARINE, 50, false),
		CombatUnit(1, UNIT_TYPEID::TERRAN_MARINE, 50, false),
		CombatUnit(2, UNIT_TYPEID::ZERG_ZERGLING, 35, false),
		CombatUnit(2, UNIT_TYPEID::ZERG_ZERGLING, 35, false),
		CombatUnit(2, UNIT_TYPEID::ZERG_ZERGLING, 35, false),
		CombatUnit(2, UNIT_TYPEID::ZERG_ZERGLING, 35, false),
	}}) == 1);

	assert(combatWinner(predictor, {{
		CombatUnit(1, UNIT_TYPEID::TERRAN_MARINE, 50, false),
		CombatUnit(2, UNIT_TYPEID::ZERG_ZERGLING, 35, false),
		CombatUnit(2, UNIT_TYPEID::ZERG_ZERGLING, 35, false),
		CombatUnit(2, UNIT_TYPEID::ZERG_ZERGLING, 35, false),
	}}) == 2);

	assert(combatWinner(predictor, {{
		makeUnit(1, UNIT_TYPEID::ZERG_SPORECRAWLER),
		makeUnit(1, UNIT_TYPEID::ZERG_SPORECRAWLER),
        makeUnit(1, UNIT_TYPEID::ZERG_SPORECRAWLER),
        makeUnit(2, UNIT_TYPEID::TERRAN_REAPER),
        makeUnit(2, UNIT_TYPEID::TERRAN_REAPER),
        makeUnit(2, UNIT_TYPEID::TERRAN_REAPER),
	}}) == 2);

	assert(combatWinner(predictor, {{
		CombatUnit(1, UNIT_TYPEID::TERRAN_LIBERATOR, 180, true),
		CombatUnit(2, UNIT_TYPEID::ZERG_BROODLORD, 225, true),
		CombatUnit(2, UNIT_TYPEID::ZERG_BROODLORD, 225, true),
		CombatUnit(2, UNIT_TYPEID::ZERG_BROODLORD, 225, true),
		CombatUnit(2, UNIT_TYPEID::ZERG_BROODLORD, 225, true),
		CombatUnit(2, UNIT_TYPEID::ZERG_BROODLORD, 225, true),
	}}) == 1);

    // TODO: Splash?
	// assert(combatWinner(predictor, {{
	// 	CombatUnit(1, UNIT_TYPEID::TERRAN_LIBERATOR, 180, true),
	// 	CombatUnit(1, UNIT_TYPEID::TERRAN_LIBERATOR, 180, true),
	// 	CombatUnit(1, UNIT_TYPEID::TERRAN_LIBERATOR, 180, true),
	// 	CombatUnit(1, UNIT_TYPEID::TERRAN_LIBERATOR, 180, true),
	// 	CombatUnit(1, UNIT_TYPEID::TERRAN_LIBERATOR, 180, true),
	// 	CombatUnit(1, UNIT_TYPEID::TERRAN_LIBERATOR, 180, true),
	// 	CombatUnit(1, UNIT_TYPEID::TERRAN_LIBERATOR, 180, true),
	// 	CombatUnit(1, UNIT_TYPEID::TERRAN_LIBERATOR, 180, true),
	// 	CombatUnit(1, UNIT_TYPEID::TERRAN_LIBERATOR, 180, true),
	// 	CombatUnit(2, UNIT_TYPEID::ZERG_CORRUPTOR, 225, true),
	// 	CombatUnit(2, UNIT_TYPEID::ZERG_CORRUPTOR, 225, true),
	// 	CombatUnit(2, UNIT_TYPEID::ZERG_CORRUPTOR, 225, true),
	// 	CombatUnit(2, UNIT_TYPEID::ZERG_CORRUPTOR, 225, true),
	// 	CombatUnit(2, UNIT_TYPEID::ZERG_CORRUPTOR, 225, true),
	// 	CombatUnit(2, UNIT_TYPEID::ZERG_CORRUPTOR, 225, true),
	// 	CombatUnit(2, UNIT_TYPEID::ZERG_CORRUPTOR, 225, true),
	// 	CombatUnit(2, UNIT_TYPEID::ZERG_CORRUPTOR, 225, true),
	// 	CombatUnit(2, UNIT_TYPEID::ZERG_CORRUPTOR, 225, true),
	// }}) == 1);

	assert(combatWinner(predictor, {{
		CombatUnit(1, UNIT_TYPEID::TERRAN_CYCLONE, 180, true),
		CombatUnit(2, UNIT_TYPEID::PROTOSS_IMMORTAL, 200, true),
	}}) == 2);

	assert(combatWinner(predictor, {{
		makeUnit(1, UNIT_TYPEID::TERRAN_BATTLECRUISER),
		makeUnit(2, UNIT_TYPEID::TERRAN_THOR),
	}}) == 1);

    assert(combatWinner(predictor, {{
		makeUnit(1, UNIT_TYPEID::ZERG_INFESTOR),
		makeUnit(2, UNIT_TYPEID::TERRAN_BANSHEE),
	}}) == 1);

	// Wins due to splash damage
	// Really depends on microing though...
	// assert(combatWinner(predictor, {{
	// 	CombatUnit(1, UNIT_TYPEID::TERRAN_LIBERATOR, 180, true),
	// 	CombatUnit(1, UNIT_TYPEID::TERRAN_LIBERATOR, 180, true),
	// 	CombatUnit(1, UNIT_TYPEID::TERRAN_LIBERATOR, 180, true),
	// 	CombatUnit(1, UNIT_TYPEID::TERRAN_LIBERATOR, 180, true),
	// 	CombatUnit(1, UNIT_TYPEID::TERRAN_LIBERATOR, 180, true),
	// 	CombatUnit(2, UNIT_TYPEID::ZERG_MUTALISK, 120, true),
	// 	CombatUnit(2, UNIT_TYPEID::ZERG_MUTALISK, 120, true),
	// 	CombatUnit(2, UNIT_TYPEID::ZERG_MUTALISK, 120, true),
	// 	CombatUnit(2, UNIT_TYPEID::ZERG_MUTALISK, 120, true),
	// 	CombatUnit(2, UNIT_TYPEID::ZERG_MUTALISK, 120, true),
	// 	CombatUnit(2, UNIT_TYPEID::ZERG_MUTALISK, 120, true),
	// 	CombatUnit(2, UNIT_TYPEID::ZERG_MUTALISK, 120, true),
	// 	CombatUnit(2, UNIT_TYPEID::ZERG_MUTALISK, 120, true),
	// 	CombatUnit(2, UNIT_TYPEID::ZERG_MUTALISK, 120, true),
	// }}) == 1);

	// Colossus can be attacked by air weapons
	assert(combatWinner(predictor, {{
		CombatUnit(1, UNIT_TYPEID::TERRAN_LIBERATOR, 180, true),
		CombatUnit(2, UNIT_TYPEID::PROTOSS_COLOSSUS, 200, false),
	}}) == 1);

	// Do not assume all enemies will just target the most beefy unit and leave the banshee alone
	// while it takes out the hydras
	assert(combatWinner(predictor, {{
		makeUnit(1, UNIT_TYPEID::ZERG_ROACH),
		makeUnit(1, UNIT_TYPEID::ZERG_ROACH),
		makeUnit(1, UNIT_TYPEID::ZERG_ROACH),
		makeUnit(1, UNIT_TYPEID::ZERG_ROACH),
		makeUnit(1, UNIT_TYPEID::ZERG_ROACH),
		makeUnit(1, UNIT_TYPEID::ZERG_ROACH),
		makeUnit(1, UNIT_TYPEID::ZERG_HYDRALISK),
		makeUnit(1, UNIT_TYPEID::ZERG_HYDRALISK),
		makeUnit(1, UNIT_TYPEID::ZERG_HYDRALISK),
		makeUnit(1, UNIT_TYPEID::ZERG_HYDRALISK),
		makeUnit(1, UNIT_TYPEID::ZERG_ZERGLING),
		makeUnit(1, UNIT_TYPEID::ZERG_ZERGLING),
		makeUnit(1, UNIT_TYPEID::ZERG_ZERGLING),
		makeUnit(1, UNIT_TYPEID::ZERG_ZERGLING),
		makeUnit(1, UNIT_TYPEID::ZERG_ZERGLING),
		makeUnit(1, UNIT_TYPEID::ZERG_ZERGLING),
		makeUnit(1, UNIT_TYPEID::ZERG_ZERGLING),
		makeUnit(2, UNIT_TYPEID::TERRAN_BANSHEE),
		makeUnit(2, UNIT_TYPEID::TERRAN_THOR),
	}}) == 1);

    string green = "\x1b[38;2;0;255;0m";
    string reset = "\033[0m";
    cout << green << "Ok" << reset << endl;
    return 0;
}
