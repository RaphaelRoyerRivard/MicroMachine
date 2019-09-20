#include "build_order.h"
#include "build_state.h"
#include <sstream>
#include <iomanip>

using namespace std;
using namespace sc2;

static string timeStr(float time) {
    stringstream ss;
    int sec = (int)(fmod(time, 60.0f));
    ss << (int)(time / 60.0f) << ":" << (sec < 10 ? "0" : "") << sec;
    return ss.str();
}


void printBuildOrderDetailed(const libvoxelbot::BuildState& startState, const BuildOrder& buildOrder, const vector<bool>* highlight) {
	libvoxelbot::BuildState state = startState;
    cout << "Starting units" << endl;
    for (auto u : startState.units) {
        cout << "\t" << u.units << "x " << UnitTypeToName(u.type);
        if (u.addon != UNIT_TYPEID::INVALID)
            cout << " + " << UnitTypeToName(u.addon);
        cout << endl;
    }
    cout << "Build order size " << buildOrder.size() << endl;
    bool success = state.simulateBuildOrder(buildOrder, [&](int i) {
        if (highlight != nullptr && (*highlight)[i]) {
            // Color the text
            cout << "\x1b[" << 48 << ";2;" << 228 << ";" << 26 << ";" << 28 << "m"; 
        }
        if (buildOrder.items[i].chronoBoosted) {
            // Color the text
            cout << "\x1b[" << 48 << ";2;" << 36 << ";" << 202 << ";" << 212 << "m"; 
        }
        string name = buildOrder[i].isUnitType() ? UnitTypeToName(buildOrder[i].typeID()) : UpgradeIDToName(buildOrder[i].upgradeID());
        cout << "Step " << i << "\t" << (int)(state.time / 60.0f) << ":" << (int)(fmod(state.time, 60.0f)) << "\t" << name << " "
             << "food: " << (state.foodCap() - state.foodAvailable()) << "/" << state.foodCap() << " resources: " << (int)state.resources.minerals << "+" << (int)state.resources.vespene << " " << (state.baseInfos.size() > 0 ? state.baseInfos[0].remainingMinerals : 0);

        // Reset color
        cout << "\033[0m";
        cout << endl;
    });

    cout << (success ? "Finished at " : "Failed at ");
    cout << (int)(state.time / 60.0f) << ":" << (int)(fmod(state.time, 60.0f)) << " resources: " << state.resources.minerals << "+" << state.resources.vespene << " mining speed: " << (int)round(state.miningSpeed().mineralsPerSecond*60) << "/min + " << (int)round(state.miningSpeed().vespenePerSecond*60) << "/min" << endl;

    // if (success) printMiningSpeedFuture(state);
}

void printBuildOrder(const vector<UNIT_TYPEID>& buildOrder) {
    cout << "Build order size " << buildOrder.size() << endl;
    for (size_t i = 0; i < buildOrder.size(); i++) {
        cout << "Step " << i << " " << UnitTypeToName(buildOrder[i]) << endl;
    }
}

void printBuildOrder(const BuildOrder& buildOrder) {
    cout << "Build order size " << buildOrder.size() << endl;
    for (size_t i = 0; i < buildOrder.size(); i++) {
        cout << "Step " << i << " ";
        if (buildOrder[i].isUnitType()) {
            cout << UnitTypeToName(buildOrder[i].typeID()) << endl;
        } else {
            cout << UpgradeIDToName(buildOrder[i].upgradeID()) << endl;
        }
    }
}

std::string BuildOrder::toString() {
    stringstream ss;
    for (size_t i = 0; i < items.size(); i++) {
        ss << setw(3) << i << " ";
        if (items[i].isUnitType()) {
            ss << UnitTypeToName(items[i].typeID());
        } else {
            ss << UpgradeIDToName(items[i].upgradeID());
        }
        ss << endl;
    }
    return ss.str();
}

static const string ChronoBoostColor = "\x1b[48;2;0;105;179m";

std::string BuildOrder::toString(libvoxelbot::BuildState initialState, BuildOrderPrintMode mode) {
    stringstream ss;
    if (mode == BuildOrderPrintMode::Brief) {
        bool success = initialState.simulateBuildOrder(*this, [&](int i) {
            if (items[i].chronoBoosted) {
                // Color the text
                ss << ChronoBoostColor;
            }

            string name = items[i].isUnitType() ? UnitTypeToName(items[i].typeID()) : UpgradeIDToName(items[i].upgradeID());
            ss << setw(3) << i << " " << timeStr(initialState.time) << " " << name;

            // Reset color
            ss << "\033[0m";
            ss << endl;
        });
        ss << (success ? "Done at " : "Failed at ") << initialState.time << endl;
    } else {
        // Detailed
        ss << "    Time Food  Min. Ves. Name" << endl;
        bool success = initialState.simulateBuildOrder(*this, [&](int i) {
            if (items[i].chronoBoosted) {
                // Color the text
                ss << ChronoBoostColor;
            }

            string name = items[i].isUnitType() ? UnitTypeToName(items[i].typeID()) : UpgradeIDToName(items[i].upgradeID());
            ss << setw(3) << i << " " << timeStr(initialState.time) << " ";
            ss << (initialState.foodCap() - initialState.foodAvailable()) << "/" << initialState.foodCap() << " ";
            ss << setw(4) << (int)initialState.resources.minerals << " " << setw(4) << (int)initialState.resources.vespene << " ";
            // ss << " food: " << (initialState.foodCap() - initialState.foodAvailable()) << "/" << initialState.foodCap() << " resources: " << (int)initialState.resources.minerals << "+" << (int)initialState.resources.vespene;
            ss << name;

            // Reset color
            ss << "\033[0m";
            ss << endl;
        });
        ss << (success ? "Done at " : "Failed at ") << timeStr(initialState.time) << endl;
    }
    return ss.str();
}
