#include "UCTConsideringDurations.h"
#include "UCTCDState.h"
#include "UCTCDMove.h"
#include "UCTCDNode.h"
#include "UCTCDTimeOut.h"

UCTConsideringDurations::UCTConsideringDurations(float pk, size_t pmax_traversals, size_t time, std::map<const sc2::Unit *, UCTCDAction> pcommand_for_unit)
    : k(pk)
    , max_traversals(pmax_traversals)
    , time_limit(time)
    , nodes_explored(1)
    , traversals(0)
    , command_for_unit(pcommand_for_unit)
{
}

UCTCDMove UCTConsideringDurations::doSearch(std::vector<UCTCDUnit> max_units, std::vector<UCTCDUnit> min_units, bool pclosest, bool pweakest, bool ppriority, bool pconsiderDistance, bool punitOwnAgent)
{
    UCTCDPlayer min = UCTCDPlayer(min_units, false);
    UCTCDPlayer max = UCTCDPlayer(max_units, true);
    UCTCDState state = UCTCDState(min, max, 0, pclosest, pweakest, ppriority, pconsiderDistance, punitOwnAgent);
    return UCTCD(state);
}

UCTCDMove UCTConsideringDurations::UCTCD(UCTCDState state)
{
    start = std::chrono::high_resolution_clock::now();
    UCTCDNode root = UCTCDNode();
    root.type = UCTCDNodeType::ROOT;
    for (traversals; traversals < max_traversals; ++traversals) {
        UCTCDState copyState = UCTCDState(state);
        traverse(root, copyState);
        auto end = std::chrono::high_resolution_clock::now();
        time_spent = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        if (time_spent.count() > time_limit)
            break;
    }
    win_value = root.get_Score();
    auto child = root.getMostVisitedChild();

    UCTCDMove move = child != nullptr ? child->move : UCTCDMove{};

    return move;
}

int UCTConsideringDurations::traverse(UCTCDNode & n, UCTCDState & s)
{   
    if (&n == NULL) {
        // not a valid node
        return -1;
    }
    
    int score;
    if (n.get_num_visits() == 0) {
        UpdateState(n, s, true);
        score = s.eval();
    }
    else {
        UpdateState(n, s, false);
        if (s.isTerminal()) {
            score = s.eval();
        }
        else {
            if (!n.hasChildren()) {
                generateChildren(s, n);
            }
            score = traverse(selectNode(n), s);
        }
    }
    n.inc_num_visits();
    n.updateTotalScore(score);
    return score;
}

UCTCDNode & UCTConsideringDurations::selectNode(UCTCDNode & n)
{
    double best_score = -INFINITY;
    UCTCDNode * best_node = nullptr;
    for (UCTCDNode & child : n.get_children()) {
        if (child.get_num_visits() == 0) return child;
        double score = (float)child.get_Score() / child.get_num_visits() +
            k * sqrt(log(n.get_num_visits()) / child.get_num_visits());
        if (score > best_score) {
            best_score = score;
            best_node = &child;
        }
    }
    return *best_node;
}

void UCTConsideringDurations::UpdateState(UCTCDNode & n, UCTCDState & s, bool leaf)
{
    if (n.type != UCTCDNodeType::FIRST || leaf) {
        if (n.type == UCTCDNodeType::SECOND) {
            s.doMove(n.get_parent()->move);
        }
        s.doMove(n.move);
    }
}

void UCTConsideringDurations::generateChildren(UCTCDState & s, UCTCDNode & n)
{
    std::vector<UCTCDMove> moves;
    bool player_to_move;
    UCTCDNodeType type;
    if (s.bothCanMove() && (n.get_parent() == nullptr || n.get_parent()->type != UCTCDNodeType::FIRST)) {
        // max is first in nash nodes
        player_to_move = true;
        type = UCTCDNodeType::FIRST;
    }
    else if (n.get_parent() != nullptr && n.get_parent()->type == UCTCDNodeType::FIRST) {
        player_to_move = false;
        type = UCTCDNodeType::SECOND;
    }
    else {
        player_to_move = s.playerToMove();
        type = UCTCDNodeType::SOLO;
    }
    moves = s.generateMoves(player_to_move, n, command_for_unit);
    for (auto move : moves) {
        n.addChild(UCTCDNode(&n, player_to_move, type, move));
        ++nodes_explored;
    }
}
