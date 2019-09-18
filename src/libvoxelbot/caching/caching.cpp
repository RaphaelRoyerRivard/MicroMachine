#include <fstream>
#include <iostream>
#include <queue>
#include "dependency_analyzer.h"
#include "../utilities/mappings.h"
#include "../utilities/unit_data_caching.h"
#include "sc2api/sc2_api.h"
#include "sc2utils/sc2_manage_process.h"

using namespace sc2;
using namespace std;

static const char* EmptyMap = "Test/Empty.SC2Map";

vector<string> attributeNames = {
    "Invalid",
    "Light",
    "Armored",
    "Biological",
    "Mechanical",
    "Robotic",
    "Psionic",
    "Massive",
    "Structure",
    "Hover",
    "Heroic",
    "Summoned",
    "Invalid",
};

vector<string> weaponTypeNames = {
    "Invalid",
    "Ground",
    "Air",
    "Any",
    "Invalid",
};

int hashFile(string path) {
    ifstream stream(path);
    int hash = 0;
    char c;
    while(stream >> c) {
        hash = (hash * 31) ^ c;
    }
    return hash;
}

void writeUnitTypeOverview (ofstream& result, const vector<UnitTypeData>& unit_types, const DependencyAnalyzer& deps) {
    for (const UnitTypeData& type : unit_types) {
        if (!type.available)
            continue;
        result << type.name << " " << UnitTypeToName(type.unit_type_id) << endl;
        result << "\tID: " << (int)type.unit_type_id << endl;
        result << "\tMinerals: " << type.mineral_cost << endl;
        result << "\tVespene: " << type.vespene_cost << endl;
        result << "\tSupply Required: " << type.food_required << endl;
        result << "\tSupply Provided: " << type.food_provided << endl;
        result << "\tBuild time: " << type.build_time << endl;
        result << "\tArmor: " << type.armor << endl;
        result << "\tMovement speed: " << type.movement_speed << endl;
        for (auto t : type.tech_alias) {
            result << "\tTech Alias: " << UnitTypeToName(t) << endl;
        }
        result << "\tUnit Alias: " << UnitTypeToName(type.unit_alias) << endl;
        result << "\tTech Aliases: ";
        for (auto t : type.tech_alias) result << UnitTypeToName(t) << " ";
        result << endl;
        result << "\tTech Requirement: " << UnitTypeToName(type.tech_requirement) << endl;

        result << "\tAbility: " << AbilityTypeToName(type.ability_id) << endl;
        result << "\tAttributes:";
        for (auto a : type.attributes) {
            result << " " << attributeNames[(int)a];
        }
        result << endl;
        result << "\tWeapons:" << endl;
        for (auto w : type.weapons) {
            result << "\t\tDamage: " << w.damage_ << endl;
            result << "\t\tTarget: " << weaponTypeNames[(int)w.type] << endl;
            result << "\t\tBonuses: ";
            for (auto b : w.damage_bonus) {
                result << attributeNames[(int)b.attribute] << "=+" << b.bonus << " ";
            }
            result << endl;
            result << "\t\tAttacks: " << w.attacks << endl;
            result << "\t\tRange: " << w.range << endl;
            result << "\t\tCooldown: " << w.speed << endl;
            result << "\t\tDPS: " << (w.attacks * w.damage_ / w.speed) << endl;
            result << endl;
        }

        result << "\tHas been: ";
        for (auto u : hasBeen(type.unit_type_id)) {
            result << UnitTypeToName(u) << ", ";
        }
        result << endl;
        result << "\tCan become: ";
        for (auto u : canBecome(type.unit_type_id)) {
            result << UnitTypeToName(u) << ", ";
        }
        result << endl;
        result << "\tDependencies: ";
        for (auto u : deps.allUnitDependencies[(int)type.unit_type_id]) {
            result << UnitTypeToName(u) << ", ";
        }
        result << endl;

        result << endl;
    }
}

// Model
// Σ Pi + Σ PiSij
class CachingBot : public sc2::Agent {
    ofstream results;
    int tick = 0;

   public:
    int mode = 0;

    void OnGameLoading() {
    }

