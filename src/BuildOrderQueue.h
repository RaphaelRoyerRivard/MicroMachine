#pragma once

#include "Common.h"
#include "MetaType.h"

class CCBot;

// This namespace is to prevent build error when compiling because the libvoxelbot library also contains a BuildOrderItem
namespace MM {
	struct BuildOrderItem
	{
		MetaType       type;		// the thing we want to 'build'
		int             priority;	// the priority at which to place it in the queue
		bool            blocking;	// whether or not we block further items

		BuildOrderItem(const MetaType & t, int p, bool b);
		bool operator<(const BuildOrderItem & x) const;
		bool operator==(const BuildOrderItem & x) const;
	};
}

class BuildOrderQueue
{
    CCBot & m_bot;
    std::deque<MM::BuildOrderItem> m_queue;

    int m_lowestPriority;
    int m_highestPriority;
    int m_defaultPrioritySpacing;
    int m_numSkippedItems;

public:

    BuildOrderQueue(CCBot & bot);

    void clearAll();											// clears the entire build order queue
    void skipItem();											// increments skippedItems
    MM::BuildOrderItem queueAsHighestPriority(const MetaType & type, bool blocking);		// queues something at the highest priority
    MM::BuildOrderItem queueAsLowestPriority(const MetaType & type, bool blocking);		// queues something at the lowest priority
    MM::BuildOrderItem queueItem(const MM::BuildOrderItem & b);			// queues something with a given priority
    void removeHighestPriorityItem();								// removes the highest priority item
    void removeCurrentHighestPriorityItem();
	void removeAllOfType(const MetaType & type);

    size_t size();													// returns the size of the queue

    bool isEmpty();
    MM::BuildOrderItem getHighestPriorityItem();	// returns the highest priority item
    MM::BuildOrderItem getNextHighestPriorityItem();	// returns the highest priority item
	int getCountOfType(const MetaType & type);	// returns the number of items of a type in the queue

    bool canSkipItem();
    std::string getQueueInformation() const;
	bool contains(const MetaType & type) const;

    // overload the bracket operator for ease of use
    MM::BuildOrderItem operator [] (int i);
};
