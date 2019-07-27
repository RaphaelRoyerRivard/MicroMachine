#pragma once
#include "sc2api/sc2_interfaces.h"
#include <libvoxelbot/buildorder/build_time_estimator.h>
#include <libvoxelbot/utilities/stdutils.h>
#include <libvoxelbot/combat/combat_environment.h>
#include <limits>
#include <vector>
#include <bitset>
#include <array>

struct AvailableUnitTypes;
struct BuildState;
struct CombatEnvironment;

inline bool canBeAttackedByAirWeapons(sc2::UNIT_TYPEID type) {
    return isFlying(type) || type == sc2::UNIT_TYPEID::PROTOSS_COLOSSUS;
}

struct CombatUnit {
	int owner;
	sc2::UNIT_TYPEID type;
	float health;
	float health_max;
	float shield;
	float shield_max;
	float energy;
	bool is_flying;
	float buffTimer = 0;
	void modifyHealth(float delta);

	CombatUnit() {}
	CombatUnit (int owner, sc2::UNIT_TYPEID type, int health, bool flying) : owner(owner), type(type), health(health), health_max(health), shield(0), shield_max(0), energy(50), is_flying(flying) {}
	CombatUnit(const sc2::Unit& unit) : owner(unit.owner), type(unit.unit_type), health(unit.health), health_max(unit.health_max), shield(unit.shield), shield_max(unit.shield_max), energy(unit.energy), is_flying(unit.is_flying) {}
};

struct CombatState {
	std::vector<CombatUnit> units;
	const CombatEnvironment* environment = nullptr;

	// Owner with the highest total health summed over all units
	int owner_with_best_outcome() const;
	std::string toString();
};

struct CombatResult {
	float time = 0;
	std::array<float, 2> averageHealthTime = {{ 0, 0 }};
	CombatState state;
};

struct CombatRecordingFrame {
    int tick;
    std::vector<std::tuple<sc2::UNIT_TYPEID, int, float>> healths;
    void add(sc2::UNIT_TYPEID type, int owner, float health, float shield);
};

struct CombatRecording {
	std::vector<CombatRecordingFrame> frames;
	void writeCSV(std::string filename);
};

struct CombatSettings {
    bool badMicro = false;
	bool debug = false;
    bool enableSplash = true;
    bool enableTimingAdjustment = true;
    bool enableSurroundLimits = true;
    bool enableMeleeBlocking = true;
	bool workersDoNoDamage = false;
	bool assumeReasonablePositioning = true;
	float maxTime = std::numeric_limits<float>::infinity();
	float startTime = 0;
};



struct CombatPredictor {
private:
	mutable std::map<uint64_t, CombatEnvironment> combatEnvironments;
public:
	CombatEnvironment defaultCombatEnvironment;
	CombatPredictor();
	void init();
	CombatResult predict_engage(const CombatState& state, bool debug=false, bool badMicro=false, CombatRecording* recording=nullptr, int defenderPlayer = 1) const;
	CombatResult predict_engage(const CombatState& state, CombatSettings settings, CombatRecording* recording=nullptr, int defenderPlayer = 1) const;

	const CombatEnvironment& getCombatEnvironment(const CombatUpgrades& upgrades, const CombatUpgrades& targetUpgrades) const;

	const CombatEnvironment& combineCombatEnvironment(const CombatEnvironment* env, const CombatUpgrades& upgrades, int upgradesOwner) const;

	float targetScore(const CombatUnit& unit, bool hasGround, bool hasAir) const;

	float mineralScore(const CombatState& initialState, const CombatResult& combatResult, int player, const std::vector<float>& timeToProduceUnits, const CombatUpgrades upgrades) const;
	float mineralScoreFixedTime(const CombatState& initialState, const CombatResult& combatResult, int player, const std::vector<float>& timeToProduceUnits, const CombatUpgrades upgrades) const;
};

CombatUnit makeUnit(int owner, sc2::UNIT_TYPEID type);

struct ArmyComposition {
    std::vector<std::pair<sc2::UNIT_TYPEID,int>> unitCounts;
    CombatUpgrades upgrades;

	void combine(const ArmyComposition& other) {
		for (auto u : other.unitCounts) {
			bool found = false;
			for (auto& u2 : unitCounts) {
				if (u2.first == u.first) {
					u2 = { u2.first, u2.second + u.second };
					found = true;
					break;
				}
			}

			if (!found) unitCounts.push_back(u);
		}

		upgrades.combine(other.upgrades);
	}
};

struct CompositionSearchSettings {
	const CombatPredictor& combatPredictor;
	const AvailableUnitTypes& availableUnitTypes;
	const BuildOptimizerNN* buildTimePredictor = nullptr;
	float availableTime = 4 * 60;

	CompositionSearchSettings(const CombatPredictor& combatPredictor, const AvailableUnitTypes& availableUnitTypes, const BuildOptimizerNN* buildTimePredictor = nullptr) : combatPredictor(combatPredictor), availableUnitTypes(availableUnitTypes), buildTimePredictor(buildTimePredictor) {}
};

ArmyComposition findBestCompositionGenetic(const CombatPredictor& predictor, const AvailableUnitTypes& availableUnitTypes, const CombatState& opponent, const BuildOptimizerNN* buildTimePredictor = nullptr, const BuildState* startingBuildState = nullptr, std::vector<std::pair<sc2::UNIT_TYPEID,int>>* seedComposition = nullptr);
ArmyComposition findBestCompositionGenetic(const CombatState& opponent, CompositionSearchSettings settings, const BuildState* startingBuildState = nullptr, std::vector<std::pair<sc2::UNIT_TYPEID,int>>* seedComposition = nullptr);

struct CombatRecorder {
private:
	std::vector<std::pair<float, std::vector<sc2::Unit>>> frames;
public:
	void tick(const sc2::ObservationInterface* observation);
	void finalize(std::string filename="recording.csv");
};

struct SurroundInfo {
    int maxAttackersPerDefender;
    int maxMeleeAttackers;
};

void logRecordings(CombatState& state, const CombatPredictor& predictor, float spawnOffset = 0, std::string msg = "recording");
float timeToBeAbleToAttack (const CombatEnvironment& env, CombatUnit& unit, float distanceToEnemy);
SurroundInfo maxSurround(float enemyGroundUnitArea, int enemyGroundUnits);
