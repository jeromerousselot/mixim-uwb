/**
 * This file has been merged from BaseUtility.h and Blackboard.h in
 * order to bring Blackboard's functionality to BaseUtility.
 *
 * Blackboard's comments have been copied since they describe the
 * migrated functionality.
 *
 *
 *
 */

#ifndef BASE_UTILITY_H
#define BASE_UTILITY_H

//BB start
#ifdef _MSC_VER
#pragma warning(disable : 4786)
#endif

#include <omnetpp.h>
#include "Blackboard.h"
#include "HostState.h"
#include "Coord.h"
/**
 * @brief Provides several utilities (mainly Blackboard functionality
 * for a host).
 *
 * @see Blackboard
 * @ingroup baseModules
 *
 * @author Andreas Koepke
 */

class BaseUtility : public Blackboard,
					public ImNotifiable
{
private:
    /**
     * The position of the Host. This coordinate is
     * synchronized with BaseMobility.
     */
    Coord pos;

protected:
	/** @brief Function to get a pointer to the host module*/
	cModule *findHost(void);

    /** @brief BBItem category number of Move*/
    int catMove;

    /** @brief BBItem category number of HostState*/
    int catHostState;

    /** @brief The hosts current state.*/
    HostState hostState;

public:

    /** @brief Initializes mobility model parameters.*/
    virtual void initialize(int);

    /** @brief Get current position */
    const Coord* getPos() {return &pos;}

    /** @brief Get the current HostState */
    const HostState& getHostState() { return hostState; }

    /**
     * We want to receive Moves and HostStates from BaseUtility to synchronize
     * hostPosition and -state.
     */
    virtual void receiveBBItem(int category, const BBItem *details, int scopeModuleId);
};

#endif