    void OnGameStart() override {
        if (mode == 0) {
            results = ofstream("bot/generated/abilities.h");
            results << "#pragma once" << endl;
            results << "#include<vector>" << endl;
            results << "extern std::vector<std::pair<int,int>> unit_type_has_ability;" << endl;
            results << "extern std::vector<std::pair<float,float>> unit_type_initial_health;" << endl;
            results << "extern std::vector<bool> unit_type_is_flying;" << endl;
            results << "extern std::vector<float> unit_type_radius;" << endl;

            results.close();

            results = ofstream("bot/generated/abilities.cpp");
            results << "#include \"abilities.h\"" << endl;
        }

        Debug()->DebugEnemyControl();
        Debug()->DebugShowMap();
        Debug()->DebugIgnoreFood();
        Debug()->DebugIgnoreResourceCost();
        Debug()->SendDebug();
        Debug()->DebugGiveAllTech();
        Debug()->SendDebug();

        Point2D mn = Observation()->GetGameInfo().playable_min;
        Point2D mx = Observation()->GetGameInfo().playable_max;

        const sc2::UnitTypes& unit_types = Observation()->GetUnitTypeData(true);

        if (mode == 1) {
            initMappings(Observation());

            DependencyAnalyzer deps;
            // TODO: Use Observation here
            deps.analyze();
            
            // Note: this depends on output from this program, so it really has to be run twice to get the correct output.
            ofstream unit_lookup = ofstream("bot/generated/units.txt");
            writeUnitTypeOverview(unit_lookup, unit_types, deps);
            unit_lookup.close();

            const sc2::UnitTypes& unit_types3 = Observation()->GetUnitTypeData(true);

            // Note: this depends on output from this program, so it really has to be run twice to get the correct output.
            unit_lookup = ofstream("bot/generated/units_with_tech.txt");
            writeUnitTypeOverview(unit_lookup, unit_types3, deps);
            unit_lookup.close();

            save_unit_data(unit_types);

            // Try loading and re-saving the data to make sure that everything is loaded correctly
            int hash = hashFile(UNIT_DATA_CACHE_PATH);
            auto unit_types2 = load_unit_data();
            save_unit_data(unit_types2, "/tmp/unit_data.data");
            int hash2 = hashFile("/tmp/unit_data.data");
            if (hash != hash2) {
                cerr << "Error in unit type data serialization code" << endl;
                exit(1);
            }

            ofstream unit_lookup2 = ofstream("bot/generated/units_verify.txt");
            writeUnitTypeOverview(unit_lookup2, unit_types2, deps);
            unit_lookup2.close();

            save_ability_data(Observation()->GetAbilityData());
            save_upgrade_data(Observation()->GetUpgradeData());
        } else {
            cerr << "Note: run program again with 'pass2' as an argument on the commandline to get the correct output" << endl;

            int i = 0;
            for (const UnitTypeData& type : unit_types) {
                if (!type.available)
                    continue;
                float x = mn.x + ((rand() % 1000) / 1000.0f) * (mx.x - mn.x);
                float y = mn.y + ((rand() % 1000) / 1000.0f) * (mx.y - mn.y);
                if (type.race == Race::Protoss && type.movement_speed == 0) {
                    Debug()->DebugCreateUnit(UNIT_TYPEID::PROTOSS_PYLON, Point2D(x, y));
                }
                Debug()->DebugCreateUnit(type.unit_type_id, Point2D(x, y));
                i++;
            }
        }

        Debug()->SendDebug();
    }

    void OnStep() override {
        if (tick == 2) {
            if (mode == 0) {
                results << "std::vector<std::pair<int,int>> unit_type_has_ability = {" << endl;
                auto ourUnits = Observation()->GetUnits(Unit::Alliance::Self);
                auto abilities = Query()->GetAbilitiesForUnits(ourUnits, true);
                const sc2::UnitTypes& unitTypes = Observation()->GetUnitTypeData();
                for (int i = 0; i < ourUnits.size(); i++) {
                    AvailableAbilities& abilitiesForUnit = abilities[i];
                    for (auto availableAbility : abilitiesForUnit.abilities) {
                        results << "\t{ " << ourUnits[i]->unit_type << ", " << availableAbility.ability_id << " }," << endl;
                        cout << unitTypes[ourUnits[i]->unit_type].name << " has " << AbilityTypeToName(availableAbility.ability_id) << endl;
                    }
                }

                results << "};" << endl;

                const sc2::UnitTypes& unit_types = Observation()->GetUnitTypeData();
                vector<pair<float, float>> initial_health(unit_types.size());
                vector<bool> is_flying(unit_types.size());
                vector<float> radii(unit_types.size());
                for (int i = 0; i < ourUnits.size(); i++) {
                    initial_health[(int)ourUnits[i]->unit_type] = make_pair(ourUnits[i]->health_max, ourUnits[i]->shield_max);
                    is_flying[(int)ourUnits[i]->unit_type] = ourUnits[i]->is_flying;
                    radii[(int)ourUnits[i]->unit_type] = ourUnits[i]->radius;
                }

                // Fix marine health, DebugGiveAllTech will make marines get 10 extra hp
                initial_health[(int)UNIT_TYPEID::TERRAN_MARINE] = make_pair(45, 0);

                results << "std::vector<std::pair<float,float>> unit_type_initial_health = {" << endl;
                for (int i = 0; i < initial_health.size(); i++) {
                    results << "\t{" << initial_health[i].first << ", " << initial_health[i].second << "},\n";
                }
                results << "};" << endl;

                results << "std::vector<bool> unit_type_is_flying = {" << endl;
                for (int i = 0; i < initial_health.size(); i++) {
                    results << "\t" << (is_flying[i] ? "true" : "false") << ",\n";
                }
                results << "};" << endl;

                results << "std::vector<float> unit_type_radius = {" << endl;
                for (int i = 0; i < initial_health.size(); i++) {
                    results << "\t" << radii[i] << ",\n";
                }
                results << "};" << endl;
                results.close();
            }

            Debug()->DebugEndGame(false);
        }

        Actions()->SendActions();
        Debug()->SendDebug();
        tick++;
    }
};

/*int main(int argc, char* argv[]) {
    Coordinator coordinator;
    if (!coordinator.LoadSettings(argc, argv)) {
        return 1;
    }

    coordinator.SetMultithreaded(true);

    CachingBot bot;
    coordinator.SetParticipants({ CreateParticipant(Race::Terran, &bot) });

    // Start the game.

    coordinator.SetRealtime(false);
    coordinator.LaunchStarcraft();
    bool do_break = false;

    bot.OnGameLoading();

    if (argc > 1 && string(argv[1]) == "pass2") {
        bot.mode = 1;
    }
    coordinator.StartGame(EmptyMap);

    while (coordinator.Update() && !do_break) {
        if (PollKeyPress()) {
            do_break = true;
        }
    }
    return 0;
}*/
