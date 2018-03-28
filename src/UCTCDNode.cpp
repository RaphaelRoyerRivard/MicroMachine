#include "UCTCDNode.h"
#include "UCTCDMove.h"

UCTCDNode::UCTCDNode() 
    : parent(nullptr)
    , is_max(true)
    , type(UCTCDNodeType::FIRST)
    , move(UCTCDMove())
    , children(std::vector<UCTCDNode>())
{
    num_visits = 0;
    current_score = 0;
}

UCTCDNode::UCTCDNode(UCTCDNode * pparent, bool pis_max, UCTCDNodeType pnode_type, UCTCDMove pmove)
    : parent(pparent)
    , is_max(pis_max)
    , type(pnode_type)
    , move(pmove)
    , children(std::vector<UCTCDNode>())
{
    num_visits = 0;
    current_score = 0;
}

bool UCTCDNode::hasChildren()
{
    return children.size() != 0;
}
void UCTCDNode::updateTotalScore(int pscore)
{
    current_score += pscore;
}
void UCTCDNode::addChild(UCTCDNode child)
{
    children.push_back(child);
}
UCTCDNode * UCTCDNode::getMostVisitedChild()
{
    size_t max_visits = 0;
    UCTCDNode * max_child = nullptr;
    for (UCTCDNode & child : children) {
        if (child.num_visits > max_visits) {
            max_visits = child.num_visits;
            max_child = &child;
        }
    }
    return max_child;
}
;

