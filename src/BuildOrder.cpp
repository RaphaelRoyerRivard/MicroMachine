#include "BuildOrder.h"

MM::BuildOrder::BuildOrder()
{

}


MM::BuildOrder::BuildOrder(const std::vector<MetaType> & vec)
    : m_buildOrder(vec)
{

}

void MM::BuildOrder::add(const MetaType & type)
{
    m_buildOrder.push_back(type);
}

size_t MM::BuildOrder::size() const
{
    return m_buildOrder.size();
}

const MetaType & MM::BuildOrder::operator [] (const size_t & index) const
{
    return m_buildOrder[index];
}

MetaType & MM::BuildOrder::operator [] (const size_t & index)
{
    return m_buildOrder[index];
}