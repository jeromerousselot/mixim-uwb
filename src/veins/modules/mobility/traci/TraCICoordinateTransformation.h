//
// Copyright (C) 2018 Dominik S. Buse <buse@ccs-labs.org>
//
// Documentation for these modules is at http://veins.car2x.org/
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//

#ifndef VEINS_MOBILITY_TRACI_NETWORKBOUNDARY_H_
#define VEINS_MOBILITY_TRACI_NETWORKBOUNDARY_H_

#include "veins/modules/mobility/traci/TraCICoord.h"
#include "veins/base/utils/Coord.h"

#include <list>

namespace Veins {

/**
 * Helper class for converting SUMO coordinates to OMNeT++ Coordinates for a given network.
 */
class TraCICoordinateTransformation {
public:
    using OmnetCoord = Coord;
    using OmnetCoordList = std::list<OmnetCoord>;
    using TraCICoordList = std::list<TraCICoord>;
    using Angle = double;

    TraCICoordinateTransformation(TraCICoord topleft, TraCICoord bottomright, float margin);
    TraCICoord omnet2traci(const OmnetCoord& coord) const;
    TraCICoordList omnet2traci(const OmnetCoordList& coords) const;
    Angle omnet2traciAngle(Angle angle) const; /**< TraCI's angle interpretation: 0 is north, 90 is east */

    OmnetCoord traci2omnet(const TraCICoord& coord) const;
    OmnetCoordList traci2omnet(const TraCICoordList& coords) const;
    Angle traci2omnetAngle(Angle angle) const; /**<  OMNeT++'s angle interpretation: 0 is east, pi/2 is north */
private:
    TraCICoord dimensions;
    TraCICoord topleft;
    TraCICoord bottomright;
    float margin;
}; // end class NetworkCoordinateTranslator

} // end namespace Veins

#endif
