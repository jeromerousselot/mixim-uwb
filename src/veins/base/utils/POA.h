/*
 * POA.h
 *
 *  Created on: Jun 15, 2016
 *      Author: Alexander Brummer
 */

#ifndef POA_H_
#define POA_H_

#include "veins/base/phyLayer/Antenna.h"
#include "veins/base/utils/Coord.h"
#include <memory>

namespace Veins {

/**
 * @brief Container class used to attach data to Airframe s which are
 * needed by the receiver for antenna gain calculation (POA is short
 * for position, orientation, antenna).
 *
 * @author Alexander Brummer
 */
class POA {
public:
    /**
     * Stores the sender's position.
     */
    Coord pos;

    /**
     * Saves the sender's orientation.
     */
    Coord orientation;

    /**
     * Shared pointer to the sender's antenna, which is necessary for
     * the receiver to calculate the gain of the transmitting antenna.
     * It is a shared pointer to ensure that the antenna still exists
     * even if the sending node is already gone.
     */
    std::shared_ptr<Antenna> antenna;

    POA(){};
    POA(Coord pos, Coord orientation, std::shared_ptr<Antenna> antenna)
        : pos(pos)
        , orientation(orientation)
        , antenna(antenna){};
    virtual ~POA(){};
};

} // namespace Veins

#endif /* POA_H_ */
