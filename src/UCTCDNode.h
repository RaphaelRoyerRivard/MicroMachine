#pragma once

#include <vector>
#include "UCTCDMove.h"

enum class UCTCDNodeType { FIRST, SECOND, SOLO, ROOT };

class UCTCDNode {
private:
    size_t num_visits;
    int current_score;
    UCTCDNode * parent;
    std::vector<UCTCDNode> children;

public:
    bool is_max;
    UCTCDNodeType type;
    UCTCDMove move;

    UCTCDNode();
    UCTCDNode(UCTCDNode * parent, bool is_max, UCTCDNodeType type, UCTCDMove move);

    size_t get_num_visits() { return num_visits; }
    void inc_num_visits() { ++num_visits; }
    int get_Score() { return current_score; }
    UCTCDNode * get_parent() { return parent; }
    std::vector<UCTCDNode> & get_children() { return children; }

    bool hasChildren();
    void updateTotalScore(int score);
    void addChild(UCTCDNode child);
    UCTCDNode * getMostVisitedChild();
};
