//
// Copyright (C) 2006-2018 Christoph Sommer <sommer@ccs-labs.org>
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

#include "veins/modules/analogueModel/SimpleObstacleShadowing.h"

using namespace Veins;

using Veins::AirFrame;

#define debugEV EV << "PhyLayer(SimpleObstacleShadowing): "

SimpleObstacleShadowing::SimpleObstacleShadowing(ObstacleControl& obstacleControl, double carrierFrequency, bool useTorus, const Coord& playgroundSize, bool debug)
    : obstacleControl(obstacleControl)
    , carrierFrequency(carrierFrequency)
    , useTorus(useTorus)
    , playgroundSize(playgroundSize)
    , debug(debug)
{
    if (useTorus) throw cRuntimeError("SimpleObstacleShadowing does not work on torus-shaped playgrounds");
}

void SimpleObstacleShadowing::filterSignal(Signal* signal, const Coord& sendersPos, const Coord& receiverPos)
{

    double factor = obstacleControl.calculateAttenuation(sendersPos, receiverPos);

    debugEV << "value is: " << factor << endl;

    signal->addUniformAttenuation(factor);
}
